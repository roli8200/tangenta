#include "CurveObject.h"
#include "DocumentIO.h"
#include "EllipseObject.h"
#include "GroupObject.h"
#include "BlurBake.h"
#include "Hyphenator.h"
#include "ImageAdjust.h"
#include "ImageObject.h"
#include "ImagePool.h"
#include "Outline.h"
#include "PathOps.h"
#include "PolygonObject.h"
#include "RawImage.h"
#include "RectangleObject.h"
#include "AutoCorrect.h"
#include "Align.h"
#include "BitmapFilters.h"
#include "Imposition.h"
#include <QPainter>
#include <QTemporaryDir>
#include "SpellChecker.h"
#include "Serialization.h"
#include "TextObject.h"
#include "Transaction.h"
#include "Vectorizer.h"

#include <QPainter>
#include <QImageWriter>
#include <QBuffer>
#include <QFile>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QTextDocument>
#include <QtTest>
#include <QSet>

using namespace vek;

namespace {

NodeId firstLayer(const Document& doc, int pageIndex)
{
    return doc.page(pageIndex).child(0)->id();
}

std::unique_ptr<RectangleObject> makeRect(double x, double y, double w, double h)
{
    auto r = std::make_unique<RectangleObject>(QSizeF(w, h));
    r->setTransform(QTransform::fromTranslate(x, y));
    return r;
}

QJsonObject snapshot(const Document& doc)
{
    return serial::toJson(doc.root());
}

bool fuzzyRect(const QRectF& a, const QRectF& b)
{
    return qAbs(a.left() - b.left()) < 1e-9 && qAbs(a.top() - b.top()) < 1e-9 && qAbs(a.width() - b.width()) < 1e-9
           && qAbs(a.height() - b.height()) < 1e-9;
}

} // namespace

class TestCore : public QObject {
    Q_OBJECT

private slots:
    void newDocumentHasMasterAndOnePage()
    {
        Document doc;
        QVERIFY(doc.masterPage().master());
        QCOMPARE(doc.pageCount(), 1);
        QCOMPARE(doc.page(0).childCount(), 1);
        QVERIFY(node_cast<Layer>(doc.page(0).child(0)));
        QVERIFY(node_cast<Layer>(doc.masterPage().child(0)));
        QCOMPARE(doc.page(0).size(), Page::A4);
        QVERIFY(!doc.isModified());
        QVERIFY(!doc.canUndo());
    }

    void insertUndoRedoKeepsId()
    {
        Document doc;
        const QJsonObject empty = snapshot(doc);
        NodeId id;
        {
            Transaction tx(doc, QStringLiteral("Rechteck"));
            id = tx.insert(firstLayer(doc, 0), -1, makeRect(10, 20, 30, 40));
            tx.commit();
        }
        QVERIFY(doc.findAs<RectangleObject>(id));
        QVERIFY(doc.isModified());
        QCOMPARE(doc.undoText(), QStringLiteral("Rechteck"));

        doc.undo();
        QVERIFY(!doc.find(id));
        QCOMPARE(snapshot(doc), empty);
        QVERIFY(!doc.isModified());

        doc.redo();
        const auto* r = doc.findAs<RectangleObject>(id);
        QVERIFY(r);
        QCOMPARE(r->boundingRect(), QRectF(10, 20, 30, 40));
    }

    void modifyIsUndoable()
    {
        Document doc;
        NodeId id;
        {
            Transaction tx(doc, QStringLiteral("Rechteck"));
            id = tx.insert(firstLayer(doc, 0), -1, makeRect(0, 0, 10, 10));
            tx.commit();
        }
        const QJsonObject before = snapshot(doc);
        {
            Transaction tx(doc, QStringLiteral("Farbe"));
            tx.modify<DrawObject>(id, [](DrawObject& o) {
                Style s = o.style();
                s.fill = Fill::solid(Color(255, 0, 0));
                o.setStyle(s);
            });
            tx.modify<DrawObject>(id, [](DrawObject& o) { o.rotate(45, QPointF(5, 5)); });
            tx.commit();
        }
        const QJsonObject after = snapshot(doc);
        QVERIFY(before != after);
        doc.undo();
        QCOMPARE(snapshot(doc), before);
        doc.redo();
        QCOMPARE(snapshot(doc), after);
    }

    void unchangedModifyRecordsNothing()
    {
        Document doc;
        Transaction tx(doc, QStringLiteral("nichts"));
        tx.modify(firstLayer(doc, 0), [](Node&) {});
        QVERIFY(tx.isEmpty());
        tx.commit();
        QVERIFY(!doc.canUndo());
    }

    void destructorRollsBack()
    {
        Document doc;
        const QJsonObject before = snapshot(doc);
        {
            Transaction tx(doc, QStringLiteral("abgebrochen"));
            tx.insert(firstLayer(doc, 0), -1, makeRect(0, 0, 1, 1));
            tx.remove(firstLayer(doc, 0));
        }
        QCOMPARE(snapshot(doc), before);
        QVERIFY(!doc.canUndo());
    }

    void structureRulesAreEnforced()
    {
        Document doc;
        Transaction tx(doc, QStringLiteral("falsch"));
        // Zeichenobjekt direkt in eine Seite: nicht erlaubt
        QCOMPARE(tx.insert(doc.page(0).id(), -1, makeRect(0, 0, 1, 1)), InvalidNodeId);
        // Seite in eine Ebene: nicht erlaubt
        QCOMPARE(tx.insert(firstLayer(doc, 0), -1, createPage(Page::A4, QStringLiteral("x"))), InvalidNodeId);
        tx.commit();
    }

    void pagesAndMasterTree()
    {
        Document doc;
        NodeId masterObj;
        {
            Transaction tx(doc, QStringLiteral("Seiten"));
            tx.insert(doc.root().id(), -1, createPage(QSizeF(100, 50), QStringLiteral("Ebene 1")));
            masterObj = tx.insert(doc.masterPage().child(0)->id(), -1, makeRect(1, 1, 5, 5));
            tx.commit();
        }
        QCOMPARE(doc.pageCount(), 2);
        QCOMPARE(doc.page(1).size(), QSizeF(100, 50));
        QCOMPARE(doc.pageOf(masterObj), &doc.masterPage());
        QCOMPARE(doc.pageIndex(doc.page(1).id()), 1);
        QCOMPARE(doc.pageIndex(doc.masterPage().id()), -1);

        // Objekt von der Masterseite auf Seite 2 verschieben und zurück per Undo
        {
            Transaction tx(doc, QStringLiteral("Verschieben"));
            tx.move(masterObj, firstLayer(doc, 1), -1);
            tx.commit();
        }
        QCOMPARE(doc.pageOf(masterObj), &doc.page(1));
        doc.undo();
        QCOMPARE(doc.pageOf(masterObj), &doc.masterPage());
        doc.undo();
        QCOMPARE(doc.pageCount(), 1);
    }

    void scaleKeepsAnchor()
    {
        RectangleObject r(QSizeF(20, 10));
        r.setTransform(QTransform::fromTranslate(10, 10));
        // breiter ziehen am rechten Rand: linker Rand bleibt
        r.scale(1.5, 1.0, QPointF(10, 15));
        QVERIFY(fuzzyRect(r.boundingRect(), QRectF(10, 10, 30, 10)));
        // proportional verkleinern um die obere linke Ecke
        r.scaleUniform(0.5, QPointF(10, 10));
        QVERIFY(fuzzyRect(r.boundingRect(), QRectF(10, 10, 15, 5)));
        // Die Skalierung wandert in Breite und Höhe, die Matrix bleibt ohne Skalierung
        QVERIFY(qAbs(r.size().width() - 15) < 1e-9 && qAbs(r.size().height() - 5) < 1e-9);
        QVERIFY(qFuzzyCompare(r.transform().m11(), 1.0) && qFuzzyCompare(r.transform().m22(), 1.0));
    }

    void rotatedRectangleStaysRectangle()
    {
        RectangleObject r(QSizeF(20, 10));
        r.setCornerRadius(RectangleObject::TopLeft, 2.0);
        r.rotate(30, QPointF(10, 5));
        r.scale(2.0, 1.0, QPointF(10, 5)); // achsparallel strecken: wird geneigt, bleibt in der Matrix
        const QTransform skewed = r.transform();
        QVERIFY(qAbs(QPointF::dotProduct(QPointF(skewed.m11(), skewed.m12()), QPointF(skewed.m21(), skewed.m22())))
                > 1e-6);
        QCOMPARE(r.size(), QSizeF(20, 10));

        RectangleObject s(QSizeF(20, 10));
        s.rotate(30, QPointF(10, 5));
        s.scaleUniform(2.0, QPointF(10, 5)); // proportional: bleibt rechtwinklig, Grösse übernimmt
        QVERIFY(qAbs(s.size().width() - 40) < 1e-9 && qAbs(s.size().height() - 20) < 1e-9);
        QVERIFY(qAbs(s.rotationAngle() - 30) < 1e-9);
    }

    void cornerRadii()
    {
        RectangleObject r(QSizeF(20, 10));
        QVERIFY(r.cornersLinked());
        QVERIFY(r.setPropertyValue(QStringLiteral("radiusTopLeft"), 3.0));
        for (int c = 0; c < 4; ++c)
            QCOMPARE(r.cornerRadius(static_cast<RectangleObject::Corner>(c)), 3.0);
        // Ecke abgerundet: Eckpunkt liegt nicht mehr in der Fläche
        QVERIFY(!r.localPath().contains(QPointF(0.2, 0.2)));
        QVERIFY(r.localPath().contains(QPointF(3, 3)));
        // begrenzt auf die halbe kürzere Seite
        r.setPropertyValue(QStringLiteral("radiusTopLeft"), 99.0);
        QCOMPARE(r.cornerRadius(RectangleObject::TopLeft), 5.0);

        r.setPropertyValue(QStringLiteral("cornersLinked"), false);
        r.setPropertyValue(QStringLiteral("radiusBottomRight"), 1.0);
        QCOMPARE(r.cornerRadius(RectangleObject::TopLeft), 5.0);
        QCOMPARE(r.cornerRadius(RectangleObject::BottomRight), 1.0);

        for (int type = 0; type < 3; ++type) {
            r.setPropertyValue(QStringLiteral("cornerType"), type);
            const QRectF b = r.localPath().boundingRect();
            QVERIFY2(qAbs(b.width() - 20) < 1e-6 && qAbs(b.height() - 10) < 1e-6, qPrintable(QString::number(type)));
        }

        // Rundreise über Eigenschaften
        RectangleObject copy;
        copy.readProperties(serial::properties(r));
        QCOMPARE(serial::properties(copy), serial::properties(r));
    }

    void guidesAreUndoableAndSaved()
    {
        Document doc;
        {
            Transaction tx(doc, QStringLiteral("Hilfslinie"));
            tx.modify<Page>(doc.masterPage().id(), [](Page& p) { p.guides().append(Guide::vertical(42.5, 148.5)); });
            tx.commit();
        }
        QCOMPARE(doc.masterPage().guides().size(), 1);
        auto loaded = io::loadFromBytes(io::saveToBytes(doc));
        QCOMPARE(loaded->masterPage().guides().value(0), Guide::vertical(42.5, 148.5));
        doc.undo();
        QVERIFY(doc.masterPage().guides().isEmpty());
    }

