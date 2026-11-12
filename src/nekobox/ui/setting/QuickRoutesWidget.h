#pragma once

#include <QList>
#include <QString>
#include <QWidget>

QT_BEGIN_NAMESPACE
class QComboBox;
class QLineEdit;
class QPushButton;
class QTableWidget;
QT_END_NAMESPACE

class QuickRoutesWidget : public QWidget {
    Q_OBJECT

public:
    explicit QuickRoutesWidget(QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *event) override;

private:
    struct OutboundChoice {
        int id;
        QString name;
    };

    QTableWidget *processTable = nullptr;
    QTableWidget *domainTable = nullptr;
    QPushButton *saveButton = nullptr;

    QWidget *makeSection(const QString &title, QTableWidget **table, const QString &firstHeader,
                         bool processSection, bool domainSection = false);

    static void setupTable(QTableWidget *table, const QStringList &headers, int outboundColumn);

    static QList<OutboundChoice> collectOutbounds();

    void fillOutboundCombo(QComboBox *combo, const QList<OutboundChoice> &choices, int selectedId) const;

    QComboBox *makeOutboundCombo(int selectedId) const;

    QComboBox *makeMethodCombo(const QString &method) const;

    QWidget *makeProcessEditor(const QString &path);

    [[nodiscard]] QString processPathAt(int row) const;

    [[nodiscard]] static int outboundAt(QTableWidget *table, int row, int outboundColumn);

    void addProcessRow(const QString &path, int outboundId);

    void addDomainRow(const QString &domain, int outboundId, const QString &method = QString());

    static void removeSelectedRows(QTableWidget *table);

    QString browseForExecutable();

    void refreshOutboundCombos();

    void loadFromStore();

    void saveToStore();
};
