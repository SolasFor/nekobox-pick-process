#include <nekobox/ui/setting/ProcessPickerDialog.h>

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QProcess>
#include <QPushButton>
#include <QSet>
#include <QSize>
#include <QStyle>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#endif

#ifdef Q_OS_WIN
// Full image path of a process we are not allowed to do much else with.
static QString QueryImagePath(DWORD pid) {
    HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (handle == nullptr) {
        return {};
    }
    wchar_t buffer[MAX_PATH * 4];
    DWORD size = static_cast<DWORD>(sizeof(buffer) / sizeof(buffer[0]));
    QString path;
    if (QueryFullProcessImageNameW(handle, 0, buffer, &size) != 0) {
        path = QString::fromWCharArray(buffer, static_cast<int>(size));
    }
    CloseHandle(handle);
    return path;
}
#endif

QList<ProcessPickerDialog::Entry> ProcessPickerDialog::readRunningProcesses() {
    QList<Entry> result;
    QSet<QString> seen;

    auto add = [&](const QString &path) {
        if (path.isEmpty()) {
            return;
        }
        const QString key = path.toLower();
        if (seen.contains(key)) {
            return;
        }
        seen.insert(key);
        result.append({QFileInfo(path).fileName(), QDir::toNativeSeparators(path)});
    };

#ifdef Q_OS_WIN
    // Snapshot instead of EnumProcesses: it also works for processes we cannot
    // open at all, and gives every process with a fallback name.
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return result;
    }
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry) != 0) {
        do {
            add(QueryImagePath(entry.th32ProcessID));
        } while (Process32NextW(snapshot, &entry) != 0);
    }
    CloseHandle(snapshot);
#elif defined(Q_OS_LINUX)
    const QDir proc(QStringLiteral("/proc"));
    const QStringList pids = proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &pid : pids) {
        bool isNumber = false;
        pid.toInt(&isNumber);
        if (!isNumber) {
            continue;
        }
        add(QFile::symLinkTarget(QStringLiteral("/proc/%1/exe").arg(pid)));
    }
#else
    QProcess ps;
    ps.start(QStringLiteral("ps"), {QStringLiteral("-axo"), QStringLiteral("comm=")});
    if (ps.waitForFinished(3000)) {
        const QString output = QString::fromLocal8Bit(ps.readAllStandardOutput());
        for (const QString &line : output.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
            add(line.trimmed());
        }
    }
#endif

    std::sort(result.begin(), result.end(), [](const Entry &left, const Entry &right) {
        const int byName = QString::compare(left.name, right.name, Qt::CaseInsensitive);
        return byName != 0 ? byName < 0 : left.path < right.path;
    });
    return result;
}

ProcessPickerDialog::ProcessPickerDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("Running applications"));
    resize(780, 520);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto *top = new QHBoxLayout();
    top->setSpacing(8);
    filterEdit = new QLineEdit(this);
    filterEdit->setMinimumHeight(32);
    filterEdit->setPlaceholderText(tr("Filter by name or path"));
    filterEdit->setClearButtonEnabled(true);
    top->addWidget(filterEdit, 1);
    auto *refreshButton = new QPushButton(tr("Refresh"), this);
    refreshButton->setMinimumHeight(32);
    refreshButton->setCursor(Qt::PointingHandCursor);
    refreshButton->setToolTip(tr("Read the process list again"));
    top->addWidget(refreshButton);
    layout->addLayout(top);

    auto *hint = new QLabel(tr("Select one or more programs. Double-click adds a single one."), this);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    table = new QTableWidget(0, 2, this);
    table->setObjectName(QStringLiteral("processPickerTable"));
    table->setHorizontalHeaderLabels({tr("Application"), tr("Path")});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    table->setColumnWidth(0, 225);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table->horizontalHeader()->setHighlightSections(false);
    table->verticalHeader()->setVisible(false);
    table->verticalHeader()->setDefaultSectionSize(36);
    table->setIconSize(QSize(18, 18));
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(false);
    table->setShowGrid(false);

    const bool dark = palette().color(QPalette::Window).lightness() < 128 ||
                      palette().color(QPalette::Base).lightness() < 128;
    table->setStyleSheet(
        dark ? QStringLiteral(
                   "QTableWidget#processPickerTable {"
                   "  border: 1px solid rgba(255, 255, 255, 28);"
                   "  border-radius: 6px;"
                   "  outline: none;"
                   "}"
                   "QTableWidget#processPickerTable::item {"
                   "  padding: 4px 10px;"
                   "  border-bottom: 1px solid rgba(255, 255, 255, 14);"
                   "}"
                   "QTableWidget#processPickerTable::item:hover {"
                   "  background-color: rgba(255, 255, 255, 16);"
                   "}"
                   "QTableWidget#processPickerTable::item:selected,"
                   "QTableWidget#processPickerTable::item:selected:!active {"
                   "  background-color: rgba(59, 130, 246, 95);"
                   "  color: #ffffff;"
                   "}")
             : QStringLiteral(
                   "QTableWidget#processPickerTable {"
                   "  border: 1px solid #d1d5db;"
                   "  border-radius: 6px;"
                   "  outline: none;"
                   "}"
                   "QTableWidget#processPickerTable::item {"
                   "  padding: 4px 10px;"
                   "  border-bottom: 1px solid #f1f5f9;"
                   "}"
                   "QTableWidget#processPickerTable::item:hover {"
                   "  background-color: rgba(37, 99, 235, 14);"
                   "}"
                   "QTableWidget#processPickerTable::item:selected,"
                   "QTableWidget#processPickerTable::item:selected:!active {"
                   "  background-color: #dbeafe;"
                   "  color: #0f172a;"
                   "}"));

    layout->addWidget(table, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    okButton = buttons->button(QDialogButtonBox::Ok);
    if (okButton != nullptr) {
        okButton->setCursor(Qt::PointingHandCursor);
    }
    if (auto *cancelButton = buttons->button(QDialogButtonBox::Cancel)) {
        cancelButton->setCursor(Qt::PointingHandCursor);
    }
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(refreshButton, &QPushButton::clicked, this, [this] { reload(); });
    connect(filterEdit, &QLineEdit::textChanged, this,
            [this](const QString &text) { applyFilter(text); });
    connect(table, &QTableWidget::itemDoubleClicked, this,
            [this](QTableWidgetItem *) { activateCurrentRow(); });
    connect(table->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this] { updateAcceptState(); });

    reload();
    filterEdit->setFocus();
}

