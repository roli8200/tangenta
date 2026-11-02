#include "ScriptHost.h"

#include "CurveObject.h"
#include "Editor.h"
#include "EllipseObject.h"
#include "Options.h"
#include "PolygonObject.h"
#include "RectangleObject.h"
#include "Serialization.h"
#include "TextObject.h"
#include "Transaction.h"

#include <QJSEngine>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace vek {

namespace {

QVariant colorValue(const std::optional<Color>& c)
{
    return c ? QVariant(c->toHex()) : QVariant();
}

// "#rrggbb", "rot"/"red" (Qt-Farbnamen) oder null
std::optional<Color> parseColor(const QVariant& v, bool* ok)
{
    *ok = true;
    if (!v.isValid() || v.isNull() || v.toString().isEmpty())
        return std::nullopt;
    const QColor c = QColor::fromString(v.toString());
    if (!c.isValid()) {
        *ok = false;
        return std::nullopt;
    }
    return Color::fromQColor(c);
}

} // namespace

// --- ScriptObject ---------------------------------------------------------------------------

const DrawObject* ScriptObject::object() const
{
    return m_host.editor().document().findAs<DrawObject>(m_id);
}

QString ScriptObject::type() const
{
    const DrawObject* o = object();
    return o ? o->typeName() : QString();
}

double ScriptObject::x() const
{
    const DrawObject* o = object();
    return o ? o->boundingRect().x() : 0.0;
}

double ScriptObject::y() const
{
    const DrawObject* o = object();
    return o ? o->boundingRect().y() : 0.0;
}

double ScriptObject::width() const
{
    const DrawObject* o = object();
    return o ? o->boundingRect().width() : 0.0;
}

double ScriptObject::height() const
{
    const DrawObject* o = object();
    return o ? o->boundingRect().height() : 0.0;
}

void ScriptObject::transform(const QTransform& m)
{
    if (!object())
        return;
    m_host.tx().modifySubtree<DrawObject>(m_id, [&m](DrawObject& o) { o.applyTransform(m); });
}

void ScriptObject::setX(double v)
{
    move(v - x(), 0);
}

void ScriptObject::setY(double v)
{
    move(0, v - y());
}

void ScriptObject::setWidth(double v)
{
    const double w = width();
    if (w <= 0 || v <= 0)
        return;
    const QPointF a(x(), y());
    transform(QTransform::fromTranslate(-a.x(), -a.y()) * QTransform::fromScale(v / w, 1.0) * QTransform::fromTranslate(a.x(), a.y()));
}

void ScriptObject::setHeight(double v)
{
    const double h = height();
    if (h <= 0 || v <= 0)
        return;
    const QPointF a(x(), y());
    transform(QTransform::fromTranslate(-a.x(), -a.y()) * QTransform::fromScale(1.0, v / h) * QTransform::fromTranslate(a.x(), a.y()));
}

QVariant ScriptObject::fill() const
{
    const DrawObject* o = object();
    if (!o || o->style().fill.isNone())
        return QVariant();
    return colorValue(o->style().fill.representativeColor());
}

void ScriptObject::setFill(const QVariant& v)
{
    bool ok = false;
    const std::optional<Color> c = parseColor(v, &ok);
    if (!ok) {
        m_host.engine().throwError(QStringLiteral("Unbekannte Farbe: %1").arg(v.toString()));
        return;
    }
    if (!object())
        return;
    m_host.tx().modify<DrawObject>(m_id, [&c](DrawObject& o) {
        Style s = o.style();
        s.fill = c ? Fill::solid(*c) : Fill::none();
        o.setStyle(s);
    });
}

QVariant ScriptObject::stroke() const
{
    const DrawObject* o = object();
    return o ? colorValue(o->style().stroke) : QVariant();
}

void ScriptObject::setStroke(const QVariant& v)
{
    bool ok = false;
    const std::optional<Color> c = parseColor(v, &ok);
    if (!ok) {
        m_host.engine().throwError(QStringLiteral("Unbekannte Farbe: %1").arg(v.toString()));
        return;
    }
    if (!object())
        return;
    m_host.tx().modify<DrawObject>(m_id, [&c](DrawObject& o) {
        Style s = o.style();
        s.stroke = c;
        o.setStyle(s);
    });
}

double ScriptObject::strokeWidth() const
{
    const DrawObject* o = object();
    return o ? o->style().strokeWidth : 0.0;
}

void ScriptObject::setStrokeWidth(double v)
{
    if (!object())
        return;
    m_host.tx().modify<DrawObject>(m_id, [v](DrawObject& o) {
        Style s = o.style();
        s.strokeWidth = std::max(0.0, v);
        o.setStyle(s);
    });
}

QString ScriptObject::text() const
{
    const auto* t = node_cast<TextObject>(object());
    return t ? t->plainText() : QString();
}

