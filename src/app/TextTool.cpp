#include "TextTool.h"

#include "Options.h"
#include "Serialization.h"

#include <QClipboard>
#include <QCursor>
#include <QMenu>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QTextBlock>

#include <algorithm>
#include <functional>
#include <cmath>

namespace vek {

namespace {

Style textStyle()
{
    // Text: schwarz gefüllt, ohne Kontur
    Style s;
    s.fill = Fill::solid(Color(0, 0, 0));
    s.stroke.reset();
    return s;
}

} // namespace

TextTool::TextTool(Canvas& canvas) : Tool(canvas) {}

TextTool::~TextTool() = default;

void TextTool::activate()
{
    m_canvas.setCursor(linking() ? Qt::DragLinkCursor : Qt::IBeamCursor);
}

void TextTool::deactivate()
{
    finishEditing();
    endLinking();
    m_dragging = false;
    m_selecting = false;
}

const TextFlow& TextTool::flow() const
{
    if (!m_flow) {
        QList<QSizeF> sizes;
        for (const TextFrameGeometry& f : m_frames)
            sizes << f.size;
        const bool paragraph = m_proto->kind() == TextObject::Kind::Paragraph;
        m_flow = std::make_unique<TextFlow>(*m_doc, paragraph ? sizes : QList<QSizeF>(), paragraph);
    }
    return *m_flow;
}

QRectF TextTool::frameRect(int frame) const
{
    if (m_proto->kind() == TextObject::Kind::Paragraph)
        return QRectF(QPointF(0, 0), m_frames[frame].size);
    return flow().extent();
}

int TextTool::frameAt(const QPointF& doc) const
{
    const double pad = m_canvas.pixelsToDoc(4.0);
    for (int i = 0; i < m_frames.size(); ++i) {
        if (!m_frames[i].visible)
            continue;
        const QPolygonF frame = m_frames[i].transform.map(QPolygonF(frameRect(i).adjusted(-pad, -pad, pad, pad)));
        if (frame.containsPoint(doc, Qt::OddEvenFill))
            return i;
    }
    return -1;
}

int TextTool::positionAt(const QPointF& doc) const
{
    int frame = frameAt(doc);
    if (frame < 0) // ausserhalb: im nächstgelegenen sichtbaren Rahmen
        frame = std::max(0, caretFrame());
    return flow().positionAt(frame, m_frames[frame].transform.inverted().map(doc));
}

int TextTool::caretFrame() const
{
    const int frame = flow().caret(m_cursor.position()).frame;
    return frame >= 0 && frame < m_frames.size() ? frame : 0;
}

void TextTool::changed()
{
    m_flow.reset();
    m_canvas.update();
    m_canvas.notifyToolState();
}

void TextTool::startEditing(std::unique_ptr<TextObject> proto, NodeId target, const QList<TextFrameGeometry>& frames)
{
    m_proto = std::move(proto);
    m_frames = frames;
    m_doc = m_proto->toDocument();
    m_doc->setUndoRedoEnabled(true);
    // Auch Änderungen von aussen (Eigenschaftsleiste über cursor()) setzen den Text neu
    QObject::connect(m_doc.get(), &QTextDocument::contentsChanged, m_doc.get(), [this] { m_flow.reset(); });
    m_cursor = QTextCursor(m_doc.get());
    m_cursor.movePosition(QTextCursor::End);
    m_target = target;
    QList<NodeId> hidden;
    for (const TextFrameGeometry& f : m_frames)
        hidden << f.id;
    m_canvas.setHiddenObjects(hidden);
    changed();
}

void TextTool::stopEditing()
{
    m_cursor = QTextCursor();
    m_flow.reset();
    m_doc.reset();
    m_proto.reset();
    m_frames.clear();
    m_target = 0;
    m_canvas.setHiddenObjects({});
    changed();
}

void TextTool::beginEditing(NodeId id, std::optional<QPointF> at)
{
    finishEditing();
    endLinking();
    Editor& ed = m_canvas.editor();
    const auto* clicked = ed.document().findAs<TextObject>(id);
    if (!clicked || ed.isLocked(id))
        return;
    // Bei einer Kette steht der Text im ersten Rahmen
    const NodeId headId = clicked->chainHead();
    const auto* original = ed.document().findAs<TextObject>(headId);
    if (!original || ed.isLocked(headId))
        return;
    std::unique_ptr<Node> copy = serial::fromJson(serial::toJson(*original), serial::Ids::Keep);
    auto* text = node_cast<TextObject>(copy.get());
    if (!text)
        return;
    copy.release();
    m_selectAfter = id;
    startEditing(std::unique_ptr<TextObject>(text), headId, m_canvas.textChainGeometry(headId));
    if (at) {
        m_cursor.setPosition(positionAt(*at));
        changed();
    }
    ed.setSelection({id});
}

void TextTool::finishEditing()
{
    if (!editing())
        return;
    Editor& ed = m_canvas.editor();
    std::unique_ptr<TextObject> result = std::move(m_proto);
    result->setFromDocument(*m_doc);
    const NodeId target = m_target;
    const NodeId select = m_selectAfter ? m_selectAfter : target;
    const bool chained = m_frames.size() > 1;
    m_selectAfter = 0;
    stopEditing();

    if (target == 0) {
        if (!result->isEmpty())
            ed.addObject(std::move(result), QObject::tr("Text erstellen"));
        return;
    }
    const auto* original = ed.document().findAs<TextObject>(target);
    if (!original)
        return;
    if (result->isEmpty() && !chained) {
        // Leerer Text verschwindet (verkettete Rahmen bleiben stehen)
        ed.setSelection({target});
        ed.deleteSelection();
        return;
    }
    if (original->paragraphs() != result->paragraphs() || original->hyphenate() != result->hyphenate()) {
        const QList<TextParagraph> paragraphs = result->paragraphs();
        const bool hyphenate = result->hyphenate();
        ed.modifyText(target, [&paragraphs, hyphenate](TextObject& t) {
            t.setParagraphs(paragraphs);
            t.setHyphenate(hyphenate);
        }, QObject::tr("Text bearbeiten"));
    }
    ed.setSelection({ed.document().find(select) ? select : target});
}

void TextTool::beginLinking(NodeId from, bool returnToPick)
{
    finishEditing();
    const auto* text = m_canvas.editor().document().findAs<TextObject>(from);
    if (!text || text->kind() != TextObject::Kind::Paragraph)
        return;
    m_linkFrom = from;
    m_linkReturnToPick = returnToPick;
    m_dragging = false;
    m_canvas.setCursor(Qt::DragLinkCursor);
    m_canvas.update();
}

void TextTool::endLinking()
{
    if (!linking())
        return;
    m_linkFrom = 0;
    m_dragging = false;
    m_canvas.setCursor(Qt::IBeamCursor);
    m_canvas.update();
}

void TextTool::finishLinking(const QPointF& doc)
{
    Editor& ed = m_canvas.editor();
    const NodeId from = m_linkFrom;
    const bool back = m_linkReturnToPick;
    const QRectF r = QRectF(m_start, doc).normalized();
    endLinking();
    bool done = false;
    if (r.width() >= m_canvas.pixelsToDoc(4.0) && r.height() >= m_canvas.pixelsToDoc(4.0)) {
        done = ed.linkNewTextFrame(from, r) != InvalidNodeId;
    } else if (const auto* hit = node_cast<TextObject>(ed.objectAt(doc, m_canvas.pixelsToDoc(3.0)))) {
        done = ed.linkTextFrame(from, hit->id());
        if (done)
            ed.setSelection({hit->id()});
    } else if (!ed.objectAt(doc, m_canvas.pixelsToDoc(3.0))) {
        // Klick ins Leere: neuer Rahmen in der Grösse des bisherigen
        if (const auto* source = ed.document().findAs<TextObject>(from))
            done = ed.linkNewTextFrame(from, QRectF(doc, source->frameSize())) != InvalidNodeId;
    }
    Q_UNUSED(done);
    if (back)
        m_canvas.setTool(ToolId::Pick);
}

CharFormat TextTool::currentFormat() const
{
    if (!editing())
        return {};
    QTextCursor c = m_cursor;
    if (c.hasSelection()) {
        // Format des ersten markierten Zeichens
        const int start = c.selectionStart();
        c.setPosition(start + 1);
    }
    return TextObject::fromQtFormat(c.charFormat());
}

TextParagraph::Align TextTool::currentAlignment() const
{
    return editing() ? TextObject::fromQtAlignment(m_cursor.blockFormat().alignment()) : TextParagraph::Align::Left;
}

void TextTool::applyFormat(const std::function<void(CharFormat&)>& fn)
{
    if (!editing())
        return;
    if (!m_cursor.hasSelection()) {
        CharFormat f = currentFormat();
        fn(f);
        m_cursor.setCharFormat(TextObject::toQtFormat(f));
        if (m_cursor.block().length() <= 1) // leerer Absatz: Zeilenhöhe folgt dem neuen Format
            m_cursor.setBlockCharFormat(TextObject::toQtFormat(f));
        changed();
        return;
    }
    // Jedes Fragment der Markierung für sich, damit nur die geänderte Eigenschaft wechselt
    struct Span {
        int from;
        int to;
        QTextCharFormat format;
    };
    QList<Span> spans;
    const int start = m_cursor.selectionStart();
    const int end = m_cursor.selectionEnd();
    for (QTextBlock block = m_doc->findBlock(start); block.isValid() && block.position() < end; block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            const int from = std::max(start, fragment.position());
            const int to = std::min(end, fragment.position() + fragment.length());
            if (from < to)
                spans << Span{from, to, fragment.charFormat()};
        }
    }
    m_cursor.beginEditBlock();
    for (const Span& s : spans) {
        CharFormat f = TextObject::fromQtFormat(s.format);
        fn(f);
        QTextCursor c(m_doc.get());
        c.setPosition(s.from);
        c.setPosition(s.to, QTextCursor::KeepAnchor);
        c.setCharFormat(TextObject::toQtFormat(f));
    }
    m_cursor.endEditBlock();
    changed();
}