QIcon ProcessPickerDialog::iconForPath(const QString &path) {
    const QString key = path.toLower();
    auto it = iconCache.constFind(key);
    if (it != iconCache.cend()) {
        return it.value();
    }
    QIcon icon;
    if (!path.isEmpty() && !path.startsWith(QLatin1String("\\\\")) && QFileInfo::exists(path)) {
        icon = iconProvider.icon(QFileInfo(path));
    }
    if (icon.isNull()) {
        icon = style()->standardIcon(QStyle::SP_FileIcon);
    }
    iconCache.insert(key, icon);
    return icon;
}

QStringList ProcessPickerDialog::selectedPaths() const {
    QStringList paths;
    if (table->selectionModel() == nullptr) {
        return paths;
    }
    auto rows = table->selectionModel()->selectedRows();
    std::sort(rows.begin(), rows.end(), [](const QModelIndex &left, const QModelIndex &right) {
        return left.row() < right.row();
    });
    for (const QModelIndex &index : rows) {
        auto *item = table->item(index.row(), 0);
        if (item == nullptr) {
            continue;
        }
        const QString path = item->data(Qt::UserRole).toString();
        if (!path.isEmpty() && !paths.contains(path)) {
            paths << path;
        }
    }
    return paths;
}

void ProcessPickerDialog::reload() {
    entries = readRunningProcesses();
    applyFilter(filterEdit != nullptr ? filterEdit->text() : QString());
}

void ProcessPickerDialog::applyFilter(const QString &text) {
    const QString needle = text.trimmed();

    table->setRowCount(0);
    QFont boldFont = table->font();
    boldFont.setBold(true);

    for (const Entry &entry : entries) {
        if (!needle.isEmpty() && !entry.name.contains(needle, Qt::CaseInsensitive) &&
            !entry.path.contains(needle, Qt::CaseInsensitive)) {
            continue;
        }
        const int row = table->rowCount();
        table->insertRow(row);
        // The path travels with the item, so filtering cannot mismatch rows.
        auto *nameItem = new QTableWidgetItem(iconForPath(entry.path), entry.name);
        nameItem->setFont(boldFont);
        nameItem->setData(Qt::UserRole, entry.path);
        nameItem->setToolTip(entry.path);
        auto *pathItem = new QTableWidgetItem(entry.path);
        pathItem->setToolTip(entry.path);
        table->setItem(row, 0, nameItem);
        table->setItem(row, 1, pathItem);
    }
    if (!needle.isEmpty() && table->rowCount() > 0) {
        table->selectRow(0);
    }
    table->scrollToTop();
    updateAcceptState();
}

void ProcessPickerDialog::updateAcceptState() {
    if (okButton == nullptr || table->selectionModel() == nullptr) {
        return;
    }
    okButton->setEnabled(!table->selectionModel()->selectedRows().isEmpty());
}

void ProcessPickerDialog::activateCurrentRow() {
    const int row = table->currentRow();
    if (row < 0) {
        return;
    }
    table->clearSelection();
    table->selectRow(row);
    accept();
}
