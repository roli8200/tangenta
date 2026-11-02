#pragma once

#include "Align.h"

#include "CurveObject.h"
#include "Document.h"
#include "DrawObject.h"
#include "EllipseObject.h"
#include "PolygonObject.h"
#include "TextObject.h"
#include "Vectorizer.h"

#include <QList>
#include <QObject>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace vek {

class ImageObject;
class GroupObject;
class Transaction;
class TableObject;

// Vorgaben für neue Ellipsen und Polygone (Eigenschaftsleiste ohne Auswahl)
struct ShapeDefaults {
    EllipseObject::Kind ellipseKind = EllipseObject::Kind::Ellipse;
    double startAngle = 0.0;
    double endAngle = 270.0;
    bool clockwise = false;
    int polygonPoints = 5;
    int starPoints = 5;
    int starSharpness = 53;
    int complexStarPoints = 9;
    int complexStarSharpness = 2;
    // Textwerkzeug ohne Auswahl: Format für neuen Grafik- bzw. Mengentext (wie)
    CharFormat artisticText{QStringLiteral("Arial"), 24.0};
    CharFormat paragraphText{QStringLiteral("Arial"), 12.0};
    // "In Kurven" eines unscharfen Objekts: Stufenkonturen je Unschärfe, Bänder je Verlauf
    int blurSteps = 48;
    int blurBands = 32;
};

// "Ausrichten an"
struct SnapSettings {
    bool guides = true;
    bool objects = true;
    bool page = true;
};

// Bearbeitungszustand rund um ein Dokument: aktuelle Seite, Auswahl, Befehle.
// Werkzeuge und Fenster sprechen nur mit dem Editor, nie direkt mit der Transaction.
// Die Auswahl hält Ids, keine Zeiger: Undo baut Knoten neu auf.
class Editor : public QObject {
    Q_OBJECT

public:
    explicit Editor(QObject* parent = nullptr);
    ~Editor() override;

    Document& document() { return *m_doc; }
    const Document& document() const { return *m_doc; }
    void setDocument(std::unique_ptr<Document> doc, const QString& filePath);
    QString filePath() const { return m_filePath; }
    void setFilePath(const QString& path) { m_filePath = path; }

    int currentPageIndex() const { return m_currentPage; }
    const Page& currentPage() const { return m_doc->page(m_currentPage); }
    void setCurrentPageIndex(int index);
    // Ebene, in die neue Objekte kommen: oberste sichtbare, nicht gesperrte Ebene der Seite
    NodeId activeLayer() const;

    // --- Auswahl ------------------------------------------------------------------------
    const QList<NodeId>& selection() const { return m_selection; }
    QList<const DrawObject*> selectedObjects() const;
    QRectF selectionBounds() const;
    bool isSelected(NodeId id) const { return m_selection.contains(id); }
    // marquee: mit dem Auswahlrahmen gewählt (Bezugsobjekt beim Ausrichten = oben links statt
    // zuletzt gewählt)
    void setSelection(const QList<NodeId>& ids, bool marquee = false);
    bool selectedByMarquee() const { return m_marquee; }

    // Anordnen > Ausrichten und verteilen
    enum class AlignTo { ActiveObjects, PageEdge, PageCenter, Grid, Point };
    // Bezugsobjekt für "Aktive Objekte": zuletzt angeklickt bzw. beim Rahmen oben links
    NodeId alignReference() const;
    void alignSelection(align::H h, align::V v, AlignTo to = AlignTo::ActiveObjects, const QPointF& point = {});
    void distributeSelection(align::Spread h, align::Spread v, bool toPage);
    void clearSelection() { setSelection({}); }
    void selectAll();

    // Oberste Objekte der sichtbaren, nicht gesperrten Ebenen: erst Masterseite, dann Seite
    // (Zeichenreihenfolge, hinten nach vorn)
    QList<const DrawObject*> pickableObjects() const;
    const DrawObject* objectAt(const QPointF& point, double tolerance) const;
    bool treatAllObjectsAsFilled() const { return m_treatAllAsFilled; }
    void setTreatAllObjectsAsFilled(bool on) { m_treatAllAsFilled = on; }

