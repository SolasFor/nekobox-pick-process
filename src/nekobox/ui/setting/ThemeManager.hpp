#pragma once

#include <QMap>
#include <QObject>
#include <QString>
#include <nekobox/dataStore/Configs.hpp>

class ThemeManager : public QObject {
    Q_OBJECT
public:
    static QMap<QString, QString> &getThemes();
    QString current_theme = "";
    void ApplyTheme(const QString &theme, bool force = false);

    static std::tuple<QString, bool> getPath(const QString &theme);
    static int getMode(const QString &theme);
    static bool isModernTheme(const QString &theme);
    static bool isDarkEffective(const QString &theme);
    static QString buildModernStylesheet(bool dark);

signals:
    void themeChanged(QString themeName);
};

extern ThemeManager *themeManager;
