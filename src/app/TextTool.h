#pragma once

#include "Canvas.h"
#include "SpellChecker.h"
#include "TextObject.h"
#include "Tool.h"

#include <QTextCursor>
#include <QHash>
#include <QTextDocument>

#include <functional>
#include <memory>
#include <optional>

class QMenu;

namespace vek {

// Textwerkzeug (F8):
//  - Klicken setzt Grafiktext an die Stelle (die Grundlinie der ersten Zeile liegt am Klickpunkt),
//  - Ziehen zieht einen Rahmen für Mengentext auf,
//  - Klicken in vorhandenen Text bearbeitet ihn an dieser Stelle.
//
// Bearbeitet wird eine Arbeitskopie (QTextDocument mit Cursor); die Zeichenfläche blendet das
// Original so lange aus. Erst beim Beenden (Esc, Klick daneben, Werkzeugwechsel) entsteht
// genau ein Undo-Schritt. Bei verketteten Mengentextrahmen wird der Text der ganzen Kette
// bearbeitet; er fliesst beim Tippen durch alle Rahmen.
//
// Verknüpfen: Ein Klick auf die untere Lasche eines Mengentextrahmens (auch mit dem
// Auswahlwerkzeug) startet das Verknüpfen. Danach verbindet ein Klick auf einen anderen
// Mengentextrahmen, ein Klick ins Leere setzt einen gleich grossen neuen Rahmen, Ziehen zieht
// einen neuen Rahmen auf. Esc bricht ab.
class TextTool final : public Tool {
public:
    explicit TextTool(Canvas& canvas);
    ~TextTool() override;

    void activate() override;
    void deactivate() override;
    void mousePress(QMouseEvent* event, const QPointF& doc) override;
    void mouseMove(QMouseEvent* event, const QPointF& doc) override;
    void mouseRelease(QMouseEvent* event, const QPointF& doc) override;
    void mouseDoubleClick(QMouseEvent* event, const QPointF& doc) override;
    bool keyPress(QKeyEvent* event) override;
    // Text an der Schreibmarke einfügen, mit Blitzkorrektur (ein Zeichen; '\n' = Absatzende)
    void typeText(const QString& text);
    bool capturesKeyboard() const override { return editing() || linking(); }
    void paintOverlay(QPainter& p) override;

    bool editing() const { return m_doc != nullptr; }
    TextObject::Kind editingKind() const { return m_proto ? m_proto->kind() : TextObject::Kind::Artistic; }
    // Bearbeitetes Objekt (bei Ketten der erste Rahmen); 0 = neuer Text, der erst beim Beenden
    // ins Dokument kommt
    NodeId target() const { return m_target; }
    // Bearbeitung beginnen (z.B. Doppelklick mit dem Auswahlwerkzeug); Schreibmarke an der
    // Stelle at (Seitenkoordinaten) oder am Ende
    void beginEditing(NodeId id, std::optional<QPointF> at = std::nullopt);
    // Bearbeitung abschliessen und als Undo-Schritt übernehmen
    void finishEditing();

    // Verknüpfen ab dem Rahmen from; returnToPick: danach zurück zum Auswahlwerkzeug
    void beginLinking(NodeId from, bool returnToPick);
    bool linking() const { return m_linkFrom != 0; }

    // Format an der Schreibmarke bzw. am Anfang der Markierung
    CharFormat currentFormat() const;
    TextParagraph::Align currentAlignment() const;
    // Auf die Markierung anwenden, ohne Markierung auf das, was als Nächstes getippt wird
    void applyFormat(const std::function<void(CharFormat&)>& fn);
    void setAlignment(TextParagraph::Align align);
    // Absatz an der Schreibmarke: Aufzählungszeichen bzw. Initiale über lines Zeilen (0 = aus)
    void setBullet(bool on);
    void setDropCap(int lines);
    bool hyphenate() const { return m_proto && m_proto->hyphenate(); }
    void setHyphenate(bool on);
    QString text() const { return m_doc ? m_doc->toPlainText() : QString(); }
    // Text der Arbeitskopie passt nicht in die Rahmen
    bool overflows() const { return editing() && editingKind() == TextObject::Kind::Paragraph && flow().overflows(); }

    // Für das Lineal beim Bearbeiten von Mengentext: Rahmen links/rechts in Seiten-mm und die
    // Tabulatoren des Absatzes an der Schreibmarke (mm ab Rahmen links). false = kein Lineal
    // (Grafiktext oder gedrehter Rahmen).
    bool rulerFrame(double* left, double* right, QList<double>* tabs) const;
    // Tabulatoren für die markierten Absätze setzen
    void setTabs(const QList<double>& tabs);
    QTextCursor& cursor() { return m_cursor; }

    // Rechtschreibfehler der Arbeitskopie (Textpositionen); das Wort, an dessen Ende gerade
    // getippt wird, zählt noch nicht
    QList<SpellChecker::Range> spellingErrors() const;
    // Kontextmenü mit Vorschlägen für das falsch geschriebene Wort an position; false = keins
    bool fillSpellMenu(QMenu& menu, int position);
    void replaceRange(int start, int length, const QString& text);

private:
    void startEditing(std::unique_ptr<TextObject> proto, NodeId target, const QList<TextFrameGeometry>& frames);
    void stopEditing();
    const TextFlow& flow() const;
    // Rahmen der Arbeitskopie, lokal in mm (Grafiktext: Ausdehnung des Textes)
    QRectF frameRect(int frame) const;
    // Rahmen an einer Seitenstelle (nur sichtbare), -1 = keiner
    int frameAt(const QPointF& doc) const;
    int positionAt(const QPointF& doc) const;
    // Rahmen, in dem die Schreibmarke steht
    int caretFrame() const;
    void changed();
    void finishLinking(const QPointF& doc);
    void endLinking();

    std::unique_ptr<TextObject> m_proto; // Lage, Stil, Art, Rahmen; Inhalt steht in m_doc
    std::unique_ptr<QTextDocument> m_doc;
    QTextCursor m_cursor;
    NodeId m_target = 0;
    NodeId m_selectAfter = 0;          // beim Beenden auswählen (angeklickter Rahmen)
    QList<TextFrameGeometry> m_frames; // Rahmen der Kette (bzw. der eine Rahmen)
    mutable std::unique_ptr<TextFlow> m_flow;
    mutable int m_spellRevision = -1;
    mutable QString m_spellLanguage;
    mutable QList<SpellChecker::Range> m_spellErrors;
    // "Fehler in allen Textrahmen anzeigen": Wellenlinien anderer Mengentexte, lokal je Objekt
    struct OtherErrors {
        QString key; // Text + Rahmengrösse + Sprache
        QList<QLineF> lines;
    };
    mutable QHash<NodeId, OtherErrors> m_otherErrors;
    void paintOtherSpelling(QPainter& p) const;

    NodeId m_linkFrom = 0;
    bool m_linkReturnToPick = false;

    bool m_dragging = false; // Rahmen aufziehen
    bool m_selecting = false;
    QPointF m_start;
    QPointF m_current;
};

} // namespace vek
