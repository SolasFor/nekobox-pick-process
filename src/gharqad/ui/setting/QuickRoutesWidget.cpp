#include <nekobox/ui/setting/QuickRoutesWidget.h>

#include <nekobox/ui/setting/ProcessPickerDialog.h>
#include <nekobox/ui/setting/ThemeManager.hpp>

#include <nekobox/dataStore/DataStore.hpp>
#include <nekobox/dataStore/Database.hpp>
#include <nekobox/dataStore/ResourceEntity.hpp>
#include <nekobox/dataStore/RouteEntity.h>
#include <nekobox/global/GuiUtils.hpp>

#include <QAbstractItemView>
#include <QApplication>
#include <QColor>
#include <QComboBox>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFocusEvent>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPalette>
#include <QPushButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace {

void setStyledFlag(QWidget *widget, const char *name, bool value) {
    if (widget == nullptr || widget->property(name).toBool() == value) {
        return;
    }
    widget->setProperty(name, value);
    if (QStyle *st = widget->style()) {
        st->unpolish(widget);
        st->polish(widget);
    }
    widget->update();
}

QComboBox *findCellCombo(QTableWidget *table, int row, int column, const QString &objectName) {
    if (table == nullptr) {
        return nullptr;
    }
    QWidget *cell = table->cellWidget(row, column);
    if (cell == nullptr) {
        return nullptr;
    }
    if (auto *direct = qobject_cast<QComboBox *>(cell)) {
        return direct;
    }
    return cell->findChild<QComboBox *>(objectName);
}

QLineEdit *findCellLineEdit(QTableWidget *table, int row, int column, const QString &objectName) {
    if (table == nullptr) {
        return nullptr;
    }
    QWidget *cell = table->cellWidget(row, column);
    if (cell == nullptr) {
        return nullptr;
    }
    if (auto *direct = qobject_cast<QLineEdit *>(cell)) {
        return direct;
    }
    return cell->findChild<QLineEdit *>(objectName);
}

} // namespace

QuickRoutesWidget::QuickRoutesWidget(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("quickRoutesRoot"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    auto *splitter = new QSplitter(Qt::Vertical, this);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(8);
    splitter->addWidget(makeSection(tr("Applications"), &processTable, &processCountBadge,
                                    &processRemoveButton, tr("Application"), true));
    splitter->addWidget(makeSection(tr("Domains"), &domainTable, &domainCountBadge,
                                    &domainRemoveButton, tr("Domain"), false, true));
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    root->addWidget(splitter, 1);

    auto *saveBar = new QFrame(this);
    saveBar->setObjectName(QStringLiteral("routesSaveBar"));
    auto *saveRow = new QHBoxLayout(saveBar);
    saveRow->setContentsMargins(10, 6, 10, 6);
    saveRow->setSpacing(12);

    auto *hint = new QLabel(
        tr("Application routes match the .exe name. Use Tun mode to capture apps that do not use the system proxy. Saving restarts the core."),
        saveBar);
    hint->setObjectName(QStringLiteral("routesHintLabel"));
    hint->setWordWrap(true);
    saveRow->addWidget(hint, 1);

    saveButton = new QPushButton(tr("Save"), saveBar);
    saveButton->setObjectName(QStringLiteral("routesSaveButton"));
    saveButton->setCursor(Qt::PointingHandCursor);
    saveButton->setMinimumHeight(32);
    saveRow->addWidget(saveButton);
    root->addWidget(saveBar);

    connect(saveButton, &QPushButton::clicked, this, [this] { saveToStore(); });
    if (themeManager != nullptr) {
        connect(themeManager, &ThemeManager::themeChanged, this, [this] { applyVisualStyle(); });
    }

    applyVisualStyle();
    loadFromStore();
}

QWidget *QuickRoutesWidget::makeSection(const QString &title, QTableWidget **table,
                                        QLabel **countBadge, QPushButton **removeButtonOut,
                                        const QString &firstHeader, bool processSection,
                                        bool domainSection) {
    auto *section = new QWidget(this);
    auto *layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto *headerFrame = new QFrame(section);
    headerFrame->setObjectName(QStringLiteral("routesSectionHeader"));
    auto *header = new QHBoxLayout(headerFrame);
    header->setContentsMargins(4, 2, 4, 2);
    header->setSpacing(8);

    auto *titleLabel = new QLabel(title, headerFrame);
    titleLabel->setObjectName(QStringLiteral("routesSectionTitle"));
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    header->addWidget(titleLabel);

    auto *badge = new QLabel(QStringLiteral("0"), headerFrame);
    badge->setObjectName(QStringLiteral("routesCountBadge"));
    badge->setAlignment(Qt::AlignCenter);
    badge->setMinimumWidth(24);
    header->addWidget(badge);
    if (countBadge != nullptr) {
        *countBadge = badge;
    }

    header->addStretch();

    auto *addButton = new QPushButton(tr("Add"), headerFrame);
    addButton->setObjectName(QStringLiteral("routesActionButton"));
    addButton->setCursor(Qt::PointingHandCursor);
    addButton->setMinimumHeight(28);
    header->addWidget(addButton);

    if (processSection) {
        auto *runningButton = new QPushButton(tr("Running processes..."), headerFrame);
        runningButton->setObjectName(QStringLiteral("routesActionButton"));
        runningButton->setCursor(Qt::PointingHandCursor);
        runningButton->setMinimumHeight(28);
        runningButton->setToolTip(
            tr("Pick programs that are already running instead of browsing for an .exe"));
        header->addWidget(runningButton);
        connect(runningButton, &QPushButton::clicked, this, [this] {
            ProcessPickerDialog dialog(this);
            if (dialog.exec() != QDialog::Accepted) {
                return;
            }
            int lastAddedRow = -1;
            for (const QString &path : dialog.selectedPaths()) {
                addProcessRow(path, Configs::proxyID);
                lastAddedRow = processTable->rowCount() - 1;
            }
            if (lastAddedRow >= 0 && processTable->item(lastAddedRow, 0) != nullptr) {
                processTable->scrollToItem(processTable->item(lastAddedRow, 0));
            }
        });
    }

    auto *removeButton = new QPushButton(tr("Remove"), headerFrame);
    removeButton->setObjectName(QStringLiteral("routesRemoveButton"));
    removeButton->setCursor(Qt::PointingHandCursor);
    removeButton->setMinimumHeight(28);
    removeButton->setToolTip(tr("Remove selected rows (Delete)"));
    removeButton->setEnabled(false);
    header->addWidget(removeButton);
    if (removeButtonOut != nullptr) {
        *removeButtonOut = removeButton;
    }

    layout->addWidget(headerFrame);

    auto *created = new QTableWidget(0, domainSection ? 3 : 2, section);
    *table = created;
    if (domainSection) {
        setupTable(created, {firstHeader, tr("Method"), tr("Outbound")}, 2);
    } else {
        setupTable(created, {firstHeader, tr("Outbound")}, 1);
    }
    layout->addWidget(created, 1);

    connect(created->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this, created] { syncSectionState(created); });

    connect(addButton, &QPushButton::clicked, this, [this, processSection] {
        if (processSection) {
            // Cancelling the file dialog must not leave a blank row behind.
            const auto path = browseForExecutable();
            if (path.isEmpty()) {
                return;
            }
            addProcessRow(path, Configs::proxyID);
            const int last = processTable->rowCount() - 1;
            if (last >= 0 && processTable->item(last, 0) != nullptr) {
                processTable->scrollToItem(processTable->item(last, 0));
            }
        } else {
            addDomainRow({}, Configs::proxyID);
            const int last = domainTable->rowCount() - 1;
            if (last >= 0) {
                if (domainTable->item(last, 0) != nullptr) {
                    domainTable->scrollToItem(domainTable->item(last, 0));
                }
                if (auto *edit = findCellLineEdit(domainTable, last, 0, QStringLiteral("domain_edit"))) {
                    edit->setFocus();
                }
            }
        }
    });
    connect(removeButton, &QPushButton::clicked, this,
            [this, created] { removeSelectedRows(created); });

    return section;
}