    void resizeToTarget()
    {
        RectangleObject r(QSizeF(20, 10));
        r.resizeTo(QRectF(5, 5, 40, 40));
        QVERIFY(fuzzyRect(r.boundingRect(), QRectF(5, 5, 40, 40)));
    }

    void rotateAroundCenter()
    {
        RectangleObject r(QSizeF(20, 10));
        r.rotate(90, QPointF(10, 5));
        QVERIFY(fuzzyRect(r.boundingRect(), QRectF(5, -5, 10, 20)));
        r.rotate(-90, QPointF(10, 5));
        QVERIFY(fuzzyRect(r.boundingRect(), QRectF(0, 0, 20, 10)));
    }

    void skewMovesOppositeEdge()
    {
        RectangleObject r(QSizeF(10, 10));
        // horizontal neigen um die Unterkante: Unterkante bleibt, Oberkante wandert
        r.skew(45, 0, QPointF(5, 10));
        const QPolygonF poly = r.path().toFillPolygon();
        QVERIFY(poly.contains(QPointF(0, 10)));
        QVERIFY(poly.contains(QPointF(-10, 0)));
    }

    void groupUngroupAndTransform()
    {
        Document doc;
        const NodeId layer = firstLayer(doc, 0);
        NodeId a, b, group;
        {
            Transaction tx(doc, QStringLiteral("Rechtecke"));
            a = tx.insert(layer, -1, makeRect(0, 0, 10, 10));
            b = tx.insert(layer, -1, makeRect(20, 0, 10, 10));
            tx.commit();
        }
        {
            Transaction tx(doc, QStringLiteral("Gruppieren"));
            group = tx.insert(layer, -1, std::make_unique<GroupObject>());
            tx.move(a, group, -1);
            tx.move(b, group, -1);
            tx.commit();
        }
        const auto* g = doc.findAs<GroupObject>(group);
        QCOMPARE(g->childCount(), 2);
        QCOMPARE(g->boundingRect(), QRectF(0, 0, 30, 10));
        {
            Transaction tx(doc, QStringLiteral("Verschieben"));
            tx.modifySubtree<DrawObject>(group, [](DrawObject& o) { o.move(QPointF(5, 5)); });
            tx.commit();
        }
        QCOMPARE(doc.findAs<DrawObject>(a)->boundingRect(), QRectF(5, 5, 10, 10));
        doc.undo();
        QCOMPARE(doc.findAs<DrawObject>(a)->boundingRect(), QRectF(0, 0, 10, 10));
        doc.undo();
        QVERIFY(!doc.find(group));
        QCOMPARE(doc.find(a)->parent()->id(), layer);
    }

    void gradientFillRoundTripAndGeometry()
    {
        Fill f = Fill::gradient(Fill::Type::Linear, Color(255, 0, 0), Color(0, 0, 255));
        f.stops.insert(1, GradientStop{0.3, Color(0, 255, 0)});
        const QSizeF size(40, 20);
        GradientGeometry g;
        g.angle = 30;
        g.edgePad = 10;
        f.setGeometry(g, size);
        const GradientGeometry back = f.geometry(size);
        QVERIFY(qAbs(back.angle - 30) < 1e-9 && qAbs(back.edgePad - 10) < 1e-9);

        Fill radial = Fill::gradient(Fill::Type::Radial, Color(0, 0, 0), Color(255, 255, 255));
        GradientGeometry rg;
        rg.offsetX = 20;
        rg.offsetY = -40;
        rg.edgePad = 5;
        radial.setGeometry(rg, size);
        const GradientGeometry rback = radial.geometry(size);
        QVERIFY(qAbs(rback.offsetX - 20) < 1e-9 && qAbs(rback.offsetY + 40) < 1e-9 && qAbs(rback.edgePad - 5) < 1e-9);

        // Speichern und Laden über den Stil
        Style s;
        s.fill = f;
        QCOMPARE(Style::fromJson(s.toJson()).fill, f);
        // alte Kurzform: Farbe als Text
        QCOMPARE(Fill::fromJson(QJsonValue(QStringLiteral("#ff0000"))), Fill::solid(Color(255, 0, 0)));
        QVERIFY(Fill::fromJson(QJsonValue(QJsonValue::Null)).isNone());
        // Pinsel: Verlauf liegt in lokalen Koordinaten des Begrenzungsrechtecks
        const QBrush b = f.brush(QRectF(10, 10, 40, 20));
        QVERIFY(b.gradient() && b.gradient()->type() == QGradient::LinearGradient);
        QCOMPARE(b.gradient()->stops().size(), 3);
    }

    void guideGeometry()
    {
        const Guide v = Guide::vertical(30, 100);
        QVERIFY(v.isVertical() && !v.isHorizontal());
        QVERIFY(qAbs(qAbs(v.distance(QPointF(35, 0))) - 5) < 1e-9);
        Guide d{QPointF(0, 0), 45};
        // 45° gegen den Uhrzeigersinn: Linie steigt nach rechts oben (y nach unten negativ)
        QVERIFY(qAbs(d.distance(QPointF(10, -10))) < 1e-9);
        const QPointF p = d.project(QPointF(10, 0));
        QVERIFY(qAbs(p.x() - 5) < 1e-9 && qAbs(p.y() + 5) < 1e-9);
    }

    void colorHexRoundTrip()
    {
        QCOMPARE(Color(255, 128, 0).toHex(), QStringLiteral("#ff8000"));
        QCOMPARE(Color(1, 2, 3, 4).toHex(), QStringLiteral("#01020304"));
        QCOMPARE(Color::fromHex(QStringLiteral("#ff8000")), std::optional<Color>(Color(255, 128, 0)));
        QCOMPARE(Color::fromHex(QStringLiteral("#01020304")), std::optional<Color>(Color(1, 2, 3, 4)));
        QVERIFY(!Color::fromHex(QStringLiteral("#zz0000")));
        QVERIFY(!Color::fromHex(QStringLiteral("ff0000")));
    }

    void fileRoundTrip()
    {
        Document doc;
        {
            Transaction tx(doc, QStringLiteral("Inhalt"));
            auto r = makeRect(10, 10, 30, 20);
            Style s;
            s.fill = Fill::solid(Color(10, 20, 30, 128));
            s.stroke.reset();
            r->setStyle(s);
            r->rotate(30, QPointF(25, 20));
            tx.insert(firstLayer(doc, 0), -1, std::move(r));
            tx.insert(doc.masterPage().child(0)->id(), -1, makeRect(0, 0, 210, 5));
            tx.insert(doc.root().id(), -1, createPage(QSizeF(148, 210), QStringLiteral("Ebene 1")));
            tx.commit();
        }
        QString error;
        const QByteArray bytes = io::saveToBytes(doc, &error);
        QVERIFY2(!bytes.isEmpty(), qPrintable(error));
        auto loaded = io::loadFromBytes(bytes, &error);
        QVERIFY2(loaded, qPrintable(error));
        QCOMPARE(snapshot(*loaded), snapshot(doc));
        QVERIFY(!loaded->isModified());

        // Ids laufen nach dem Laden weiter, ohne Kollision
        Transaction tx(*loaded, QStringLiteral("neu"));
        const NodeId fresh = tx.insert(firstLayer(*loaded, 0), -1, makeRect(0, 0, 1, 1));
        tx.commit();
        QVERIFY(!doc.find(fresh));
    }

