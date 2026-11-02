#pragma once

#include "Color.h"
#include "DrawObject.h"
#include "TextFlow.h"

#include <QList>
#include <QSizeF>
#include <QString>
#include <QTextCharFormat>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

class QTextDocument;

namespace vek {

// Zeichenformat eines Textbereichs
struct CharFormat {
    CharFormat() = default;
    CharFormat(QString fontFamily, double size, bool isBold = false, bool isItalic = false, bool isUnderline = false,
               std::optional<Color> textColor = std::nullopt)
        : family(std::move(fontFamily)), sizePt(size), bold(isBold), italic(isItalic), underline(isUnderline),
          color(textColor)
    {
    }

    QString family = QStringLiteral("Arial");
    double sizePt = 24.0;
    bool bold = false;
    bool italic = false;
    bool underline = false;
    std::optional<Color> color; // leer = Füllung des Objekts

    friend bool operator==(const CharFormat&, const CharFormat&) = default;
};

struct TextRun {
    QString text;
    CharFormat format;
    friend bool operator==(const TextRun&, const TextRun&) = default;
};

struct TextParagraph {
    enum class Align { Left, Center, Right, Justify };

    // Mindestens ein Lauf, auch bei leerem Absatz: er trägt das Format für die Zeilenhöhe
    QList<TextRun> runs;
    Align align = Align::Left;
    QList<double> tabs; // Tabulatoren in mm ab dem linken Rand
    bool bullet = false; // Aufzählungszeichen (hängender Einzug)
    int dropCap = 0;     // Initiale über so viele Zeilen (0 = keine)

    friend bool operator==(const TextParagraph&, const TextParagraph&) = default;
};

// Text in zwei Arten:
//  - Grafiktext (Artistic): eine oder mehrere Zeilen ohne Umbruch, verhält sich wie ein Objekt.
//    Skalieren und Neigen bleiben in der Matrix, die Schrift wird mit verzerrt.
//  - Mengentext (Paragraph): fliesst in einen Rahmen und bricht an dessen Breite um. Wird der
//    Rahmen skaliert, ändert sich nur seine Grösse und der Text bricht neu um. Text, der nicht
//    in den Rahmen passt, ist ausgeblendet. Rahmen lassen sich verketten (auch über Seiten
//    hinweg): Der Text steht im ersten Rahmen der Kette und fliesst durch die folgenden.
//
// Lokale Koordinaten: (0,0) ist die linke obere Ecke des Textblocks bzw. Rahmens, Masse in mm.
// Gesetzt wird mit QTextDocument; die Glyphen werden zu Pfaden, damit Füllung, Kontur,
// Transformation, Treffertest und "In Kurven" wie bei jedem anderen Objekt funktionieren.
class TextObject final : public DrawObject {
public:
    enum class Kind { Artistic, Paragraph };

    using Piece = TextPiece;

    // Layout-Einheiten je mm (QFont-Pixelgrössen müssen ganzzahlig sein)
    static constexpr double kUnitsPerMm = 100.0;
    // Absatzeigenschaften im QTextBlockFormat (QTextFormat::UserProperty + n)
    static constexpr int kBulletProperty = 0x100000 + 10;
    static constexpr int kDropCapProperty = 0x100000 + 11;
    // Standard-Tabulatorabstand (0,5 Zoll)
    static constexpr double kDefaultTabMm = 12.7;

    TextObject() = default;
    explicit TextObject(Kind kind, const QSizeF& frame = {});

    QString typeName() const override { return QStringLiteral("text"); }
    QPainterPath localPath() const override;
    QRectF boundingRect() const override;
    // An einen Pfad angepasster Text liegt in Seitenkoordinaten auf dem Pfad
    QPainterPath path() const override;
    // Rahmen (Mengentext) bzw. Ausdehnung (Grafiktext) statt der Glyphen
    QPainterPath snapPath() const override;
    bool hitTest(const QPointF& point, double tolerance, bool treatAsFilled) const override;
    void applyTransform(const QTransform& m) override;

    Kind kind() const { return m_kind; }
    QSizeF frameSize() const { return m_frame; }
    void setFrameSize(const QSizeF& size);
    // Rahmen (Mengentext) bzw. Ausdehnung des gesetzten Textes (Grafiktext), lokal
    QRectF frameRect() const;

    const QList<TextParagraph>& paragraphs() const { return m_paragraphs; }
    void setParagraphs(const QList<TextParagraph>& paragraphs);
    // Ganzer Text als ein Absatz je Zeile, alle im selben Format
    void setPlainText(const QString& text, const CharFormat& format = {});
    QString plainText() const;
    bool isEmpty() const { return plainText().isEmpty(); }

    // Format auf den ganzen Text anwenden (Eigenschaftsleiste ohne Textbearbeitung)
    void applyToAll(const std::function<void(CharFormat&)>& fn);
    void setAlignment(TextParagraph::Align align);
    // Silbentrennung (Mengentext): weiche Trennstriche nur beim Setzen, nie im gespeicherten Text
    bool hyphenate() const { return m_hyphenate; }
    void setHyphenate(bool on);

    // Gefärbte Glyphenpfade, lokal in mm (bei Folgerahmen der Teil des Kettentextes, der hier steht)
    const QList<Piece>& pieces() const;
    // Wie pieces(), aber durch die Hüllen verformt (so wie der Text erscheint)
    QList<Piece> shownPieces() const;