void TextTool::setHyphenate(bool on)
{
    if (!editing())
        return;
    m_proto->setHyphenate(on);
    changed();
}

bool TextTool::rulerFrame(double* left, double* right, QList<double>* tabs) const
{
    if (!editing() || m_proto->kind() != TextObject::Kind::Paragraph)
        return false;
    const TextFrameGeometry& f = m_frames[caretFrame()];
    const QTransform& t = f.transform;
    if (std::abs(t.m12()) > 1e-9 || std::abs(t.m21()) > 1e-9 || t.m11() <= 0.0)
        return false;
    const QRectF frame = t.mapRect(QRectF(QPointF(0, 0), f.size));
    *left = frame.left();
    *right = frame.right();
    tabs->clear();
    for (const QTextOption::Tab& tab : m_cursor.blockFormat().tabPositions())
        tabs->append(tab.position / TextObject::kUnitsPerMm * t.m11());
    return true;
}

void TextTool::setTabs(const QList<double>& tabs)
{
    if (!editing())
        return;
    QList<double> sorted = tabs;
    std::sort(sorted.begin(), sorted.end());
    QList<QTextOption::Tab> list;
    const QTransform& t = m_frames[caretFrame()].transform;
    const double scale = t.m11() > 0 ? t.m11() : 1.0;
    for (double mm : sorted)
        list << QTextOption::Tab(mm / scale * TextObject::kUnitsPerMm, QTextOption::LeftTab);
    QTextBlockFormat bf;
    bf.setTabPositions(list);
    m_cursor.mergeBlockFormat(bf);
    changed();
}