    Style defaultStyle() const { return m_defaultStyle; }
    ShapeDefaults& shapeDefaults() { return m_shapeDefaults; }
    const ShapeDefaults& shapeDefaults() const { return m_shapeDefaults; }
    SnapSettings& snapSettings() { return m_snap; }
    const SnapSettings& snapSettings() const { return m_snap; }

    // Hilfslinien liegen auf der Masterseite und gelten für alle Seiten
    // Sichtbare Hilfslinien: erst die globalen (Masterseite), dann die der aktuellen Seite.
    // Indizes beziehen sich auf diese Liste.
    QList<Guide> guides() const;
    // Neue Hilfslinien sind global
    void addGuide(const Guide& guide);
    bool isGuidePageSpecific(int index) const;
    // Alle Hilfslinien auf einmal (Optionen > Hilfslinien): globale und die der aktuellen Seite
    void replaceGuides(const QList<Guide>& global, const QList<Guide>& currentPageOnly);
    // Hilfslinie zwischen global und "nur diese Seite" verschieben; bleibt ausgewählt
    void setGuidePageSpecific(int index, bool pageSpecific);
    void setGuide(int index, const Guide& guide, const QString& label);
    void removeGuide(int index);
    // Ausgewählte Hilfslinie (-1 = keine); schliesst eine Objektauswahl aus
    int selectedGuide() const { return m_selectedGuide; }
    void setSelectedGuide(int index);

    // --- Befehle (je ein Undo-Schritt) ------------------------------------------------------
    NodeId addObject(std::unique_ptr<DrawObject> object, const QString& label);
    // Bild einfügen (Datei unverändert bzw. Bild aus der Zwischenablage), mittig um center oder
    // auf der Seitenmitte. Grösse aus der Auflösung der Datei (sonst 96 dpi).
    // Liefert InvalidNodeId, wenn die Daten kein lesbares Bild sind.
    // name: Objektname (z.B. Bildnachweis aus der Bildersuche)
    NodeId importImage(const QByteArray& encoded, std::optional<QPointF> center = std::nullopt, const QString& name = QString());
    NodeId importImage(const QImage& image, std::optional<QPointF> center = std::nullopt);
    // Vektorimport (SVG, DXF, PDF …): die Objekte kommen als Gruppe auf die aktuelle Ebene,
    // mit centerOnPage mittig auf die Seite (sonst an ihre Koordinaten); wird ausgewählt
    NodeId importGroup(std::unique_ptr<GroupObject> group, bool centerOnPage, const QString& label);
    void transformSelection(const QTransform& m, const QString& label);
    // Eigenschaftsleiste
    void setSelectionBounds(const QRectF& target);
    void scaleSelection(double sx, double sy);
    // Winkel gegen den Uhrzeigersinn; nur bei genau einem Objekt sinnvoll
    void rotateSelectionTo(double degreesCcw);
    void mirrorSelection(Qt::Orientation orientation);
    void setSelectionStrokeWidth(double width);
    // Linienstil und Pfeilspitzen; nicht angegebene Teile bleiben, wie sie sind
    void setSelectionOutlineShape(std::optional<int> dash, std::optional<int> startArrow, std::optional<int> endArrow);
    // Konturstift (F12): alle Konturfelder von pen übernehmen (Farbe, Breite, Stil, Ecken, Enden,
    // Lage, Pfeile, Kalligrafie, hinter Füllung, mit Objekt skalieren); ohne Auswahl die Vorgabe
    void setSelectionOutlinePen(const Style& pen);
    void setObjectProperty(const QString& id, const QVariant& value, const QString& label);
    // Zwischenablage
    void copySelection();
    void cutSelection();
    void paste();
    static bool canPaste();
    void deleteSelection();
    void duplicateSelection();
    // "Verbindung trennen": ausgewählte Verbindungslinien von ihren Objekten lösen
    bool canBreakConnectors() const;
    void breakConnectors();
    // Kopie der Auswahl mit Transformation (Ziehen mit Strg im Windows-Modus)
    void duplicateSelectionWith(const QTransform& m, const QString& label);
    void groupSelection();
    void ungroupSelection();
    // Anordnen-Menü
    void ungroupAll();                 // Gruppierung aller Objekte aufheben (auch verschachtelt)
    void unlockAll();                  // Alle Objekte freigeben
    void clearTransformations();       // Änderungen zurücknehmen: Drehung/Neigung/Spiegelung weg
    bool outlineToObject();            // Umriss in Objekt umwandeln (Strg+Umschalt+Q)
    bool joinCurves(double tolerance); // Kurven verbinden: nahe Endpunkte zusammenführen
    // Änderungen-Andockfenster: m auf die Auswahl, mit copies > 0 auf so viele Kopien (jede
    // relativ zur vorigen); ein Rückgängig-Schritt
    void transformSelectionCopies(const QTransform& m, int copies, const QString& label);