    // Text an Pfad anpassen (Grafiktext): Buchstaben laufen einzeln gedreht
    // am Umriss von pathObject entlang. offset = Abstand vom Pfadanfang, distance = Abstand
    // vom Pfad (mm), flip = auf die andere Seite/in Gegenrichtung. Die Transformation des Textes
    // gilt dann nicht; der Text folgt dem Pfadobjekt.
    NodeId pathObject() const { return m_pathId; }
    bool onPath() const { return m_pathId != 0; }
    // Als echte Schrift ausgebbar (PDF): weder an einen Pfad angepasst noch durch eine Hülle verformt
    bool exportableAsText() const { return !fitted() && !hasEnvelope(); }
    double pathOffset() const { return m_pathOffset; }
    double pathDistance() const { return m_pathDistance; }
    bool pathFlip() const { return m_pathFlip; }
    void setPathFit(NodeId pathObject, double offset, double distance, bool flip);
    // Vom Dokument nach jeder Änderung gesetzt: Umriss des Pfadobjekts (Seite)
    void resolvePath(const std::function<const DrawObject*(NodeId)>& find) const;

    // Verketteter Mengentext: Folgerahmen (0 = keiner). Gespeichert wird nur dieser Verweis.
    NodeId nextFrame() const { return m_next; }
    void setNextFrame(NodeId id) { m_next = id; }
    // Kette, in der dieser Rahmen liegt, in Flussreihenfolge; leer = nicht verkettet.
    // Das Dokument setzt sie nach jeder Änderung neu (linkChains).
    QList<NodeId> chain() const { return m_chain ? *m_chain : QList<NodeId>(); }
    // Erster Rahmen der Kette (hält den Text); eigene Id, wenn nicht verkettet
    NodeId chainHead() const { return m_chain ? m_chain->first() : id(); }
    bool isFollower() const { return m_chainIndex > 0; }
    // Text passt nicht in den Rahmen bzw. in alle Rahmen der Kette
    bool overflows() const;
    // Ketten aus den nextFrame-Verweisen aufbauen und den Text durch die Rahmen fliessen lassen.
    // Ungültige Verweise (Ziel fehlt, hat schon einen Vorgänger, Kreis) zählen nicht.
    static void linkChains(std::vector<const TextObject*> texts);
    // Erster Rahmen einer Kette: der Text, der gerade in jedem Rahmen steht (der letzte bekommt
    // auch den Überlauf). Zum Aufheben der Verknüpfung.
    QList<QList<TextParagraph>> paragraphsByFrame() const;

    // Arbeitskopie zum Bearbeiten und zurück. Das Dokument ist schon gesetzt (Breite, Umbruch).
    // withHyphens: Text mit weichen Trennstrichen für die Anzeige (nicht zum Bearbeiten)
    std::unique_ptr<QTextDocument> toDocument(bool withHyphens = false) const;
    void setFromDocument(const QTextDocument& doc);
    // Setzt ein Dokument so, wie dieses Objekt es setzen würde (Breite bei Mengentext)
    void prepareLayout(QTextDocument& doc) const;
    static QTextCharFormat toQtFormat(const CharFormat& format);
    static CharFormat fromQtFormat(const QTextCharFormat& format);
    static Qt::Alignment toQtAlignment(TextParagraph::Align align);
    static TextParagraph::Align fromQtAlignment(Qt::Alignment align);

    void writeProperties(QJsonObject& out) const override;
    void readProperties(const QJsonObject& in) override;

private:
    struct FrameContent {
        QList<Piece> pieces;
        QPainterPath all;
    };
    struct Cache {
        QList<FrameContent> frames; // je Rahmen der Kette
        QRectF extent;
        bool overflow = false;
        QList<TextGlyph> glyphs; // erster Rahmen, für Text an Pfad
    };
    bool fitted() const { return m_pathId != 0 && !m_pathShape.isEmpty(); }
    const FrameContent& fittedContent() const;
    const Cache& cache() const;
    const FrameContent* content() const;
    void invalidate() const { m_cache.reset(); }
    void clearChain() const;

    Kind m_kind = Kind::Artistic;
    QSizeF m_frame;
    QList<TextParagraph> m_paragraphs;
    bool m_hyphenate = false;
    NodeId m_next = 0;
    NodeId m_pathId = 0;
    double m_pathOffset = 0.0;
    double m_pathDistance = 0.0;
    bool m_pathFlip = false;
    mutable QPainterPath m_pathShape;
    struct FitCache {
        QPainterPath shape;
        double offset = 0, distance = 0;
        bool flip = false;
        std::shared_ptr<const Cache> source;
        FrameContent content;
    };
    mutable FitCache m_fit;
    mutable std::shared_ptr<const Cache> m_cache;
    // Von linkChains gesetzt: Kette, eigene Stelle darin, Grössen der Folgerahmen (nur erster
    // Rahmen) und dessen Satz (nur Folgerahmen)
    mutable std::shared_ptr<const QList<NodeId>> m_chain;
    mutable int m_chainIndex = 0;
    mutable QList<QSizeF> m_followerSizes;
    mutable std::shared_ptr<const Cache> m_source;
};

} // namespace vek
