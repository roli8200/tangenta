#include "Options.h"

#include "Editor.h"
#include "Transaction.h"
#include "Page.h"

#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QSettings>

namespace vek {

namespace {

// Vorgaben (aus dem Optionen-Dialog, siehe docs/optionen.md).
// Auswahllisten und Optionsfelder speichern den Index.
const QHash<QString, QVariant>& defaults()
{
    static const QHash<QString, QVariant> table = {
        // --- Arbeitsbereich > Allgemein
        {QStringLiteral("ws/general/startup"), 2},
        {QStringLiteral("ws/general/newDocDialog"), false},
        {QStringLiteral("ws/general/undo"), 150},
        {QStringLiteral("ws/general/undoBitmap"), 2},
        {QStringLiteral("ws/general/centerDialogs"), true},
        {QStringLiteral("ws/general/dockerTitles"), true},
        {QStringLiteral("ws/general/autoPopup"), false},
        {QStringLiteral("ws/general/sounds"), true},
        // --- Anzeige
        {QStringLiteral("ws/display/proof"), false},
        {QStringLiteral("ws/display/interruptRedraw"), true},
        {QStringLiteral("ws/display/manualRefresh"), false},
        {QStringLiteral("ws/display/offscreen"), true},
        {QStringLiteral("ws/display/autopan"), true},
        {QStringLiteral("ws/display/tooltips"), true},
        {QStringLiteral("ws/display/nodeTracking"), false},
        {QStringLiteral("ws/display/postscript"), true},
        {QStringLiteral("ws/display/antialiasBitmaps"), true},
        {QStringLiteral("ws/display/hideCurveBox"), false},
        {QStringLiteral("ws/display/highlightOutline"), false},
        {QStringLiteral("ws/display/fullPreview"), 1},
        {QStringLiteral("ws/display/gradientSteps"), 256},
        {QStringLiteral("ws/display/wheel"), 0},
        // --- Bearbeiten
        {QStringLiteral("ws/edit/angleStep"), 15.0},
        {QStringLiteral("ws/edit/precision"), 3},
        {QStringLiteral("ws/edit/extrudeFacet"), 1.0},
        {QStringLiteral("ws/edit/nodeSize"), 0},
        {QStringLiteral("ws/edit/saveFacet"), false},
        {QStringLiteral("ws/edit/dblClickBitmap"), true},
        {QStringLiteral("ws/edit/curveDirection"), true},
        {QStringLiteral("ws/edit/superclipCenter"), 0},
        // --- An Objekten ausrichten
        {QStringLiteral("ws/snap/enabled"), true},
        {QStringLiteral("ws/snap/radius"), 10},
        {QStringLiteral("ws/snap/markers"), true},
        {QStringLiteral("ws/snap/screentip"), true},
        {QStringLiteral("ws/snap/page"), false},
        {QStringLiteral("ws/snap/modes"), QStringLiteral("node,intersection,midpoint,quadrant,tangent,perpendicular,edge,center,baseline")},
        // --- Dynamische Hilfslinien
        {QStringLiteral("ws/dynguides/enabled"), false},
        {QStringLiteral("ws/dynguides/snapToMarks"), true},
        {QStringLiteral("ws/dynguides/angleTip"), true},
        {QStringLiteral("ws/dynguides/distance"), 2.5},
        {QStringLiteral("ws/dynguides/distanceTip"), true},
        {QStringLiteral("ws/dynguides/angles"), QStringLiteral("0,45,90,135")},
        {QStringLiteral("ws/dynguides/angleList"), QStringLiteral("0,30,45,60,90,120,135,150")},
        {QStringLiteral("ws/dynguides/extendSegment"), true},
        // --- Ausrichtungshilfslinien
        {QStringLiteral("ws/alignguides/enabled"), false},
        {QStringLiteral("ws/alignguides/color"), QStringLiteral("#ff00ff")},
        {QStringLiteral("ws/alignguides/scope"), 0},
        {QStringLiteral("ws/alignguides/centerEdges"), true},
        {QStringLiteral("ws/alignguides/useMargins"), false},
        {QStringLiteral("ws/alignguides/margin"), 5.0},
        {QStringLiteral("ws/alignguides/marginScope"), 1},
        // --- Warnungen (ausgeschaltete, durch Komma getrennt)
        {QStringLiteral("ws/warnings/off"), QString()},
        // --- VBA
        {QStringLiteral("ws/scripts/folder"), QString()},
        {QStringLiteral("ws/edit/joinTolerance"), 0.5},
        {QStringLiteral("ws/scripts/timeout"), 30},
        // --- Speichern
        {QStringLiteral("ws/save/autosave"), true},
        {QStringLiteral("ws/save/autosaveMinutes"), 20},
        {QStringLiteral("ws/save/folderMode"), 0},
        {QStringLiteral("ws/save/folder"), QString()},
        {QStringLiteral("ws/save/textMode"), 0},
        {QStringLiteral("ws/save/askBeforeSave"), true},
        {QStringLiteral("ws/save/backupOnSave"), true},
        // --- SuperTrace
        {QStringLiteral("ws/trace/quick"), 0},
        {QStringLiteral("ws/trace/performance"), 50},
        {QStringLiteral("ws/trace/mergeMode"), 0},
        {QStringLiteral("ws/trace/lastPreset"), 1}, // zuletzt im Dialog benutzter Bildtyp (Logo)
        // --- Plugins
        {QStringLiteral("ws/plugins/folders"), QString()},
        // --- Text
        {QStringLiteral("ws/text/frames/showLinks"), true},
        {QStringLiteral("ws/text/frames/showFrames"), true},
        {QStringLiteral("ws/text/frames/autoFit"), false},
        {QStringLiteral("ws/text/frames/formatScope"), 2},
        {QStringLiteral("ws/text/frames/webCompatible"), false},
        {QStringLiteral("ws/text/fonts/truetype"), true},
        {QStringLiteral("ws/text/fonts/ttSymbols"), true},
        {QStringLiteral("ws/text/fonts/type1"), true},
        {QStringLiteral("ws/text/fonts/t1Symbols"), true},
        {QStringLiteral("ws/text/fonts/docFonts"), false},
        {QStringLiteral("ws/text/fonts/sampleFlyout"), false},
        {QStringLiteral("ws/text/fonts/styleFlyout"), true},
        {QStringLiteral("ws/text/fonts/previewName"), true},
        {QStringLiteral("ws/text/fonts/recent"), 5},
        {QStringLiteral("ws/text/fonts/matching"), 1},
        {QStringLiteral("ws/text/spell/auto"), true},
        {QStringLiteral("ws/text/spell/visibility"), 1},
        {QStringLiteral("ws/text/spell/suggestions"), 10},
        {QStringLiteral("ws/text/spell/addToAutocorrect"), false},
        {QStringLiteral("ws/text/spell/showIgnored"), true},
        {QStringLiteral("ws/text/autocorrect/capSentence"), false},
        {QStringLiteral("ws/text/autocorrect/twoCaps"), true},
        {QStringLiteral("ws/text/autocorrect/dayNames"), true},
        {QStringLiteral("ws/text/autocorrect/hyperlink"), true},
        {QStringLiteral("ws/text/autocorrect/language"), 0},
        {QStringLiteral("ws/text/autocorrect/typoQuotes"), true},
        {QStringLiteral("ws/text/autocorrect/numbersStraight"), true},
        {QStringLiteral("ws/text/autocorrect/singleOpen"), QStringLiteral("‹")},
        {QStringLiteral("ws/text/autocorrect/singleClose"), QStringLiteral("›")},
        {QStringLiteral("ws/text/autocorrect/doubleOpen"), QStringLiteral("«")},
        {QStringLiteral("ws/text/autocorrect/doubleClose"), QStringLiteral("»")},
        {QStringLiteral("ws/text/autocorrect/replace"), true},
        {QStringLiteral("ws/text/autocorrect/replacements"), QString()},
        // --- Hilfsmittelpalette
        {QStringLiteral("ws/tools/dimension/style"), 0},
        {QStringLiteral("ws/tools/dimension/precision"), 2},
        {QStringLiteral("ws/tools/dimension/units"), 0},
        {QStringLiteral("ws/tools/dimension/prefix"), QString()},
        {QStringLiteral("ws/tools/dimension/suffix"), QString()},
        {QStringLiteral("ws/tools/shapeRecognition/delay"), 1.0},
        {QStringLiteral("ws/tools/freehand/smoothing"), 50},
        {QStringLiteral("ws/tools/freehand/corner"), 5},
        {QStringLiteral("ws/tools/freehand/straight"), 5},
        {QStringLiteral("ws/tools/freehand/autoJoin"), 5},
        {QStringLiteral("ws/tools/pick/crosshair"), false},
        {QStringLiteral("ws/tools/pick/treatFilled"), true},
        {QStringLiteral("ws/tools/pick/redrawComplex"), true},
        {QStringLiteral("ws/tools/pick/delay"), 0.5},
        {QStringLiteral("ws/tools/pick/modifiers"), 0},
        {QStringLiteral("ws/tools/ellipse/kind"), 0},
        {QStringLiteral("ws/tools/ellipse/start"), 0.0},
        {QStringLiteral("ws/tools/ellipse/end"), 270.0},
        {QStringLiteral("ws/tools/ellipse/clockwise"), false},
        {QStringLiteral("ws/tools/eraser/width"), 1.0},
        {QStringLiteral("ws/tools/eraser/reduceNodes"), true},
        {QStringLiteral("ws/tools/rect/cornerType"), 0},
        {QStringLiteral("ws/tools/rect/radius"), 0.0},
        {QStringLiteral("ws/tools/rect/scaleCorners"), true},
        {QStringLiteral("ws/tools/table/rows"), 3},
        {QStringLiteral("ws/tools/table/cols"), 4},
        {QStringLiteral("ws/tools/table/tabMode"), 1},
        {QStringLiteral("ws/tools/table/tabOrder"), 0},
        {QStringLiteral("ws/tools/connector/anchors"), true},
        {QStringLiteral("ws/tools/connector/distance"), 8.0},
        {QStringLiteral("ws/tools/mesh/cols"), 2},
        {QStringLiteral("ws/tools/mesh/rows"), 2},
        {QStringLiteral("ws/tools/attrPipette/properties"), true},
        {QStringLiteral("ws/tools/blend/steps"), 20},
        {QStringLiteral("ws/tools/contour/steps"), 4},
        {QStringLiteral("ws/tools/distort/mode"), 0},
        {QStringLiteral("ws/tools/envelope/mode"), 3},
        {QStringLiteral("ws/tools/shaping/radius"), 10.0},
        {QStringLiteral("ws/tools/smartFill/fill"), QStringLiteral("#FFD24D")},
        {QStringLiteral("ws/tools/smartFill/outline"), true},
        {QStringLiteral("ws/tools/shaping/strength"), 50.0},
        {QStringLiteral("ws/tools/freeTransform/mode"), 0},
        {QStringLiteral("ws/tools/distort/frequency"), 10},
        {QStringLiteral("ws/tools/attrPipette/transform"), false},
        {QStringLiteral("ws/tools/attrPipette/effects"), false},
        {QStringLiteral("ws/tools/knife/keepOne"), false},
        {QStringLiteral("ws/tools/knife/autoClose"), true},
        {QStringLiteral("ws/tools/graph/cols"), 4},
        {QStringLiteral("ws/tools/graph/rows"), 3},
        {QStringLiteral("ws/tools/spiral/type"), 0},
        {QStringLiteral("ws/tools/spiral/revolutions"), 4},
        {QStringLiteral("ws/tools/spiral/expansion"), 100},
        {QStringLiteral("ws/tools/angle/precision"), 2},
        {QStringLiteral("ws/tools/angle/units"), 0},
        {QStringLiteral("ws/tools/angle/prefix"), QString()},
        {QStringLiteral("ws/tools/angle/suffix"), QString()},
        {QStringLiteral("ws/tools/zoom/rightZoom"), 0},
        {QStringLiteral("ws/tools/zoom/rightPan"), 0},
        {QStringLiteral("ws/tools/zoom/relative"), true},
        {QStringLiteral("ws/tools/zoom/wheelCenter"), false},
        {QStringLiteral("ws/tools/zoom/calibration"), 1.0},
        // --- Anpassung
        {QStringLiteral("ws/custom/bar/menu"), true},
        {QStringLiteral("ws/custom/bar/status"), true},
        {QStringLiteral("ws/custom/bar/standard"), true},
        {QStringLiteral("ws/custom/bar/property"), true},
        {QStringLiteral("ws/custom/bar/toolbox"), true},
        {QStringLiteral("ws/custom/bar/rulers"), true},
        {QStringLiteral("ws/custom/buttonSize"), 0},
        {QStringLiteral("ws/custom/buttonLimit"), 2},
        {QStringLiteral("ws/custom/buttonFormat"), 0},
        {QStringLiteral("ws/custom/barTitles"), true},
        {QStringLiteral("ws/custom/lockBars"), true},
        {QStringLiteral("ws/custom/menuMode"), 0},
        {QStringLiteral("ws/custom/shortcuts"), QString()},
        {QStringLiteral("ws/custom/palette/autoUpdate"), true},
        {QStringLiteral("ws/custom/palette/rows"), 1},
        {QStringLiteral("ws/custom/palette/wideBorders"), false},
        {QStringLiteral("ws/custom/palette/largeSwatches"), false},
        {QStringLiteral("ws/custom/palette/noColor"), true},
        {QStringLiteral("ws/custom/palette/rightClick"), 1},
        {QStringLiteral("ws/custom/app/transparent"), false},
        {QStringLiteral("ws/custom/app/bars"), false},
        {QStringLiteral("ws/custom/app/dockers"), false},
        {QStringLiteral("ws/custom/app/colorUi"), false},
        {QStringLiteral("ws/custom/app/degree"), 0},

        // --- Dokument > Allgemein (Duplikatabstand und Verschiebungsabstand stehen schon auf der Masterseite)
        {QStringLiteral("doc/general/fillOpenCurves"), false},
        {QStringLiteral("doc/general/autoConvertBitmaps"), true},
        {QStringLiteral("doc/general/viewMode"), 1},
        {QStringLiteral("doc/general/colorMode"), 0},
        // --- Seitengrösse, Layout, Etikett
        {QStringLiteral("doc/page/showBorder"), true},
        {QStringLiteral("doc/page/resolution"), 300},
        {QStringLiteral("doc/page/bleed"), 0.0},
        {QStringLiteral("doc/page/showBleed"), false},
        {QStringLiteral("doc/layout/type"), 0},
        {QStringLiteral("doc/layout/facing"), false},
        {QStringLiteral("doc/layout/startOn"), 1},
        {QStringLiteral("doc/label/labels"), 0},
        {QStringLiteral("doc/label/type"), QString()},
        {QStringLiteral("doc/label/name"), QString()},
        {QStringLiteral("doc/label/w"), 0.0},
        {QStringLiteral("doc/label/h"), 0.0},
        {QStringLiteral("doc/label/cols"), 1},
        {QStringLiteral("doc/label/rows"), 1},
        {QStringLiteral("doc/label/left"), 0.0},
        {QStringLiteral("doc/label/top"), 0.0},
        {QStringLiteral("doc/label/hgap"), 0.0},
        {QStringLiteral("doc/label/vgap"), 0.0},
        // --- Hintergrund
        {QStringLiteral("doc/background/type"), 0},
        {QStringLiteral("doc/background/color"), QStringLiteral("#ffffff")},
        {QStringLiteral("doc/background/bitmap"), QString()},
        {QStringLiteral("doc/background/linked"), 1},
        {QStringLiteral("doc/background/customSize"), false},
        {QStringLiteral("doc/background/width"), 0.0},
        {QStringLiteral("doc/background/height"), 0.0},
        {QStringLiteral("doc/background/keepRatio"), true},
        {QStringLiteral("doc/background/printExport"), true},
        // --- Hilfslinien
        {QStringLiteral("doc/guides/show"), true},
        {QStringLiteral("doc/guides/snap"), false},
        {QStringLiteral("doc/guides/color"), QStringLiteral("#0000ff")},
        {QStringLiteral("doc/guides/presetColor"), QStringLiteral("#008000")},
        {QStringLiteral("doc/guides/presetMode"), 0},
        {QStringLiteral("doc/guides/presets"), QString()},
        // --- Gitter
        {QStringLiteral("doc/grid/mode"), 0}, // 0 = Linien pro mm, 1 = Abstand in mm
        {QStringLiteral("doc/grid/h"), 0.2},
        {QStringLiteral("doc/grid/v"), 0.2},
        {QStringLiteral("doc/grid/snap"), false},
        {QStringLiteral("doc/grid/show"), false},
        {QStringLiteral("doc/grid/dots"), 0},
        {QStringLiteral("doc/grid/baseline/spacing"), 14.0},
        {QStringLiteral("doc/grid/baseline/color"), QStringLiteral("#e6e6ff")},
        {QStringLiteral("doc/grid/baseline/top"), 15.0},
        {QStringLiteral("doc/grid/baseline/snap"), false},
        {QStringLiteral("doc/grid/baseline/show"), false},
        {QStringLiteral("doc/grid/pixel/opacity"), 60.0},
        {QStringLiteral("doc/grid/pixel/color"), QStringLiteral("#e6e6ff")},
        {QStringLiteral("doc/grid/pixel/show"), true},
        {QStringLiteral("doc/grid/pixel/snap"), true},
        // --- Lineale (Schrittweite = Verschiebungsabstand der Masterseite)
        {QStringLiteral("doc/rulers/super"), 2},
        {QStringLiteral("doc/rulers/micro"), 2},
        {QStringLiteral("doc/rulers/sameUnits"), true},
        {QStringLiteral("doc/rulers/units"), 0},
        {QStringLiteral("doc/rulers/unitsV"), 0},
        {QStringLiteral("doc/rulers/scale"), 1.0},
        {QStringLiteral("doc/guides/custom/margin"), 12.7},
        {QStringLiteral("doc/guides/custom/columns"), 3},
        {QStringLiteral("doc/guides/custom/gutter"), 5.0},
        {QStringLiteral("doc/guides/custom/gridH"), 25.4},
        {QStringLiteral("doc/guides/custom/gridV"), 25.4},
        {QStringLiteral("doc/rulers/sameHV"), true},
        {QStringLiteral("doc/rulers/originX"), 0.0},
        {QStringLiteral("doc/rulers/originY"), 0.0},
        {QStringLiteral("doc/rulers/ticks"), 10},
        {QStringLiteral("doc/rulers/moreTicks"), true},
        // --- Speichern, HTML
        {QStringLiteral("doc/save/cmx"), false},
        {QStringLiteral("doc/save/bitmapCompression"), true},
        {QStringLiteral("doc/save/graphicCompression"), true},
        {QStringLiteral("doc/save/patterns"), 0},
        {QStringLiteral("doc/save/blends"), 1},
        {QStringLiteral("doc/html/tolerance"), 0},
        {QStringLiteral("doc/html/whitespace"), 100},
        {QStringLiteral("doc/html/whitespacePos"), 4},
        {QStringLiteral("doc/html/client"), true},
        {QStringLiteral("doc/html/server"), false},
        {QStringLiteral("doc/html/serverType"), 0},

        // --- Global
        // Bildersuche: API-Schlüssel je Archiv (leer = Quelle nicht benutzt)
        {QStringLiteral("global/imageSearch/key/magnific"), QString()},
        {QStringLiteral("global/imageSearch/key/icons8"), QString()},
        {QStringLiteral("global/imageSearch/key/pixabay"), QString()},
        {QStringLiteral("global/imageSearch/key/unsplash"), QString()},
        {QStringLiteral("global/imageSearch/key/pexels"), QString()},
        {QStringLiteral("global/imageSearch/key/adobe"), QString()},
        {QStringLiteral("global/imageSearch/key/getty"), QString()},
        {QStringLiteral("global/imageSearch/key/istock"), QString()},
        {QStringLiteral("global/imageSearch/onlyFree"), true},
        {QStringLiteral("global/imageSearch/kind"), 0},
        // KI (Optionen > Global > KI): Zugänge als JSON, lokal je Benutzer
        {QStringLiteral("global/ai/accounts"), QStringLiteral("[]")},
        {QStringLiteral("global/ai/account"), QString()},
        {QStringLiteral("global/ai/mayWrite"), true},
        {QStringLiteral("global/ai/claudePath"), QString()},
        // Websuche der KI: 0 = SearXNG, 1 = Brave Search
        {QStringLiteral("global/web/provider"), 0},
        {QStringLiteral("global/web/searxng"), QStringLiteral("http://127.0.0.1:8888")},
        {QStringLiteral("global/web/searxngKey"), QString()},
        {QStringLiteral("global/web/braveKey"), QString()},
        {QStringLiteral("global/print/style"), 0},
        {QStringLiteral("global/print/currentOnly"), false},
        {QStringLiteral("global/print/driver"), QString()},
        {QStringLiteral("global/print/textAsGraphics"), false},
        {QStringLiteral("global/print/softwareClip"), false},
        {QStringLiteral("global/print/bitmapChunks"), false},
        {QStringLiteral("global/print/beziers"), false},
        {QStringLiteral("global/print/bands"), false},
        {QStringLiteral("global/print/pageSize"), false},
        {QStringLiteral("global/bitmapfx/preview"), 3},
        {QStringLiteral("global/bitmapfx/lastValues"), true},
        {QStringLiteral("global/filters/inactive"), QString()},
        {QStringLiteral("global/filters/associate"), QStringLiteral("vek")},
        {QStringLiteral("global/color/spotPalette"), 0},
        {QStringLiteral("global/color/pantone"), 0},
        {QStringLiteral("global/color/paletteDialog"), true},
    };
    return table;
}

QString settingsKey(const QString& key)
{
    return QStringLiteral("options/") + key;
}

} // namespace

Options& Options::instance()
{
    static Options options;
    return options;
}

Options::Options() = default;

void Options::attach(Editor* editor)
{
    if (m_editor)
        m_editor->disconnect(this);
    m_editor = editor;
    if (m_editor) {
        connect(m_editor, &Editor::documentChanged, this, &Options::changed);
        connect(m_editor, &Editor::documentReplaced, this, &Options::changed);
        connect(m_editor, &QObject::destroyed, this, [this] { m_editor = nullptr; });
    }
    emit changed();
}

bool Options::known(const QString& key) const
{
    return defaults().contains(key);
}

QVariant Options::defaultValue(const QString& key) const
{
    return defaults().value(key);
}

QVariant Options::value(const QString& key) const
{
    const QVariant fallback = defaultValue(key);
    if (isDocument(key)) {
        if (!m_editor)
            return fallback;
        const QJsonValue v = m_editor->document().masterPage().options().value(key);
        if (v.isUndefined())
            return fallback;
        QVariant result = v.toVariant();
        if (fallback.isValid())
            result.convert(fallback.metaType());
        return result;
    }
    QVariant result = QSettings().value(settingsKey(key), fallback);
    if (fallback.isValid() && result.metaType() != fallback.metaType())
        result.convert(fallback.metaType());
    return result;
}

void Options::apply(const QVariantMap& values)
{
    bool any = false;
    QJsonObject docOptions;
    bool docChanged = false;
    if (m_editor)
        docOptions = m_editor->document().masterPage().options();
    QSettings settings;
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        if (value(it.key()) == it.value())
            continue;
        if (isDocument(it.key())) {
            if (!m_editor)
                continue;
            // Nur Abweichungen von der Vorgabe speichern
            if (it.value() == defaultValue(it.key()))
                docOptions.remove(it.key());
            else
                docOptions.insert(it.key(), QJsonValue::fromVariant(it.value()));
            docChanged = true;
        } else {
            settings.setValue(settingsKey(it.key()), it.value());
            any = true;
        }
    }
    if (docChanged) {
        // Löst über documentChanged auch changed() aus
        Transaction tx(m_editor->document(), tr("Optionen"));
        tx.modify<Page>(m_editor->document().masterPage().id(), [&docOptions](Page& p) { p.setOptions(docOptions); });
        tx.commit();
    } else if (any) {
        emit changed();
    }
}