    // Bearbeiten-Menü
    bool canRepeat() const { return bool(m_repeat); }
    QString repeatLabel() const { return m_repeatLabel; }
    void repeatLast(); // Wiederholen (Strg+R): letzten Befehl auf die aktuelle Auswahl
    void cloneSelection();
    void convertClonesToObjects(); // Klone/Symbolinstanzen in eigenständige Objekte
    NodeId createSymbol(const QString& name);
    QList<NodeId> symbols() const; // Symboldefinitionen (verborgene Ebene "Symbole" der Masterseite)
    NodeId placeSymbol(NodeId symbol, const QPointF& center);
    bool deleteSymbol(NodeId symbol); // nur unbenutzte
    void renameSymbol(NodeId symbol, const QString& name);
    void renameObject(NodeId id, const QString& name);
    int symbolUses(NodeId symbol) const;
    enum CopyWhat { CopyOutlinePen = 1, CopyOutlineColor = 2, CopyFill = 4, CopyText = 8 };
    void copyPropertiesFrom(NodeId source, int what);
    void selectAllText();
    // Objekte der aktuellen Seite (oberste Ebene), die pred erfüllen, werden ausgewählt
    int selectWhere(const std::function<bool(const DrawObject&)>& pred);
    int replaceText(const QString& find, const QString& with, bool caseSensitive, bool wholeWords);
    void setSelectionFill(const Fill& fill);
    void setObjectFill(NodeId id, const Fill& fill, const QString& label);
    void setObjectStroke(NodeId id, const std::optional<Color>& color, const QString& label);
    // Attributpipette: Eigenschaften (1), Transformationen (2), Effekte (4) von source auf target
    void applyAttributes(const DrawObject& source, NodeId target, int what);

    // Effekte-Menü
    // SuperClip: Auswahl in den Rahmen frame setzen (frame darf schon ein SuperClip sein)
    NodeId placeInsideFrame(NodeId frame);
    void extractContents();          // Inhalt extrahieren
    void createEmptySuperClipFrame();
    enum class ClipFit { Center, FitProportional, FillProportional, Stretch };
    void fitSuperClipContents(ClipFit fit);
    void setSuperClipContentsLocked(bool locked);
    void setSelectionLens(const Lens& lens);
    void setSelectionBevel(const Bevel& bevel);
    void setSelectionShadow(const DropShadow& shadow, const QString& label);
    // Interaktive Effekte
    NodeId blendObjects(NodeId from, NodeId to, int steps);
    void setObjectContour(NodeId id, const ContourEffect& c, const QString& label);
    void setSelectionContour(const ContourEffect& c, const QString& label);
    void setObjectExtrusion(NodeId id, const Extrusion& e, const QString& label);
    void setSelectionExtrusion(const Extrusion& e, const QString& label);
    // Objekt durch eine Kurve mit gleichem Stil und gleichen Effekten ersetzen (Verzerren, Hülle)
    NodeId replaceObjectWithCurve(NodeId id, const QList<SubPath>& paths, const QString& label);
    // Überblendung, Kontur, Extrusion in Einzelobjekte trennen (Strg+K); false = nichts zu trennen
    bool separateEffects();
    void setObjectShadow(NodeId id, const DropShadow& shadow, const QString& label);
    void clearSelectionEffects();    // Effekt löschen
    void copyEffectsFrom(NodeId source, const QString& label);