void TextTool::setAlignment(TextParagraph::Align align)
{
    if (!editing())
        return;
    QTextBlockFormat bf;
    bf.setAlignment(TextObject::toQtAlignment(align));
    m_cursor.mergeBlockFormat(bf);
    changed();
}

void TextTool::setBullet(bool on)
{
    if (!editing())
        return;
    QTextBlockFormat bf;
    bf.setProperty(TextObject::kBulletProperty, on);
    m_cursor.mergeBlockFormat(bf);
    changed();
}

void TextTool::setDropCap(int lines)
{
    if (!editing())
        return;
    QTextBlockFormat bf;
    bf.setProperty(TextObject::kDropCapProperty, lines);
    m_cursor.mergeBlockFormat(bf);
    changed();
}

void TextTool::mousePress(QMouseEvent* event, const QPointF& doc)
{
    if (linking()) {
        if (event->button() == Qt::LeftButton) {
            m_dragging = true;
            m_start = m_current = doc;
        } else {
            endLinking();
        }
        return;
    }
    if (event->button() == Qt::RightButton && editing() && frameAt(doc) >= 0) {
        // Rechtschreibvorschläge
        QMenu menu;
        if (fillSpellMenu(menu, positionAt(doc)))
            menu.exec(QCursor::pos());
        return;
    }
    if (event->button() != Qt::LeftButton)
        return;
    if (editing() && m_proto->kind() == TextObject::Kind::Paragraph) {
        // Untere Lasche eines Rahmens: verknüpfen
        const int tab = m_canvas.textLinkTabAt(event->position(), m_frames);
        if (tab >= 0) {
            NodeId from = m_frames[tab].id;
            if (from == 0) { // neuer Text: erst anlegen
                finishEditing();
                from = m_canvas.editor().selection().value(0);
            }
            if (from != 0)
                beginLinking(from, false);
            return;
        }
    }
    if (editing() && frameAt(doc) >= 0) {
        m_selecting = true;
        m_cursor.setPosition(positionAt(doc),
                             event->modifiers() & Qt::ShiftModifier ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor);
        changed();
        return;
    }
    finishEditing();
    Editor& ed = m_canvas.editor();
    if (const auto* hit = node_cast<TextObject>(ed.objectAt(doc, m_canvas.pixelsToDoc(3.0)))) {
        beginEditing(hit->id(), doc);
        m_selecting = editing();
        return;
    }
    m_dragging = true;
    m_start = m_current = doc;
}