void QuickRoutesWidget::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    refreshOutboundCombos();
    syncAllSections();
}

void QuickRoutesWidget::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
    if (event != nullptr && event->type() == QEvent::PaletteChange) {
        applyVisualStyle();
    }
}

bool QuickRoutesWidget::eventFilter(QObject *watched, QEvent *event) {
    if (watched == nullptr || event == nullptr) {
        return QWidget::eventFilter(watched, event);
    }

    if (watched == processTable || watched == domainTable) {
        auto *table = static_cast<QTableWidget *>(watched);
        if (event->type() == QEvent::KeyPress) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if ((keyEvent->key() == Qt::Key_Delete || keyEvent->key() == Qt::Key_Backspace) &&
                qobject_cast<QLineEdit *>(QApplication::focusWidget()) == nullptr) {
                removeSelectedRows(table);
                return true;
            }
        } else if (event->type() == QEvent::Leave) {
            int &hovered = (table == processTable) ? hoveredProcessRow : hoveredDomainRow;
            if (hovered != -1) {
                hovered = -1;
                syncSectionState(table);
            }
        }
        return QWidget::eventFilter(watched, event);
    }

    if ((processTable != nullptr && watched == processTable->viewport()) ||
        (domainTable != nullptr && watched == domainTable->viewport())) {
        if (event->type() == QEvent::Leave) {
            QTableWidget *table = (processTable != nullptr && watched == processTable->viewport())
                                      ? processTable
                                      : domainTable;
            int &hovered = (table == processTable) ? hoveredProcessRow : hoveredDomainRow;
            if (hovered != -1) {
                hovered = -1;
                syncSectionState(table);
            }
        }
        return QWidget::eventFilter(watched, event);
    }

    auto *widget = qobject_cast<QWidget *>(watched);
    if (widget == nullptr) {
        return QWidget::eventFilter(watched, event);
    }

    if (event->type() == QEvent::Enter && widget->objectName() == QLatin1String("routeRowCell")) {
        if (QTableWidget *table = ownerTableFor(widget)) {
            const int row = rowForWidget(table, widget);
            int &hovered = (table == processTable) ? hoveredProcessRow : hoveredDomainRow;
            if (hovered != row) {
                hovered = row;
                syncSectionState(table);
            }
        }
    } else if (event->type() == QEvent::Leave &&
               widget->objectName() == QLatin1String("routeRowCell")) {
        if (QTableWidget *table = ownerTableFor(widget)) {
            const int row = rowForWidget(table, widget);
            int &hovered = (table == processTable) ? hoveredProcessRow : hoveredDomainRow;
            if (hovered == row) {
                hovered = -1;
                syncSectionState(table);
            }
        }
    } else if (event->type() == QEvent::MouseButtonPress) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            if (QTableWidget *table = ownerTableFor(widget)) {
                const int row = rowForWidget(table, widget);
                if (row >= 0) {
                    if (widget->property("rowSelectSurface").toBool()) {
                        selectRowFromClick(table, row, mouseEvent->modifiers(), true);
                        return true;
                    }
                    if (widget->property("rowEditorControl").toBool()) {
                        if (table->selectionModel() != nullptr &&
                            (!table->selectionModel()->isRowSelected(row, QModelIndex()) ||
                             table->selectionModel()->selectedRows().size() > 1)) {
                            selectRowFromClick(table, row, Qt::NoModifier, false);
                        }
                        return false;
                    }
                }
            }
        }
    } else if (event->type() == QEvent::FocusIn &&
               widget->property("rowEditorControl").toBool()) {
        auto *focusEvent = static_cast<QFocusEvent *>(event);
        if (focusEvent->reason() == Qt::TabFocusReason ||
            focusEvent->reason() == Qt::BacktabFocusReason ||
            focusEvent->reason() == Qt::ShortcutFocusReason) {
            if (QTableWidget *table = ownerTableFor(widget)) {
                const int row = rowForWidget(table, widget);
                if (row >= 0 && table->selectionModel() != nullptr &&
                    !table->selectionModel()->isRowSelected(row, QModelIndex())) {
                    selectRowFromClick(table, row, Qt::NoModifier, false);
                }
            }
        }
    }

    return QWidget::eventFilter(watched, event);
}

