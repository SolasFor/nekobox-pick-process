#pragma once

#include <QDialog>
#include <QFileIconProvider>
#include <QHash>
#include <QIcon>
#include <QList>
#include <QString>
#include <QStringList>

QT_BEGIN_NAMESPACE
class QLineEdit;
class QPushButton;
class QTableWidget;
QT_END_NAMESPACE

// Picks one or more already running programs, so a route can be added without
// browsing the disk for the .exe first.
class ProcessPickerDialog : public QDialog {
    Q_OBJECT

public:
    explicit ProcessPickerDialog(QWidget *parent = nullptr);

    // Full paths of the picked executables, in the order they appear in the list.
    [[nodiscard]] QStringList selectedPaths() const;

private:
    struct Entry {
        QString name;
        QString path;
    };

    static QList<Entry> readRunningProcesses();

    void reload();
    void applyFilter(const QString &text);
    void activateCurrentRow();
    void updateAcceptState();
    QIcon iconForPath(const QString &path);

    QLineEdit *filterEdit = nullptr;
    QTableWidget *table = nullptr;
    QPushButton *okButton = nullptr;
    QFileIconProvider iconProvider;
    QHash<QString, QIcon> iconCache;
    QList<Entry> entries;
};