void ScriptObject::setText(const QString& text)
{
    const auto* t = node_cast<TextObject>(object());
    if (!t) {
        m_host.engine().throwError(QStringLiteral("Objekt %1 ist kein Text").arg(m_id));
        return;
    }
    // Format des ersten Laufs behalten
    const CharFormat format = t->paragraphs().isEmpty() || t->paragraphs().first().runs.isEmpty()
                                  ? CharFormat()
                                  : t->paragraphs().first().runs.first().format;
    m_host.tx().modify<TextObject>(m_id, [&](TextObject& o) { o.setPlainText(text, format); });
}

QString ScriptObject::name() const
{
    const DrawObject* o = object();
    return o ? o->name() : QString();
}

void ScriptObject::setName(const QString& n)
{
    if (object())
        m_host.tx().modify<DrawObject>(m_id, [&n](DrawObject& o) { o.setName(n); });
}

void ScriptObject::move(double dx, double dy)
{
    if (dx != 0.0 || dy != 0.0)
        transform(QTransform::fromTranslate(dx, dy));
}

void ScriptObject::moveTo(double nx, double ny)
{
    move(nx - x(), ny - y());
}

void ScriptObject::rotate(double degrees, QJSValue cx, QJSValue cy)
{
    const DrawObject* o = object();
    if (!o)
        return;
    QPointF c = o->boundingRect().center();
    if (cx.isNumber() && cy.isNumber())
        c = QPointF(cx.toNumber(), cy.toNumber());
    // Gegen den Uhrzeigersinn am Bildschirm (y zeigt nach unten)
    transform(QTransform::fromTranslate(-c.x(), -c.y()) * QTransform().rotate(-degrees) * QTransform::fromTranslate(c.x(), c.y()));
}

void ScriptObject::scale(double sx, QJSValue sy)
{
    const DrawObject* o = object();
    if (!o || sx == 0.0)
        return;
    const double fy = sy.isNumber() ? sy.toNumber() : sx;
    const QPointF c = o->boundingRect().center();
    transform(QTransform::fromTranslate(-c.x(), -c.y()) * QTransform::fromScale(sx, fy) * QTransform::fromTranslate(c.x(), c.y()));
}

void ScriptObject::remove()
{
    if (!object())
        return;
    m_host.newSelection().removeAll(m_id);
    m_host.tx().remove(m_id);
}

QJSValue ScriptObject::duplicate(double dx, double dy)
{
    const DrawObject* o = object();
    if (!o)
        return QJSValue(QJSValue::NullValue);
    std::unique_ptr<Node> copy = serial::fromJson(serial::toJson(*o), serial::Ids::Drop);
    auto* d = node_cast<DrawObject>(copy.get());
    if (!d)
        return QJSValue(QJSValue::NullValue);
    copy.release();
    d->applyTransform(QTransform::fromTranslate(dx, dy));
    return m_host.wrap(m_host.insert(std::unique_ptr<DrawObject>(d)));
}

QVariant ScriptObject::get(const QString& property) const
{
    const DrawObject* o = object();
    return o ? o->propertyValue(property) : QVariant();
}

void ScriptObject::set(const QString& property, const QVariant& value)
{
    if (!object())
        return;
    bool known = false;
    for (const PropertyDescriptor& d : object()->propertyDescriptors())
        known = known || d.id == property;
    if (!known) {
        m_host.engine().throwError(QStringLiteral("Unbekannte Eigenschaft '%1' (vorhanden: %2)")
                                       .arg(property, properties().join(QStringLiteral(", "))));
        return;
    }
    m_host.tx().modify<DrawObject>(m_id, [&](DrawObject& o) { o.setPropertyValue(property, value); });
}

QStringList ScriptObject::properties() const
{
    QStringList ids;
    if (const DrawObject* o = object())
        for (const PropertyDescriptor& d : o->propertyDescriptors())
            ids << d.id;
    return ids;
}

// --- ScriptApi ------------------------------------------------------------------------------

int ScriptApi::pageCount() const
{
    return m_host.editor().document().pageCount();
}

int ScriptApi::currentPage() const
{
    return m_host.editor().currentPageIndex() + 1;
}

void ScriptApi::setCurrentPage(int index)
{
    m_host.editor().setCurrentPageIndex(std::clamp(index - 1, 0, pageCount() - 1));
}

double ScriptApi::pageWidth() const
{
    return m_host.editor().currentPage().size().width();
}

double ScriptApi::pageHeight() const
{
    return m_host.editor().currentPage().size().height();
}

QString ScriptApi::version() const
{
    return QStringLiteral("1");
}