    void fileRoundTripOnDisk()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("test.vek"));
        Document doc;
        QString error;
        QVERIFY2(io::save(doc, path, &error), qPrintable(error));
        auto loaded = io::load(path, &error);
        QVERIFY2(loaded, qPrintable(error));
        QCOMPARE(snapshot(*loaded), snapshot(doc));
    }

    void rejectsForeignAndBrokenFiles()
    {
        QString error;
        QVERIFY(!io::loadFromBytes(QByteArray("kein zip"), &error));
        QVERIFY(!error.isEmpty());
    }

    // --- Kurven -------------------------------------------------------------------------------

    void curveFromRectangleIsClosed()
    {
        RectangleObject r(QSizeF(40, 20));
        auto c = CurveObject::fromPath(r.localPath());
        QCOMPARE(c->subPaths().size(), 1);
        QVERIFY(c->subPaths().first().closed);
        QCOMPARE(c->nodeCount(), 4);
        QCOMPARE(c->localPath().boundingRect(), QRectF(0, 0, 40, 20));
        // abgerundete Ecken werden zu Kurvensegmenten
        r.setCornerRadius(RectangleObject::TopLeft, 5);
        auto rounded = CurveObject::fromPath(r.localPath());
        QVERIFY(rounded->nodeCount() > 4);
        QVERIFY(std::any_of(rounded->subPaths().first().nodes.begin(), rounded->subPaths().first().nodes.end(),
                            [](const CurveNode& n) { return n.curveIn; }));
    }

    void curveInsertKeepsShape()
    {
        CurveObject c({SubPath{{CurveNode{QPointF(0, 0)}, CurveNode{QPointF(30, 0), true, QPointF(10, 20), QPointF(20, 20)}}, false}});
        const QPainterPath before = c.localPath();
        const NodeRef added = c.insertNode(NodeRef{0, 1}, 0.3);
        QCOMPARE(added, (NodeRef{0, 1}));
        QCOMPARE(c.nodeCount(), 3);
        // Beide Hälften ergeben dieselbe Kurve
        for (double t : {0.1, 0.25, 0.5, 0.8}) {
            const QPointF a = before.pointAtPercent(t);
            double d1 = 0;
            double d2 = 0;
            c.nearestOnSegment(NodeRef{0, 1}, a, &d1);
            c.nearestOnSegment(NodeRef{0, 2}, a, &d2);
            QVERIFY2(std::min(d1, d2) < 1e-3, qPrintable(QString::number(std::min(d1, d2))));
        }
        // Linie: neuer Knoten liegt auf der Strecke
        CurveObject line({SubPath{{CurveNode{QPointF(0, 0)}, CurveNode{QPointF(10, 0)}}, false}});
        line.insertNode(NodeRef{0, 1}, 0.5);
        QCOMPARE(line.node(NodeRef{0, 1}).point, QPointF(5, 0));
    }

    void curveDeleteAndClose()
    {
        CurveObject c({SubPath{{CurveNode{QPointF(0, 0)}, CurveNode{QPointF(10, 0)}, CurveNode{QPointF(10, 10)},
                                CurveNode{QPointF(0, 10)}}, false}});
        c.setClosed(0, true);
        QVERIFY(c.hasSegmentInto(NodeRef{0, 0}));
        c.deleteNodes({NodeRef{0, 1}, NodeRef{0, 2}});
        QCOMPARE(c.nodeCount(), 2);
        QCOMPARE(c.node(NodeRef{0, 1}).point, QPointF(0, 10));
        c.deleteNodes({NodeRef{0, 0}});
        QVERIFY(c.subPaths().isEmpty()); // ein einzelner Knoten ist kein Linienzug
    }

    void curveToCurveAndKinds()
    {
        CurveObject c({SubPath{{CurveNode{QPointF(0, 0)}, CurveNode{QPointF(30, 0)}, CurveNode{QPointF(30, 30)}}, false}});
        c.setSegmentCurve(NodeRef{0, 1}, true);
        QCOMPARE(c.node(NodeRef{0, 1}).c1, QPointF(10, 0));
        QCOMPARE(c.node(NodeRef{0, 1}).c2, QPointF(20, 0));
        c.setSegmentCurve(NodeRef{0, 2}, true);
        // Glatt: beide Steuerpunkte am mittleren Knoten auf einer Geraden durch ihn
        c.moveControl(NodeRef{0, 1}, true, QPointF(20, -5));
        c.setKind(NodeRef{0, 1}, NodeKind::Smooth);
        const QPointF p = c.node(NodeRef{0, 1}).point;
        const QPointF in = c.node(NodeRef{0, 1}).c2 - p;
        const QPointF out = c.node(NodeRef{0, 2}).c1 - p;
        QVERIFY(qAbs(in.x() * out.y() - in.y() * out.x()) < 1e-9);
        QVERIFY(QPointF::dotProduct(in, out) < 0);
        // Symmetrisch: Ziehen eines Steuerpunkts spiegelt den anderen
        c.setKind(NodeRef{0, 1}, NodeKind::Symmetric);
        c.moveControl(NodeRef{0, 1}, false, QPointF(36, 8));
        QCOMPARE(c.node(NodeRef{0, 1}).c2, QPointF(24, -8));
        // In Linie
        c.setSegmentCurve(NodeRef{0, 2}, false);
        QVERIFY(!c.node(NodeRef{0, 2}).curveIn);
    }

    void curveBendMovesPointExactly()
    {
        CurveObject c({SubPath{{CurveNode{QPointF(0, 0)}, CurveNode{QPointF(30, 0), true, QPointF(10, 0), QPointF(20, 0)}}, false}});
        for (double t : {0.3, 0.5, 0.7}) {
            CurveObject k(c.subPaths());
            const QPointF before = k.pointOnSegment(NodeRef{0, 1}, t);
            k.bendSegment(NodeRef{0, 1}, t, QPointF(0, 10));
            const QPointF after = k.pointOnSegment(NodeRef{0, 1}, t);
            QVERIFY2(QLineF(after, before + QPointF(0, 10)).length() < 1e-9, qPrintable(QString::number(t)));
        }
    }

    void curveReverseTwiceIsIdentity()
    {
        CurveObject c({SubPath{{CurveNode{QPointF(0, 0)}, CurveNode{QPointF(30, 0), true, QPointF(10, 5), QPointF(20, 5)},
                                CurveNode{QPointF(30, 30)}}, true}});
        const QList<SubPath> original = c.subPaths();
        c.reverse(0);
        QVERIFY(c.subPaths() != original);
        QCOMPARE(c.localPath().boundingRect(), CurveObject(original).localPath().boundingRect());
        c.reverse(0);
        QCOMPARE(c.subPaths(), original);
    }

    void curveSerializesWithBlur()
    {
        Document doc;
        Transaction tx(doc, QStringLiteral("x"));
        auto c = std::make_unique<CurveObject>(QList<SubPath>{
            SubPath{{CurveNode{QPointF(1, 2)}, CurveNode{QPointF(5, 6), true, QPointF(2, 3), QPointF(4, 5), NodeKind::Smooth}}, true}});
        c->setBlur(1.5);
        const NodeId id = tx.insert(firstLayer(doc, 0), -1, std::move(c));
        tx.commit();
        QString error;
        std::unique_ptr<Document> loaded = io::loadFromBytes(io::saveToBytes(doc, &error), &error);
        QVERIFY2(loaded, qPrintable(error));
        const auto* back = loaded->findAs<CurveObject>(id);
        QVERIFY(back);
        QCOMPARE(back->subPaths(), doc.findAs<CurveObject>(id)->subPaths());
        QCOMPARE(back->blur(), 1.5);
    }

    void fitKeepsCornersAndSmoothsArcs()
    {
        // Rechteck aus vielen Punkten: vier Ecken, vier gerade Segmente
        QList<QPointF> rect;
        for (int i = 0; i < 40; ++i)
            rect << QPointF(i, 0);
        for (int i = 0; i < 20; ++i)
            rect << QPointF(40, i);
        for (int i = 40; i > 0; --i)
            rect << QPointF(i, 20);
        for (int i = 20; i > 0; --i)
            rect << QPointF(0, i);
        const SubPath r = pathops::fitPolyline(rect, true, 0.05);
        QVERIFY(r.closed);
        QCOMPARE(r.nodes.size(), 4);
        for (const CurveNode& n : r.nodes)
            QVERIFY(!n.curveIn);

        // Kreis aus 200 Punkten: wenige glatte Kurvenknoten, eng an der Form
        QList<QPointF> circle;
        for (int i = 0; i < 200; ++i)
            circle << QPointF(50 + 10 * std::cos(2 * M_PI * i / 200), 50 + 10 * std::sin(2 * M_PI * i / 200));
        const SubPath c = pathops::fitPolyline(circle, true, 0.02);
        QVERIFY2(c.nodes.size() >= 2 && c.nodes.size() <= 12, qPrintable(QString::number(c.nodes.size())));
        const QPainterPath p = CurveObject(QList<SubPath>{c}).localPath();
        for (int i = 0; i < 100; ++i) {
            const QPointF q = p.pointAtPercent(i / 100.0);
            QVERIFY(std::abs(std::hypot(q.x() - 50, q.y() - 50) - 10) < 0.05);
        }
    }

    void cutOperationsWorkInMillimetres()
    {
        // Kreis (Kurven) mit einem Rechteck schneiden: das Ergebnis folgt dem Bogen genau
        QPainterPath disc;
        disc.addEllipse(QPointF(0, 0), 5, 5);
        QPainterPath half;
        half.addRect(QRectF(-10, -10, 20, 10));
        const QPainterPath top = pathops::intersected(disc, half);
        QVERIFY(std::abs(top.boundingRect().height() - 5.0) < 0.01);
        QVERIFY(std::abs(top.boundingRect().width() - 10.0) < 0.01);
        // Linie an einer Fläche teilen
        const QList<QPolygonF> pieces = pathops::clipLines({QPolygonF{QPointF(-10, 0), QPointF(10, 0)}}, disc, false);
        QCOMPARE(pieces.size(), 2);
        QVERIFY(std::abs(pieces[0].last().x() + 5.0) < 0.01);
        QVERIFY(std::abs(pieces[1].first().x() - 5.0) < 0.01);
    }

    void artisticMediaFollowsSpineAndSaves()
    {
        Document doc;
        Transaction tx(doc, QStringLiteral("x"));
        auto c = std::make_unique<CurveObject>(QList<SubPath>{SubPath{{CurveNode{QPointF(10, 50)}, CurveNode{QPointF(110, 50)}}, false}});
        ArtisticMedia media;
        media.kind = ArtisticMedia::Kind::Preset;
        media.preset = 1; // gleichmässig mit runden Enden
        media.width = 8.0;
        c->setMedia(media);
        // Der Umriss ist der Strich: 8 mm hoch, mit runden Enden 108 mm lang
        const QRectF b = c->localPath().boundingRect();
        QVERIFY2(std::abs(b.height() - 8.0) < 0.1 && std::abs(b.width() - 100.0) < 0.1, qPrintable(QStringLiteral("%1 x %2").arg(b.width()).arg(b.height())));
        QCOMPARE(c->spinePath().boundingRect().height(), 0.0);
        const NodeId id = tx.insert(firstLayer(doc, 0), -1, std::move(c));
        tx.commit();
        QString error;
        std::unique_ptr<Document> loaded = io::loadFromBytes(io::saveToBytes(doc, &error), &error);
        QVERIFY2(loaded, qPrintable(error));
        QCOMPARE(loaded->findAs<CurveObject>(id)->media(), media);
    }

    void arrowheadsSitOnOpenEnds()
    {
        Style s;
        s.strokeWidth = 1.0;
        s.endArrow = 2; // Dreieck: 5 Einheiten lang
        QPainterPath line;
        line.moveTo(0, 0);
        line.lineTo(50, 0);
        const outline::Stroke stroke = outline::withArrows(line, s);
        // Spitze genau am Ende, Linie unter der Spitze gekürzt, Anfang unverändert
        QVERIFY(std::abs(stroke.heads.boundingRect().right() - 50.0) < 1e-6);
        QVERIFY(std::abs(stroke.heads.boundingRect().width() - 5.0) < 1e-6);
        QVERIFY(std::abs(stroke.line.elementAt(1).x - 45.5) < 1e-6);
        QCOMPARE(stroke.line.elementAt(0).x, 0.0);
        // Geschlossene Pfade bekommen keine Pfeile
        QPainterPath box;
        box.addRect(0, 0, 10, 10);
        QVERIFY(outline::withArrows(box, s).heads.isEmpty());
        // Speichern und Laden
        s.dash = 3;
        s.startArrow = 6;
        QCOMPARE(Style::fromJson(s.toJson()), s);
    }

    void ellipsesStayEllipses()
    {
        EllipseObject e(QSizeF(40, 20));
        QVERIFY(std::abs(e.localPath().boundingRect().width() - 40) < 1e-6);
        // Skalieren ändert Breite und Höhe, die Form bleibt eine Ellipse
        e.applyTransform(xform::scaling(0.5, 2.0, QPointF(0, 0)));
        QVERIFY(std::abs(e.size().width() - 20) < 1e-9 && std::abs(e.size().height() - 40) < 1e-9);
        QVERIFY(e.transform().isIdentity());
        // Kreissegment 0..90° gegen den Uhrzeigersinn: das Viertel oben rechts
        e.setKind(EllipseObject::Kind::Pie);
        e.setAngles(0, 90);
        const QRectF pie = e.localPath().boundingRect();
        QVERIFY2(std::abs(pie.left() - 10) < 1e-6 && std::abs(pie.top()) < 1e-6 && std::abs(pie.bottom() - 20) < 1e-6,
                 qPrintable(QStringLiteral("%1 %2 %3").arg(pie.left()).arg(pie.top()).arg(pie.bottom())));
        // Richtung ändern: die anderen drei Viertel
        e.setClockwise(true);
        QVERIFY(std::abs(e.localPath().boundingRect().left()) < 1e-6);
        // Bogen ist offen
        e.setKind(EllipseObject::Kind::Arc);
        QVERIFY(!CurveObject::fromPath(e.localPath())->subPaths().first().closed);
        // Speichern und Laden
        auto json = serial::toJson(e);
        auto back = serial::fromJson(json, serial::Ids::Keep);
        const auto* loaded = node_cast<EllipseObject>(back.get());
        QVERIFY(loaded && loaded->kind() == EllipseObject::Kind::Arc && loaded->clockwise());
    }

    void polygonsAndStars()
    {
        using K = PolygonObject::Kind;
        auto corners = [](const PolygonObject& p) {
            int n = 0;
            for (const SubPath& s : CurveObject::fromPath(p.localPath())->subPaths())
                n += int(s.nodes.size());
            return n;
        };
        QCOMPARE(corners(PolygonObject(QSizeF(50, 50), K::Polygon, 6, 0)), 6);
        QCOMPARE(corners(PolygonObject(QSizeF(50, 50), K::Star, 5, 53)), 10);
        // Erste Ecke zeigt nach oben
        const QRectF b = PolygonObject(QSizeF(50, 50), K::Polygon, 5, 0).localPath().boundingRect();
        QVERIFY(std::abs(b.top()) < 1e-9);
        // Komplexer Stern {9/3}: neun Ecken, ein Linienzug
        const PolygonObject complex(QSizeF(50, 50), K::ComplexStar, 9, 2);
        QCOMPARE(corners(complex), 9);
        // {6/2}: zwei Dreiecke (Davidstern)
        QCOMPARE(CurveObject::fromPath(PolygonObject(QSizeF(50, 50), K::ComplexStar, 6, 1).localPath())->subPaths().size(), 2);
        // Schärfe bleibt im gültigen Bereich
        QCOMPARE(PolygonObject(QSizeF(50, 50), K::ComplexStar, 9, 10).sharpness(), 3);
        QCOMPARE(PolygonObject(QSizeF(50, 50), K::Polygon, 1, 0).points(), 3);
        auto back = serial::fromJson(serial::toJson(complex), serial::Ids::Keep);
        const auto* loaded = node_cast<PolygonObject>(back.get());
        QVERIFY(loaded && loaded->kind() == K::ComplexStar && loaded->points() == 9 && loaded->sharpness() == 2);
    }

    void textObjectsLayOutAndRoundTrip()
    {
        // Grafiktext: kein Umbruch, Ausdehnung wächst mit dem Text
        TextObject title(TextObject::Kind::Artistic);
        title.setPlainText(QStringLiteral("Hier kommt mein Titeltext"), CharFormat{QStringLiteral("Arial"), 24.0});
        const QRectF one = title.frameRect();
        QVERIFY2(one.width() > 60 && one.height() > 6 && one.height() < 15, qPrintable(QStringLiteral("%1 x %2").arg(one.width()).arg(one.height())));
        QVERIFY(!title.localPath().isEmpty());
        QVERIFY(one.contains(title.localPath().boundingRect().adjusted(1, 1, -1, -1)));
        // Skalieren verzerrt den Grafiktext (bleibt in der Matrix)
        title.applyTransform(xform::scaling(2.0, 1.0, QPointF()));
        QVERIFY(std::abs(title.boundingRect().width() - 2 * one.width()) < 1e-6);
        QCOMPARE(title.frameRect(), one);

        // Mengentext bricht an der Rahmenbreite um; breiterer Rahmen = weniger Zeilen
        TextObject para(TextObject::Kind::Paragraph, QSizeF(40, 60));
        para.setPlainText(QStringLiteral("Stand 21.10.2026 abends, nach dem Löschen der 24 Snapshots und der "
                                         "53 internen Snapshots älter als 30 Tage."),
                          CharFormat{QStringLiteral("Arial"), 12.0});
        const double narrowHeight = para.localPath().boundingRect().height();
        para.applyTransform(xform::scaling(3.0, 1.0, QPointF()));
        QVERIFY(std::abs(para.frameSize().width() - 120) < 1e-6); // Skalierung wandert in den Rahmen
        QVERIFY(para.transform().isIdentity());
        QVERIFY(para.localPath().boundingRect().height() < narrowHeight * 0.7);
        QCOMPARE(para.boundingRect(), QRectF(0, 0, 120, 60));

        // Bereich mit eigener Farbe und fett, Speichern und Laden
        TextParagraph p;
        p.runs << TextRun{QStringLiteral("rot "), CharFormat{QStringLiteral("Arial"), 12.0, true, false, false, Color(255, 0, 0)}}
               << TextRun{QStringLiteral("schwarz"), CharFormat{QStringLiteral("Arial"), 12.0}};
        p.align = TextParagraph::Align::Center;
        p.tabs = {20.0, 45.5};
        para.setParagraphs({p, TextParagraph{}});
        QCOMPARE(para.pieces().size(), 2);
        auto back = serial::fromJson(serial::toJson(para), serial::Ids::Keep);
        const auto* loaded = node_cast<TextObject>(back.get());
        QVERIFY(loaded && loaded->kind() == TextObject::Kind::Paragraph);
        QCOMPARE(loaded->paragraphs(), para.paragraphs());
        QCOMPARE(loaded->frameSize(), para.frameSize());

        // Tabulator: Text nach dem Tab beginnt an der Tabulatorposition
        TextObject tabbed(TextObject::Kind::Paragraph, QSizeF(150, 30));
        TextParagraph tp;
        tp.runs << TextRun{QStringLiteral("Name	Wert"), CharFormat{QStringLiteral("Arial"), 12.0}};
        tp.tabs = {60.0};
        tabbed.setParagraphs({tp});
        QVERIFY2(tabbed.localPath().boundingRect().right() > 60.0,
                 qPrintable(QString::number(tabbed.localPath().boundingRect().right())));

        // Silbentrennung nach den deutschen Trennmustern
        const Hyphenator& de = Hyphenator::german();
        QCOMPARE(de.hyphenated(QStringLiteral("Silbentrennung")), QStringLiteral("Sil-ben-tren-nung"));
        QCOMPARE(de.hyphenated(QStringLiteral("Zeichenprogramm")), QStringLiteral("Zei-chen-pro-gramm"));
        QCOMPARE(de.hyphenated(QStringLiteral("ab")), QStringLiteral("ab"));
        // Im schmalen Rahmen trennt der Mengentext lange Wörter; gespeichert wird ohne Trennstriche
        TextObject narrow(TextObject::Kind::Paragraph, QSizeF(30, 60));
        narrow.setPlainText(QStringLiteral("Die Silbentrennung im Zeichenprogramm funktioniert"), CharFormat{QStringLiteral("Arial"), 12.0});
        const QRectF plain = narrow.localPath().boundingRect();
        narrow.setHyphenate(true);
        const QRectF hyphenated = narrow.localPath().boundingRect();
        QVERIFY2(hyphenated != plain, "Silbentrennung ändert den Umbruch");
        QVERIFY(!narrow.plainText().contains(QChar(QChar::SoftHyphen)));
        auto again = serial::fromJson(serial::toJson(narrow), serial::Ids::Keep);
        QVERIFY(node_cast<TextObject>(again.get())->hyphenate());

        // Arbeitskopie (QTextDocument) und zurück verliert nichts
        TextObject copy(TextObject::Kind::Paragraph, QSizeF(120, 60));
        copy.setFromDocument(*para.toDocument());
        QCOMPARE(copy.paragraphs(), para.paragraphs());
        QCOMPARE(copy.plainText(), QStringLiteral("rot schwarz\n"));
    }

    void rampsInterpolate()
    {
        const QRectF b(0, 0, 100, 50);
        Ramp r;
        QVERIFY(!r.isActive());
        QCOMPARE(Ramp::uniform(30).valueAt(QPointF(80, 10), b), 30.0);
        r.type = Ramp::Type::Linear;
        r.from = 10;
        r.to = 90;
        QCOMPARE(r.valueAt(QPointF(0, 25), b), 10.0);
        QCOMPARE(r.valueAt(QPointF(50, 0), b), 50.0);
        QCOMPARE(r.valueAt(QPointF(150, 25), b), 90.0); // ausserhalb: Endwert
        r.type = Ramp::Type::Radial;
        r.start = QPointF(0.5, 0.5);
        QCOMPARE(r.valueAt(QPointF(50, 25), b), 10.0);
        QCOMPARE(r.valueAt(QPointF(75, 25), b), 50.0);
        r.type = Ramp::Type::Conical;
        QCOMPARE(r.valueAt(QPointF(50, 50), b), 30.0); // Viertelkreis im Uhrzeigersinn
        QCOMPARE(Ramp::fromJson(r.toJson()), r);
        QCOMPARE(r.minimum(), 10.0);
        QCOMPARE(r.maximum(), 90.0);
    }

    void blurBakesToContours()
    {
        // Gausskante: halb am Rand, fast nichts weit aussen, fast alles weit innen
        QVERIFY(std::abs(bake::edgeAlpha(0, 2) - 0.5) < 1e-9);
        QVERIFY(bake::edgeAlpha(5, 2) < 0.01 && bake::edgeAlpha(-5, 2) > 0.99);

        QPainterPath rect;
        rect.addRect(QRectF(0, 0, 40, 20));
        const QList<bake::Part> parts{bake::Part{rect, Fill::solid(Color(0, 0, 0))}};
        auto group = bake::blurToContours(parts, Ramp::uniform(2.0), QTransform(), QRectF(0, 0, 40, 20));
        QVERIFY(group);
        QCOMPARE(group->childCount(), bake::kDefaultSteps);
        // Aussen gross und fast durchsichtig, innen klein und deckend
        const auto* outer = node_cast<DrawObject>(group->child(0));
        const auto* inner = node_cast<DrawObject>(group->child(group->childCount() - 1));
        QVERIFY2(std::abs(outer->boundingRect().left() + 5.0) < 0.1, qPrintable(QString::number(outer->boundingRect().left())));
        QVERIFY(outer->transparency().maximum() > 95);
        QVERIFY(!inner->transparency().isActive());
        QVERIFY(std::abs(inner->boundingRect().left() - 5.0) < 0.1);
        // Zusammen ergibt die Schichtung die Gausskante (Mitte jedes Rings)
        double covered = 0.0;
        for (int k = 0; k < group->childCount(); ++k)
            covered = 1.0 - (1.0 - covered) * (1.0 - (100.0 - node_cast<DrawObject>(group->child(k))->transparency().maximum()) / 100.0);
        QVERIFY(std::abs(covered - 1.0) < 1e-9);

        // Verlauf: Bänder, links scharf (eine Kontur), rechts weich (viele)
        Ramp ramp;
        ramp.type = Ramp::Type::Linear;
        ramp.from = 0.0;
        ramp.to = 3.0;
        auto banded = bake::blurToContours(parts, ramp, QTransform(), QRectF(0, 0, 40, 20));
        QVERIFY(banded && banded->childCount() > bake::kDefaultSteps * 2);
        double left = 1e9;
        double right = -1e9;
        for (int k = 0; k < banded->childCount(); ++k) {
            const QRectF b = node_cast<DrawObject>(banded->child(k))->boundingRect();
            left = std::min(left, b.left());
            right = std::max(right, b.right());
        }
        QVERIFY2(left > -1.0 && right > 45.0, qPrintable(QStringLiteral("%1 %2").arg(left).arg(right)));
    }

    void textChainsFlowThroughFrames()
    {
        const QString story = QStringLiteral(
            "Vorgaben vom 26.10.2026, umgesetzt im neuen Rechner. Der alte Rechner bleibt unberuehrt, damit "
            "er reproduzierbar bleibt. Zahlintervalle hoechstens 1, 3, 6, 12 und 24 Monate; 36 Monate "
            "entfallen. Laufzeitrabatt 2, 4, 15 und 20 Prozent, Gesamtdeckel 20 Prozent.");
        Document doc;
        auto a = std::make_unique<TextObject>(TextObject::Kind::Paragraph, QSizeF(50, 20));
        a->setPlainText(story, CharFormat{QStringLiteral("Arial"), 12.0});
        auto b = std::make_unique<TextObject>(TextObject::Kind::Paragraph, QSizeF(80, 40));
        b->setTransform(QTransform::fromTranslate(100, 0));
        NodeId ia = 0;
        NodeId ib = 0;
        {
            Transaction tx(doc, QStringLiteral("Text"));
            ia = tx.insert(firstLayer(doc, 0), -1, std::move(a));
            ib = tx.insert(firstLayer(doc, 0), -1, std::move(b));
            tx.commit();
        }
        // Allein: Text läuft über, sichtbar ist nur, was in den Rahmen passt
        const auto* ta = doc.findAs<TextObject>(ia);
        const auto* tb = doc.findAs<TextObject>(ib);
        QVERIFY(ta->overflows());
        QVERIFY2(ta->localPath().boundingRect().bottom() <= 20.5, qPrintable(QString::number(ta->localPath().boundingRect().bottom())));
        QVERIFY(tb->pieces().isEmpty());

        // Verknüpft: der Rest fliesst oben im zweiten, breiteren Rahmen weiter
        {
            Transaction tx(doc, QStringLiteral("Verknüpfen"));
            tx.modify<TextObject>(ia, [ib](TextObject& o) { o.setNextFrame(ib); });
            tx.commit();
        }
        ta = doc.findAs<TextObject>(ia);
        tb = doc.findAs<TextObject>(ib);
        QCOMPARE(ta->chain(), (QList<NodeId>{ia, ib}));
        QVERIFY(tb->isFollower() && tb->chainHead() == ia && !ta->isFollower());
        QVERIFY(!ta->overflows() && !tb->overflows());
        const QRectF inB = tb->localPath().boundingRect();
        QVERIFY2(inB.top() < 4 && inB.bottom() <= 40.5 && inB.right() > 50.5, qPrintable(QStringLiteral("%1 %2 %3").arg(inB.top()).arg(inB.bottom()).arg(inB.right())));

        // Jeder Rahmen hat seinen Teil, zusammen ergibt das den ganzen Text
        auto plain = [](const QList<TextParagraph>& paragraphs) {
            QStringList lines;
            for (const TextParagraph& p : paragraphs) {
                QString line;
                for (const TextRun& r : p.runs)
                    line += r.text;
                lines << line;
            }
            return lines.join(QLatin1Char('\n'));
        };
        const QList<QList<TextParagraph>> parts = ta->paragraphsByFrame();
        QCOMPARE(parts.size(), 2);
        QVERIFY(!plain(parts[0]).isEmpty() && !plain(parts[1]).isEmpty());
        QCOMPARE(plain(parts[0]) + plain(parts[1]), story);

        // Kleinerer Folgerahmen: wieder Überlauf; Rückgängig fliesst zurück
        {
            Transaction tx(doc, QStringLiteral("Grösse"));
            tx.modify<TextObject>(ib, [](TextObject& o) { o.setFrameSize(QSizeF(30, 8)); });
            tx.commit();
        }
        QVERIFY(doc.findAs<TextObject>(ia)->overflows());
        QVERIFY(doc.findAs<TextObject>(ib)->overflows());
        doc.undo();
        QVERIFY(!doc.findAs<TextObject>(ia)->overflows());

        // Speichern behält den Verweis, eine Kopie mit neuen Ids nicht
        ta = doc.findAs<TextObject>(ia);
        auto kept = serial::fromJson(serial::toJson(*ta), serial::Ids::Keep);
        QCOMPARE(node_cast<TextObject>(kept.get())->nextFrame(), ib);
        auto copy = serial::fromJson(serial::toJson(*ta, serial::Ids::Drop), serial::Ids::Drop);
        QCOMPARE(node_cast<TextObject>(copy.get())->nextFrame(), NodeId(0));

        // Verweis aufgehoben: beide wieder einzeln
        doc.undo();
        QVERIFY(doc.findAs<TextObject>(ia)->chain().isEmpty());
        QVERIFY(!doc.findAs<TextObject>(ib)->isFollower());
        QVERIFY(doc.findAs<TextObject>(ib)->pieces().isEmpty());
    }

    void imagesLiveInThePoolAndTheFile()
    {
        // Testbild 200 x 100 px mit 100 dpi, links rot, rechts blau
        QImage source(200, 100, QImage::Format_RGB32);
        source.fill(Qt::blue);
        for (int y = 0; y < 100; ++y)
            for (int x = 0; x < 100; ++x)
                source.setPixel(x, y, qRgb(255, 0, 0));
        source.setDotsPerMeterX(int(100 / 0.0254));
        source.setDotsPerMeterY(int(100 / 0.0254));
        const QString key = imagepool::addImage(source);
        QVERIFY(!key.isEmpty());
        QCOMPARE(imagepool::addImage(source), key); // gleicher Inhalt, gleicher Schlüssel
        QCOMPARE(imagepool::image(key).size(), QSize(200, 100));
        QVERIFY(imagepool::add(QByteArray("kein Bild")).isEmpty());

        // Rahmen anfangs = Bildrechteck; ein verschobener Knoten beschneidet
        ImageObject image(key, QSizeF(50.8, 25.4));
        QVERIFY(!image.isCropped());
        QCOMPARE(image.localPath().boundingRect(), QRectF(0, 0, 50.8, 25.4));
        QList<SubPath> frame = image.frame();
        frame[0].nodes[1].point = QPointF(30, 10);
        image.setFrame(frame);
        QVERIFY(image.isCropped());
        QVERIFY(!image.hitTest(QPointF(45, 3), 0.1, false)); // weggeschnittene Ecke
        QVERIFY(image.hitTest(QPointF(10, 20), 0.1, false));

        // Speichern und Laden: Bild liegt im Container unter images/
        Document doc;
        {
            Transaction tx(doc, QStringLiteral("Bild"));
            auto copy = std::make_unique<ImageObject>(key, image.size());
            copy->setFrame(image.frame());
            tx.insert(doc.page(0).child(0)->id(), -1, std::move(copy));
            tx.commit();
        }
        const QByteArray bytes = io::saveToBytes(doc);
        QVERIFY(bytes.contains("images/" + key.toLatin1() + ".png"));
        QString error;
        std::unique_ptr<Document> loaded = io::loadFromBytes(bytes, &error);
        QVERIFY2(loaded, qPrintable(error));
        const auto* back = node_cast<ImageObject>(loaded->page(0).child(0)->child(0));
        QVERIFY(back && back->key() == key && back->isCropped());
        QCOMPARE(back->image().pixelColor(10, 10), QColor(255, 0, 0));
    }

    void vectorizerTracesShapes()
    {
        // Weisser Grund, roter Kreis, blauer Ring (mit Loch zum Hintergrund)
        QImage source(240, 160, QImage::Format_RGB32);
        source.fill(Qt::white);
        {
            QPainter p(&source);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(220, 30, 30));
            p.drawEllipse(QPointF(60, 80), 45, 45);
            QPainterPath ring;
            ring.addRect(130, 30, 90, 100);
            ring.addRect(155, 55, 40, 50);
            p.setBrush(QColor(30, 60, 200));
            p.drawPath(ring);
        }
        trace::Settings settings = trace::presetSettings(trace::Preset::Logo);
        settings.colors = 3;
        settings.removeBackground = true;
        // Ohne "Farbe aus ganzem Bild entfernen" bleibt das weisse Loch im Ring
        const trace::Result partial = trace::run(source, settings);
        QCOMPARE(partial.colors, 3);
        settings.removeColorEverywhere = true;
        const trace::Result result = trace::run(source, settings);
        QCOMPARE(result.colors, 2);
        QCOMPARE(result.shapes.size(), 2);
        QVERIFY(result.nodes < 60); // wenige Knoten statt Pixeltreppen

        // Wieder rastern und mit dem Original vergleichen
        QImage back(source.size(), QImage::Format_RGB32);
        back.fill(Qt::white);
        {
            QPainter p(&back);
            p.setRenderHint(QPainter::Antialiasing);
            for (const trace::Shape& shape : result.shapes)
                p.fillPath(CurveObject::toPainterPath(shape.paths), shape.color);
        }
        int same = 0;
        for (int y = 0; y < source.height(); ++y)
            for (int x = 0; x < source.width(); ++x) {
                const QColor a = source.pixelColor(x, y), b = back.pixelColor(x, y);
                same += std::abs(a.red() - b.red()) + std::abs(a.green() - b.green()) + std::abs(a.blue() - b.blue()) < 90;
            }
        const double ratio = double(same) / (source.width() * source.height());
        QVERIFY2(ratio > 0.98, qPrintable(QString::number(ratio)));
        // Das Loch im Ring bleibt offen
        QCOMPARE(back.pixelColor(175, 80), QColor(Qt::white));

        // Strichgrafik: zwei Farben, Hintergrund weg
        const trace::Result lineArt = trace::run(source, trace::presetSettings(trace::Preset::LineArt));
        QCOMPARE(lineArt.colors, 1);
    }

    // Wie das Monitor-Symbol im PowerTRACE-Video: schwarz auf weiss, geglättete Kanten
    void vectorizerClassicOnLineArt()
    {
        QImage source(640, 640, QImage::Format_RGB32);
        source.fill(Qt::white);
        {
            QPainter p(&source);
            p.setRenderHint(QPainter::Antialiasing);
            QPainterPath monitor;
            monitor.addRoundedRect(QRectF(20.5, 40.3, 600, 460), 40, 40);
            monitor.addRect(QRectF(65.5, 90.3, 510, 305));
            monitor.addEllipse(QPointF(320.5, 440.3), 25, 25);
            monitor.addPolygon(QPolygonF({QPointF(270.5, 500.3), QPointF(370.5, 500.3), QPointF(385.5, 570.3), QPointF(255.5, 570.3)}));
            monitor.addRoundedRect(QRectF(170.5, 570.3, 300, 30), 15, 15);
            p.fillPath(monitor.simplified(), Qt::black);
        }
        // Strichgrafik: eine Kurve mit zwei Löchern, wenige Knoten, gerade Kanten
        const trace::Result lineArt = trace::run(source, trace::presetSettings(trace::Preset::LineArt));
        QCOMPARE(lineArt.colors, 1);
        QCOMPARE(lineArt.shapes.size(), 1);
        QCOMPARE(lineArt.shapes.first().paths.size(), 3);
        QVERIFY2(lineArt.nodes < 50, qPrintable(QString::number(lineArt.nodes)));
        // Bildschirm: Rechteck aus vier Ecken
        const SubPath* screen = nullptr;
        for (const SubPath& path : lineArt.shapes.first().paths) {
            const QRectF b = CurveObject::toPainterPath({path}).boundingRect();
            if (std::abs(b.width() - 510) < 3 && std::abs(b.height() - 305) < 3)
                screen = &path;
        }
        QVERIFY(screen);
        QCOMPARE(screen->nodes.size(), 4);
        for (const CurveNode& n : screen->nodes)
            QVERIFY(!n.curveIn);

        // Freigestellt (durchsichtiger Hintergrund) und am Bildrand: Schwarz ist nicht der Hintergrund
        {
            QImage cut(640, 600, QImage::Format_ARGB32);
            cut.fill(Qt::transparent);
            QPainter p(&cut);
            p.setRenderHint(QPainter::Antialiasing);
            QPainterPath monitor;
            monitor.addRoundedRect(QRectF(0, 0, 640, 460), 40, 40);
            monitor.addRect(QRectF(45.5, 50.3, 550, 305));
            monitor.addEllipse(QPointF(320.5, 400.3), 25, 25);
            monitor.addRect(QRectF(270.5, 460, 100, 140));
            p.fillPath(monitor.simplified(), Qt::black);
            p.end();
            const trace::Result r = trace::run(cut, trace::presetSettings(trace::Preset::LineArt));
            QCOMPARE(r.colors, 1);
            QCOMPARE(r.shapes.size(), 1);
            QCOMPARE(r.shapes.first().paths.size(), 3);
        }

        // Logo: die Graustufen der Kantenglättung werden keine eigenen Farben
        for (trace::Preset preset : {trace::Preset::Logo, trace::Preset::Clipart, trace::Preset::HighQuality}) {
            const trace::Result r = trace::run(source, trace::presetSettings(preset));
            QVERIFY2(r.colors == 2, qPrintable(trace::presetName(preset) + QStringLiteral(": ") + QString::number(r.colors)));
            QVERIFY2(r.shapes.size() <= 4, qPrintable(QString::number(r.shapes.size())));
        }

        // Eigenständige dünne Linie in einer dritten Farbe bleibt
        QImage lines = source.copy();
        {
            QPainter p(&lines);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(QPen(QColor(220, 20, 20), 1.5));
            p.drawLine(QPointF(100, 620), QPointF(560, 620));
        }
        trace::Settings three = trace::presetSettings(trace::Preset::Logo);
        bool red = false;
        for (const trace::PaletteEntry& e : trace::run(lines, three).palette)
            red = red || (e.color.red() > 150 && e.color.green() < 100);
        QVERIFY(red);

        // Register Farben: Weiss in Grau umfärben, beide Farben zusammenführen
        trace::Settings logo = trace::presetSettings(trace::Preset::Logo);
        const trace::Result base = trace::run(source, logo);
        QCOMPARE(base.palette.size(), 2);
        logo.recolor.insert(base.palette[0].source, qRgb(128, 128, 128));
        const trace::Result gray = trace::run(source, logo);
        QVERIFY(std::any_of(gray.palette.begin(), gray.palette.end(),
                            [](const trace::PaletteEntry& e) { return e.color == QColor(128, 128, 128); }));
        logo.recolor.insert(base.palette[1].source, qRgb(128, 128, 128));
        const trace::Result one = trace::run(source, logo);
        QCOMPARE(one.colors, 1);
        QCOMPARE(one.shapes.size(), 1);

        // Hintergrund mit angegebener Farbe: Weiss weg, aber nur am Rand -> Bildschirm bleibt weiss
        trace::Settings given = trace::presetSettings(trace::Preset::Logo);
        given.removeBackground = true;
        given.background = QColor(Qt::white);
        const trace::Result keepScreen = trace::run(source, given);
        QCOMPARE(keepScreen.colors, 2);
        given.removeColorEverywhere = true;
        QCOMPARE(trace::run(source, given).colors, 1);

        // Blaue Insel im roten Feld auf blauem Grund: zusammengelegt würde sie zum Loch
        QImage island(200, 200, QImage::Format_RGB32);
        island.fill(QColor(30, 40, 160));
        {
            QPainter p(&island);
            p.fillRect(QRect(40, 40, 120, 120), QColor(200, 30, 30));
            p.fillRect(QRect(80, 80, 40, 40), QColor(30, 40, 160));
        }
        const trace::Result stacked = trace::run(island, trace::presetSettings(trace::Preset::Logo));
        QImage back(island.size(), QImage::Format_RGB32);
        back.fill(Qt::white);
        {
            QPainter p(&back);
            for (const trace::Shape& shape : stacked.shapes)
                p.fillPath(CurveObject::toPainterPath(shape.paths), shape.color);
        }
        QCOMPARE(back.pixelColor(100, 100), QColor(30, 40, 160));
        QCOMPARE(back.pixelColor(60, 60), QColor(200, 30, 30));
        QCOMPARE(back.pixelColor(10, 10), QColor(30, 40, 160));
    }

    void imageAdjustments()
    {
        QImage img(64, 32, QImage::Format_ARGB32);
        for (int y = 0; y < 32; ++y)
            for (int x = 0; x < 64; ++x)
                img.setPixel(x, y, qRgba(60 + x, 100 + y, 140, 255));
        // Invertieren zweimal = Original
        const QImage img16 = img.convertToFormat(QImage::Format_RGBA64);
        QCOMPARE(adjust::invert(adjust::invert(img)), img16);
        // Sättigung reduzieren: Grau
        const QColor gray = adjust::desaturate(img).pixelColor(5, 5);
        QVERIFY(gray.red() == gray.green() && gray.green() == gray.blue());
        // Diagonale Tonkurve ändert nichts, eine steile hellt auf
        QCOMPARE(adjust::toneCurve(img, {QPointF(0, 0), QPointF(255, 255)}), img16);
        QVERIFY(adjust::toneCurve(img, {QPointF(0, 0), QPointF(128, 200), QPointF(255, 255)}).pixelColor(5, 5).blue() > 140);
        const auto table = adjust::curveTable({QPointF(0, 0), QPointF(64, 120), QPointF(255, 255)});
        for (int i = 1; i < 256; ++i)
            QVERIFY(table[std::size_t(i)] >= table[std::size_t(i - 1)]); // monoton, keine Überschwinger
        // Automatische Anpassung streckt den Umfang
        const auto h = adjust::histogram(adjust::autoAdjust(img));
        QVERIFY(h[0][0] > 0 && h[0][255] > 0);
        // Wärmer: mehr Rot, weniger Blau; Gamma > 1 hellt auf
        adjust::Basic warm;
        warm.temperature = 50;
        const QColor w = adjust::apply(img, warm).pixelColor(5, 5);
        QVERIFY(w.red() > 65 && w.blue() < 140);
        QVERIFY(adjust::gamma(img, 2.0).pixelColor(5, 5).red() > 65);
        QCOMPARE(adjust::posterize(img, 2).pixelColor(5, 5), QColor(0, 0, 255));
    }

    void imagesKeepSixteenBits()
    {
        // Verlauf mit 1024 Stufen je Kanal, wie ein 14-Bit-Foto zwischen den 8-Bit-Stufen
        QImage ramp(1024, 4, QImage::Format_RGBA64);
        for (int y = 0; y < ramp.height(); ++y) {
            auto* line = reinterpret_cast<QRgba64*>(ramp.scanLine(y));
            for (int x = 0; x < ramp.width(); ++x)
                line[x] = QRgba64::fromRgba64(quint16(x * 64), quint16(x * 32), quint16(65535 - x * 64), 65535);
        }
        auto distinctRed = [](const QImage& image) {
            QSet<quint16> values;
            const QImage img = image.convertToFormat(QImage::Format_RGBA64);
            const auto* line = reinterpret_cast<const QRgba64*>(img.constScanLine(0));
            for (int x = 0; x < img.width(); ++x)
                values.insert(line[x].red());
            return values.size();
        };
        QCOMPARE(distinctRed(ramp), 1024);
        // Anpassungen behalten die Zwischenstufen (8 Bit hätte höchstens 256)
        QVERIFY(distinctRed(adjust::gamma(ramp, 1.4)) > 900);
        adjust::Basic warm;
        warm.temperature = 20;
        warm.contrast = 15;
        QVERIFY(distinctRed(adjust::apply(ramp, warm)) > 700);
        QVERIFY(distinctRed(adjust::toneCurve(ramp, {QPointF(0, 0), QPointF(128, 150), QPointF(255, 255)})) > 900);
        QCOMPARE(adjust::invert(adjust::invert(ramp)), ramp);

        // Bildpool: 16-Bit-PNG hin und zurück ohne Verlust
        const QString key = imagepool::addImage(ramp);
        QCOMPARE(imagepool::sourceBits(key), 16);
        QCOMPARE(imagepool::image(key), ramp);
        QCOMPARE(imagepool::displayImage(key).format(), QImage::Format_ARGB32_Premultiplied);
        // 16-Bit-TIFF (Qt-Modul imageformats) ebenso
        if (QImageWriter::supportedImageFormats().contains("tiff")) {
            QByteArray tiff;
            QBuffer buffer(&tiff);
            buffer.open(QIODevice::WriteOnly);
            QVERIFY(QImageWriter(&buffer, "tiff").write(ramp));
            const QString tiffKey = imagepool::add(tiff);
            QCOMPARE(imagepool::suffix(tiffKey), QStringLiteral("tiff"));
            QCOMPARE(imagepool::sourceBits(tiffKey), 16);
            QCOMPARE(distinctRed(imagepool::image(tiffKey)), 1024);
        } else {
            qWarning("Kein TIFF-Modul (Qt imageformats) installiert");
        }
        // Anzeige: ein Grau genau zwischen 128 und 129 wird gerastert, im Mittel 128,5
        QImage between(16, 16, QImage::Format_RGBA64);
        between.fill(QColor::fromRgba64(QRgba64::fromRgba64(quint16(128.5 * 257), quint16(128.5 * 257), quint16(128.5 * 257), 65535)));
        const QImage shown = imagepool::ditherTo8Bit(between);
        double sum = 0;
        QSet<int> levels;
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x) {
                sum += qRed(shown.pixel(x, y));
                levels.insert(qRed(shown.pixel(x, y)));
            }
        QCOMPARE(levels, (QSet<int>{128, 129}));
        QVERIFY(std::abs(sum / 256 - 128.5) < 0.05);
        // 8-Bit-Werte bleiben beim Rastern exakt
        QImage exact(4, 4, QImage::Format_ARGB32_Premultiplied);
        exact.fill(QColor(10, 200, 30, 180));
        QCOMPARE(imagepool::ditherTo8Bit(exact.convertToFormat(QImage::Format_RGBA64)), exact);
        // 8-Bit-Quellen werden verlustfrei auf 16 Bit gehoben
        QImage small(4, 4, QImage::Format_RGB32);
        small.fill(QColor(10, 200, 30));
        const QString key8 = imagepool::addImage(small);
        QCOMPARE(imagepool::sourceBits(key8), 8);
        QCOMPARE(imagepool::image(key8).format(), QImage::Format_RGBA64);
        QCOMPARE(imagepool::image(key8).pixelColor(1, 1).rgb(), QColor(10, 200, 30).rgb());
    }

    void rawFilesAreRecognised()
    {
        // Kein Bild: LibRaw lehnt ab, ohne abzustürzen
        QVERIFY(raw::decode(QByteArray("kein Rohbild")).isNull());
        QVERIFY(raw::decode(QByteArray()).isNull());
        QVERIFY(imagepool::importSuffixes().contains(QStringLiteral("nef")));
        QVERIFY(imagepool::importSuffixes().contains(QStringLiteral("png")));
    }

    // Echte Rohdatei (z.B. Nikon D500, 14 Bit), nur mit VEK_RAW_FILE
    void rawImportFromFile()
    {
        const QString path = qEnvironmentVariable("VEK_RAW_FILE");
        if (path.isEmpty())
            QSKIP("nur mit VEK_RAW_FILE");
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray bytes = file.readAll();
        QElapsedTimer timer;
        timer.start();
        const QString key = imagepool::add(bytes);
        qInfo("Rohdatei entwickelt in %lld ms", timer.elapsed());
        QVERIFY(!key.isEmpty());
        const QImage image = imagepool::image(key);
        qInfo("%d x %d, Quelle %d Bit, Endung %s", image.width(), image.height(), imagepool::sourceBits(key),
              qPrintable(imagepool::suffix(key)));
        // Nicht das eingebettete Vorschaubild (160 x 120), sondern das ganze Foto
        QVERIFY(image.width() > 2000 && image.height() > 1000);
        QCOMPARE(imagepool::sourceBits(key), 16);
        QCOMPARE(image.format(), QImage::Format_RGBA64);
        // Feine Abstufungen: in einer Bildzeile viel mehr verschiedene Werte, als 8 Bit hergeben
        QSet<quint16> values;
        for (int y = image.height() / 2; y < image.height() / 2 + 8; ++y) {
            const auto* line = reinterpret_cast<const QRgba64*>(image.constScanLine(y));
            for (int x = 0; x < image.width(); ++x)
                values.insert(line[x].green());
        }
        qInfo("verschiedene Grünwerte in 8 Zeilen: %lld", qsizetype(values.size()));
        QVERIFY(values.size() > 256);
        if (path.endsWith(QLatin1String(".nef"), Qt::CaseInsensitive))
            QCOMPARE(imagepool::suffix(key), QStringLiteral("nef"));
        // Im Dokument bleibt die Rohdatei unverändert (verlustfrei) und wird beim Laden neu entwickelt
        Document doc;
        {
            Transaction tx(doc, QStringLiteral("Bild"));
            tx.insert(firstLayer(doc, 0), -1, std::make_unique<ImageObject>(key, QSizeF(100, 66)));
            tx.commit();
        }
        const QByteArray saved = io::saveToBytes(doc);
        QVERIFY(saved.size() > bytes.size() * 9 / 10);
        QVERIFY(io::loadFromBytes(saved));
        QCOMPARE(imagepool::encoded(key), bytes);
        const QString shots = qEnvironmentVariable("VEK_SCREENSHOT_DIR");
        if (!shots.isEmpty())
            imagepool::displayImage(key).scaledToWidth(1200, Qt::SmoothTransformation).save(shots + QStringLiteral("/60-nef.png"));
    }

    void pageSettingsLiveOnMaster()
    {
        Document doc;
        {
            Transaction tx(doc, QStringLiteral("x"));
            tx.modify<Page>(doc.masterPage().id(), [](Page& p) {
                p.setNudge(0.5);
                p.setDuplicateOffset(QPointF(2, -3));
            });
            tx.commit();
        }
        QString error;
        std::unique_ptr<Document> loaded = io::loadFromBytes(io::saveToBytes(doc, &error), &error);
        QVERIFY2(loaded, qPrintable(error));
        QCOMPARE(loaded->masterPage().nudge(), 0.5);
        QCOMPARE(loaded->masterPage().duplicateOffset(), QPointF(2, -3));
        doc.undo();
        QCOMPARE(doc.masterPage().nudge(), 0.1);
    }

    void modifiedFlagFollowsHistory()
    {
        Document doc;
        QSignalSpy spy(&doc, &Document::modifiedChanged);
        {
            Transaction tx(doc, QStringLiteral("a"));
            tx.insert(firstLayer(doc, 0), -1, makeRect(0, 0, 1, 1));
            tx.commit();
        }
        doc.setClean();
        QVERIFY(!doc.isModified());
        doc.undo();
        QVERIFY(doc.isModified());
        doc.redo();
        QVERIFY(!doc.isModified());
        QCOMPARE(spy.count(), 4);
    }

    void rectangleCornersScaleWithOption()
    {
        RectangleObject r(QSizeF(40, 20));
        r.setCornerRadius(RectangleObject::TopLeft, 4.0);
        r.applyTransform(QTransform::fromScale(2.0, 3.0));
        QCOMPARE(r.size(), QSizeF(80, 60));
        QCOMPARE(r.cornerRadius(RectangleObject::TopLeft), 8.0);
        RectangleObject::setScaleCorners(false);
        r.applyTransform(QTransform::fromScale(0.5, 0.5));
        RectangleObject::setScaleCorners(true);
        QCOMPARE(r.cornerRadius(RectangleObject::TopLeft), 8.0);
    }


    void autocorrectWhileTyping()
    {
        using namespace autocorrect;
        Settings s;
        s.capitalizeSentences = true;
        s.replacements = Settings::parseReplacements(QStringLiteral("zb=z. B.;mfg = Mit freundlichen Grüssen"));
        auto type = [&s](const QString& text) {
            QString out;
            for (QChar c : text) {
                const Edit e = onType(out, c, s);
                out.chop(e.remove);
                out += e.insert;
            }
            return out;
        };
        QCOMPARE(type(QStringLiteral("am montag kommt ZWei. das geht ")), QStringLiteral("Am Montag kommt Zwei. Das geht "));
        QCOMPARE(type(QStringLiteral("er sagt \"hallo\" und 'du' ")), QStringLiteral("Er sagt «hallo» und ‹du› "));
        QCOMPARE(type(QStringLiteral("5' 10\" gross ")), QStringLiteral("5' 10\" gross "));
        QCOMPARE(type(QStringLiteral("geht's ")), QStringLiteral("Geht’s "));
        QCOMPARE(type(QStringLiteral("x zb mfg,")), QStringLiteral("X z. B. Mit freundlichen Grüssen,"));
        s.capitalizeSentences = false;
        s.typographicQuotes = false;
        QCOMPARE(type(QStringLiteral("a \"b\" ")), QStringLiteral("a \"b\" "));
        s.language = Settings::Language::French;
        QCOMPARE(type(QStringLiteral("lundi montag ")), QStringLiteral("lundi montag "));
    }


    void spellCheckerFindsErrors()
    {
        SpellChecker& sc = SpellChecker::instance();
        sc.setSearchPaths({QStringLiteral(VEK_DICTIONARY_SOURCE)});
        QTemporaryDir dir;
        sc.setUserDictionary(dir.filePath(QStringLiteral("eigene.txt")));
        QVERIFY(sc.availableLanguages().contains(QStringLiteral("de_CH")));
        sc.setLanguage(QStringLiteral("de_CH"));
        QVERIFY(sc.available());
        QVERIFY(sc.isCorrect(QStringLiteral("Strasse")));
        QVERIFY(sc.isCorrect(QStringLiteral("Grüsse")));
        QVERIFY(sc.isCorrect(QStringLiteral("Kaffeemaschine")));
        QVERIFY(!sc.isCorrect(QStringLiteral("Kafeemaschine")));
        QVERIFY(sc.isCorrect(QStringLiteral("A4")));
        QVERIFY(sc.suggestions(QStringLiteral("Kafeemaschine")).contains(QStringLiteral("Kaffeemaschine")));
        const QString text = QStringLiteral("Das ist ein Tesst mit Feler, aber TANGENTAX ist neu.");
        QList<SpellChecker::Range> errors = sc.check(text);
        QStringList words;
        for (const auto& r : errors)
            words << text.mid(r.start, r.length);
        QCOMPARE(words, (QStringList{QStringLiteral("Tesst"), QStringLiteral("Feler"), QStringLiteral("TANGENTAX")}));
        sc.addToUserDictionary(QStringLiteral("TANGENTAX"));
        sc.ignore(QStringLiteral("Feler"));
        QCOMPARE(sc.check(text).size(), 1);
        QCOMPARE(sc.check(text, true).size(), 2);
        QVERIFY(sc.check(text, true).last().ignored);
        // Benutzerwörterbuch bleibt über einen Neustart
        sc.setUserDictionary(dir.filePath(QStringLiteral("eigene.txt")));
        QVERIFY(sc.isCorrect(QStringLiteral("TANGENTAX")));
        const SpellChecker::Range w = SpellChecker::wordAt(text, 14);
        QCOMPARE(text.mid(w.start, w.length), QStringLiteral("Tesst"));
        // Englisch und Französisch (UTF-8-Wörterbücher)
        sc.setLanguage(QStringLiteral("en_US"));
        QVERIFY(sc.isCorrect(QStringLiteral("color")) && !sc.isCorrect(QStringLiteral("colr")));
        sc.setLanguage(QStringLiteral("fr_FR"));
        QVERIFY(sc.isCorrect(QStringLiteral("été")) && !sc.isCorrect(QStringLiteral("étè")));
        sc.setLanguage(QStringLiteral("de_CH"));
        sc.setUserDictionary(QString());
    }


    void alignAndDistribute()
    {
        using namespace align;
        const QRectF ref(10, 10, 20, 20), r(50, 60, 10, 4);
        QCOMPARE(offset(r, ref, H::Left, V::None), QPointF(-40, 0));
        QCOMPARE(offset(r, ref, H::Right, V::Top), QPointF(-30, -50));
        QCOMPARE(offset(r, ref, H::Center, V::Center), QPointF(-35, -42));
        QCOMPARE(offset(r, ref, H::None, V::Bottom), QPointF(0, -34));
        QCOMPARE(toGrid(QRectF(4.2, 7.9, 3, 3), QSizeF(5, 5), H::Left, V::Bottom), QPointF(0.8, -0.9));
        // Drei Objekte in falscher Reihenfolge: Linke Kanten gleichmässig zwischen erstem und letztem
        const QList<QRectF> rects{QRectF(100, 0, 10, 10), QRectF(0, 0, 20, 10), QRectF(30, 0, 5, 10)};
        QList<QPointF> d = distribute(rects, QRectF(0, 0, 110, 10), Spread::Start, Spread::None);
        QCOMPARE(d[1], QPointF(0, 0));
        QCOMPARE(d[0], QPointF(0, 0));
        QCOMPARE(d[2].x(), 50.0 - 30.0);
        // Abstand: gleiche Lücken (110 - 35) / 2 = 37.5
        d = distribute(rects, QRectF(0, 0, 110, 10), Spread::Spacing, Spread::None);
        QCOMPARE(rects[2].left() + d[2].x(), 57.5);
        QCOMPARE(rects[0].left() + d[0].x(), 100.0);
        // Mitten senkrecht über einen grösseren Umfang (Seitenumfang)
        const QList<QRectF> col{QRectF(0, 0, 5, 10), QRectF(0, 20, 5, 10)};
        d = distribute(col, QRectF(0, 0, 100, 200), Spread::None, Spread::Center);
        QCOMPARE(d[0].y(), 0.0);
        QCOMPARE(col[1].center().y() + d[1].y(), 195.0);
    }


    void bitmapFilters()
    {
        using namespace filters;
        QImage img(40, 30, QImage::Format_RGBA64);
        img.fill(Qt::white);
        {
            QPainter p(&img);
            p.fillRect(QRect(10, 10, 20, 10), Qt::black);
        }
        // Jeder Filter läuft, behält die Grösse und liefert 16 Bit
        const QList<std::pair<const char*, QImage>> all{
            {"gaussian", gaussian(img, 4)}, {"motion", motion(img, 6, 30)}, {"radial", radialBlur(img, 30)}, {"zoom", zoomBlur(img, 30)},
            {"smooth", smooth(img, 40)}, {"usm", unsharpMask(img, 100, 2, 0)}, {"noise", addNoise(img, 30, false)}, {"median", median(img, 1)},
            {"find", findEdges(img, 50)}, {"detect", detectEdges(img, 50)}, {"trace", traceContour(img, 128)}, {"solarize", solarize(img, 128)},
            {"psy", psychedelic(img, 60)}, {"halftone", halftone(img, 6)}, {"pixelate", pixelate(img, 5)}, {"ripple", ripple(img, 3, 10)},
            {"swirl", swirl(img, 90)}, {"wind", wind(img, 10)}, {"tiles", tiles(img, 4, 10)}, {"crystal", crystallize(img, 6)},
            {"vignette", vignette(img, 60, 50, Color(0, 0, 0))}, {"mosaic", mosaic(img, 6)}, {"emboss", emboss(img, 20, 135)},
            {"pinch", pinch(img, 50)}, {"sphere", sphere(img, 60)}, {"charcoal", charcoal(img, 2, 50)}, {"pencil", pencil(img, 6)},
            {"oil", oilPaint(img, 2, 10)}, {"diffuse", diffuse(img, 2)}, {"bw", toBlackWhite(img, 128)}, {"gray", toGrayscale(img)},
            {"palette", toPalette(img, 256)}, {"mask", colorMask(img, {Color(255, 255, 255)}, 5)}};
        for (const auto& [name, out] : all)
            QVERIFY2(out.size() == img.size(), name);
        // Stichproben
        QCOMPARE(QColor(findEdges(img, 50).pixel(5, 5)), QColor(Qt::white));   // ohne Kante weiss
        QVERIFY(QColor(detectEdges(img, 50).pixel(10, 15)).red() > 100);        // Kante hell
        QCOMPARE(qAlpha(colorMask(img, {Color(255, 255, 255)}, 5).pixel(2, 2)), 0);
        QCOMPARE(qAlpha(colorMask(img, {Color(255, 255, 255)}, 5).pixel(15, 15)), 255);
        QCOMPARE(QColor(solarize(img, 128).pixel(2, 2)), QColor(Qt::black)); // Weiss über der Schwelle kippt
        const QImage blur = gaussian(img, 6);
        QVERIFY(QColor(blur.pixel(10, 15)).red() > 20 && QColor(blur.pixel(10, 15)).red() < 235); // Kante weich
        QCOMPARE(pixelate(img, 40).pixel(0, 0), pixelate(img, 40).pixel(39, 29));
    }

    void impositionLayouts()
    {
        using namespace impose;
        const QSizeF a5(148, 210);
        // Seiten nach ihrer Lage auf dem Bogen: Zeile für Zeile von oben links
        auto pagesOf = [](const Side& side) {
            QList<Placement> sorted = side.pages;
            std::sort(sorted.begin(), sorted.end(), [](const Placement& a, const Placement& b) {
                return std::abs(a.trim.top() - b.trim.top()) > 0.1 ? a.trim.top() < b.trim.top() : a.trim.left() < b.trim.left();
            });
            QList<int> out;
            for (const Placement& p : sorted)
                out << p.page;
            return out;
        };
        // 16 A5-Seiten als Heft auf A4 quer, Drucker wendet an der kurzen Kante
        Settings s;
        s.layout = Layout::Booklet;
        s.paper = QSizeF(210, 297);
        s.flip = Flip::ShortEdge;
        Result r = arrange(s, 16, a5);
        QVERIFY2(r.ok(), qPrintable(r.error));
        QCOMPARE(r.sheet, QSizeF(297, 210));
        QVERIFY(!r.warning.isEmpty()); // 296 mm auf 297 mm: randlos
        QCOMPARE(r.sheets.size(), 4);
        QVERIFY(r.duplex);
        QCOMPARE(pagesOf(r.sheets[0].front), (QList<int>{15, 0}));  // Seite 16 | Seite 1
        QCOMPARE(pagesOf(r.sheets[0].back), (QList<int>{1, 14}));   // Seite 2 | Seite 15
        QCOMPARE(pagesOf(r.sheets[3].front), (QList<int>{9, 6}));
        QCOMPARE(pagesOf(r.sheets[3].back), (QList<int>{7, 8}));    // Heftmitte: 8 | 9
        // Beide Seiten im Block mittig, Bund ohne Abstand
        QCOMPARE(r.sheets[0].front.pages[0].trim.right(), r.sheets[0].front.pages[1].trim.left());
        QCOMPARE(r.sheets[0].front.pages[0].trim.left(), (297 - 296) / 2.0);
        // Jede Seite genau einmal
        QList<int> all;
        for (const Sheet& sh : r.sheets)
            all << pagesOf(sh.front) << pagesOf(sh.back);
        std::sort(all.begin(), all.end());
        for (int i = 0; i < 16; ++i)
            QCOMPARE(all[i], i);
        // Schnittmarken nur aussen, keine im Bund; Falzmarke im Bund
        const double spine = r.sheets[0].front.pages[0].trim.right();
        for (const QLineF& l : r.sheets[0].front.cropMarks)
            QVERIFY(!(qFuzzyCompare(l.x1(), spine) && qFuzzyCompare(l.x2(), spine)));
        QCOMPARE(r.sheets[0].front.foldMarks.size(), 2);
        QCOMPARE(r.sheets[0].front.foldMarks[0].x1(), spine);
        // Lange Kante: Rückseite steht auf dem Kopf
        s.flip = Flip::LongEdge;
        r = arrange(s, 16, a5);
        QCOMPARE(pagesOf(r.sheets[0].back), (QList<int>{14, 1}));
        QCOMPARE(r.sheets[0].back.pages[0].rotation, 180);
        QCOMPARE(r.sheets[0].back.pages[1].rotation, 180);
        // 10 Seiten: auf 12 ergänzt, Leerseiten am Ende
        s.flip = Flip::ShortEdge;
        r = arrange(s, 10, a5);
        QCOMPARE(r.pageCount, 12);
        QCOMPARE(r.blankPages, 2);
        QCOMPARE(pagesOf(r.sheets[0].front), (QList<int>{-1, 0}));
        // Bundzuwachs: innerster Bogen ganz verschoben, äusserster nicht
        s.creep = 1.0;
        r = arrange(s, 16, a5);
        QCOMPARE(r.sheets[0].front.pages[1].shift, 0.0);
        QCOMPARE(r.sheets[3].front.pages[0].shift, 1.0);   // linke Seite nach rechts zum Bund
        QCOMPARE(r.sheets[3].front.pages[1].shift, -1.0);
        s.creep = 0.0;
        // A4-Seiten passen als Heft nicht auf A4, wohl aber auf A3 (mit Rand)
        r = arrange(s, 8, QSizeF(210, 297));
        QVERIFY(!r.ok());
        s.paper = QSizeF(297, 420);
        r = arrange(s, 8, QSizeF(140, 200));
        QVERIFY(r.ok() && r.warning.isEmpty());
        s.paper = QSizeF(210, 297);

        // Falzbogen 8 Seiten: 16 A5 auf A3 geht nur bündig (ohne Kopfbeschnitt und Anschnitt)
        s.layout = Layout::Signature8;
        s.paper = QSizeF(297, 420);
        s.flip = Flip::LongEdge; // Bogen hoch: lange Kante = senkrechte Achse, Rückseite nicht gedreht
        r = arrange(s, 16, a5);
        QVERIFY2(r.ok() && !r.warning.isEmpty(), qPrintable(r.error));
        s.gap = 6;
        s.bleed = 3;
        QVERIFY(!arrange(s, 16, a5).ok());
        // ... mit Kopfbeschnitt 6 mm und 3 mm Anschnitt auf SRA3
        s.paper = QSizeF(320, 450);
        r = arrange(s, 16, a5);
        QVERIFY2(r.ok() && r.warning.isEmpty(), qPrintable(r.error));
        QCOMPARE(r.sheet, QSizeF(320, 450));
        QCOMPARE(r.sheets.size(), 2);
        // Bogen 1 (aussen): Seiten 1-4 und 13-16; vorn 13↓ 4↓ / 16 1, hinten 3↓ 14↓ / 2 15
        QCOMPARE(pagesOf(r.sheets[0].front), (QList<int>{12, 3, 15, 0}));
        QCOMPARE(pagesOf(r.sheets[0].back), (QList<int>{2, 13, 1, 14}));
        QCOMPARE(r.sheets[0].front.pages[0].rotation, 180);
        QCOMPARE(r.sheets[0].front.pages[2].rotation, 0);
        QCOMPARE(pagesOf(r.sheets[1].front), (QList<int>{8, 7, 11, 4}));
        // Kopf: obere Reihe endet 6 mm über der unteren, Anschnitt dort höchstens 3 mm
        QCOMPARE(r.sheets[0].front.pages[2].trim.top() - r.sheets[0].front.pages[0].trim.bottom(), 6.0);
        QCOMPARE(r.sheets[0].front.pages[0].bleed.bottom(), 3.0);
        QCOMPARE(r.sheets[0].front.pages[0].bleed.right(), 0.0); // Bund
        // Hintereinander (Klebebindung): Bogen 1 = Seiten 1-8
        s.nested = false;
        r = arrange(s, 16, a5);
        QCOMPARE(pagesOf(r.sheets[0].front), (QList<int>{4, 3, 7, 0}));
        QCOMPARE(pagesOf(r.sheets[1].front), (QList<int>{12, 11, 15, 8}));

        // Visitenkarten 85 x 55 auf A4: hoch 2 x 5 = 10 Nutzen
        s = Settings();
        s.layout = Layout::StepRepeat;
        s.paper = QSizeF(210, 297);
        s.gap = 0;
        r = arrange(s, 2, QSizeF(85, 55));
        QVERIFY(r.ok());
        QCOMPARE(r.sheets.size(), 2);
        QCOMPARE(r.sheets[0].front.pages.size(), 10);
        QCOMPARE(r.sheets[1].front.pages[7].page, 1);
        QVERIFY(!r.sheets[0].hasBack);
        // Mehrfachnutzen der Reihe nach, 2 x 2 fest
        s.layout = Layout::NUp;
        s.columns = 2;
        s.rows = 2;
        r = arrange(s, 5, QSizeF(90, 120));
        QCOMPARE(r.sheets.size(), 2);
        QCOMPARE(pagesOf(r.sheets[1].front), (QList<int>{4}));
    }

};

QTEST_MAIN(TestCore)
#include "tst_core.moc"