    // Hülle (Effekte > Hülle): Grundformen, Kurven und Text; das Objekt bleibt, was es ist
    static bool canEnvelope(const DrawObject& o);
    void setObjectEnvelopes(NodeId id, const QList<Envelope>& envelopes, const QString& label);
    // Form als Hülle auf die Auswahl (Vorlage oder "Form aus Objekt"): shape wird in die Form
    // des Objekts eingepasst; pageCoordinates = shape liegt auf der Seite (ein anderes Objekt) und
    // wird erst in die lokalen Koordinaten des Ziels gebracht. addNew: weitere Hülle über die
    // bestehende, sonst ersetzt sie die oberste. Leere Form = Rechteck (Neue Hülle).
    bool applyEnvelopeShape(const QPainterPath& shape, bool pageCoordinates, Envelope::Mode mode, bool addNew);
    void setSelectionEnvelopeMode(Envelope::Mode mode);
    void clearSelectionEnvelopes();
    // Linse "Fixiert": was die Linse gerade zeigt, wird zu eigenen Objekten (SuperClip in der
    // Form der Linse, Inhalt mit der Linse verändert); ersetzt die Linse, liefert den SuperClip
    NodeId freezeLens(NodeId lens);
    // Objekte unter stop in Zeichenreihenfolge (Masterseite, dann Seite), oberste Ebene
    QList<const DrawObject*> objectsBelow(const DrawObject& stop) const;

    // SuperClip-Inhalt bearbeiten (wie "SuperClip bearbeiten"): solange er läuft,
    // wählt und ändert man nur die Inhalte dieses SuperClips
    NodeId editedSuperClip() const;
    bool beginSuperClipEdit(NodeId clip);
    void endSuperClipEdit();
    // Inhalte des ausgewählten SuperClips auswählen (beginnt die Bearbeitung)
    bool selectSuperClipContents();
    // Eigenschaften von source auf target (Rechts ziehen: Füllung/Kontur hierher kopieren)
    void copyPropertiesTo(NodeId source, NodeId target, int what);