QStringList Options::unitNames()
{
    return {tr("Millimeter"), tr("Zentimeter"), tr("Meter"), tr("Zoll"), tr("Punkt"), tr("Pixel"), tr("Pica"),
            tr("Kilometer"), tr("Fuss")};
}

QString Options::unitSymbol(int unit)
{
    static const char* symbols[] = {"mm", "cm", "m", "in", "pt", "px", "pc", "km", "ft"};
    return QLatin1String(symbols[std::clamp(unit, 0, 8)]);
}

double Options::unitInMm(int unit)
{
    static const double mm[] = {1.0, 10.0, 1000.0, 25.4, 25.4 / 72.0, 25.4 / 96.0, 25.4 / 6.0, 1000000.0, 304.8};
    return mm[std::clamp(unit, 0, 8)];
}

int Options::rulerUnit(bool horizontal) const
{
    if (horizontal || flag(QStringLiteral("doc/rulers/sameHV")))
        return integer(QStringLiteral("doc/rulers/units"));
    return integer(QStringLiteral("doc/rulers/unitsV"));
}

double Options::rulerValue(double mm, bool horizontal) const
{
    const double origin = number(horizontal ? QStringLiteral("doc/rulers/originX") : QStringLiteral("doc/rulers/originY"));
    const double scale = std::max(1e-9, number(QStringLiteral("doc/rulers/scale")));
    return (mm - origin) * scale / unitInMm(rulerUnit(horizontal));
}

