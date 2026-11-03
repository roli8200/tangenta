#include "Language.h"

#include <QApplication>
#include <QFont>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QTranslator>

namespace vek {

namespace {

const QString kSettingsKey = QStringLiteral("ui/language");

bool supported(const QString& code)
{
    for (const Language& l : languages())
        if (l.code == code)
            return true;
    return false;
}

} // namespace

QList<Language> languages()
{
    return {{QStringLiteral("de"), QStringLiteral("Deutsch")},
            {QStringLiteral("en"), QStringLiteral("English")},
            {QStringLiteral("fr"), QStringLiteral("Français")},
            {QStringLiteral("it"), QStringLiteral("Italiano")},
            {QStringLiteral("es"), QStringLiteral("Español")},
            {QStringLiteral("pt"), QStringLiteral("Português")},
            {QStringLiteral("zh"), QStringLiteral("简体中文")}};
}

QString languageSetting()
{
    const QString code = QSettings().value(kSettingsKey).toString();
    return supported(code) ? code : QString();
}

void setLanguageSetting(const QString& code)
{
    QSettings settings;
    if (supported(code))
        settings.setValue(kSettingsKey, code);
    else
        settings.remove(kSettingsKey);
}

QString activeLanguage()
{
    const QString chosen = languageSetting();
    if (!chosen.isEmpty())
        return chosen;
    // Erste Systemsprache, die TANGENTA kann ("de-CH" -> "de")
    for (const QString& ui : QLocale::system().uiLanguages()) {
        // Traditionelles Chinesisch (Taiwan, Hongkong) nicht mit der vereinfachten Fassung bedienen
        if (ui.startsWith(QLatin1String("zh"), Qt::CaseInsensitive)
            && (ui.contains(QLatin1String("Hant"), Qt::CaseInsensitive) || ui.contains(QLatin1String("TW"), Qt::CaseInsensitive)
                || ui.contains(QLatin1String("HK"), Qt::CaseInsensitive) || ui.contains(QLatin1String("MO"), Qt::CaseInsensitive)))
            continue;
        const QString code = ui.left(2).toLower();
        if (supported(code))
            return code;
    }
    return QStringLiteral("en");
}

void applyLanguageFont(const QString& code)
{
    static const QFont original = QApplication::font();
    QFont font = original;
    if (code == QLatin1String("zh"))
        font.setFamilies({original.family(), QStringLiteral("Microsoft YaHei UI"), QStringLiteral("Microsoft YaHei"),
                          QStringLiteral("PingFang SC"), QStringLiteral("Noto Sans CJK SC"), QStringLiteral("Source Han Sans SC"),
                          QStringLiteral("WenQuanYi Micro Hei"), QStringLiteral("SimSun")});
    QApplication::setFont(font);
}

void installTranslations()
{
    const QString code = activeLanguage();
    applyLanguageFont(code);
    // Deutsch steht im Quelltext, braucht also nur Qts eigene Texte
    if (code != QLatin1String("de")) {
        static QTranslator app;
        if (app.load(QStringLiteral(":/i18n/tangenta_") + code))
            QApplication::installTranslator(&app);
    }
    // Qt-eigene Texte (Abbrechen, Ja/Nein, Dateidialog): erst neben der Anwendung
    // (Windows-Fassung), sonst aus der Qt-Installation (Linux)
    static QTranslator qt;
    // Qt führt Chinesisch als zh_CN und Portugiesisch als pt_BR
    const QString qtCode = code == QLatin1String("zh") ? QStringLiteral("zh_CN") : code == QLatin1String("pt") ? QStringLiteral("pt_BR") : code;
    const QString name = QStringLiteral("qt_") + qtCode;
    if (qt.load(name, QApplication::applicationDirPath() + QStringLiteral("/translations"))
        || qt.load(name, QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        QApplication::installTranslator(&qt);
}

} // namespace vek