    // Tabelle-Menü: Text der Auswahl in eine Tabelle (Zeilen = Absätze, Spalten = Trennzeichen;
    // leeres Trennzeichen = eine Spalte) und zurück
    NodeId textToTable(const QString& separator);
    // Bitmaps-Menü: Auswahl durch ein Bild ersetzen (In Bitmap konvertieren), Bild auf seinen
    // Rahmen zuschneiden
    NodeId replaceSelectionWithImage(const QImage& pixels, const QRectF& bounds, const QString& label);
    bool cropBitmapToFrame(NodeId id);
    NodeId tableToText(const QString& separator);
    // Grösse der Grundform des ersten ausgewählten Objekts (für den Verlaufsdialog)
    QSizeF selectionLocalSize() const;
    void setSelectionStroke(std::optional<Color> color);
    void setSelectionBlur(double mm);
    // Transparenz/Unschärfe: ein Objekt (interaktives Werkzeug) bzw. alle ausgewählten
    void setObjectEffect(NodeId id, Effect effect, const Ramp& ramp, const QString& label);
    void setSelectionEffect(Effect effect, const std::function<void(Ramp&)>& fn, const QString& label);
    // Formwerkzeug: Knotenbearbeitung an einer Kurve als ein Undo-Schritt
    void modifyCurve(NodeId id, const std::function<void(CurveObject&)>& fn, const QString& label);
    void modifyText(NodeId id, const std::function<void(TextObject&)>& fn, const QString& label);
    // Beliebiges Objekt als ein Undo-Schritt ändern (z.B. Glyphengriff einer perfekten Form)
    void modifyObject(NodeId id, const std::function<void(DrawObject&)>& fn, const QString& label);
    void modifyTable(NodeId id, const std::function<void(TableObject&)>& fn, const QString& label);
    // Rahmen eines Bildes (Beschneiden mit dem Formwerkzeug)
    void setImageFrame(NodeId id, const QList<SubPath>& frame, const QString& label);
    // Ausgewähltes Bild (genau eines), sonst nullptr
    const ImageObject* selectedImage() const;
    // Vektorisieren wie SuperTrace: Gruppe aus Kurven je Farbe deckungsgleich über dem Bild;
    // liefert die Gruppe (ausgewählt) oder InvalidNodeId
    NodeId vectorizeImage(NodeId id, const trace::Result& result, bool deleteOriginal);
    // Bearbeitete Pixel übernehmen (Bildanpassung, Effekte); Grösse und Rahmen bleiben
    void setImagePixels(NodeId id, const QImage& pixels, const QString& label);
    // Auf allen ausgewählten Text als Ganzes (Eigenschaftsleiste ohne Textbearbeitung)
    void applyTextFormat(const std::function<void(CharFormat&)>& fn, const QString& label);
    void setTextAlignment(TextParagraph::Align align);
    // Text > Text an Pfad anpassen / Text begradigen (Grafiktext)
    bool fitTextToPath(NodeId text, NodeId pathObject);
    bool canFitSelectedTextToPath() const;
    bool fitSelectedTextToPath(); // Auswahl aus einem Grafiktext und einem anderen Objekt
    void straightenSelectedText();
    // Text an Pfad: nicht angegebene Werte bleiben
    void setTextPathParams(std::optional<double> offset, std::optional<double> distance, std::optional<bool> flip);
    // Aufzählungszeichen und Initiale für alle Absätze der ausgewählten Texte
    void setTextBullets(bool on);
    void setTextDropCap(int lines);
    void setTextHyphenation(bool on);
    // Ausgewählter Text; Folgerahmen einer Kette stehen für deren ersten Rahmen (der den Text hält)
    QList<const TextObject*> selectedTexts() const;
    // Mengentextrahmen verketten: Text, der in from keinen Platz hat, fliesst
    // in to weiter (auch auf einer anderen Seite). Nur 1:1: from darf noch keinen Folgerahmen
    // haben, to keinen Vorgänger und nicht schon in der Kette liegen; eigener Text von to kommt hinten an den
    // Kettentext.
    bool canLinkTextFrame(NodeId from, NodeId to) const;
    bool linkTextFrame(NodeId from, NodeId to);
    // Neuer, leerer Folgerahmen für from (Seitenkoordinaten, aktuelle Ebene); wird ausgewählt
    NodeId linkNewTextFrame(NodeId from, const QRectF& rect);
    // Text > Verknüpfen: ausgewählte Mengentextrahmen in Auswahlreihenfolge verketten
    bool canLinkSelectedTextFrames() const;
    void linkSelectedTextFrames();
    // Verknüpfung aufheben: jeder Rahmen der betroffenen Ketten behält den Text, der darin steht
    bool canUnlinkSelectedTextFrames() const;
    void unlinkSelectedTextFrames();
    // "In Kurven umwandeln" (Strg+Q): Grundformen werden zu Kurven gleicher Form und Lage
    void convertSelectionToCurves();
    bool canConvertSelectionToCurves() const;
    // Kombinieren (Strg+L) und Kombination aufheben (Strg+K)
    void combineSelection();
    bool canCombineSelection() const;
    void breakApartSelection();
    bool canBreakApartSelection() const;

    // Sperren: gesperrte Objekte bleiben wählbar, Befehle lassen sie aber unverändert
    void setSelectionLocked(bool locked);
    bool selectionHasLocked() const;
    bool selectionHasUnlocked() const;
    // true, wenn das Objekt oder eine Gruppe darüber gesperrt ist
    bool isLocked(NodeId id) const;

    // Anordnung (Stapelreihenfolge)
    enum class Arrange { ToPageFront, ToPageBack, ToLayerFront, ToLayerBack, ForwardOne, BackOne, Reverse };
    void arrangeSelection(Arrange how);
    bool canArrange(Arrange how) const;
    // "Vor..." / "Hinter...": Auswahl direkt vor bzw. hinter target legen
    void placeSelectionRelativeTo(NodeId target, bool inFront);

