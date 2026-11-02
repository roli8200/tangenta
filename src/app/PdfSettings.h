#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace vek {

// Einstellungen für "Als PDF freigeben" (Registerkarten Allgemein, Farbe,
// Dokument, Objekte, Druckvorstufe, Sicherheit). Vordefinierte und eigene Voreinstellungen.
struct PdfSettings {
    // Allgemein
    enum class Range { Document, Selection, CurrentPage, Pages };
    Range range = Range::Document;
    QString pages = QStringLiteral("1");     // "1-3, 5"
    bool sizeFromSelection = false;          // Seitengrösse durch die markierten Objekte
    enum class Compatibility { Acrobat4, Acrobat5, Acrobat6, Acrobat7, Acrobat8, Acrobat9, PdfX1a, PdfX3, PdfA1b, PdfA2b };
    Compatibility compatibility = Compatibility::Acrobat6;

    // Farbe
    enum class Colors { Native, Rgb, Cmyk, Gray };
    Colors colors = Colors::Native;
    bool useProofSettings = false;           // Farbprüfeinstellungen statt Dokumentfarben
    bool embedProfile = true;
    bool keepOverprint = true;

    // Dokument
    QString author;
    QString keywords;
    QString subject;
    bool fastWebView = false;
    bool bookmarks = true;                   // je Seite ein Lesezeichen (Seitenname)
    bool thumbnails = false;                 // Skizzen
    enum class StartView { PageOnly, FullScreen, Bookmarks, Thumbnails };
    StartView startView = StartView::PageOnly;
    bool ascii85 = false;

    // Objekte
    enum class Compression { None, Zip, Jpeg };
    Compression compression = Compression::Jpeg;
    int jpegQuality = 90;                    // 0..100 (100 = beste Qualität)
    bool downsampleColor = true;
    int colorDpi = 200;
    bool downsampleGray = true;
    int grayDpi = 200;
    bool downsampleMono = true;
    int monoDpi = 600;
    bool compressText = true;
    bool textAsCurves = false;
    bool embedFonts = true;
    bool embedBase14 = true;
    bool subsetFonts = true;
    int subsetBelow = 80;                    // Teilmenge, wenn weniger als so viel % benutzt
    bool complexFillsAsBitmaps = false;

    // Druckvorstufe
    bool bleedLimit = false;
    double bleed = 3.175;                    // mm
    bool cropMarks = false;
    bool registrationMarks = false;
    bool fileInfo = false;
    bool densitometer = false;

    // Sicherheit
    bool openPassword = false;
    QString openPasswordText;
    bool permissionPassword = false;
    QString permissionPasswordText;
    enum class Printing { None, LowResolution, HighResolution };
    Printing printing = Printing::HighResolution;
    enum class Changes { None, Assembly, FormFill, Comments, AllButExtract };
    Changes changes = Changes::None;
    bool allowCopy = true;

    // Vorabkontrolle
    bool preflightIgnore = false;            // "Nicht mehr überprüfen"

    bool marksNeeded() const { return cropMarks || registrationMarks || fileInfo || densitometer; }
    bool isPdfX() const { return compatibility == Compatibility::PdfX1a || compatibility == Compatibility::PdfX3; }
    bool isPdfA() const { return compatibility == Compatibility::PdfA1b || compatibility == Compatibility::PdfA2b; }
    QString compatibilityLevel() const; // "1.3" .. "1.7"

    QJsonObject toJson() const;
    static PdfSettings fromJson(const QJsonObject& o);

    // Voreinstellungen: eingebaute und eigene (QSettings)
    static QStringList builtinPresets();
    static QStringList presets();       // eingebaute + eigene
    static bool isBuiltin(const QString& name);
    static PdfSettings preset(const QString& name);
    static void savePreset(const QString& name, const PdfSettings& settings);
    static void removePreset(const QString& name);
    // Zuletzt verwendete Einstellungen und Voreinstellung
    static PdfSettings last(QString* presetName = nullptr);
    static void remember(const PdfSettings& settings, const QString& presetName);
};

// Seitenliste "1-3, 5, 8-" -> 0-basierte Indizes (ungültige Teile werden übergangen)
QList<int> parsePageList(const QString& text, int pageCount);

} // namespace vek