QString Options::positionText(const QPointF& mm) const
{
    const int decimals = std::clamp(integer(QStringLiteral("ws/edit/precision")), 0, 6);
    const QString x = QString::number(rulerValue(mm.x(), true), 'f', decimals);
    const QString y = QString::number(rulerValue(mm.y(), false), 'f', decimals);
    return QStringLiteral("( %1 ; %2 ) %3").arg(x, y, unitSymbol(rulerUnit(true)));
}

autocorrect::Settings Options::autocorrectSettings() const
{
    autocorrect::Settings s;
    s.capitalizeSentences = flag(QStringLiteral("ws/text/autocorrect/capSentence"));
    s.fixTwoCaps = flag(QStringLiteral("ws/text/autocorrect/twoCaps"));
    s.capitalizeDays = flag(QStringLiteral("ws/text/autocorrect/dayNames"));
    s.language = autocorrect::Settings::Language(std::clamp(integer(QStringLiteral("ws/text/autocorrect/language")), 0, 3));
    s.typographicQuotes = flag(QStringLiteral("ws/text/autocorrect/typoQuotes"));
    s.straightAfterNumbers = flag(QStringLiteral("ws/text/autocorrect/numbersStraight"));
    auto quote = [this](const QString& key, const QString& fallback) {
        const QString v = value(key).toString();
        return v.isEmpty() ? fallback : v;
    };
    s.singleOpen = quote(QStringLiteral("ws/text/autocorrect/singleOpen"), s.singleOpen);
    s.singleClose = quote(QStringLiteral("ws/text/autocorrect/singleClose"), s.singleClose);
    s.doubleOpen = quote(QStringLiteral("ws/text/autocorrect/doubleOpen"), s.doubleOpen);
    s.doubleClose = quote(QStringLiteral("ws/text/autocorrect/doubleClose"), s.doubleClose);
    s.replaceText = flag(QStringLiteral("ws/text/autocorrect/replace"));
    s.replacements = autocorrect::Settings::parseReplacements(value(QStringLiteral("ws/text/autocorrect/replacements")).toString());
    return s;
}

