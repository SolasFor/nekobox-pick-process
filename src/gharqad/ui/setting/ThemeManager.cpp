#include <QApplication>
#include <QColor>
#include <QFile>
#include <QGuiApplication>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>
#include <QTextStream>
#include <QtGlobal>

#include <nekobox/dataStore/Utils.hpp>
#include <nekobox/ui/setting/ThemeManager.hpp>

ThemeManager *themeManager = new ThemeManager;

namespace {

QPalette buildModernPalette(bool dark) {
    QPalette pal;
    if (dark) {
        const QColor windowBg(0x13, 0x16, 0x1c);
        const QColor baseBg(0x10, 0x13, 0x18);
        const QColor altBase(0x16, 0x1a, 0x22);
        const QColor buttonBg(0x1e, 0x23, 0x2d);
        const QColor textPrimary(0xee, 0xf2, 0xf6);
        const QColor textMuted(0x94, 0xa3, 0xb8);
        const QColor textDisabled(0x64, 0x74, 0x8b);
        const QColor accent(0x3b, 0x82, 0xf6);

        pal.setColor(QPalette::Window, windowBg);
        pal.setColor(QPalette::WindowText, textPrimary);
        pal.setColor(QPalette::Base, baseBg);
        pal.setColor(QPalette::AlternateBase, altBase);
        pal.setColor(QPalette::ToolTipBase, QColor(0x1b, 0x20, 0x29));
        pal.setColor(QPalette::ToolTipText, textPrimary);
        pal.setColor(QPalette::PlaceholderText, textMuted);
        pal.setColor(QPalette::Text, textPrimary);
        pal.setColor(QPalette::Button, buttonBg);
        pal.setColor(QPalette::ButtonText, textPrimary);
        pal.setColor(QPalette::BrightText, Qt::white);
        pal.setColor(QPalette::Link, QColor(0x60, 0xa5, 0xfa));
        pal.setColor(QPalette::Highlight, accent);
        pal.setColor(QPalette::HighlightedText, Qt::white);
        pal.setColor(QPalette::Midlight, QColor(0x26, 0x2c, 0x38));
        pal.setColor(QPalette::Mid, QColor(0x2d, 0x35, 0x44));
        pal.setColor(QPalette::Dark, QColor(0x0c, 0x0e, 0x12));

        pal.setColor(QPalette::Disabled, QPalette::WindowText, textDisabled);
        pal.setColor(QPalette::Disabled, QPalette::Text, textDisabled);
        pal.setColor(QPalette::Disabled, QPalette::ButtonText, textDisabled);
    } else {
        const QColor windowBg(0xf4, 0xf6, 0xfa);
        const QColor baseBg(0xff, 0xff, 0xff);
        const QColor altBase(0xf8, 0xfa, 0xfc);
        const QColor buttonBg(0xff, 0xff, 0xff);
        const QColor textPrimary(0x0f, 0x17, 0x2a);
        const QColor textMuted(0x64, 0x74, 0x8b);
        const QColor textDisabled(0x94, 0xa3, 0xb8);
        const QColor accent(0x25, 0x63, 0xeb);

        pal.setColor(QPalette::Window, windowBg);
        pal.setColor(QPalette::WindowText, textPrimary);
        pal.setColor(QPalette::Base, baseBg);
        pal.setColor(QPalette::AlternateBase, altBase);
        pal.setColor(QPalette::ToolTipBase, baseBg);
        pal.setColor(QPalette::ToolTipText, textPrimary);
        pal.setColor(QPalette::PlaceholderText, textMuted);
        pal.setColor(QPalette::Text, textPrimary);
        pal.setColor(QPalette::Button, buttonBg);
        pal.setColor(QPalette::ButtonText, textPrimary);
        pal.setColor(QPalette::BrightText, Qt::white);
        pal.setColor(QPalette::Link, accent);
        pal.setColor(QPalette::Highlight, accent);
        pal.setColor(QPalette::HighlightedText, Qt::white);
        pal.setColor(QPalette::Midlight, QColor(0xe2, 0xe8, 0xf0));
        pal.setColor(QPalette::Mid, QColor(0xcb, 0xd5, 0xe1));
        pal.setColor(QPalette::Dark, QColor(0x94, 0xa3, 0xb8));

        pal.setColor(QPalette::Disabled, QPalette::WindowText, textDisabled);
        pal.setColor(QPalette::Disabled, QPalette::Text, textDisabled);
        pal.setColor(QPalette::Disabled, QPalette::ButtonText, textDisabled);
    }
    return pal;
}

} // namespace

