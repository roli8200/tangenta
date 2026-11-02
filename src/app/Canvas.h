#pragma once

#include "Editor.h"
#include "SnapEngine.h"
#include "Tool.h"

#include <QHash>
#include <QWidget>

#include <array>
#include <memory>
#include <optional>

namespace vek {

enum class ToolId {
    Pick, Shape, Crop, Knife, Eraser, Zoom, Freehand, Line, Bezier, ArtisticMedia, Rectangle, Ellipse, Polygon,
    Star, ComplexStar, InteractiveFill, Text, Transparency, Blur, Spiral, GraphPaper, Pan, DimensionParallel,
    DimensionHV, DimensionAngle, Connector, ConnectorAngle, ConnectorRound, SmartDrawing, Table, Mesh, ColorPipette, AttributePipette, Perspective, DropShadow, Blend, Contour, Distort, Envelope, Extrude, BSpline, VirtualDelete, Pen, Polyline, ThreePointCurve,
    ThreePointRectangle, ThreePointEllipse, Smear, Smudge, Roughen, Twirl, Attract, Repel, FreeTransform, SmartFill, PerfectBasic, PerfectArrow,
    PerfectFlowchart, PerfectBanner, PerfectCallout, Count
};

// Lage eines Mengentextrahmens (für Ketten): lokal 0..size, transform = lokal -> Seite
struct TextFrameGeometry {
    NodeId id = 0;
    QTransform transform;
    QSizeF size;
    bool visible = true; // liegt auf der angezeigten Seite oder der Masterseite
};

class ArtisticMediaTool;
class CropTool;
class EraserTool;
class PickTool;
class ShapeTool;
class MeshTool;
class TableTool;
class TextTool;

// Zeichenfläche: zeigt die aktuelle Seite samt Masterseite und Hilfslinien, verteilt Maus
// und Tastatur an das aktive Werkzeug. Ansicht = Zoom (Pixel je mm) + Lage des Seitenursprungs.
class Canvas : public QWidget {
    Q_OBJECT

public:
    explicit Canvas(Editor& editor, QWidget* parent = nullptr);
    ~Canvas() override;

    Editor& editor() { return m_editor; }
    const Editor& editor() const { return m_editor; }

    void setTool(ToolId id);
    ToolId toolId() const { return m_toolId; }
    ShapeTool& shapeTool() const;
    MeshTool& meshTool() const;
    TableTool& tableTool() const;
    Tool* currentTool() const { return tool(); }
    PickTool& pickTool() const;
    CropTool& cropTool() const;
    EraserTool& eraserTool() const;
    ArtisticMediaTool& mediaTool() const;
    TextTool& textTool() const;
    // Objekte werden gerade vom Werkzeug als Arbeitskopie gezeigt (Text bearbeiten)
    void setHiddenObjects(const QList<NodeId>& ids);
    void setHiddenObject(NodeId id) { setHiddenObjects(id != 0 ? QList<NodeId>{id} : QList<NodeId>()); }

    // Verkettete Mengentextrahmen: alle Rahmen der Kette von id in Flussreihenfolge
    QList<TextFrameGeometry> textChainGeometry(NodeId id) const;
    // Laschen oben und unten in der Mitte der Rahmen und blaue Pfeile von
    // Rahmen zu Rahmen. Die untere Lasche des letzten Rahmens zeigt Überlauf (▼); overflow
    // leer = Zustand aus dem Dokument.
    void paintTextChain(QPainter& p, const QList<TextFrameGeometry>& frames, std::optional<bool> overflow) const;
    // Untere Lasche des letzten Rahmens unter der Maus (Bildschirmkoordinaten): Index in frames,
    // sonst -1. Ein Rahmen hat höchstens einen Folgerahmen; verknüpfte Laschen bieten nichts an.
    int textLinkTabAt(const QPointF& screen, const QList<TextFrameGeometry>& frames) const;
    // Werkzeug meldet geänderten Zustand (z.B. gewählte Knoten) an die Eigenschaftsleiste
    void notifyToolState() { emit toolStateChanged(); }

    double zoom() const { return m_zoom; }
    QTransform viewTransform() const;
    QPointF toDoc(const QPointF& screen) const;
    QPointF toScreen(const QPointF& doc) const;
    double pixelsToDoc(double px) const { return px / m_zoom; }

    // Zoom in Prozent: 100 % = Originalgrösse am Bildschirm
    double zoomPercent() const;
    void setZoomPercent(double percent);
    void fitPage();
    void zoomToRect(const QRectF& docRect);
    void zoomBy(double factor, const QPointF& screenAnchor);
    void zoomIn() { zoomBy(1.25, rect().center()); }
    void zoomOut() { zoomBy(1.0 / 1.25, rect().center()); }