void QuickRoutesWidget::setupTable(QTableWidget *table, const QStringList &headers,
                                   int outboundColumn) {
    table->setObjectName(QStringLiteral("routesTable"));
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setMinimumSectionSize(120);
    if (headers.size() > 2) {
        table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
        table->setColumnWidth(1, 160);
    }
    table->horizontalHeader()->setSectionResizeMode(outboundColumn, QHeaderView::Interactive);
    table->horizontalHeader()->setHighlightSections(false);
    table->verticalHeader()->setVisible(false);
    table->verticalHeader()->setDefaultSectionSize(44);
    table->setColumnWidth(outboundColumn, 195);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(false);
    table->setShowGrid(false);
    table->setFocusPolicy(Qt::StrongFocus);
    table->viewport()->setAttribute(Qt::WA_Hover, true);
    table->installEventFilter(this);
    table->viewport()->installEventFilter(this);
}

void QuickRoutesWidget::applyVisualStyle() {
    if (applyingStyle) {
        return;
    }
    applyingStyle = true;

    bool dark = false;
    if (themeManager != nullptr && !themeManager->current_theme.isEmpty()) {
        const int mode = ThemeManager::getMode(themeManager->current_theme);
        if (mode == 1) {
            dark = true;
        } else if (mode == 2) {
            dark = false;
        } else {
            dark = palette().color(QPalette::Window).lightness() < 128 ||
                   palette().color(QPalette::Base).lightness() < 128;
        }
    } else {
        dark = palette().color(QPalette::Window).lightness() < 128 ||
               palette().color(QPalette::Base).lightness() < 128;
    }

    const QString qss = dark
        ? QStringLiteral(
              "QTableWidget#routesTable {"
              "  background-color: rgba(18, 20, 24, 170);"
              "  border: 1px solid rgba(255, 255, 255, 26);"
              "  border-radius: 6px;"
              "  outline: none;"
              "}"
              "QTableWidget#routesTable QHeaderView::section {"
              "  background-color: rgba(255, 255, 255, 12);"
              "  color: rgba(255, 255, 255, 215);"
              "  font-weight: 600;"
              "  padding: 6px 10px;"
              "  border: none;"
              "  border-bottom: 1px solid rgba(255, 255, 255, 28);"
              "  border-right: 1px solid rgba(255, 255, 255, 14);"
              "}"
              "QTableWidget#routesTable::item {"
              "  padding: 0px;"
              "  border: none;"
              "  background: transparent;"
              "}"
              "QTableWidget#routesTable::item:selected,"
              "QTableWidget#routesTable::item:selected:!active {"
              "  background-color: rgba(59, 130, 246, 48);"
              "}"
              "QFrame#routeRowCell {"
              "  background-color: transparent;"
              "  border-top: none;"
              "  border-right: none;"
              "  border-bottom: 1px solid rgba(255, 255, 255, 15);"
              "  border-left: none;"
              "}"
              "QFrame#routeRowCell[firstCol=\"true\"] {"
              "  border-left: 3px solid transparent;"
              "}"
              "QFrame#routeRowCell[rowHovered=\"true\"][rowSelected=\"false\"] {"
              "  background-color: rgba(255, 255, 255, 14);"
              "}"
              "QFrame#routeRowCell[firstCol=\"true\"][rowHovered=\"true\"][rowSelected=\"false\"] {"
              "  border-left: 3px solid rgba(96, 165, 250, 120);"
              "}"
              "QFrame#routeRowCell[rowSelected=\"true\"] {"
              "  background-color: rgba(59, 130, 246, 68);"
              "  border-bottom: 1px solid rgba(96, 165, 250, 110);"
              "}"
              "QFrame#routeRowCell[firstCol=\"true\"][rowSelected=\"true\"] {"
              "  border-left: 3px solid #60a5fa;"
              "}"
              "QFrame#processSelectChip {"
              "  background-color: rgba(255, 255, 255, 14);"
              "  border: 1px solid rgba(255, 255, 255, 28);"
              "  border-radius: 5px;"
              "}"
              "QFrame#processSelectChip:hover {"
              "  background-color: rgba(96, 165, 250, 42);"
              "  border: 1px solid rgba(96, 165, 250, 140);"
              "}"
              "QFrame#processSelectChip[rowSelected=\"true\"] {"
              "  background-color: rgba(59, 130, 246, 105);"
              "  border: 1px solid #60a5fa;"
              "}"
              "QLabel#processSelectDot {"
              "  color: rgba(255, 255, 255, 105);"
              "  font-size: 12px;"
              "  background: transparent;"
              "}"
              "QLabel#processSelectDot[rowSelected=\"true\"] {"
              "  color: #bfdbfe;"
              "}"
              "QLabel#processSelectName {"
              "  color: rgba(255, 255, 255, 235);"
              "  background: transparent;"
              "}"
              "QLabel#domainRowGrip {"
              "  background-color: rgba(255, 255, 255, 12);"
              "  border: 1px solid rgba(255, 255, 255, 24);"
              "  border-radius: 5px;"
              "  color: rgba(255, 255, 255, 120);"
              "  font-size: 12px;"
              "}"
              "QLabel#domainRowGrip:hover {"
              "  background-color: rgba(96, 165, 250, 42);"
              "  border: 1px solid rgba(96, 165, 250, 140);"
              "  color: #dbeafe;"
              "}"
              "QLabel#domainRowGrip[rowSelected=\"true\"] {"
              "  background-color: rgba(59, 130, 246, 105);"
              "  border: 1px solid #60a5fa;"
              "  color: #eff6ff;"
              "}"
              "QLabel#routesCountBadge {"
              "  background-color: rgba(96, 165, 250, 38);"
              "  color: #bfdbfe;"
              "  border: 1px solid rgba(96, 165, 250, 75);"
              "  border-radius: 9px;"
              "  padding: 1px 7px;"
              "  font-size: 11px;"
              "  font-weight: 600;"
              "}"
              "QPushButton#routesActionButton {"
              "  padding: 4px 12px;"
              "}"
              "QPushButton#routesRemoveButton {"
              "  padding: 4px 12px;"
              "}"
              "QPushButton#routesRemoveButton:enabled {"
              "  border: 1px solid rgba(248, 113, 113, 120);"
              "  color: #fecaca;"
              "}"
              "QPushButton#routesRemoveButton:enabled:hover {"
              "  background-color: rgba(239, 68, 68, 68);"
              "  border: 1px solid #f87171;"
              "  color: #ffffff;"
              "}"
              "QFrame#routesSaveBar {"
              "  background-color: rgba(255, 255, 255, 8);"
              "  border: 1px solid rgba(255, 255, 255, 20);"
              "  border-radius: 6px;"
              "}"
              "QLabel#routesHintLabel {"
              "  color: rgba(255, 255, 255, 155);"
              "}"
              "QPushButton#routesSaveButton {"
              "  background-color: rgba(37, 99, 235, 205);"
              "  color: #ffffff;"
              "  border: 1px solid #60a5fa;"
              "  border-radius: 5px;"
              "  padding: 5px 18px;"
              "  font-weight: 600;"
              "}"
              "QPushButton#routesSaveButton:hover {"
              "  background-color: rgba(59, 130, 246, 235);"
              "}")
        : QStringLiteral(
              "QTableWidget#routesTable {"
              "  background-color: #ffffff;"
              "  border: 1px solid #d1d5db;"
              "  border-radius: 6px;"
              "  outline: none;"
              "}"
              "QTableWidget#routesTable QHeaderView::section {"
              "  background-color: #f3f4f6;"
              "  color: #1f2937;"
              "  font-weight: 600;"
              "  padding: 6px 10px;"
              "  border: none;"
              "  border-bottom: 1px solid #d1d5db;"
              "  border-right: 1px solid #e5e7eb;"
              "}"
              "QTableWidget#routesTable::item {"
              "  padding: 0px;"
              "  border: none;"
              "  background: transparent;"
              "}"
              "QTableWidget#routesTable::item:selected,"
              "QTableWidget#routesTable::item:selected:!active {"
              "  background-color: rgba(37, 99, 235, 26);"
              "}"
              "QFrame#routeRowCell {"
              "  background-color: transparent;"
              "  border-top: none;"
              "  border-right: none;"
              "  border-bottom: 1px solid #e5e7eb;"
              "  border-left: none;"
              "}"
              "QFrame#routeRowCell[firstCol=\"true\"] {"
              "  border-left: 3px solid transparent;"
              "}"
              "QFrame#routeRowCell[rowHovered=\"true\"][rowSelected=\"false\"] {"
              "  background-color: rgba(37, 99, 235, 12);"
              "}"
              "QFrame#routeRowCell[firstCol=\"true\"][rowHovered=\"true\"][rowSelected=\"false\"] {"
              "  border-left: 3px solid #93c5fd;"
              "}"
              "QFrame#routeRowCell[rowSelected=\"true\"] {"
              "  background-color: rgba(37, 99, 235, 34);"
              "  border-bottom: 1px solid #93c5fd;"
              "}"
              "QFrame#routeRowCell[firstCol=\"true\"][rowSelected=\"true\"] {"
              "  border-left: 3px solid #2563eb;"
              "}"
              "QFrame#processSelectChip {"
              "  background-color: #f8fafc;"
              "  border: 1px solid #cbd5e1;"
              "  border-radius: 5px;"
              "}"
              "QFrame#processSelectChip:hover {"
              "  background-color: #eff6ff;"
              "  border: 1px solid #60a5fa;"
              "}"
              "QFrame#processSelectChip[rowSelected=\"true\"] {"
              "  background-color: #dbeafe;"
              "  border: 1px solid #2563eb;"
              "}"
              "QLabel#processSelectDot {"
              "  color: #94a3b8;"
              "  font-size: 12px;"
              "  background: transparent;"
              "}"
              "QLabel#processSelectDot[rowSelected=\"true\"] {"
              "  color: #1d4ed8;"
              "}"
              "QLabel#processSelectName {"
              "  color: #0f172a;"
              "  background: transparent;"
              "}"
              "QLabel#domainRowGrip {"
              "  background-color: #f8fafc;"
              "  border: 1px solid #cbd5e1;"
              "  border-radius: 5px;"
              "  color: #64748b;"
              "  font-size: 12px;"
              "}"
              "QLabel#domainRowGrip:hover {"
              "  background-color: #eff6ff;"
              "  border: 1px solid #60a5fa;"
              "  color: #1d4ed8;"
              "}"
              "QLabel#domainRowGrip[rowSelected=\"true\"] {"
              "  background-color: #dbeafe;"
              "  border: 1px solid #2563eb;"
              "  color: #1d4ed8;"
              "}"
              "QLabel#routesCountBadge {"
              "  background-color: #dbeafe;"
              "  color: #1e40af;"
              "  border: 1px solid #93c5fd;"
              "  border-radius: 9px;"
              "  padding: 1px 7px;"
              "  font-size: 11px;"
              "  font-weight: 600;"
              "}"
              "QPushButton#routesActionButton {"
              "  padding: 4px 12px;"
              "}"
              "QPushButton#routesRemoveButton {"
              "  padding: 4px 12px;"
              "}"
              "QPushButton#routesRemoveButton:enabled {"
              "  border: 1px solid #fca5a5;"
              "  color: #b91c1c;"
              "}"
              "QPushButton#routesRemoveButton:enabled:hover {"
              "  background-color: #fee2e2;"
              "  border: 1px solid #ef4444;"
              "  color: #991b1b;"
              "}"
              "QFrame#routesSaveBar {"
              "  background-color: #f8fafc;"
              "  border: 1px solid #e2e8f0;"
              "  border-radius: 6px;"
              "}"
              "QLabel#routesHintLabel {"
              "  color: #475569;"
              "}"
              "QPushButton#routesSaveButton {"
              "  background-color: #2563eb;"
              "  color: #ffffff;"
              "  border: 1px solid #1d4ed8;"
              "  border-radius: 5px;"
              "  padding: 5px 18px;"
              "  font-weight: 600;"
              "}"
              "QPushButton#routesSaveButton:hover {"
              "  background-color: #1d4ed8;"
              "}");

    if (styleSheet() != qss) {
        setStyleSheet(qss);
    }
    applyingStyle = false;
}