QJSValue ScriptApi::rectangle(double x, double y, double w, double h, double radius)
{
    auto r = std::make_unique<RectangleObject>(QSizeF(std::abs(w), std::abs(h)));
    if (radius > 0)
        for (auto c : {RectangleObject::TopLeft, RectangleObject::TopRight, RectangleObject::BottomRight, RectangleObject::BottomLeft})
            r->setCornerRadius(c, radius);
    r->setTransform(QTransform::fromTranslate(std::min(x, x + w), std::min(y, y + h)));
    return m_host.wrap(m_host.insert(std::move(r)));
}

QJSValue ScriptApi::ellipse(double x, double y, double w, double h)
{
    auto e = std::make_unique<EllipseObject>(QSizeF(std::abs(w), std::abs(h)));
    e->setTransform(QTransform::fromTranslate(std::min(x, x + w), std::min(y, y + h)));
    return m_host.wrap(m_host.insert(std::move(e)));
}

QJSValue ScriptApi::polygon(double x, double y, double w, double h, int corners, bool star)
{
    auto p = std::make_unique<PolygonObject>(QSizeF(std::abs(w), std::abs(h)),
                                             star ? PolygonObject::Kind::Star : PolygonObject::Kind::Polygon,
                                             std::clamp(corners, 3, 500), 53);
    p->setTransform(QTransform::fromTranslate(std::min(x, x + w), std::min(y, y + h)));
    return m_host.wrap(m_host.insert(std::move(p)));
}

QJSValue ScriptApi::line(double x1, double y1, double x2, double y2)
{
    SubPath path;
    path.nodes = {CurveNode{QPointF(x1, y1)}, CurveNode{QPointF(x2, y2)}};
    auto c = std::make_unique<CurveObject>(QList<SubPath>{path});
    return m_host.wrap(m_host.insert(std::move(c)));
}

QJSValue ScriptApi::polyline(const QJSValue& points, bool closed)
{
    SubPath path;
    const int n = points.property(QStringLiteral("length")).toInt();
    for (int i = 0; i < n; ++i) {
        const QJSValue p = points.property(quint32(i));
        path.nodes.append(CurveNode{QPointF(p.property(0).toNumber(), p.property(1).toNumber())});
    }
    if (path.nodes.size() < 2) {
        m_host.engine().throwError(QStringLiteral("polyline braucht mindestens zwei Punkte [[x, y], …]"));
        return QJSValue();
    }
    path.closed = closed;
    return m_host.wrap(m_host.insert(std::make_unique<CurveObject>(QList<SubPath>{path})));
}

QJSValue ScriptApi::text(double x, double y, const QString& text, double sizePt, const QString& font)
{
    auto t = std::make_unique<TextObject>(TextObject::Kind::Artistic);
    t->setPlainText(text, CharFormat(font.isEmpty() ? QStringLiteral("Arial") : font, sizePt));
    t->setTransform(QTransform::fromTranslate(x, y));
    return m_host.wrap(m_host.insert(std::move(t)));
}

QJSValue ScriptApi::selection() const
{
    const QList<NodeId> ids = m_host.selectionChanged() ? m_host.newSelection() : m_host.editor().selection();
    QJSValue array = m_host.engine().newArray(quint32(ids.size()));
    for (int i = 0; i < ids.size(); ++i)
        array.setProperty(quint32(i), m_host.wrap(ids[i]));
    return array;
}

void ScriptApi::select(const QJSValue& objects)
{
    QList<NodeId> ids;
    auto add = [&ids](const QJSValue& v) {
        if (auto* o = qobject_cast<ScriptObject*>(v.toQObject()))
            ids << NodeId(o->id());
    };
    if (objects.isArray()) {
        const int n = objects.property(QStringLiteral("length")).toInt();
        for (int i = 0; i < n; ++i)
            add(objects.property(quint32(i)));
    } else {
        add(objects);
    }
    m_host.newSelection() = ids;
    m_host.markSelectionSet();
}

QJSValue ScriptApi::objects() const
{
    QList<NodeId> ids;
    const Page& page = m_host.editor().currentPage();
    for (int l = 0; l < page.childCount(); ++l)
        if (const auto* layer = node_cast<Layer>(page.child(l)))
            for (int i = 0; i < layer->childCount(); ++i)
                if (const auto* o = node_cast<DrawObject>(layer->child(i)))
                    ids << o->id();
    QJSValue array = m_host.engine().newArray(quint32(ids.size()));
    for (int i = 0; i < ids.size(); ++i)
        array.setProperty(quint32(i), m_host.wrap(ids[i]));
    return array;
}

QJSValue ScriptApi::find(int id) const
{
    return m_host.editor().document().findAs<DrawObject>(NodeId(id)) ? m_host.wrap(NodeId(id)) : QJSValue(QJSValue::NullValue);
}