    bool guidesVisible() const { return m_guidesVisible; }
    void setGuidesVisible(bool visible);
    // Ausgewählte Hilfslinie (liegt im Editor), Vorschau beim Ziehen und Drehen
    int selectedGuide() const { return m_editor.selectedGuide(); }
    void setSelectedGuide(int index) { m_editor.setSelectedGuide(index); }
    void setGuidePreview(std::optional<Guide> guide);
    int guideAt(const QPointF& screen) const;
    // Sichtbares Stück einer Hilfslinie in Bildschirmkoordinaten, inset px vom Rand entfernt
    bool guideSegment(const Guide& guide, double inset, QPointF* a, QPointF* b) const;

    // Farbe aus der Palette zuerst dem Werkzeug anbieten (z.B. Farbstopp der interaktiven Füllung)
    bool offerPaletteColor(const std::optional<Color>& color, bool fill);

    // Vorschau eines Effekts (Transparenz, Unschärfe) beim Ziehen, bevor er ins Dokument kommt
    void setEffectPreview(NodeId id, Effect effect, const Ramp& ramp);
    void clearEffectPreview();

    // Seite ohne Werkzeuge, Hilfslinien und Gitter in einen beliebigen Maler zeichnen (Drucken,
    // Export, Vollbildvorschau): pxPerMm Massstab, offset = Lage des Seitenursprungs im Gerät.
    // background: Seitenhintergrund mitzeichnen (Optionen > Hintergrund drucken und exportieren)
    void renderPage(QPainter& p, int pageIndex, double pxPerMm, const QPointF& offset, bool background) const;
    // Seitenhintergrund (Farbe oder Bitmap) in Bildschirmkoordinaten des aktuellen Massstabs
    void paintBackground(QPainter& p, const QRectF& pageRect) const;
    // Objekt mit zusätzlicher Transformation (Seitenkoordinaten) zeichnen, z.B. beim Ziehen
    void paintObjectPreview(QPainter& p, const DrawObject& o, const QTransform& extra) const;
    // Objekt, das (noch) nicht im Dokument steht, wie im Dokument zeichnen (Arbeitskopien der Werkzeuge)
    void paintObjectDirect(QPainter& p, const DrawObject& o) const;
    // Doppelseiten: zweite Seite neben der aktuellen (Index, Versatz in mm), -1 = keine
    std::pair<int, double> facingPartner() const;

    // Blauer Hinweis wie "Knoten" oder "Hilfslinie" an der Einrast-Stelle
    void paintSnapHint(QPainter& p, const SnapResult& snap) const;