void TextTool::mouseMove(QMouseEvent*, const QPointF& doc)
{
    if (m_selecting && editing()) {
        m_cursor.setPosition(positionAt(doc), QTextCursor::KeepAnchor);
        changed();
        return;
    }
    if (m_dragging) {
        m_current = doc;
        m_canvas.update();
    }
}

void TextTool::mouseRelease(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton)
        return;
    if (m_selecting) {
        m_selecting = false;
        return;
    }
    if (!m_dragging)
        return;
    m_dragging = false;
    m_current = doc;
    if (linking()) {
        finishLinking(doc);
        return;
    }
    const QRectF r = QRectF(m_start, m_current).normalized();
    const ShapeDefaults& d = m_canvas.editor().shapeDefaults();
    std::unique_ptr<TextObject> proto;
    if (r.width() < m_canvas.pixelsToDoc(4.0) || r.height() < m_canvas.pixelsToDoc(4.0)) {
        // Grafiktext: Grundlinie der ersten Zeile am Klickpunkt
        proto = std::make_unique<TextObject>(TextObject::Kind::Artistic);
        proto->setPlainText(QString(), d.artisticText);
        const QFontMetricsF metrics(TextObject::toQtFormat(d.artisticText).font());
        proto->setTransform(QTransform::fromTranslate(m_start.x(), m_start.y() - metrics.ascent() / TextObject::kUnitsPerMm));
    } else {
        proto = std::make_unique<TextObject>(TextObject::Kind::Paragraph, r.size());
        proto->setPlainText(QString(), d.paragraphText);
        proto->setTransform(QTransform::fromTranslate(r.left(), r.top()));
    }
    proto->setStyle(textStyle());
    m_canvas.editor().clearSelection();
    const TextFrameGeometry frame{0, proto->transform(), proto->frameSize(), true};
    startEditing(std::move(proto), 0, {frame});
}

