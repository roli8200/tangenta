#include "Language.h"

#include <QApplication>
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
            {QStringLiteral("it"), QStringLiteral("Italiano")}};
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
        const QString code = ui.left(2).toLower();
        if (supported(code))
            return code;
    }
    return QStringLiteral("en");
}

void installTranslations()
{
    const QString code = activeLanguage();
    // Deutsch steht im Quelltext, braucht also nur Qts eigene Texte
    if (code != QLatin1String("de")) {
        static QTranslator app;
        if (app.load(QStringLiteral(":/i18n/tangenta_") + code))
            QApplication::installTranslator(&app);
    }
    // Qt-eigene Texte (Abbrechen, Ja/Nein, Dateidialog): erst neben der Anwendung
    // (Windows-Fassung), sonst aus der Qt-Installation (Linux)
    static QTranslator qt;
    const QString name = QStringLiteral("qt_") + code;
    if (qt.load(name, QApplication::applicationDirPath() + QStringLiteral("/translations"))
        || qt.load(name, QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        QApplication::installTranslator(&qt);
}

} // namespace vek