    // Formen (Objekt > Formen, Andockfenster "Formen"). Ergebnisse sind
    // Kurven mit dem Stil des Ziels.
    enum class ShapeOp { Weld, Trim, Intersect, Simplify, FrontMinusBack, BackMinusFront, Boundary };
    static QString shapeOpName(ShapeOp op);
    // Verschmelzen, Zuschneiden, Schnittmenge: Quellen auf ein Ziel anwenden
    bool shapeWithTarget(ShapeOp op, const QList<NodeId>& sources, NodeId target, bool keepSources, bool keepTarget);
    // Auf die Auswahl (Menü, Leiste); Ziel ist das zuletzt ausgewählte Objekt
    bool shapeSelection(ShapeOp op);
    bool canShapeSelection(ShapeOp op) const;

    void moveSelectionToMaster();
    void moveSelectionToCurrentPage();
    void insertPage(int index);
    void deletePage(int index);
    void setPageSize(const QSizeF& size, bool allPages);
    // Dokumenteinstellungen der Seiten-Eigenschaftsleiste (liegen auf der Masterseite)
    double nudge() const { return m_doc->masterPage().nudge(); }
    void setNudge(double mm);
    QPointF duplicateOffset() const { return m_doc->masterPage().duplicateOffset(); }
    void setDuplicateOffset(const QPointF& offset);

    // --- Beschneiden, Messer, Radierer ---------------------------------------------------
    // Objekte, auf die sich ein Schnitt bezieht: die Auswahl (Gruppen aufgelöst) oder, ohne
    // Auswahl, alle bearbeitbaren Objekte der aktuellen Seite
    QList<NodeId> cutTargets(bool wholePageIfNoSelection) const;
    // Jedes Ziel durch das Ergebnis von fn ersetzen; nullopt = unverändert, leere Liste = löschen.
    // Ein Undo-Schritt; die neuen Objekte werden ausgewählt.
    using Replacement = std::optional<std::vector<std::unique_ptr<DrawObject>>>;
    bool replaceObjects(const QList<NodeId>& targets, const std::function<Replacement(const DrawObject&)>& fn,
                        const QString& label);

signals:
    void superClipEditChanged(NodeId clip);
    void documentReplaced();
    void documentChanged();
    void selectionChanged();
    void guideSelectionChanged(int index);
    void currentPageChanged(int index);
    void modifiedChanged(bool modified);

private:
    void onDocumentChanged();
    // Oberste ausgewählte Objekte in Baumreihenfolge; gesperrte nur auf Wunsch
    QList<NodeId> topLevelSelection(bool includeLocked = false) const;
    // Vor dem Löschen: Ketten um die gelöschten Rahmen herum neu schliessen; fällt der erste
    // Rahmen weg, übernimmt der nächste verbleibende den Text
    void closeTextChains(Transaction& tx, const QList<NodeId>& removed) const;
    // Vor dem Löschen: Verbindungslinien zu gelöschten Objekten an Ort und Stelle lösen
    void detachConnectors(Transaction& tx, const QList<NodeId>& removed) const;
    // Seite und Index in deren Liste für einen Index in guides()
    std::pair<NodeId, int> guideSlot(int index) const;
    // Kinder von parent in die Reihenfolge order bringen (nur nötige Verschiebungen)
    void reorder(class Transaction& tx, NodeId parent, const QList<NodeId>& order);

    // Ausgewählte Objekte auf target(s) übertragen (Eigenschaften kopieren, Rechts ziehen)
    void copyProperties(const DrawObject& source, const QList<NodeId>& targets, int what);

    std::unique_ptr<Document> m_doc;
    NodeId m_clipEdit = InvalidNodeId;
    QString m_filePath;
    int m_currentPage = 0;
    QList<NodeId> m_selection;
    bool m_marquee = false;
    std::function<void()> m_repeat;
    QString m_repeatLabel;
    void remember(const QString& label, std::function<void()> fn)
    {
        m_repeatLabel = label;
        m_repeat = std::move(fn);
    }
    NodeId symbolLayer(Transaction* tx);
    int m_selectedGuide = -1;
    bool m_treatAllAsFilled = true;
    Style m_defaultStyle;
    ShapeDefaults m_shapeDefaults;
    SnapSettings m_snap;
};

} // namespace vek