void TextTool::mouseDoubleClick(QMouseEvent*, const QPointF& doc)
{
    if (!editing() || frameAt(doc) < 0)
        return;
    m_cursor.setPosition(positionAt(doc));
    m_cursor.select(QTextCursor::WordUnderCursor);
    m_selecting = false;
    changed();
}

bool TextTool::keyPress(QKeyEvent* e)
{
    if (linking()) {
        if (e->key() == Qt::Key_Escape) {
            const bool back = m_linkReturnToPick;
            endLinking();
            if (back)
                m_canvas.setTool(ToolId::Pick);
        }
        return true;
    }
    if (!editing())
        return false;
    const bool ctrl = e->modifiers() & Qt::ControlModifier;
    const auto mode = e->modifiers() & Qt::ShiftModifier ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor;
    auto move = [&](QTextCursor::MoveOperation op) {
        m_cursor.movePosition(op, mode);
        changed();
        return true;
    };
    // Zeilenweise Bewegung nach dem eigenen Satz (über Rahmengrenzen hinweg)
    auto moveTo = [&](int position) {
        m_cursor.setPosition(position, mode);
        changed();
        return true;
    };

    if (e->matches(QKeySequence::Undo)) {
        m_doc->undo(&m_cursor);
        changed();
        return true;
    }
    if (e->matches(QKeySequence::Redo)) {
        m_doc->redo(&m_cursor);
        changed();
        return true;
    }
    if (e->matches(QKeySequence::SelectAll)) {
        m_cursor.select(QTextCursor::Document);
        changed();
        return true;
    }
    if (e->matches(QKeySequence::Copy) || e->matches(QKeySequence::Cut)) {
        if (m_cursor.hasSelection()) {
            QGuiApplication::clipboard()->setText(m_cursor.selectedText().replace(QChar::ParagraphSeparator, QLatin1Char('\n')));
            if (e->matches(QKeySequence::Cut))
                m_cursor.removeSelectedText();
            changed();
        }
        return true;
    }
    if (e->matches(QKeySequence::Paste)) {
        m_cursor.insertText(QGuiApplication::clipboard()->text());
        changed();
        return true;
    }
    if (ctrl && (e->key() == Qt::Key_B || e->key() == Qt::Key_I || e->key() == Qt::Key_U)) {
        const CharFormat now = currentFormat();
        const int key = e->key();
        applyFormat([&now, key](CharFormat& f) {
            if (key == Qt::Key_B)
                f.bold = !now.bold;
            else if (key == Qt::Key_I)
                f.italic = !now.italic;
            else
                f.underline = !now.underline;
        });
        return true;
    }

    switch (e->key()) {
    case Qt::Key_Escape:
        finishEditing();
        return true;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        typeText(QStringLiteral("\n"));
        return true;
    case Qt::Key_Tab:
        m_cursor.insertText(QStringLiteral("\t"));
        changed();
        return true;
    case Qt::Key_Backspace:
        if (!m_cursor.hasSelection() && ctrl)
            m_cursor.movePosition(QTextCursor::PreviousWord, QTextCursor::KeepAnchor);
        if (m_cursor.hasSelection())
            m_cursor.removeSelectedText();
        else
            m_cursor.deletePreviousChar();
        changed();
        return true;
    case Qt::Key_Delete:
        if (!m_cursor.hasSelection() && ctrl)
            m_cursor.movePosition(QTextCursor::NextWord, QTextCursor::KeepAnchor);
        if (m_cursor.hasSelection())
            m_cursor.removeSelectedText();
        else
            m_cursor.deleteChar();
        changed();
        return true;
    case Qt::Key_Left: return move(ctrl ? QTextCursor::PreviousWord : QTextCursor::Left);
    case Qt::Key_Right: return move(ctrl ? QTextCursor::NextWord : QTextCursor::Right);
    case Qt::Key_Up: return moveTo(flow().lineAbove(m_cursor.position()));
    case Qt::Key_Down: return moveTo(flow().lineBelow(m_cursor.position()));
    case Qt::Key_Home: return ctrl ? move(QTextCursor::Start) : moveTo(flow().lineStart(m_cursor.position()));
    case Qt::Key_End: return ctrl ? move(QTextCursor::End) : moveTo(flow().lineEnd(m_cursor.position()));
    default:
        break;
    }
    // Druckbare Zeichen; AltGr (Strg+Alt unter Windows) liefert z.B. @ und €
    const QString text = e->text();
    const bool altGr = (e->modifiers() & Qt::ControlModifier) && (e->modifiers() & Qt::AltModifier);
    if (!text.isEmpty() && text.at(0).isPrint() && (!ctrl || altGr))
        typeText(text);
    // Alle übrigen Tasten bleiben beim Text, damit z.B. Buchstaben-Kürzel kein Werkzeug wechseln
    return true;
}

