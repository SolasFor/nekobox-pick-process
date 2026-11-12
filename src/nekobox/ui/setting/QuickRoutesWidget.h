#pragma once

#include <QFileIconProvider>
#include <QList>
#include <QString>
#include <QWidget>

QT_BEGIN_NAMESPACE
class QComboBox;
class QEvent;
class QFrame;
class QLabel;
class QLineEdit;
class QPushButton;
class QShowEvent;
class QTableWidget;
QT_END_NAMESPACE

class QuickRoutesWidget : public QWidget {
    Q_OBJECT

public:
    explicit QuickRoutesWidget(QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct OutboundChoice {
        int id;
        QString name;
    };

    QTableWidget *processTable = nullptr;
    QTableWidget *domainTable = nullptr;
    QLabel *processCountBadge = nullptr;
    QLabel *domainCountBadge = nullptr;
    QPushButton *processRemoveButton = nullptr;
    QPushButton *domainRemoveButton = nullptr;
    QPushButton *saveButton = nullptr;
    int hoveredProcessRow = -1;
    int hoveredDomainRow = -1;
    bool applyingStyle = false;
    QFileIconProvider iconProvider;

    QWidget *makeSection(const QString &title, QTableWidget **table, QLabel **countBadge,
                         QPushButton **removeButtonOut, const QString &firstHeader,
                         bool processSection, bool domainSection = false);

    void setupTable(QTableWidget *table, const QStringList &headers, int outboundColumn);

    void applyVisualStyle();

    QFrame *wrapCellContent(QTableWidget *table, QWidget *inner, bool firstCol,
                            int leftMargin = 8, int rightMargin = 8);

    void registerRowSurface(QWidget *widget);

    void registerRowEditor(QWidget *widget);

    [[nodiscard]] static int rowForWidget(QTableWidget *table, QWidget *widget);

    [[nodiscard]] QTableWidget *ownerTableFor(QWidget *widget) const;

    void selectRowFromClick(QTableWidget *table, int row, Qt::KeyboardModifiers modifiers,
                            bool focusTable);

    void syncSectionState(QTableWidget *table);

    void syncAllSections();

    static QList<OutboundChoice> collectOutbounds();

    void fillOutboundCombo(QComboBox *combo, const QList<OutboundChoice> &choices, int selectedId) const;

    QComboBox *makeOutboundCombo(int selectedId);

    QComboBox *makeMethodCombo(const QString &method);

    QWidget *makeProcessEditor(const QString &path);

    QWidget *makeDomainEditor(const QString &domain, QComboBox *methodCombo);

    void updateProcessChip(QWidget *chip, const QString &rawPath);

    [[nodiscard]] QString processPathAt(int row) const;

    [[nodiscard]] static int outboundAt(QTableWidget *table, int row, int outboundColumn);

    void addProcessRow(const QString &path, int outboundId);

    void addDomainRow(const QString &domain, int outboundId, const QString &method = QString());

    void removeSelectedRows(QTableWidget *table);

    QString browseForExecutable();

    void refreshOutboundCombos();

    void loadFromStore();

    void saveToStore();
};
