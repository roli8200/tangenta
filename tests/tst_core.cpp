#include "ImageCorrect.h"
#include "Blend.h"
#include "BlendObject.h"
#include "CurveObject.h"
#include "Effects.h"
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
#include "MediaLibrary.h"
#include "Arrangement.h"
#include "PolygonObject.h"
#include "RawImage.h"
#include "RectangleObject.h"
#include "AutoCorrect.h"
#include "Align.h"
#include "BitmapFilters.h"
#include "BitmapArt.h"
#include "BitmapEffect.h"
#include "BitmapFrame.h"
#include "Imposition.h"
#include <QPainter>
#include <QTemporaryDir>
#include "SpellChecker.h"
#include "Serialization.h"
#include "SuperClipObject.h"
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
#include <QtMath>
#include <QSet>
#include "PageLayout.h"
#include "QrCodeObject.h"
#include "Mosaic.h"
#include <QFileInfo>

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

    void overlappingGlyphContoursFillLikeNonzero()
    {
        // Variable Schriften: Konturen eines Zeichens überlappen (Nonzero). Kurven füllen gerade-ungerade;
        // ohne Vereinigung entstanden Löcher an den Überlappungen
        QPainterPath glyphs;
        glyphs.setFillRule(Qt::WindingFill);
        glyphs.addEllipse(QPointF(0, 0), 20, 20);
        glyphs.addEllipse(QPointF(25, 0), 20, 20);          // überlappt den ersten Kreis
        QPainterPath ring;
        ring.addEllipse(QPointF(100, 0), 20, 20);
        QPainterPath inner;
        inner.addEllipse(QPointF(100, 0), 10, 10);
        glyphs.addPath(ring);
        glyphs.addPath(inner.toReversed());                  // Loch: andersherum, ganz innen
        const QPainterPath before = glyphs;
        QPainterPath odd = before;
        odd.setFillRule(Qt::OddEvenFill);
        QVERIFY(!odd.contains(QPointF(12, 0)));              // vorher: Loch in der Überlappung

        QPainterPath fixed = pathops::withoutOverlaps(glyphs, 0.05);
        fixed.setFillRule(Qt::OddEvenFill);
        QVERIFY(fixed.contains(QPointF(12, 0)));             // Überlappung gefüllt
        QVERIFY(fixed.contains(QPointF(-15, 0)));
        QVERIFY(fixed.contains(QPointF(40, 0)));
        QVERIFY(!fixed.contains(QPointF(12, 25)));
        QVERIFY(fixed.contains(QPointF(115, 0)));            // Ring bleibt Ring
        QVERIFY(!fixed.contains(QPointF(100, 0)));
        QVERIFY(std::abs(fixed.boundingRect().left() + 20) < 0.2);
        QVERIFY(std::abs(fixed.boundingRect().right() - 120) < 0.2);
        // Ohne Überlappung bleibt der Pfad unverändert (gleiche Kurvenpunkte)
        QPainterPath plain;
        plain.addPath(ring);
        plain.addPath(inner.toReversed());
        QCOMPARE(pathops::withoutOverlaps(plain, 0.05).elementCount(), plain.elementCount());
    }

    // Löcher schliessen (Begrenzung, Schneidelinien): alle oder nur schmale
    void holesClosed()
    {
        QPainterPath shape;
        shape.addRect(0, 0, 100, 100);
        shape.addEllipse(QPointF(30, 50), 20, 20);          // breites Loch (Radius 20)
        shape.addRect(70, 20, 0.4, 30);                      // Schlitz 0,4 mm breit
        QPainterPath inner;                                  // Insel im breiten Loch bleibt (gerade Tiefe)
        inner.addEllipse(QPointF(30, 50), 5, 5);
        shape.addPath(inner);
        shape.setFillRule(Qt::OddEvenFill);
        const QPainterPath all = pathops::withoutHoles(shape);
        QVERIFY(all.contains(QPointF(30, 40)));
        QVERIFY(all.contains(QPointF(70.2, 30)));
        QVERIFY(all.contains(QPointF(30, 50)));
        const QPainterPath narrow = pathops::withoutHoles(shape, 1.0);
        QVERIFY(!narrow.contains(QPointF(30, 40)));          // breites Loch bleibt
        QVERIFY(narrow.contains(QPointF(70.2, 30)));         // Schlitz zu
        QVERIFY(narrow.contains(QPointF(30, 50)));           // Insel bleibt
        // Ohne Löcher unverändert
        QPainterPath plain;
        plain.addRect(0, 0, 10, 10);
        QCOMPARE(pathops::withoutHoles(plain).elementCount(), plain.elementCount());
    }

    void overlapMergeKeepsStraightEdges()
    {
        // Wie ein A der variablen Cinzel in mm (30 mm hoch): zwei schräge Beine mit geraden Kanten und
        // Serifenbogen, die sich oben überlappen. Nach dem Vereinigen bleiben Kanten gerade und Ecken an Ort
        auto leg = [](QPointF top, QPointF footL, QPointF footR, double bulge) {
            QPainterPath p;
            p.moveTo(top);
            p.lineTo(footR);
            p.cubicTo(footR + QPointF(bulge, 0), footR + QPointF(bulge, 40), footR + QPointF(bulge * 2, 40));
            p.lineTo(footL + QPointF(-bulge * 2, 40));
            p.cubicTo(footL + QPointF(-bulge, 40), footL + QPointF(-bulge, 0), footL);
            p.closeSubpath();
            return p;
        };
        QPainterPath units;
        units.addPath(leg(QPointF(1000, 0), QPointF(300, 2400), QPointF(700, 2400), 120));
        units.addPath(leg(QPointF(1000, 0), QPointF(1500, 2400), QPointF(1900, 2400), 120).translated(-60, 0));
        QPainterPath glyph = QTransform::fromScale(0.0125, 0.0125).map(units);
        glyph.setFillRule(Qt::WindingFill);
        const QPainterPath fixed = pathops::withoutOverlaps(glyph, 0.002);
        // Abstand in beide Richtungen, gemessen fein abgeflacht (x1000)
        const QTransform fine = QTransform::fromScale(1000, 1000);
        const QList<QPolygonF> original = fine.map(glyph).simplified().toSubpathPolygons();
        const QList<QPolygonF> merged = fine.map(fixed).toSubpathPolygons();
        auto distance = [](const QPointF& a, const QList<QPolygonF>& polys) {
            double best = 1e18;
            for (const QPolygonF& o : polys)
                for (int k = 0; k < o.size(); ++k) {
                    const QPointF u = o[k], v = o[(k + 1) % o.size()], d = v - u;
                    const double len2 = QPointF::dotProduct(d, d);
                    const double t = len2 > 0 ? std::clamp(QPointF::dotProduct(a - u, d) / len2, 0.0, 1.0) : 0.0;
                    best = std::min(best, QLineF(a, u + t * d).length());
                }
            return best / 1000.0; // mm
        };
        double worst = 0;
        for (const QPolygonF& poly : merged)
            for (const QPointF& pt : poly)
                worst = std::max(worst, distance(pt, original));
        for (const QPolygonF& poly : original)
            for (const QPointF& pt : poly)
                worst = std::max(worst, distance(pt, merged));
        QVERIFY2(worst < 0.01, qPrintable(QString::number(worst)));
        // Mitte der geraden Aussenkante des linken Beins liegt exakt auf dem neuen Rand (nicht gebogen)
        const QPointF mid = (QPointF(1000, 0) + QPointF(300, 2400)) * 0.0125 / 2.0;
        QVERIFY2(distance(fine.map(mid), merged) < 0.005, qPrintable(QString::number(distance(fine.map(mid), merged))));
        QPainterPath odd = fixed;
        odd.setFillRule(Qt::OddEvenFill);
        QVERIFY(odd.contains(QPointF(975, 120) * 0.0125)); // Überlappung oben gefüllt
        QPainterPath before = glyph;
        before.setFillRule(Qt::OddEvenFill);
        QVERIFY(!before.contains(QPointF(975, 120) * 0.0125));
    }

    void weldNearTangentShapesStaysClean()
    {
        // Tropfen: Kreis plus Dreieck, dessen Seiten den Kreis (fast) berühren. Qt-Boolesche
        // Operationen lieferten hier Haarspalten oder lange Sporne entlang einer Seite.
        auto coverage = [](const QPainterPath& path, const QRectF& box) {
            QImage img(400, 400, QImage::Format_Grayscale8);
            img.fill(0);
            QPainter p(&img);
            p.setRenderHint(QPainter::Antialiasing);
            p.setTransform(QTransform::fromScale(400 / box.width(), 400 / box.height()) * QTransform::fromTranslate(0, 0));
            p.translate(-box.topLeft());
            p.fillPath(path, Qt::white);
            p.end();
            return img;
        };
        int bad = 0;
        QStringList report;
        for (double radius : {17.0, 40.0}) {
            for (double angle = 50.0; angle <= 64.0; angle += 1.0) {
                for (double inset : {1.0, 0.99, 0.97, 0.95}) {
                    QPainterPath circle;
                    circle.addEllipse(QPointF(0, 0), radius, radius);
                    // Berührpunkte bei ±angle vom Scheitel; Spitze auf der Tangente durch diese Punkte
                    const double a = qDegreesToRadians(angle);
                    const QPointF left(-radius * std::sin(a) * inset, -radius * std::cos(a) * inset);
                    const QPointF right(radius * std::sin(a) * inset, -radius * std::cos(a) * inset);
                    const double apexY = -radius / std::cos(a);
                    QPainterPath tri;
                    tri.moveTo(0, apexY);
                    tri.lineTo(right);
                    tri.lineTo(left);
                    tri.closeSubpath();
                    // Wie Anordnen > Formen > Verschmelzen: Vereinigung, dann Kurven anpassen
                    pathops::Geometry g;
                    g.area = pathops::united(circle, tri);
                    const auto curve = pathops::rebuild(RectangleObject(QSizeF(1, 1)), g, 0.02);
                    QVERIFY(curve);
                    const QPainterPath result = curve->path();
                    QPainterPath both = circle;
                    both.addPath(tri);
                    const QRectF box = both.boundingRect().adjusted(-2, -2, 2, 2);
                    QImage want = coverage(circle, box);
                    {
                        const QImage t = coverage(tri, box);
                        for (int y = 0; y < want.height(); ++y)
                            for (int x = 0; x < want.width(); ++x)
                                want.scanLine(y)[x] = std::max(want.scanLine(y)[x], t.constScanLine(y)[x]);
                    }
                    const QImage got = coverage(result, box);
                    int diff = 0;
                    for (int y = 0; y < want.height(); ++y)
                        for (int x = 0; x < want.width(); ++x)
                            diff += std::abs(int(want.constScanLine(y)[x]) - int(got.constScanLine(y)[x])) > 96;
                    const bool spur = !both.boundingRect().adjusted(-0.05, -0.05, 0.05, 0.05).contains(result.boundingRect());
                    if (diff > 6 || spur) {
                        ++bad;
                        report << QStringLiteral("R%1 %2° %3: diff %4%5").arg(radius).arg(angle).arg(inset).arg(diff).arg(spur ? " Sporn" : "");
                    }
                }
            }
        }
        QVERIFY2(bad == 0, qPrintable(report.join(QStringLiteral("; "))));
    }

    void qrCodeEncodesPayloadsAndRoundTrips()
    {
        // Inhalte, wie Telefone sie lesen
        QCOMPARE(qr::payload(qr::Kind::Url, {{QStringLiteral("url"), QStringLiteral("tangenta.ch")}}),
                 QStringLiteral("https://tangenta.ch"));
        QCOMPARE(qr::payload(qr::Kind::Url, {{QStringLiteral("url"), QStringLiteral("http://a.b/c")}}), QStringLiteral("http://a.b/c"));
        QCOMPARE(qr::payload(qr::Kind::Phone, {{QStringLiteral("number"), QStringLiteral("+41 52 123 45 67")}}),
                 QStringLiteral("tel:+41521234567"));
        QCOMPARE(qr::payload(qr::Kind::Email, {{QStringLiteral("to"), QStringLiteral("info@example.ch")},
                                               {QStringLiteral("subject"), QStringLiteral("Grüsse & mehr")}}),
                 QStringLiteral("mailto:info@example.ch?subject=Gr%C3%BCsse%20%26%20mehr"));
        QCOMPARE(qr::payload(qr::Kind::Wifi, {{QStringLiteral("ssid"), QStringLiteral("Café;Gast")},
                                              {QStringLiteral("password"), QStringLiteral("a:b\\c")},
                                              {QStringLiteral("security"), QStringLiteral("WPA")},
                                              {QStringLiteral("hidden"), QStringLiteral("1")}}),
                 QStringLiteral("WIFI:T:WPA;S:Café\\;Gast;P:a\\:b\\\\c;H:true;;"));
        QCOMPARE(qr::payload(qr::Kind::Wifi, {{QStringLiteral("ssid"), QStringLiteral("Offen")},
                                              {QStringLiteral("password"), QStringLiteral("x")},
                                              {QStringLiteral("security"), QStringLiteral("nopass")}}),
                 QStringLiteral("WIFI:T:nopass;S:Offen;;"));
        const QString card = qr::payload(qr::Kind::Contact, {{QStringLiteral("first"), QStringLiteral("Anna")},
                                                             {QStringLiteral("last"), QStringLiteral("Muster")},
                                                             {QStringLiteral("org"), QStringLiteral("Beispiel AG, Zürich")},
                                                             {QStringLiteral("city"), QStringLiteral("Zürich")}});
        QVERIFY(card.startsWith(QStringLiteral("BEGIN:VCARD\r\nVERSION:3.0\r\nN:Muster;Anna;;;\r\nFN:Anna Muster\r\n")));
        QVERIFY(card.contains(QStringLiteral("ORG:Beispiel AG\\, Zürich\r\n")));
        QVERIFY(card.contains(QStringLiteral("ADR;TYPE=WORK:;;;Zürich;;;")));
        QVERIFY(card.endsWith(QStringLiteral("END:VCARD")));

        // Kodieren: Version 1 = 21 Module; Suchmuster oben links (Ring, heller Ring, Kern)
        const qr::Matrix m = qr::encode(QStringLiteral("HELLO WORLD"), 1);
        QCOMPARE(m.size, 21);
        QCOMPARE(m.version, 1);
        QVERIFY(m.at(0, 0) && m.at(6, 0) && m.at(0, 6) && !m.at(1, 1) && m.at(3, 3) && !m.at(7, 7));
        QVERIFY(m.at(20, 0) && m.at(0, 20) && !m.at(19, 1));
        // Höhere Fehlerkorrektur braucht bei langem Text mehr Module
        const QString longText = QStringLiteral("https://tangenta.ch/de/download/windows?quelle=flyer-2026");
        QVERIFY(qr::encode(longText, 3).size > qr::encode(longText, 0).size);
        QString error;
        QCOMPARE(qr::encode(QString(5000, u'x'), 3, &error).size, 0);
        QVERIFY(!error.isEmpty());

        // Objekt: Fläche samt Ruhezone, Module mit der Füllung
        QrCodeObject code;
        code.setContent(qr::Kind::Url, {{QStringLiteral("url"), QStringLiteral("tangenta.ch")}});
        code.setSize(QSizeF(30, 30));
        code.setQuietZone(4);
        code.setTransform(QTransform::fromTranslate(10, 20));
        QCOMPARE(code.boundingRect(), QRectF(10, 20, 30, 30));
        const QPainterPath modules = code.localPath();
        QVERIFY(!modules.isEmpty());
        const int n = code.matrix().size;
        const double mw = 30.0 / (n + 8);
        // Erstes Modul oben links dunkel, Ruhezone frei
        QVERIFY(modules.contains(QPointF(4.5 * mw, 4.5 * mw)));
        QVERIFY(!modules.contains(QPointF(2 * mw, 2 * mw)));
        QVERIFY(!modules.contains(QPointF(5.5 * mw, 5.5 * mw))); // heller Ring des Suchmusters
        QVERIFY(modules.contains(QPointF(7.5 * mw, 7.5 * mw)));  // Kern
        QVERIFY(code.hitTest(QPointF(11, 21), 0.1, false));       // auch in der Ruhezone
        // Skalieren ändert die Grösse, nicht die Matrix
        code.applyTransform(QTransform::fromScale(2, 2));
        QCOMPARE(code.size(), QSizeF(60, 60));
        QCOMPARE(code.transform().m11(), 1.0);
        // Punkte: gleiche Matrix, andere Form
        code.setShape(QrCodeObject::Shape::Dots);
        const QPainterPath dots = code.localPath();
        const double mw2 = 60.0 / (n + 8);
        QVERIFY(dots.contains(QPointF(4.5 * mw2, 7.5 * mw2)));   // Ring des Suchmusters
        QVERIFY(!dots.contains(QPointF(5.5 * mw2, 7.5 * mw2)));  // heller Ring bleibt frei
        QVERIFY(dots.contains(QPointF(7.5 * mw2, 7.5 * mw2)));   // Kern

        // Speichern und Laden
        code.setContent(qr::Kind::Wifi, {{QStringLiteral("ssid"), QStringLiteral("Gast")}, {QStringLiteral("password"), QStringLiteral("geheim")}});
        code.setErrorCorrection(3);
        code.setBackground(Color(255, 240, 200));
        auto back = serial::fromJson(serial::toJson(code), serial::Ids::Keep);
        const auto* copy = node_cast<QrCodeObject>(back.get());
        QVERIFY(copy);
        QCOMPARE(copy->kind(), qr::Kind::Wifi);
        QCOMPARE(copy->payload(), code.payload());
        QCOMPARE(copy->errorCorrection(), 3);
        QVERIFY(copy->shape() == QrCodeObject::Shape::Dots);
        QCOMPARE(copy->quietZone(), 4);
        QVERIFY(copy->background() == std::optional<Color>(Color(255, 240, 200)));
        QCOMPARE(copy->size(), QSizeF(60, 60));
        // Eigenschaftsleiste: Hintergrund aus, Inhalt
        QrCodeObject bar;
        QVERIFY(bar.setPropertyValue(QStringLiteral("qrBackground"), false));
        QVERIFY(!bar.background());
        QVERIFY(bar.setPropertyValue(QStringLiteral("qrData"), QStringLiteral("example.org")));
        QCOMPARE(bar.payload(), QStringLiteral("https://example.org"));

        // Bilder zum Prüfen mit einem Leser (VEK_QR_DIR)
        const QString dir = qEnvironmentVariable("VEK_QR_DIR");
        if (!dir.isEmpty()) {
            for (int shape = 0; shape < 2; ++shape) {
                QrCodeObject sample;
                sample.setContent(qr::Kind::Url, {{QStringLiteral("url"), QStringLiteral("https://tangenta.ch/?quelle=qr")}});
                sample.setShape(QrCodeObject::Shape(shape));
                sample.setQuietZone(4);
                sample.setSize(QSizeF(400, 400));
                QImage img(400, 400, QImage::Format_RGB32);
                img.fill(Qt::white);
                QPainter p(&img);
                p.setRenderHint(QPainter::Antialiasing);
                p.fillPath(sample.localPath(), Qt::black);
                p.end();
                img.save(dir + QStringLiteral("/qr-%1.png").arg(shape));
            }
        }
    }

    void superClipReadsOldTypeName()
    {
        // Neu gespeichert als "superclip"; Dateien aus 0.85 und älter heissen noch "powerclip"
        QJsonObject json = serial::toJson(SuperClipObject());
        QCOMPARE(json.value(QStringLiteral("type")).toString(), QStringLiteral("superclip"));
        json.insert(QStringLiteral("type"), QStringLiteral("powerclip"));
        auto back = serial::fromJson(json, serial::Ids::Keep);
        QVERIFY(node_cast<SuperClipObject>(back.get()));
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

    // Wie das Monitor-Symbol im SuperTrace-Video: schwarz auf weiss, geglättete Kanten
    // Schrift auf einem Farbverlauf (wie ein Plakat in kleiner Auflösung): Titel, Untertitel und
    // Kleingedrucktes müssen als Flächen in Textfarbe erhalten bleiben
    void vectorizerKeepsSmallText()
    {
        const QSize size(520, 360);
        QImage source(size, QImage::Format_RGB32);
        QImage mask(size, QImage::Format_Grayscale8);
        mask.fill(0);
        {
            QPainter p(&source);
            QLinearGradient sky(0, 0, 0, size.height());
            sky.setColorAt(0, QColor(0x2a, 0x1f, 0x55));
            sky.setColorAt(0.6, QColor(0x8a, 0x3f, 0x7a));
            sky.setColorAt(1, QColor(0xf0, 0x8a, 0x4b));
            p.fillRect(source.rect(), sky);
        }
        auto write = [&](const QString& text, int px, bool bold, int y) {
            QFont f(QStringLiteral("Arial"));
            f.setPixelSize(px);
            f.setBold(bold);
            QPainter p(&source);
            p.setRenderHint(QPainter::TextAntialiasing);
            p.setFont(f);
            p.setPen(QColor(0xff, 0xf3, 0xe0));
            p.drawText(QPoint(24, y), text);
            QPainter m(&mask);
            QFont plain = f;
            plain.setStyleStrategy(QFont::NoAntialias);
            m.setFont(plain);
            m.setPen(Qt::white);
            m.drawText(QPoint(24, y), text);
        };
        write(QStringLiteral("SOMMERNACHT"), 44, true, 80);
        write(QStringLiteral("Open Air am See · 21. Juni 2027"), 16, false, 120);
        write(QStringLiteral("Tickets ab 29 CHF, Vorverkauf ab 1. Mai"), 12, false, 300);

        auto score = [&](trace::Preset preset) {
            const trace::Result r = trace::run(source, trace::presetSettings(preset));
            QImage out(size, QImage::Format_RGB32);
            out.fill(Qt::black);
            QPainter p(&out);
            p.setRenderHint(QPainter::Antialiasing, false);
            for (const trace::Shape& s : r.shapes)
                p.fillPath(CurveObject::toPainterPath(s.paths), s.color);
            p.end();
            if (const QString dir = qEnvironmentVariable("VEK_SCREENSHOT_DIR"); !dir.isEmpty()) {
                source.save(dir + QStringLiteral("/trace-text-original.png"));
                out.save(dir + QStringLiteral("/trace-text-%1.png").arg(int(preset)));
            }
            const auto cream = [](QRgb c) { return qRed(c) > 200 && qGreen(c) > 185 && qBlue(c) > 160; };
            int hit = 0, total = 0, wrong = 0, background = 0;
            int lineHit[3] = {0, 0, 0}, lineTotal[3] = {0, 0, 0};
            for (int y = 0; y < size.height(); ++y) {
                const int line = y < 90 ? 0 : y < 200 ? 1 : 2;
                for (int x = 0; x < size.width(); ++x) {
                    const bool text = qGray(mask.pixel(x, y)) > 128;
                    const bool shown = cream(out.pixel(x, y));
                    (text ? total : background)++;
                    if (text) {
                        ++lineTotal[line];
                        if (shown)
                            ++lineHit[line];
                    }
                    if (text && shown)
                        ++hit;
                    if (!text && shown)
                        ++wrong;
                }
            }
            qInfo().noquote() << "  Titel" << double(lineHit[0]) / lineTotal[0] << "Untertitel" << double(lineHit[1]) / lineTotal[1]
                              << "Kleingedrucktes" << double(lineHit[2]) / lineTotal[2] << "Knoten" << r.nodes;
            return std::pair{double(hit) / total, double(wrong) / std::max(1, total)};
        };
        QList<std::tuple<trace::Preset, double, double>> results;
        for (const trace::Preset preset : {trace::Preset::Clipart, trace::Preset::Logo, trace::Preset::DetailedLogo, trace::Preset::HighQuality}) {
            const auto [recall, extra] = score(preset);
            qInfo().noquote() << trace::presetName(preset) << "Text erhalten" << recall << "zu viel" << extra;
            results << std::tuple{preset, recall, extra};
        }
        for (const auto& [preset, recall, extra] : std::as_const(results)) {
            QVERIFY2(recall > 0.85, qPrintable(trace::presetName(preset) + QStringLiteral(": nur %1 der Schrift erhalten").arg(recall)));
            QVERIFY2(extra < 0.45, qPrintable(trace::presetName(preset) + QStringLiteral(": %1 zu viel Schriftfarbe").arg(extra)));
        }
    }

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

    void bitmapArt()
    {
        using namespace filters;
        // Verlauf mit Formen, ein Pixel ganz durchsichtig
        QImage img(64, 48, QImage::Format_ARGB32);
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
                img.setPixel(x, y, qRgb(x * 4, y * 5, 255 - x * 2));
        {
            QPainter p(&img);
            p.fillRect(QRect(10, 10, 20, 12), Qt::black);
            p.setBrush(Qt::yellow);
            p.drawEllipse(QRect(36, 20, 20, 20));
        }
        img.setPixel(3, 3, qRgba(0, 0, 0, 0));
        using Fn = std::function<QImage(const QImage&)>;
        const QList<std::pair<const char*, Fn>> all{
            {"posterize", [](const QImage& i) { return posterize(i, 4); }},
            {"soften", [](const QImage& i) { return soften(i, 60); }},
            {"impressionism", [](const QImage& i) { return impressionism(i, 4, 30, 0, 7); }},
            {"watercolor", [](const QImage& i) { return watercolor(i, 2, 50, 50, 0, 7); }},
            {"scraperboard", [](const QImage& i) { return scraperboard(i, 60, 50, false); }},
            {"scraperboardColor", [](const QImage& i) { return scraperboard(i, 60, 50, true); }},
            {"wavyPaper", [](const QImage& i) { return wavyPaper(i, 50, true); }},
            {"fabric", [](const QImage& i) { return fabric(i, 6, 50, 0, 15); }},
            {"glassBlock", [](const QImage& i) { return glassBlock(i, 12, 10); }},
            {"smokedGlass", [](const QImage& i) { return smokedGlass(i, Color(32, 32, 32), 50, 20); }}};
        const QImage ref = img.convertToFormat(QImage::Format_RGBA64);
        for (const auto& [name, fn] : all) {
            const QImage out = fn(img);
            QVERIFY2(out.size() == img.size(), name);
            QVERIFY2(out.format() == QImage::Format_RGBA64, name);
            QVERIFY2(qAlpha(out.pixel(3, 3)) == 0, name); // Transparenz bleibt
            QVERIFY2(qAlpha(out.pixel(20, 30)) == 255, name);
            QVERIFY2(out == fn(img), name); // reproduzierbar
            QVERIFY2(out != ref, name);     // wirkt
        }
        // Zufall hängt am Startwert
        QVERIFY(impressionism(img, 4, 30, 0, 1) != impressionism(img, 4, 30, 0, 2));
        QVERIFY(watercolor(img, 2, 50, 50, 0, 1) != watercolor(img, 2, 50, 50, 0, 2));

        // Tontrennung: zwei Stufen ergeben je Kanal nur 0 oder 255
        const QImage post = posterize(img, 2);
        for (int y = 0; y < post.height(); ++y)
            for (int x = 0; x < post.width(); ++x) {
                const QRgb c = post.pixel(x, y);
                if (qAlpha(c) == 255)
                    for (int v : {qRed(c), qGreen(c), qBlue(c)})
                        QVERIFY(v == 0 || v == 255);
            }
        // Weichzeichnen: 0 % lässt das Bild unverändert, 100 % bleibt milder als Gauss
        QCOMPARE(soften(img, 0).convertToFormat(QImage::Format_ARGB32), img);
        auto diff = [](const QImage& a, const QImage& b) {
            double sum = 0;
            for (int y = 0; y < a.height(); ++y)
                for (int x = 0; x < a.width(); ++x)
                    sum += std::abs(qRed(a.pixel(x, y)) - qRed(b.pixel(x, y))) + std::abs(qGreen(a.pixel(x, y)) - qGreen(b.pixel(x, y)));
            return sum / (a.width() * a.height());
        };
        QVERIFY(diff(soften(img, 100), ref) > 0.5);
        QVERIFY(diff(soften(img, 100), ref) < diff(gaussian(img, 6), ref));
        // Rauchglas: schwarz zu 100 % ist fast schwarz, ohne Tönung und Unschärfe fast unverändert
        const QImage dark = smokedGlass(img, Color(0, 0, 0), 100, 20);
        QVERIFY(QColor(dark.pixel(50, 10)).value() < 20);
        QVERIFY(diff(smokedGlass(img, Color(0, 0, 0), 0, 0), ref) < 2);
        // Getönt: rote Scheibe macht Weiss rötlich, halb dunkle dunkelt ab
        QImage white(32, 32, QImage::Format_RGB32);
        white.fill(Qt::white);
        const QColor red(smokedGlass(white, Color(220, 30, 30), 80, 20).pixel(16, 16));
        QVERIFY(red.red() > red.green() + 80 && red.red() > red.blue() + 80);
        QVERIFY(QColor(smokedGlass(white, Color(32, 32, 32), 50, 20).pixel(16, 16)).value() < 160);
        // Schabkarton: dunkle Fläche bleibt schwarz, weisse Fläche wird überwiegend hell
        QImage half(64, 32, QImage::Format_RGB32);
        half.fill(Qt::black);
        QPainter(&half).fillRect(QRect(32, 0, 32, 32), Qt::white);
        const QImage sb = scraperboard(half, 60, 50, false);
        QCOMPARE(QColor(sb.pixel(5, 16)).value(), 0);
        double bright = 0;
        for (int y = 0; y < 32; ++y)
            for (int x = 44; x < 64; ++x)
                bright += QColor(sb.pixel(x, y)).valueF();
        QVERIFY(bright / (20 * 32) > 0.6);
        // Farbige Linien übernehmen die Bildfarbe
        QImage blue(48, 48, QImage::Format_RGB32);
        blue.fill(QColor(80, 120, 255));
        double hueSum = 0;
        const QImage sbc = scraperboard(blue, 60, 80, true);
        for (int y = 0; y < 48; ++y)
            for (int x = 0; x < 48; ++x)
                hueSum += qBlue(sbc.pixel(x, y)) - qRed(sbc.pixel(x, y));
        QVERIFY(hueSum > 0);
        // Wellenpapier ohne Farbe ist grau, mit Farbe nicht
        const QRgb g = wavyPaper(img, 50, false).pixel(50, 40);
        QVERIFY(std::abs(qRed(g) - qGreen(g)) <= 1 && std::abs(qGreen(g) - qBlue(g)) <= 1);
        const QRgb wc = wavyPaper(img, 50, true).pixel(50, 5);
        QVERIFY(std::abs(qRed(wc) - qBlue(wc)) > 30);
        // Gewebe: in der einfarbigen Fläche wechseln Faden und Lücke (hell und dunkel)
        QImage flat(60, 60, QImage::Format_RGB32);
        flat.fill(QColor(180, 140, 100));
        const QImage fab = fabric(flat, 12, 50, 0, 0);
        int lo = 255, hi = 0;
        for (int x = 0; x < 60; ++x) {
            const int v = QColor(fab.pixel(x, 30)).value();
            lo = std::min(lo, v);
            hi = std::max(hi, v);
        }
        QVERIFY(hi - lo > 60);
        // Glasblock: einfarbige Fläche bleibt in der Blockmitte gleich, Fugen setzen sich ab
        const QImage gb = glassBlock(flat, 20, 20);
        QVERIFY(std::abs(QColor(gb.pixel(10, 10)).red() - 180) < 8);
        QVERIFY(std::abs(QColor(gb.pixel(20, 10)).red() - 180) > 15);
        // Aquarell und Impressionismus behalten die Grundfarbe grob bei
        for (const QImage& o : {watercolor(flat, 3, 30, 30, 0, 1), impressionism(flat, 6, 0, 0, 1)}) {
            const QColor m(o.pixel(30, 30));
            QVERIFY(std::abs(m.red() - 180) < 40 && std::abs(m.blue() - 100) < 40 && m.red() > m.blue());
        }
    }

    // Vorschaubilder der künstlerischen Effekte (nur mit VEK_ART_PREVIEW_DIR und VEK_ART_PREVIEW_SRC)
    void bitmapArtPreview()
    {
        const QString dir = qEnvironmentVariable("VEK_ART_PREVIEW_DIR");
        const QString srcPath = qEnvironmentVariable("VEK_ART_PREVIEW_SRC");
        if (dir.isEmpty() || srcPath.isEmpty())
            QSKIP("nur für Sichtprüfung");
        using namespace filters;
        QImage src(srcPath);
        QVERIFY(!src.isNull());
        if (src.width() > 2000)
            src = src.scaledToWidth(2000, Qt::SmoothTransformation);
        using Fn = std::function<QImage(const QImage&)>;
        const QList<std::pair<const char*, Fn>> all{
            {"posterize", [](const QImage& i) { return posterize(i, 6); }},
            {"soften", [](const QImage& i) { return soften(i, 50); }},
            {"impressionism", [](const QImage& i) { return impressionism(i, 16, 30, 0); }},
            {"watercolor", [](const QImage& i) { return watercolor(i, 5, 50, 50, 0); }},
            {"scraperboard", [](const QImage& i) { return scraperboard(i, 50, 50, false); }},
            {"scraperboard_color", [](const QImage& i) { return scraperboard(i, 50, 50, true); }},
            {"wavypaper", [](const QImage& i) { return wavyPaper(i, 50, true); }},
            {"wavypaper_gray", [](const QImage& i) { return wavyPaper(i, 50, false); }},
            {"fabric", [](const QImage& i) { return fabric(i, 16, 50, 0, 0); }},
            {"glassblock", [](const QImage& i) { return glassBlock(i, 60, 60); }},
            {"smokedglass", [](const QImage& i) { return smokedGlass(i, Color(32, 32, 32), 50, 20); }},
            {"smokedglass_blue", [](const QImage& i) { return smokedGlass(i, Color(40, 80, 160), 60, 30); }},
            {"duotone", [](const QImage& i) { return duotone(i, Color(0, 0, 0), Color(150, 95, 40), 50, 16); }},
            {"straighten", [](const QImage& i) { return straighten(i, 6, true); }},
            {"rotate3d", [](const QImage& i) { return rotate3d(i, 20, 20); }},
            {"perspective", [](const QImage& i) { return perspective(i, 40, 0); }}};
        const QString only = qEnvironmentVariable("VEK_ART_PREVIEW_ONLY");
        for (const auto& [name, fn] : all) {
            if (!only.isEmpty() && !only.split(',').contains(QLatin1String(name)))
                continue;
            QElapsedTimer t;
            t.start();
            const QImage out = fn(src);
            qInfo("%s: %lld ms (%dx%d)", name, t.elapsed(), src.width(), src.height());
            QVERIFY(out.convertToFormat(QImage::Format_ARGB32).save(dir + "/" + name + ".png"));
        }
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


    // Kombination aufheben bei Grafiktext: Zeilen, dann Wörter, dann Zeichen; Lage bleibt
    void textBreakApart()
    {
        TextObject text(TextObject::Kind::Artistic);
        text.setPlainText(QStringLiteral("Erste Zeile\n\nZweite"), CharFormat(QStringLiteral("Arial"), 20));
        text.setTransform(QTransform().translate(10, 20).rotate(-30));
        auto lines = text.breakApart();
        QCOMPARE(int(lines.size()), 2); // leere Zeile fällt weg
        QCOMPARE(lines[0]->plainText(), QStringLiteral("Erste Zeile"));
        QCOMPARE(lines[1]->plainText(), QStringLiteral("Zweite"));
        QPainterPath all;
        for (const auto& l : lines)
            all.addPath(l->path());
        const QRectF a = all.boundingRect();
        const QRectF b = text.path().boundingRect();
        QVERIFY(std::abs(a.left() - b.left()) < 0.02 && std::abs(a.top() - b.top()) < 0.02
                && std::abs(a.right() - b.right()) < 0.02 && std::abs(a.bottom() - b.bottom()) < 0.02);
        auto words = lines[0]->breakApart();
        QCOMPARE(int(words.size()), 2);
        QCOMPARE(words[1]->plainText(), QStringLiteral("Zeile"));
        auto chars = words[1]->breakApart();
        QCOMPARE(int(chars.size()), 5);
        QVERIFY(chars[4]->breakApart().empty());
        // Mengentext bleibt ganz
        TextObject frame(TextObject::Kind::Paragraph, QSizeF(50, 50));
        frame.setPlainText(QStringLiteral("a\nb"));
        QVERIFY(frame.breakApart().empty());
    }
    // --- Ecken: Abrunden, Ausbuchten, Abschrägen ----------------------------------------------
private:
    static SubPath polyPath(const QList<QPointF>& pts, bool closed = true)
    {
        SubPath sp;
        sp.closed = closed;
        for (const QPointF& p : pts)
            sp.nodes.append(CurveNode{p});
        return sp;
    }

    // Grösste Abweichung der Kurve vom Kreis (center, r) im Bereich der Ecke (Abstand < near zur Ecke)
    static double circleDeviation(const QList<SubPath>& paths, const QPointF& corner, double nearDist,
                                  const QPointF& center, double r)
    {
        const QPainterPath path = CurveObject::toPainterPath(paths);
        double worst = 0.0;
        for (int i = 0; i <= 2000; ++i) {
            const QPointF p = path.pointAtPercent(i / 2000.0);
            if (QLineF(p, corner).length() < nearDist)
                worst = std::max(worst, std::abs(QLineF(p, center).length() - r));
        }
        return worst;
    }

private slots:

    void cornersFilletTriangle()
    {
        // Gleichseitiges Dreieck, Seite 100; Abrunden mit 10 mm
        const double h = 100.0 * std::sqrt(3.0) / 2.0;
        const QList<SubPath> tri{polyPath({QPointF(0, h), QPointF(100, h), QPointF(50, 0)})};
        const QList<SubPath> r = pathops::applyCorners(tri, pathops::CornerOp::Fillet, 10.0);
        QCOMPARE(r.size(), 1);
        QVERIFY(r[0].closed);
        // je Ecke zwei Tangentenpunkte + ein Zwischenknoten (Bogen 120° in zwei Stücken)
        QCOMPARE(r[0].nodes.size(), 9);
        // Tangentenpunkte 10/tan(30°) = 17,32 mm von der Ecke entfernt, auf den Kanten
        const double d = 10.0 / std::tan(std::numbers::pi / 6.0);
        bool found = false;
        for (const CurveNode& n : r[0].nodes)
            found = found || QLineF(n.point, QPointF(d, h)).length() < 1e-6;
        QVERIFY(found);
        // Mittelpunkt der Rundung auf der Winkelhalbierenden, 20 mm von der Ecke
        const QPointF center(d, h - 10.0);
        const double dev = circleDeviation(r, QPointF(0, h), d * 0.95, center, 10.0);
        QVERIFY2(dev < 0.02, qPrintable(QString::number(dev)));
        // Fläche: Dreieck minus drei Zwickel (r²·(√3 − π/3) je Ecke)
        const QRectF b = CurveObject::toPainterPath(r).boundingRect();
        QVERIFY(std::abs(b.left() - (center.x() - 10.0)) < 0.01);
        QVERIFY(std::abs(b.bottom() - h) < 1e-6);
        // Die Tangentenpunkte sind glatte Knoten
        for (const CurveNode& n : r[0].nodes)
            QCOMPARE(n.kind, NodeKind::Smooth);
    }

    void cornersChamferAndScallopSquare()
    {
        const QList<SubPath> sq{polyPath({QPointF(0, 0), QPointF(40, 0), QPointF(40, 40), QPointF(0, 40)})};
        const QList<SubPath> ch = pathops::applyCorners(sq, pathops::CornerOp::Chamfer, 5.0);
        QCOMPARE(ch[0].nodes.size(), 8);
        for (const CurveNode& n : ch[0].nodes)
            QVERIFY(!n.curveIn);
        QVERIFY(!CurveObject::toPainterPath(ch).contains(QPointF(1, 1)));
        QVERIFY(CurveObject::toPainterPath(ch).contains(QPointF(3, 3)));
        bool cut = false;
        for (const CurveNode& n : ch[0].nodes)
            cut = cut || QLineF(n.point, QPointF(5, 0)).length() < 1e-9;
        QVERIFY(cut);

        // Ausbuchten: Viertelkreis um die Ecke, nach innen gewölbt
        const QList<SubPath> sc = pathops::applyCorners(sq, pathops::CornerOp::Scallop, 8.0);
        QCOMPARE(sc[0].nodes.size(), 8);
        const double dev = circleDeviation(sc, QPointF(0, 0), 7.9, QPointF(0, 0), 8.0);
        QVERIFY2(dev < 0.02, qPrintable(QString::number(dev)));
        const QPainterPath sp = CurveObject::toPainterPath(sc);
        QVERIFY(!sp.contains(QPointF(5, 5)));  // innerhalb des Ausschnitts
        QVERIFY(sp.contains(QPointF(7, 7)));   // ausserhalb des Kreises
        QVERIFY(sp.contains(QPointF(20, 20)));
    }

    void cornersClampAndSkip()
    {
        // Riesiger Radius: auf die halbe Seite begrenzt, aus dem Quadrat wird ein Kreis
        const QList<SubPath> sq{polyPath({QPointF(0, 0), QPointF(40, 0), QPointF(40, 40), QPointF(0, 40)})};
        const QList<SubPath> big = pathops::applyCorners(sq, pathops::CornerOp::Fillet, 500.0);
        const double dev = circleDeviation(big, QPointF(20, 20), 100.0, QPointF(20, 20), 20.0);
        QVERIFY2(dev < 0.05, qPrintable(QString::number(dev)));

        // Nur gewählte Knoten; offene Enden bleiben; glatte Knoten bleiben
        const QList<SubPath> one = pathops::applyCorners(sq, pathops::CornerOp::Fillet, 5.0, {NodeRef{0, 2}});
        QCOMPARE(one[0].nodes.size(), 5); // 3 unveränderte Ecken + 2 Tangentenpunkte (90° = ein Bogenstück)
        const QList<SubPath> open{polyPath({QPointF(0, 0), QPointF(40, 0), QPointF(40, 40)}, false)};
        const QList<SubPath> o = pathops::applyCorners(open, pathops::CornerOp::Chamfer, 5.0);
        QCOMPARE(o[0].nodes.size(), 4);
        QCOMPARE(o[0].nodes.first().point, QPointF(0, 0));
        QCOMPARE(o[0].nodes.last().point, QPointF(40, 40));
        SubPath smooth = sq[0];
        for (CurveNode& n : smooth.nodes)
            n.kind = NodeKind::Smooth;
        QCOMPARE(pathops::applyCorners({smooth}, pathops::CornerOp::Fillet, 5.0), QList<SubPath>{smooth});
        // Gerade Fortsetzung (kein Knick) bleibt unverändert
        const QList<SubPath> straight{polyPath({QPointF(0, 0), QPointF(10, 0), QPointF(20, 0)}, false)};
        QCOMPARE(pathops::applyCorners(straight, pathops::CornerOp::Fillet, 5.0), straight);
    }

    void cornersLineCurveCorner()
    {
        // Ecke zwischen einer Linie und einem Kurvensegment: Ergebnis glatt an beiden Übergängen
        SubPath sp = polyPath({QPointF(0, 0), QPointF(50, 0)}, false);
        CurveNode end{QPointF(50, 50)};
        end.curveIn = true;
        end.c1 = QPointF(60, 15);
        end.c2 = QPointF(60, 35);
        sp.nodes.append(end);
        const QList<SubPath> r = pathops::applyCorners({sp}, pathops::CornerOp::Fillet, 6.0);
        QVERIFY(r[0].nodes.size() >= 4);
        QCOMPARE(r[0].nodes.first().point, QPointF(0, 0));
        QVERIFY(QLineF(r[0].nodes.last().point, QPointF(50, 50)).length() < 1e-6);
        // Der Knick ist weg: an jedem neuen Knoten stimmen die Richtungen überein
        const QPainterPath path = CurveObject::toPainterPath(r);
        for (int i = 1; i < 200; ++i) {
            const double a = path.angleAtPercent((i - 1) / 200.0), b = path.angleAtPercent(i / 200.0);
            double diff = std::abs(a - b);
            diff = std::min(diff, 360.0 - diff);
            QVERIFY2(diff < 10.0, qPrintable(QStringLiteral("%1: %2").arg(i).arg(diff)));
        }
    }

    void polygonNodeShapeRoundTrip()
    {
        PolygonObject p(QSizeF(100, 100), PolygonObject::Kind::Polygon, 6, 0);
        QCOMPARE(p.shapeNodes().size(), 12);
        const QPainterPath plain = p.localPath();
        PolygonObject::NodeShape s;
        s.outerScale = 1.0;
        s.innerScale = 0.5;
        p.setNodeShape(s);
        // Seitenmitten nach innen: geschwungenes Polygon, Ecken bleiben
        QVERIFY(p.localPath() != plain);
        QVERIFY(!p.localPath().contains(QPointF(50 + 40, 50))); // vorher innen
        QVERIFY(plain.contains(QPointF(50 + 40, 50)));
        QVERIFY(std::abs(p.shapeNodes()[0].y() - 0.0) < 1e-9);
        const std::unique_ptr<Node> copy = serial::fromJson(serial::toJson(p));
        const auto* q = node_cast<PolygonObject>(copy.get());
        QVERIFY(q);
        QCOMPARE(q->nodeShape(), s);
        QCOMPARE(q->localPath(), p.localPath());
        // Ohne Bearbeitung wird nichts geschrieben
        QVERIFY(!serial::properties(PolygonObject(QSizeF(10, 10), PolygonObject::Kind::Star, 5, 50)).contains(QStringLiteral("nodeShape")));
    }

    void curveTransformNodes()
    {
        CurveObject c({polyPath({QPointF(0, 0), QPointF(10, 0), QPointF(10, 10)}, false)});
        c.setSegmentCurve(NodeRef{0, 2}, true);
        const QPointF c1 = c.node(NodeRef{0, 2}).c1;
        QTransform rot;
        rot.rotate(90);
        c.transformNodes({NodeRef{0, 1}, NodeRef{0, 2}}, rot);
        QVERIFY(QLineF(c.node(NodeRef{0, 1}).point, QPointF(0, 10)).length() < 1e-9);
        QVERIFY(QLineF(c.node(NodeRef{0, 2}).point, QPointF(-10, 10)).length() < 1e-9);
        QVERIFY(QLineF(c.node(NodeRef{0, 2}).c1, rot.map(c1)).length() < 1e-9);
        QCOMPARE(c.node(NodeRef{0, 0}).point, QPointF(0, 0)); // nicht gewählt

        // Ein Eckknoten allein: auch die Steuerpunkte am Nachbarknoten drehen mit (Schlaufe wie beim Vorbild)
        CurveObject sq({polyPath({QPointF(0, 0), QPointF(3, 0), QPointF(3, 3), QPointF(0, 3)}, true)});
        sq.setSegmentCurve(NodeRef{0, 2}, true); // (3,0) -> (3,3)
        sq.setSegmentCurve(NodeRef{0, 3}, true); // (3,3) -> (0,3)
        const QPointF far1 = sq.node(NodeRef{0, 2}).c1, far2 = sq.node(NodeRef{0, 3}).c2;
        QTransform half;
        half.translate(1.5, 1.5);
        half.rotate(180);
        half.translate(-1.5, -1.5);
        sq.transformNodes({NodeRef{0, 2}}, half);
        QVERIFY(QLineF(sq.node(NodeRef{0, 2}).point, QPointF(0, 0)).length() < 1e-9);
        QVERIFY(QLineF(sq.node(NodeRef{0, 2}).c1, half.map(far1)).length() < 1e-9);
        QVERIFY(QLineF(sq.node(NodeRef{0, 3}).c2, half.map(far2)).length() < 1e-9);
        QCOMPARE(sq.node(NodeRef{0, 1}).point, QPointF(3, 0));
    }

    // --- Smart-Füllung: planares Netz aus offenen und geschlossenen Linien -----------------
private:
    static QList<SubPath> pathsOf(const QPainterPath& p) { return CurveObject::fromPath(p)->subPaths(); }
    static QList<SubPath> circlePaths(const QPointF& c, double r)
    {
        QPainterPath p;
        p.addEllipse(c, r, r);
        return pathsOf(p);
    }
    static QList<SubPath> rectPaths(const QRectF& r)
    {
        QPainterPath p;
        p.addRect(r);
        return pathsOf(p);
    }
    static SubPath linePath(const QPointF& a, const QPointF& b) { return SubPath{{CurveNode{a}, CurveNode{b}}, false}; }
    // Fläche (Löcher gegenläufig) fein abgetastet
    static double areaOf(const QList<SubPath>& face)
    {
        double sum = 0;
        for (const QPolygonF& poly : vek::pathops::flattenFine(CurveObject::toPainterPath(face), 0.05)) {
            double a = 0;
            for (int i = 0; i < poly.size(); ++i)
                a += poly[i].x() * poly[(i + 1) % poly.size()].y() - poly[(i + 1) % poly.size()].x() * poly[i].y();
            sum += a / 2;
        }
        return std::abs(sum);
    }

private slots:
    void arrangementPieSlicesFromOpenLines()
    {
        using vek::pathops::Arrangement;
        // Kreis, von zwei offenen Linien (über den Rand hinaus) geviertelt: die Stummel draussen zählen nicht
        QList<SubPath> paths = circlePaths(QPointF(50, 50), 20);
        paths << linePath(QPointF(20, 50), QPointF(80, 50)) << linePath(QPointF(50, 20), QPointF(50, 80));
        const Arrangement arr(paths);
        QCOMPARE(arr.faceCount(), 4);
        const QList<SubPath> slice = arr.faceAt(QPointF(60, 40));
        QCOMPARE(slice.size(), 1);
        QVERIFY(slice.first().closed);
        QVERIFY2(std::abs(areaOf(slice) - M_PI * 400 / 4) < 0.5, qPrintable(QString::number(areaOf(slice))));
        // Der Bogen bleibt eine Bézierkurve
        QVERIFY(std::any_of(slice.first().nodes.begin(), slice.first().nodes.end(), [](const CurveNode& n) { return n.curveIn; }));
        QVERIFY(slice.first().nodes.size() <= 6);
        QVERIFY(CurveObject::toPainterPath(slice).boundingRect().adjusted(-0.01, -0.01, 0.01, 0.01).contains(QRectF(50, 30, 20, 20)));
        // Ausserhalb aller Maschen: nichts
        QVERIFY(arr.faceAt(QPointF(5, 5)).isEmpty());
        QVERIFY(arr.faceAt(QPointF(75, 50.5)).isEmpty()); // neben dem Stummel ausserhalb

        // Windrad: drei Linien durch die Mitte -> sechs Stücke
        QList<SubPath> wheel = circlePaths(QPointF(0, 0), 10);
        for (int k = 0; k < 3; ++k) {
            const double a = k * M_PI / 3;
            wheel << linePath(QPointF(-12 * std::cos(a), -12 * std::sin(a)), QPointF(12 * std::cos(a), 12 * std::sin(a)));
        }
        const Arrangement w(wheel);
        QCOMPARE(w.faceCount(), 6);
        QVERIFY(std::abs(areaOf(w.faceAt(QPointF(5 * std::cos(0.5), 5 * std::sin(0.5)))) - M_PI * 100 / 6) < 0.3);
    }

    void arrangementDanglingAndGaps()
    {
        using vek::pathops::Arrangement;
        // Linie, die im Rechteck endet, teilt nichts
        QList<SubPath> paths = rectPaths(QRectF(0, 0, 100, 50));
        paths << linePath(QPointF(20, 25), QPointF(50, 25));
        const Arrangement a(paths);
        QCOMPARE(a.faceCount(), 1);
        QVERIFY(std::abs(areaOf(a.faceAt(QPointF(30, 10))) - 5000) < 0.5);
        // Diagonale teilt in zwei Dreiecke
        paths << linePath(QPointF(0, 0), QPointF(100, 50));
        const Arrangement b(paths);
        QCOMPARE(b.faceCount(), 2);
        QVERIFY(std::abs(areaOf(b.faceAt(QPointF(80, 10))) - 2500) < 0.5);
        QVERIFY(std::abs(areaOf(b.faceAt(QPointF(10, 40))) - 2500) < 0.5);
        // Kleine Lücke an beiden Enden: innerhalb der Toleranz geschlossen, sonst nicht
        QList<SubPath> gap = rectPaths(QRectF(0, 0, 100, 50));
        gap << linePath(QPointF(50, 0.1), QPointF(50, 49.85));
        QVERIFY(std::abs(areaOf(Arrangement(gap, 0.25).faceAt(QPointF(25, 25))) - 2500) < 0.5);
        QVERIFY(std::abs(areaOf(Arrangement(gap, 0.0).faceAt(QPointF(25, 25))) - 5000) < 0.5);
        // Ecke aus zwei offenen Linien mit Lücke zwischen den Enden plus Rahmen
        QList<SubPath> corner = rectPaths(QRectF(0, 0, 100, 100));
        corner << linePath(QPointF(0, 60), QPointF(39.9, 60)) << linePath(QPointF(40, 60.1), QPointF(40, 100));
        QVERIFY(std::abs(areaOf(Arrangement(corner, 0.25).faceAt(QPointF(20, 80))) - 1600) < 1.0);
        // Gleiche Linie doppelt und zwei Rechtecke mit gemeinsamer Kante
        QList<SubPath> dup = rectPaths(QRectF(0, 0, 50, 50));
        dup << rectPaths(QRectF(50, 0, 50, 50)) << rectPaths(QRectF(0, 0, 50, 50)) << linePath(QPointF(0, 25), QPointF(50, 25))
            << linePath(QPointF(0, 25), QPointF(50, 25));
        const Arrangement d(dup);
        QVERIFY(std::abs(areaOf(d.faceAt(QPointF(75, 25))) - 2500) < 0.5);
        QVERIFY(std::abs(areaOf(d.faceAt(QPointF(25, 10))) - 1250) < 0.5);
    }

    void arrangementHolesAndRings()
    {
        using vek::pathops::Arrangement;
        // Quadrat mit zwei Kreisen darin (berühren nichts): Klick daneben -> Quadrat mit Loch
        QList<SubPath> paths = rectPaths(QRectF(0, 0, 100, 100));
        paths << circlePaths(QPointF(50, 50), 30) << circlePaths(QPointF(50, 50), 10);
        const Arrangement a(paths);
        const QList<SubPath> outer = a.faceAt(QPointF(5, 5));
        QCOMPARE(outer.size(), 2); // Rand + ein Loch (der innere Kreis liegt im äusseren)
        QVERIFY2(std::abs(areaOf(outer) - (10000 - M_PI * 900)) < 1.0, qPrintable(QString::number(areaOf(outer))));
        QVERIFY(!CurveObject::toPainterPath(outer).contains(QPointF(50, 50)));
        // Ring zwischen den Kreisen
        const QList<SubPath> ring = a.faceAt(QPointF(50, 25));
        QCOMPARE(ring.size(), 2);
        QVERIFY(std::abs(areaOf(ring) - M_PI * 800) < 1.0);
        QVERIFY(CurveObject::toPainterPath(ring).contains(QPointF(50, 25)));
        QVERIFY(!CurveObject::toPainterPath(ring).contains(QPointF(50, 50)));
        QVERIFY(std::abs(areaOf(a.faceAt(QPointF(50, 50))) - M_PI * 100) < 0.5);

        // Ring, von einer Linie in zwei Hälften geteilt (Buchstabe "G")
        QList<SubPath> g = circlePaths(QPointF(50, 50), 30);
        g << circlePaths(QPointF(50, 50), 15) << linePath(QPointF(10, 50), QPointF(90, 50));
        const Arrangement ga(g);
        const QList<SubPath> half = ga.faceAt(QPointF(50, 27));
        QCOMPARE(half.size(), 1);
        QVERIFY2(std::abs(areaOf(half) - M_PI * (900 - 225) / 2) < 1.0, qPrintable(QString::number(areaOf(half))));

        // Steg: ein Kreis im Quadrat, durch eine Linie mit dem Rand verbunden -> Loch trotzdem frei
        QList<SubPath> bridge = rectPaths(QRectF(0, 0, 100, 100));
        bridge << circlePaths(QPointF(50, 50), 10) << linePath(QPointF(0, 50), QPointF(40, 50));
        const QList<SubPath> around = Arrangement(bridge).faceAt(QPointF(80, 80));
        QVERIFY2(std::abs(areaOf(around) - (10000 - M_PI * 100)) < 1.0, qPrintable(QString::number(areaOf(around))));
        QVERIFY(!CurveObject::toPainterPath(around).contains(QPointF(50, 50)));
    }

    void arrangementCurvedBoundaries()
    {
        using vek::pathops::Arrangement;
        // Rechteck, von einer S-Kurve (offen, Bézier) in zwei gleiche Hälften geteilt
        QList<SubPath> paths = rectPaths(QRectF(0, 0, 100, 60));
        SubPath wave;
        wave.nodes << CurveNode{QPointF(-10, 30)} << CurveNode{QPointF(110, 30), true, QPointF(40, -20), QPointF(60, 80)};
        paths << wave;
        const Arrangement a(paths);
        QCOMPARE(a.faceCount(), 2);
        const QList<SubPath> top = a.faceAt(QPointF(90, 5));
        QVERIFY2(std::abs(areaOf(top) - 3000) < 1.0, qPrintable(QString::number(areaOf(top))));
        QVERIFY(std::any_of(top.first().nodes.begin(), top.first().nodes.end(), [](const CurveNode& n) { return n.curveIn; }));
        QVERIFY(top.first().nodes.size() <= 8);
        // Zwei sich kreuzende Wellen und ein Kreis darüber: jeder Punkt liegt in einer Masche,
        // die Flächen aller Maschen ergeben zusammen das Rechteck
        QList<SubPath> mix = rectPaths(QRectF(0, 0, 100, 60));
        SubPath wave2;
        wave2.nodes << CurveNode{QPointF(-10, 20)} << CurveNode{QPointF(110, 40), true, QPointF(30, 90), QPointF(70, -30)};
        mix << wave << wave2 << circlePaths(QPointF(50, 30), 12);
        const Arrangement m(mix);
        QVERIFY(m.faceCount() >= 8);
        double total = 0;
        QList<QPainterPath> seen;
        for (int y = 1; y < 60; y += 2)
            for (int x = 1; x < 100; x += 2) {
                const QPointF p(x + 0.37, y + 0.21);
                if (std::any_of(seen.begin(), seen.end(), [&p](const QPainterPath& s) { return s.contains(p); }))
                    continue;
                const QList<SubPath> f = m.faceAt(p);
                QVERIFY2(!f.isEmpty(), qPrintable(QStringLiteral("%1,%2").arg(x).arg(y)));
                seen << CurveObject::toPainterPath(f);
                total += areaOf(f);
            }
        QVERIFY2(std::abs(total - 6000) < 3.0, qPrintable(QString::number(total)));
        // Viele Linien und Kreise (die Linien laufen alle durch einen Punkt): schnell genug
        QList<SubPath> many = rectPaths(QRectF(0, 0, 200, 200));
        for (int k = 0; k < 150; ++k) {
            many << linePath(QPointF(0, k * 1.3 + 2), QPointF(200, 200 - k * 1.1));
            many << circlePaths(QPointF(10 + k, 100 + (k % 7) * 10), 8);
        }
        QElapsedTimer timer;
        timer.start();
        const Arrangement big(many);
        QVERIFY(!big.faceAt(QPointF(150, 30)).isEmpty());
        QVERIFY2(timer.elapsed() < 2000, qPrintable(QString::number(timer.elapsed())));
    }

    // --- Mischmodi, Innenschatten, gedrehte Extrusion -----------------------------------------
    void blendModeMath()
    {
        using blending::composite;
        auto near = [](QRgb a, QRgb b) {
            return std::abs(qRed(a) - qRed(b)) <= 1 && std::abs(qGreen(a) - qGreen(b)) <= 1 && std::abs(qBlue(a) - qBlue(b)) <= 1
                   && std::abs(qAlpha(a) - qAlpha(b)) <= 1;
        };
        auto text = [](QRgb c) { return QStringLiteral("(%1,%2,%3,%4)").arg(qRed(c)).arg(qGreen(c)).arg(qBlue(c)).arg(qAlpha(c)); };
        const QRgb back = qRgb(200, 100, 50);
        struct Case {
            BlendMode mode;
            QRgb source;
            QRgb expected;
        };
        const Case cases[] = {
            {BlendMode::Normal, qRgb(10, 20, 30), qRgb(10, 20, 30)},
            {BlendMode::Multiply, qRgb(128, 255, 0), qRgb(100, 100, 0)},
            {BlendMode::Screen, qRgb(100, 100, 100), qRgb(222, 161, 130)},
            {BlendMode::Darken, qRgb(100, 150, 25), qRgb(100, 100, 25)},
            {BlendMode::Lighten, qRgb(100, 150, 25), qRgb(200, 150, 50)},
            {BlendMode::Difference, qRgb(50, 150, 25), qRgb(150, 50, 25)},
            {BlendMode::Exclusion, qRgb(255, 0, 255), qRgb(55, 100, 205)},
            {BlendMode::Add, qRgb(100, 100, 100), qRgb(255, 200, 150)},
            {BlendMode::Subtract, qRgb(50, 150, 25), qRgb(150, 0, 25)},
            {BlendMode::Divide, qRgb(255, 200, 100), qRgb(200, 128, 128)},
            {BlendMode::Red, qRgb(7, 8, 9), qRgb(7, 100, 50)},
            {BlendMode::Blue, qRgb(7, 8, 9), qRgb(200, 100, 9)},
            {BlendMode::And, qRgb(0xf0, 0x0f, 0xff), qRgb(200 & 0xf0, 100 & 0x0f, 50)},
            {BlendMode::Xor, qRgb(0xff, 0, 0), qRgb(200 ^ 0xff, 100, 50)},
            {BlendMode::Invert, qRgb(1, 2, 3), qRgb(55, 155, 205)},
            {BlendMode::Overlay, qRgb(128, 128, 128), qRgb(200, 100, 50)},
            {BlendMode::HardLight, qRgb(0, 255, 128), qRgb(0, 255, 50)},
        };
        for (const Case& c : cases) {
            const QRgb got = composite(c.mode, back, c.source);
            QVERIFY2(near(got, c.expected), qPrintable(blending::id(c.mode) + QLatin1Char(' ') + text(got) + QStringLiteral(" != ") + text(c.expected)));
        }
        // Nicht trennbar: Farbe/Helligkeit tauschen Farbton und Helligkeit (W3C)
        const QRgb gray = qRgb(128, 128, 128), red = qRgb(255, 0, 0);
        QVERIFY2(near(composite(BlendMode::Color, gray, red), qRgb(255, 74, 74)), qPrintable(text(composite(BlendMode::Color, gray, red))));
        QVERIFY2(near(composite(BlendMode::Luminosity, red, gray), qRgb(255, 74, 74)), qPrintable(text(composite(BlendMode::Luminosity, red, gray))));
        // Farbton von Rot auf grünem Untergrund: Rot mit Sättigung und Helligkeit des Grüns
        const QRgb hue = composite(BlendMode::Hue, qRgb(0, 128, 0), red);
        QVERIFY2(qRed(hue) > 100 && qGreen(hue) < 40 && qBlue(hue) < 40, qPrintable(text(hue)));
        // Sättigung von Grau: Untergrund wird grau
        const QRgb sat = composite(BlendMode::Saturation, qRgb(200, 50, 50), gray);
        QVERIFY2(qRed(sat) == qGreen(sat) && qGreen(sat) == qBlue(sat), qPrintable(text(sat)));
        // Halbdurchsichtige Quelle (vormultipliziert): Multiplizieren zur Hälfte
        const QRgb half = composite(BlendMode::Multiply, qRgb(255, 255, 255), qRgba(64, 0, 0, 128));
        QVERIFY2(near(half, qRgb(191, 127, 127)), qPrintable(text(half)));
        // Dahinter: Objekt nur, wo der Untergrund durchsichtig ist
        QCOMPARE(composite(BlendMode::Behind, back, qRgb(0, 0, 255)), back);
        QVERIFY(near(composite(BlendMode::Behind, 0u, qRgb(0, 0, 255)), qRgb(0, 0, 255)));
        // layerPixel normal darübergelegt ergibt dasselbe wie composite, für alle Modi
        auto over = [](QRgb dst, QRgb src) {
            const double k = 1.0 - qAlpha(src) / 255.0;
            return qRgba(int(std::lround(qRed(src) + qRed(dst) * k)), int(std::lround(qGreen(src) + qGreen(dst) * k)),
                         int(std::lround(qBlue(src) + qBlue(dst) * k)), int(std::lround(qAlpha(src) + qAlpha(dst) * k)));
        };
        for (BlendMode m : blending::all())
            for (QRgb src : {qRgb(30, 200, 90), qRgba(20, 60, 10, 128), qRgba(0, 0, 0, 0)})
                for (QRgb dst : {back, qRgba(50, 50, 0, 100), qRgb(255, 255, 255)}) {
                    const QRgb a = over(dst, blending::layerPixel(m, dst, src)), b = composite(m, dst, src);
                    QVERIFY2(std::abs(qRed(a) - qRed(b)) <= 2 && std::abs(qGreen(a) - qGreen(b)) <= 2 && std::abs(qBlue(a) - qBlue(b)) <= 2
                                 && std::abs(qAlpha(a) - qAlpha(b)) <= 2,
                             qPrintable(blending::id(m) + QLatin1Char(' ') + text(a) + QStringLiteral(" != ") + text(b)));
                }
        // Kennungen eindeutig, Hin und zurück
        QSet<QString> ids;
        for (BlendMode m : blending::all()) {
            QCOMPARE(blending::fromId(blending::id(m)), std::optional<BlendMode>(m));
            ids.insert(blending::id(m));
            QVERIFY(!blending::label(m).isEmpty());
        }
        QCOMPARE(ids.size(), int(blending::all().size()));
        QVERIFY(blending::all().size() >= 27);
        QCOMPARE(blending::cssName(BlendMode::Multiply), QStringLiteral("multiply"));
        // Ganzes Bild
        QImage src(2, 1, QImage::Format_ARGB32_Premultiplied), dst(2, 1, QImage::Format_ARGB32_Premultiplied);
        src.setPixel(0, 0, qRgb(128, 255, 0));
        src.setPixel(1, 0, 0);
        dst.fill(back);
        const QImage l = blending::layer(BlendMode::Multiply, src, dst);
        QVERIFY(near(l.pixel(0, 0), qRgb(100, 100, 0)));
        QCOMPARE(qAlpha(l.pixel(1, 0)), 0);
    }

    void blendAlongPath()
    {
        // Zwei Kreise mit radialem Verlauf, überblendet entlang eines Bogens
        auto circle = [](const QPointF& at, const Color& inner) {
            auto e = std::make_unique<EllipseObject>(QSizeF(10, 10));
            e->setTransform(QTransform::fromTranslate(at.x() - 5, at.y() - 5));
            Style s;
            s.fill = Fill::gradient(Fill::Type::Radial, inner, Color(0, 40, 60));
            s.stroke.reset();
            e->setStyle(s);
            return e;
        };
        BlendObject blend;
        blend.setSteps(9);
        attachDetachedChild(blend, circle(QPointF(0, 0), Color(255, 255, 255)));
        attachDetachedChild(blend, circle(QPointF(100, 0), Color(0, 200, 220)));
        QPainterPath arc(QPointF(0, 0));
        arc.cubicTo(QPointF(0, 80), QPointF(100, 80), QPointF(100, 0));
        // Ohne Pfad: Stufen auf der Geraden, Verlauf bleibt Verlauf
        QList<std::shared_ptr<CurveObject>> steps = blend.intermediates();
        QCOMPARE(steps.size(), 9);
        QVERIFY(std::abs(steps[4]->boundingRect().center().y()) < 0.5);
        QCOMPARE(steps[4]->style().fill.type, Fill::Type::Radial);
        QCOMPARE(steps[4]->style().fill.stops.first().color, Color(128, 228, 238));
        QCOMPARE(steps[4]->style().fill.stops.last().color, Color(0, 40, 60));
        // Mit Pfad: Mittelpunkte liegen auf dem Bogen
        attachDetachedChild(blend, CurveObject::fromPath(arc));
        QVERIFY(blend.pathObject());
        steps = blend.intermediates();
        for (const auto& s : std::as_const(steps)) {
            const QPointF c = s->boundingRect().center();
            double best = 1e9;
            for (int i = 0; i <= 400; ++i)
                best = std::min(best, QLineF(arc.pointAtPercent(i / 400.0), c).length());
            QVERIFY2(best < 0.6, qPrintable(QStringLiteral("%1, %2: %3").arg(c.x()).arg(c.y()).arg(best)));
        }
        QVERIFY(steps[4]->boundingRect().center().y() > 50);
        // Alle Objekte drehen: Stufe in der Mitte (Tangente waagrecht, am Anfang senkrecht) um 90° gedreht
        blend.setRotateAlongPath(true);
        QVERIFY(blend.intermediates()[4]->style().fill.isGradient());
        // Über den ganzen Pfad: Start und Ende springen an die Pfadenden
        blend.setPropertyValue(QStringLiteral("fullPath"), true);
        QVERIFY(QLineF(blend.start()->boundingRect().center(), arc.pointAtPercent(0)).length() < 1e-6);
        QVERIFY(QLineF(blend.end()->boundingRect().center(), arc.pointAtPercent(1)).length() < 1e-6);
        // Reihenfolge umkehren: Ende zuunterst
        QCOMPARE(blend.paintOrder().first(), blend.start());
        blend.setReversed(true);
        QCOMPARE(blend.paintOrder().first(), blend.end());
        QCOMPARE(blend.paintOrder().size(), 11);
        // Speichern und Laden
        auto back = serial::fromJson(serial::toJson(blend), serial::Ids::Drop);
        const auto* b = node_cast<BlendObject>(back.get());
        QVERIFY(b && b->pathObject());
        QVERIFY(b->fullPath() && b->rotateAlongPath() && b->reversed());
        QCOMPARE(b->intermediates().size(), 9);
        // Verlauf gegen gleichmässige Farbe: Verlauf mit überblendeten Stopps
        const Fill mixed = BlendObject::mixFill(Fill::gradient(Fill::Type::Linear, Color(0, 0, 0), Color(255, 255, 255)), Fill::solid(Color(255, 0, 0)), 0.5);
        QCOMPARE(mixed.type, Fill::Type::Linear);
        QCOMPARE(mixed.stops.first().color, Color(128, 0, 0));
        QCOMPARE(mixed.stops.last().color, Color(255, 128, 128));
    }

    void embossKeepsTransparency()
    {
        QImage img(40, 40, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter(&img).fillRect(QRect(10, 10, 20, 20), QColor(200, 30, 30));
        for (int mode = 0; mode < 4; ++mode) {
            const QImage out = filters::embossColored(img, 20, 135, mode, Color(0, 0, 255)).convertToFormat(QImage::Format_ARGB32);
            QCOMPARE(qAlpha(out.pixel(2, 2)), 0);
            QCOMPARE(qAlpha(out.pixel(20, 20)), 255);
            // Kanten oben links und unten rechts unterschiedlich hell
            QVERIFY(qGray(out.pixel(10, 20)) != qGray(out.pixel(29, 20)));
        }
        // Originalfarben: Mitte bleibt rot
        const QImage original = filters::embossColored(img, 20, 135, 0, Color()).convertToFormat(QImage::Format_ARGB32);
        QCOMPARE(QColor(original.pixel(20, 20)), QColor(200, 30, 30));
        // Alle Effekte der Menügruppen haben eine Definition und laufen auf einem kleinen Bild
        for (const fx::Definition& def : fx::definitions())
            if (QStringList{QStringLiteral("color"), QStringLiteral("contour"), QStringLiteral("distort"), QStringLiteral("noise"), QStringLiteral("sharpen")}.contains(def.group)
                || def.id == QLatin1String("3d.emboss") || def.id == QLatin1String("3d.pinch") || def.id.startsWith(QLatin1String("blur.")))
                QVERIFY2(!fx::apply(img, fx::defaults(def), 1.0).isNull(), qPrintable(def.id));
    }

    void blendShadowExtrusionRoundTrip()
    {
        RectangleObject r(QSizeF(20, 10));
        r.setBlendMode(BlendMode::Overlay);
        DropShadow s;
        QCOMPARE(s.blend, BlendMode::Multiply); // neue Schatten multiplizieren
        s.active = true;
        s.inner = true;
        s.offset = QPointF(1.5, -1.5);
        s.blend = BlendMode::Screen;
        r.setShadow(s);
        Extrusion e;
        e.active = true;
        e.rotX = 20;
        e.rotY = -35;
        e.rotZ = 10;
        r.setExtrusion(e);
        auto back = serial::fromJson(serial::toJson(r), serial::Ids::Keep);
        const auto* o = node_cast<DrawObject>(back.get());
        QVERIFY(o);
        QCOMPARE(o->blendMode(), BlendMode::Overlay);
        QCOMPARE(o->shadow(), s);
        QCOMPARE(o->extrusion(), e);
        QVERIFY(o->hasEffects());
        // Ältere Dateien: ohne Angabe Normal, auch der Schatten; keine Drehung
        QJsonObject json = serial::toJson(r);
        json.remove(QStringLiteral("blend"));
        QJsonObject sj = json.value(QStringLiteral("shadow")).toObject();
        sj.remove(QStringLiteral("blend"));
        sj.remove(QStringLiteral("inner"));
        json[QStringLiteral("shadow")] = sj;
        QJsonObject ej = json.value(QStringLiteral("extrusion")).toObject();
        for (const char* k : {"rx", "ry", "rz"})
            ej.remove(QLatin1String(k));
        json[QStringLiteral("extrusion")] = ej;
        auto old = serial::fromJson(json, serial::Ids::Keep);
        const auto* oo = node_cast<DrawObject>(old.get());
        QCOMPARE(oo->blendMode(), BlendMode::Normal);
        QCOMPARE(oo->shadow().blend, BlendMode::Normal);
        QVERIFY(!oo->shadow().inner);
        QVERIFY(!oo->extrusion().isRotated());
        // Normal wird nicht geschrieben
        RectangleObject plain(QSizeF(5, 5));
        QVERIFY(!serial::toJson(plain).contains(QStringLiteral("blend")));
        // Effekt löschen setzt den Mischmodus zurück
        r.clearEffects();
        QCOMPARE(r.blendMode(), BlendMode::Normal);
    }

    void extrusionRotationGeometry()
    {
        const QPointF c(50, 50);
        Extrusion e;
        e.active = true;
        e.vanish = QPointF(30, -40); // 50 mm entfernt
        e.depth = 40;
        // Ohne Drehung wie bisher: Rückseite 40 % des Wegs zum Fluchtpunkt, Vorderfläche unverändert
        const QPointF p(60, 45);
        const QPointF vp = c + e.vanish;
        QVERIFY(QLineF(e.project(c, p, 1.0), p + (vp - p) * 0.4).length() < 1e-9);
        QVERIFY(QLineF(e.project(c, p, 0.0), p).length() < 1e-9);
        QVERIFY(e.faceTransform(c).isIdentity());
        e.parallel = true;
        QVERIFY(QLineF(e.project(c, p, 1.0), p + e.vanish * 0.4).length() < 1e-9);
        e.parallel = false;
        // z: 90° im Uhrzeigersinn um die Mitte
        e.rotZ = 90;
        QVERIFY(QLineF(e.project(c, QPointF(60, 50), 0.0), QPointF(50, 60)).length() < 1e-9);
        QVERIFY(QLineF(e.faceTransform(c).map(QPointF(60, 50)), QPointF(50, 60)).length() < 1e-9);
        // x: Oberkante kippt nach hinten (Tiefe positiv), Unterkante nach vorn
        e.rotZ = 0;
        e.rotX = 40;
        double zTop = 0, zBottom = 0;
        e.project(c, QPointF(50, 40), 0.0, &zTop);
        e.project(c, QPointF(50, 60), 0.0, &zBottom);
        QVERIFY(zTop > 0 && zBottom < 0);
        // Die projektive Abbildung der Vorderfläche stimmt mit der Projektion überein
        e.rotY = -25;
        const QTransform face = e.faceTransform(c);
        QVERIFY(!face.isAffine());
        for (const QPointF& q : {QPointF(40, 40), QPointF(62, 41), QPointF(55, 58)})
            QVERIFY2(QLineF(face.map(q), e.project(c, q, 0.0)).length() < 1e-6, qPrintable(QStringLiteral("%1 %2").arg(q.x()).arg(q.y())));
        // Parallel: affin, ebenso gleich
        e.parallel = true;
        const QTransform pf = e.faceTransform(c);
        QVERIFY(pf.isAffine());
        for (const QPointF& q : {QPointF(40, 40), QPointF(62, 41)})
            QVERIFY(QLineF(pf.map(q), e.project(c, q, 0.0)).length() < 1e-6);
        // y um 90°: die Fläche steht hochkant, ihre Breite verschwindet (parallel)
        e.rotX = 0;
        e.rotY = 90;
        const QRectF edgeOn = e.faceTransform(c).mapRect(QRectF(40, 40, 20, 20));
        const QPointF dir = e.vanish * 0.4 / e.bodyDepth();
        QVERIFY2(edgeOn.width() <= std::abs(dir.x()) * 20 + 1e-6, qPrintable(QString::number(edgeOn.width())));
    }
    // Mehrseitenansicht: Raster, Zeile, Spalte und Doppelseiten
    void multiPageLayout()
    {
        using namespace pagelayout;
        const QList<QSizeF> a4(8, QSizeF(210, 297));
        Settings s; // Raster, 4 Spalten, 40 mm
        QList<QRectF> r = arrange(a4, s);
        QCOMPARE(r.size(), 8);
        QCOMPARE(r[0], QRectF(0, 0, 210, 297));
        QCOMPARE(r[3].topLeft(), QPointF(3 * 250, 0));
        QCOMPARE(r[4].topLeft(), QPointF(0, 337));
        s.columns = 3;
        s.spacing = 5;
        r = arrange(a4, s);
        QCOMPARE(r[3].topLeft(), QPointF(0, 302));
        QCOMPARE(r[7].topLeft(), QPointF(215, 604));
        s.arrangement = Arrangement::Horizontal;
        r = arrange(a4, s);
        QCOMPARE(r[7].topLeft(), QPointF(7 * 215, 0));
        s.arrangement = Arrangement::Vertical;
        r = arrange(a4, s);
        QCOMPARE(r[7].topLeft(), QPointF(0, 7 * 302));
        // Verschiedene Grössen: Zelle so gross wie der grösste Inhalt, Seite waagrecht mittig
        const QList<QSizeF> mixed{QSizeF(420, 594), QSizeF(85, 55), QSizeF(148, 210)};
        s.arrangement = Arrangement::Grid;
        s.columns = 2;
        s.spacing = 10;
        r = arrange(mixed, s);
        QCOMPARE(r[1].topLeft(), QPointF(430, 0));
        QCOMPARE(r[2].topLeft(), QPointF(136, 604));
        // Doppelseiten, Beginn rechts: Seite 1 allein rechts, dann Bögen (2,3), (4,5) ohne Abstand
        s = Settings{};
        s.spacing = 10;
        s.facing = true;
        s.arrangement = Arrangement::Vertical;
        r = arrange(QList<QSizeF>(5, QSizeF(100, 200)), s);
        QCOMPARE(r[0].topLeft(), QPointF(100, 0));
        QCOMPARE(r[1].topLeft(), QPointF(0, 210));
        QCOMPARE(r[2].topLeft(), QPointF(100, 210));
        QCOMPARE(r[4].topLeft(), QPointF(100, 420));
        QCOMPARE(spreads(5, true, false), (QList<QList<int>>{{0, 1}, {2, 3}, {4}}));
        // Raster mit Doppelseiten: Spaltenzahl zählt Seiten, also zwei Bögen je Zeile
        s.arrangement = Arrangement::Grid;
        s.startRight = false;
        r = arrange(QList<QSizeF>(6, QSizeF(100, 200)), s);
        QCOMPARE(r[1].topLeft(), QPointF(100, 0));
        QCOMPARE(r[2].topLeft(), QPointF(210, 0));
        QCOMPARE(r[4].topLeft(), QPointF(0, 210));
    }

    // Reihenfolge und Namen der Seiten überstehen Speichern und Laden
    void pageOrderAndNamesRoundTrip()
    {
        Document doc;
        {
            Transaction tx(doc, QStringLiteral("Seiten"));
            tx.insert(doc.root().id(), -1, createPage(QSizeF(100, 50), QStringLiteral("Ebene 1")));
            tx.insert(doc.root().id(), -1, createPage(QSizeF(80, 80), QStringLiteral("Ebene 1")));
            tx.commit();
        }
        const NodeId first = doc.page(0).id();
        const NodeId last = doc.page(2).id();
        {
            Transaction tx(doc, QStringLiteral("Umbenennen und verschieben"));
            tx.modify<Page>(first, [](Page& p) { p.setName(QStringLiteral("Titel")); });
            tx.modify<Page>(last, [](Page& p) { p.setName(QStringLiteral("Rückseite")); });
            tx.move(last, doc.root().id(), 1); // nach vorn (nach der Masterseite)
            tx.commit();
        }
        QCOMPARE(doc.page(0).id(), last);
        QString error;
        const QByteArray bytes = io::saveToBytes(doc, &error);
        QVERIFY2(!bytes.isEmpty(), qPrintable(error));
        auto loaded = io::loadFromBytes(bytes, &error);
        QVERIFY2(loaded, qPrintable(error));
        QCOMPARE(loaded->pageCount(), 3);
        QCOMPARE(loaded->page(0).name(), QStringLiteral("Rückseite"));
        QCOMPARE(loaded->page(0).size(), QSizeF(80, 80));
        QCOMPARE(loaded->page(1).name(), QStringLiteral("Titel"));
        QCOMPARE(loaded->page(2).size(), QSizeF(100, 50));
        doc.undo();
        QCOMPARE(doc.page(0).id(), first);
        QVERIFY(doc.page(0).name().isEmpty());
    }

    // --- Bitmap-Effekte: Seite aufrollen, Zylinder, weitere Korrekturen ---------------------

    void bitmapCorrectionsMath()
    {
        // Testbild: Verlauf mit Farbflächen, halbtransparente Ecke
        QImage img(64, 48, QImage::Format_RGBA64);
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
                img.setPixelColor(x, y, QColor((x * 4) % 256, (y * 5) % 256, ((x / 8 + y / 8) % 2) * 180 + 30, x < 4 && y < 4 ? 128 : 255));
        const QImage img16 = img.convertToFormat(QImage::Format_RGBA64);

        // Kanalmixer: Grundeinstellung = unverändert, Rot und Blau tauschen
        QCOMPARE(adjust::channelMixer(img, adjust::ChannelMixer()), img16);
        adjust::ChannelMixer swap;
        swap.rows = {{{0, 0, 100, 0}, {0, 100, 0, 0}, {100, 0, 0, 0}}};
        const QImage swapped = adjust::channelMixer(img, swap);
        QCOMPARE(swapped.format(), QImage::Format_RGBA64);
        for (const QPoint& p : {QPoint(10, 10), QPoint(40, 30), QPoint(2, 2)}) {
            const QRgba64 a = reinterpret_cast<const QRgba64*>(img16.constScanLine(p.y()))[p.x()];
            const QRgba64 b = reinterpret_cast<const QRgba64*>(swapped.constScanLine(p.y()))[p.x()];
            QCOMPARE(b.red(), a.blue());
            QCOMPARE(b.blue(), a.red());
            QCOMPARE(b.green(), a.green());
            QCOMPARE(b.alpha(), a.alpha()); // Deckkraft bleibt
        }
        adjust::ChannelMixer constant;
        constant.rows[1] = {0, 0, 0, 50};
        QCOMPARE(adjust::channelMixer(img, constant).pixelColor(30, 30).green(), 128);

        // Selektive Farbe: alles 0 = unverändert; Rottöne mit +Cyan dämpfen nur Rot
        QCOMPARE(adjust::selectiveColor(img, adjust::SelectiveColor()), img16);
        QImage swatches(3, 1, QImage::Format_RGBA64);
        swatches.setPixelColor(0, 0, QColor(220, 40, 30));
        swatches.setPixelColor(1, 0, QColor(30, 60, 220));
        swatches.setPixelColor(2, 0, QColor(128, 128, 128));
        adjust::SelectiveColor reds;
        reds.cmyk[int(adjust::ColorRange::Reds)] = {60, 0, 0, 0};
        const QImage sel = adjust::selectiveColor(swatches, reds);
        QVERIFY(sel.pixelColor(0, 0).red() < 180);
        QCOMPARE(sel.pixelColor(1, 0), swatches.pixelColor(1, 0));
        QCOMPARE(sel.pixelColor(2, 0), swatches.pixelColor(2, 0));
        QCOMPARE(adjust::colorRangeWeight(1, 1, 0, adjust::ColorRange::Yellows), 1.0);
        QCOMPARE(adjust::colorRangeWeight(1, 1, 0, adjust::ColorRange::Reds), 0.0);
        adjust::SelectiveColor whites;
        whites.relative = false;
        whites.cmyk[int(adjust::ColorRange::Whites)] = {0, 0, 40, 0};
        QImage white(1, 1, QImage::Format_RGBA64);
        white.fill(Qt::white);
        QVERIFY(adjust::selectiveColor(white, whites).pixelColor(0, 0).blue() < 180);

        // Farben ersetzen: nur der Bereich um die alte Farbe ändert sich
        QImage two(20, 10, QImage::Format_RGBA64);
        two.fill(QColor(200, 30, 30));
        for (int y = 0; y < 10; ++y)
            for (int x = 10; x < 20; ++x)
                two.setPixelColor(x, y, QColor(40, 180, 60));
        adjust::ReplaceColors rc;
        rc.oldColor = Color(200, 30, 30);
        rc.newColor = Color(30, 30, 200);
        rc.range = 20;
        const QImage replaced = adjust::replaceColors(two, rc);
        QVERIFY(replaced.pixelColor(3, 3).blue() > 150);
        QVERIFY(replaced.pixelColor(3, 3).red() < 80);
        QCOMPARE(replaced.pixelColor(15, 5), two.pixelColor(15, 5));
        // Farbton zusätzlich drehen: neue Farbe Blau + 120 Grad = Rot
        rc.hue = 120;
        QVERIFY(adjust::replaceColors(two, rc).pixelColor(3, 3).red() > 150);

        // Lokaler Ausgleich: zwei flaue Hälften (dunkel/hell mit schwacher Struktur) werden kontrastreicher
        QImage flat(80, 40, QImage::Format_RGBA64);
        for (int y = 0; y < flat.height(); ++y)
            for (int x = 0; x < flat.width(); ++x) {
                const int base = x < 40 ? 40 : 200;
                const int bump = (x + y) % 25; // schwache, gleichmässig verteilte Struktur
                flat.setPixelColor(x, y, QColor(base + bump, base + bump, base));
            }
        auto spread = [](const QImage& i, int x0) {
            double lo = 1e9, hi = -1e9;
            for (int y = 10; y < 30; ++y)
                for (int x = x0; x < x0 + 20; ++x) {
                    const double v = i.pixelColor(x, y).greenF();
                    lo = std::min(lo, v);
                    hi = std::max(hi, v);
                }
            return hi - lo;
        };
        const QImage eq = adjust::localEqualize(flat, 20, 20);
        QCOMPARE(eq.size(), flat.size());
        QVERIFY2(spread(eq, 10) > spread(flat, 10) * 2, qPrintable(QString::number(spread(eq, 10))));
        QVERIFY(spread(eq, 50) > spread(flat, 50) * 2);
        QCOMPARE(adjust::localEqualize(img, 16, 16).pixelColor(1, 1).alpha(), 128);

        // Proben-/Zielabgleich: Ziel = Probe ist unverändert; ein Farbstich in den Mitteltönen verschwindet
        const std::array<Color, 3> plain{Color(0, 0, 0), Color(128, 128, 128), Color(255, 255, 255)};
        QCOMPARE(adjust::sampleTargetBalance(img, plain, plain), img16);
        QImage cast(4, 1, QImage::Format_RGBA64);
        cast.fill(QColor(150, 128, 110));
        const QImage balanced = adjust::sampleTargetBalance(
            cast, {Color(0, 0, 0), Color(150, 128, 110), Color(255, 255, 255)}, {Color(0, 0, 0), Color(130, 130, 130), Color(255, 255, 255)});
        const QColor bc = balanced.pixelColor(1, 0);
        QVERIFY(std::abs(bc.red() - 130) <= 1 && std::abs(bc.green() - 130) <= 1 && std::abs(bc.blue() - 130) <= 1);
    }

    void bitmapPageCurlAndCylinder()
    {
        QImage img(120, 90, QImage::Format_RGBA64);
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
                img.setPixelColor(x, y, QColor((x * 2) % 256, (y * 3) % 256, ((x / 10 + y / 10) % 2) * 200));
        const QImage img16 = img.convertToFormat(QImage::Format_RGBA64);

        // Zylinder 0 % = unverändert; sonst verschoben, aber nur in der gewählten Richtung
        QCOMPARE(filters::cylinder(img, 0, false), img16);
        QCOMPARE(filters::cylinder(img, 0, true), img16);
        const QImage convex = filters::cylinder(img, 80, false);
        QCOMPARE(convex.size(), img.size());
        QVERIFY(convex.pixelColor(30, 40) != img.pixelColor(30, 40));
        QCOMPARE(convex.pixelColor(50, 40).green(), img.pixelColor(50, 40).green());
        const QImage concave = filters::cylinder(img, -80, true);
        QVERIFY(concave.pixelColor(60, 20) != img.pixelColor(60, 20));
        QCOMPARE(concave.pixelColor(60, 20).red(), img.pixelColor(60, 20).red());

        // Seite aufrollen unten rechts: Gegenecke oben links unberührt, Ecke zeigt den Hintergrund
        filters::PageCurl curl;
        curl.corner = filters::PageCurl::BottomRight;
        curl.background = Color(10, 200, 20);
        curl.curlColor = Color(250, 240, 200);
        const QImage curled = filters::pageCurl(img, curl);
        QCOMPARE(curled.size(), img.size());
        for (const QPoint& p : {QPoint(0, 0), QPoint(5, 5), QPoint(40, 30), QPoint(119, 0), QPoint(0, 89)})
            QCOMPARE(curled.pixelColor(p), img16.pixelColor(p));
        QCOMPARE(curled.pixelColor(119, 89).rgb(), QColor(10, 200, 20).rgb());
        // Rolle in Rollenfarbe (schattiert) zwischen Ecke und Falz
        bool rollSeen = false;
        for (int i = 0; i < 60 && !rollSeen; ++i) {
            const QColor c = curled.pixelColor(119 - i, 89 - i * 90 / 120);
            rollSeen = c.red() > 120 && std::abs(c.red() - c.green()) < 40 && c.blue() < c.green() && c.green() > 100 && c.red() > c.blue();
        }
        QVERIFY(rollSeen);
        // Durchsichtiger Hintergrund, andere Ecke und Richtung
        curl.corner = filters::PageCurl::TopLeft;
        curl.vertical = false;
        curl.transparentBackground = true;
        curl.opaque = false;
        const QImage clear = filters::pageCurl(img, curl);
        QCOMPARE(clear.pixelColor(0, 0).alpha(), 0);
        QCOMPARE(clear.pixelColor(119, 89), img16.pixelColor(119, 89));

        // Sichtprüfung am Foto
        const QString shots = qEnvironmentVariable("VEK_SCREENSHOT_DIR");
        const QString photo = QStringLiteral("C:/Windows/Web/Wallpaper/ThemeC/img29.jpg");
        if (!shots.isEmpty() && QFile::exists(photo)) {
            const QImage p = QImage(photo).scaledToWidth(900, Qt::SmoothTransformation);
            filters::PageCurl pc;
            pc.curlColor = Color(245, 240, 225);
            pc.background = Color(40, 40, 48);
            filters::pageCurl(p, pc).save(shots + QStringLiteral("/fx-pagecurl-br.png"));
            pc.corner = filters::PageCurl::TopLeft;
            pc.vertical = false;
            pc.opaque = false;
            pc.width = 40;
            pc.height = 70;
            filters::pageCurl(p, pc).save(shots + QStringLiteral("/fx-pagecurl-tl-transparent.png"));
            filters::cylinder(p, 70, false).save(shots + QStringLiteral("/fx-cylinder-plus70.png"));
            filters::cylinder(p, -70, true).save(shots + QStringLiteral("/fx-cylinder-minus70-vertical.png"));
            adjust::localEqualize(p, 100, 100).save(shots + QStringLiteral("/fx-local-equalize.png"));
            adjust::SelectiveColor sc;
            sc.cmyk[int(adjust::ColorRange::Blues)] = {0, 60, 0, 0};
            sc.cmyk[int(adjust::ColorRange::Cyans)] = {0, 60, 0, 0};
            adjust::selectiveColor(p, sc).save(shots + QStringLiteral("/fx-selective-color.png"));
            adjust::ReplaceColors rc2;
            const QColor sample = p.pixelColor(p.width() / 2, p.height() / 3);
            rc2.oldColor = Color(quint8(sample.red()), quint8(sample.green()), quint8(sample.blue()));
            rc2.newColor = Color(200, 60, 160);
            rc2.range = 40;
            adjust::replaceColors(p, rc2).save(shots + QStringLiteral("/fx-replace-colors.png"));
            adjust::ChannelMixer cm;
            cm.rows = {{{0, 0, 100, 0}, {0, 100, 0, 0}, {100, 0, 0, 0}}};
            adjust::channelMixer(p, cm).save(shots + QStringLiteral("/fx-channel-mixer.png"));
            adjust::sampleTargetBalance(p, {Color(20, 20, 30), Color(110, 120, 140), Color(230, 235, 245)},
                                        {Color(0, 0, 0), Color(140, 120, 100), Color(255, 255, 255)})
                .save(shots + QStringLiteral("/fx-sample-target.png"));
            p.save(shots + QStringLiteral("/fx-original.png"));
        }
    }

    // --- Effektstapel (nicht-destruktive Bitmap-Effekte) ------------------------------------

    void bitmapEffectStackRoundTrip()
    {
        RectangleObject rect(QSizeF(40, 30));
        QVERIFY(rect.bitmapEffects().isEmpty());
        QVERIFY(!rect.hasBitmapEffects());
        BitmapEffect soften = fx::defaults(*fx::find(QStringLiteral("blur.soften")));
        soften.values = {70};
        BitmapEffect frame = fx::defaults(*fx::find(QStringLiteral("creative.frame")));
        frame.values[0] = 3;
        frame.colors = {Color(200, 10, 20)};
        frame.enabled = false;
        rect.setBitmapEffects({soften, frame});
        QVERIFY(rect.hasBitmapEffects());
        QVERIFY(rect.hasEffects());

        const QJsonObject json = serial::toJson(rect);
        QVERIFY(json.contains(QStringLiteral("bitmapEffects")));
        auto copy = serial::fromJson(json, serial::Ids::Drop);
        auto* back = dynamic_cast<RectangleObject*>(copy.get());
        QVERIFY(back);
        QCOMPARE(back->bitmapEffects().size(), 2);
        QCOMPARE(back->bitmapEffects(), rect.bitmapEffects());
        QCOMPARE(back->bitmapEffects()[1].colors.value(0), Color(200, 10, 20));
        QVERIFY(!back->bitmapEffects()[1].enabled);

        // Ältere Dateien ohne Effektstapel: leer; Objekte ohne Stapel schreiben nichts
        QJsonObject old = json;
        old.remove(QStringLiteral("bitmapEffects"));
        auto plain = serial::fromJson(old, serial::Ids::Drop);
        QVERIFY(dynamic_cast<DrawObject*>(plain.get())->bitmapEffects().isEmpty());
        QVERIFY(!serial::toJson(*plain).contains(QStringLiteral("bitmapEffects")));

        // Effekte kopieren (Effekt kopieren von …, Umwandeln) nimmt den Stapel mit, Effekt löschen leert ihn
        EllipseObject other(QSizeF(10, 10));
        other.copyEffects(rect);
        QCOMPARE(other.bitmapEffects(), rect.bitmapEffects());
        other.clearEffects();
        QVERIFY(other.bitmapEffects().isEmpty());

        // Ganzes Dokument speichern/laden und Rückgängig
        Document doc;
        const NodeId layer = firstLayer(doc, 0);
        NodeId id;
        {
            Transaction tx(doc, QStringLiteral("Rechteck"));
            id = tx.insert(layer, -1, std::make_unique<RectangleObject>(QSizeF(20, 20)));
            tx.commit();
        }
        {
            Transaction tx(doc, QStringLiteral("Weichzeichnen"));
            tx.modify<DrawObject>(id, [&](DrawObject& o) { o.setBitmapEffects({soften}); });
            tx.commit();
        }
        QString error;
        auto loaded = io::loadFromBytes(io::saveToBytes(doc, &error), &error);
        QVERIFY2(loaded, qPrintable(error));
        QCOMPARE(loaded->findAs<DrawObject>(id)->bitmapEffects(), QList<BitmapEffect>{soften});
        doc.undo();
        QVERIFY(doc.findAs<DrawObject>(id)->bitmapEffects().isEmpty());
        doc.redo();
        QCOMPARE(doc.findAs<DrawObject>(id)->bitmapEffects().size(), 1);

        // Unbekannte bzw. unvollständige Werte: Vorgaben, Bereich begrenzt
        const BitmapEffect partial = fx::normalized(BitmapEffect{QStringLiteral("creative.frame"), {99}, {}, true});
        QCOMPARE(partial.values.size(), fx::find(QStringLiteral("creative.frame"))->sliders.size());
        QCOMPARE(partial.values[0], filters::frameShapeCount() - 1);
        QCOMPARE(partial.colors.size(), 1);
        QVERIFY(!fx::anyActive({BitmapEffect{QStringLiteral("gibt.es.nicht"), {}, {}, true}}));
    }

    void bitmapEffectChainDeterministic()
    {
        // Muster mit Farbe
        QImage img(80, 60, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        {
            QPainter p(&img);
            for (int y = 0; y < 6; ++y)
                for (int x = 0; x < 8; ++x)
                    p.fillRect(QRect(x * 10, y * 10, 10, 10), (x + y) % 2 ? QColor(220, 30, 30) : QColor(20, 40, 200));
        }
        BitmapEffect soften = fx::defaults(*fx::find(QStringLiteral("blur.soften")));
        BitmapEffect frame = fx::defaults(*fx::find(QStringLiteral("creative.frame")));
        BitmapEffect posterize = fx::defaults(*fx::find(QStringLiteral("art.posterize")));
        posterize.values = {2};
        const QList<BitmapEffect> stack{soften, frame, posterize};
        const QImage a = fx::applyStack(img, stack, 1.0);
        const QImage b = fx::applyStack(img, stack, 1.0);
        QCOMPARE(a.size(), img.size());
        QCOMPARE(a.format(), QImage::Format_ARGB32_Premultiplied);
        QVERIFY(a == b); // gleiche Eingabe, gleiches Ergebnis

        // Ausgeschalteter Effekt zählt nicht
        QList<BitmapEffect> off = stack;
        off[2].enabled = false;
        QVERIFY(fx::applyStack(img, off, 1.0) == fx::applyStack(img, {soften, frame}, 1.0));
        QVERIFY(fx::applyStack(img, off, 1.0) != a);
        // Reihenfolge zählt
        QVERIFY(fx::applyStack(img, {frame, soften}, 1.0) != fx::applyStack(img, {soften, frame}, 1.0));

        // Rahmen als Maske: Ecke durchsichtig, Mitte deckend; als Farbrand: Ecke in Rahmenfarbe
        QCOMPARE(qAlpha(a.pixel(0, 0)), 0);
        QCOMPARE(qAlpha(a.pixel(40, 30)), 255);
        BitmapEffect border = frame;
        border.values[6] = 1; // Farbrand
        border.colors = {Color(0, 255, 0)};
        const QImage bordered = fx::applyStack(img, {border}, 1.0);
        QCOMPARE(qAlpha(bordered.pixel(1, 1)), 255);
        QVERIFY(qGreen(bordered.pixel(1, 1)) > 240 && qRed(bordered.pixel(1, 1)) < 15);
        QCOMPARE(bordered.pixel(40, 30), img.pixel(40, 30));
        // Alle Vorlagen: Ecke offen, Mitte unverändert; Vorschaubilder vorhanden
        for (int shape = 0; shape < filters::frameShapeCount(); ++shape) {
            filters::FrameSettings s;
            s.shape = shape;
            QVERIFY2(filters::frameCoverage(s, 0.5, 0.5, 400, 300) < 0.05, filters::frameShapeName(shape));
            QVERIFY2(filters::frameCoverage(s, 200, 150, 400, 300) > 0.99, filters::frameShapeName(shape));
            QVERIFY(!filters::frameThumbnail(shape, QSize(64, 48)).isNull());
        }
        // Skalierung 50 %: aussen alles durchsichtig
        filters::FrameSettings small;
        small.scale = 50;
        QCOMPARE(filters::frameCoverage(small, 50, 150, 400, 300), 0.0);

        // Pixelregler skalieren mit der Auflösung, andere nicht
        const fx::Definition& glass = *fx::find(QStringLiteral("creative.glassblock"));
        QCOMPARE(fx::scaledValues(glass, {60, 40}, 0.5), (QList<int>{30, 20}));
        const fx::Definition& charcoal = *fx::find(QStringLiteral("art.charcoal"));
        QCOMPARE(fx::scaledValues(charcoal, {4, 50}, 2.0), (QList<int>{8, 50}));
        // Rand über das Objekt hinaus nur bei Effekten, die ihn brauchen
        QVERIFY(fx::margin({fx::defaults(*fx::find(QStringLiteral("blur.smooth")))}) > 0.0);
        QCOMPARE(fx::margin({frame}), 0.0);
        // Gauss: Reichweite wächst mit dem Radius (etwa 3 Sigma), sonst harte Kante
        BitmapEffect gauss = fx::defaults(*fx::find(QStringLiteral("blur.gaussian")));
        gauss.values = {100};
        QVERIFY(fx::margin({gauss}) >= 150.0);
        gauss.values = {4};
        QVERIFY(fx::margin({gauss}) < 40.0);
    }

    // Zylinder, Seite aufrollen und die Korrekturen stehen im Effektverzeichnis (auch als Live-Effekt)
    void bitmapCorrectionRegistry()
    {
        QImage img(80, 60, QImage::Format_ARGB32_Premultiplied);
        img.fill(QColor(200, 60, 40));
        for (const char* id : {"3d.cylinder", "3d.pagecurl", "adjust.localequalize", "adjust.sampletarget", "adjust.selectivecolor",
                               "adjust.replacecolors", "adjust.channelmixer"}) {
            const fx::Definition* def = fx::find(QString::fromLatin1(id));
            QVERIFY2(def && def->apply, id);
            const BitmapEffect e = fx::defaults(*def);
            QCOMPARE(fx::normalized(e), e);
            QCOMPARE(fx::apply(img, e, 1.0).size(), img.size());
            // Rundreise über JSON
            QCOMPARE(BitmapEffect::fromJson(e.toJson()), e);
        }
        QCOMPARE(fx::find(QStringLiteral("3d.pagecurl"))->group, QStringLiteral("3d"));
        QCOMPARE(fx::find(QStringLiteral("adjust.channelmixer"))->group, QStringLiteral("effects-adjust"));
        // Seite aufrollen (Vorgabe: Ecke unten rechts, weisser Hintergrund) als Stapeleffekt
        const QImage curled = fx::apply(img, fx::defaults(*fx::find(QStringLiteral("3d.pagecurl"))), 1.0);
        QCOMPARE(curled.pixelColor(79, 59).rgb(), QColor(Qt::white).rgb());
        QCOMPARE(curled.pixelColor(0, 0).rgb(), QColor(200, 60, 40).rgb());
        // Kanalmixer mit Rot <-> Blau über die Effektwerte
        BitmapEffect mix = fx::defaults(*fx::find(QStringLiteral("adjust.channelmixer")));
        mix.values = fx::channelMixerValues(adjust::ChannelMixer{{{{0, 0, 100, 0}, {0, 100, 0, 0}, {100, 0, 0, 0}}}});
        const QColor swapped = fx::apply(img, mix, 1.0).pixelColor(10, 10);
        QCOMPARE(swapped.red(), 40);
        QCOMPARE(swapped.blue(), 200);
        // Einstellungen <-> Werte
        filters::PageCurl pc;
        pc.corner = filters::PageCurl::TopLeft;
        pc.width = 30;
        const filters::PageCurl back = fx::pageCurlSettings(fx::pageCurlValues(pc), {pc.curlColor, pc.background});
        QCOMPARE(back.corner, filters::PageCurl::TopLeft);
        QCOMPARE(back.width, 30);
        adjust::SelectiveColor sc;
        sc.cmyk[3][2] = -40;
        sc.relative = false;
        const adjust::SelectiveColor sc2 = fx::selectiveColorSettings(fx::selectiveColorValues(sc));
        QCOMPARE(sc2.cmyk[3][2], -40);
        QVERIFY(!sc2.relative);
    }

    // --- Künstlerische Medien: Pinsel und Sprühdose -----------------------------------------
    // Drei Farbstreifen nebeneinander (rot, grün, blau), je 10 x 2 mm
    static ArtItem stripes()
    {
        ArtItem art;
        const Color colors[] = {Color(255, 0, 0), Color(0, 255, 0), Color(0, 0, 255)};
        for (int i = 0; i < 3; ++i) {
            QPainterPath p;
            p.addRect(QRectF(i * 10.0, 0, 10, 2));
            art.append(ArtPiece{p, colors[i]});
        }
        return art;
    }

    void mediaBrushMapsAlongPath()
    {
        ArtisticMedia brush;
        brush.kind = ArtisticMedia::Kind::Brush;
        double height = 0;
        brush.art = {media::normalizedBrush(stripes(), &brush.artAspect, &height)};
        QCOMPARE(brush.artAspect, 15.0);
        QCOMPARE(height, 2.0);
        brush.width = 4.0;
        // Gerade: die Streifen teilen die Länge in Drittel, quer 4 mm, Reihenfolge und Farben bleiben
        QPainterPath straight(QPointF(10, 50));
        straight.lineTo(110, 50);
        const ArtItem pieces = media::brushPieces(straight, brush);
        QCOMPARE(pieces.size(), 3);
        for (int i = 0; i < 3; ++i) {
            QCOMPARE(pieces[i].color, stripes()[i].color);
            const QRectF b = pieces[i].path.boundingRect();
            QVERIFY2(std::abs(b.left() - (10 + i * 100.0 / 3)) < 0.05 && std::abs(b.width() - 100.0 / 3) < 0.05, qPrintable(QString::number(b.left())));
            QVERIFY2(std::abs(b.top() - 48) < 0.01 && std::abs(b.bottom() - 52) < 0.01, qPrintable(QString::number(b.top())));
        }
        QVERIFY(pieces[1].path.contains(QPointF(60, 51)));
        // Gebogen: jeder Streifen liegt um "seinen" Drittelpunkt des Bogens
        QPainterPath arc(QPointF(10, 80));
        arc.cubicTo(QPointF(10, 20), QPointF(110, 20), QPointF(110, 80));
        const ArtItem bent = media::brushPieces(arc, brush);
        QCOMPARE(bent.size(), 3);
        for (int i = 0; i < 3; ++i) {
            QCOMPARE(bent[i].color, stripes()[i].color);
            const QPointF expected = arc.pointAtPercent(arc.percentAtLength(arc.length() * (i + 0.5) / 3));
            QVERIFY2(bent[i].path.contains(expected), qPrintable(QStringLiteral("%1 %2").arg(expected.x()).arg(expected.y())));
            for (int k = 0; k < 3; ++k)
                if (k != i)
                    QVERIFY(!bent[k].path.contains(expected));
        }
        // Breite quer zum Bogen: am Scheitel 4 mm hoch
        const QPointF top = arc.pointAtPercent(arc.percentAtLength(arc.length() / 2));
        QVERIFY(bent[1].path.contains(top + QPointF(0, 1.8)) && bent[1].path.contains(top - QPointF(0, 1.8)));
        QVERIFY(!bent[1].path.contains(top + QPointF(0, 2.3)));
        // Wiederholen: Kachel 4 mm x 15 = 60 mm, auf 100 mm also zweimal (je 50 mm)
        brush.brushRepeat = true;
        const ArtItem repeated = media::brushPieces(straight, brush);
        QCOMPARE(repeated.size(), 6);
        QCOMPARE(repeated[3].color, Color(255, 0, 0));
        QVERIFY(std::abs(repeated[3].path.boundingRect().left() - 60) < 0.05); // 10 + 50
        // Ohne eigene Vorlage: eingebauter Pinsel
        ArtisticMedia builtIn;
        builtIn.kind = ArtisticMedia::Kind::Brush;
        QVERIFY(!media::brushPieces(straight, builtIn).isEmpty());
        QVERIFY(media::builtInBrushCount() >= 4);
    }

    void mediaSprayerIsDeterministic()
    {
        QPainterPath line(QPointF(0, 50));
        line.lineTo(100, 50);
        ArtisticMedia spray;
        spray.kind = ArtisticMedia::Kind::Sprayer;
        spray.sprayList = 3; // Herzen
        spray.width = 5.0;
        spray.spacing = 10.0;
        spray.seed = 42;
        const QList<pathops::SprayPiece> a = pathops::sprayPieces(line, spray);
        QCOMPARE(a.size(), 11);
        QCOMPARE(pathops::sprayPieces(line, spray), a); // gleiche Zufallszahl, gleiches Ergebnis
        spray.seed = 43;
        QVERIFY(pathops::sprayPieces(line, spray) != a);
        // Mehrere Objekte je Tupfer
        spray.dabCount = 3;
        QCOMPARE(pathops::sprayPieces(line, spray).size(), 33);
        // Eigene Sprühliste der Reihe nach, ohne Grössenvariation, entlang des Pfads, abwechselnd versetzt
        ArtItem red, blue;
        QPainterPath square;
        square.addRect(QRectF(-0.5, -0.5, 1, 1));
        red << ArtPiece{square, Color(255, 0, 0)};
        blue << ArtPiece{square, Color(0, 0, 255)};
        spray.art = {red, blue};
        spray.dabCount = 1;
        spray.order = ArtisticMedia::Order::Sequential;
        spray.sizeVariation = 0;
        spray.randomRotate = false;
        spray.offsetMode = ArtisticMedia::Offset::Alternate;
        spray.offset = 100; // eine Grösse
        const QList<pathops::SprayPiece> seq = pathops::sprayPieces(line, spray);
        QCOMPARE(seq.size(), 11);
        for (int i = 0; i < seq.size(); ++i) {
            QCOMPARE(seq[i].color, i % 2 ? Color(0, 0, 255) : Color(255, 0, 0));
            const QRectF b = seq[i].path.boundingRect();
            QVERIFY2(std::abs(b.width() - 5) < 1e-6, qPrintable(QString::number(b.width())));
            QVERIFY(std::abs(b.center().x() - 10 * i) < 1e-6);
            QVERIFY(std::abs(b.center().y() - (i % 2 ? 45.0 : 55.0)) < 1e-6);
        }
        // Nach Richtung: von links nach rechts der erste Eintrag, zurück der zweite
        spray.order = ArtisticMedia::Order::Direction;
        spray.offsetMode = ArtisticMedia::Offset::None;
        for (const pathops::SprayPiece& p : pathops::sprayPieces(line, spray))
            QCOMPARE(p.color, Color(255, 0, 0));
        for (const pathops::SprayPiece& p : pathops::sprayPieces(line.toReversed(), spray))
            QCOMPARE(p.color, Color(0, 0, 255));
    }

    void mediaBrushAndSprayerRoundTrip()
    {
        Document doc;
        Transaction tx(doc, QStringLiteral("x"));
        SubPath sp{{CurveNode{QPointF(10, 50)}, CurveNode{QPointF(110, 50)}}, false};
        auto brushCurve = std::make_unique<CurveObject>(QList<SubPath>{sp});
        ArtisticMedia brush;
        brush.kind = ArtisticMedia::Kind::Brush;
        brush.art = {media::normalizedBrush(stripes(), &brush.artAspect, nullptr)};
        brush.artName = QStringLiteral("Streifen");
        brush.brushRepeat = true;
        brush.width = 3.0;
        brushCurve->setMedia(brush);
        const ArtItem before = brushCurve->mediaPieces();
        const NodeId brushId = tx.insert(firstLayer(doc, 0), -1, std::move(brushCurve));
        auto sprayCurve = std::make_unique<CurveObject>(QList<SubPath>{sp});
        ArtisticMedia spray;
        spray.kind = ArtisticMedia::Kind::Sprayer;
        spray.art = {stripes().mid(0, 1), stripes().mid(1, 2)};
        spray.artName = QStringLiteral("Eigene");
        spray.dabCount = 2;
        spray.sizeVariation = 10;
        spray.order = ArtisticMedia::Order::Direction;
        spray.offsetMode = ArtisticMedia::Offset::Alternate;
        spray.offset = 55;
        spray.randomRotate = false;
        spray.seed = 99;
        sprayCurve->setMedia(spray);
        const NodeId sprayId = tx.insert(firstLayer(doc, 0), -1, std::move(sprayCurve));
        tx.commit();
        QString error;
        std::unique_ptr<Document> loaded = io::loadFromBytes(io::saveToBytes(doc, &error), &error);
        QVERIFY2(loaded, qPrintable(error));
        QCOMPARE(loaded->findAs<CurveObject>(brushId)->media(), brush);
        QCOMPARE(loaded->findAs<CurveObject>(brushId)->mediaPieces(), before);
        QCOMPARE(loaded->findAs<CurveObject>(sprayId)->media(), spray);
        // Ältere Dateien ohne die neuen Felder: Standardwerte, "randomOrder" wird übernommen
        const ArtisticMedia old = media::fromJson(QJsonObject{{QStringLiteral("kind"), QStringLiteral("sprayer")},
                                                              {QStringLiteral("randomOrder"), false}});
        QCOMPARE(old.order, ArtisticMedia::Order::Sequential);
        QCOMPARE(old.dabCount, 1);
        QCOMPARE(old.sizeVariation, 40.0);
        QVERIFY(old.art.isEmpty());
    }

    void mediaArtFromObjects()
    {
        // Rechteck mit Füllung und Kontur: zwei Flächen; Gruppe wird aufgelöst
        auto rect = std::make_unique<RectangleObject>(QSizeF(20, 10));
        rect->setTransform(QTransform::fromTranslate(10, 10));
        Style s;
        s.fill = Fill::solid(Color(255, 0, 0));
        s.stroke = Color(0, 0, 255);
        s.strokeWidth = 1.0;
        rect->setStyle(s);
        const ArtItem pieces = media::piecesOf(*rect);
        QCOMPARE(pieces.size(), 2);
        QCOMPARE(pieces[0].color, Color(255, 0, 0));
        QCOMPARE(pieces[1].color, Color(0, 0, 255));
        QVERIFY(std::abs(pieces[1].path.boundingRect().width() - 21) < 0.01);
        const ArtItem item = media::normalizedSprayItem(pieces);
        QRectF b;
        for (const ArtPiece& p : item)
            b = b.united(p.path.boundingRect());
        QVERIFY(std::abs(b.width() - 1.0) < 1e-9 && std::abs(b.center().x()) < 1e-9 && std::abs(b.center().y()) < 1e-9);
        GroupObject group;
        attachDetachedChild(group, std::move(rect));
        QCOMPARE(media::piecesOf(group).size(), 2);
    }

    void mediaLibraryStoresUserStrokes()
    {
        QTemporaryDir dir;
        MediaLibrary& lib = MediaLibrary::instance();
        lib.setFilePath(dir.filePath(QStringLiteral("medien.json")));
        QVERIFY(lib.user().isEmpty());
        const int builtInBrushes = int(lib.brushes().size());
        const MediaStroke brush = lib.addUserBrush(QString(), stripes());
        QCOMPARE(brush.name, QStringLiteral("Eigener Pinsel 1"));
        QCOMPARE(brush.media.width, 2.0); // natürliche Höhe
        const MediaStroke spray = lib.addUserSprayer(QStringLiteral("Quadrate"), {stripes().mid(0, 1), stripes().mid(2, 1)});
        QCOMPARE(spray.media.art.size(), 2);
        QCOMPARE(int(lib.brushes().size()), builtInBrushes + 1);
        lib.noteUsed(lib.presets().at(2));
        lib.noteUsed(spray);
        lib.noteUsed(lib.presets().at(2));
        QCOMPARE(lib.recent().size(), 2);
        QCOMPARE(lib.recent().first().media.preset, 2);
        QCOMPARE(lib.strokeFor(spray.media).name, QStringLiteral("Quadrate"));
        // Neu laden: alles aus der Datei
        lib.setFilePath(dir.filePath(QStringLiteral("medien.json")));
        QCOMPARE(lib.user().size(), 2);
        QCOMPARE(lib.user().at(0).media, brush.media);
        QCOMPARE(lib.user().at(1).media, spray.media);
        QCOMPARE(lib.recent().size(), 2);
        QVERIFY(lib.removeUser(QStringLiteral("Quadrate")));
        QCOMPARE(lib.user().size(), 1);
        lib.setFilePath(dir.filePath(QStringLiteral("leer.json")));
    }

    // --- Effekte: Blockschatten, Punktraster, Fotomosaik, Hüllenzuordnung, Abschrägung bis Mitte,
    // Linse Bitmap-Effekt -------------------------------------------------------------------------

    void blockShadowIsSweptUnion()
    {
        // Referenz: p liegt im Schatten, wenn p - t * v für ein t in [0, 1] in der Fläche liegt
        auto check = [](const QPainterPath& area, const QPointF& v) {
            const QPainterPath swept = sweepArea(area, v);
            const QRectF box = area.boundingRect().united(area.boundingRect().translated(v)).adjusted(-1, -1, 1, 1);
            int samples = 0, wrong = 0;
            for (double y = box.top(); y < box.bottom(); y += 0.37)
                for (double x = box.left(); x < box.right(); x += 0.37) {
                    const QPointF p(x, y);
                    bool inside = false;
                    for (int k = 0; k <= 400 && !inside; ++k)
                        inside = area.contains(p - v * (k / 400.0));
                    ++samples;
                    if (inside != swept.contains(p))
                        ++wrong;
                }
            return double(wrong) / samples;
        };
        // Quadrat: Sechseck mit Fläche 100 + 2 * 50
        QPainterPath square;
        square.addRect(QRectF(0, 0, 10, 10));
        const QPainterPath hex = sweepArea(square, QPointF(5, 5));
        QVERIFY(hex.contains(QPointF(12, 12)) && hex.contains(QPointF(11, 2)));
        QVERIFY(!hex.contains(QPointF(12, 1)) && !hex.contains(QPointF(1, 12)));
        double area = 0;
        for (const QPolygonF& poly : hex.toFillPolygons()) {
            double a = 0;
            for (int i = 0; i < poly.size(); ++i)
                a += poly[i].x() * poly[(i + 1) % poly.size()].y() - poly[(i + 1) % poly.size()].x() * poly[i].y();
            area += std::abs(a) / 2.0;
        }
        QVERIFY2(std::abs(area - 200.0) < 0.5, qPrintable(QString::number(area)));
        // Ellipse, Stern (nicht konvex) und Ring (mit Loch) in verschiedene Richtungen
        QPainterPath ellipse;
        ellipse.addEllipse(QRectF(0, 0, 20, 12));
        QPainterPath star;
        for (int i = 0; i < 10; ++i) {
            const double a = i * M_PI / 5.0, r = i % 2 ? 4.0 : 10.0;
            const QPointF p(10 + r * std::cos(a), 10 + r * std::sin(a));
            if (i == 0)
                star.moveTo(p);
            else
                star.lineTo(p);
        }
        star.closeSubpath();
        QPainterPath ring;
        ring.addEllipse(QRectF(0, 0, 20, 20));
        ring.addEllipse(QRectF(5, 5, 10, 10));
        for (const auto& [shape, v] : {std::pair{ellipse, QPointF(8, -3)}, std::pair{star, QPointF(-6, 9)}, std::pair{ring, QPointF(2, 1)}}) {
            const double err = check(shape, v);
            QVERIFY2(err < 0.01, qPrintable(QString::number(err)));
        }
        // Kurzer Zug: das Loch des Rings bleibt, "Löcher entfernen" füllt es; Vereinfachen lässt das
        // Objekt selbst aus; Vergrössern wächst nach allen Seiten
        BlockShadow s;
        s.active = true;
        s.offset = QPointF(2, 1);
        QVERIFY(!blockShadowPath(ring, s).contains(QPointF(10, 10.5)));
        s.removeHoles = true;
        QVERIFY(blockShadowPath(ring, s).contains(QPointF(10, 10.5)));
        s.removeHoles = false;
        s.simplify = true;
        QVERIFY(!blockShadowPath(ring, s).contains(QPointF(2, 10)));
        QVERIFY(blockShadowPath(ring, s).contains(QPointF(21, 11)));
        s.simplify = false;
        s.expand = 1.5;
        const QRectF grown = blockShadowPath(square, s).boundingRect();
        QVERIFY2(std::abs(grown.left() + 1.5) < 0.05 && std::abs(grown.right() - 13.5) < 0.05, qPrintable(QString::number(grown.right())));
        // Polar: Tiefe und Richtung
        s.setPolar(10, 90);
        QVERIFY(std::abs(s.offset.x()) < 1e-9 && std::abs(s.offset.y() + 10) < 1e-9);
        QCOMPARE(std::lround(s.angle()), 90L);
        // Gespeichert am Objekt, alte Dateien ohne Angabe: kein Schatten
        RectangleObject r(QSizeF(10, 10));
        s.color = Color(200, 10, 10);
        r.setBlockShadow(s);
        auto back = serial::fromJson(serial::toJson(r), serial::Ids::Keep);
        QCOMPARE(static_cast<DrawObject*>(back.get())->blockShadow(), s);
        RectangleObject plain(QSizeF(10, 10));
        QVERIFY(!static_cast<DrawObject*>(serial::fromJson(serial::toJson(plain), serial::Ids::Keep).get())->blockShadow().active);
        QVERIFY(r.hasEffects());
    }

    void pointillizeCellsAndColors()
    {
        // 100 x 50 Pixel: links rot, rechts blau, als 20 x 10 mm
        QImage img(100, 50, QImage::Format_ARGB32_Premultiplied);
        img.fill(QColor(255, 0, 0));
        {
            QPainter p(&img);
            p.fillRect(QRect(50, 0, 50, 50), QColor(0, 0, 255));
        }
        mosaics::PointSettings s;
        s.cell = 2.0;
        s.scale = 80;
        QList<mosaics::Dot> dots = mosaics::pointillize(img, QSizeF(20, 10), s);
        QCOMPARE(dots.size(), 50); // 10 x 5 Zellen
        for (const mosaics::Dot& d : dots) {
            QVERIFY(std::abs(d.size - 1.6) < 1e-9);
            QCOMPARE(d.color, d.center.x() < 10 ? Color(255, 0, 0) : Color(0, 0, 255));
        }
        // Formen: Kreis, Quadrat, eigene Form in die Zelle eingepasst
        const QRectF c = mosaics::dotPath(dots.first(), mosaics::PointSettings::Shape::Circle).boundingRect();
        QVERIFY(std::abs(c.width() - 1.6) < 1e-6 && std::abs(c.center().x() - 1.0) < 1e-6);
        QPainterPath heart;
        heart.addRect(QRectF(0, 0, 40, 20));
        const QRectF h = mosaics::dotPath(dots.first(), mosaics::PointSettings::Shape::Custom, heart).boundingRect();
        QVERIFY(std::abs(h.width() - 1.6) < 1e-6 && std::abs(h.height() - 0.8) < 1e-6);
        // Durchsichtige Hälfte: keine Punkte dort
        QImage half = img;
        {
            QPainter p(&half);
            p.setCompositionMode(QPainter::CompositionMode_Clear);
            p.fillRect(QRect(50, 0, 50, 50), Qt::transparent);
        }
        QCOMPARE(mosaics::pointillize(half, QSizeF(20, 10), s).size(), 25);
        // Grösse nach Helligkeit: Verlauf schwarz -> weiss, Punkte werden kleiner, Farbe fest
        QImage ramp(100, 10, QImage::Format_ARGB32_Premultiplied);
        for (int x = 0; x < 100; ++x)
            for (int y = 0; y < 10; ++y)
                ramp.setPixelColor(x, y, QColor(x * 255 / 99, x * 255 / 99, x * 255 / 99));
        s.mode = mosaics::PointSettings::Mode::Size;
        s.color = Color(0, 128, 0);
        s.minSize = 0;
        dots = mosaics::pointillize(ramp, QSizeF(20, 2), s);
        QCOMPARE(dots.size(), 10);
        for (int i = 1; i < dots.size(); ++i)
            QVERIFY(dots[i].size < dots[i - 1].size);
        QCOMPARE(dots.first().color, Color(0, 128, 0));
        // Farbe begrenzen: aus dem Verlauf genau zwei Farben
        s.mode = mosaics::PointSettings::Mode::Color;
        s.limitColors = 2;
        dots = mosaics::pointillize(ramp, QSizeF(20, 2), s);
        QSet<QString> colors;
        for (const mosaics::Dot& d : dots)
            colors << d.color.toHex();
        QCOMPARE(colors.size(), 2);
        // Rasterwinkel: gedrehtes Raster deckt das Bild ebenfalls ab (etwa gleich viele Zellen)
        s.limitColors = 0;
        s.angle = 30;
        const int rotated = int(mosaics::pointillize(img, QSizeF(20, 10), s).size());
        QVERIFY2(rotated > 40 && rotated < 62, qPrintable(QString::number(rotated)));
    }

    void photoMosaicMatchesMeanColors()
    {
        // Kachelordner: fünf einfarbige Bilder und eine Datei, die kein Bild ist
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QList<std::pair<QString, QColor>> tiles{{QStringLiteral("rot.png"), QColor(250, 10, 10)},
                                                       {QStringLiteral("gruen.png"), QColor(10, 240, 10)},
                                                       {QStringLiteral("blau.png"), QColor(10, 10, 250)},
                                                       {QStringLiteral("weiss.png"), QColor(255, 255, 255)},
                                                       {QStringLiteral("schwarz.png"), QColor(0, 0, 0)}};
        for (const auto& [name, color] : tiles) {
            QImage t(40, 30, QImage::Format_RGB32);
            t.fill(color);
            QVERIFY(t.save(dir.filePath(name)));
        }
        QFile junk(dir.filePath(QStringLiteral("liesmich.txt")));
        QVERIFY(junk.open(QIODevice::WriteOnly));
        junk.write("kein Bild");
        junk.close();
        const QList<mosaics::Tile> lib = mosaics::loadTiles(dir.path(), 16);
        QCOMPARE(lib.size(), 5);
        auto tileName = [&](int i) { return i >= 0 ? QFileInfo(lib[i].path).fileName() : QString(); };
        // Vorlage 4 x 2 Zellen in bekannten Farben
        QImage target(80, 40, QImage::Format_ARGB32_Premultiplied);
        const QColor cellColors[2][4] = {{QColor(255, 0, 0), QColor(0, 255, 0), QColor(0, 0, 255), QColor(240, 240, 240)},
                                         {QColor(20, 20, 20), QColor(200, 30, 40), QColor(40, 200, 60), QColor(30, 50, 220)}};
        const QString expected[2][4] = {{QStringLiteral("rot.png"), QStringLiteral("gruen.png"), QStringLiteral("blau.png"), QStringLiteral("weiss.png")},
                                        {QStringLiteral("schwarz.png"), QStringLiteral("rot.png"), QStringLiteral("gruen.png"), QStringLiteral("blau.png")}};
        {
            QPainter p(&target);
            for (int r = 0; r < 2; ++r)
                for (int c = 0; c < 4; ++c)
                    p.fillRect(QRect(c * 20, r * 20, 20, 20), cellColors[r][c]);
        }
        mosaics::PhotoSettings s;
        s.columns = 4;
        s.avoidNeighbors = false;
        QCOMPARE(mosaics::autoRows(target.size(), 4), 2);
        const QList<mosaics::Cell> cells = mosaics::matchTiles(target, lib, s);
        QCOMPARE(cells.size(), 8);
        for (const mosaics::Cell& c : cells)
            QCOMPARE(tileName(c.tile), expected[c.row][c.column]);
        // Wiederholungen begrenzt: ganz rote Vorlage, jede Kachel höchstens einmal
        QImage red(80, 20, QImage::Format_ARGB32_Premultiplied);
        red.fill(QColor(255, 0, 0));
        s.rows = 1;
        s.maxRepeats = 1;
        QSet<int> used;
        const QList<mosaics::Cell> limited = mosaics::matchTiles(red, lib, s);
        for (const mosaics::Cell& c : limited)
            used << c.tile;
        QCOMPARE(used.size(), 4);
        QCOMPARE(tileName(limited.first().tile), QStringLiteral("rot.png"));
        // Ohne Grenze: überall die rote Kachel
        s.maxRepeats = 0;
        for (const mosaics::Cell& c : mosaics::matchTiles(red, lib, s))
            QCOMPARE(tileName(c.tile), QStringLiteral("rot.png"));
        // Nachbarn vermeiden: nie zweimal dasselbe nebeneinander
        s.avoidNeighbors = true;
        const QList<mosaics::Cell> spread = mosaics::matchTiles(red, lib, s);
        for (int i = 1; i < spread.size(); ++i)
            QVERIFY(spread[i].tile != spread[i - 1].tile);
        // Farbangleichung 100 %: blaue Kachel wird zur Zellfarbe hin verschoben
        int blue = 0;
        while (tileName(blue) != QStringLiteral("blau.png"))
            ++blue;
        const QColor shifted = mosaics::meanColor(mosaics::tileImage(lib[blue], QSize(10, 10), QColor(250, 10, 10), 100));
        QVERIFY2(shifted.red() > 240 && shifted.blue() < 20, qPrintable(shifted.name()));
        const QColor kept = mosaics::meanColor(mosaics::tileImage(lib[blue], QSize(10, 10), QColor(250, 10, 10), 0));
        QVERIFY(kept.blue() > 240);
        // Ganzes Mosaik als ein Bild
        s.avoidNeighbors = false;
        s.rows = 0;
        s.tint = 0;
        const QImage whole = mosaics::renderMosaic(target, lib, cells, s, 8);
        QCOMPARE(whole.size(), QSize(32, 16));
        QCOMPARE(whole.pixelColor(4, 4).red(), 250);
        QCOMPARE(whole.pixelColor(28, 12).blue(), 250);
    }

    void envelopeMappingModesDiffer()
    {
        // Hülle mit hochgezogener oberer Mitte und nach rechts gezogener rechter Mitte
        Envelope e = Envelope::rectangle(QRectF(0, 0, 100, 100));
        e.moveHandle(1, QPointF(60, -30));
        e.moveHandle(3, QPointF(130, 50));
        QSet<QString> results;
        using M = Envelope::Mapping;
        for (M m : {M::Standard, M::Original, M::Putty, M::Horizontal, M::Vertical}) {
            Envelope k = e;
            k.mapping = m;
            // Ecken bleiben bei allen Zuordnungen, wo sie sind
            QVERIFY(QLineF(k.map(QPointF(0, 0)), QPointF(0, 0)).length() < 1e-9);
            QVERIFY(QLineF(k.map(QPointF(100, 100)), QPointF(100, 100)).length() < 1e-9);
            const QPointF a = k.map(QPointF(25, 30)), b = k.map(QPointF(70, 60));
            results << QStringLiteral("%1,%2,%3,%4").arg(a.x(), 0, 'f', 3).arg(a.y(), 0, 'f', 3).arg(b.x(), 0, 'f', 3).arg(b.y(), 0, 'f', 3);
            QPainterPath sq;
            sq.addRect(QRectF(10, 10, 80, 80));
            QVERIFY(!k.map(sq).isEmpty());
            // Speichern und laden
            QCOMPARE(Envelope::fromJson(k.toJson()).mapping, m);
        }
        QCOMPARE(results.size(), 5);
        // Kitt folgt dem oberen Rand genau; Horizontal hält Zeilen gerade, Vertikal Spalten
        Envelope k = e;
        k.mapping = M::Putty;
        QVERIFY(QLineF(k.map(QPointF(50, 0)), QPointF(60, -30)).length() < 1e-6);
        k.mapping = M::Horizontal;
        QVERIFY(std::abs(k.map(QPointF(50, 0)).y()) < 1e-9);
        QVERIFY(std::abs(k.map(QPointF(100, 50)).x() - 130) < 1e-6);
        k.mapping = M::Vertical;
        QVERIFY(std::abs(k.map(QPointF(50, 0)).y() + 30) < 1e-6);
        QVERIFY(std::abs(k.map(QPointF(100, 50)).x() - 100) < 1e-9);
        // Alte Dateien ohne Angabe: Standard
        QJsonObject old = e.toJson();
        old.remove(QStringLiteral("mapping"));
        QCOMPARE(Envelope::fromJson(old).mapping, M::Standard);
    }

    void bevelToCenterHeightField()
    {
        // Quadrat 21 x 21 Pixel ganz gefüllt: Abstand zum Rand wächst bis zur Mitte (Pyramide)
        QImage mask(21, 21, QImage::Format_Grayscale8);
        mask.fill(255);
        float maxD = 0;
        const QList<float> d = distanceToEdge(mask, &maxD);
        QCOMPARE(d.size(), 21 * 21);
        QCOMPARE(maxD, 11.0f);
        QCOMPARE(d[10 * 21 + 10], 11.0f);
        QCOMPARE(d[10 * 21 + 0], 1.0f);
        QCOMPARE(d[10 * 21 + 5], 6.0f);
        QCOMPARE(d[0], 1.0f);
        // Ein Loch in der Mitte: dort 0, daneben 1
        mask.setPixel(10, 10, 0);
        const QList<float> h = distanceToEdge(mask, &maxD);
        QCOMPARE(h[10 * 21 + 10], 0.0f);
        QCOMPARE(h[10 * 21 + 11], 1.0f);
        // Gespeichert: bis zur Mitte; alte Dateien: fester Abstand
        Bevel b;
        b.style = Bevel::Style::Soft;
        b.toCenter = true;
        QCOMPARE(Bevel::fromJson(b.toJson()), b);
        QJsonObject old = b.toJson();
        old.remove(QStringLiteral("toCenter"));
        QVERIFY(!Bevel::fromJson(old).toCenter);
    }

    void lensBitmapEffectFiltersBelow()
    {
        Lens l;
        l.type = Lens::Type::BitmapEffect;
        l.bitmapEffect = QStringLiteral("pixelate");
        l.amount = 100;
        QVERIFY(!l.changesColor());
        QCOMPARE(Lens::fromJson(l.toJson()), l);
        QVERIFY(Lens::bitmapEffects().size() >= 10);
        // Verlauf: Verpixeln macht benachbarte Punkte gleich
        QImage img(64, 64, QImage::Format_ARGB32_Premultiplied);
        for (int y = 0; y < 64; ++y)
            for (int x = 0; x < 64; ++x)
                img.setPixelColor(x, y, QColor(x * 4, y * 4, 128));
        const QImage out = l.applyBitmapEffect(img, 1.0);
        QCOMPARE(out.size(), img.size());
        QCOMPARE(out.format(), img.format());
        QVERIFY(out != img);
        QCOMPARE(out.pixel(1, 1), out.pixel(2, 2));
        for (const auto& [id, name] : Lens::bitmapEffects()) {
            Lens k = l;
            k.bitmapEffect = id;
            k.amount = 60;
            QVERIFY2(k.applyBitmapEffect(img, 2.0) != img, qPrintable(id));
        }
    }

};

QTEST_MAIN(TestCore)
#include "tst_core.moc"