QList<SpellChecker::Range> TextTool::spellingErrors() const
{
    if (!editing())
        return {};
    const Options& o = Options::instance();
    SpellChecker& sc = SpellChecker::instance();
    if (m_spellRevision != m_doc->revision() || m_spellLanguage != sc.language()) {
        m_spellErrors = sc.check(m_doc->toPlainText(), o.flag(QStringLiteral("ws/text/spell/showIgnored")));
        m_spellRevision = m_doc->revision();
        m_spellLanguage = sc.language();
    }
    QList<SpellChecker::Range> result = m_spellErrors;
    // Das Wort direkt vor der Schreibmarke wird erst nach dem Weitertippen geprüft
    if (!m_cursor.hasSelection()) {
        const int pos = m_cursor.position();
        result.removeIf([pos](const SpellChecker::Range& r) { return pos >= r.start && pos <= r.start + r.length; });
    }
    return result;
}

void TextTool::replaceRange(int start, int length, const QString& text)
{
    if (!editing())
        return;
    QTextCursor c(m_doc.get());
    c.setPosition(start);
    c.setPosition(start + length, QTextCursor::KeepAnchor);
    c.insertText(text);
    m_cursor.setPosition(start + int(text.size()));
    changed();
}

bool TextTool::fillSpellMenu(QMenu& menu, int position)
{
    if (!editing())
        return false;
    const QString plain = m_doc->toPlainText();
    SpellChecker& sc = SpellChecker::instance();
    const SpellChecker::Range w = SpellChecker::wordAt(plain, position);
    if (w.length == 0)
        return false;
    const QString word = plain.mid(w.start, w.length);
    if (sc.isCorrect(word))
        return false;
    const Options& o = Options::instance();
    const QStringList suggestions = sc.suggestions(word, std::max(1, o.integer(QStringLiteral("ws/text/spell/suggestions"))));
    for (const QString& s : suggestions) {
        QAction* a = menu.addAction(s);
        QFont f = a->font();
        f.setBold(true);
        a->setFont(f);
        QObject::connect(a, &QAction::triggered, &menu, [this, w, s, word] {
            replaceRange(w.start, w.length, s);
            // "Korrekturen der Blitzkorrektur hinzufügen"
            Options& opt = Options::instance();
            if (opt.flag(QStringLiteral("ws/text/spell/addToAutocorrect"))) {
                QString list = opt.value(QStringLiteral("ws/text/autocorrect/replacements")).toString();
                if (!list.isEmpty())
                    list += QLatin1Char('\n');
                opt.set(QStringLiteral("ws/text/autocorrect/replacements"), list + word + QLatin1Char('=') + s);
            }
        });
    }
    if (suggestions.isEmpty())
        menu.addAction(QObject::tr("(Keine Vorschläge)"))->setEnabled(false);
    menu.addSeparator();
    QObject::connect(menu.addAction(QObject::tr("Alle ignorieren")), &QAction::triggered, &menu, [this, word] {
        SpellChecker::instance().ignore(word);
        m_spellRevision = -1;
        m_canvas.update();
    });
    QObject::connect(menu.addAction(QObject::tr("Zum Wörterbuch hinzufügen")), &QAction::triggered, &menu, [this, word] {
        SpellChecker::instance().addToUserDictionary(word);
        m_spellRevision = -1;
        m_canvas.update();
    });
    return true;
}