QMap<QString, QString> &ThemeManager::getThemes() {
    static QMap<QString, QString> map;
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
    }
    return map;
}

bool ThemeManager::isModernTheme(const QString &theme) {
    const auto lower = theme.trimmed().toLower();
    return lower.isEmpty() || lower == "system" || lower == "modern" ||
           lower == "modern dark" || lower == "modern light";
}

bool ThemeManager::isDarkEffective(const QString &theme) {
    const int mode = getMode(theme);
    if (mode == 1) {
        return true;
    }
    if (mode == 2) {
        return false;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (qApp != nullptr && QGuiApplication::styleHints() != nullptr) {
        const auto scheme = QGuiApplication::styleHints()->colorScheme();
        if (scheme == Qt::ColorScheme::Dark) {
            return true;
        }
        if (scheme == Qt::ColorScheme::Light) {
            return false;
        }
    }
#endif
    if (qApp != nullptr) {
        return qApp->palette().color(QPalette::Window).lightness() < 128;
    }
    return true;
}

std::tuple<QString, bool> ThemeManager::getPath(const QString &theme) {
    auto lowerTheme = theme.toLower();
    if (isModernTheme(lowerTheme)) {
        return std::make_tuple(qApp->style()->name(), false);
    } else if (lowerTheme == "dark") {
        return std::make_tuple(":nekobox/qss/MaterialDark.qss", true);
    } else if (lowerTheme == "amoled") {
        return std::make_tuple(":nekobox/qss/AMOLED.qss", true);
    } else if (lowerTheme == "aqua") {
        return std::make_tuple(":nekobox/qss/Aqua.qss", true);
    } else if (lowerTheme == "kawaii") {
        return std::make_tuple(":nekobox/qss/Kawaii.qss", true);
    } else if (lowerTheme == "flatgray") {
        return std::make_tuple(":nekobox/qss/flatgray.css", true);
    } else if (lowerTheme == "lightblue") {
        return std::make_tuple(":nekobox/qss/lightblue.css", true);
    } else if (lowerTheme == "blacksoft") {
        return std::make_tuple(":nekobox/qss/blacksoft.css", true);
    } else {
        return std::make_tuple(theme, false);
    }
}

int ThemeManager::getMode(const QString &theme) {
    auto lowerTheme = theme.trimmed().toLower();
    if (lowerTheme.isEmpty() || lowerTheme == "system" || lowerTheme == "modern") {
        return 0;
    } else if (lowerTheme == "modern dark" || lowerTheme == "dark" || lowerTheme == "amoled" ||
               lowerTheme == "aqua" || lowerTheme == "blacksoft") {
        return 1;
    } else if (lowerTheme == "modern light" || lowerTheme == "kawaii" ||
               lowerTheme == "flatgray" || lowerTheme == "lightblue" ||
               lowerTheme == "windowsvista") {
        return 2;
    } else {
        return 0;
    }
}

QString ThemeManager::buildModernStylesheet(bool dark) {
    if (dark) {
        return QStringLiteral(
            "QMainWindow, QDialog, QMessageBox {"
            "  background-color: #13161c;"
            "  color: #eef2f6;"
            "}"
            "QWidget {"
            "  color: #eef2f6;"
            "  selection-background-color: #2563eb;"
            "  selection-color: #ffffff;"
            "}"
            "QWidget:disabled {"
            "  color: rgba(255, 255, 255, 90);"
            "}"
            "QToolTip {"
            "  background-color: #1c212b;"
            "  color: #f1f5f9;"
            "  border: 1px solid rgba(96, 165, 250, 140);"
            "  border-radius: 5px;"
            "  padding: 4px 8px;"
            "}"
            "QMenuBar {"
            "  background-color: #13161c;"
            "  color: #e2e8f0;"
            "  border-bottom: 1px solid rgba(255, 255, 255, 18);"
            "}"
            "QMenuBar::item {"
            "  background: transparent;"
            "  padding: 5px 10px;"
            "  border-radius: 4px;"
            "}"
            "QMenuBar::item:selected {"
            "  background-color: rgba(59, 130, 246, 75);"
            "  color: #ffffff;"
            "}"
            "QMenu {"
            "  background-color: #191d26;"
            "  color: #eef2f6;"
            "  border: 1px solid rgba(255, 255, 255, 34);"
            "  border-radius: 7px;"
            "  padding: 5px;"
            "}"
            "QMenu::item {"
            "  padding: 6px 26px 6px 26px;"
            "  border-radius: 4px;"
            "}"
            "QMenu::item:selected {"
            "  background-color: rgba(59, 130, 246, 115);"
            "  color: #ffffff;"
            "}"
            "QMenu::item:disabled {"
            "  color: rgba(255, 255, 255, 80);"
            "}"
            "QMenu::separator {"
            "  height: 1px;"
            "  background: rgba(255, 255, 255, 22);"
            "  margin: 4px 8px;"
            "}"
            "QTabWidget::pane {"
            "  background-color: #151922;"
            "  border: 1px solid rgba(255, 255, 255, 24);"
            "  border-radius: 6px;"
            "  top: -1px;"
            "}"
            "QTabBar {"
            "  background: transparent;"
            "}"
            "QTabBar::tab {"
            "  background-color: rgba(255, 255, 255, 9);"
            "  color: rgba(255, 255, 255, 175);"
            "  border: 1px solid rgba(255, 255, 255, 20);"
            "  border-bottom: 2px solid transparent;"
            "  border-top-left-radius: 6px;"
            "  border-top-right-radius: 6px;"
            "  padding: 6px 14px;"
            "  margin-right: 4px;"
            "}"
            "QTabBar::tab:hover {"
            "  background-color: rgba(96, 165, 250, 32);"
            "  color: #ffffff;"
            "  border-color: rgba(96, 165, 250, 95);"
            "}"
            "QTabBar::tab:selected {"
            "  background-color: rgba(59, 130, 246, 62);"
            "  color: #ffffff;"
            "  border: 1px solid rgba(96, 165, 250, 140);"
            "  border-bottom: 2px solid #60a5fa;"
            "  font-weight: 600;"
            "}"
            "QTableView, QTableWidget, QListView, QListWidget, QTreeView, QTreeWidget {"
            "  background-color: #101318;"
            "  alternate-background-color: #141820;"
            "  border: 1px solid rgba(255, 255, 255, 24);"
            "  border-radius: 6px;"
            "  gridline-color: rgba(255, 255, 255, 14);"
            "  outline: none;"
            "}"
            "QTableView::item, QTableWidget::item, QListView::item, QListWidget::item, QTreeView::item, QTreeWidget::item {"
            "  padding: 4px 8px;"
            "  border-bottom: 1px solid rgba(255, 255, 255, 10);"
            "}"
            "QTableView::item:hover, QTableWidget::item:hover, QListView::item:hover, QListWidget::item:hover, QTreeView::item:hover, QTreeWidget::item:hover {"
            "  background-color: rgba(255, 255, 255, 14);"
            "}"
            "QTableView::item:selected, QTableView::item:selected:!active,"
            "QTableWidget::item:selected, QTableWidget::item:selected:!active {"
            "  background-color: rgba(59, 130, 246, 74);"
            "}"
            "QListView::item:selected, QListView::item:selected:!active,"
            "QListWidget::item:selected, QListWidget::item:selected:!active,"
            "QTreeView::item:selected, QTreeView::item:selected:!active,"
            "QTreeWidget::item:selected, QTreeWidget::item:selected:!active {"
            "  background-color: rgba(59, 130, 246, 90);"
            "  color: #ffffff;"
            "}"
            "QHeaderView {"
            "  background: transparent;"
            "  border: none;"
            "}"
            "QHeaderView::section:horizontal {"
            "  background-color: #181c24;"
            "  color: rgba(255, 255, 255, 220);"
            "  font-weight: 600;"
            "  padding: 6px 10px;"
            "  border: none;"
            "  border-bottom: 1px solid rgba(255, 255, 255, 28);"
            "  border-right: 1px solid rgba(255, 255, 255, 14);"
            "}"
            "QHeaderView::section:vertical {"
            "  background-color: #151921;"
            "  color: rgba(255, 255, 255, 165);"
            "  font-weight: 600;"
            "  padding: 0px 6px;"
            "  border: none;"
            "  border-bottom: 1px solid rgba(255, 255, 255, 12);"
            "  border-right: 1px solid rgba(255, 255, 255, 18);"
            "}"
            "QHeaderView::section:hover {"
            "  background-color: #212836;"
            "  color: #ffffff;"
            "}"
            "QTableCornerButton::section {"
            "  background-color: #181c24;"
            "  border: none;"
            "  border-bottom: 1px solid rgba(255, 255, 255, 28);"
            "  border-right: 1px solid rgba(255, 255, 255, 18);"
            "}"
            "QPushButton {"
            "  background-color: #1e232d;"
            "  color: #eef2f6;"
            "  border: 1px solid rgba(255, 255, 255, 32);"
            "  border-radius: 5px;"
            "  padding: 5px 14px;"
            "}"
            "QPushButton:hover {"
            "  background-color: rgba(59, 130, 246, 52);"
            "  border: 1px solid rgba(96, 165, 250, 155);"
            "  color: #ffffff;"
            "}"
            "QPushButton:pressed {"
            "  background-color: rgba(37, 99, 235, 115);"
            "  border: 1px solid #60a5fa;"
            "}"
            "QPushButton:checked {"
            "  background-color: rgba(37, 99, 235, 95);"
            "  border: 1px solid #60a5fa;"
            "  color: #ffffff;"
            "}"
            "QPushButton:default {"
            "  background-color: rgba(37, 99, 235, 195);"
            "  border: 1px solid #60a5fa;"
            "  color: #ffffff;"
            "  font-weight: 600;"
            "}"
            "QPushButton:default:hover {"
            "  background-color: rgba(59, 130, 246, 230);"
            "}"
            "QPushButton:disabled {"
            "  background-color: rgba(255, 255, 255, 7);"
            "  color: rgba(255, 255, 255, 75);"
            "  border: 1px solid rgba(255, 255, 255, 15);"
            "}"
            "QToolButton {"
            "  background-color: rgba(255, 255, 255, 10);"
            "  color: #eef2f6;"
            "  border: 1px solid rgba(255, 255, 255, 24);"
            "  border-radius: 5px;"
            "  padding: 4px 8px;"
            "}"
            "QToolButton:hover {"
            "  background-color: rgba(96, 165, 250, 42);"
            "  border: 1px solid rgba(96, 165, 250, 145);"
            "  color: #ffffff;"
            "}"
            "QToolButton:pressed {"
            "  background-color: rgba(37, 99, 235, 105);"
            "  border: 1px solid #60a5fa;"
            "}"
            "QLineEdit, QPlainTextEdit, QTextEdit {"
            "  background-color: #181c24;"
            "  color: #f1f5f9;"
            "  border: 1px solid rgba(255, 255, 255, 32);"
            "  border-radius: 5px;"
            "  padding: 4px 8px;"
            "  selection-background-color: #2563eb;"
            "  selection-color: #ffffff;"
            "}"
            "QLineEdit:hover, QPlainTextEdit:hover, QTextEdit:hover {"
            "  border: 1px solid rgba(96, 165, 250, 120);"
            "}"
            "QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus {"
            "  background-color: #1c212b;"
            "  border: 1px solid #60a5fa;"
            "}"
            "QAbstractSpinBox QLineEdit, QComboBox QLineEdit {"
            "  background: transparent;"
            "  border: none;"
            "  padding: 0px;"
            "}"
            "QTextBrowser#masterLogBrowser {"
            "  background-color: #0e1116;"
            "  border: 1px solid rgba(255, 255, 255, 24);"
            "  border-radius: 6px;"
            "  padding: 6px;"
            "}"
            "QGroupBox {"
            "  background-color: rgba(255, 255, 255, 6);"
            "  border: 1px solid rgba(255, 255, 255, 24);"
            "  border-radius: 7px;"
            "  margin-top: 12px;"
            "  padding-top: 10px;"
            "}"
            "QGroupBox::title {"
            "  subcontrol-origin: margin;"
            "  subcontrol-position: top left;"
            "  left: 10px;"
            "  padding: 0 6px;"
            "  color: #93c5fd;"
            "  font-weight: 600;"
            "}"
            "QScrollArea {"
            "  background: transparent;"
            "  border: none;"
            "}"
            "QScrollBar:vertical {"
            "  background: transparent;"
            "  width: 9px;"
            "  margin: 2px 1px 2px 1px;"
            "}"
            "QScrollBar::handle:vertical {"
            "  background: rgba(255, 255, 255, 52);"
            "  min-height: 28px;"
            "  border-radius: 4px;"
            "}"
            "QScrollBar::handle:vertical:hover {"
            "  background: rgba(96, 165, 250, 165);"
            "}"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,"
            "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
            "  background: none;"
            "  height: 0px;"
            "}"
            "QScrollBar:horizontal {"
            "  background: transparent;"
            "  height: 9px;"
            "  margin: 1px 2px 1px 2px;"
            "}"
            "QScrollBar::handle:horizontal {"
            "  background: rgba(255, 255, 255, 52);"
            "  min-width: 28px;"
            "  border-radius: 4px;"
            "}"
            "QScrollBar::handle:horizontal:hover {"
            "  background: rgba(96, 165, 250, 165);"
            "}"
            "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal,"
            "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {"
            "  background: none;"
            "  width: 0px;"
            "}"
            "QCheckBox, QRadioButton {"
            "  spacing: 6px;"
            "}");
    }

    return QStringLiteral(
        "QMainWindow, QDialog, QMessageBox {"
        "  background-color: #f4f6fa;"
        "  color: #0f172a;"
        "}"
        "QWidget {"
        "  color: #0f172a;"
        "  selection-background-color: #2563eb;"
        "  selection-color: #ffffff;"
        "}"
        "QWidget:disabled {"
        "  color: #94a3b8;"
        "}"
        "QToolTip {"
        "  background-color: #ffffff;"
        "  color: #0f172a;"
        "  border: 1px solid #93c5fd;"
        "  border-radius: 5px;"
        "  padding: 4px 8px;"
        "}"
        "QMenuBar {"
        "  background-color: #f4f6fa;"
        "  color: #0f172a;"
        "  border-bottom: 1px solid #e2e8f0;"
        "}"
        "QMenuBar::item {"
        "  background: transparent;"
        "  padding: 5px 10px;"
        "  border-radius: 4px;"
        "}"
        "QMenuBar::item:selected {"
        "  background-color: #dbeafe;"
        "  color: #1d4ed8;"
        "}"
        "QMenu {"
        "  background-color: #ffffff;"
        "  color: #0f172a;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 7px;"
        "  padding: 5px;"
        "}"
        "QMenu::item {"
        "  padding: 6px 26px 6px 26px;"
        "  border-radius: 4px;"
        "}"
        "QMenu::item:selected {"
        "  background-color: #dbeafe;"
        "  color: #1e40af;"
        "}"
        "QMenu::item:disabled {"
        "  color: #94a3b8;"
        "}"
        "QMenu::separator {"
        "  height: 1px;"
        "  background: #e2e8f0;"
        "  margin: 4px 8px;"
        "}"
        "QTabWidget::pane {"
        "  background-color: #ffffff;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  top: -1px;"
        "}"
        "QTabBar {"
        "  background: transparent;"
        "}"
        "QTabBar::tab {"
        "  background-color: #eef2f6;"
        "  color: #475569;"
        "  border: 1px solid #cbd5e1;"
        "  border-bottom: 2px solid transparent;"
        "  border-top-left-radius: 6px;"
        "  border-top-right-radius: 6px;"
        "  padding: 6px 14px;"
        "  margin-right: 4px;"
        "}"
        "QTabBar::tab:hover {"
        "  background-color: #eff6ff;"
        "  color: #1d4ed8;"
        "  border-color: #93c5fd;"
        "}"
        "QTabBar::tab:selected {"
        "  background-color: #dbeafe;"
        "  color: #1e3a8a;"
        "  border: 1px solid #60a5fa;"
        "  border-bottom: 2px solid #2563eb;"
        "  font-weight: 600;"
        "}"
        "QTableView, QTableWidget, QListView, QListWidget, QTreeView, QTreeWidget {"
        "  background-color: #ffffff;"
        "  alternate-background-color: #f8fafc;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  gridline-color: #f1f5f9;"
        "  outline: none;"
        "}"
        "QTableView::item, QTableWidget::item, QListView::item, QListWidget::item, QTreeView::item, QTreeWidget::item {"
        "  padding: 4px 8px;"
        "  border-bottom: 1px solid #f1f5f9;"
        "}"
        "QTableView::item:hover, QTableWidget::item:hover, QListView::item:hover, QListWidget::item:hover, QTreeView::item:hover, QTreeWidget::item:hover {"
        "  background-color: rgba(37, 99, 235, 14);"
        "}"
        "QTableView::item:selected, QTableView::item:selected:!active,"
        "QTableWidget::item:selected, QTableWidget::item:selected:!active {"
        "  background-color: rgba(37, 99, 235, 36);"
        "}"
        "QListView::item:selected, QListView::item:selected:!active,"
        "QListWidget::item:selected, QListWidget::item:selected:!active,"
        "QTreeView::item:selected, QTreeView::item:selected:!active,"
        "QTreeWidget::item:selected, QTreeWidget::item:selected:!active {"
        "  background-color: #dbeafe;"
        "  color: #0f172a;"
        "}"
        "QHeaderView {"
        "  background: transparent;"
        "  border: none;"
        "}"
        "QHeaderView::section:horizontal {"
        "  background-color: #f1f5f9;"
        "  color: #1e293b;"
        "  font-weight: 600;"
        "  padding: 6px 10px;"
        "  border: none;"
        "  border-bottom: 1px solid #cbd5e1;"
        "  border-right: 1px solid #e2e8f0;"
        "}"
        "QHeaderView::section:vertical {"
        "  background-color: #f8fafc;"
        "  color: #475569;"
        "  font-weight: 600;"
        "  padding: 0px 6px;"
        "  border: none;"
        "  border-bottom: 1px solid #f1f5f9;"
        "  border-right: 1px solid #e2e8f0;"
        "}"
        "QHeaderView::section:hover {"
        "  background-color: #e2e8f0;"
        "  color: #0f172a;"
        "}"
        "QTableCornerButton::section {"
        "  background-color: #f1f5f9;"
        "  border: none;"
        "  border-bottom: 1px solid #cbd5e1;"
        "  border-right: 1px solid #e2e8f0;"
        "}"
        "QPushButton {"
        "  background-color: #ffffff;"
        "  color: #0f172a;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 5px;"
        "  padding: 5px 14px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #eff6ff;"
        "  border: 1px solid #60a5fa;"
        "  color: #1d4ed8;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #dbeafe;"
        "  border: 1px solid #2563eb;"
        "}"
        "QPushButton:checked {"
        "  background-color: #dbeafe;"
        "  border: 1px solid #2563eb;"
        "  color: #1e40af;"
        "}"
        "QPushButton:default {"
        "  background-color: #2563eb;"
        "  border: 1px solid #1d4ed8;"
        "  color: #ffffff;"
        "  font-weight: 600;"
        "}"
        "QPushButton:default:hover {"
        "  background-color: #1d4ed8;"
        "}"
        "QPushButton:disabled {"
        "  background-color: #f8fafc;"
        "  color: #94a3b8;"
        "  border: 1px solid #e2e8f0;"
        "}"
        "QToolButton {"
        "  background-color: #ffffff;"
        "  color: #0f172a;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 5px;"
        "  padding: 4px 8px;"
        "}"
        "QToolButton:hover {"
        "  background-color: #eff6ff;"
        "  border: 1px solid #60a5fa;"
        "  color: #1d4ed8;"
        "}"
        "QToolButton:pressed {"
        "  background-color: #dbeafe;"
        "  border: 1px solid #2563eb;"
        "}"
        "QLineEdit, QPlainTextEdit, QTextEdit {"
        "  background-color: #ffffff;"
        "  color: #0f172a;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 5px;"
        "  padding: 4px 8px;"
        "  selection-background-color: #2563eb;"
        "  selection-color: #ffffff;"
        "}"
        "QLineEdit:hover, QPlainTextEdit:hover, QTextEdit:hover {"
        "  border: 1px solid #60a5fa;"
        "}"
        "QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus {"
        "  border: 1px solid #2563eb;"
        "}"
        "QAbstractSpinBox QLineEdit, QComboBox QLineEdit {"
        "  background: transparent;"
        "  border: none;"
        "  padding: 0px;"
        "}"
        "QTextBrowser#masterLogBrowser {"
        "  background-color: #ffffff;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  padding: 6px;"
        "}"
        "QGroupBox {"
        "  background-color: #ffffff;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 7px;"
        "  margin-top: 12px;"
        "  padding-top: 10px;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  subcontrol-position: top left;"
        "  left: 10px;"
        "  padding: 0 6px;"
        "  color: #1d4ed8;"
        "  font-weight: 600;"
        "}"
        "QScrollArea {"
        "  background: transparent;"
        "  border: none;"
        "}"
        "QScrollBar:vertical {"
        "  background: transparent;"
        "  width: 9px;"
        "  margin: 2px 1px 2px 1px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: #cbd5e1;"
        "  min-height: 28px;"
        "  border-radius: 4px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "  background: #60a5fa;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "  background: none;"
        "  height: 0px;"
        "}"
        "QScrollBar:horizontal {"
        "  background: transparent;"
        "  height: 9px;"
        "  margin: 1px 2px 1px 2px;"
        "}"
        "QScrollBar::handle:horizontal {"
        "  background: #cbd5e1;"
        "  min-width: 28px;"
        "  border-radius: 4px;"
        "}"
        "QScrollBar::handle:horizontal:hover {"
        "  background: #60a5fa;"
        "}"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal,"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {"
        "  background: none;"
        "  width: 0px;"
        "}"
        "QCheckBox, QRadioButton {"
        "  spacing: 6px;"
        "}");
}

void ThemeManager::ApplyTheme(const QString &theme, bool force) {
    static QString nativeStyleName;
    if (nativeStyleName.isEmpty() && qApp != nullptr && qApp->style() != nullptr) {
        nativeStyleName = qApp->style()->name();
    }

    if (!force && theme == current_theme) {
        return;
    }

    static const QString nekoCss = QStringLiteral(
        "\nQMessageBox {\n"
        "    messagebox-text-interaction-flags: 5;\n"
        "}\n");

    if (isModernTheme(theme)) {
        qApp->setStyleSheet(QString());
        if (!nativeStyleName.isEmpty()) {
            qApp->setStyle(nativeStyleName);
        }
        const bool dark = isDarkEffective(theme);
        qApp->setPalette(buildModernPalette(dark));
        qApp->setStyleSheet(buildModernStylesheet(dark) + nekoCss);
    } else {
        QString themePath;
        bool isFile = false;
        std::tie(themePath, isFile) = getPath(theme);

        if (!isFile) {
            qApp->setStyleSheet(QString());
            qApp->setStyle(themePath);
            if (qApp->style() != nullptr) {
                qApp->setPalette(qApp->style()->standardPalette());
            }
            qApp->setStyleSheet(nekoCss);
        } else {
            QFile f(themePath);
            if (f.open(QFile::ReadOnly | QFile::Text)) {
                QTextStream ts(&f);
                QString qss = ts.readAll();
                f.close();

                auto lowerTheme = theme.toLower();
                if (lowerTheme == "flatgray" || lowerTheme == "lightblue" ||
                    lowerTheme == "blacksoft") {
                    qss.replace(":/qss/", ":/nekobox/qss/");
                }

                qApp->setStyleSheet(qss + nekoCss);
            }
        }
    }

    current_theme = theme;
    emit themeChanged(theme);
}