    static QColor previewColor() { return QColor(0x1e, 0x88, 0xe5); }

signals:
    void cursorMoved(const QPointF& doc);
    void toolChanged(ToolId id);
    void toolStateChanged();
    // Rechtsklick auf ausgewählte Objekte: das Hauptfenster zeigt das Kontextmenü
    void contextMenuRequested(const QPoint& globalPos);
    // Rechtsklick auf eine Hilfslinie (Index in Editor::guides(), schon ausgewählt)
    void guideContextMenuRequested(int index, const QPoint& globalPos);
    void viewChanged();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    bool event(QEvent* e) override;
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dragMoveEvent(QDragMoveEvent* e) override;
    void dropEvent(QDropEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void leaveEvent(QEvent* e) override;

private:
    void paintPage(QPainter& p, const Page& page) const;
    void paintObject(QPainter& p, const DrawObject& o) const;
    void paintObjectSharp(QPainter& p, const DrawObject& o) const;
    // Linse: was unter o liegt (Seite, Masterseite, Objekte davor) in ein Bild, verändert, auf o beschnitten
    void paintLens(QPainter& p, const DrawObject& o) const;
    void paintBelow(QPainter& p, const DrawObject& stop) const;
    void paintBevel(QPainter& p, const DrawObject& o) const;
    void paintPowerClipEdit(QPainter& p) const;
    // Kleine Leiste unter einem ausgewählten PowerClip (Bearbeiten, Auswählen, Extrahieren,
    // Sperren, Einpassen) bzw. "Fertig" während der Bearbeitung
    void updateClipBar();
    class PowerClipBar* m_clipBar = nullptr;
    struct ClipBarState {
        bool show = false;
        QPoint pos;
        bool editing = false;
        bool locked = false;
        friend bool operator==(const ClipBarState&, const ClipBarState&) = default;
    };
    ClipBarState m_clipBarState;
public:
    PowerClipBar* powerClipBar() const { return m_clipBar; }
private:
    // Drahtmodell-Linse: Objekte darunter in Kontur- und Füllfarbe der Linse
    mutable std::optional<std::pair<QColor, QColor>> m_wireLens;
public:
    // Hinterlegter Schatten unter o (auch für die Vorschau des Schattenwerkzeugs)
    void paintShadow(QPainter& p, const DrawObject& o, const DropShadow& shadow) const;
    // Abbildung Objekt -> Schatten (Seitenkoordinaten), perspektivisch an Unter-/Oberkante
    static QTransform shadowTransform(const QRectF& bounds, const DropShadow& shadow);
    // Kontur-Stufen eines Objekts (Seitenkoordinaten, von hinten nach vorn) mit Farben
    struct ContourStep {
        QPainterPath path;
        Color fill;
        std::optional<Color> stroke;
        bool front; // über dem Objekt (Stufen nach innen)
    };
    static QList<ContourStep> contourSteps(const DrawObject& o, const ContourEffect& c);
    // Seitenflächen der Extrusion (Seitenkoordinaten, von hinten nach vorn) mit Farben
    static QList<std::pair<QPolygonF, QColor>> extrusionFaces(const DrawObject& o, const Extrusion& e);
private:
    void strokeOnly(QPainter& p, const DrawObject& o) const;
public:
    // Kontur nach dem Konturstift (Ecken, Enden, Lage, Kalligrafie, Pfeile) in Bildschirmkoordinaten;
    // closed: die Form ist eine Fläche (innen/aussen gelten nur dann)
    static void strokeStyled(QPainter& p, const QPainterPath& screenPath, const Style& s, double zoom, bool closed,
                             const QPainterPath& arrowsPage = {}, const QTransform& view = {});
private:
    void paintWireframe(QPainter& p, const DrawObject& o, bool simple) const;
public:
    // Ansicht > Einfacher Umriss … Pixel (Dokumentoption doc/general/viewMode)
    enum class ViewMode { Draft = 0, Enhanced = 1, SimpleWireframe = 2, Wireframe = 3, Normal = 4, Pixel = 5 };
    ViewMode viewMode() const;
    // Ganzseitenvorschau/Vorschau markierter Objekte: nur diese Objekte zeichnen (leer = alle)
    void setRenderFilter(const QList<NodeId>& ids) { m_renderFilter = ids; }
    // PDF-Ausgabe: Text als echte Schrift (Glyphenläufe, Schrift eingebettet) statt als Kurven
    void setTextAsGlyphs(bool on) { m_textAsGlyphs = on; }
    // PDF-Ausgabe: Verläufe, Muster und Maschen als Bitmap (für ältere RIPs)
    void setComplexFillsAsBitmaps(bool on) { m_complexFillsAsBitmaps = on; }
private:
    bool m_textAsGlyphs = false;
    bool m_complexFillsAsBitmaps = false;
    QList<NodeId> m_renderFilter;
    void paintGuides(QPainter& p) const;
    void paintGrid(QPainter& p, const QRectF& pageRect) const;
    Tool* tool() const { return m_tools[static_cast<std::size_t>(m_toolId)].get(); }
    double pixelsPerMm100() const;
    double physicalPixelsPerMm() const;
public:
    // Schwenken (Hilfsmittel 'Schwenken'): Ansicht um Bildschirmpixel verschieben
    void panBy(const QPointF& delta);
private:

    Editor& m_editor;
    std::array<std::unique_ptr<Tool>, static_cast<std::size_t>(ToolId::Count)> m_tools;
    ToolId m_toolId = ToolId::Pick;
    ToolId m_lastDrawingTool = ToolId::Rectangle;
    double m_zoom = 2.0;
    QPointF m_origin; // Bildschirmlage von Seitenpunkt (0,0)
    bool m_panning = false;
    QPointF m_panStart;
    bool m_fitPending = true;
    bool m_guidesVisible = true;
    std::optional<Guide> m_guidePreview;
    QList<NodeId> m_hidden;
    // Beim Rendern ausserhalb der Zeichenfläche: Bereich des Geräts statt rect()
    mutable std::optional<QRectF> m_renderArea;
    mutable bool m_rendering = false;
    mutable bool m_output = false; // Druck/Export: nicht druckbare Ebenen auslassen
    QRectF paintArea() const { return m_renderArea.value_or(QRectF(rect())); }
    // Hintergrund-Bitmap (eingebettet im Bildpool oder verknüpfte Datei), zwischengespeichert
    mutable QString m_backgroundSource;
    mutable QImage m_backgroundImage;
    QImage backgroundImage() const;
    struct EffectPreview {
        std::optional<Ramp> transparency;
        std::optional<Ramp> blur;
    };
    QHash<NodeId, EffectPreview> m_effectPreview;
    // Fertig berechnete Unschärfe/Transparenz je Objekt, gültig bis zur nächsten Änderung am
    // Dokument oder an der Ansicht (Weichzeichnen grosser Fotos dauert sonst bei jedem Neuzeichnen)
    struct EffectCache {
        QImage image;
        QRect target;
        QTransform view;
        qreal dpr = 1.0;
    };
    mutable QHash<NodeId, EffectCache> m_effectCache;
};

} // namespace vek