QFrame *QuickRoutesWidget::wrapCellContent(QTableWidget * /*table*/, QWidget *inner, bool firstCol,
                                           int leftMargin, int rightMargin) {
    auto *cellFrame = new QFrame();
    cellFrame->setObjectName(QStringLiteral("routeRowCell"));
    cellFrame->setAttribute(Qt::WA_StyledBackground, true);
    cellFrame->setAttribute(Qt::WA_Hover, true);
    cellFrame->setCursor(Qt::PointingHandCursor);
    cellFrame->setProperty("firstCol", firstCol);
    cellFrame->setProperty("rowSelected", false);
    cellFrame->setProperty("rowHovered", false);
    registerRowSurface(cellFrame);

    auto *layout = new QHBoxLayout(cellFrame);
    layout->setContentsMargins(leftMargin, 6, rightMargin, 6);
    layout->setSpacing(0);
    layout->addWidget(inner, 1);
    return cellFrame;
}

void QuickRoutesWidget::registerRowSurface(QWidget *widget) {
    if (widget == nullptr) {
        return;
    }
    widget->setProperty("rowSelectSurface", true);
    widget->installEventFilter(this);
}

void QuickRoutesWidget::registerRowEditor(QWidget *widget) {
    if (widget == nullptr) {
        return;
    }
    widget->setProperty("rowEditorControl", true);
    widget->setCursor(Qt::ArrowCursor);
    widget->installEventFilter(this);
}