void TextTool::typeText(const QString& text)
{
    if (!editing())
        return;
    m_cursor.beginEditBlock();
    if (text.size() == 1 && !m_cursor.hasSelection()) {
        const QString before = m_cursor.block().text().left(m_cursor.positionInBlock());
        const autocorrect::Edit fix = autocorrect::onType(before, text.at(0), Options::instance().autocorrectSettings());
        if (fix.remove > 0)
            m_cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, fix.remove);
        if (!fix.insert.isEmpty() || fix.remove > 0)
            m_cursor.insertText(fix.insert);
    } else if (text != QLatin1String("\n")) {
        m_cursor.insertText(text);
    }
    if (text == QLatin1String("\n"))
        m_cursor.insertBlock();
    m_cursor.endEditBlock();
    changed();
}

namespace {

void drawWave(QPainter& p, const QPointF& a, const QPointF& e)
{
    const QPointF along = e - a;
    const double len = std::hypot(along.x(), along.y());
    if (len < 2.0)
        return;
    const QPointF dir = along / len, normal(-dir.y(), dir.x());
    QPolygonF wave;
    for (double t = 0; t <= len; t += 2.0)
        wave << a + dir * t + normal * (int(t / 2.0) % 2 == 0 ? -1.2 : 1.2);
    p.drawPolyline(wave);
}

} // namespace

void TextTool::paintOtherSpelling(QPainter& p) const
{
    const Editor& ed = m_canvas.editor();
    SpellChecker& sc = SpellChecker::instance();
    QList<NodeId> editedChain;
    for (const TextFrameGeometry& f : m_frames)
        editedChain << f.id;
    std::function<void(const Node&)> visit = [&](const Node& n) {
        if (const auto* t = node_cast<TextObject>(&n)) {
            if (t->kind() != TextObject::Kind::Paragraph || !t->chain().isEmpty() || editedChain.contains(t->id()))
                return;
            const std::unique_ptr<QTextDocument> d = t->toDocument();
            const QString text = d->toPlainText();
            const QString key = text + QStringLiteral("|%1x%2|").arg(t->frameSize().width()).arg(t->frameSize().height()) + sc.language();
            OtherErrors& cache = m_otherErrors[t->id()];
            if (cache.key != key) {
                cache.key = key;
                cache.lines.clear();
                const QList<SpellChecker::Range> errors = sc.check(text);
                if (!errors.isEmpty()) {
                    const TextFlow f(*d, {t->frameSize()}, true);
                    for (const SpellChecker::Range& r : errors)
                        for (const QPolygonF& poly : f.selection(0, r.start, r.start + r.length).toSubpathPolygons()) {
                            const QRectF b = poly.boundingRect();
                            cache.lines << QLineF(b.bottomLeft(), b.bottomRight());
                        }
                }
            }
            const QTransform toScreen = t->transform() * m_canvas.viewTransform();
            for (const QLineF& l : std::as_const(cache.lines))
                drawWave(p, toScreen.map(l.p1()), toScreen.map(l.p2()));
            return;
        }
        for (int i = 0; i < n.childCount(); ++i)
            visit(*n.child(i));
    };
    p.save();
    p.setPen(QPen(QColor(0xe0, 0x20, 0x20), 1.0));
    visit(ed.currentPage());
    p.restore();
}