void ScriptApi::print(const QJSValue& a, const QJSValue& b, const QJSValue& c, const QJSValue& d)
{
    QStringList parts;
    for (const QJSValue& v : {a, b, c, d})
        if (!v.isUndefined())
            parts << v.toString();
    m_host.log(parts.join(QLatin1Char(' ')));
}

// --- ScriptHost -----------------------------------------------------------------------------

ScriptHost::ScriptHost(Editor& editor, QObject* parent) : QObject(parent), m_editor(editor) {}

ScriptHost::~ScriptHost() = default;

Transaction& ScriptHost::tx()
{
    return *m_tx;
}

QJSValue ScriptHost::wrap(NodeId id)
{
    if (id == InvalidNodeId)
        return QJSValue(QJSValue::NullValue);
    return m_engine->newQObject(new ScriptObject(*this, id)); // gehört der Engine
}

NodeId ScriptHost::insert(std::unique_ptr<DrawObject> object)
{
    // Neue Objekte bekommen die Vorgaben des Editors (Füllung, Kontur) wie beim Zeichnen
    Style s = m_editor.defaultStyle();
    if (const auto* c = node_cast<CurveObject>(object.get()); c && !c->subPaths().first().closed)
        s.fill = Fill::none();
    if (node_cast<TextObject>(object.get())) {
        s.fill = Fill::solid(Color(0, 0, 0));
        s.stroke.reset();
    }
    object->setStyle(s);
    const NodeId id = m_tx->insert(m_editor.activeLayer(), -1, std::move(object));
    if (!m_selectionSet)
        m_newSelection = {id};
    return id;
}

ScriptHost::Result ScriptHost::run(const QString& code, const QString& name, int timeoutMs)
{
    Result result;
    m_output.clear();
    m_newSelection.clear();
    m_selectionSet = false;
    m_engine = std::make_unique<QJSEngine>();
    QJSValue api = m_engine->newQObject(new ScriptApi(*this));
    m_engine->globalObject().setProperty(QStringLiteral("tg"), api);
    // console.log wie im Browser, landet in der Ausgabe
    m_engine->evaluate(QStringLiteral("var console = { log: function() { tg.print(Array.prototype.join.call(arguments, ' ')); },"
                                      " warn: function() { tg.print('Warnung: ' + Array.prototype.join.call(arguments, ' ')); },"
                                      " error: function() { tg.print('Fehler: ' + Array.prototype.join.call(arguments, ' ')); } };"));

    const QString label = name.isEmpty() ? tr("Skript") : tr("Skript: %1").arg(name);
    m_tx = std::make_unique<Transaction>(m_editor.document(), label);

    // Zeitgrenze gegen Endlosschleifen: ein Wächter unterbricht die Engine
    if (timeoutMs < 0)
        timeoutMs = int(Options::instance().number(QStringLiteral("ws/scripts/timeout")) * 1000.0);
    std::mutex mutex;
    std::condition_variable cv;
    bool done = false;
    bool interrupted = false;
    QJSEngine* engine = m_engine.get();
    std::thread watchdog([&] {
        std::unique_lock lock(mutex);
        if (timeoutMs > 0 && !cv.wait_for(lock, std::chrono::milliseconds(timeoutMs), [&] { return done; })) {
            interrupted = true;
            engine->setInterrupted(true);
        }
    });
    QStringList stack;
    const QJSValue value = m_engine->evaluate(code, name.isEmpty() ? QStringLiteral("skript.js") : name, 1, &stack);
    {
        std::lock_guard lock(mutex);
        done = true;
    }
    cv.notify_all();
    watchdog.join();

    if (interrupted) {
        result.error = tr("Abgebrochen: Das Skript lief länger als %1 s.").arg(timeoutMs / 1000.0);
    } else if (value.isError()) {
        result.error = tr("Zeile %1: %2").arg(value.property(QStringLiteral("lineNumber")).toInt()).arg(value.toString());
    } else if (m_engine->hasError()) {
        result.error = m_engine->catchError().toString();
    }
    result.ok = result.error.isEmpty();
    if (result.ok) {
        result.value = value;
        const bool changed = !m_tx->isEmpty();
        m_tx->commit();
        m_tx.reset();
        if (changed || m_selectionSet) {
            QList<NodeId> sel;
            for (NodeId id : std::as_const(m_newSelection))
                if (m_editor.document().find(id))
                    sel << id;
            if (!sel.isEmpty() || m_selectionSet)
                m_editor.setSelection(sel);
        }
    } else {
        // Fehler: alles zurück, das Dokument bleibt wie vorher
        m_tx->rollback();
        m_tx.reset();
    }
    result.output = m_output;
    for (const QString& line : std::as_const(m_output))
        emit output(line);
    // Die Engine bleibt bis zum nächsten Lauf bestehen, damit result.value gültig ist
    return result;
}

} // namespace vek