int QuickRoutesWidget::rowForWidget(QTableWidget *table, QWidget *widget) {
    if (table == nullptr || widget == nullptr) {
        return -1;
    }
    for (int row = 0; row < table->rowCount(); ++row) {
        for (int col = 0; col < table->columnCount(); ++col) {
            QWidget *cell = table->cellWidget(row, col);
            if (cell != nullptr && (cell == widget || cell->isAncestorOf(widget))) {
                return row;
            }
        }
    }
    return -1;
}

QTableWidget *QuickRoutesWidget::ownerTableFor(QWidget *widget) const {
    if (widget == nullptr) {
        return nullptr;
    }
    if (processTable != nullptr && processTable->isAncestorOf(widget)) {
        return processTable;
    }
    if (domainTable != nullptr && domainTable->isAncestorOf(widget)) {
        return domainTable;
    }
    return nullptr;
}

void QuickRoutesWidget::selectRowFromClick(QTableWidget *table, int row,
                                           Qt::KeyboardModifiers modifiers, bool focusTable) {
    if (table == nullptr || table->selectionModel() == nullptr || row < 0 ||
        row >= table->rowCount()) {
        return;
    }
    auto *selModel = table->selectionModel();
    const QModelIndex target = table->model()->index(row, 0);

    if (modifiers.testFlag(Qt::ControlModifier)) {
        selModel->select(target, QItemSelectionModel::Toggle | QItemSelectionModel::Rows);
        selModel->setCurrentIndex(target, QItemSelectionModel::NoUpdate);
    } else if (modifiers.testFlag(Qt::ShiftModifier)) {
        int anchor = table->currentRow();
        if (anchor < 0 || anchor >= table->rowCount()) {
            anchor = row;
        }
        const int top = std::min(anchor, row);
        const int bottom = std::max(anchor, row);
        const QItemSelection range(table->model()->index(top, 0),
                                   table->model()->index(bottom, table->columnCount() - 1));
        selModel->select(range, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    } else {
        selModel->select(target, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        selModel->setCurrentIndex(target, QItemSelectionModel::NoUpdate);
    }

    if (focusTable) {
        table->setFocus(Qt::MouseFocusReason);
    }
    syncSectionState(table);
}

void QuickRoutesWidget::syncSectionState(QTableWidget *table) {
    if (table == nullptr) {
        return;
    }

    const int hoveredRow = (table == processTable) ? hoveredProcessRow : hoveredDomainRow;
    auto *selModel = table->selectionModel();
    int selectedCount = 0;

    for (int row = 0; row < table->rowCount(); ++row) {
        const bool selected = selModel != nullptr && selModel->isRowSelected(row, QModelIndex());
        const bool hovered = (row == hoveredRow);
        if (selected) {
            ++selectedCount;
        }
        for (int col = 0; col < table->columnCount(); ++col) {
            QWidget *cell = table->cellWidget(row, col);
            if (cell == nullptr) {
                continue;
            }
            setStyledFlag(cell, "rowSelected", selected);
            setStyledFlag(cell, "rowHovered", hovered);
            if (col == 0) {
                if (auto *chip = cell->findChild<QWidget *>(QStringLiteral("processSelectChip"))) {
                    setStyledFlag(chip, "rowSelected", selected);
                }
                if (auto *dot = cell->findChild<QLabel *>(QStringLiteral("processSelectDot"))) {
                    dot->setText(selected ? QStringLiteral("●") : QStringLiteral("○"));
                    setStyledFlag(dot, "rowSelected", selected);
                }
                if (auto *grip = cell->findChild<QLabel *>(QStringLiteral("domainRowGrip"))) {
                    grip->setText(selected ? QStringLiteral("●") : QStringLiteral("○"));
                    setStyledFlag(grip, "rowSelected", selected);
                }
            }
        }
    }

    QLabel *badge = (table == processTable) ? processCountBadge : domainCountBadge;
    if (badge != nullptr) {
        badge->setText(QString::number(table->rowCount()));
    }

    QPushButton *removeBtn = (table == processTable) ? processRemoveButton : domainRemoveButton;
    if (removeBtn != nullptr) {
        removeBtn->setEnabled(selectedCount > 0);
        removeBtn->setText(selectedCount > 1 ? tr("Remove (%1)").arg(selectedCount) : tr("Remove"));
    }
}

void QuickRoutesWidget::syncAllSections() {
    syncSectionState(processTable);
    syncSectionState(domainTable);
}

QList<QuickRoutesWidget::OutboundChoice> QuickRoutesWidget::collectOutbounds() {
    QList<OutboundChoice> list;
    list.append({Configs::proxyID, tr("proxy")});
    list.append({Configs::directID, tr("direct")});
    list.append({Configs::blockID, tr("block")});

    if (Configs::profileManager == nullptr) {
        return list;
    }
    for (const auto &gid : Configs::profileManager->groupsTabOrder) {
        auto group = Configs::profileManager->GetGroup(gid);
        if (group == nullptr) {
            continue;
        }
        for (const auto &pid : group->profiles) {
            auto profile = Configs::profileManager->GetProfile(pid);
            if (profile == nullptr) {
                continue;
            }
            auto name = profile->DisplayName();
            if (!group->name.isEmpty()) {
                name = group->name + " / " + name;
            }
            list.append({pid, name});
        }
    }
    return list;
}

void QuickRoutesWidget::fillOutboundCombo(QComboBox *combo, const QList<OutboundChoice> &choices,
                                          int selectedId) const {
    combo->clear();
    int selectedIndex = 0;
    bool found = false;
    for (const auto &choice : choices) {
        combo->addItem(choice.name, choice.id);
        if (choice.id == selectedId) {
            selectedIndex = combo->count() - 1;
            found = true;
        }
    }
    if (!found) {
        combo->addItem(tr("Missing outbound (%1)").arg(selectedId), selectedId);
        selectedIndex = combo->count() - 1;
    }
    combo->setCurrentIndex(selectedIndex);
}

QComboBox *QuickRoutesWidget::makeOutboundCombo(int selectedId) {
    auto *combo = new QComboBox();
    combo->setObjectName(QStringLiteral("outbound_combo"));
    combo->setMinimumHeight(30);
    combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    fillOutboundCombo(combo, collectOutbounds(), selectedId);
    registerRowEditor(combo);
    combo->setCursor(Qt::PointingHandCursor);
    return combo;
}

QString QuickRoutesWidget::browseForExecutable() {
    QString startPath;
    if (Configs::resourceManager != nullptr) {
        startPath = Configs::resourceManager->getLatestPath();
    }
    const auto selected = QFileDialog::getOpenFileName(this, tr("Select an application"), startPath,
#ifdef Q_OS_WIN
                                                       tr("Applications (*.exe);;All files (*.*)")
#else
                                                       QString()
#endif
    );
    if (selected.isEmpty()) {
        return {};
    }
    if (Configs::resourceManager != nullptr) {
        Configs::resourceManager->latest_path = QFileInfo(selected).absolutePath();
    }
    return QDir::toNativeSeparators(selected);
}

QWidget *QuickRoutesWidget::makeProcessEditor(const QString &path) {
    auto *holder = new QWidget();
    registerRowSurface(holder);
    auto *layout = new QHBoxLayout(holder);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto *chip = new QFrame(holder);
    chip->setObjectName(QStringLiteral("processSelectChip"));
    chip->setAttribute(Qt::WA_StyledBackground, true);
    chip->setCursor(Qt::PointingHandCursor);
    chip->setMinimumWidth(165);
    chip->setMaximumWidth(225);
    chip->setMinimumHeight(30);
    chip->setProperty("rowSelected", false);
    registerRowSurface(chip);

    auto *chipLayout = new QHBoxLayout(chip);
    chipLayout->setContentsMargins(8, 2, 10, 2);
    chipLayout->setSpacing(6);

    auto *dotLabel = new QLabel(QStringLiteral("○"), chip);
    dotLabel->setObjectName(QStringLiteral("processSelectDot"));
    dotLabel->setAlignment(Qt::AlignCenter);
    dotLabel->setFixedWidth(14);
    dotLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    auto *iconLabel = new QLabel(chip);
    iconLabel->setObjectName(QStringLiteral("processSelectIcon"));
    iconLabel->setFixedSize(18, 18);
    iconLabel->setScaledContents(true);
    iconLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    auto *nameLabel = new QLabel(chip);
    nameLabel->setObjectName(QStringLiteral("processSelectName"));
    nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    chipLayout->addWidget(dotLabel);
    chipLayout->addWidget(iconLabel);
    chipLayout->addWidget(nameLabel, 1);

    auto *edit = new QLineEdit(path, holder);
    edit->setObjectName(QStringLiteral("process_path"));
    edit->setMinimumHeight(30);
    edit->setPlaceholderText(tr("Full path to an .exe, or just its file name"));
    registerRowEditor(edit);
    edit->setCursor(Qt::IBeamCursor);

    auto *browse = new QToolButton(holder);
    browse->setObjectName(QStringLiteral("processBrowseButton"));
    browse->setText(QStringLiteral("…"));
    browse->setMinimumSize(32, 30);
    browse->setToolTip(tr("Select an application"));
    registerRowEditor(browse);
    browse->setCursor(Qt::PointingHandCursor);

    connect(edit, &QLineEdit::textChanged, chip,
            [this, chip](const QString &value) { updateProcessChip(chip, value); });

    connect(browse, &QToolButton::clicked, holder, [this, edit] {
        const auto selected = browseForExecutable();
        if (!selected.isEmpty()) {
            edit->setText(selected);
        }
    });

    updateProcessChip(chip, path);

    layout->addWidget(chip);
    layout->addWidget(edit, 1);
    layout->addWidget(browse);
    return holder;
}

void QuickRoutesWidget::updateProcessChip(QWidget *chip, const QString &rawPath) {
    if (chip == nullptr) {
        return;
    }
    const QString trimmed = rawPath.trimmed();
    const QString normalized = QDir::fromNativeSeparators(trimmed);
    QString shortName = QFileInfo(normalized).fileName();
    if (shortName.isEmpty()) {
        shortName = trimmed.isEmpty() ? tr("No path specified") : trimmed;
    }

    if (auto *nameLabel = chip->findChild<QLabel *>(QStringLiteral("processSelectName"))) {
        QFont f = nameLabel->font();
        f.setBold(!trimmed.isEmpty());
        nameLabel->setFont(f);
        const QFontMetrics fm(f);
        nameLabel->setText(fm.elidedText(shortName, Qt::ElideMiddle, 145));
    }

    if (auto *iconLabel = chip->findChild<QLabel *>(QStringLiteral("processSelectIcon"))) {
        QIcon icon;
        if (!trimmed.isEmpty() && !trimmed.startsWith(QLatin1String("\\\\")) &&
            QFileInfo::exists(trimmed)) {
            icon = iconProvider.icon(QFileInfo(trimmed));
        }
        if (icon.isNull()) {
            icon = style()->standardIcon(QStyle::SP_FileIcon);
        }
        iconLabel->setPixmap(icon.pixmap(18, 18));
    }

    const QString selectHint =
        tr("Click to select this row (Ctrl or Shift for multiple, Delete to remove)");
    chip->setToolTip(trimmed.isEmpty() ? selectHint : (trimmed + QLatin1Char('\n') + selectHint));
}

QWidget *QuickRoutesWidget::makeDomainEditor(const QString &domain, QComboBox *methodCombo) {
    auto *holder = new QWidget();
    registerRowSurface(holder);
    auto *layout = new QHBoxLayout(holder);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto *grip = new QLabel(QStringLiteral("○"), holder);
    grip->setObjectName(QStringLiteral("domainRowGrip"));
    grip->setAlignment(Qt::AlignCenter);
    grip->setFixedSize(30, 30);
    grip->setCursor(Qt::PointingHandCursor);
    grip->setProperty("rowSelected", false);
    grip->setToolTip(tr("Click to select this row (Ctrl or Shift for multiple, Delete to remove)"));
    registerRowSurface(grip);

    auto *edit = new QLineEdit(domain, holder);
    edit->setObjectName(QStringLiteral("domain_edit"));
    edit->setMinimumHeight(30);
    registerRowEditor(edit);
    edit->setCursor(Qt::IBeamCursor);

    auto updatePlaceholder = [edit](QComboBox *combo) {
        if (combo != nullptr && combo->currentData().toString() == QLatin1String("suffix")) {
            edit->setPlaceholderText(tr("Domain suffix, for example: google.com"));
        } else {
            edit->setPlaceholderText(tr("Domain keyword, for example: example.com"));
        }
    };
    if (methodCombo != nullptr) {
        connect(methodCombo, &QComboBox::currentIndexChanged, edit,
                [updatePlaceholder, methodCombo] { updatePlaceholder(methodCombo); });
    }
    updatePlaceholder(methodCombo);

    layout->addWidget(grip);
    layout->addWidget(edit, 1);
    return holder;
}

QString QuickRoutesWidget::processPathAt(int row) const {
    auto *edit = findCellLineEdit(processTable, row, 0, QStringLiteral("process_path"));
    return edit != nullptr ? edit->text().trimmed() : QString();
}

QComboBox *QuickRoutesWidget::makeMethodCombo(const QString &method) {
    auto *combo = new QComboBox();
    combo->setObjectName(QStringLiteral("method_combo"));
    combo->setMinimumHeight(30);
    combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    combo->addItem(tr("keyword"), QStringLiteral("keyword"));
    combo->addItem(tr("suffix"), QStringLiteral("suffix"));
    combo->setItemData(0, tr("Matches any domain that contains this text"), Qt::ToolTipRole);
    combo->setItemData(1, tr("Matches this domain and all of its subdomains"), Qt::ToolTipRole);
    const int index = combo->findData(method);
    combo->setCurrentIndex(index >= 0 ? index : 0);
    registerRowEditor(combo);
    combo->setCursor(Qt::PointingHandCursor);
    return combo;
}

int QuickRoutesWidget::outboundAt(QTableWidget *table, int row, int outboundColumn) {
    auto *combo = findCellCombo(table, row, outboundColumn, QStringLiteral("outbound_combo"));
    if (combo == nullptr) {
        return Configs::proxyID;
    }
    return combo->currentData().toInt();
}

void QuickRoutesWidget::addProcessRow(const QString &path, int outboundId) {
    const int row = processTable->rowCount();
    processTable->insertRow(row);
    for (int col = 0; col < processTable->columnCount(); ++col) {
        auto *item = new QTableWidgetItem();
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        processTable->setItem(row, col, item);
    }
    processTable->setCellWidget(
        row, 0, wrapCellContent(processTable, makeProcessEditor(path), true, 10, 8));
    processTable->setCellWidget(
        row, 1, wrapCellContent(processTable, makeOutboundCombo(outboundId), false, 6, 10));
    syncSectionState(processTable);
}

void QuickRoutesWidget::addDomainRow(const QString &domain, int outboundId,
                                     const QString &method) {
    const int row = domainTable->rowCount();
    domainTable->insertRow(row);
    for (int col = 0; col < domainTable->columnCount(); ++col) {
        auto *item = new QTableWidgetItem();
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        domainTable->setItem(row, col, item);
    }

    auto *methodCombo = makeMethodCombo(method.isEmpty() ? QStringLiteral("keyword") : method);
    domainTable->setCellWidget(
        row, 0, wrapCellContent(domainTable, makeDomainEditor(domain, methodCombo), true, 10, 8));
    domainTable->setCellWidget(row, 1, wrapCellContent(domainTable, methodCombo, false, 6, 6));
    domainTable->setCellWidget(
        row, 2, wrapCellContent(domainTable, makeOutboundCombo(outboundId), false, 6, 10));
    syncSectionState(domainTable);
}

void QuickRoutesWidget::removeSelectedRows(QTableWidget *table) {
    if (table == nullptr || table->selectionModel() == nullptr) {
        return;
    }
    auto rows = table->selectionModel()->selectedRows();
    if (rows.isEmpty()) {
        return;
    }
    std::sort(rows.begin(), rows.end(),
              [](const QModelIndex &a, const QModelIndex &b) { return a.row() > b.row(); });
    for (const auto &index : rows) {
        table->removeRow(index.row());
    }
    if (table == processTable) {
        hoveredProcessRow = -1;
    } else if (table == domainTable) {
        hoveredDomainRow = -1;
    }
    table->clearSelection();
    syncSectionState(table);
}

void QuickRoutesWidget::refreshOutboundCombos() {
    const auto choices = collectOutbounds();
    auto updateTable = [&](QTableWidget *table, int outboundColumn) {
        if (table == nullptr) {
            return;
        }
        for (int row = 0; row < table->rowCount(); row++) {
            auto *combo =
                findCellCombo(table, row, outboundColumn, QStringLiteral("outbound_combo"));
            if (combo == nullptr) {
                continue;
            }
            const auto selected = combo->currentData().toInt();
            QSignalBlocker blocker(combo);
            fillOutboundCombo(combo, choices, selected);
        }
    };
    updateTable(processTable, 1);
    updateTable(domainTable, 2);
}

void QuickRoutesWidget::loadFromStore() {
    hoveredProcessRow = -1;
    hoveredDomainRow = -1;
    processTable->setRowCount(0);
    domainTable->setRowCount(0);
    if (Configs::dataStore == nullptr || Configs::dataStore->routing == nullptr ||
        Configs::dataStore->routing->quick_routes == nullptr) {
        syncAllSections();
        return;
    }
    auto routes = Configs::dataStore->routing->quick_routes;
    routes->normalize();
    for (int i = 0; i < routes->process_match.size(); i++) {
        addProcessRow(routes->process_match[i], routes->process_outbound[i]);
    }
    for (int i = 0; i < routes->domain_match.size(); i++) {
        addDomainRow(routes->domain_match[i], routes->domain_outbound[i],
                     i < routes->domain_method.size() ? routes->domain_method[i] : QString());
    }
    syncAllSections();
}

void QuickRoutesWidget::saveToStore() {
    if (Configs::dataStore == nullptr || Configs::dataStore->routing == nullptr ||
        Configs::dataStore->routing->quick_routes == nullptr) {
        return;
    }
    auto routes = Configs::dataStore->routing->quick_routes;
    routes->process_match.clear();
    routes->process_outbound.clear();
    routes->domain_match.clear();
    routes->domain_outbound.clear();
    routes->domain_method.clear();

    for (int row = 0; row < processTable->rowCount(); row++) {
        const auto path = processPathAt(row);
        if (path.isEmpty()) {
            continue;
        }
        routes->process_match.append(path);
        routes->process_outbound.append(outboundAt(processTable, row, 1));
    }
    for (int row = 0; row < domainTable->rowCount(); row++) {
        auto *edit = findCellLineEdit(domainTable, row, 0, QStringLiteral("domain_edit"));
        const auto domain = edit != nullptr ? edit->text().trimmed() : QString();
        if (domain.isEmpty()) {
            continue;
        }
        auto *methodCombo = findCellCombo(domainTable, row, 1, QStringLiteral("method_combo"));
        const auto method = methodCombo != nullptr ? methodCombo->currentData().toString()
                                                   : QStringLiteral("keyword");
        routes->domain_match.append(domain);
        routes->domain_method.append(method == QLatin1String("suffix")
                                         ? QStringLiteral("suffix")
                                         : QStringLiteral("keyword"));
        routes->domain_outbound.append(outboundAt(domainTable, row, 2));
    }
    routes->normalize();

    Configs::dataStore->routing->Save();

    if (MW_show_log) {
        MW_show_log(tr("[Routes] Saved %1 application and %2 domain routes")
                        .arg(routes->process_match.size())
                        .arg(routes->domain_match.size()));
    }
    if (MW_dialog_message) {
        MW_dialog_message("", "UpdateDataStore,RouteChanged");
    }
}