QList<Guide> Options::presetGuides(const QSizeF& page) const
{
    QList<Guide> g;
    auto box = [&](double inset) {
        g << Guide::vertical(inset, 0, true) << Guide::vertical(page.width() - inset, 0, true)
          << Guide::horizontal(inset, 0, true) << Guide::horizontal(page.height() - inset, 0, true);
    };
    if (integer(QStringLiteral("doc/guides/presetMode")) == 1) {
        // Benutzerdefiniert: Ränder, Spalten mit Abstand, Hilfslinienraster
        const double margin = number(QStringLiteral("doc/guides/custom/margin"));
        if (margin > 0)
            box(margin);
        const int columns = integer(QStringLiteral("doc/guides/custom/columns"));
        const double gutter = number(QStringLiteral("doc/guides/custom/gutter"));
        if (columns > 1) {
            const double inner = page.width() - 2 * margin;
            const double col = (inner - (columns - 1) * gutter) / columns;
            for (int i = 1; i < columns; ++i) {
                const double x = margin + i * col + (i - 1) * gutter;
                g << Guide::vertical(x, 0, true) << Guide::vertical(x + gutter, 0, true);
            }
        }
        const double gh = number(QStringLiteral("doc/guides/custom/gridH"));
        const double gv = number(QStringLiteral("doc/guides/custom/gridV"));
        if (gh > 0 && gv > 0 && text(QStringLiteral("doc/guides/presets")).contains(QLatin1String("grid"))) {
            for (double x = gh; x < page.width(); x += gh)
                g << Guide::vertical(x, 0, true);
            for (double y = gv; y < page.height(); y += gv)
                g << Guide::horizontal(y, 0, true);
        }
        return g;
    }
    const QStringList on = text(QStringLiteral("doc/guides/presets")).split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (on.contains(QLatin1String("cm")))
        box(10.0);
    if (on.contains(QLatin1String("bleed"))) {
        const double bleed = number(QStringLiteral("doc/page/bleed"));
        box(-(bleed > 0 ? bleed : 3.175));
    }
    if (on.contains(QLatin1String("page")))
        box(0.0);
    if (on.contains(QLatin1String("printable")))
        box(4.233); // übliche Druckerränder (1/6 Zoll)
    if (on.contains(QLatin1String("columns"))) {
        // Dreispaltiges Rundschreiben: halber Zoll Rand, 3 Spalten, 1/4 Zoll Abstand
        const double margin = 12.7, gutter = 6.35;
        box(margin);
        const double col = (page.width() - 2 * margin - 2 * gutter) / 3.0;
        for (int i = 1; i < 3; ++i) {
            const double x = margin + i * col + (i - 1) * gutter;
            g << Guide::vertical(x, 0, true) << Guide::vertical(x + gutter, 0, true);
        }
    }
    if (on.contains(QLatin1String("grid"))) {
        for (double x = 25.4; x < page.width(); x += 25.4)
            g << Guide::vertical(x, 0, true);
        for (double y = 25.4; y < page.height(); y += 25.4)
            g << Guide::horizontal(y, 0, true);
    }
    if (on.contains(QLatin1String("origin")))
        g << Guide::vertical(0, 0, true) << Guide::horizontal(0, 0, true);
    return g;
}

void Options::resetWorkspace()
{
    QSettings().remove(QStringLiteral("options"));
    emit changed();
}

} // namespace vek
