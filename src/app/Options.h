#pragma once

#include "AutoCorrect.h"

#include "Page.h"

#include <QColor>
#include <QObject>
#include <QVariant>
#include <QVariantMap>

namespace vek {

class Editor;

// Einstellungen des Dialogs "Optionen" (Extras > Optionen, Strg+J).
//
// Jede Einstellung hat einen Schlüssel ("ws/...", "global/..." oder "doc/...") und eine Vorgabe
// (Options.cpp, Werte). Arbeitsbereich und Global gelten je Benutzer und
// stehen in QSettings ("options/<schlüssel>"); Dokument-Einstellungen gehören zur Datei und
// stehen auf der Masterseite (Änderung = ein Undo-Schritt "Optionen").
class Options : public QObject {
    Q_OBJECT

public:
    static Options& instance();

    // Dokument-Einstellungen kommen aus dem Dokument dieses Editors
    void attach(Editor* editor);

    bool known(const QString& key) const;
    bool isDocument(const QString& key) const { return key.startsWith(QLatin1String("doc/")); }
    QVariant defaultValue(const QString& key) const;
    QVariant value(const QString& key) const;
    bool flag(const QString& key) const { return value(key).toBool(); }
    double number(const QString& key) const { return value(key).toDouble(); }
    int integer(const QString& key) const { return value(key).toInt(); }
    QString text(const QString& key) const { return value(key).toString(); }
    QColor color(const QString& key) const { return QColor(value(key).toString()); }

    // Mehrere Werte auf einmal (OK im Dialog); Dokument-Werte als ein Undo-Schritt
    void apply(const QVariantMap& values);
    void set(const QString& key, const QVariant& value) { apply({{key, value}}); }
    // Blitzkorrektur aus Optionen > Text > Blitzkorrektur
    autocorrect::Settings autocorrectSettings() const;
    // Arbeitsbereich und Global auf die Vorgaben zurücksetzen (Tests)
    void resetWorkspace();

    // Lineal-Einheiten (Index = doc/rulers/units)
    static QStringList unitNames();
    static QString unitSymbol(int unit);
    static double unitInMm(int unit);
    // Seiten-mm -> Linealwert (Ursprung, Zeichnungsmassstab, Einheit); horizontal bzw. senkrecht
    double rulerValue(double mm, bool horizontal) const;
    int rulerUnit(bool horizontal) const;
    // Position wie in der Statusleiste, z.B. "( 12.500 ; 40.000 ) mm"
    QString positionText(const QPointF& mm) const;

    // Hilfslinien einer Voreinstellung (Optionen > Hilfslinien > Voreinstellungen) für eine Seite
    QList<Guide> presetGuides(const QSizeF& page) const;

signals:
    // Nach jeder Änderung, auch wenn das Dokument wechselt oder Undo Dokument-Werte ändert
    void changed();

private:
    Options();
    Editor* m_editor = nullptr;
};

} // namespace vek