void TextTool::paintOverlay(QPainter& p)
{
    p.save();
    const QTransform view = m_canvas.viewTransform();
    if (m_dragging) {
        QPen pen(Canvas::previewColor(), 1.0, Qt::DashLine);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawRect(view.mapRect(QRectF(m_start, m_current).normalized()));
    }
    if (linking())
        m_canvas.paintTextChain(p, m_canvas.textChainGeometry(m_linkFrom), std::nullopt);
    if (editing()) {
        const bool paragraph = m_proto->kind() == TextObject::Kind::Paragraph;
        const Style& s = m_proto->style();
        const TextFlow& f = flow();
        const TextFlow::Caret caret = f.caret(m_cursor.position());
        for (int i = 0; i < m_frames.size(); ++i) {
            if (!m_frames[i].visible)
                continue;
            const QTransform toScreen = m_frames[i].transform * view;
            // Mengentextrahmen gestrichelt
            if (paragraph) {
                p.setPen(QPen(QColor(0x50, 0x50, 0x50), 1.0, Qt::DashLine));
                p.setBrush(Qt::NoBrush);
                p.drawPolygon(toScreen.map(QPolygonF(frameRect(i))));
            }
            // Text der Arbeitskopie
            for (const TextObject::Piece& piece : f.pieces(i)) {
                if (piece.color) {
                    p.fillPath(toScreen.map(piece.path), piece.color->toQColor());
                } else if (!s.fill.isNone()) {
                    QBrush brush = s.fill.brush(piece.path.boundingRect());
                    brush.setTransform(brush.transform() * toScreen);
                    p.fillPath(toScreen.map(piece.path), brush);
                }
            }
            // Rechtschreibfehler: rote Wellenlinie unter dem Wort, ignorierte grau
            if (Options::instance().flag(QStringLiteral("ws/text/spell/auto"))) {
                for (const SpellChecker::Range& r : spellingErrors()) {
                    const QPainterPath area = f.selection(i, r.start, r.start + r.length);
                    if (area.isEmpty())
                        continue;
                    p.setPen(QPen(r.ignored ? QColor(0x90, 0x90, 0x90) : QColor(0xe0, 0x20, 0x20), 1.0));
                    p.setBrush(Qt::NoBrush);
                    for (const QPolygonF& poly : area.toSubpathPolygons()) {
                        const QRectF b = poly.boundingRect();
                        drawWave(p, toScreen.map(b.bottomLeft()), toScreen.map(b.bottomRight()));
                    }
                }
            }
            // Markierung
            if (m_cursor.hasSelection()) {
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(0x3d, 0x7b, 0xd6, 90));
                p.drawPath(toScreen.map(f.selection(i, m_cursor.selectionStart(), m_cursor.selectionEnd())));
            }
            // Schreibmarke
            if (caret.frame == i) {
                p.setPen(QPen(Qt::black, 1.5));
                p.drawLine(toScreen.map(caret.top), toScreen.map(caret.top + QPointF(0, caret.height)));
            }
        }
        if (paragraph)
            m_canvas.paintTextChain(p, m_frames, f.overflows());
    }
    const Options& o = Options::instance();
    if (o.flag(QStringLiteral("ws/text/spell/auto")) && o.integer(QStringLiteral("ws/text/spell/visibility")) == 0)
        paintOtherSpelling(p);
    p.restore();
}

} // namespace vek
