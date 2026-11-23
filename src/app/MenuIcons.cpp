#include "MenuIcons.h"

#include "Icons.h"

#include <QAction>
#include <QCoreApplication>
#include <QMenu>
#include <QMenuBar>
#include <QStringList>

namespace vek {

namespace {

struct Entry {
    const char* menu; // Quelltexte des Elternmenüs, mit | getrennt
    const char* text; // Quelltexte des Eintrags, mit | getrennt
    const char* icon;
};

// Erzeugt aus der Liste der Einträge ohne Symbol (Test everyMenuEntryHasIcon, VEK_MENU_DUMP)
const Entry kEntries[] = {
    {"&Datei", "S&chliessen|Schliessen", "doc-close"},
    {"&Datei", "A&lle schliessen", "doc-close-all"},
    {"&Datei", "Speichern &unter …", "doc-save-as"},
    {"&Datei", "Im Web veröffentlichen: &HTML …", "doc-html"},
    {"&Datei", "Als &PDF freigeben …", "doc-pdf"},
    {"&Datei", "Druck&vorschau …", "doc-print-preview"},
    {"&Datei", "&Ausschiessen …", "doc-imposition"},
    {"&Datei", "&Beenden", "app-exit"},
    {"&Bearbeiten|Bearbeiten", "&Inhalte einfügen …", "edit-paste-special"},
    {"S&ymbol|Symbol", "&Neues Symbol …", "symbol-new"},
    {"S&ymbol|Symbol", "In &Objekte zurückwandeln|In Objekte zurückwandeln", "symbol-revert"},
    {"S&ymbol|Symbol", "Symbol-&Manager|Symbol-Manager", "symbol-manager"},
    {"&Bearbeiten|Bearbeiten", "&Duplizieren|Duplizieren", "edit-duplicate"},
    {"&Bearbeiten|Bearbeiten", "An &Ort und Stelle duplizieren|An Ort und Stelle duplizieren", "edit-duplicate-inplace"},
    {"&Bearbeiten|Bearbeiten", "&Eigenschaften kopieren von …", "tool-attr-pipette"},
    {"&Bearbeiten|Bearbeiten", "&Vielfach duplizieren …", "edit-step-repeat"},
    {"Alles &auswählen|Alles a&uswählen", "Alles &auswählen|Alles a&uswählen", "select-all"},
    {"Alles &auswählen|Alles a&uswählen", "&Text|Text", "select-text"},
    {"Alles &auswählen|Alles a&uswählen", "&Knoten|Knoten", "select-nodes"},
    {"&Bearbeiten|Bearbeiten", "Alles &auswählen|Alles a&uswählen", "select-all"},
    {"&Suchen und ersetzen", "Objekte &suchen …", "edit-find"},
    {"&Suchen und ersetzen", "Text &ersetzen …", "edit-replace"},
    {"&Bearbeiten|Bearbeiten", "&Suchen und ersetzen", "edit-find"},
    {"&Ansicht", "Ein&facher Umriss|Einfacher Umriss", "view-wireframe-simple"},
    {"&Ansicht", "&Umriss|Umriss", "view-wireframe"},
    {"&Ansicht", "&Entwurf|Entwurf", "view-draft"},
    {"&Ansicht", "&Normal|Normal", "view-normal"},
    {"&Ansicht", "Er&weitert|Erweitert", "view-enhanced"},
    {"&Ansicht", " Pixel|&Pixel|Pixel", "view-pixel"},
    {"&Ansicht", "A&lle Seiten einpassen", "pages-all"},
    {"&Ansicht", "&Vorschau markierter Objekte", "view-preview-selected"},
    {"&Ansicht", "Auf &Auswahl zoomen", "zoom-selection"},
    {"&Ansicht", "Auf a&lle Objekte zoomen", "zoom-all"},
    {"&Ansicht", "&Lineale|Lineale", "view-rulers"},
    {"&Ansicht", "&Hilfslinien|Hilfslinien", "view-guides"},
    {"&Ansicht", "&Gitter|G&itter|Gitter", "view-grid"},
    {"&Ansicht", "G&rundlinienraster|Grundlinienraster", "baseline-grid"},
    {"A&usrichten an|Ausrichten an", "&Hilfslinien|Hilfslinien", "snap-guides"},
    {"A&usrichten an|Ausrichten an", "&Objekte|Objekte", "snap-objects"},
    {"A&usrichten an|Ausrichten an", "&Seite|Seite", "snap-page"},
    {"A&usrichten an|Ausrichten an", "&Gitter|G&itter|Gitter", "snap-grid"},
    {"A&usrichten an|Ausrichten an", "&Dynamische Hilfslinien|Dynamische Hilfslinien", "snap-dynamic"},
    {"A&usrichten an|Ausrichten an", "Aus&richtungshilfslinien|Ausrichtungshilfslinien", "snap-alignment"},
    {"A&usrichten an|Ausrichten an", "G&rundlinienraster|Grundlinienraster", "snap-baseline"},
    {"A&usrichten an|Ausrichten an", "Einstellungen …", "page-settings"},
    {"&Ansicht", "A&usrichten an|Ausrichten an", "snap"},
    {"&Ansicht", "&Farbtiefe des Bildschirms …", "view-color-depth"},
    {"&Ansicht", "&Oberflächengrösse|Oberflächengrösse", "view-ui-size"},
    {"&Layout|Layout", "Seite &umbenennen …", "page-rename"},
    {"&Layout|Layout", "Seite &verschieben …", "page-move"},
    {"&Layout|Layout", "Vorige Seite", "nav-prev"},
    {"&Layout|Layout", "Nächste Seite", "nav-next"},
    {"&Layout|Layout", "Seitenausrichtung &wechseln", "page-orientation"},
    {"An&ordnen", "&Gruppieren|Gruppieren", "arrange-group"},
    {"An&ordnen", "Gruppierung &aufheben|Gruppierung aufheben", "arrange-ungroup"},
    {"An&ordnen", "Gruppierung a&ller Objekte aufheben|Gruppierung aller Objekte aufheben", "arrange-ungroup-all"},
    {"&Formen|Formen", "F&ormen …", "shaping"},
    {"An&ordnen", "Auf &Masterseite verschieben|Auf Masterseite verschieben", "page-to-master"},
    {"An&ordnen", "Auf aktuelle &Seite verschieben", "page-from-master"},
    {"&Anpassen", "&Tonkurve …", "adjust-tone-curve"},
    {"Effe&kte|Effekte", "&Anpassen", "adjust-menu"},
    {"Effe&kte|Effekte", "&Korrektur", "correction-menu"},
    {"Super&Clip", "In &Rahmen platzieren …", "fx-superclip"},
    {"Super&Clip", "Inhalt e&xtrahieren|Inhalt extrahieren", "clip-extract"},
    {"Super&Clip", "&Leeren SuperClip-Rahmen erstellen|Leeren SuperClip-Rahmen erstellen", "clip-empty"},
    {"Super&Clip", "Inhalt in SuperClip &sperren|Inhalt in SuperClip sperren", "clip-lock"},
    {"Super&Clip", "Inhalt &zentrieren", "clip-center"},
    {"Super&Clip", "Inhalt proportional &einpassen", "clip-fit"},
    {"Super&Clip", "Rahmen proportional &füllen", "clip-fill"},
    {"Super&Clip", "Inhalt auf Rahmengrösse &strecken", "clip-stretch"},
    {"Effekt &kopieren|Effekt kopieren", "Effekte von …", "fx-copy"},
    {"Effe&kte|Effekte", "Effekt &kopieren|Effekt kopieren", "fx-copy"},
    {"Effekt kl&onen|Effekt klonen", "Effekte von …", "fx-clone"},
    {"Effe&kte|Effekte", "Effekt kl&onen|Effekt klonen", "fx-clone"},
    {"&Bitmaps|Bitmaps", "Bild &geraderichten …", "bitmap-straighten"},
    {"&Bitmaps|Bitmaps", "&Umrissvektorisierung|Umrissvektorisierung", "bitmap-trace"},
    {"&Bitmaps|Bitmaps", "Bitmap &zuschneiden|Bitmap zuschneiden", "bitmap-crop"},
    {"&Bitmaps|Bitmaps", "Bitmap-&Farbmaske …", "bitmap-color-mask"},
    {"&Bitmaps|Bitmaps", "&Modus", "bitmap-mode"},
    {"&Bitmaps|Bitmaps", "Bitmap &neu aufbauen …", "bitmap-resample"},
    {"&Bitmaps|Bitmaps", "&3D-Effekte", "bitmap-3d"},
    {"&Bitmaps|Bitmaps", "Künstlerische &Striche", "bitmap-art"},
    {"&Bitmaps|Bitmaps", "&Unschärfe|Unschärfe", "bitmap-blur"},
    {"&Bitmaps|Bitmaps", "&Farbtransformation", "bitmap-color"},
    {"&Bitmaps|Bitmaps", "&Kontur|Kontur", "bitmap-contour"},
    {"&Bitmaps|Bitmaps", "Kr&eativ", "bitmap-creative"},
    {"&Bitmaps|Bitmaps", "&Verzerren|Verzerren", "bitmap-distort"},
    {"&Bitmaps|Bitmaps", "&Rauschen", "bitmap-noise"},
    {"&Bitmaps|Bitmaps", "&Schärfe", "bitmap-sharpen"},
    {"&Text|Text", "Text &begradigen|Text begradigen", "text-straighten"},
    {"&Mengentextrahmen|Mengentextrahmen", "&Verknüpfen|Verknüpfen", "text-link"},
    {"&Mengentextrahmen|Mengentextrahmen", "Verknüpfung &aufheben|Verknüpfung aufheben", "text-unlink"},
    {"&Text|Text", "&Mengentextrahmen|Mengentextrahmen", "text-frame"},
    {"&Text|Text", "&Rechtschreibprüfung …", "text-spell"},
    {"&Tabelle|Ta&belle|Tabelle", "&Text in Tabelle konvertieren …", "table-from-text"},
    {"&Einfügen|Einfügen", "Zeile &oben", "table-insert-row-above"},
    {"&Einfügen|Einfügen", "Zeile &unten", "table-insert-row-below"},
    {"&Einfügen|Einfügen", "Spalte &links", "table-insert-col-left"},
    {"&Einfügen|Einfügen", "Spalte &rechts", "table-insert-col-right"},
    {"&Einfügen|Einfügen", "&Zeilen einfügen …", "table-insert-rows"},
    {"&Einfügen|Einfügen", "&Spalten einfügen …", "table-insert-cols"},
    {"&Tabelle|Ta&belle|Tabelle", "&Einfügen|Einfügen", "table-insert"},
    {"A&uswählen|Auswählen", "&Tabelle|Ta&belle|Tabelle", "table-select-table"},
    {"A&uswählen|Auswählen", "&Zeile", "table-select-row"},
    {"A&uswählen|Auswählen", "&Spalte", "table-select-col"},
    {"&Tabelle|Ta&belle|Tabelle", "A&uswählen|Auswählen", "table-select"},
    {"&Löschen|Löschen", "&Zeile", "table-delete-row"},
    {"&Löschen|Löschen", "&Spalte", "table-delete-col"},
    {"&Löschen|Löschen", "&Tabelle|Ta&belle|Tabelle", "table-delete-table"},
    {"&Tabelle|Ta&belle|Tabelle", "&Löschen|Löschen", "table-delete"},
    {"&Verteilen|Verteilen", "Zeilen &gleichmässig verteilen", "table-distribute-rows"},
    {"&Verteilen|Verteilen", "Spalten gleichmässig &verteilen", "table-distribute-cols"},
    {"&Tabelle|Ta&belle|Tabelle", "&Verteilen|Verteilen", "table-distribute"},
    {"&Tabelle|Ta&belle|Tabelle", "In &Zeilen aufteilen …", "table-split-rows"},
    {"&Tabelle|Ta&belle|Tabelle", "In &Spalten aufteilen …", "table-split-cols"},
    {"&Tabelle|Ta&belle|Tabelle", "Zellen &teilen|Zellen teilen", "table-split-cells"},
    {"&Tabelle|Ta&belle|Tabelle", "Tabelle in Te&xt konvertieren …", "table-to-text"},
    {"E&xtras", "&Optionen …|Optionen …", "options"},
    {"E&xtras", "&KI-Assistent", "ai-assistant"},
    {"&Skripte|Skripte", "Skript-&Editor", "script-editor"},
    {"&Skripte|Skripte", "Skript &ausführen …", "script-run"},
    {"&Skripte|Skripte", "Skript&ordner öffnen|Skriptordner öffnen", "script-folder"},
    {"E&xtras", "&Skripte|Skripte", "script-run"},
    {"&Fenster", "Ü&berlappend", "win-cascade"},
    {"&Fenster", "&Nebeneinander", "win-tile"},
    {"&Fenster", "Nä&chstes Fenster", "win-next"},
    {"&Fenster", "&Vorheriges Fenster", "win-prev"},
    {"&Fenster", "S&chliessen|Schliessen", "doc-close"},
    {"&Fenster", "A&lle schliessen", "doc-close-all"},
    {"&Andockfenster|Andockfenster", "Objekt-Manager", "docker-objects"},
    {"&Andockfenster|Andockfenster", "&Formen|Formen", "shaping"},
    {"&Andockfenster|Andockfenster", "&Objekteigenschaften|Objekteigenschaften", "object-properties"},
    {"&Andockfenster|Andockfenster", "Symbol-&Manager|Symbol-Manager", "symbol-manager"},
    {"&Andockfenster|Andockfenster", "Ä&nderungen|Änderungen", "docker-history"},
    {"&Andockfenster|Andockfenster", "&Skripte|Skripte", "script-editor"},
    {"&Andockfenster|Andockfenster", "Linse", "fx-lens"},
    {"&Andockfenster|Andockfenster", "&Hülle|Hülle", "tool-envelope"},
    {"&Andockfenster|Andockfenster", "Ecken", "corner-round"},
    {"&Andockfenster|Andockfenster", "Effe&kte|Effekte", "docker-effects"},
    {"&Andockfenster|Andockfenster", "Künstlerische Medien", "tool-media"},
    {"&Andockfenster|Andockfenster", "Seiten", "pages-list"},
    {"&Andockfenster|Andockfenster", "KI", "ai-assistant"},
    {"&Fenster", "&Andockfenster|Andockfenster", "docker"},
};

QString plain(QString text)
{
    // Kürzel-& entfernen, "&&" bleibt ein &
    text.replace(QStringLiteral("&&"), QStringLiteral("\x01"));
    text.remove(QLatin1Char('&'));
    text.replace(QLatin1Char('\x01'), QLatin1Char('&'));
    // Sprache: " (Language)" hängt am Titel
    if (text.endsWith(QLatin1String(" (Language)")))
        text.chop(11);
    return text.trimmed();
}

bool matches(const char* sources, const QString& shown)
{
    for (const QString& s : QString::fromUtf8(sources).split(QLatin1Char('|'))) {
        const QByteArray utf8 = s.toUtf8();
        if (plain(QCoreApplication::translate("vek::MainWindow", utf8.constData())) == shown)
            return true;
    }
    return false;
}

void assign(QMenu* menu)
{
    const QString title = plain(menu->title());
    for (QAction* a : menu->actions()) {
        if (a->isSeparator())
            continue;
        if (QMenu* sub = a->menu())
            assign(sub);
        if (!a->icon().isNull() || a->property("dynamicEntry").toBool())
            continue;
        const QString text = plain(a->text());
        for (const Entry& e : kEntries) {
            if (matches(e.text, text) && matches(e.menu, title)) {
                a->setIcon(icon(QString::fromLatin1(e.icon)));
                break;
            }
        }
    }
}

} // namespace

void assignMenuIcons(QMenu* menu)
{
    if (menu)
        assign(menu);
}

void assignMenuIcons(QMenuBar* bar)
{
    for (QAction* top : bar->actions())
        if (QMenu* m = top->menu())
            assign(m);
}

} // namespace vek
