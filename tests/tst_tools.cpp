// Bedienung der Werkzeuge mit echten Mausereignissen auf der Zeichenfläche.
// Mit VEK_SCREENSHOT_DIR=<Ordner> legen die Tests Bildschirmfotos der Zwischenstände ab.

#include "Canvas.h"
#include "CdrImport.h"
#include "ColorDialog.h"
#include "ColorPalette.h"
#include "GradientDialog.h"
#include "Icons.h"
#include "ImageObject.h"
#include "DocumentIO.h"
#include "Editor.h"
#include "CurveObject.h"
#include "ExpressionSpinBox.h"
#include "ShapeTool.h"
#include "ArtisticMediaTool.h"
#include "CutTools.h"
#include "FlyoutButton.h"
#include "GroupObject.h"
#include "MainWindow.h"
#include "PickTool.h"
#include "PolygonObject.h"
#include "RectangleObject.h"
#include "TextObject.h"
#include "TextTool.h"
#include "TraceDialog.h"
#include "Serialization.h"
#include "EffectTool.h"
#include "ShapingDocker.h"
#include "OptionsDialog.h"
#include "Options.h"
#include "EllipseObject.h"
#include "SmartDrawingTool.h"
#include "ConnectorObject.h"
#include "DimensionObject.h"
#include "TableObject.h"
#include "MeshTool.h"
#include <QTemporaryDir>
#include "Output.h"
#include "AlignDialog.h"
#include "BlendObject.h"
#include "InteractiveTools.h"
#include "InteractiveFillTool.h"
#include "OutlineDialog.h"
#include "ShapingTools.h"
#include "PerfectShapeTool.h"
#include "PatternDialog.h"
#include "PathOps.h"
#include "ExportDialog.h"
#include "ImageCodecs.h"
#include "SvgImport.h"
#include "PdfImport.h"
#include "DxfImport.h"
#include "FontFile.h"
#include "FontMatch.h"
#include "FontMatchDialog.h"
#include "ImpositionDialog.h"
#include "PdfExport.h"
#include "PdfSettingsDialog.h"
#include "Ai.h"
#include "AiChat.h"
#include "AiPanel.h"
#include "AiTools.h"
#include "McpServer.h"
#include "ClaudeCode.h"
#include "ImageSearch.h"
#include "WebSearch.h"
#include "ImageSearchDialog.h"
#include "ImageSearchSamples.h"
#include <QTabWidget>
#include <QSettings>
#include "FontPrograms.h"
#include <QFontDatabase>
#include <QRawFont>
#include <QButtonGroup>
#include "EffectDockers.h"
#include "SuperClipBar.h"
#include <QLabel>
#include <QTabBar>
#include <QHostAddress>
#include <QTcpSocket>
#include <QTcpServer>
#include <QListWidget>
#include "ShadowTool.h"
#include "TableTool.h"
#include "PerspectiveTool.h"
#include "SuperClipObject.h"
#include "PipetteTools.h"
#include "CloneObject.h"
#include "TransformDocker.h"
#include "ScriptHost.h"
#include <QFontDatabase>
#include <QSettings>
#include "FontBox.h"
#include "SpellChecker.h"
#include "SpellDialog.h"
#include "Transaction.h"
#include "ImagePool.h"
#include "SnapEngine.h"
#include "DisplayDepth.h"
#include "ImageDialogs.h"
#include "Ruler.h"

#include <QBuffer>
#include <QComboBox>
#include <QDir>
#include <QSvgRenderer>
#include <QDoubleSpinBox>
#include <QFontComboBox>
#include <QElapsedTimer>
#include <QLineEdit>
#include <QMenu>
#include <QTimer>
#include <QToolBar>
#include <QSlider>
#include <QUrlQuery>
#include <QSet>

#include <QtTest>
#include <QTreeWidget>
#include <QDockWidget>
#include <QPushButton>
#include <QCheckBox>

using namespace vek;

namespace {

constexpr double kTol = 0.6; // mm; Mauspositionen sind ganze Pixel

bool near(const QRectF& a, const QRectF& b, double tol = kTol)
{
    return qAbs(a.left() - b.left()) < tol && qAbs(a.top() - b.top()) < tol && qAbs(a.right() - b.right()) < tol
           && qAbs(a.bottom() - b.bottom()) < tol;
}

QString describe(const QRectF& r)
{
    return QStringLiteral("(%1, %2, %3 x %4)").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height());
}

} // namespace

class TestTools : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<Editor> m_editor;
    std::unique_ptr<Canvas> m_canvas;
    ulong m_clock = 0;

    QPoint screen(const QPointF& doc) const { return m_canvas->toScreen(doc).toPoint(); }
    QRectF bounds() const { return m_editor->selectionBounds(); }

    // Griffe sitzen 7 px ausserhalb des Begrenzungsrechtecks
    QPoint handle(Qt::Corner corner) const
    {
        const QRectF r = m_canvas->viewTransform().mapRect(bounds()).adjusted(-7, -7, 7, 7);
        switch (corner) {
        case Qt::TopLeftCorner: return r.topLeft().toPoint();
        case Qt::TopRightCorner: return r.topRight().toPoint();
        case Qt::BottomLeftCorner: return r.bottomLeft().toPoint();
        default: return r.bottomRight().toPoint();
        }
    }
    QPoint rightHandle() const
    {
        const QRectF r = m_canvas->viewTransform().mapRect(bounds()).adjusted(-7, -7, 7, 7);
        return QPointF(r.right(), r.center().y()).toPoint();
    }

    // Zeitstempel weit auseinander, damit Qt keine Doppelklicks daraus macht
    void press(const QPoint& p, Qt::KeyboardModifiers m = {})
    {
        QTest::mousePress(m_canvas.get(), Qt::LeftButton, m, p, 1000);
    }
    void move(const QPoint& p) { QTest::mouseMove(m_canvas.get(), p); }
    void release(const QPoint& p, Qt::KeyboardModifiers m = {})
    {
        QTest::mouseRelease(m_canvas.get(), Qt::LeftButton, m, p, 10);
    }
    void drag(const QPoint& from, const QPoint& to, Qt::KeyboardModifiers m = {}, const QString& shot = {})
    {
        press(from, m);
        // in Schritten, wie eine echte Mausbewegung
        for (int i = 1; i <= 4; ++i)
            move(from + (to - from) * i / 4);
        if (!shot.isEmpty())
            screenshot(shot);
        release(to, m);
    }
    void click(const QPoint& p) { QTest::mouseClick(m_canvas.get(), Qt::LeftButton, {}, p, 1000); }

    void screenshot(const QString& name)
    {
        const QString dir = qEnvironmentVariable("VEK_SCREENSHOT_DIR");
        if (!dir.isEmpty())
            m_canvas->grab().save(dir + QLatin1Char('/') + name + QStringLiteral(".png"));
    }

    NodeId drawRect(const QRectF& r)
    {
        m_canvas->setTool(ToolId::Rectangle);
        drag(screen(r.topLeft()), screen(r.bottomRight()));
        return m_editor->selection().value(0);
    }

private slots:
    void init()
    {
        m_editor = std::make_unique<Editor>();
        m_canvas = std::make_unique<Canvas>(*m_editor);
        m_canvas->resize(700, 900);
        m_canvas->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_canvas.get()));
        m_canvas->fitPage();
    }

    void cleanup()
    {
        m_canvas.reset();
        m_editor.reset();
    }

    void drawRectangle()
    {
        const NodeId id = drawRect(QRectF(20, 40, 100, 50));
        auto* r = m_editor->document().findAs<RectangleObject>(id);
        QVERIFY(r);
        QVERIFY2(near(r->boundingRect(), QRectF(20, 40, 100, 50)), qPrintable(describe(r->boundingRect())));
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Rechteck erstellen"));
        // nach dem Zeichnen ist wieder das Auswahlwerkzeug aktiv
        QCOMPARE(m_canvas->toolId(), ToolId::Pick);
        QCOMPARE(m_editor->selection(), QList<NodeId>{id});
    }

    void moveSnapsToGuide()
    {
        m_editor->addGuide(Guide::vertical(100.0, 148.5));
        drawRect(QRectF(20, 40, 50, 30));
        // rechte Kante (70) bis knapp vor die Hilfslinie ziehen: rastet auf 100 ein
        drag(screen(QPointF(45, 55)), screen(QPointF(45 + 29.2, 55)), {}, QStringLiteral("10-einrasten-hilfslinie"));
        QVERIFY2(qAbs(bounds().right() - 100.0) < 1e-9, qPrintable(describe(bounds())));
    }

    void cornerScalesProportionally()
    {
        drawRect(QRectF(20, 40, 100, 50));
        screenshot(QStringLiteral("01-ausgewaehlt"));
        // untere rechte Ecke um die halbe Grösse nach aussen: 150 %, obere linke Ecke bleibt
        drag(handle(Qt::BottomRightCorner), handle(Qt::BottomRightCorner) + screen(QPointF(50, 25)) - screen(QPointF(0, 0)),
             {}, QStringLiteral("02-ecke-ziehen-vorschau"));
        QVERIFY2(near(bounds(), QRectF(20, 40, 150, 75)), qPrintable(describe(bounds())));
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Skalieren"));
        m_editor->document().undo();
        QVERIFY(near(bounds(), QRectF(20, 40, 100, 50)));
    }

    void cornerWithShiftScalesFromCenter()
    {
        drawRect(QRectF(20, 40, 100, 50));
        drag(handle(Qt::BottomRightCorner), handle(Qt::BottomRightCorner) + screen(QPointF(50, 25)) - screen(QPointF(0, 0)),
             Qt::ShiftModifier);
        // vom Mittelpunkt (70, 65) aus auf 200 %
        QVERIFY2(near(bounds(), QRectF(-30, 15, 200, 100)), qPrintable(describe(bounds())));
    }

    void sideStretchesOneDirection()
    {
        drawRect(QRectF(20, 40, 100, 50));
        drag(rightHandle(), rightHandle() + screen(QPointF(30, 0)) - screen(QPointF(0, 0)), {},
             QStringLiteral("03-seite-ziehen-vorschau"));
        QVERIFY2(near(bounds(), QRectF(20, 40, 130, 50)), qPrintable(describe(bounds())));
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Strecken"));
        // Das Rechteck ist danach wirklich 130 mm breit, ohne Skalierung in der Matrix
        auto* r = m_editor->document().findAs<RectangleObject>(m_editor->selection().first());
        QVERIFY(qAbs(r->size().width() - 130) < kTol && qAbs(r->size().height() - 50) < kTol);
        QVERIFY(qFuzzyCompare(r->transform().m11(), 1.0));
    }

    void moveByDragging()
    {
        drawRect(QRectF(20, 40, 100, 50));
        drag(screen(QPointF(70, 65)), screen(QPointF(90, 95)), {}, QStringLiteral("04-verschieben-vorschau"));
        QVERIFY2(near(bounds(), QRectF(40, 70, 100, 50)), qPrintable(describe(bounds())));
    }

    void secondClickRotates()
    {
        drawRect(QRectF(40, 60, 100, 50));
        const QPointF c = bounds().center(); // (90, 85)
        click(screen(c));                    // zweiter Klick auf die Auswahl: Drehmodus
        screenshot(QStringLiteral("05-drehmodus"));
        // obere rechte Ecke um 90° im Uhrzeigersinn um die Mitte führen
        const QPoint from = handle(Qt::TopRightCorner);
        const QPointF v = m_canvas->toDoc(from) - c;
        const QPointF rotated = c + QPointF(-v.y(), v.x());
        drag(from, screen(rotated), {}, QStringLiteral("06-drehen-vorschau"));
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Drehen"));
        QVERIFY2(near(bounds(), QRectF(65, 35, 50, 100)), qPrintable(describe(bounds())));
        screenshot(QStringLiteral("07-gedreht"));
        // nach dem Drehen bleibt der Drehmodus aktiv; nochmaliger Klick schaltet zurück
        click(screen(c));
        screenshot(QStringLiteral("08-zurueck-skalieren"));
    }

    void rotateSnapsWithCtrl()
    {
        drawRect(QRectF(40, 60, 100, 50));
        const QPointF c = bounds().center();
        click(screen(c));
        const QPoint from = handle(Qt::TopRightCorner);
        const QPointF v = m_canvas->toDoc(from) - c;
        // etwa 40° drehen, mit Strg auf 45° einrasten
        QTransform t;
        t.rotate(40);
        drag(from, screen(c + t.map(v)), Qt::ControlModifier);
        auto* r = m_editor->document().findAs<DrawObject>(m_editor->selection().first());
        const double angle = qRadiansToDegrees(std::atan2(r->transform().m12(), r->transform().m11()));
        QVERIFY2(qAbs(angle - 45.0) < 1e-6, qPrintable(QString::number(angle)));
    }

    void marqueeSelectsEnclosedOnly()
    {
        const NodeId a = drawRect(QRectF(20, 20, 30, 30));
        const NodeId b = drawRect(QRectF(60, 20, 30, 30));
        drawRect(QRectF(100, 20, 60, 30)); // ragt aus dem Rahmen
        m_editor->clearSelection();
        drag(screen(QPointF(10, 10)), screen(QPointF(120, 60)), {}, QStringLiteral("09-rahmen"));
        QCOMPARE(m_editor->selection(), (QList<NodeId>{a, b}));
    }

    void shiftClickTogglesSelection()
    {
        const NodeId a = drawRect(QRectF(20, 20, 30, 30));
        const NodeId b = drawRect(QRectF(60, 20, 30, 30));
        click(screen(QPointF(35, 35)));
        QTest::mouseClick(m_canvas.get(), Qt::LeftButton, Qt::ShiftModifier, screen(QPointF(75, 35)), 1000);
        QCOMPARE(m_editor->selection(), (QList<NodeId>{a, b}));
        QTest::mouseClick(m_canvas.get(), Qt::LeftButton, Qt::ShiftModifier, screen(QPointF(35, 35)), 1000);
        QCOMPARE(m_editor->selection(), (QList<NodeId>{b}));
    }

    void escapeClearsSelection()
    {
        drawRect(QRectF(20, 20, 30, 30));
        QTest::keyClick(m_canvas.get(), Qt::Key_Escape);
        QVERIFY(m_editor->selection().isEmpty());
    }

    void guidesAreGlobalOrPageSpecific()
    {
        m_editor->insertPage(1);
        m_editor->setCurrentPageIndex(0);
        m_editor->addGuide(Guide::horizontal(50, 0));
        m_editor->addGuide(Guide::vertical(70, 0));
        // Neue Hilfslinien gelten auf allen Seiten
        m_editor->setCurrentPageIndex(1);
        QCOMPARE(m_editor->guides().size(), 2);
        QVERIFY(!m_editor->isGuidePageSpecific(0));

        // Auf Seite 1: die senkrechte nur hier
        m_editor->setCurrentPageIndex(0);
        m_editor->setSelectedGuide(1);
        m_editor->setGuidePageSpecific(1, true);
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Hilfslinie nur auf dieser Seite"));
        QCOMPARE(m_editor->guides().size(), 2);
        QCOMPARE(m_editor->selectedGuide(), 1);
        QVERIFY(m_editor->isGuidePageSpecific(1));
        QCOMPARE(m_editor->guides().at(1), Guide::vertical(70, 0));
        m_editor->setCurrentPageIndex(1);
        QCOMPARE(m_editor->guides(), (QList<Guide>{Guide::horizontal(50, 0)}));

        // Seitenspezifische Hilfslinie verschieben und wieder global machen
        m_editor->setCurrentPageIndex(0);
        m_editor->setGuide(1, Guide::vertical(80, 0), QStringLiteral("verschieben"));
        QCOMPARE(m_editor->document().page(0).guides(), (QList<Guide>{Guide::vertical(80, 0)}));
        m_editor->setGuidePageSpecific(1, false);
        m_editor->setCurrentPageIndex(1);
        QCOMPARE(m_editor->guides().size(), 2);
        m_editor->document().undo();
        QCOMPARE(m_editor->guides().size(), 1);

        // Rechtsklick auf eine Hilfslinie mit dem Auswahlwerkzeug: Kontextmenü dafür
        m_editor->setCurrentPageIndex(0);
        int menuFor = -1;
        connect(m_canvas.get(), &Canvas::guideContextMenuRequested, m_canvas.get(), [&menuFor](int i, const QPoint&) { menuFor = i; });
        m_canvas->setTool(ToolId::Pick);
        QTest::mouseClick(m_canvas.get(), Qt::RightButton, {}, screen(QPointF(120, 50)));
        QCOMPARE(menuFor, 0);
        QCOMPARE(m_editor->selectedGuide(), 0);
    }

    void guideRotatesBySecondClick()
    {
        m_editor->addGuide(Guide::vertical(100.0, 148.5));
        const QPoint onGuide = screen(QPointF(100, 100));
        click(onGuide); // auswählen
        QCOMPARE(m_editor->selectedGuide(), 0);
        click(onGuide); // zweiter Klick: Drehgriffe
        screenshot(QStringLiteral("12-hilfslinie-drehgriffe"));
        QPointF a;
        QPointF b;
        QVERIFY(m_canvas->guideSegment(m_editor->guides().first(), 16.0, &a, &b));
        // oberes Ende um 45° im Uhrzeigersinn um den Anker führen, mit Strg eingerastet
        const QPointF top = a.y() < b.y() ? a : b;
        const QPointF c = m_canvas->toScreen(m_editor->guides().first().anchor);
        QTransform t;
        t.rotate(45);
        const QPointF target = c + t.map(top - c);
        drag(top.toPoint(), target.toPoint(), Qt::ControlModifier, QStringLiteral("13-hilfslinie-drehen-vorschau"));
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Hilfslinie drehen"));
        QVERIFY2(qAbs(m_editor->guides().first().angle - 45.0) < 1e-9,
                 qPrintable(QString::number(m_editor->guides().first().angle)));
        QCOMPARE(m_editor->guides().first().anchor, QPointF(100.0, 148.5));
        screenshot(QStringLiteral("14-hilfslinie-gedreht"));
        m_editor->document().undo();
        QVERIFY(m_editor->guides().first().isVertical());
    }

    void guideRotatesAroundFreePivot()
    {
        m_editor->addGuide(Guide::vertical(100.0, 148.5));
        const QPoint onGuide = screen(QPointF(100, 100));
        click(onGuide);
        click(onGuide);
        // Drehpunkt 40 mm neben die Linie ziehen (Mausraster: auf einen halben mm genau): die Hilfslinie selbst bleibt, kein Undo-Schritt
        drag(screen(QPointF(100, 148.5)), screen(QPointF(140, 148.5)));
        QCOMPARE(m_editor->guides().first().anchor, QPointF(100.0, 148.5));
        QVERIFY(m_editor->document().undoText() != QStringLiteral("Hilfslinie drehen"));
        screenshot(QStringLiteral("14b-hilfslinie-drehpunkt-daneben"));

        // oberes Ende um 90° im Uhrzeigersinn um den neuen Drehpunkt: Linie wird waagrecht,
        // ihr Abstand zum Drehpunkt (40 mm) bleibt, sie liegt nun 40 mm darüber
        QPointF a;
        QPointF b;
        QVERIFY(m_canvas->guideSegment(m_editor->guides().first(), 16.0, &a, &b));
        const QPointF top = a.y() < b.y() ? a : b;
        const QPointF c = m_canvas->toScreen(QPointF(140, 148.5));
        QTransform t;
        t.rotate(90);
        drag(top.toPoint(), (c + t.map(top - c)).toPoint(), Qt::ControlModifier);
        const Guide g = m_editor->guides().first();
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Hilfslinie drehen"));
        QVERIFY2(qAbs(g.angle) < 1e-9 || qAbs(g.angle - 180.0) < 1e-9, qPrintable(QString::number(g.angle)));
        QVERIFY2(qAbs(qAbs(g.distance(QPointF(140, 148.5))) - 40.0) < 0.5, qPrintable(QString::number(g.distance(QPointF(140, 148.5)))));
        QVERIFY2(qAbs(g.anchor.y() - 108.5) < 0.5, qPrintable(QString::number(g.anchor.y())));
    }

    void expressionsInFields()
    {
        QCOMPARE(evaluateExpression(QStringLiteral("297/2")).value(), 148.5);
        QCOMPARE(evaluateExpression(QStringLiteral("(210-20)*0,5")).value(), 95.0);
        QCOMPARE(evaluateExpression(QStringLiteral("-3+4 mm")).value(), 1.0);
        QCOMPARE(evaluateExpression(QStringLiteral("2*-3")).value(), -6.0);
        QCOMPARE(evaluateExpression(QStringLiteral("1'000/4")).value(), 250.0);
        QVERIFY(!evaluateExpression(QStringLiteral("297/")));
        QVERIFY(!evaluateExpression(QStringLiteral("5/0")));
        QVERIFY(!evaluateExpression(QStringLiteral("(1+2")));
    }

    void freehandDrawsAndContinuesLines()
    {
        m_canvas->setTool(ToolId::Freehand);
        click(screen(QPointF(30, 60)));
        click(screen(QPointF(80, 60)));
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Linie zeichnen"));
        const NodeId id = m_editor->selection().value(0);
        auto curve = [&] { return m_editor->document().findAs<CurveObject>(id); };
        QVERIFY(curve());
        QCOMPARE(curve()->nodeCount(), 2);
        QCOMPARE(m_canvas->toolId(), ToolId::Freehand); // Werkzeug bleibt aktiv
        QVERIFY(curve()->style().fill.isNone());

        // Doppelklick auf das Ende, dann Klick: nächstes Segment desselben Linienzugs
        QTest::mouseDClick(m_canvas.get(), Qt::LeftButton, {}, screen(QPointF(80, 60)), 1000);
        click(screen(QPointF(80, 110)));
        QCOMPARE(curve()->nodeCount(), 3);
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Kurve fortsetzen"));

        // Am Ende weiter und auf dem Anfang beenden: Kurve schliesst sich
        click(screen(QPointF(80, 110)));
        click(screen(QPointF(30, 60)));
        QCOMPARE(curve()->nodeCount(), 3);
        QVERIFY(curve()->subPaths().first().closed);
        screenshot(QStringLiteral("21-freihand-dreieck"));
    }

    void freehandDragDrawsFreeLine()
    {
        m_canvas->setTool(ToolId::Freehand);
        const QPoint from = screen(QPointF(30, 150));
        press(from);
        // Bogen: viele Mauspunkte, vereinfacht bleiben nur die nötigen Knoten
        for (int i = 1; i <= 40; ++i) {
            const double a = M_PI * i / 40;
            move(screen(QPointF(30 + 40 * (1 - std::cos(a)), 150 - 30 * std::sin(a))));
        }
        release(screen(QPointF(110, 150)));
        const auto* c = m_editor->document().findAs<CurveObject>(m_editor->selection().value(0));
        QVERIFY(c);
        // Wenige Knoten mit Bézierkurven, die dem Bogen folgen
        QVERIFY2(c->nodeCount() >= 2 && c->nodeCount() < 12, qPrintable(QString::number(c->nodeCount())));
        bool curved = false;
        for (const CurveNode& n : c->subPaths().first().nodes)
            curved = curved || n.curveIn;
        QVERIFY(curved);
        QVERIFY2(near(c->boundingRect(), QRectF(30, 120, 80, 30), 1.2), qPrintable(describe(c->boundingRect())));
    }

    void lineToolDrawsStraightLine()
    {
        m_canvas->setTool(ToolId::Line);
        drag(screen(QPointF(30, 60)), screen(QPointF(90, 75)));
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Linie zeichnen"));
        const auto* c = m_editor->document().findAs<CurveObject>(m_editor->selection().value(0));
        QVERIFY(c);
        QCOMPARE(c->nodeCount(), 2);
        QCOMPARE(m_canvas->toolId(), ToolId::Line);

        // Strg: Richtung rastet in 15°-Schritten ein (hier waagrecht)
        drag(screen(QPointF(30, 120)), screen(QPointF(90, 123)), Qt::ControlModifier);
        const auto* h = m_editor->document().findAs<CurveObject>(m_editor->selection().value(0));
        const QPolygonF pts = h->transform().map(h->localPath()).toFillPolygon();
        QVERIFY(qAbs(pts.at(0).y() - pts.at(1).y()) < 1e-6);
    }

    void bezierToolDrawsCurve()
    {
        m_canvas->setTool(ToolId::Bezier);
        click(screen(QPointF(30, 60)));                                   // Eckknoten
        drag(screen(QPointF(80, 60)), screen(QPointF(100, 60)));          // glatter Knoten
        QVERIFY(m_editor->selection().isEmpty()); // noch nichts angelegt
        QTest::mouseDClick(m_canvas.get(), Qt::LeftButton, {}, screen(QPointF(130, 100)), 1000);
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Kurve zeichnen"));
        const NodeId id = m_editor->selection().value(0);
        const auto* c = m_editor->document().findAs<CurveObject>(id);
        QVERIFY(c);
        QCOMPARE(c->nodeCount(), 3);
        QVERIFY(c->node(NodeRef{0, 1}).curveIn);  // Strecke in den gezogenen Knoten ist Kurve
        QVERIFY(c->node(NodeRef{0, 2}).curveIn);  // die nächste beginnt mit dessen Steuerpunkt
        QCOMPARE(c->node(NodeRef{0, 1}).kind, NodeKind::Symmetric);
        QVERIFY(c->style().fill.isNone());
        screenshot(QStringLiteral("24-bezier"));

        // Neue Kurve, Klick auf den ersten Knoten schliesst sie
        m_editor->setSelection({});
        click(screen(QPointF(30, 150)));
        click(screen(QPointF(90, 150)));
        click(screen(QPointF(60, 190)));
        click(screen(QPointF(30, 150)));
        const auto* t = m_editor->document().findAs<CurveObject>(m_editor->selection().value(0));
        QVERIFY(t && t->id() != id);
        QCOMPARE(t->nodeCount(), 3);
        QVERIFY(t->subPaths().first().closed);

        // Esc verwirft eine angefangene Kurve
        click(screen(QPointF(150, 150)));
        click(screen(QPointF(170, 150)));
        QTest::keyClick(m_canvas.get(), Qt::Key_Escape);
        m_canvas->setTool(ToolId::Pick); // Werkzeugwechsel würde eine offene Kurve anlegen
        QCOMPARE(m_editor->selection(), QList<NodeId>{t->id()});
    }

    void cropToolCropsToFrame()
    {
        const NodeId inside = drawRect(QRectF(20, 40, 100, 50));
        drawRect(QRectF(150, 200, 20, 20)); // liegt ganz ausserhalb des Rahmens
        m_editor->clearSelection();
        m_canvas->setTool(ToolId::Crop);
        drag(screen(QPointF(50, 30)), screen(QPointF(160, 70)));
        CropTool& crop = m_canvas->cropTool();
        QVERIFY(crop.frame());
        screenshot(QStringLiteral("25-beschneiden-rahmen"));
        // Rahmen am rechten Griff schmaler ziehen
        drag(screen(QPointF(160, 50)), screen(QPointF(150, 50)));
        QVERIFY(std::abs(crop.frame()->right() - 150) < 0.5);
        QTest::mouseDClick(m_canvas.get(), Qt::LeftButton, {}, screen(QPointF(100, 50)), 1000);
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Beschneiden"));
        QVERIFY(!crop.frame());
        QVERIFY(!m_editor->document().find(inside));
        // Ohne Auswahl wirkt Beschneiden auf die ganze Seite: das äussere Rechteck ist weg
        const Layer& layer = *node_cast<Layer>(m_editor->currentPage().child(0));
        QCOMPARE(layer.childCount(), 1);
        const auto* c = node_cast<CurveObject>(layer.child(0));
        QVERIFY(c);
        QVERIFY2(near(c->boundingRect(), QRectF(50, 40, 70, 30)), qPrintable(describe(c->boundingRect())));
        QCOMPARE(c->nodeCount(), 4);
        screenshot(QStringLiteral("26-beschneiden-ergebnis"));
    }

    void knifeSplitsObjects()
    {
        drawRect(QRectF(20, 40, 100, 50));
        m_canvas->setTool(ToolId::Knife);
        click(screen(QPointF(60, 30)));
        click(screen(QPointF(60, 100)));
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Messer"));
        const QList<const DrawObject*> pieces = m_editor->selectedObjects();
        QCOMPARE(pieces.size(), 2);
        QList<double> widths{pieces[0]->boundingRect().width(), pieces[1]->boundingRect().width()};
        std::sort(widths.begin(), widths.end());
        QVERIFY2(std::abs(widths[0] - 40) < 0.5 && std::abs(widths[1] - 60) < 0.5, qPrintable(QStringLiteral("%1 %2").arg(widths[0]).arg(widths[1])));

        // Freier Schnitt durch ein Stück: wieder zwei Teile
        m_editor->clearSelection();
        press(screen(QPointF(90, 30)));
        for (int i = 0; i <= 10; ++i)
            move(screen(QPointF(90 + 5 * std::sin(i * 0.6), 30 + 7 * i)));
        release(screen(QPointF(92, 100)));
        QCOMPARE(m_editor->selectedObjects().size(), 2);
        screenshot(QStringLiteral("27-messer"));
    }

    void eraserBitesOutOfShapes()
    {
        const NodeId id = drawRect(QRectF(20, 40, 100, 50));
        m_canvas->setTool(ToolId::Eraser);
        EraserTool& eraser = m_canvas->eraserTool();
        eraser.setWidth(10.0);
        drag(screen(QPointF(40, 40)), screen(QPointF(70, 40)), {}, QStringLiteral("28-radierer-vorschau"));
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Radieren"));
        QVERIFY(!m_editor->document().find(id));
        const auto* c = m_editor->document().findAs<CurveObject>(m_editor->selection().value(0));
        QVERIFY(c);
        QVERIFY(!c->path().contains(QPointF(55, 42)));  // weggeradiert
        QVERIFY(c->path().contains(QPointF(55, 60)));   // bleibt
        QVERIFY(c->path().contains(QPointF(100, 42)));  // neben der Spur bleibt
        QVERIFY2(c->nodeCount() < 20, qPrintable(QString::number(c->nodeCount())));
        screenshot(QStringLiteral("29-radierer"));

        // Durch und durch radiert: ein Objekt mit zwei Teilen
        drag(screen(QPointF(100, 30)), screen(QPointF(100, 100)));
        const auto* split = m_editor->document().findAs<CurveObject>(m_editor->selection().value(0));
        QVERIFY(split);
        QCOMPARE(split->subPaths().size(), 2);
    }

    void artisticMediaDrawsStrokes()
    {
        m_canvas->setTool(ToolId::ArtisticMedia);
        ArtisticMediaTool& tool = m_canvas->mediaTool();
        ArtisticMedia media = tool.media();
        media.kind = ArtisticMedia::Kind::Preset;
        media.preset = 0;
        media.width = 8.0;
        tool.setMedia(media);
        const QPoint from = screen(QPointF(30, 150));
        press(from);
        for (int i = 1; i <= 40; ++i)
            move(screen(QPointF(30 + 2.5 * i, 150 - 20 * std::sin(i * M_PI / 20))));
        release(screen(QPointF(130, 150)));
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Künstlerische Medien"));
        const NodeId id = m_editor->selection().value(0);
        const auto* c = m_editor->document().findAs<CurveObject>(id);
        QVERIFY(c && c->media().isActive());
        QVERIFY2(c->nodeCount() <= 10, qPrintable(QString::number(c->nodeCount()))); // geglättet
        QVERIFY(!c->style().stroke);
        QVERIFY(!c->style().fill.isNone());
        screenshot(QStringLiteral("30-kuenstlerische-medien"));

        // Einstellungen gelten auch für die gewählte Medienkurve
        media.width = 3.0;
        tool.setMedia(media);
        QCOMPARE(m_editor->document().findAs<CurveObject>(id)->media().width, 3.0);
        // Kalligrafie
        media.kind = ArtisticMedia::Kind::Calligraphy;
        media.angle = 45;
        tool.setMedia(media);
        screenshot(QStringLiteral("31-kalligrafie"));

        // Strg+Q bricht die Medien auf: der Umriss wird zur gewöhnlichen Kurve
        m_editor->convertSelectionToCurves();
        const auto* broken = m_editor->document().findAs<CurveObject>(m_editor->selection().value(0));
        QVERIFY(broken && !broken->media().isActive());
        QVERIFY(broken->subPaths().first().closed);
    }

    // Stapelreihenfolge der Ebene als Liste von Ids (hinten nach vorn)
    QList<NodeId> stackOrder() const
    {
        const Node* layer = m_editor->currentPage().child(0);
        QList<NodeId> ids;
        for (int i = 0; i < layer->childCount(); ++i)
            ids << layer->child(i)->id();
        return ids;
    }

    void arrangeChangesStackingOrder()
    {
        const NodeId a = drawRect(QRectF(20, 40, 40, 40));
        const NodeId b = drawRect(QRectF(30, 50, 40, 40));
        const NodeId c = drawRect(QRectF(40, 60, 40, 40));
        QCOMPARE(stackOrder(), (QList<NodeId>{a, b, c}));
        m_editor->setSelection({a});
        QVERIFY(!m_editor->canArrange(Editor::Arrange::BackOne)); // liegt schon ganz hinten
        m_editor->arrangeSelection(Editor::Arrange::ForwardOne);
        QCOMPARE(stackOrder(), (QList<NodeId>{b, a, c}));
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Eins nach vorn"));
        m_editor->arrangeSelection(Editor::Arrange::ToLayerFront);
        QCOMPARE(stackOrder(), (QList<NodeId>{b, c, a}));
        m_editor->arrangeSelection(Editor::Arrange::ToPageBack);
        QCOMPARE(stackOrder(), (QList<NodeId>{a, b, c}));
        m_editor->setSelection({a, b, c});
        m_editor->arrangeSelection(Editor::Arrange::Reverse);
        QCOMPARE(stackOrder(), (QList<NodeId>{c, b, a}));
        m_editor->document().undo();
        QCOMPARE(stackOrder(), (QList<NodeId>{a, b, c}));

        // "Hinter ...": Auswahl wählen, dann das Objekt anklicken, hinter das sie soll
        m_editor->setSelection({c});
        m_canvas->pickTool().beginPlaceRelative(false);
        click(screen(QPointF(25, 45))); // nur a liegt dort
        QCOMPARE(stackOrder(), (QList<NodeId>{c, a, b}));
    }

    void lockedObjectsStayPut()
    {
        const NodeId id = drawRect(QRectF(20, 40, 60, 40));
        m_editor->setSelectionLocked(true);
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Objekt sperren"));
        QVERIFY(m_editor->isLocked(id));
        const QRectF before = m_editor->selectionBounds();
        // Ziehen, Pfeiltaste, Löschen und Radieren lassen es unverändert
        drag(screen(QPointF(50, 60)), screen(QPointF(90, 90)));
        QTest::keyClick(m_canvas.get(), Qt::Key_Right);
        m_editor->deleteSelection();
        QVERIFY(m_editor->document().find(id));
        QCOMPARE(m_editor->selectionBounds(), before);
        QVERIFY(m_editor->isSelected(id)); // bleibt aber wählbar
        screenshot(QStringLiteral("32-gesperrt"));
        // Gespeichert und geladen bleibt es gesperrt
        QString error;
        std::unique_ptr<Document> loaded = io::loadFromBytes(io::saveToBytes(m_editor->document(), &error), &error);
        QVERIFY(loaded && loaded->findAs<DrawObject>(id)->locked());
        m_editor->setSelectionLocked(false);
        QTest::keyClick(m_canvas.get(), Qt::Key_Right);
        QVERIFY(m_editor->selectionBounds().left() > before.left());
    }

    void combineAndBreakApart()
    {
        drawRect(QRectF(20, 40, 30, 30));
        const NodeId second = drawRect(QRectF(80, 40, 30, 30));
        m_editor->selectAll();
        QVERIFY(m_editor->canCombineSelection());
        m_editor->combineSelection();
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Kombinieren"));
        const auto* combined = m_editor->document().findAs<CurveObject>(m_editor->selection().value(0));
        QVERIFY(combined);
        QCOMPARE(combined->subPaths().size(), 2);
        QVERIFY(!m_editor->document().find(second));
        m_editor->breakApartSelection();
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Kombination aufheben"));
        QCOMPARE(m_editor->selection().size(), 2);
        QVERIFY(!m_editor->canBreakApartSelection());
    }

    void outlineGetsArrowsAndDashes()
    {
        m_canvas->setTool(ToolId::Freehand);
        click(screen(QPointF(30, 60)));
        click(screen(QPointF(120, 60)));
        m_editor->setSelectionStrokeWidth(1.0);
        m_editor->setSelectionOutlineShape(std::nullopt, 5, 2);
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Pfeilspitze"));
        m_editor->setSelectionOutlineShape(1, std::nullopt, std::nullopt);
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Linienstil"));
        const Style& s = m_editor->selectedObjects().first()->style();
        QCOMPARE(s.startArrow, 5);
        QCOMPARE(s.endArrow, 2);
        QCOMPARE(s.dash, 1);
        m_canvas->setTool(ToolId::Pick);
        screenshot(QStringLiteral("33-pfeilspitzen"));
    }

    void ellipseToolDrawsEllipsesAndCircles()
    {
        m_canvas->setTool(ToolId::Ellipse);
        drag(screen(QPointF(20, 40)), screen(QPointF(80, 70)));
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Ellipse erstellen"));
        const auto* e = m_editor->document().findAs<EllipseObject>(m_editor->selection().value(0));
        QVERIFY(e);
        QVERIFY2(near(e->boundingRect(), QRectF(20, 40, 60, 30)), qPrintable(describe(e->boundingRect())));
        QCOMPARE(m_canvas->toolId(), ToolId::Pick);

        // Strg: Kreis
        m_canvas->setTool(ToolId::Ellipse);
        drag(screen(QPointF(100, 40)), screen(QPointF(160, 70)), Qt::ControlModifier);
        const auto* circle = m_editor->document().findAs<EllipseObject>(m_editor->selection().value(0));
        QVERIFY(circle);
        QVERIFY(std::abs(circle->size().width() - circle->size().height()) < 1e-6);

        // Kreissegment über die Eigenschaft wie in der Leiste
        m_editor->setObjectProperty(QStringLiteral("ellipseKind"), 1, QStringLiteral("Ellipsenart"));
        m_editor->setObjectProperty(QStringLiteral("endAngle"), 90.0, QStringLiteral("Endwinkel"));
        QCOMPARE(m_editor->document().findAs<EllipseObject>(circle->id())->kind(), EllipseObject::Kind::Pie);
        // Vorgabe für neue Ellipsen: Bogen
        m_editor->clearSelection();
        m_editor->shapeDefaults().ellipseKind = EllipseObject::Kind::Arc;
        m_canvas->setTool(ToolId::Ellipse);
        drag(screen(QPointF(20, 120)), screen(QPointF(80, 160)));
        QCOMPARE(m_editor->document().findAs<EllipseObject>(m_editor->selection().value(0))->kind(), EllipseObject::Kind::Arc);
        screenshot(QStringLiteral("34-ellipsen"));
    }

    void polygonToolsDrawPolygonsAndStars()
    {
        m_canvas->setTool(ToolId::Polygon);
        drag(screen(QPointF(20, 40)), screen(QPointF(70, 90)));
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Polygon erstellen"));
        auto polygon = [this] { return m_editor->document().findAs<PolygonObject>(m_editor->selection().value(0)); };
        QVERIFY(polygon() && polygon()->points() == 5);
        m_editor->setObjectProperty(QStringLiteral("points"), 8.0, QStringLiteral("Punkte"));
        QCOMPARE(polygon()->points(), 8);

        m_canvas->setTool(ToolId::Star);
        drag(screen(QPointF(90, 40)), screen(QPointF(140, 90)));
        QVERIFY(polygon() && polygon()->kind() == PolygonObject::Kind::Star && polygon()->sharpness() == 53);

        m_editor->clearSelection();
        m_editor->shapeDefaults().complexStarPoints = 7;
        m_canvas->setTool(ToolId::ComplexStar);
        drag(screen(QPointF(20, 110)), screen(QPointF(70, 160)));
        QVERIFY(polygon() && polygon()->kind() == PolygonObject::Kind::ComplexStar && polygon()->points() == 7);
        screenshot(QStringLiteral("35-polygone"));

        // In Kurven konvertieren behält die Form
        const QRectF before = polygon()->boundingRect();
        m_editor->convertSelectionToCurves();
        const auto* c = m_editor->document().findAs<CurveObject>(m_editor->selection().value(0));
        QVERIFY(c && near(c->boundingRect(), before));
    }

    void textToolCreatesAndEditsText()
    {
        // Klick: Grafiktext, Grundlinie am Klickpunkt
        m_canvas->setTool(ToolId::Text);
        TextTool& tool = m_canvas->textTool();
        click(screen(QPointF(30, 60)));
        QVERIFY(tool.editing());
        QTest::keyClicks(m_canvas.get(), QStringLiteral("Hier kommt mein Titeltext"));
        QCOMPARE(tool.text(), QStringLiteral("Hier kommt mein Titeltext"));
        screenshot(QStringLiteral("36-grafiktext-tippen"));
        QTest::keyClick(m_canvas.get(), Qt::Key_Escape);
        QVERIFY(!tool.editing());
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Text erstellen"));
        const NodeId titleId = m_editor->selection().value(0);
        const auto* title = m_editor->document().findAs<TextObject>(titleId);
        QVERIFY(title && title->kind() == TextObject::Kind::Artistic);
        QCOMPARE(title->plainText(), QStringLiteral("Hier kommt mein Titeltext"));
        const QRectF titleBox = title->boundingRect();
        QVERIFY2(titleBox.top() < 60 && titleBox.bottom() > 60 && std::abs(titleBox.left() - 30) < 1,
                 qPrintable(describe(titleBox)));

        // Ziehen: Mengentextrahmen, Text bricht darin um
        drag(screen(QPointF(20, 100)), screen(QPointF(90, 160)));
        QVERIFY(tool.editing());
        QCOMPARE(tool.editingKind(), TextObject::Kind::Paragraph);
        QTest::keyClicks(m_canvas.get(), QStringLiteral("Stand 21.10.2026 abends, nach dem Entfernen der 24 Snapshots und der 53 internen Snapshots."));
        QTest::keyClick(m_canvas.get(), Qt::Key_Return);
        QTest::keyClicks(m_canvas.get(), QStringLiteral("Zweiter Absatz"));
        screenshot(QStringLiteral("37-mengentext-tippen"));
        QTest::keyClick(m_canvas.get(), Qt::Key_Escape);
        const auto* para = m_editor->document().findAs<TextObject>(m_editor->selection().value(0));
        QVERIFY(para && para->kind() == TextObject::Kind::Paragraph);
        QVERIFY2(near(para->boundingRect(), QRectF(20, 100, 70, 60)), qPrintable(describe(para->boundingRect())));
        QCOMPARE(para->paragraphs().size(), 2);
        QVERIFY(para->localPath().boundingRect().height() > 10); // mehrere Zeilen

        // Klick in vorhandenen Text: bearbeiten, erstes Wort rot und fett
        click(screen(QPointF(31, 58)));
        QVERIFY(tool.editing());
        QCOMPARE(tool.target(), titleId);
        QTest::keyClick(m_canvas.get(), Qt::Key_Home, Qt::ControlModifier);
        QTest::keyClick(m_canvas.get(), Qt::Key_Right, Qt::ControlModifier | Qt::ShiftModifier);
        tool.applyFormat([](CharFormat& f) { f.color = Color(255, 0, 0); });
        QTest::keyClick(m_canvas.get(), Qt::Key_B, Qt::ControlModifier);
        QTest::keyClick(m_canvas.get(), Qt::Key_End, Qt::ControlModifier);
        QTest::keyClicks(m_canvas.get(), QStringLiteral("!"));
        QTest::keyClick(m_canvas.get(), Qt::Key_Delete); // Entf am Ende: nichts, das Objekt bleibt
        screenshot(QStringLiteral("38-text-formatieren"));
        QTest::keyClick(m_canvas.get(), Qt::Key_Escape);
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Text bearbeiten"));
        title = m_editor->document().findAs<TextObject>(titleId);
        QVERIFY(title);
        QCOMPARE(title->plainText(), QStringLiteral("Hier kommt mein Titeltext!"));
        const TextRun first = title->paragraphs().first().runs.first();
        QCOMPARE(first.text, QStringLiteral("Hier "));
        QVERIFY(first.format.bold && first.format.color == Color(255, 0, 0));
        QVERIFY(!title->paragraphs().first().runs.last().format.bold);
        QCOMPARE(title->pieces().size(), 2);

        // Doppelklick mit dem Auswahlwerkzeug: Text an dieser Stelle bearbeiten
        m_canvas->setTool(ToolId::Pick);
        QTest::mouseDClick(m_canvas.get(), Qt::LeftButton, {}, screen(QPointF(31, 58)));
        QCOMPARE(m_canvas->toolId(), ToolId::Text);
        QVERIFY(tool.editing() && tool.target() == titleId);
        QTest::keyClick(m_canvas.get(), Qt::Key_Escape);
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Text bearbeiten")); // unverändert: kein neuer Schritt

        // In Kurven: je Farbe eine Kurve, zusammen als Gruppe; rot bleibt rot
        m_editor->setSelection({titleId});
        m_editor->convertSelectionToCurves();
        const auto* group = m_editor->document().findAs<GroupObject>(m_editor->selection().value(0));
        QVERIFY(group && group->childCount() == 2);
        bool red = false;
        for (int i = 0; i < group->childCount(); ++i) {
            const auto* c = node_cast<CurveObject>(group->child(i));
            QVERIFY(c);
            red = red || c->style().fill == Fill::solid(Color(255, 0, 0));
        }
        QVERIFY(red);
        m_editor->document().undo();
        QVERIFY(m_editor->document().findAs<TextObject>(titleId));

        // Rückgängig nimmt die Bearbeitung als Ganzes zurück
        m_editor->document().undo();
        QCOMPARE(m_editor->document().findAs<TextObject>(titleId)->plainText(), QStringLiteral("Hier kommt mein Titeltext"));

        // Silbentrennung im schmalen Rahmen (Sichtprüfung per Bildschirmfoto)
        auto hyphenated = std::make_unique<TextObject>(TextObject::Kind::Paragraph, QSizeF(32, 40));
        hyphenated->setPlainText(QStringLiteral("Die Silbentrennung im Zeichenprogramm funktioniert wunderbar"),
                                 CharFormat{QStringLiteral("Arial"), 12.0});
        hyphenated->setHyphenate(true);
        hyphenated->setTransform(QTransform::fromTranslate(120, 100));
        Style black;
        black.fill = Fill::solid(Color(0, 0, 0));
        black.stroke.reset();
        hyphenated->setStyle(black);
        m_editor->addObject(std::move(hyphenated), QStringLiteral("Text"));
        screenshot(QStringLiteral("39-silbentrennung"));

        // Text leeren und beenden: das Objekt verschwindet
        m_canvas->textTool().beginEditing(titleId);
        QTest::keyClick(m_canvas.get(), Qt::Key_A, Qt::ControlModifier);
        QTest::keyClick(m_canvas.get(), Qt::Key_Backspace);
        QTest::keyClick(m_canvas.get(), Qt::Key_Escape);
        QVERIFY(!m_editor->document().findAs<TextObject>(titleId));
    }

    void textFramesLinkAcrossPages()
    {
        const QString story = QStringLiteral(
            "Vorgaben vom 26.10.2026, umgesetzt im neuen Rechner. Der alte Rechner bleibt unberuehrt, damit "
            "er reproduzierbar bleibt. Zahlintervalle hoechstens 1, 3, 6, 12 und 24 Monate; 36 Monate "
            "entfallen. Laufzeitrabatt 2, 4, 15 und 20 Prozent, Gesamtdeckel 20 Prozent.");
        TextTool& tool = m_canvas->textTool();
        m_canvas->setTool(ToolId::Text);
        drag(screen(QPointF(20, 40)), screen(QPointF(80, 60)));
        QVERIFY(tool.editing());
        QTest::keyClicks(m_canvas.get(), story);
        QVERIFY(tool.overflows());
        QTest::keyClick(m_canvas.get(), Qt::Key_Escape);
        const NodeId first = m_editor->selection().value(0);
        QVERIFY(m_editor->document().findAs<TextObject>(first)->overflows());
        m_canvas->setTool(ToolId::Pick);
        m_editor->setSelection({first});
        screenshot(QStringLiteral("51-ueberlauf"));

        // Untere Lasche anklicken, dann den Folgerahmen aufziehen
        click(screen(QPointF(50, 60)));
        QCOMPARE(m_canvas->toolId(), ToolId::Text);
        QVERIFY(tool.linking());
        drag(screen(QPointF(100, 40)), screen(QPointF(200, 140))); // reichlich Platz, auch mit Ersatzschrift
        QVERIFY(!tool.linking());
        QCOMPARE(m_canvas->toolId(), ToolId::Pick);
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Text verknüpfen"));
        const NodeId second = m_editor->selection().value(0);
        const auto* b = m_editor->document().findAs<TextObject>(second);
        QVERIFY(b && b->isFollower() && b->chainHead() == first);
        QVERIFY(!b->pieces().isEmpty());
        QVERIFY(!m_editor->document().findAs<TextObject>(first)->overflows());
        QVERIFY2(near(b->boundingRect(), QRectF(100, 40, 100, 100)), qPrintable(describe(b->boundingRect())));
        screenshot(QStringLiteral("52-verknuepft"));
        // Verknüpfte Lasche des ersten Rahmens: keine zweite Verbindung (nur 1:1)
        m_editor->setSelection({first});
        click(screen(QPointF(50, 60)));
        QCOMPARE(m_canvas->toolId(), ToolId::Pick);
        QVERIFY(!tool.linking());
        QVERIFY(!m_editor->linkNewTextFrame(first, QRectF(10, 200, 40, 20)));
        m_editor->setSelection({second});

        // Klick in den zweiten Rahmen bearbeitet den Text der ganzen Kette
        m_canvas->setTool(ToolId::Text);
        tool.beginEditing(second);
        QCOMPARE(tool.target(), first);
        QTest::keyClick(m_canvas.get(), Qt::Key_End, Qt::ControlModifier);
        QTest::keyClicks(m_canvas.get(), QStringLiteral(" ENDE"));
        screenshot(QStringLiteral("53-kette-bearbeiten"));
        QTest::keyClick(m_canvas.get(), Qt::Key_Escape);
        QVERIFY(m_editor->document().findAs<TextObject>(first)->plainText().endsWith(QStringLiteral("ENDE")));
        QVERIFY(m_editor->document().findAs<TextObject>(second)->plainText().isEmpty());

        // Weiter auf der nächsten Seite: Klick ins Leere setzt einen gleich grossen Rahmen
        m_editor->insertPage(1);
        m_editor->setCurrentPageIndex(1);
        tool.beginLinking(second, false);
        click(screen(QPointF(30, 30)));
        const NodeId third = m_editor->selection().value(0);
        QVERIFY(third != second);
        QCOMPARE(m_editor->document().findAs<TextObject>(first)->chain(), (QList<NodeId>{first, second, third}));
        QCOMPARE(m_editor->document().pageOf(third), &m_editor->document().page(1));
        QVERIFY2(near(m_editor->document().findAs<TextObject>(third)->boundingRect(), QRectF(30, 30, 100, 100)),
                 qPrintable(describe(m_editor->document().findAs<TextObject>(third)->boundingRect())));

        // Mittleren Rahmen löschen: die Kette schliesst sich
        m_editor->setCurrentPageIndex(0);
        m_editor->setSelection({second});
        m_editor->deleteSelection();
        QCOMPARE(m_editor->document().findAs<TextObject>(first)->chain(), (QList<NodeId>{first, third}));
        m_editor->document().undo();
        QCOMPARE(m_editor->document().findAs<TextObject>(first)->chain(), (QList<NodeId>{first, second, third}));
        // Ersten Rahmen löschen: der zweite übernimmt den Text
        m_editor->setSelection({first});
        m_editor->deleteSelection();
        QVERIFY(m_editor->document().findAs<TextObject>(second)->plainText().endsWith(QStringLiteral("ENDE")));
        QCOMPARE(m_editor->document().findAs<TextObject>(second)->chain(), (QList<NodeId>{second, third}));
        m_editor->document().undo();

        // Verknüpfung aufheben: jeder Rahmen behält den Text, der darin steht
        m_editor->setSelection({second});
        QVERIFY(m_editor->canUnlinkSelectedTextFrames());
        m_editor->unlinkSelectedTextFrames();
        const auto* a = m_editor->document().findAs<TextObject>(first);
        b = m_editor->document().findAs<TextObject>(second);
        QVERIFY(a->chain().isEmpty() && !b->isFollower());
        QVERIFY(!a->plainText().isEmpty() && !b->plainText().isEmpty());
        QCOMPARE(a->plainText() + b->plainText() + m_editor->document().findAs<TextObject>(third)->plainText(),
                 story + QStringLiteral(" ENDE"));

        // Text > Verknüpfen: ausgewählte Rahmen in Auswahlreihenfolge
        m_editor->setSelection({first, second});
        QVERIFY(m_editor->canLinkSelectedTextFrames());
        m_editor->linkSelectedTextFrames();
        QCOMPARE(m_editor->document().findAs<TextObject>(first)->chain(), (QList<NodeId>{first, second}));
        QVERIFY(m_editor->document().findAs<TextObject>(second)->plainText().isEmpty());
    }

    void importedImagesCanBeCropped()
    {
        QImage source(300, 150, QImage::Format_RGB32);
        source.fill(QColor(0x30, 0x80, 0xd0));
        for (int y = 0; y < 150; ++y)
            for (int x = 0; x < 150; ++x)
                source.setPixel(x, y, qRgb(0xf0, 0xa0, 0x20));
        source.setDotsPerMeterX(int(150 / 0.0254)); // 150 dpi: 50.8 x 25.4 mm
        source.setDotsPerMeterY(int(150 / 0.0254));
        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        source.save(&buffer, "PNG");

        const NodeId id = m_editor->importImage(png, QPointF(80, 80));
        QVERIFY(id != InvalidNodeId);
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Importieren"));
        const auto* image = m_editor->document().findAs<ImageObject>(id);
        QVERIFY(image);
        QVERIFY2(near(image->boundingRect(), QRectF(80 - 25.4, 80 - 12.7, 50.8, 25.4)), qPrintable(describe(image->boundingRect())));
        QCOMPARE(m_editor->selection(), QList<NodeId>{id});
        QCOMPARE(m_editor->importImage(QByteArray("kein Bild")), InvalidNodeId);
        screenshot(QStringLiteral("46-bild-importiert"));

        // Formwerkzeug: Ecke oben rechts nach innen ziehen beschneidet das Bild
        m_canvas->setTool(ToolId::Shape);
        const QPointF corner(80 + 25.4, 80 - 12.7);
        drag(screen(corner), screen(corner + QPointF(-15, 8)));
        image = m_editor->document().findAs<ImageObject>(id);
        QVERIFY(image->isCropped());
        QVERIFY(!image->hitTest(corner + QPointF(-1, 1), 0.1, false));
        QVERIFY(image->hitTest(QPointF(70, 85), 0.1, false));
        screenshot(QStringLiteral("47-bild-beschnitten"));
        m_editor->document().undo();
        QVERIFY(!m_editor->document().findAs<ImageObject>(id)->isCropped());
        m_canvas->setTool(ToolId::Pick);

        // Skalieren und Drehen bleiben in der Matrix, das Bild selbst bleibt unverändert
        m_editor->scaleSelection(2.0, 2.0);
        image = m_editor->document().findAs<ImageObject>(id);
        QVERIFY(std::abs(image->size().width() - 50.8) < 0.01); // dpi steht in Punkten je Meter (gerundet)
        QVERIFY(std::abs(image->boundingRect().width() - 2 * image->size().width()) < 1e-6);

        // SuperTrace: Dialog rechnet, Ergebnis liegt als Gruppe deckungsgleich über dem Bild
        TraceDialog dialog(image->image(), trace::Preset::Logo);
        dialog.resize(900, 600);
        dialog.recompute();
        QCOMPARE(dialog.result().colors, 2);
        if (!qEnvironmentVariable("VEK_SCREENSHOT_DIR").isEmpty())
            dialog.grab().save(qEnvironmentVariable("VEK_SCREENSHOT_DIR") + QStringLiteral("/48-supertrace.png"));
        const NodeId group = m_editor->vectorizeImage(id, dialog.result(), false);
        QVERIFY(group != InvalidNodeId);
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Bitmap vektorisieren"));
        const auto* traced = m_editor->document().findAs<GroupObject>(group);
        QVERIFY(traced && traced->childCount() == 2);
        QVERIFY2(near(traced->boundingRect(), m_editor->document().findAs<ImageObject>(id)->boundingRect()),
                 qPrintable(describe(traced->boundingRect())));
        QVERIFY(m_editor->document().findAs<ImageObject>(id)); // Original bleibt

        // Bildanpassung: neue Pixel, gleiche Grösse und Rahmen; Rückgängig stellt sie zurück
        const QString before = m_editor->document().findAs<ImageObject>(id)->key();
        ImageAdjustDialog adjustDialog(m_editor->document().findAs<ImageObject>(id)->image());
        adjust::Basic b;
        b.brightness = 40;
        b.temperature = 30;
        adjustDialog.setBasic(b);
        const QImage adjusted = adjustDialog.result();
        QVERIFY(adjusted.pixelColor(10, 10).red() > 0xf0 - 1);
        m_editor->setImagePixels(id, adjusted, QStringLiteral("Bildanpassung"));
        QVERIFY(m_editor->document().findAs<ImageObject>(id)->key() != before);
        m_editor->document().undo();
        QCOMPARE(m_editor->document().findAs<ImageObject>(id)->key(), before);
    }

    void nudgeAndDuplicateUsePageSettings()
    {
        drawRect(QRectF(20, 40, 100, 50));
        const QPointF start = m_editor->selectionBounds().topLeft();
        m_editor->setNudge(0.5);
        QTest::keyClick(m_canvas.get(), Qt::Key_Right);
        QVERIFY2(std::abs(m_editor->selectionBounds().left() - start.x() - 0.5) < 1e-6,
                 qPrintable(describe(m_editor->selectionBounds())));
        QTest::keyClick(m_canvas.get(), Qt::Key_Down, Qt::ShiftModifier); // grössere Schrittweite: ×2
        QVERIFY(std::abs(m_editor->selectionBounds().top() - start.y() - 1.0) < 1e-6);
        m_editor->setDuplicateOffset(QPointF(10, 0));
        m_editor->duplicateSelection();
        QVERIFY(std::abs(m_editor->selectionBounds().left() - start.x() - 10.5) < 1e-6);
    }

    void shapeToolBendsAndEditsNodes()
    {
        m_canvas->setTool(ToolId::Freehand);
        click(screen(QPointF(30, 100)));
        click(screen(QPointF(130, 100)));
        const NodeId id = m_editor->selection().value(0);
        auto curve = [&] { return m_editor->document().findAs<CurveObject>(id); };

        m_canvas->setTool(ToolId::Shape);
        ShapeTool& shape = m_canvas->shapeTool();
        QVERIFY(shape.curve());
        click(screen(QPointF(130, 100))); // Endknoten wählen
        QCOMPARE(shape.selectedNodes(), (QList<NodeRef>{NodeRef{0, 1}}));
        QVERIFY(!shape.canRun(ShapeTool::Command::ToLine));
        shape.run(ShapeTool::Command::ToCurve);
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("In Kurve umwandeln"));
        QVERIFY(curve()->node(NodeRef{0, 1}).curveIn);
        screenshot(QStringLiteral("22-form-in-kurve"));

        // Segment in der Mitte nach oben ziehen: die Kurve folgt der Maus
        drag(screen(QPointF(80, 100)), screen(QPointF(80, 70)), {}, QStringLiteral("23-form-verbiegen-vorschau"));
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Kurve verbiegen"));
        double d = 0;
        curve()->nearestOnSegment(NodeRef{0, 1}, QPointF(80, 70), &d);
        QVERIFY2(d < 1.0, qPrintable(QString::number(d)));

        // Steuerpunkt am Endknoten ziehen
        click(screen(QPointF(130, 100)));
        const QPointF c2 = curve()->node(NodeRef{0, 1}).c2;
        drag(screen(c2), screen(c2 + QPointF(0, 20)));
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Steuerpunkt verschieben"));
        QVERIFY(qAbs(curve()->node(NodeRef{0, 1}).c2.y() - (c2.y() + 20)) < 0.5);

        // Doppelklick auf die Linie fügt einen Knoten ein, Doppelklick auf den Knoten löscht ihn
        const QPointF on = curve()->pointOnSegment(NodeRef{0, 1}, 0.4);
        QTest::mouseDClick(m_canvas.get(), Qt::LeftButton, {}, screen(on), 1000);
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Knoten hinzufügen"));
        QCOMPARE(curve()->nodeCount(), 3);
        screenshot(QStringLiteral("24-form-knoten-eingefuegt"));
        const QPointF added = curve()->node(NodeRef{0, 1}).point;
        QTest::mouseDClick(m_canvas.get(), Qt::LeftButton, {}, screen(added), 1000);
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Knoten löschen"));
        QCOMPARE(curve()->nodeCount(), 2);
    }

    void convertRectangleToCurves()
    {
        const NodeId id = drawRect(QRectF(20, 40, 100, 50));
        m_editor->setSelectionFill(Fill::solid(Color(200, 0, 0)));
        m_editor->convertSelectionToCurves();
        const auto* c = m_editor->document().findAs<CurveObject>(m_editor->selection().value(0));
        QVERIFY(c);
        QCOMPARE(c->nodeCount(), 4);
        QVERIFY(c->subPaths().first().closed);
        QVERIFY(near(c->boundingRect(), QRectF(20, 40, 100, 50)));
        QCOMPARE(c->style().fill, Fill::solid(Color(200, 0, 0)));
        m_editor->document().undo();
        QVERIFY(m_editor->document().findAs<RectangleObject>(id));
    }

    void blurSoftensEdges()
    {
        drawRect(QRectF(40, 60, 100, 60));
        m_editor->setSelectionFill(Fill::solid(Color(0, 0, 0)));
        m_editor->setSelectionStroke(std::nullopt);
        m_editor->clearSelection();
        const QPoint outside = screen(QPointF(38.5, 90));
        const QPoint inside = screen(QPointF(90, 90));
        QImage sharp = m_canvas->grab().toImage();
        QCOMPARE(qGray(sharp.pixel(outside)), 255);
        m_editor->setSelection({m_editor->pickableObjects().first()->id()});
        m_editor->setSelectionBlur(3.0);
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Unschärfe"));
        m_editor->clearSelection();
        QImage blurred = m_canvas->grab().toImage();
        QVERIFY2(qGray(blurred.pixel(outside)) < 200, qPrintable(QString::number(qGray(blurred.pixel(outside)))));
        QVERIFY(qGray(blurred.pixel(inside)) < 10);
        screenshot(QStringLiteral("25-unschaerfe"));
    }

    void transparencyAndBlurRamps()
    {
        const NodeId id = drawRect(QRectF(20, 40, 100, 50));
        m_editor->setSelectionFill(Fill::solid(Color(0, 0, 0)));
        m_editor->setSelectionStroke(std::nullopt);
        auto gray = [this](double x, double y) {
            m_editor->clearSelection();
            return qGray(m_canvas->grab().toImage().pixel(screen(QPointF(x, y))));
        };

        // Interaktive Transparenz: Ziehen legt einen Verlauf von deckend nach durchsichtig an
        m_canvas->setTool(ToolId::Transparency);
        m_editor->setSelection({id});
        drag(screen(QPointF(30, 65)), screen(QPointF(110, 65)), {}, QStringLiteral("54-transparenz-vorschau"));
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Interaktive Transparenz"));
        const auto* r = m_editor->document().findAs<DrawObject>(id);
        QCOMPARE(r->transparency().type, Ramp::Type::Linear);
        QCOMPARE(r->transparency().to, 100.0);
        QVERIFY(qAbs(r->transparency().start.x() - 0.1) < 0.02 && qAbs(r->transparency().end.x() - 0.9) < 0.02);
        QVERIFY(near(r->boundingRect(), QRectF(20, 40, 100, 50))); // Pfade unverändert
        m_editor->setSelection({id});
        screenshot(QStringLiteral("54-transparenz"));
        QVERIFY2(gray(25, 65) < 30, qPrintable(QString::number(gray(25, 65))));
        QVERIFY2(gray(115, 65) > 225, qPrintable(QString::number(gray(115, 65))));
        const int middle = gray(70, 65);
        QVERIFY2(middle > 90 && middle < 170, qPrintable(QString::number(middle)));

        // Speichern und Laden behält den Verlauf
        auto back = serial::fromJson(serial::toJson(*r), serial::Ids::Keep);
        QCOMPARE(node_cast<DrawObject>(back.get())->transparency(), r->transparency());

        // Gleichmässig 50 %
        m_editor->setSelection({id});
        m_editor->setSelectionEffect(Effect::Transparency, [](Ramp& t) { t = Ramp::uniform(50.0); }, QStringLiteral("T"));
        const int half = gray(70, 65);
        QVERIFY2(half > 110 && half < 145, qPrintable(QString::number(half)));
        m_editor->document().undo();
        m_editor->document().undo();
        QVERIFY(!m_editor->document().findAs<DrawObject>(id)->transparency().isActive());

        // Interaktive Unschärfe: links scharf, rechts unscharf
        QCOMPARE(gray(19.2, 65), 255);
        m_canvas->setTool(ToolId::Blur);
        m_editor->setSelection({id});
        drag(screen(QPointF(30, 65)), screen(QPointF(110, 65)));
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Interaktive Unschärfe"));
        const Ramp b = m_editor->document().findAs<DrawObject>(id)->blurRamp();
        QVERIFY(b.type == Ramp::Type::Linear && b.from == 0.0 && b.to == 2.0);
        m_editor->setSelection({id});
        screenshot(QStringLiteral("55-unschaerfe-verlauf"));
        QVERIFY2(gray(19.2, 65) > 245, qPrintable(QString::number(gray(19.2, 65))));
        QVERIFY2(gray(120.8, 65) < 200, qPrintable(QString::number(gray(120.8, 65))));
        auto again = serial::fromJson(serial::toJson(*m_editor->document().findAs<DrawObject>(id)), serial::Ids::Keep);
        QCOMPARE(node_cast<DrawObject>(again.get())->blurRamp(), b);

        // In Kurven: die Unschärfe wird dauerhaft (Stufenkonturen) und sieht fast gleich aus
        m_canvas->setTool(ToolId::Pick);
        m_editor->setSelection({id});
        m_editor->setSelectionEffect(Effect::Blur, [](Ramp& x) { x = Ramp::uniform(2.0); }, QStringLiteral("U"));
        const int blurredEdge = gray(121.0, 65);
        const int blurredInside = gray(70, 65);
        m_editor->setSelection({id});
        m_editor->convertSelectionToCurves();
        const auto* baked = m_editor->document().findAs<GroupObject>(m_editor->selection().value(0));
        QVERIFY(baked && baked->childCount() >= 5);
        QVERIFY(!baked->blurRamp().isActive());
        screenshot(QStringLiteral("59-unschaerfe-in-kurven"));
        const int bakedEdge = gray(121.0, 65);
        QVERIFY2(std::abs(bakedEdge - blurredEdge) < 15, qPrintable(QStringLiteral("%1 %2").arg(bakedEdge).arg(blurredEdge)));
        QVERIFY(std::abs(gray(70, 65) - blurredInside) < 10);
        m_editor->document().undo();
        m_editor->document().undo();

        // Regler der Objektleiste (stärkste Stelle): der Verlauf bleibt, nur stärker
        m_canvas->setTool(ToolId::Pick);
        m_editor->setSelection({id});
        m_editor->setSelectionEffect(Effect::Blur, [](Ramp& x) { x.from *= 2; x.to *= 2; }, QStringLiteral("U"));
        QCOMPARE(m_editor->document().findAs<DrawObject>(id)->blur(), 4.0);
    }

    void displayDepthIsKnown()
    {
        const DisplayDepth d = displayDepth(QGuiApplication::primaryScreen());
        qInfo("Bildschirm %s: %d Bit je Kanal%s (%s)", qPrintable(d.name), d.bitsPerColor, d.hdr ? ", HDR" : "",
              qPrintable(d.source));
        QVERIFY(d.bitsPerColor >= 8);
    }

    void blurWorksOnImagesAndHandles()
    {
        // Bild (rot) mit leichter Unschärfe bleibt sichtbar
        QImage red(200, 100, QImage::Format_RGB32);
        red.fill(Qt::red);
        const NodeId image = m_editor->importImage(red, QPointF(80, 80));
        QVERIFY(image != InvalidNodeId);
        m_editor->setSelection({image});
        m_editor->setSelectionBlur(0.1);
        m_editor->clearSelection();
        const QColor center = m_canvas->grab().toImage().pixelColor(screen(QPointF(80, 80)));
        QVERIFY2(center.red() > 200 && center.green() < 60, qPrintable(center.name()));

        // Blur-Werkzeug: Anfangspunkt eines Verlaufs schrittweise ziehen
        const NodeId id = drawRect(QRectF(20, 150, 100, 50));
        m_canvas->setTool(ToolId::Blur);
        m_editor->setSelection({id});
        drag(screen(QPointF(30, 175)), screen(QPointF(110, 175)));
        const Ramp before = m_editor->document().findAs<DrawObject>(id)->blurRamp();
        QVERIFY(before.isGradient());
        press(screen(QPointF(30, 175)));
        for (int i = 1; i <= 8; ++i) {
            move(screen(QPointF(30 + i * 2.5, 175 + i)));
            screenshot(QStringLiteral("61-blur-anfang-%1").arg(i));
        }
        release(screen(QPointF(50, 183)));
        const Ramp after = m_editor->document().findAs<DrawObject>(id)->blurRamp();
        qInfo("Anfang %.3f/%.3f -> %.3f/%.3f, Ende %.3f/%.3f -> %.3f/%.3f", before.start.x(), before.start.y(), after.start.x(),
              after.start.y(), before.end.x(), before.end.y(), after.end.x(), after.end.y());
        QVERIFY(std::abs(after.start.x() - 0.3) < 0.02 && std::abs(after.start.y() - (33.0 / 50.0)) < 0.03);
        QCOMPARE(after.end, before.end);
        // Beide Griffe dicht beieinander: der Klick nimmt den näheren, hier den Anfang
        m_editor->setSelectionEffect(Effect::Blur, [](Ramp& r) { r.start = QPointF(0.5, 0.5); r.end = QPointF(0.52, 0.5); },
                                     QStringLiteral("nah"));
        m_editor->setSelection({id});
        drag(screen(QPointF(69.8, 175)), screen(QPointF(40, 175)));
        const Ramp close = m_editor->document().findAs<DrawObject>(id)->blurRamp();
        QVERIFY2(close.start.x() < 0.3 && std::abs(close.end.x() - 0.52) < 1e-9,
                 qPrintable(QStringLiteral("%1 %2").arg(close.start.x()).arg(close.end.x())));
    }

    void shapingCombinesObjects()
    {
        using Op = Editor::ShapeOp;
        // A unten (blau), B darüber (rot); sie überlappen im Quadrat 60..80 / 60..80
        const NodeId a = drawRect(QRectF(20, 40, 60, 40));
        m_editor->setSelectionFill(Fill::solid(Color(0, 0, 255)));
        const NodeId b = drawRect(QRectF(60, 60, 60, 40));
        m_editor->setSelectionFill(Fill::solid(Color(255, 0, 0)));
        const int before = int(m_editor->pickableObjects().size());
        auto only = [this]() -> const DrawObject* {
            return m_editor->document().findAs<DrawObject>(m_editor->selection().value(0));
        };
        auto inside = [](const DrawObject* o, double x, double y) { return o && o->path().contains(QPointF(x, y)); };

        // Verschmelzen: eine Kurve, Stil des Ziels (zuletzt gewählt)
        m_editor->setSelection({a, b});
        QVERIFY(m_editor->shapeSelection(Op::Weld));
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Verschmelzen"));
        QCOMPARE(int(m_editor->pickableObjects().size()), before - 1);
        QVERIFY(near(only()->boundingRect(), QRectF(20, 40, 100, 60)));
        QCOMPARE(only()->style().fill, Fill::solid(Color(255, 0, 0)));
        QVERIFY(inside(only(), 70, 70) && inside(only(), 30, 50) && !inside(only(), 30, 95));
        m_editor->document().undo();

        // Zuschneiden: Ziel B verliert die Überlappung, die Quelle A bleibt
        QVERIFY(m_editor->shapeWithTarget(Op::Trim, {a}, b, true, false));
        QCOMPARE(int(m_editor->pickableObjects().size()), before);
        QVERIFY(m_editor->document().findAs<DrawObject>(a));
        QVERIFY(!inside(only(), 70, 70) && inside(only(), 100, 90));
        m_editor->document().undo();

        // Schnittmenge: neues Objekt aus der Überlappung, beide bleiben
        m_editor->setSelection({a, b});
        QVERIFY(m_editor->shapeSelection(Op::Intersect));
        QCOMPARE(int(m_editor->pickableObjects().size()), before + 1);
        QVERIFY2(near(only()->boundingRect(), QRectF(60, 60, 20, 20)), qPrintable(describe(only()->boundingRect())));
        m_editor->document().undo();

        // Vereinfachen: A verliert, was B verdeckt; B bleibt unverändert
        m_editor->setSelection({a, b});
        QVERIFY(m_editor->shapeSelection(Op::Simplify));
        QVERIFY(m_editor->document().findAs<RectangleObject>(b));
        const DrawObject* lower = nullptr;
        for (const DrawObject* o : m_editor->pickableObjects())
            if (o->id() != b && o->style().fill == Fill::solid(Color(0, 0, 255)))
                lower = o;
        QVERIFY(inside(lower, 30, 50) && !inside(lower, 70, 70));
        m_editor->document().undo();

        // Vorn minus hinten / Hinten minus vorn: ein Objekt bleibt übrig
        m_editor->setSelection({a, b});
        QVERIFY(m_editor->shapeSelection(Op::FrontMinusBack));
        QCOMPARE(int(m_editor->pickableObjects().size()), before - 1);
        QVERIFY(inside(only(), 100, 90) && !inside(only(), 70, 70));
        QCOMPARE(only()->style().fill, Fill::solid(Color(255, 0, 0)));
        m_editor->document().undo();
        m_editor->setSelection({a, b});
        QVERIFY(m_editor->shapeSelection(Op::BackMinusFront));
        QVERIFY(inside(only(), 30, 50) && !inside(only(), 70, 70));
        QCOMPARE(only()->style().fill, Fill::solid(Color(0, 0, 255)));
        m_editor->document().undo();

        // Begrenzung: Umriss ohne Füllung um beide, Originale bleiben
        m_editor->setSelection({a, b});
        QVERIFY(m_editor->shapeSelection(Op::Boundary));
        QCOMPARE(int(m_editor->pickableObjects().size()), before + 1);
        QVERIFY(only()->style().fill.isNone() && only()->style().stroke.has_value());
        QVERIFY(near(only()->boundingRect(), QRectF(20, 40, 100, 60)));
        m_editor->document().undo();

        // Andockfenster: Quelle wählen, "Zuschneiden", dann das Ziel anklicken
        ShapingDocker docker(*m_editor, *m_canvas);
        docker.setOperation(Op::Trim);
        QCOMPARE(docker.applyButton()->text(), QStringLiteral("Zuschneiden"));
        QVERIFY(docker.keepSourceBox()->isChecked() && !docker.keepTargetBox()->isChecked());
        m_editor->setSelection({a});
        docker.apply();
        QVERIFY(m_canvas->pickTool().pickingTarget());
        click(screen(QPointF(110, 95)));
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Zuschneiden"));
        QVERIFY(!inside(only(), 70, 70) && inside(only(), 100, 90));
        QVERIFY(m_editor->document().findAs<DrawObject>(a));
        docker.setOperation(Op::Boundary);
        QCOMPARE(docker.applyButton()->text(), QStringLiteral("Anwenden"));
        if (!qEnvironmentVariable("VEK_SCREENSHOT_DIR").isEmpty()) {
            docker.setOperation(Op::Intersect);
            docker.resize(220, 220);
            docker.grab().save(qEnvironmentVariable("VEK_SCREENSHOT_DIR") + QStringLiteral("/62-formen.png"));
        }
    }

    void optionsDialogClassic()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest")); // sonst kein QSettings
        Options& opt = Options::instance();
        opt.resetWorkspace();
        opt.attach(m_editor.get());
        OptionsDialog dialog(*m_editor, nullptr);
        // Alle Seiten
        const QStringList pages = dialog.pagePaths();
        for (const char* path : {"Arbeitsbereich/Allgemein", "Arbeitsbereich/Anzeige", "Arbeitsbereich/Bearbeiten",
                                 "Arbeitsbereich/An Objekten ausrichten", "Arbeitsbereich/Dynamische Hilfslinien",
                                 "Arbeitsbereich/Ausrichtungshilfslinien", "Arbeitsbereich/Warnungen", "Arbeitsbereich/Skripte",
                                 "Arbeitsbereich/Speichern", "Arbeitsbereich/SuperTrace", "Arbeitsbereich/Plugins",
                                 "Arbeitsbereich/Text/Mengentextrahmen", "Arbeitsbereich/Text/Schriften",
                                 "Arbeitsbereich/Text/Rechtschreibprüfung", "Arbeitsbereich/Text/Blitzkorrektur",
                                 "Arbeitsbereich/Hilfsmittelpalette/Bemassung", "Arbeitsbereich/Hilfsmittelpalette/Spirale",
                                 "Arbeitsbereich/Hilfsmittelpalette/Zoom/Schwenken", "Arbeitsbereich/Hilfsmittelpalette/Freihand/Bezier", "Arbeitsbereich/Anpassung/Befehle",
                                 "Arbeitsbereich/Anpassung/Farbpalette", "Dokument/Allgemein", "Dokument/Seitengrösse",
                                 "Dokument/Layout", "Dokument/Etikett", "Dokument/Hintergrund", "Dokument/Hilfslinien",
                                 "Dokument/Hilfslinien/Horizontal", "Dokument/Hilfslinien/Vertikal",
                                 "Dokument/Hilfslinien/Hilfslinien", "Dokument/Hilfslinien/Voreinstellungen", "Dokument/Gitter",
                                 "Dokument/Lineale", "Dokument/Speichern", "Dokument/HTML exportieren", "Global/Drucken",
                                 "Global/Drucken/Treiberkompatibilität", "Global/Bitmap Effects", "Global/Filter",
                                 "Global/Filter/Verknüpfen", "Global/Farbe"})
            QVERIFY2(pages.contains(QString::fromUtf8(path)), path);
        QVERIFY(dialog.selectPage(QStringLiteral("Dokument/Gitter")));
        if (const QString shots = qEnvironmentVariable("VEK_SCREENSHOT_DIR"); !shots.isEmpty()) {
            dialog.resize(900, 620);
            int n = 0;
            for (const char* path : {"Arbeitsbereich/Allgemein", "Arbeitsbereich/An Objekten ausrichten", "Dokument/Gitter",
                                     "Dokument/Seitengrösse", "Arbeitsbereich/Anpassung/Befehle", "Dokument/Hilfslinien/Horizontal"}) {
                dialog.selectPage(QString::fromUtf8(path));
                dialog.grab().save(shots + QStringLiteral("/64-optionen-%1.png").arg(++n));
            }
            dialog.selectPage(QStringLiteral("Dokument/Gitter"));
        }

        // Vorgaben
        QCOMPARE(opt.integer(QStringLiteral("ws/general/undo")), 150);
        QCOMPARE(opt.number(QStringLiteral("ws/edit/angleStep")), 15.0);
        QVERIFY(!opt.flag(QStringLiteral("doc/grid/show")));

        // Arbeitsbereich-Werte gehen in die Einstellungen, Dokument-Werte in die Datei (Undo)
        dialog.setPending(QStringLiteral("ws/edit/angleStep"), 30.0);
        dialog.setPending(QStringLiteral("doc/grid/show"), true);
        dialog.setPending(QStringLiteral("doc/grid/mode"), 1);
        dialog.setPending(QStringLiteral("doc/grid/h"), 10.0);
        dialog.setPending(QStringLiteral("doc/grid/v"), 10.0);
        dialog.setPending(QStringLiteral("doc/grid/snap"), true);
        dialog.setPending(QStringLiteral("doc/background/type"), 1);
        dialog.setPending(QStringLiteral("doc/background/color"), QStringLiteral("#ffff00"));
        dialog.accept();
        QCOMPARE(opt.number(QStringLiteral("ws/edit/angleStep")), 30.0);
        QVERIFY(opt.flag(QStringLiteral("doc/grid/show")));
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Optionen"));
        auto back = serial::fromJson(serial::toJson(m_editor->document().masterPage()), serial::Ids::Keep);
        QCOMPARE(node_cast<Page>(back.get())->options().value(QStringLiteral("doc/grid/h")).toDouble(), 10.0);

        // Hintergrund gelb, Gitter sichtbar; Einrasten am 10-mm-Gitter
        m_editor->clearSelection();
        m_canvas->repaint();
        const QImage shot = m_canvas->grab().toImage();
        const QColor bg = shot.pixelColor(screen(QPointF(13, 17)));
        QVERIFY2(bg.red() > 240 && bg.green() > 240 && bg.blue() < 30, qPrintable(bg.name()));
        screenshot(QStringLiteral("63-gitter-hintergrund"));
        m_editor->snapSettings().objects = false;
        const NodeId id = drawRect(QRectF(21, 41, 18, 18));
        QVERIFY2(near(m_editor->document().findAs<DrawObject>(id)->boundingRect(), QRectF(20, 40, 20, 20)),
                 qPrintable(describe(m_editor->document().findAs<DrawObject>(id)->boundingRect())));

        // Undo nimmt die Dokument-Optionen zurück
        m_editor->document().undo(); // Rechteck
        m_editor->document().undo(); // Optionen
        QVERIFY(!opt.flag(QStringLiteral("doc/grid/show")));

        // Hilfslinien-Liste: waagrechte hinzufügen, eine davon nur auf dieser Seite
        OptionsDialog guides(*m_editor, nullptr);
        QVERIFY(guides.selectPage(QStringLiteral("Dokument/Hilfslinien/Horizontal")));
        QWidget* holder = guides.field(QStringLiteral("guides/Horizontal"));
        QVERIFY(holder);
        auto* value = holder->findChildren<QDoubleSpinBox*>().value(1); // y
        auto* add = holder->findChildren<QPushButton*>().value(0);
        auto* local = holder->findChild<QCheckBox*>();
        QVERIFY(value && add && local);
        value->setValue(50);
        add->click();
        value->setValue(80);
        local->setChecked(true);
        add->click();
        guides.accept();
        QCOMPARE(m_editor->guides().size(), 2);
        QCOMPARE(m_editor->document().masterPage().guides(), (QList<Guide>{Guide::horizontal(50, 0)}));
        QCOMPARE(m_editor->currentPage().guides(), (QList<Guide>{Guide::horizontal(80, 0)}));

        // Rückgängig-Tiefe
        opt.set(QStringLiteral("ws/general/undo"), 2);
        m_editor->document().setUndoLimit(opt.integer(QStringLiteral("ws/general/undo")));
        drawRect(QRectF(100, 100, 10, 10));
        drawRect(QRectF(120, 100, 10, 10));
        drawRect(QRectF(140, 100, 10, 10));
        m_editor->document().undo();
        m_editor->document().undo();
        QVERIFY(!m_editor->document().canUndo());
        m_editor->document().setUndoLimit(0);
        opt.resetWorkspace();
    }

    void rulersGuidesAndSmartSnapping()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        Options& opt = Options::instance();
        opt.resetWorkspace();
        opt.attach(m_editor.get());

        // Lineal: Zentimeter, Ursprung bei (50, 20), Massstab 1:100
        opt.apply({{QStringLiteral("doc/rulers/units"), 1}, {QStringLiteral("doc/rulers/originX"), 50.0},
                   {QStringLiteral("doc/rulers/originY"), 20.0}, {QStringLiteral("doc/rulers/scale"), 100.0}});
        QCOMPARE(opt.rulerValue(60.0, true), 100.0); // 10 mm * 100 = 1000 mm = 100 cm
        QCOMPARE(opt.rulerValue(20.0, false), 0.0);
        QVERIFY(opt.positionText(QPointF(60, 25)).endsWith(QStringLiteral("cm")));
        m_canvas->repaint();
        m_editor->document().undo();
        QCOMPARE(opt.rulerValue(60.0, true), 60.0);

        // Voreinstellung: Ein-Zentimeter-Ränder und Seitenränder (8 Hilfslinien, voreingestellt)
        opt.set(QStringLiteral("doc/guides/presets"), QStringLiteral("cm,page"));
        const QList<Guide> preset = opt.presetGuides(QSizeF(210, 297));
        QCOMPARE(preset.size(), 8);
        QVERIFY(preset.contains(Guide::vertical(200, 0, true)) && preset.contains(Guide::horizontal(287, 0, true)));
        QVERIFY(std::all_of(preset.begin(), preset.end(), [](const Guide& g) { return g.preset; }));
        m_editor->replaceGuides(preset, {});
        auto back = serial::fromJson(serial::toJson(m_editor->document().masterPage()), serial::Ids::Keep);
        QVERIFY(node_cast<Page>(back.get())->guides().first().preset);
        m_editor->replaceGuides({}, {});

        // Ausrichtungshilfslinien: neues Rechteck rastet an der Oberkante eines anderen ein
        m_editor->snapSettings().objects = false;
        const NodeId first = drawRect(QRectF(20, 40, 30, 30));
        const double top = m_editor->document().findAs<DrawObject>(first)->boundingRect().top();
        opt.set(QStringLiteral("ws/alignguides/enabled"), true);
        const NodeId aligned = drawRect(QRectF(100, 41, 20, 20));
        QVERIFY2(std::abs(m_editor->document().findAs<DrawObject>(aligned)->boundingRect().top() - top) < 1e-6,
                 qPrintable(describe(m_editor->document().findAs<DrawObject>(aligned)->boundingRect())));
        SnapEngine engine(*m_canvas);
        const SnapResult r = engine.snap(QPointF(150, top + 0.5));
        QVERIFY(r.snapped() && !r.lines.isEmpty());
        opt.set(QStringLiteral("ws/alignguides/enabled"), false);

        // Dynamische Hilfslinien: unter 45° von der Ecke (20,40) aus, an 2,5-mm-Marken
        opt.set(QStringLiteral("ws/dynguides/enabled"), true);
        SnapEngine dyn(*m_canvas);
        const QPointF corner = m_editor->document().findAs<DrawObject>(first)->boundingRect().topLeft();
        const SnapResult d = dyn.snap(corner + QPointF(30.3, -30.0));
        QVERIFY2(d.snapped() && d.hint.contains(QStringLiteral("45")), qPrintable(d.hint));
        const double along = std::hypot(d.point.x() - corner.x(), d.point.y() - corner.y());
        QVERIFY2(std::abs(along / 2.5 - std::round(along / 2.5)) < 1e-6, qPrintable(QString::number(along)));
        QVERIFY(std::abs((d.point.x() - corner.x()) + (d.point.y() - corner.y())) < 1e-6); // auf der 45°-Linie
        opt.resetWorkspace();
    }

    void pageAndBackgroundOptions()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        Options& opt = Options::instance();
        opt.resetWorkspace();
        opt.attach(m_editor.get());
        auto pixel = [this](const QPointF& doc) {
            m_editor->clearSelection();
            m_canvas->repaint();
            return m_canvas->grab().toImage().pixelColor(screen(doc));
        };

        // Offene Kurve mit Füllung: ungefüllt, ausser "Offene Kurven füllen"
        auto open = std::make_unique<CurveObject>(QList<SubPath>{SubPath{{CurveNode{QPointF(20, 20)}, CurveNode{QPointF(80, 20)},
                                                                            CurveNode{QPointF(80, 80)}},
                                                                           false}});
        Style st;
        st.fill = Fill::solid(Color(255, 0, 0));
        st.stroke.reset();
        open->setStyle(st);
        m_editor->addObject(std::move(open), QStringLiteral("Kurve"));
        QVERIFY(pixel(QPointF(70, 35)).red() > 240 && pixel(QPointF(70, 35)).green() > 240); // weiss
        opt.set(QStringLiteral("doc/general/fillOpenCurves"), true);
        QCOMPARE(pixel(QPointF(70, 35)), QColor(255, 0, 0));
        m_editor->document().undo();
        m_editor->document().undo();

        // Hintergrund-Bitmap eingebettet: wird gekachelt gezeichnet und mit der Datei gespeichert
        QImage tile(20, 20, QImage::Format_RGB32);
        tile.fill(QColor(0, 160, 0));
        tile.setDotsPerMeterX(int(254 / 0.0254)); // 20 px = 2 mm
        tile.setDotsPerMeterY(int(254 / 0.0254));
        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        tile.save(&buffer, "PNG");
        const QString key = imagepool::add(png);
        opt.apply({{QStringLiteral("doc/background/type"), 2}, {QStringLiteral("doc/background/bitmap"), QStringLiteral("pool:") + key}});
        const QColor green = pixel(QPointF(100, 150));
        QVERIFY2(green.green() > 140 && green.red() < 30, qPrintable(green.name()));
        const QByteArray saved = io::saveToBytes(m_editor->document());
        QVERIFY(saved.contains(key.toLatin1())); // images/<key>.png im ZIP
        m_editor->document().undo();

        // Randanschnitt sichtbar ausserhalb der Seite
        opt.apply({{QStringLiteral("doc/page/bleed"), 5.0}, {QStringLiteral("doc/page/showBleed"), true}});
        const QColor bleed = pixel(QPointF(-5, 150));
        QVERIFY2(bleed.red() > 150 && bleed.green() < 120, qPrintable(bleed.name()));
        m_editor->document().undo();

        // Doppelseiten: Seite 2 links neben Seite 3; Klick darauf wechselt die Seite
        m_editor->insertPage(1);
        m_editor->insertPage(2);
        m_editor->setCurrentPageIndex(2);
        opt.set(QStringLiteral("doc/layout/facing"), true);
        const auto [partner, offset] = m_canvas->facingPartner();
        QCOMPARE(partner, 1);
        QCOMPARE(offset, -210.0);
        click(screen(QPointF(-100, 150)));
        QCOMPARE(m_editor->currentPageIndex(), 1);
        opt.set(QStringLiteral("doc/layout/facing"), false);

        // Layout Klappkarte: die Seite wird halb so hoch (zwei Seiten je Bogen)
        OptionsDialog layout(*m_editor, nullptr);
        auto* type = qobject_cast<QComboBox*>(layout.field(QStringLiteral("doc/layout/type")));
        QVERIFY(type);
        type->setCurrentIndex(3);
        layout.accept();
        QCOMPARE(m_editor->currentPage().size(), QSizeF(210, 148.5));

        // Etikett L7163: eine Seite ist ein Etikett
        OptionsDialog labels(*m_editor, nullptr);
        labels.setPending(QStringLiteral("doc/label/labels"), 1);
        auto* tree = labels.findChild<QTreeWidget*>(QString(), Qt::FindChildrenRecursively);
        QTreeWidgetItem* l7163 = nullptr;
        for (QTreeWidget* t : labels.findChildren<QTreeWidget*>()) {
            for (QTreeWidgetItem* item : t->findItems(QStringLiteral("L7163"), Qt::MatchStartsWith | Qt::MatchRecursive))
                l7163 = item, tree = t;
        }
        QVERIFY(tree && l7163);
        tree->setCurrentItem(l7163);
        labels.accept();
        QCOMPARE(m_editor->currentPage().size(), QSizeF(99.1, 38.1));
        QCOMPARE(opt.integer(QStringLiteral("doc/label/cols")), 2);

        // Seite ohne Werkzeuge in ein Bild zeichnen (Druck/Export)
        auto rect = std::make_unique<RectangleObject>(QSizeF(20, 10));
        rect->setTransform(QTransform::fromTranslate(10, 10));
        Style blue;
        blue.fill = Fill::solid(Color(0, 0, 255));
        rect->setStyle(blue);
        m_editor->addObject(std::move(rect), QStringLiteral("R"));
        QImage out(200, 80, QImage::Format_ARGB32_Premultiplied);
        out.fill(Qt::white);
        QPainter painter(&out);
        m_canvas->renderPage(painter, m_editor->currentPageIndex(), 2.0, QPointF(0, 0), true);
        painter.end();
        QCOMPARE(out.pixelColor(40, 30), QColor(0, 0, 255));
        QCOMPARE(out.pixelColor(100, 60), QColor(Qt::white));
        opt.resetWorkspace();
    }

    void toolOptionsTakeEffect()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        Options& opt = Options::instance();
        opt.resetWorkspace();
        opt.attach(m_editor.get());

        // Spirale: 4 Umdrehungen im aufgezogenen Rechteck
        m_canvas->setTool(ToolId::Spiral);
        drag(screen(QPointF(20, 20)), screen(QPointF(80, 80)));
        const auto* spiral = m_editor->document().findAs<CurveObject>(m_editor->selection().value(0));
        QVERIFY(spiral && !spiral->subPaths().first().closed);
        QVERIFY2(near(spiral->boundingRect(), QRectF(20, 20, 60, 60), 2.5), qPrintable(describe(spiral->boundingRect())));
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Spirale erstellen"));

        // Millimeterpapier: Gruppe aus 4 x 3 Zellen
        m_canvas->setTool(ToolId::GraphPaper);
        drag(screen(QPointF(100, 20)), screen(QPointF(180, 80)));
        const auto* graph = m_editor->document().findAs<GroupObject>(m_editor->selection().value(0));
        QVERIFY(graph && graph->childCount() == 12);

        // Rechteck-Vorgaben: abgeschrägte Ecken mit 5 mm
        opt.apply({{QStringLiteral("ws/tools/rect/cornerType"), 2}, {QStringLiteral("ws/tools/rect/radius"), 5.0}});
        const NodeId r = drawRect(QRectF(20, 100, 40, 30));
        const auto* rect = m_editor->document().findAs<RectangleObject>(r);
        QVERIFY(rect && rect->cornerType() == RectangleObject::CornerType::Chamfer && rect->cornerRadius(RectangleObject::TopLeft) == 5.0);

        // Messer "als ein Objekt belassen": ein Objekt mit zwei Teilen
        opt.set(QStringLiteral("ws/tools/knife/keepOne"), true);
        const int before = int(m_editor->pickableObjects().size());
        m_editor->setSelection({r});
        m_canvas->setTool(ToolId::Knife);
        click(screen(QPointF(40, 95)));
        click(screen(QPointF(40, 135)));
        QCOMPARE(int(m_editor->pickableObjects().size()), before);
        const auto* cut = m_editor->document().findAs<CurveObject>(m_editor->pickableObjects().last()->id());
        QVERIFY(cut && cut->subPaths().size() == 2);

        // Schwenken verschiebt die Ansicht, ohne Objekte zu ändern
        m_canvas->setTool(ToolId::Pan);
        const QPointF before0 = m_canvas->toScreen(QPointF(0, 0));
        drag(QPoint(300, 300), QPoint(340, 320));
        QCOMPARE(m_canvas->toScreen(QPointF(0, 0)), before0 + QPointF(40, 20));

        // Windows-Standard: Strg beim Loslassen = Kopie an der neuen Stelle
        opt.set(QStringLiteral("ws/tools/pick/modifiers"), 1);
        m_canvas->setTool(ToolId::Pick);
        const NodeId moved = drawRect(QRectF(120, 120, 20, 20));
        const int count = int(m_editor->pickableObjects().size());
        m_canvas->setTool(ToolId::Pick);
        m_editor->setSelection({moved});
        press(screen(QPointF(130, 130)));
        move(screen(QPointF(140, 130)));
        move(screen(QPointF(150, 130)));
        release(screen(QPointF(160, 130)), Qt::ControlModifier);
        QCOMPARE(int(m_editor->pickableObjects().size()), count + 1);
        QVERIFY(near(m_editor->document().findAs<DrawObject>(moved)->boundingRect(), QRectF(120, 120, 20, 20), 1.0));
        opt.resetWorkspace();
    }

    void dimensionsMeasure()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        Options::instance().resetWorkspace();
        Options::instance().attach(m_editor.get());
        m_editor->snapSettings().objects = false;
        m_editor->snapSettings().page = false;
        m_editor->snapSettings().guides = false;

        // Parallelbemassung: zwei Punkte, dann die Lage der Masslinie
        m_canvas->setTool(ToolId::DimensionParallel);
        click(screen(QPointF(20, 50)));
        click(screen(QPointF(80, 50)));
        click(screen(QPointF(50, 40)));
        const auto* dim = m_editor->document().findAs<DimensionObject>(m_editor->selection().value(0));
        QVERIFY(dim);
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Bemassung"));
        QVERIFY2(std::abs(dim->measuredValue() - 60.0) < 0.6, qPrintable(dim->text()));
        QVERIFY(dim->text().endsWith(QStringLiteral(" mm")));
        QVERIFY(dim->boundingRect().top() < 41.0); // Masslinie oben

        // Skalieren misst neu, Einheit cm über die Eigenschaft
        m_editor->setSelection({dim->id()});
        m_editor->transformSelection(xform::scaling(2.0, 2.0, QPointF(20, 50)), QStringLiteral("S"));
        dim = m_editor->document().findAs<DimensionObject>(dim->id());
        QVERIFY(std::abs(dim->measuredValue() - 120.0) < 1.2);
        m_editor->setObjectProperty(QStringLiteral("units"), 1, QStringLiteral("Einheiten"));
        dim = m_editor->document().findAs<DimensionObject>(dim->id());
        QVERIFY2(dim->text().endsWith(QStringLiteral(" cm")) && std::abs(dim->measuredValue() - 12.0) < 0.12, qPrintable(dim->text()));
        auto back = serial::fromJson(serial::toJson(*dim), serial::Ids::Keep);
        QCOMPARE(node_cast<DimensionObject>(back.get())->text(), dim->text());

        // Horizontal/Vertikal: Masslinie seitlich gezogen = senkrechte Bemassung
        m_canvas->setTool(ToolId::DimensionHV);
        click(screen(QPointF(120, 100)));
        click(screen(QPointF(140, 160)));
        click(screen(QPointF(170, 130)));
        const auto* hv = m_editor->document().findAs<DimensionObject>(m_editor->selection().value(0));
        QVERIFY(hv && hv->kind() == DimensionObject::Kind::Vertical);
        QVERIFY2(std::abs(hv->measuredValue() - 60.0) < 0.6, qPrintable(hv->text()));

        // Winkelbemassung: rechter Winkel
        m_canvas->setTool(ToolId::DimensionAngle);
        click(screen(QPointF(40, 200)));
        click(screen(QPointF(90, 200)));
        click(screen(QPointF(40, 150)));
        click(screen(QPointF(60, 180)));
        const auto* angle = m_editor->document().findAs<DimensionObject>(m_editor->selection().value(0));
        QVERIFY(angle && angle->kind() == DimensionObject::Kind::Angular);
        QVERIFY2(std::abs(angle->measuredValue() - 90.0) < 1.0, qPrintable(angle->text()));
        QVERIFY(angle->text().endsWith(QStringLiteral("°")));
        screenshot(QStringLiteral("67-bemassung"));
    }

    void tableToolCreatesAndEditsCells()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        m_editor->snapSettings().objects = false;
        m_editor->snapSettings().page = false;
        m_editor->snapSettings().guides = false;
        Options& o = Options::instance();
        o.set(QStringLiteral("ws/tools/table/rows"), 2);
        o.set(QStringLiteral("ws/tools/table/cols"), 3);
        o.set(QStringLiteral("ws/tools/table/tabMode"), 1);
        o.set(QStringLiteral("ws/tools/table/tabOrder"), 0);

        m_canvas->setTool(ToolId::Table);
        press(screen(QPointF(20, 20)));
        move(screen(QPointF(60, 40)));
        release(screen(QPointF(80, 40)));
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Tabelle erstellen"));
        const auto* table = m_editor->document().findAs<TableObject>(m_editor->selection().value(0));
        QVERIFY(table);
        QCOMPARE(table->rows(), 2);
        QCOMPARE(table->columns(), 3);
        QVERIFY2(near(table->boundingRect(), QRectF(20, 20, 60, 20), 0.5), qPrintable(describe(table->boundingRect())));
        const NodeId id = table->id();

        // In Zelle 0/0 schreiben, Tab springt weiter, erst Esc schreibt ins Dokument
        click(screen(QPointF(25, 25)));
        QTest::keyClicks(m_canvas.get(), QStringLiteral("A1"));
        QTest::keyClick(m_canvas.get(), Qt::Key_Tab);
        QTest::keyClicks(m_canvas.get(), QStringLiteral("B1"));
        QCOMPARE(m_editor->document().findAs<TableObject>(id)->cell(0, 0).text, QString());
        // Klick in Zelle unten rechts, Tab läuft über das Ende an den Anfang
        click(screen(QPointF(75, 35)));
        QTest::keyClicks(m_canvas.get(), QStringLiteral("C2"));
        QTest::keyClick(m_canvas.get(), Qt::Key_Tab);
        QTest::keyClick(m_canvas.get(), Qt::Key_Backspace);
        QTest::keyClick(m_canvas.get(), Qt::Key_Escape);
        table = m_editor->document().findAs<TableObject>(id);
        QCOMPARE(table->cell(0, 0).text, QStringLiteral("A"));
        QCOMPARE(table->cell(0, 1).text, QStringLiteral("B1"));
        QCOMPARE(table->cell(1, 2).text, QStringLiteral("C2"));
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Tabelle bearbeiten"));
        QVERIFY(!table->textPieces().isEmpty());

        // Zeilen/Spalten über die Eigenschaftsleiste, Inhalt bleibt, Grösse bleibt
        m_editor->setSelection({id});
        m_editor->setObjectProperty(QStringLiteral("columns"), 4, QStringLiteral("Spalten"));
        table = m_editor->document().findAs<TableObject>(id);
        QCOMPARE(table->columns(), 4);
        QCOMPARE(table->cell(0, 1).text, QStringLiteral("B1"));
        QVERIFY(near(table->boundingRect(), QRectF(20, 20, 60, 20), 0.5));
        // Skalieren verteilt sich auf die Zellen, Speichern/Laden behält alles
        const double before = table->columnWidths().first();
        m_editor->transformSelection(xform::scaling(2.0, 1.0, QPointF(20, 20)), QStringLiteral("S"));
        table = m_editor->document().findAs<TableObject>(id);
        QVERIFY2(std::abs(table->columnWidths().first() - 2 * before) < 0.01, qPrintable(QString::number(table->columnWidths().first()) + describe(table->boundingRect())));
        auto back = serial::fromJson(serial::toJson(*table), serial::Ids::Keep);
        const auto* copy = node_cast<TableObject>(back.get());
        QVERIFY(copy && copy->rows() == 2 && copy->columns() == 4 && copy->cell(1, 2).text == QStringLiteral("C2"));

        // Palettenfarbe färbt die aktuelle Zelle
        m_canvas->setTool(ToolId::Table);
        click(screen(QPointF(25, 25)));
        QVERIFY(m_canvas->offerPaletteColor(Color(255, 0, 0), true));
        QTest::keyClick(m_canvas.get(), Qt::Key_Escape);
        QCOMPARE(m_editor->document().findAs<TableObject>(id)->cell(0, 0).fill, std::optional<Color>(Color(255, 0, 0)));
        m_canvas->setTool(ToolId::Pick);
    }

    void meshFillToolEditsNodes()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        m_editor->snapSettings().objects = false;
        m_editor->snapSettings().page = false;
        m_editor->snapSettings().guides = false;
        Options::instance().set(QStringLiteral("ws/tools/mesh/cols"), 2);
        Options::instance().set(QStringLiteral("ws/tools/mesh/rows"), 2);

        m_canvas->setTool(ToolId::Rectangle);
        press(screen(QPointF(20, 20)));
        move(screen(QPointF(60, 40)));
        release(screen(QPointF(80, 60)));
        const NodeId id = m_editor->selection().value(0);
        m_editor->setSelectionFill(Fill::solid(Color(255, 255, 255)));

        // Klick ins Objekt legt das Netz an (3 x 3 Knoten)
        m_canvas->setTool(ToolId::Mesh);
        click(screen(QPointF(30, 30)));
        auto fill = [&] { return m_editor->document().findAs<DrawObject>(id)->style().fill; };
        QVERIFY(fill().isMesh());
        QCOMPARE(fill().mesh.nodeCount(), 9);
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Maschenfüllung"));

        // Knoten oben links wählen und rot färben
        click(screen(QPointF(20, 20)));
        QCOMPARE(m_canvas->meshTool().selectedNodes(), QList<int>{0});
        QVERIFY(m_canvas->offerPaletteColor(Color(255, 0, 0), true));
        QCOMPARE(fill().mesh.colors[0], Color(255, 0, 0));
        QCOMPARE(fill().mesh.colors[4], Color(255, 255, 255));

        // Mittelknoten ziehen
        press(screen(QPointF(50, 40)));
        move(screen(QPointF(55, 42)));
        release(screen(QPointF(60, 45)));
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Maschenknoten verschieben"));
        const QPointF mid = fill().mesh.points[4];
        QVERIFY2(std::abs(mid.x() - 40.0 / 60.0) < 0.02 && std::abs(mid.y() - 25.0 / 40.0) < 0.03,
                 qPrintable(QStringLiteral("%1 %2").arg(mid.x()).arg(mid.y())));

        // Gerastert: oben links rot, unten rechts weiss
        const QImage img = fill().mesh.render(QSize(60, 40));
        QVERIFY(qRed(img.pixel(0, 0)) > 240 && qGreen(img.pixel(0, 0)) < 40);
        QVERIFY(qRed(img.pixel(59, 39)) > 240 && qGreen(img.pixel(59, 39)) > 240);
        QVERIFY(qAlpha(img.pixel(30, 20)) == 255);

        // Speichern/Laden, neu aufteilen behält die Farben ungefähr
        QCOMPARE(Fill::fromJson(fill().toJson()), fill());
        m_canvas->meshTool().setGrid(4, 4);
        QCOMPARE(fill().mesh.nodeCount(), 25);
        QCOMPARE(fill().mesh.colors[0], Color(255, 0, 0));
        // Entf löscht das Netz
        QTest::keyClick(m_canvas.get(), Qt::Key_Delete);
        QVERIFY(!fill().isMesh());
        m_canvas->setTool(ToolId::Pick);
    }

    void exportAndImposition()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        auto rect = std::make_unique<RectangleObject>(QSizeF(50, 30));
        rect->setTransform(QTransform::fromTranslate(10, 10));
        Style s;
        s.fill = Fill::solid(Color(255, 0, 0));
        rect->setStyle(s);
        m_editor->addObject(std::move(rect), QStringLiteral("R"));
        QTemporaryDir dir;
        QString error;
        // Bitmap: rot an der Stelle des Rechtecks, weiss daneben
        const QString png = dir.filePath(QStringLiteral("seite.png"));
        QVERIFY2(output::exportBitmap(*m_canvas, png, 0, 96, false, &error), qPrintable(error));
        QImage img(png);
        QVERIFY(!img.isNull());
        const QSizeF page = m_editor->document().page(0).size();
        QCOMPARE(img.width(), int(std::lround(page.width() / 25.4 * 96)));
        const QColor inside = img.pixelColor(int(30 / 25.4 * 96), int(20 / 25.4 * 96));
        QVERIFY2(inside.red() > 240 && inside.green() < 20, qPrintable(inside.name()));
        QCOMPARE(img.pixelColor(int(100 / 25.4 * 96), int(150 / 25.4 * 96)), QColor(Qt::white));
        // PDF, SVG, HTML
        const QString pdf = dir.filePath(QStringLiteral("doc.pdf"));
        QVERIFY2(output::exportPdf(*m_canvas, pdf, QList<int>{0}, &error), qPrintable(error));
        QFile pf(pdf);
        QVERIFY(pf.open(QIODevice::ReadOnly) && pf.read(5) == "%PDF-");
        const QByteArray svg = output::pageSvg(*m_canvas, 0);
        QVERIFY(svg.contains("<svg") && svg.contains("#ff0000"));
        const QString html = dir.filePath(QStringLiteral("doc.html"));
        QVERIFY2(output::exportHtml(*m_canvas, html, QStringLiteral("Test & Co"), &error), qPrintable(error));
        QFile hf(html);
        QVERIFY(hf.open(QIODevice::ReadOnly));
        const QByteArray h = hf.readAll();
        QVERIFY(h.contains("<title>Test &amp; Co</title>") && h.contains("<svg") && !h.contains("<?xml"));

        // Nicht druckbare Ebene fehlt im Export
        const NodeId layer = m_editor->document().page(0).child(0)->id();
        {
            Transaction tx(m_editor->document(), QStringLiteral("L"));
            tx.modify<Layer>(layer, [](Layer& l) { l.setPrintable(false); });
            tx.commit();
        }
        QVERIFY(output::exportBitmap(*m_canvas, png, 0, 96, false, &error));
        QCOMPARE(QImage(png).pixelColor(int(30 / 25.4 * 96), int(20 / 25.4 * 96)), QColor(Qt::white));
        m_editor->document().undo();

        // Ausschiessen: Etiketten 2 x 3 je Bogen, Layout "Buch" 2 Seiten nebeneinander
        Options& o = Options::instance();
        o.attach(m_editor.get());
        o.set(QStringLiteral("doc/label/labels"), 1);
        o.set(QStringLiteral("doc/label/w"), 90.0);
        o.set(QStringLiteral("doc/label/h"), 50.0);
        o.set(QStringLiteral("doc/label/cols"), 2);
        o.set(QStringLiteral("doc/label/rows"), 3);
        o.set(QStringLiteral("doc/label/left"), 10.0);
        o.set(QStringLiteral("doc/label/top"), 20.0);
        o.set(QStringLiteral("doc/label/hgap"), 5.0);
        o.set(QStringLiteral("doc/label/vgap"), 2.0);
        output::Imposition imp = output::impose(*m_canvas, {0, 0, 0, 0, 0, 0, 0});
        QCOMPARE(imp.sheets.size(), 2);
        QCOMPARE(imp.sheets[0].pages.size(), 6);
        QCOMPARE(imp.sheets[0].pages[3].second, QPointF(105, 72));
        QCOMPARE(imp.sheets[1].pages[0].second, QPointF(10, 20));
        o.set(QStringLiteral("doc/label/labels"), 0);
        o.set(QStringLiteral("doc/layout/type"), 1);
        imp = output::impose(*m_canvas, {0, 0, 0});
        QCOMPARE(imp.sheets.size(), 2);
        QCOMPARE(imp.sheetSize, QSizeF(page.width() * 2, page.height()));
        QCOMPARE(imp.sheets[0].pages[1].second, QPointF(page.width(), 0));
        o.set(QStringLiteral("doc/layout/type"), 0);
    }

    void spellingInTextToolAndDialog()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        SpellChecker& sc = SpellChecker::instance();
        sc.setSearchPaths({QStringLiteral(VEK_DICTIONARY_SOURCE)});
        sc.setUserDictionary(QString());
        sc.setLanguage(QStringLiteral("de_CH"));
        Options::instance().set(QStringLiteral("ws/text/autocorrect/capSentence"), false);

        // Mengentext aufziehen und tippen; das Wort an der Schreibmarke zählt noch nicht
        m_canvas->setTool(ToolId::Text);
        press(screen(QPointF(20, 20)));
        move(screen(QPointF(80, 40)));
        release(screen(QPointF(120, 60)));
        TextTool& tt = m_canvas->textTool();
        QVERIFY(tt.editing());
        QTest::keyClicks(m_canvas.get(), QStringLiteral("Ein Tesst"));
        QVERIFY(tt.spellingErrors().isEmpty());
        QTest::keyClicks(m_canvas.get(), QStringLiteral(" mit Feler"));
        QCOMPARE(tt.spellingErrors().size(), 1);
        QCOMPARE(tt.text().mid(tt.spellingErrors().first().start, tt.spellingErrors().first().length), QStringLiteral("Tesst"));
        // Kontextmenü: erster Vorschlag ersetzt das Wort
        QMenu menu;
        QVERIFY(tt.fillSpellMenu(menu, 6));
        QVERIFY(!menu.actions().isEmpty());
        QAction* test = nullptr;
        for (QAction* a : menu.actions())
            if (a->text() == QStringLiteral("Test"))
                test = a;
        QVERIFY(test);
        test->trigger();
        QCOMPARE(tt.text(), QStringLiteral("Ein Test mit Feler"));
        QVERIFY(!tt.fillSpellMenu(menu, 1)); // "Ein" ist richtig
        QTest::keyClick(m_canvas.get(), Qt::Key_Escape);
        const NodeId id = m_editor->selection().value(0);
        QVERIFY(m_editor->document().findAs<TextObject>(id));

        // Dialog: findet "Feler", Ersetzen schreibt ins Dokument
        SpellDialog dialog(*m_editor);
        QCOMPARE(dialog.currentWord(), QStringLiteral("Feler"));
        dialog.replace(QStringLiteral("Fehler"));
        QCOMPARE(m_editor->document().findAs<TextObject>(id)->plainText(), QStringLiteral("Ein Test mit Fehler"));
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Rechtschreibung"));
        QVERIFY(dialog.currentWord().isEmpty());
        m_canvas->setTool(ToolId::Pick);
    }

    void fontListFollowsOptions()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        QSettings().remove(QStringLiteral("fonts/recent"));
        auto text = std::make_unique<TextObject>(TextObject::Kind::Artistic);
        text->setPlainText(QStringLiteral("Hallo"), CharFormat(QStringLiteral("Courier New"), 12));
        m_editor->addObject(std::move(text), QStringLiteral("T"));
        Options& o = Options::instance();
        o.set(QStringLiteral("ws/text/fonts/docFonts"), true);
        QCOMPARE(FontBox::families(m_editor->document()), QStringList{QStringLiteral("Courier New")});
        o.set(QStringLiteral("ws/text/fonts/docFonts"), false);
        // Ohne Filter: die Schriften des Systems (offscreen ohne Schriftordner evtl. sehr wenige)
        for (const QString& f : FontBox::families(m_editor->document()))
            QVERIFY(QFontDatabase::families().contains(f));
        // Zuletzt benutzte Schriften stehen oben, höchstens so viele wie eingestellt
        o.set(QStringLiteral("ws/text/fonts/recent"), 2);
        FontBox::noteUsed(QStringLiteral("Arial"));
        FontBox::noteUsed(QStringLiteral("Courier New"));
        FontBox::noteUsed(QStringLiteral("Times New Roman"));
        QCOMPARE(FontBox::recentFamilies(), (QStringList{QStringLiteral("Times New Roman"), QStringLiteral("Courier New")}));
        FontBox box(*m_editor);
        QCOMPARE(box.itemData(0).toString(), QStringLiteral("Times New Roman"));
        box.setCurrentFamily(QStringLiteral("Gibt Es Nicht"));
        QCOMPARE(box.currentFamily(), QStringLiteral("Gibt Es Nicht"));
        QSettings().remove(QStringLiteral("fonts/recent"));
        o.set(QStringLiteral("ws/text/fonts/recent"), 5);
    }

    void bitmapEffectPreviewAndLastValues()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        QSettings().remove(QStringLiteral("bitmapfx"));
        Options& o = Options::instance();
        o.set(QStringLiteral("global/bitmapfx/preview"), 1);
        o.set(QStringLiteral("global/bitmapfx/lastValues"), true);
        QImage img(40, 20, QImage::Format_RGB32);
        img.fill(Qt::red);
        auto fn = [](const QImage& i, const QList<int>&) {
            QImage out = i.convertToFormat(QImage::Format_ARGB32);
            out.fill(Qt::blue);
            return out;
        };
        const QList<SliderSpec> sliders{SliderSpec{QStringLiteral("A:")}};
        {
            EffectDialog d(QStringLiteral("TestFx"), img, sliders, fn);
            QCOMPARE(d.previewMode(), 1);
            const QImage shown = d.previewImage();
            QCOMPARE(shown.pixelColor(2, 10), QColor(Qt::red));
            QCOMPARE(shown.pixelColor(shown.width() - 3, 10), QColor(Qt::blue));
            d.setValues({42});
            d.accept();
        }
        EffectDialog again(QStringLiteral("TestFx"), img, sliders, fn);
        QCOMPARE(again.values().value(0), 42);
        o.set(QStringLiteral("global/bitmapfx/preview"), 2);
        EffectDialog plain(QStringLiteral("TestFx"), img, sliders, fn);
        QCOMPARE(plain.previewImage().pixelColor(2, 10), QColor(Qt::blue));
        QSettings().remove(QStringLiteral("bitmapfx"));
        o.set(QStringLiteral("global/bitmapfx/preview"), 3);
    }

    void javascriptDrivesDocument()
    {
        ScriptHost host(*m_editor);
        const int before = m_editor->document().undoCount();
        ScriptHost::Result r = host.run(QStringLiteral(R"(
            const a = tg.rectangle(10, 20, 30, 15, 2);
            a.fill = "#ff0000";
            a.stroke = null;
            const b = tg.ellipse(50, 20, 20, 20);
            b.fill = "royalblue";
            b.move(5, 0);
            const t = tg.text(10, 60, "Hallo", 18);
            const p = tg.polygon(100, 20, 30, 30, 6);
            p.set("points", 8);
            const l = tg.polyline([[0, 0], [10, 5], [20, 0]]);
            console.log("Objekte:", tg.objects().length, a.width, b.x, p.get("points"));
            a.id;
        )"), QStringLiteral("test"));
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.output, QStringList{QStringLiteral("Objekte: 5 30 55 8")});
        // Ein Rückgängig-Schritt für den ganzen Lauf
        QCOMPARE(m_editor->document().undoCount(), before + 1);
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Skript: test"));
        const auto* rect = m_editor->document().findAs<RectangleObject>(NodeId(r.value.toInt()));
        QVERIFY(rect);
        QCOMPARE(rect->style().fill, Fill::solid(Color(255, 0, 0)));
        QVERIFY(!rect->style().stroke);
        QVERIFY(near(rect->boundingRect(), QRectF(10, 20, 30, 15), 0.01));
        QCOMPARE(m_editor->selection().size(), 1); // zuletzt erzeugtes Objekt
        m_editor->document().undo();
        QVERIFY(!m_editor->document().findAs<RectangleObject>(rect->id()));
        m_editor->document().redo();

        // Auswahl lesen und ändern, Text
        r = host.run(QStringLiteral(R"(
            for (const o of tg.objects()) if (o.type === "text") { o.text = "Welt"; tg.select(o); }
            tg.selection()[0].text;
        )"));
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.value.toString(), QStringLiteral("Welt"));

        // Fehler: nichts geändert, Meldung mit Zeile
        const int count = m_editor->document().undoCount();
        r = host.run(QStringLiteral("tg.rectangle(0, 0, 10, 10);\nnichtDa();"));
        QVERIFY(!r.ok);
        QVERIFY2(r.error.contains(QStringLiteral("Zeile 2")), qPrintable(r.error));
        QCOMPARE(m_editor->document().undoCount(), count);
        r = host.run(QStringLiteral("tg.objects()[0].fill = 'keineFarbe';"));
        QVERIFY(!r.ok && r.error.contains(QStringLiteral("Unbekannte Farbe")));
        // Endlosschleife wird abgebrochen
        r = host.run(QStringLiteral("while (true) {}"), QString(), 300);
        QVERIFY(!r.ok && r.error.contains(QStringLiteral("Abgebrochen")));
        QCOMPARE(m_editor->document().undoCount(), count);
    }

    void alignAndDistributeObjects()
    {
        auto add = [this](const QRectF& r) {
            auto o = std::make_unique<RectangleObject>(r.size());
            o->setTransform(QTransform::fromTranslate(r.left(), r.top()));
            Style s;
            s.stroke.reset(); // ohne Kontur: Begrenzung = Geometrie
            s.fill = Fill::solid(Color(200, 200, 200));
            o->setStyle(s);
            return m_editor->addObject(std::move(o), QStringLiteral("R"));
        };
        const NodeId a = add(QRectF(10, 10, 20, 10));
        const NodeId b = add(QRectF(50, 40, 10, 30));
        const NodeId c = add(QRectF(90, 25, 15, 15));
        auto rect = [this](NodeId id) { return m_editor->document().findAs<DrawObject>(id)->boundingRect(); };

        // Angeklickt: Bezug = zuletzt gewähltes Objekt (b)
        m_editor->setSelection({a, c, b});
        QCOMPARE(m_editor->alignReference(), b);
        m_editor->alignSelection(align::H::None, align::V::Top);
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Ausrichten"));
        QCOMPARE(rect(a).top(), 40.0);
        QCOMPARE(rect(c).top(), 40.0);
        QCOMPARE(rect(b).top(), 40.0);
        m_editor->document().undo();
        // Mit dem Rahmen gewählt: Bezug = Objekt oben links (a)
        m_editor->setSelection({b, c, a}, true);
        QCOMPARE(m_editor->alignReference(), a);
        m_editor->alignSelection(align::H::Left, align::V::None);
        QCOMPARE(rect(b).left(), 10.0);
        QCOMPARE(rect(c).left(), 10.0);
        m_editor->document().undo();
        // Mittelpunkt auf Seite (Taste P): alle Mitten in der Seitenmitte
        const QPointF mid = QRectF(QPointF(0, 0), m_editor->currentPage().size()).center();
        m_editor->alignSelection(align::H::Center, align::V::Center, Editor::AlignTo::PageEdge);
        QVERIFY(QLineF(rect(a).center(), mid).length() < 1e-9 && QLineF(rect(c).center(), mid).length() < 1e-9);
        m_editor->document().undo();
        // Verteilen: Abstand waagrecht im Auswahlumfang, ein Rückgängig-Schritt
        m_editor->distributeSelection(align::Spread::Spacing, align::Spread::None, false);
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Verteilen"));
        QCOMPARE(rect(a).left(), 10.0);
        QCOMPARE(rect(c).right(), 105.0);
        const double gap1 = rect(b).left() - rect(a).right(), gap2 = rect(c).left() - rect(b).right();
        QVERIFY(std::abs(gap1 - gap2) < 1e-9 && std::abs(gap1 - 25.0) < 1e-9);
        m_editor->document().undo();

        // Dialog: Reiter Ausrichten mit Bezug Seitenrand, dann Verteilen
        AlignDialog dialog(*m_editor);
        dialog.setAlign(align::H::Right, align::V::None, 1);
        dialog.apply();
        const double pageRight = m_editor->currentPage().size().width();
        QCOMPARE(rect(a).right(), pageRight);
        QCOMPARE(rect(b).right(), pageRight);
        dialog.setDistribute(align::Spread::None, align::Spread::Center, true);
        dialog.apply();
        QCOMPARE(rect(a).center().y(), 5.0);
        // Tastenkürzel über das Menü: T richtet oben aus (Bezug a, mit dem Rahmen gewählt)
        MainWindow window;
        QAction* top = nullptr;
        for (QAction* act : window.findChildren<QAction*>())
            if (act->shortcut() == QKeySequence(Qt::Key_T))
                top = act;
        QVERIFY(top);
    }

    void arrangeMenuCommands()
    {
        auto add = [this](std::unique_ptr<DrawObject> o, const QRectF& r) {
            o->setTransform(QTransform::fromTranslate(r.left(), r.top()));
            Style s;
            s.fill = Fill::solid(Color(200, 0, 0));
            s.stroke = Color(0, 0, 0);
            s.strokeWidth = 2.0;
            o->setStyle(s);
            return m_editor->addObject(std::move(o), QStringLiteral("R"));
        };
        auto rect = [this](NodeId id) { return m_editor->document().findAs<DrawObject>(id)->boundingRect(); };
        const NodeId a = add(std::make_unique<RectangleObject>(QSizeF(20, 10)), QRectF(10, 10, 20, 10));

        // Änderungen-Andockfenster: Position absolut mit Bezug oben links, dann relativ mit 2 Kopien
        TransformDocker docker(*m_editor);
        m_editor->setSelection({a});
        docker.showPage(TransformDocker::Page::Position);
        docker.setAnchor(0);
        docker.setValues(50.0, 60.0);
        docker.apply();
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Position"));
        QVERIFY(std::abs(rect(a).left() - 50.0) < 1e-9 && std::abs(rect(a).top() - 60.0) < 1e-9);
        docker.setRelative(true);
        docker.setValues(30.0, 0.0);
        docker.setCopies(2);
        docker.apply();
        QCOMPARE(m_editor->selection().size(), 1);
        QVERIFY(std::abs(rect(m_editor->selection().first()).left() - 110.0) < 1e-9);
        m_editor->document().undo(); // ein Schritt für beide Kopien
        QCOMPARE(m_editor->document().page(0).child(0)->childCount(), 1);
        // Grösse mit Bezug Mitte, Drehen 90°
        m_editor->setSelection({a});
        docker.setCopies(0);
        docker.showPage(TransformDocker::Page::Size);
        docker.setAnchor(4);
        const QPointF c = rect(a).center();
        docker.setValues(rect(a).width() * 2, rect(a).height());
        docker.apply();
        QVERIFY(QLineF(rect(a).center(), c).length() < 1e-6);
        docker.showPage(TransformDocker::Page::Rotate);
        docker.setValues(90.0, 0.0);
        docker.apply();
        QVERIFY(m_editor->document().findAs<DrawObject>(a)->transform().type() >= QTransform::TxRotate);
        // Änderungen zurücknehmen: Drehung weg, Mitte bleibt
        const QPointF mid = rect(a).center();
        m_editor->clearTransformations();
        QVERIFY(m_editor->document().findAs<DrawObject>(a)->transform().type() <= QTransform::TxTranslate);
        QVERIFY(QLineF(rect(a).center(), mid).length() < 1e-6);

        // Umriss in Objekt umwandeln: neue gefüllte Kurve in Konturfarbe, Original ohne Kontur
        m_editor->setSelection({a});
        QVERIFY(m_editor->outlineToObject());
        const auto* ring = m_editor->document().findAs<CurveObject>(m_editor->selection().value(0));
        QVERIFY(ring && ring->style().fill == Fill::solid(Color(0, 0, 0)) && !ring->style().stroke);
        QVERIFY(!m_editor->document().findAs<DrawObject>(a)->style().stroke);
        QVERIFY(std::abs(ring->boundingRect().width() - (rect(a).width() + 2.0)) < 0.05);

        // Kurven verbinden: zwei Linien mit gemeinsamem Endpunkt -> ein Linienzug
        auto line = [](QPointF p, QPointF q) {
            SubPath sp;
            sp.nodes = {CurveNode{p}, CurveNode{q}};
            return std::make_unique<CurveObject>(QList<SubPath>{sp});
        };
        const NodeId l1 = m_editor->addObject(line(QPointF(0, 100), QPointF(20, 100)), QStringLiteral("L"));
        const NodeId l2 = m_editor->addObject(line(QPointF(40, 120), QPointF(20.2, 100.1)), QStringLiteral("L"));
        const NodeId l3 = m_editor->addObject(line(QPointF(40, 120), QPointF(0.1, 100.2)), QStringLiteral("L"));
        m_editor->setSelection({l1, l2, l3});
        QVERIFY(m_editor->joinCurves(0.5));
        const auto* joined = m_editor->document().findAs<CurveObject>(m_editor->selection().value(0));
        QVERIFY(joined);
        QCOMPARE(joined->subPaths().size(), 1);
        QVERIFY(joined->subPaths().first().closed);
        QCOMPARE(joined->subPaths().first().nodes.size(), 3);

        // Gruppierung aller Objekte aufheben, Alle freigeben
        const NodeId b = add(std::make_unique<RectangleObject>(QSizeF(5, 5)), QRectF(0, 0, 5, 5));
        const NodeId c2 = add(std::make_unique<RectangleObject>(QSizeF(5, 5)), QRectF(10, 0, 5, 5));
        m_editor->setSelection({b, c2});
        m_editor->groupSelection();
        const NodeId inner = m_editor->selection().first();
        m_editor->setSelection({inner, a});
        m_editor->groupSelection();
        m_editor->ungroupAll();
        QCOMPARE(m_editor->selection().size(), 3);
        m_editor->setSelectionLocked(true);
        m_editor->unlockAll();
        QVERIFY(!m_editor->document().findAs<DrawObject>(b)->locked());
    }

    void editMenuCommands()
    {
        auto add = [this](const QRectF& r, const Color& fill) {
            auto o = std::make_unique<RectangleObject>(r.size());
            o->setTransform(QTransform::fromTranslate(r.left(), r.top()));
            Style s;
            s.fill = Fill::solid(fill);
            s.stroke = Color(0, 0, 0);
            s.strokeWidth = 0.5;
            o->setStyle(s);
            return m_editor->addObject(std::move(o), QStringLiteral("R"));
        };
        auto obj = [this](NodeId id) { return m_editor->document().findAs<DrawObject>(id); };
        const NodeId a = add(QRectF(10, 10, 20, 10), Color(255, 0, 0));
        const NodeId b = add(QRectF(50, 10, 10, 10), Color(0, 0, 255));

        // Wiederholen: Verschieben auf ein anderes Objekt
        m_editor->setSelection({a});
        m_editor->transformSelection(QTransform::fromTranslate(5, 0), QStringLiteral("Verschieben"));
        m_editor->setSelection({b});
        QVERIFY(m_editor->canRepeat());
        m_editor->repeatLast();
        QCOMPARE(obj(b)->boundingRect().left(), 55.0);

        // Klon folgt dem Original
        m_editor->setSelection({a});
        m_editor->cloneSelection();
        const NodeId clone = m_editor->selection().first();
        QVERIFY(node_cast<CloneObject>(obj(clone)));
        const QRectF cloneBefore = obj(clone)->boundingRect();
        QVERIFY(cloneBefore.width() > 19.9);
        m_editor->setSelection({a});
        m_editor->setSelectionFill(Fill::solid(Color(0, 200, 0)));
        m_editor->transformSelection(xform::scaling(2.0, 1.0, QPointF(15, 10)), QStringLiteral("S"));
        const auto* k = node_cast<CloneObject>(obj(clone));
        QCOMPARE(k->rendered()->style().fill, Fill::solid(Color(0, 200, 0)));
        QVERIFY(std::abs(obj(clone)->boundingRect().width() - 2 * cloneBefore.width()) < 0.01);
        // Speichern/Laden behält den Stand, In Objekte zurückwandeln
        auto back = serial::fromJson(serial::toJson(*k), serial::Ids::Keep);
        QVERIFY(node_cast<CloneObject>(back.get())->rendered());
        m_editor->setSelection({clone});
        m_editor->convertClonesToObjects();
        QVERIFY(node_cast<RectangleObject>(obj(m_editor->selection().first())));

        // Symbol: Auswahl wird Definition, Instanz an alter Stelle; zweite Instanz
        m_editor->setSelection({b});
        const NodeId sym = m_editor->createSymbol(QStringLiteral("Quadrat"));
        QCOMPARE(m_editor->symbols(), QList<NodeId>{sym});
        const NodeId inst = m_editor->selection().first();
        QVERIFY(std::abs(obj(inst)->boundingRect().left() - 55.0) < 1e-6);
        m_editor->placeSymbol(sym, QPointF(100, 100));
        QCOMPARE(m_editor->symbolUses(sym), 2);
        QVERIFY(!m_editor->deleteSymbol(sym));
        // Instanzen sind wählbar, die Definition (verborgene Ebene) nicht
        bool pickable = false;
        for (const DrawObject* o : m_editor->pickableObjects())
            pickable = pickable || o->id() == sym;
        QVERIFY(!pickable);

        // Eigenschaften kopieren: Füllung von a auf die Instanz-Kopie
        const NodeId c = add(QRectF(10, 50, 10, 10), Color(10, 10, 10));
        m_editor->setSelection({c});
        m_editor->copyPropertiesFrom(a, Editor::CopyFill | Editor::CopyOutlinePen);
        QCOMPARE(obj(c)->style().fill, Fill::solid(Color(0, 200, 0)));

        // Vielfach duplizieren, Objekte suchen
        m_editor->setSelection({c});
        m_editor->transformSelectionCopies(QTransform::fromTranslate(15, 0), 3, QStringLiteral("Vielfach duplizieren"));
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Vielfach duplizieren"));
        const int greens = m_editor->selectWhere([](const DrawObject& o) { return o.style().fill == Fill::solid(Color(0, 200, 0)); });
        QCOMPARE(greens, 6); // a, zurückgewandelter Klon, c und drei Kopien

        // Text ersetzen
        auto text = std::make_unique<TextObject>(TextObject::Kind::Artistic);
        text->setPlainText(QStringLiteral("Hallo Welt, hallo Bern"), CharFormat(QStringLiteral("Arial"), 12));
        const NodeId t = m_editor->addObject(std::move(text), QStringLiteral("T"));
        QCOMPARE(m_editor->replaceText(QStringLiteral("hallo"), QStringLiteral("Grüezi"), false, true), 2);
        QCOMPARE(m_editor->document().findAs<TextObject>(t)->plainText(), QStringLiteral("Grüezi Welt, Grüezi Bern"));
        m_editor->selectAllText();
        QCOMPARE(m_editor->selection(), QList<NodeId>{t});
    }

    void pipettesSampleAndApply()
    {
        m_canvas->resize(800, 600);
        auto add = [this](const QRectF& r, const Color& fill, double strokeWidth) {
            auto o = std::make_unique<RectangleObject>(r.size());
            o->setTransform(QTransform::fromTranslate(r.left(), r.top()));
            Style s;
            s.fill = Fill::solid(fill);
            s.stroke = Color(0, 0, 0);
            s.strokeWidth = strokeWidth;
            o->setStyle(s);
            return m_editor->addObject(std::move(o), QStringLiteral("R"));
        };
        const NodeId a = add(QRectF(20, 20, 30, 30), Color(250, 120, 0), 0.5);
        const NodeId b = add(QRectF(80, 20, 30, 30), Color(0, 0, 255), 0.2);
        auto obj = [this](NodeId id) { return m_editor->document().findAs<DrawObject>(id); };

        // Farbpipette: aufnehmen in a, in b anwenden (Füllung), auf den Rand von b (Kontur)
        m_canvas->setTool(ToolId::ColorPipette);
        click(screen(QPointF(35, 35)));
        auto* pip = static_cast<ColorPipetteTool*>(m_canvas->currentTool());
        QVERIFY(pip->color());
        QCOMPARE(*pip->color(), Color(250, 120, 0));
        click(screen(QPointF(95, 35)));
        QCOMPARE(obj(b)->style().fill, Fill::solid(Color(250, 120, 0)));
        QTest::mouseClick(m_canvas.get(), Qt::LeftButton, Qt::ShiftModifier, screen(QPointF(95, 35)), 1000);
        QCOMPARE(obj(b)->style().stroke, std::optional<Color>(Color(250, 120, 0)));

        // Attributpipette: Kontur/Füllung und Grösse von a auf b
        m_editor->setSelection({a});
        m_editor->setSelectionFill(Fill::solid(Color(0, 160, 0)));
        Options::instance().set(QStringLiteral("ws/tools/attrPipette/transform"), true);
        m_canvas->setTool(ToolId::AttributePipette);
        click(screen(QPointF(35, 35)));
        click(screen(QPointF(95, 35)));
        QCOMPARE(obj(b)->style().fill, Fill::solid(Color(0, 160, 0)));
        QCOMPARE(obj(b)->style().strokeWidth, 0.5);
        QVERIFY(std::abs(obj(b)->boundingRect().width() - obj(a)->boundingRect().width()) < 0.01);
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Attribute übertragen"));
        Options::instance().set(QStringLiteral("ws/tools/attrPipette/transform"), false);
        m_canvas->setTool(ToolId::Pick);
    }

    void effectsMenuCommands()
    {
        m_canvas->resize(800, 600);
        auto add = [this](const QRectF& r, const Color& fill) {
            auto o = std::make_unique<RectangleObject>(r.size());
            o->setTransform(QTransform::fromTranslate(r.left(), r.top()));
            Style s;
            s.fill = Fill::solid(fill);
            s.stroke.reset();
            o->setStyle(s);
            return m_editor->addObject(std::move(o), QStringLiteral("R"));
        };
        auto obj = [this](NodeId id) { return m_editor->document().findAs<DrawObject>(id); };
        auto pixel = [this](const QPointF& doc) {
            QImage img(3, 3, QImage::Format_ARGB32_Premultiplied);
            img.fill(Qt::white);
            QPainter p(&img);
            m_canvas->renderPage(p, 0, 8.0, QPointF(1.5, 1.5) - doc * 8.0, true);
            p.end();
            return img.pixelColor(1, 1);
        };

        // SuperClip: grosses Rechteck in kleinen Rahmen, nur im Rahmen sichtbar
        const NodeId frame = add(QRectF(20, 20, 20, 20), Color(255, 255, 255));
        const NodeId content = add(QRectF(10, 10, 60, 60), Color(0, 0, 255));
        m_editor->setSelection({content});
        const NodeId clip = m_editor->placeInsideFrame(frame);
        QVERIFY(node_cast<SuperClipObject>(obj(clip)));
        QCOMPARE(pixel(QPointF(30, 30)), QColor(0, 0, 255));
        QCOMPARE(pixel(QPointF(60, 60)), QColor(Qt::white)); // ausserhalb des Rahmens unsichtbar
        QVERIFY(near(obj(clip)->boundingRect(), QRectF(20, 20, 20, 20)));
        // Inhalt bewegt sich mit, proportional einpassen, extrahieren
        m_editor->setSelection({clip});
        m_editor->transformSelection(QTransform::fromTranslate(10, 0), QStringLiteral("M"));
        QCOMPARE(obj(content)->boundingRect().left(), 20.0);
        m_editor->fitSuperClipContents(Editor::ClipFit::FitProportional);
        QVERIFY(near(obj(content)->boundingRect(), QRectF(30, 20, 20, 20), 0.01));
        auto back = serial::fromJson(serial::toJson(*obj(clip)), serial::Ids::Keep);
        QCOMPARE(back->childCount(), 2);
        m_editor->extractContents();
        QCOMPARE(m_editor->selection(), QList<NodeId>{content});
        QVERIFY(!obj(clip));

        // Linse: Invertieren über weisser Seite = schwarz; Effekt löschen
        const NodeId lens = add(QRectF(100, 100, 20, 20), Color(255, 0, 0));
        m_editor->setSelection({lens});
        Lens l;
        l.type = Lens::Type::Invert;
        m_editor->setSelectionLens(l);
        QCOMPARE(pixel(QPointF(110, 110)), QColor(Qt::black));
        m_editor->clearSelectionEffects();
        QCOMPARE(pixel(QPointF(110, 110)), QColor(255, 0, 0));

        // Abschrägung: Licht von oben links hellt die obere linke Kante auf, dunkelt unten rechts
        Bevel b;
        b.style = Bevel::Style::Soft;
        b.width = 4.0;
        b.intensity = 100.0;
        m_editor->setSelectionBevel(b);
        const QColor topLeft = pixel(QPointF(100.7, 110)), bottomRight = pixel(QPointF(119.3, 110));
        QVERIFY2(topLeft.green() > 40 && bottomRight.red() < 200, qPrintable(topLeft.name() + bottomRight.name()));
        // Effekt kopieren auf ein anderes Objekt
        const NodeId other = add(QRectF(150, 100, 20, 20), Color(0, 255, 0));
        m_editor->setSelection({other});
        m_editor->copyEffectsFrom(lens, QStringLiteral("Effekt kopieren"));
        QVERIFY(obj(other)->bevel().isActive());

        // Perspektive: rechte obere Ecke nach innen ziehen
        m_editor->setSelection({other});
        m_canvas->setTool(ToolId::Perspective);
        auto* persp = static_cast<PerspectiveTool*>(m_canvas->currentTool());
        QVERIFY(persp->active());
        persp->moveCorner(1, QPointF(165, 105));
        const auto* curve = m_editor->document().findAs<CurveObject>(m_editor->selection().value(0));
        QVERIFY(curve);
        QVERIFY(std::abs(curve->boundingRect().top() - 100.0) < 0.01);
        QVERIFY(std::abs(curve->boundingRect().right() - 170.0) < 0.01); // unten rechts bleibt
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Perspektive"));
        m_canvas->setTool(ToolId::Pick);
    }

    void viewModes()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        Options::instance().attach(m_editor.get());
        m_canvas->resize(800, 600);
        auto o = std::make_unique<RectangleObject>(QSizeF(40, 40));
        o->setTransform(QTransform::fromTranslate(20, 20));
        Style s;
        s.fill = Fill::gradient(Fill::Type::Linear, Color(255, 0, 0), Color(0, 0, 255));
        s.stroke.reset();
        o->setStyle(s);
        const NodeId id = m_editor->addObject(std::move(o), QStringLiteral("R"));
        m_editor->clearSelection();
        auto shot = [this](const QPointF& doc) {
            const QImage img = m_canvas->grab().toImage();
            return img.pixelColor(m_canvas->toScreen(doc).toPoint());
        };
        Options& opt = Options::instance();
        // Erweitert: Verlauf; Entwurf: gleichmässig in der ersten Farbe
        opt.set(QStringLiteral("doc/general/viewMode"), 1);
        const QColor enhanced = shot(QPointF(55, 40));
        opt.set(QStringLiteral("doc/general/viewMode"), 0);
        QCOMPARE(shot(QPointF(55, 40)), QColor(255, 0, 0));
        QVERIFY(enhanced.blue() > 100);
        // Einfacher Umriss: innen weiss, auf dem Rand schwarz
        opt.set(QStringLiteral("doc/general/viewMode"), 2);
        QCOMPARE(shot(QPointF(40, 40)), QColor(Qt::white));
        // Pixel: grobe Pixel (benachbarte Bildschirmpunkte innerhalb eines 96-dpi-Pixels gleich)
        opt.set(QStringLiteral("doc/general/viewMode"), 5);
        QVERIFY(shot(QPointF(40, 40)).red() > 0);
        // Vorschau markierter Objekte: Filter zeichnet nur die gewählten
        m_canvas->setRenderFilter({id + 1000});
        QImage img(10, 10, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::white);
        {
            QPainter p(&img);
            m_canvas->renderPage(p, 0, 1.0, QPointF(-20, -20), false);
        }
        m_canvas->setRenderFilter({});
        QCOMPARE(img.pixelColor(5, 5), QColor(Qt::white));
        opt.set(QStringLiteral("doc/general/viewMode"), 1);
    }

    void tableMenuCommands()
    {
        // Kern: verbinden, einfügen, löschen, aufteilen, verteilen
        TableObject t(QSizeF(60, 30), 3, 3);
        t.setCellText(0, 0, QStringLiteral("A"));
        t.setCellText(0, 1, QStringLiteral("B"));
        t.merge({0, 0, 1, 1});
        QCOMPARE(t.cell(0, 0).text, QStringLiteral("A B"));
        QVERIFY(t.isCovered(1, 1) && !t.isCovered(2, 2));
        QCOMPARE(t.cellAt(QPointF(25, 15)), (std::pair{0, 0}));
        QCOMPARE(t.cellRect(1, 1), QRectF(0, 0, 40, 20));
        t.insertRow(1); // mitten in die verbundene Zelle: sie wächst
        QCOMPARE(t.rows(), 4);
        QCOMPARE(t.cell(0, 0).rowSpan, 3);
        t.removeColumn(0); // Anker in Spalte 0 wandert nach rechts
        QCOMPARE(t.columns(), 2);
        QCOMPARE(t.cell(0, 0).colSpan, 1);
        QCOMPARE(t.cell(0, 0).rowSpan, 3);
        t.unmerge(0, 0);
        QVERIFY(!t.isCovered(1, 0));
        TableObject s(QSizeF(40, 20), 2, 2);
        s.splitIntoRows(0, 0, 3);
        QCOMPARE(s.rows(), 4);
        QVERIFY(!s.isCovered(1, 0) && !s.isCovered(2, 0));
        QCOMPARE(s.cell(0, 1).rowSpan, 3); // Nachbar überspannt die neuen Zeilen
        QVERIFY(std::abs(s.rowHeights()[0] - 10.0 / 3.0) < 1e-9);
        auto back = serial::fromJson(serial::toJson(s), serial::Ids::Keep);
        QCOMPARE(node_cast<TableObject>(back.get())->cell(0, 1).rowSpan, 3);

        // Werkzeug: Bereich ziehen und über das Menü verbinden
        m_editor->snapSettings().objects = false;
        m_editor->snapSettings().page = false;
        m_editor->snapSettings().guides = false;
        auto table = std::make_unique<TableObject>(QSizeF(60, 30), 3, 3);
        table->setTransform(QTransform::fromTranslate(20, 20));
        const NodeId id = m_editor->addObject(std::move(table), QStringLiteral("T"));
        m_canvas->setTool(ToolId::Table);
        TableTool& tool = m_canvas->tableTool();
        press(screen(QPointF(25, 25)));
        move(screen(QPointF(45, 35)));
        release(screen(QPointF(45, 35)));
        QCOMPARE(tool.selection(), (TableObject::Range{0, 0, 1, 1}));
        MainWindow window;
        // Der Befehl über das Hauptfenster braucht dessen Zeichenfläche; hier direkt über den Editor
        tool.finishEditing();
        m_editor->modifyTable(id, [](TableObject& o) { o.merge({0, 0, 1, 1}); }, QStringLiteral("Zellen verbinden"));
        QCOMPARE(m_editor->document().findAs<TableObject>(id)->cell(0, 0).colSpan, 2);
        m_canvas->setTool(ToolId::Pick);

        // Text <-> Tabelle
        auto text = std::make_unique<TextObject>(TextObject::Kind::Artistic);
        text->setPlainText(QStringLiteral("a,b,c\nd,e"), CharFormat(QStringLiteral("Arial"), 10));
        const NodeId tid = m_editor->addObject(std::move(text), QStringLiteral("T"));
        m_editor->setSelection({tid});
        const NodeId made = m_editor->textToTable(QStringLiteral(","));
        const auto* conv = m_editor->document().findAs<TableObject>(made);
        QVERIFY(conv && conv->rows() == 2 && conv->columns() == 3 && conv->cell(1, 1).text == QStringLiteral("e"));
        const NodeId back2 = m_editor->tableToText(QStringLiteral(";"));
        QCOMPARE(m_editor->document().findAs<TextObject>(back2)->plainText(), QStringLiteral("a;b;c\nd;e;"));
    }

    void bitmapCommands()
    {
        // In Bitmap konvertieren: Rechteck wird ein Bild gleicher Lage
        auto o = std::make_unique<RectangleObject>(QSizeF(20, 10));
        o->setTransform(QTransform::fromTranslate(30, 40));
        Style s;
        s.fill = Fill::solid(Color(255, 0, 0));
        s.stroke.reset();
        o->setStyle(s);
        const NodeId id = m_editor->addObject(std::move(o), QStringLiteral("R"));
        QImage pixels(100, 50, QImage::Format_ARGB32_Premultiplied);
        pixels.fill(Qt::red);
        const NodeId img = m_editor->replaceSelectionWithImage(pixels, QRectF(30, 40, 20, 10), QStringLiteral("In Bitmap konvertieren"));
        const auto* image = m_editor->document().findAs<ImageObject>(img);
        QVERIFY(image && !m_editor->document().find(id));
        QVERIFY(near(image->boundingRect(), QRectF(30, 40, 20, 10)));
        // Rahmen auf die linke Hälfte, zuschneiden: Bild halb so breit, Lage bleibt
        SubPath frame;
        frame.closed = true;
        frame.nodes = {CurveNode{QPointF(0, 0)}, CurveNode{QPointF(10, 0)}, CurveNode{QPointF(10, 10)}, CurveNode{QPointF(0, 10)}};
        m_editor->setImageFrame(img, {frame}, QStringLiteral("Rahmen"));
        QVERIFY(m_editor->cropBitmapToFrame(img));
        image = m_editor->document().findAs<ImageObject>(img);
        QCOMPARE(image->image().width(), 50);
        QVERIFY(std::abs(image->size().width() - 10.0) < 1e-6);
        QVERIFY(near(image->boundingRect(), QRectF(30, 40, 10, 10), 0.01));
    }

    void dropShadowTool()
    {
        m_canvas->resize(800, 600);
        m_editor->snapSettings().objects = false;
        m_editor->snapSettings().page = false;
        auto o = std::make_unique<RectangleObject>(QSizeF(30, 30));
        o->setTransform(QTransform::fromTranslate(40, 40));
        Style s;
        s.fill = Fill::solid(Color(255, 255, 0));
        s.stroke.reset();
        o->setStyle(s);
        const NodeId id = m_editor->addObject(std::move(o), QStringLiteral("R"));
        auto obj = [this, id] { return m_editor->document().findAs<DrawObject>(id); };
        auto pixel = [this](const QPointF& doc) {
            QImage img(3, 3, QImage::Format_ARGB32_Premultiplied);
            img.fill(Qt::white);
            QPainter p(&img);
            m_canvas->renderPage(p, 0, 8.0, QPointF(1.5, 1.5) - doc * 8.0, true);
            p.end();
            return img.pixelColor(1, 1);
        };
        // Flach: aus der Mitte 15 mm nach rechts unten ziehen = 5 mm Versatz (3-fach untersetzt)
        m_canvas->setTool(ToolId::DropShadow);
        press(screen(QPointF(55, 55)));
        move(screen(QPointF(62, 62)));
        release(screen(QPointF(70, 70)));
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Hinterlegter Schatten"));
        QVERIFY(obj()->shadow().active && !obj()->shadow().perspective);
        QVERIFY(QLineF(obj()->shadow().offset, QPointF(5, 5)).length() < 0.3);
        QVERIFY(QLineF(ShadowTool::handles(*obj(), obj()->shadow()).second, QPointF(70, 70)).length() < 0.6);
        // Unter dem Objekt rechts unten liegt jetzt Schatten (grau statt weiss), das Objekt deckt
        QVERIFY2(pixel(QPointF(73, 73)).red() < 230, qPrintable(pixel(QPointF(73, 73)).name()));
        QCOMPARE(pixel(QPointF(55, 55)), QColor(255, 255, 0));
        QCOMPARE(pixel(QPointF(30, 30)), QColor(Qt::white));
        // Perspektivisch: an der Unterkante ansetzen, nach oben rechts ziehen
        const DropShadow p = ShadowTool::shadowFor(*obj(), QPointF(55, 70), QPointF(70, 55), obj()->shadow());
        QVERIFY(p.perspective && !p.fromTop);
        QVERIFY(std::abs(p.stretch - 50.0) < 1.0);
        QVERIFY(std::abs(std::abs(p.angle) - 45.0) < 1.0);
        const auto [start, end] = ShadowTool::handles(*obj(), p);
        QVERIFY(QLineF(start, QPointF(55, 70)).length() < 1e-6);
        QVERIFY(QLineF(end, QPointF(70, 55)).length() < 0.5);
        // Speichern/Laden, Effekt löschen entfernt auch den Schatten
        auto back = serial::fromJson(serial::toJson(*obj()), serial::Ids::Keep);
        QVERIFY(node_cast<DrawObject>(back.get())->shadow().active);
        m_editor->setSelection({id});
        m_editor->clearSelectionEffects();
        QVERIFY(!obj()->shadow().active);
        m_canvas->setTool(ToolId::Pick);
    }

    void interactiveEffectTools()
    {
        m_canvas->resize(800, 600);
        m_editor->snapSettings().objects = false;
        m_editor->snapSettings().page = false;
        auto add = [this](std::unique_ptr<DrawObject> o, const QRectF& r, const Color& fill) {
            o->setTransform(QTransform::fromTranslate(r.left(), r.top()));
            Style s;
            s.fill = Fill::solid(fill);
            s.stroke = Color(0, 0, 0);
            s.strokeWidth = 0.2;
            o->setStyle(s);
            return m_editor->addObject(std::move(o), QStringLiteral("R"));
        };
        auto obj = [this](NodeId id) { return m_editor->document().findAs<DrawObject>(id); };
        // Überblendung von Rechteck (rot) zu Ellipse (blau) per Ziehen
        const NodeId a = add(std::make_unique<RectangleObject>(QSizeF(10, 10)), QRectF(20, 20, 10, 10), Color(255, 0, 0));
        const NodeId b = add(std::make_unique<EllipseObject>(QSizeF(10, 10)), QRectF(80, 20, 10, 10), Color(0, 0, 255));
        Options::instance().set(QStringLiteral("ws/tools/blend/steps"), 5);
        m_canvas->setTool(ToolId::Blend);
        press(screen(QPointF(25, 25)));
        move(screen(QPointF(60, 25)));
        release(screen(QPointF(85, 25)));
        const auto* blend = node_cast<BlendObject>(obj(m_editor->selection().value(0)));
        QVERIFY(blend);
        QCOMPARE(blend->intermediates().size(), 5);
        const auto mid = blend->intermediates()[2];
        QCOMPARE(mid->style().fill, Fill::solid(Color(128, 0, 128)));
        QVERIFY(std::abs(mid->boundingRect().center().x() - 55.0) < 0.5);
        // Ende verschieben: Stufen folgen
        m_editor->setSelection({blend->id()});
        m_editor->setObjectProperty(QStringLiteral("steps"), 9, QStringLiteral("Schritte"));
        QCOMPARE(node_cast<BlendObject>(obj(blend->id()))->intermediates().size(), 9);
        // Trennen (Strg+K): Gruppe aus 11 Objekten
        QVERIFY(m_editor->separateEffects());
        QCOMPARE(m_editor->document().find(m_editor->selection().first())->childCount(), 11);
        Q_UNUSED(a);
        Q_UNUSED(b);

        // Kontur aussen per Ziehen
        const NodeId c = add(std::make_unique<RectangleObject>(QSizeF(20, 20)), QRectF(20, 60, 20, 20), Color(0, 200, 0));
        m_canvas->setTool(ToolId::Contour);
        Options::instance().set(QStringLiteral("ws/tools/contour/steps"), 4);
        press(screen(QPointF(40, 70)));
        move(screen(QPointF(44, 70)));
        release(screen(QPointF(48, 70)));
        QVERIFY(obj(c)->contour().active);
        QCOMPARE(obj(c)->contour().direction, ContourEffect::Direction::Outside);
        QVERIFY(std::abs(obj(c)->contour().offset - 2.0) < 0.2);
        const auto steps = Canvas::contourSteps(*obj(c), obj(c)->contour());
        QCOMPARE(steps.size(), 4);
        QVERIFY(steps.first().path.boundingRect().width() > 35.0); // äusserste Stufe ~ 20 + 2*8

        // Extrusion: Seitenflächen zum Fluchtpunkt
        const NodeId e = add(std::make_unique<RectangleObject>(QSizeF(20, 20)), QRectF(100, 60, 20, 20), Color(200, 200, 0));
        m_editor->setSelection({e});
        Extrusion ex;
        ex.active = true;
        ex.vanish = QPointF(40, -40);
        ex.depth = 25;
        m_editor->setSelectionExtrusion(ex, QStringLiteral("Extrusion"));
        const auto faces = Canvas::extrusionFaces(*obj(e), obj(e)->extrusion());
        QVERIFY(faces.size() >= 5); // Rückseite + 4 Seiten
        QVERIFY(QLineF(faces.first().first.boundingRect().center(), QPointF(120, 60)).length() < 1.0); // Rückseite verschoben

        // Verzerren: Wirbel dreht Punkte nahe der Mitte stärker
        const QList<SubPath> sq = CurveObject::fromPath([] {
            QPainterPath p;
            p.addRect(0, 0, 20, 20);
            return p;
        }())->subPaths();
        const auto twisted = DistortTool::distort(sq, DistortTool::Mode::Twister, 90, 10, QPointF(10, 10));
        QVERIFY(!twisted.isEmpty() && twisted.first().nodes.size() > 4);
        const auto pushed = DistortTool::distort(sq, DistortTool::Mode::PushPull, 50, 10, QPointF(10, 10));
        QVERIFY(std::abs(pushed.first().nodes.first().point.x() - (-5.0)) < 1e-9);

        // Hülle: oberen Mittelgriff anheben -> Kante oben gewölbt, Ecken bleiben; das Rechteck
        // bleibt ein Rechteck (die Hülle ist ein Effekt)
        const NodeId h = add(std::make_unique<RectangleObject>(QSizeF(40, 20)), QRectF(20, 120, 40, 20), Color(0, 0, 0));
        m_editor->setSelection({h});
        m_canvas->setTool(ToolId::Envelope);
        auto* env = static_cast<EnvelopeTool*>(m_canvas->currentTool());
        QVERIFY(env->active());
        env->moveHandle(1, QPointF(40, 110));
        const auto* shaped = node_cast<RectangleObject>(obj(h));
        QVERIFY(shaped && shaped->hasEnvelope());
        QVERIFY(std::abs(shaped->boundingRect().top() - 110.0) < 0.6);
        QVERIFY(std::abs(shaped->boundingRect().bottom() - 140.0) < 0.01);
        m_canvas->setTool(ToolId::Pick);
    }

    // Video "Use Envelope": Hülle auf Text (Vorlage), Text bleibt bearbeitbar; Hülle aus Objekt
    void envelopeKeepsTextEditable()
    {
        m_canvas->resize(800, 600);
        auto text = std::make_unique<TextObject>(TextObject::Kind::Artistic);
        text->setPlainText(QStringLiteral("HEMA"), CharFormat(QStringLiteral("Arial"), 72));
        text->setTransform(QTransform::fromTranslate(30, 60));
        const NodeId id = m_editor->addObject(std::move(text), QStringLiteral("T"));
        auto textObj = [this, id] { return m_editor->document().findAs<TextObject>(id); };
        const QRectF before = textObj()->path().boundingRect();
        const QList<std::pair<QString, QPainterPath>> presets = EnvelopeTool::presets();
        int waist = -1;
        for (int i = 0; i < presets.size(); ++i)
            if (presets[i].first == QStringLiteral("Taille"))
                waist = i;
        QVERIFY(waist >= 0);
        m_editor->setSelection({id});
        QVERIFY(m_editor->applyEnvelopeShape(presets[waist].second, false, Envelope::Mode::Unconstrained, false));
        QVERIFY(textObj() && textObj()->hasEnvelope());
        const QRectF shaped = textObj()->path().boundingRect();
        QVERIFY2(near(shaped, before, 0.6), qPrintable(describe(shaped) + describe(before)));
        // Taille: die Mitte der Oberkante liegt tiefer als die Ecken
        const Envelope env = textObj()->envelopes().last();
        const QRectF box = env.box;
        QVERIFY(env.map(QPointF(box.center().x(), box.top())).y() > box.top() + 0.15 * box.height());
        QVERIFY(QLineF(env.map(box.topLeft()), box.topLeft()).length() < 0.05);
        screenshot(QStringLiteral("60-huelle-text"));
        // Text bearbeiten: Hülle bleibt
        m_editor->modifyText(id, [](TextObject& t) { t.setPlainText(QStringLiteral("HEMA"), CharFormat(QStringLiteral("Arial"), 72, true)); },
                             QStringLiteral("Fett"));
        QVERIFY(textObj()->hasEnvelope());
        // Speichern und Laden
        auto back = serial::fromJson(serial::toJson(*textObj()), serial::Ids::Keep);
        QCOMPARE(node_cast<DrawObject>(back.get())->envelopes(), textObj()->envelopes());
        // In Kurven: Hülle wird dauerhaft
        const QRectF bold = textObj()->path().boundingRect();
        m_editor->convertSelectionToCurves();
        const auto* curves = m_editor->document().findAs<DrawObject>(m_editor->selection().value(0));
        QVERIFY(curves && !node_cast<TextObject>(curves) && !curves->hasEnvelope());
        QVERIFY2(near(curves->boundingRect(), bold, 0.3), qPrintable(describe(curves->boundingRect()) + describe(bold)));
        m_editor->document().undo();
        QVERIFY(textObj() && textObj()->hasEnvelope());

        // Hülle aus Objekt: Rechteck in Ellipsenform
        auto rect = std::make_unique<RectangleObject>(QSizeF(40, 20));
        rect->setTransform(QTransform::fromTranslate(100, 150));
        const NodeId r = m_editor->addObject(std::move(rect), QStringLiteral("R"));
        auto ellipse = std::make_unique<EllipseObject>(QSizeF(40, 20));
        ellipse->setTransform(QTransform::fromTranslate(160, 150));
        const NodeId e = m_editor->addObject(std::move(ellipse), QStringLiteral("E"));
        m_editor->setSelection({r});
        QVERIFY(m_editor->applyEnvelopeShape(m_editor->document().findAs<DrawObject>(e)->path(), true, EnvelopeTool::mode(), false));
        const auto* shapedRect = m_editor->document().findAs<RectangleObject>(r);
        QVERIFY(shapedRect && shapedRect->hasEnvelope());
        QVERIFY(!shapedRect->path().contains(QPointF(100.6, 150.6))); // Ecke abgerundet
        QVERIFY(shapedRect->path().contains(QPointF(120, 160)));
        QVERIFY(near(shapedRect->boundingRect(), QRectF(100, 150, 40, 20), 0.3));
        // Verschieben und Skalieren behalten die Form
        m_editor->transformSelection(xform::scaling(2, 1, QPointF(100, 150)), QStringLiteral("S"));
        QVERIFY2(near(m_editor->document().findAs<DrawObject>(r)->boundingRect(), QRectF(100, 150, 80, 20), 0.3),
                 qPrintable(describe(m_editor->document().findAs<DrawObject>(r)->boundingRect())));
        m_editor->setSelectionEnvelopeMode(Envelope::Mode::Straight);
        QCOMPARE(m_editor->document().findAs<DrawObject>(r)->envelopes().last().mode, Envelope::Mode::Straight);
        // Neue Hülle darüber, dann löschen
        QVERIFY(m_editor->applyEnvelopeShape({}, false, Envelope::Mode::Straight, true));
        QCOMPARE(m_editor->document().findAs<DrawObject>(r)->envelopes().size(), 2);
        m_editor->clearSelectionEnvelopes();
        QVERIFY(!m_editor->document().findAs<DrawObject>(r)->hasEnvelope());

        // Andockfenster: Vorlage anwenden schaltet auf das Hüllenwerkzeug
        EnvelopeDocker docker(*m_editor, *m_canvas);
        docker.presetList()->setCurrentRow(0);
        QVERIFY(docker.apply());
        QVERIFY(m_editor->document().findAs<DrawObject>(r)->hasEnvelope());
        QCOMPARE(m_canvas->toolId(), ToolId::Envelope);
        screenshot(QStringLiteral("61-huelle-werkzeug"));
        m_canvas->setTool(ToolId::Pick);
    }

    // Video "SuperClip inside": schwebende Leiste, Inhalt im Rahmen bearbeiten, Fertig
    void superClipEditing()
    {
        m_canvas->resize(800, 600);
        auto add = [this](const QRectF& r, const Color& fill) {
            auto o = std::make_unique<RectangleObject>(r.size());
            o->setTransform(QTransform::fromTranslate(r.left(), r.top()));
            Style s;
            s.fill = Fill::solid(fill);
            o->setStyle(s);
            return m_editor->addObject(std::move(o), QStringLiteral("R"));
        };
        auto obj = [this](NodeId id) { return m_editor->document().findAs<DrawObject>(id); };
        const NodeId frame = add(QRectF(40, 40, 40, 40), Color(255, 255, 255));
        const NodeId content = add(QRectF(20, 20, 100, 80), Color(0, 0, 255));
        m_editor->setSelection({content});
        const NodeId clip = m_editor->placeInsideFrame(frame);
        m_canvas->setTool(ToolId::Pick);
        m_canvas->update();
        QTRY_VERIFY(m_canvas->superClipBar()->isVisible());
        QVERIFY(m_canvas->superClipBar()->geometry().top() > m_canvas->toScreen(QPointF(0, 80)).y());
        screenshot(QStringLiteral("62-superclip-leiste"));

        // Bearbeiten: nur noch die Inhalte sind wählbar, Ziehen verschiebt den Inhalt allein
        QVERIFY(m_editor->selectSuperClipContents());
        QCOMPARE(m_editor->editedSuperClip(), clip);
        QCOMPARE(m_editor->selection(), QList<NodeId>{content});
        QCOMPARE(m_editor->pickableObjects().size(), 1);
        drag(screen(QPointF(60, 60)), screen(QPointF(70, 60)));
        QVERIFY2(std::abs(obj(content)->boundingRect().left() - 30.0) < 0.3, qPrintable(describe(obj(content)->boundingRect())));
        QVERIFY(near(obj(clip)->boundingRect(), QRectF(40, 40, 40, 40)));
        screenshot(QStringLiteral("63-superclip-bearbeiten"));
        // Esc = Fertig
        QTest::keyClick(m_canvas.get(), Qt::Key_Escape);
        QCOMPARE(m_editor->editedSuperClip(), InvalidNodeId);
        QCOMPARE(m_editor->selection(), QList<NodeId>{clip});
        // Rückgängig bis vor den SuperClip beendet eine laufende Bearbeitung
        QVERIFY(m_editor->beginSuperClipEdit(clip));
        m_editor->document().undo();
        m_editor->document().undo();
        QVERIFY(!obj(clip));
        QCOMPARE(m_editor->editedSuperClip(), InvalidNodeId);
        QCOMPARE(m_editor->pickableObjects().size(), 2);
    }

    // Video "Football": Linse Fischauge, Fixiert; dazu Blickpunkt, Drahtmodell, Farblinsen
    void lensFishEyeAndFrozen()
    {
        m_canvas->resize(800, 600);
        auto add = [this](std::unique_ptr<DrawObject> o, const QRectF& r, std::optional<Color> fill) {
            o->setTransform(QTransform::fromTranslate(r.left(), r.top()));
            Style s;
            s.fill = fill ? Fill::solid(*fill) : Fill::none();
            s.stroke = Color(0, 0, 0);
            s.strokeWidth = 0.2;
            o->setStyle(s);
            return m_editor->addObject(std::move(o), QStringLiteral("O"));
        };
        auto pixel = [this](const QPointF& doc) {
            QImage img(3, 3, QImage::Format_ARGB32_Premultiplied);
            img.fill(Qt::white);
            QPainter p(&img);
            m_canvas->renderPage(p, 0, 8.0, QPointF(1.5, 1.5) - doc * 8.0, true);
            p.end();
            return img.pixelColor(1, 1);
        };
        // Kleines schwarzes Quadrat in der Mitte unter einem Kreis
        add(std::make_unique<RectangleObject>(QSizeF(4, 4)), QRectF(128, 128, 4, 4), Color(0, 0, 0));
        const NodeId ball = add(std::make_unique<EllipseObject>(QSizeF(40, 40)), QRectF(110, 110, 40, 40), std::nullopt);
        QCOMPARE(pixel(QPointF(133.5, 130)), QColor(Qt::white));
        m_editor->setSelection({ball});
        LensDocker docker(*m_editor);
        docker.setLensType(Lens::Type::FishEye);
        docker.amountBox()->setValue(150);
        docker.apply();
        QCOMPARE(m_editor->document().findAs<DrawObject>(ball)->lens().type, Lens::Type::FishEye);
        // Fischauge wölbt die Mitte: das Quadrat erscheint grösser
        QCOMPARE(pixel(QPointF(133.5, 130)), QColor(Qt::black));
        screenshot(QStringLiteral("64-fischauge"));
        // Fixiert: Linse wird zu Objekten (SuperClip in Kreisform), die man wegschieben kann
        docker.frozenBox()->setChecked(true);
        const NodeId frozen = m_editor->selection().value(0);
        const auto* clip = m_editor->document().findAs<SuperClipObject>(frozen);
        QVERIFY(clip && !m_editor->document().find(ball));
        QVERIFY(!clip->contents().isEmpty());
        QCOMPARE(pixel(QPointF(133.5, 130)), QColor(Qt::black));
        m_editor->transformSelection(QTransform::fromTranslate(80, 0), QStringLiteral("M"));
        QCOMPARE(pixel(QPointF(213.5, 130)), QColor(Qt::black)); // gewölbtes Quadrat wandert mit
        QCOMPARE(pixel(QPointF(133.5, 130)), QColor(Qt::white)); // Original darunter unverändert

        // Vergrössern mit Blickpunkt: die Linse zeigt ein rotes Quadrat von anderswo
        add(std::make_unique<RectangleObject>(QSizeF(4, 4)), QRectF(103, 208, 4, 4), Color(255, 0, 0));
        const NodeId glass = add(std::make_unique<RectangleObject>(QSizeF(20, 20)), QRectF(40, 200, 20, 20), std::nullopt);
        m_editor->setSelection({glass});
        Lens magnify;
        magnify.type = Lens::Type::Magnify;
        magnify.amount = 20; // Faktor 2
        magnify.viewpoint = true;
        magnify.viewOffset = QPointF(55, 0); // Mitte (50,210) -> (105,210)
        m_editor->setSelectionLens(magnify);
        QCOMPARE(pixel(QPointF(50, 210)), QColor(255, 0, 0));
        QCOMPARE(pixel(QPointF(43, 210)), QColor(Qt::white)); // ausserhalb von 2 x 2 mm
        // Drahtmodell: Flächen darunter in der Füllfarbe der Linse
        add(std::make_unique<RectangleObject>(QSizeF(30, 30)), QRectF(200, 200, 30, 30), Color(0, 0, 255));
        const NodeId wire = add(std::make_unique<RectangleObject>(QSizeF(20, 20)), QRectF(205, 205, 20, 20), std::nullopt);
        m_editor->setSelection({wire});
        Lens w;
        w.type = Lens::Type::Wireframe;
        w.color = Color(0, 0, 0);
        w.color2 = Color(255, 255, 0);
        m_editor->setSelectionLens(w);
        QCOMPARE(pixel(QPointF(215, 215)), QColor(255, 255, 0));
        // Farblinsen im Kern
        Lens limit;
        limit.type = Lens::Type::ColorLimit;
        limit.amount = 100;
        limit.color = Color(255, 0, 0);
        QCOMPARE(limit.mapColor(Color(255, 255, 255)), Color(255, 0, 0));
        Lens map;
        map.type = Lens::Type::CustomColorMap;
        map.color = Color(0, 0, 0);
        map.color2 = Color(0, 255, 0);
        QCOMPARE(map.mapColor(Color(255, 255, 255)), Color(0, 255, 0));
        Lens heat;
        heat.type = Lens::Type::HeatMap;
        heat.amount = 0;
        QCOMPARE(heat.mapColor(Color(255, 255, 255)), Color(255, 0, 0));
        auto json = Lens::fromJson(magnify.toJson());
        QCOMPARE(json, magnify);
    }

    // Video "Travel flyer": B-Spline, Virtuelles Segment löschen, Rechts ziehen (SuperClip innen)
    void travelFlyerTools()
    {
        m_canvas->resize(800, 600);
        // B-Spline: Klicks setzen Steuerpunkte, Eingabe beendet; Anfang und Ende an den Endpunkten
        m_canvas->setTool(ToolId::BSpline);
        for (const QPointF& p : {QPointF(20, 20), QPointF(40, 60), QPointF(60, 20), QPointF(80, 60)})
            click(screen(p));
        QTest::keyClick(m_canvas.get(), Qt::Key_Return);
        const auto* spline = m_editor->document().findAs<CurveObject>(m_editor->selection().value(0));
        QVERIFY(spline);
        const SubPath& sp = spline->subPaths().first();
        QVERIFY(!sp.closed && sp.nodes.size() >= 3);
        QVERIFY(QLineF(spline->transform().map(sp.nodes.first().point), QPointF(20, 20)).length() < 0.6);
        QVERIFY(QLineF(spline->transform().map(sp.nodes.last().point), QPointF(80, 60)).length() < 0.6);
        QVERIFY(sp.nodes.last().curveIn);
        // Glatt: die Kurve geht nicht durch die inneren Steuerpunkte
        double closest = 1e9;
        const QPainterPath splinePath = spline->path();
        for (int i = 0; i <= 200; ++i)
            closest = std::min(closest, QLineF(splinePath.pointAtPercent(i / 200.0), QPointF(40, 60)).length());
        QVERIFY2(closest > 2.0, qPrintable(QString::number(closest)));
        // Klick auf den ersten Punkt schliesst
        for (const QPointF& p : {QPointF(120, 20), QPointF(160, 20), QPointF(140, 60), QPointF(120, 20)})
            click(screen(p));
        const auto* closedSpline = m_editor->document().findAs<CurveObject>(m_editor->selection().value(0));
        QVERIFY(closedSpline && closedSpline != spline && closedSpline->subPaths().first().closed);

        // Virtuelles Segment löschen: Kreuz aus zwei Linien, rechter Arm verschwindet bis zum Schnitt
        auto line = [this](const QPointF& a, const QPointF& b) {
            SubPath s;
            s.nodes << CurveNode{a} << CurveNode{b};
            auto c = std::make_unique<CurveObject>(QList<SubPath>{s});
            return m_editor->addObject(std::move(c), QStringLiteral("L"));
        };
        const NodeId horizontal = line(QPointF(20, 100), QPointF(120, 100));
        const NodeId vertical = line(QPointF(70, 80), QPointF(70, 130));
        m_canvas->setTool(ToolId::VirtualDelete);
        click(screen(QPointF(95, 100)));
        auto bbox = [this](NodeId id) {
            const DrawObject* o = m_editor->document().findAs<DrawObject>(id);
            return o ? o->boundingRect() : QRectF();
        };
        // Das ersetzte Objekt ist neu: das übrig gebliebene Stück endet am Schnittpunkt
        const QList<const DrawObject*> all = m_editor->pickableObjects();
        bool found = false;
        for (const DrawObject* o : all)
            if (std::abs(o->boundingRect().top() - 100.0) < 0.01 && std::abs(o->boundingRect().height()) < 0.01)
                found = found || (std::abs(o->boundingRect().left() - 20.0) < 0.05 && std::abs(o->boundingRect().right() - 70.0) < 0.05);
        QVERIFY(found);
        QVERIFY(near(bbox(vertical), QRectF(70, 80, 0, 50)));
        // Rahmen: unteres Stück der senkrechten Linie liegt ganz darin
        drag(screen(QPointF(60, 98)), screen(QPointF(80, 135)));
        bool shortened = false;
        for (const DrawObject* o : m_editor->pickableObjects())
            if (std::abs(o->boundingRect().left() - 70.0) < 0.01 && std::abs(o->boundingRect().width()) < 0.01)
                shortened = std::abs(o->boundingRect().bottom() - 100.0) < 0.05 && std::abs(o->boundingRect().top() - 80.0) < 0.05;
        QVERIFY(shortened);
        // Rechteck mit Linie darüber: Klick auf die Oberkante rechts der Linie löscht die rechte Hälfte
        // bis zu beiden Schnittpunkten (oben und unten); übrig bleibt die linke Hälfte, offen
        auto box = std::make_unique<RectangleObject>(QSizeF(40, 30));
        box->setTransform(QTransform::fromTranslate(20, 160));
        const NodeId boxId = m_editor->addObject(std::move(box), QStringLiteral("R"));
        line(QPointF(40, 150), QPointF(40, 200));
        m_canvas->setTool(ToolId::VirtualDelete);
        click(screen(QPointF(50, 160)));
        QVERIFY(!m_editor->document().find(boxId));
        bool opened = false;
        for (const DrawObject* o : m_editor->pickableObjects())
            if (const auto* c = node_cast<CurveObject>(o); c && near(c->boundingRect(), QRectF(20, 160, 20, 30), 0.05))
                opened = c->subPaths().size() == 1 && !c->subPaths().first().closed;
        QVERIFY(opened);
        Q_UNUSED(horizontal);

        // Rechts ziehen: Füllung hierher kopieren, Hierher kopieren, SuperClip innen
        m_canvas->setTool(ToolId::Pick);
        auto rect = [this](const QRectF& r, const Color& fill) {
            auto o = std::make_unique<RectangleObject>(r.size());
            o->setTransform(QTransform::fromTranslate(r.left(), r.top()));
            Style s;
            s.fill = Fill::solid(fill);
            o->setStyle(s);
            return m_editor->addObject(std::move(o), QStringLiteral("R"));
        };
        const NodeId a = rect(QRectF(200, 200, 20, 20), Color(255, 0, 0));
        const NodeId b = rect(QRectF(260, 200, 30, 30), Color(0, 0, 255));
        m_editor->setSelection({a});
        PickTool& pick = m_canvas->pickTool();
        pick.rightDrop(PickTool::DropAction::CopyFill, b, QTransform::fromTranslate(60, 0));
        QCOMPARE(m_editor->document().findAs<DrawObject>(b)->style().fill.color, Color(255, 0, 0));
        QCOMPARE(m_editor->document().findAs<DrawObject>(a)->boundingRect().left(), 200.0); // Quelle bleibt
        // Mit der Maus: rechts ziehen und im Menü "Hierher kopieren" wählen
        QTimer poll;
        poll.setInterval(20);
        QStringList texts;
        QObject::connect(&poll, &QTimer::timeout, [&] {
            auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            if (!menu || !menu->isVisible())
                return;
            for (QAction* act : menu->actions()) {
                texts << act->text();
                if (act->text() == QStringLiteral("Hierher &kopieren"))
                    menu->setActiveAction(act);
            }
            if (!qEnvironmentVariable("VEK_SCREENSHOT_DIR").isEmpty())
                menu->grab().save(qEnvironmentVariable("VEK_SCREENSHOT_DIR") + QStringLiteral("/65-rechts-ziehen.png"));
            QTest::keyClick(menu, Qt::Key_Return);
            poll.stop();
        });
        poll.start();
        const int before = int(m_editor->pickableObjects().size());
        QTest::mousePress(m_canvas.get(), Qt::RightButton, {}, screen(QPointF(210, 210)), 1000);
        for (int i = 1; i <= 4; ++i)
            QTest::mouseMove(m_canvas.get(), screen(QPointF(210, 210 + 10 * i)));
        QTest::mouseRelease(m_canvas.get(), Qt::RightButton, {}, screen(QPointF(210, 250)), 10);
        QVERIFY2(texts.contains(QStringLiteral("&SuperClip innen")), qPrintable(texts.join(QLatin1Char('|'))));
        QCOMPARE(int(m_editor->pickableObjects().size()), before + 1);
        QVERIFY(near(m_editor->selectionBounds(), QRectF(200, 240, 20, 20), 0.6));
        // SuperClip innen: die Kopie kommt in das blaue Quadrat
        const NodeId copy = m_editor->selection().value(0);
        pick.rightDrop(PickTool::DropAction::SuperClipInside, b, QTransform());
        const auto* clip = m_editor->document().findAs<SuperClipObject>(m_editor->selection().value(0));
        QVERIFY(clip && clip->contents().size() == 1 && clip->contents().first()->id() == copy);
    }

    // Video "Hinterlegter Schatten": Regler zwischen Anfangs- und Endpunkt (Schatten = Deckung);
    // dasselbe als Mittelpunkt bei Transparenz, Unschärfe und Farbverlauf
    void slidersBetweenHandles()
    {
        m_canvas->resize(800, 600);
        m_editor->snapSettings().objects = false;
        m_editor->snapSettings().page = false;
        auto add = [this](const QRectF& r) {
            auto o = std::make_unique<RectangleObject>(r.size());
            o->setTransform(QTransform::fromTranslate(r.left(), r.top()));
            Style s;
            s.fill = Fill::solid(Color(200, 200, 200));
            o->setStyle(s);
            return m_editor->addObject(std::move(o), QStringLiteral("R"));
        };
        auto obj = [this](NodeId id) { return m_editor->document().findAs<DrawObject>(id); };
        // Schatten: Regler von 50 % auf 80 % der Linie ziehen
        const NodeId shadowed = add(QRectF(20, 20, 40, 30));
        DropShadow sh;
        sh.active = true;
        sh.offset = QPointF(10, 20.0 / 3.0); // Endgriff 3-fach weiter: (30, 20) von der Mitte
        sh.opacity = 50;
        m_editor->setObjectShadow(shadowed, sh, QStringLiteral("S"));
        m_editor->setSelection({shadowed});
        m_canvas->setTool(ToolId::DropShadow);
        const QPointF c(40, 35);
        drag(screen(c + QPointF(15, 10)), screen(c + QPointF(24, 16)), {}, QStringLiteral("66-schatten-regler"));
        QVERIFY2(std::abs(obj(shadowed)->shadow().opacity - 80.0) < 3.0, qPrintable(QString::number(obj(shadowed)->shadow().opacity)));
        QCOMPARE(obj(shadowed)->shadow().offset, QPointF(10, 20.0 / 3.0)); // Versatz unverändert

        // Transparenz: linear von links (0 %) nach rechts (100 %), Mittelpunkt auf ein Viertel
        const NodeId faded = add(QRectF(20, 100, 80, 30));
        Ramp ramp;
        ramp.type = Ramp::Type::Linear;
        ramp.from = 0;
        ramp.to = 100;
        ramp.start = QPointF(0, 0.5);
        ramp.end = QPointF(1, 0.5);
        m_editor->setObjectEffect(faded, Effect::Transparency, ramp, QStringLiteral("T"));
        m_editor->setSelection({faded});
        m_canvas->setTool(ToolId::Transparency);
        drag(screen(QPointF(60, 115)), screen(QPointF(40, 115)), {}, QStringLiteral("67-transparenz-mittelpunkt"));
        const Ramp moved = obj(faded)->transparency();
        QVERIFY2(std::abs(moved.midpoint - 0.25) < 0.03, qPrintable(QString::number(moved.midpoint)));
        const QRectF local(0, 0, 80, 30);
        QVERIFY(std::abs(moved.valueAt(QPointF(80 * moved.midpoint, 15), local) - 50.0) < 1e-6);
        QVERIFY(std::abs(moved.valueAt(QPointF(80, 15), local) - 100.0) < 1e-6);
        QCOMPARE(Ramp::fromJson(moved.toJson()), moved);

        // Unschärfe: derselbe Regler
        m_editor->setObjectEffect(faded, Effect::Blur, [] {
            Ramp b;
            b.type = Ramp::Type::Radial;
            b.from = 0;
            b.to = 2;
            b.start = QPointF(0.5, 0.5);
            b.end = QPointF(1, 0.5);
            return b;
        }(), QStringLiteral("B"));
        m_canvas->setTool(ToolId::Blur);
        // Radius von der Mitte (60) zum rechten Rand (100): Regler bei 80 -> auf 90 ziehen
        drag(screen(QPointF(80, 115)), screen(QPointF(90, 115)));
        QVERIFY2(std::abs(obj(faded)->blurRamp().midpoint - 0.75) < 0.03, qPrintable(QString::number(obj(faded)->blurRamp().midpoint)));

        // Farbverlauf: zwei Farben, Mittelpunkt wird zum Mittelstopp wie im Verlaufsdialog
        const NodeId filled = add(QRectF(20, 160, 80, 30));
        m_editor->setObjectFill(filled, Fill::gradient(Fill::Type::Linear, Color(0, 0, 0), Color(255, 255, 255)), QStringLiteral("F"));
        m_editor->setSelection({filled});
        m_canvas->setTool(ToolId::InteractiveFill);
        drag(screen(QPointF(60, 175)), screen(QPointF(80, 175)), {}, QStringLiteral("68-verlauf-mittelpunkt"));
        const Fill f = obj(filled)->style().fill;
        QCOMPARE(f.stops.size(), 3);
        QVERIFY2(std::abs(f.stops[1].position - 0.75) < 0.03, qPrintable(QString::number(f.stops[1].position)));
        QCOMPARE(InteractiveFillTool::midpoint(f).has_value(), true);
        m_canvas->setTool(ToolId::Pick);
    }

    // Kurs "alle Werkzeuge": Konturstift (F12)
    void outlinePenDialog()
    {
        m_canvas->resize(800, 600);
        auto pixel = [this](const QPointF& doc) {
            QImage img(3, 3, QImage::Format_ARGB32_Premultiplied);
            img.fill(Qt::white);
            QPainter p(&img);
            m_canvas->renderPage(p, 0, 8.0, QPointF(1.5, 1.5) - doc * 8.0, true);
            p.end();
            return img.pixelColor(1, 1);
        };
        auto rect = std::make_unique<RectangleObject>(QSizeF(40, 30));
        rect->setTransform(QTransform::fromTranslate(20, 20));
        Style s;
        s.fill = Fill::solid(Color(255, 255, 0));
        s.stroke = Color(0, 0, 255);
        s.strokeWidth = 4.0;
        rect->setStyle(s);
        const NodeId id = m_editor->addObject(std::move(rect), QStringLiteral("R"));
        auto obj = [this, id] { return m_editor->document().findAs<DrawObject>(id); };
        // Mittig: je 2 mm innen und aussen
        QCOMPARE(pixel(QPointF(18.5, 35)), QColor(0, 0, 255));
        QCOMPARE(pixel(QPointF(21.5, 35)), QColor(0, 0, 255));

        // Dialog: innen, runde Ecken, eckige Enden, hinter Füllung, mit Objekt skalieren
        m_editor->setSelection({id});
        OutlinePenDialog dialog(obj()->style());
        dialog.positionGroup()->button(1)->click();
        dialog.joinGroup()->button(1)->click();
        dialog.capGroup()->button(2)->click();
        dialog.scaleBox()->setChecked(true);
        const Style pen = dialog.style();
        QCOMPARE(pen.position, Style::Position::Inside);
        QCOMPARE(pen.join, Style::Join::Round);
        QCOMPARE(pen.cap, Style::Cap::Square);
        m_editor->setSelectionOutlinePen(pen);
        QCOMPARE(obj()->style().fill, Fill::solid(Color(255, 255, 0))); // Füllung bleibt
        QCOMPARE(pixel(QPointF(18.5, 35)), QColor(Qt::white));    // aussen nichts mehr
        QCOMPARE(pixel(QPointF(23.0, 35)), QColor(0, 0, 255));    // innen 4 mm
        QCOMPARE(pixel(QPointF(25.0, 35)), QColor(255, 255, 0));
        // Aussen
        Style outside = obj()->style();
        outside.position = Style::Position::Outside;
        m_editor->setSelectionOutlinePen(outside);
        QCOMPARE(pixel(QPointF(17.0, 35)), QColor(0, 0, 255));
        QCOMPARE(pixel(QPointF(21.0, 35)), QColor(255, 255, 0));
        // Hinter Füllung (mittig): die innere Hälfte verdeckt die Füllung
        Style behind = obj()->style();
        behind.position = Style::Position::Center;
        behind.behindFill = true;
        m_editor->setSelectionOutlinePen(behind);
        QCOMPARE(pixel(QPointF(18.5, 35)), QColor(0, 0, 255));
        QCOMPARE(pixel(QPointF(21.0, 35)), QColor(255, 255, 0));
        // Mit Objekt skalieren
        m_editor->transformSelection(xform::scaling(2, 2, QPointF(20, 20)), QStringLiteral("S"));
        QVERIFY(std::abs(obj()->style().strokeWidth - 8.0) < 1e-9);
        Style fixed = obj()->style();
        fixed.scaleWithObject = false;
        m_editor->setSelectionOutlinePen(fixed);
        m_editor->transformSelection(xform::scaling(0.5, 0.5, QPointF(20, 20)), QStringLiteral("S"));
        QVERIFY(std::abs(obj()->style().strokeWidth - 8.0) < 1e-9);
        // Speichern/Laden
        auto back = serial::fromJson(serial::toJson(*obj()), serial::Ids::Keep);
        QCOMPARE(node_cast<DrawObject>(back.get())->style(), obj()->style());

        // Kalligrafie: waagrechte Linie mit flacher Feder (20 %, 0°) ist nur 0,8 mm dick
        SubPath sp;
        sp.nodes << CurveNode{QPointF(100, 100)} << CurveNode{QPointF(160, 100)};
        auto line = std::make_unique<CurveObject>(QList<SubPath>{sp});
        Style ls;
        ls.stroke = Color(0, 0, 0);
        ls.strokeWidth = 4.0;
        ls.nibStretch = 20.0;
        line->setStyle(ls);
        m_editor->addObject(std::move(line), QStringLiteral("L"));
        QCOMPARE(pixel(QPointF(130, 100)), QColor(Qt::black));
        QCOMPARE(pixel(QPointF(130, 101.2)), QColor(Qt::white));
        // um 90° gedreht: volle Breite
        Style turned = ls;
        turned.nibAngle = 90;
        m_editor->setSelectionOutlinePen(turned);
        QCOMPARE(pixel(QPointF(130, 101.2)), QColor(Qt::black));
        if (!qEnvironmentVariable("VEK_SCREENSHOT_DIR").isEmpty()) {
            dialog.show();
            dialog.grab().save(qEnvironmentVariable("VEK_SCREENSHOT_DIR") + QStringLiteral("/69-konturstift.png"));
        }
    }

    // Kurs "alle Werkzeuge": Stift, Polylinie, 3-Punkt-Kurve, -Rechteck, -Ellipse
    void threePointAndPolylineTools()
    {
        m_canvas->resize(800, 600);
        m_editor->snapSettings().objects = false;
        m_editor->snapSettings().page = false;
        m_editor->snapSettings().guides = false;
        auto sel = [this] { return m_editor->document().findAs<DrawObject>(m_editor->selection().value(0)); };
        // 3-Punkt-Rechteck: Grundlinie 45°, Höhe 10 mm
        m_canvas->setTool(ToolId::ThreePointRectangle);
        drag(screen(QPointF(20, 60)), screen(QPointF(50, 30)));
        move(screen(QPointF(30, 70)));
        click(screen(QPointF(30, 70)));
        const auto* r = node_cast<RectangleObject>(sel());
        QVERIFY(r);
        QVERIFY2(std::abs(r->localPath().boundingRect().width() - std::hypot(30, 30)) < 0.6,
                 qPrintable(describe(r->localPath().boundingRect())));
        QVERIFY(std::abs(r->localPath().boundingRect().height() - 10.0 * std::sqrt(2.0)) < 0.6);
        QVERIFY(std::abs(std::abs(r->rotationAngle()) - 45.0) < 1.0);
        // 3-Punkt-Ellipse: waagrechte Achse 40 mm, halbe Höhe 10 mm
        m_canvas->setTool(ToolId::ThreePointEllipse);
        drag(screen(QPointF(100, 50)), screen(QPointF(140, 50)));
        click(screen(QPointF(120, 60)));
        const auto* e = node_cast<EllipseObject>(sel());
        QVERIFY(e);
        QVERIFY2(near(e->boundingRect(), QRectF(100, 40, 40, 20), 0.6), qPrintable(describe(e->boundingRect())));
        // 3-Punkt-Kurve geht durch den dritten Punkt
        m_canvas->setTool(ToolId::ThreePointCurve);
        drag(screen(QPointF(20, 120)), screen(QPointF(80, 120)));
        click(screen(QPointF(50, 100)));
        const auto* c = node_cast<CurveObject>(sel());
        QVERIFY(c && c->subPaths().first().nodes.last().curveIn);
        QVERIFY2(std::abs(c->boundingRect().top() - 100.0) < 0.6, qPrintable(describe(c->boundingRect())));
        // Polylinie: zwei Klicks gerade, dann frei ziehen, Eingabe
        m_canvas->setTool(ToolId::Polyline);
        click(screen(QPointF(20, 160)));
        click(screen(QPointF(60, 160)));
        press(screen(QPointF(60, 170)));
        for (int i = 1; i <= 8; ++i)
            move(screen(QPointF(60 + 2 * i, 170 + (i % 2) * 3)));
        release(screen(QPointF(76, 170)));
        QTest::keyClick(m_canvas.get(), Qt::Key_Return);
        const auto* pl = node_cast<CurveObject>(sel());
        QVERIFY(pl && pl->subPaths().first().nodes.size() >= 6 && !pl->subPaths().first().closed);
        // Stift: wie Bézier, Klick auf den Anfang schliesst
        m_canvas->setTool(ToolId::Pen);
        click(screen(QPointF(120, 150)));
        click(screen(QPointF(160, 150)));
        click(screen(QPointF(140, 180)));
        click(screen(QPointF(120, 150)));
        const auto* pen = node_cast<CurveObject>(sel());
        QVERIFY(pen && pen->subPaths().first().closed && pen->subPaths().first().nodes.size() == 3);
        m_canvas->setTool(ToolId::Pick);
    }

    // Kurs "alle Werkzeuge": Formgebung (Verschmieren, Aufrauen, Wirbel, Anziehen, Abstossen, Freie Transformation)
    void shapingBrushTools()
    {
        m_canvas->resize(800, 600);
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        Options::instance().resetWorkspace();
        auto addRect = [this](const QRectF& r) {
            auto o = std::make_unique<RectangleObject>(r.size());
            o->setTransform(QTransform::fromTranslate(r.left(), r.top()));
            Style s;
            s.fill = Fill::solid(Color(255, 0, 0));
            o->setStyle(s);
            return m_editor->addObject(std::move(o), QStringLiteral("R"));
        };
        auto sel = [this] { return m_editor->document().findAs<DrawObject>(m_editor->selection().value(0)); };
        // Verschmieren: von der rechten Kante nach aussen ziehen -> Ausbuchtung nach rechts
        const NodeId rect = addRect(QRectF(20, 20, 40, 30));
        m_canvas->setTool(ToolId::Smear);
        drag(screen(QPointF(60, 35)), screen(QPointF(75, 35)));
        const auto* smeared = node_cast<CurveObject>(sel());
        QVERIFY(smeared);
        QVERIFY2(smeared->boundingRect().right() > 65.0, qPrintable(describe(smeared->boundingRect())));
        QVERIFY(std::abs(smeared->boundingRect().left() - 20.0) < 0.2); // links unberührt
        QCOMPARE(smeared->style().fill, Fill::solid(Color(255, 0, 0)));
        m_editor->document().undo();
        m_editor->setSelection({rect});
        QVERIFY(node_cast<RectangleObject>(sel()));

        // Aufrauen: Kante wird zackig (viele Knoten, aus dem Rechteck heraus)
        m_canvas->setTool(ToolId::Roughen);
        drag(screen(QPointF(40, 20)), screen(QPointF(50, 20)));
        const auto* rough = node_cast<CurveObject>(sel());
        QVERIFY(rough && rough->nodeCount() > 12);
        QVERIFY(rough->boundingRect().top() < 19.5);
        m_editor->document().undo();
        m_editor->setSelection({rect});

        // Wirbel, Anziehen, Abstossen über die Werkzeugschritte (ohne Zeitgeber)
        auto* tool = [this](ToolId id) {
            m_canvas->setTool(id);
            return static_cast<BrushShapingTool*>(m_canvas->currentTool());
        }(ToolId::Repel);
        tool->step(QPointF(40, 22), QPointF(40, 22));
        tool->commit();
        QVERIFY(sel()->boundingRect().top() < 20.0 - 0.1); // Rand nahe der Spitze weggedrückt
        m_editor->document().undo();
        m_editor->setSelection({rect});
        tool = [this] {
            m_canvas->setTool(ToolId::Attract);
            return static_cast<BrushShapingTool*>(m_canvas->currentTool());
        }();
        for (int i = 0; i < 5; ++i)
            tool->step(QPointF(40, 25), QPointF(40, 25));
        tool->commit();
        QVERIFY(!sel()->path().contains(QPointF(40, 20.5))); // Rand zur Spitze hin eingezogen
        m_editor->document().undo();
        m_editor->setSelection({rect});
        tool = [this] {
            m_canvas->setTool(ToolId::Twirl);
            return static_cast<BrushShapingTool*>(m_canvas->currentTool());
        }();
        for (int i = 0; i < 10; ++i)
            tool->step(QPointF(60, 50), QPointF(60, 50));
        tool->commit();
        QVERIFY(node_cast<CurveObject>(sel()));
        m_editor->document().undo();
        m_editor->setSelection({rect});

        // Freie Transformation: Drehen um einen Punkt, Spiegeln an einer Geraden
        QCOMPARE(FreeTransformTool::transformFor(FreeTransformTool::Mode::Rotate, QPointF(0, 0), QPointF(0, 10), QRectF()).map(QPointF(10, 0)),
                 QPointF(0, 10));
        const QPointF mirrored = FreeTransformTool::transformFor(FreeTransformTool::Mode::Mirror, QPointF(0, 0), QPointF(10, 0), QRectF())
                                     .map(QPointF(3, 4));
        QVERIFY(QLineF(mirrored, QPointF(3, -4)).length() < 1e-9);
        m_canvas->setTool(ToolId::FreeTransform);
        Options::instance().set(QStringLiteral("ws/tools/freeTransform/mode"), 0);
        drag(screen(QPointF(20, 20)), screen(QPointF(20, 60))); // Drehpunkt oben links, 90°
        QVERIFY2(near(sel()->boundingRect(), QRectF(-10, 20, 30, 40), 0.6), qPrintable(describe(sel()->boundingRect())));
        m_canvas->setTool(ToolId::Pick);
    }

    // Kurs "alle Werkzeuge": Smart-Füllung (Schnittfläche zweier Kreise)
    void smartFillCreatesRegion()
    {
        m_canvas->resize(800, 600);
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        Options::instance().resetWorkspace();
        for (const QRectF& r : {QRectF(20, 20, 40, 40), QRectF(45, 20, 40, 40)}) {
            auto e = std::make_unique<EllipseObject>(r.size());
            e->setTransform(QTransform::fromTranslate(r.left(), r.top()));
            m_editor->addObject(std::move(e), QStringLiteral("E"));
        }
        m_canvas->setTool(ToolId::SmartFill);
        click(screen(QPointF(52.5, 40)));
        const auto* lens = node_cast<CurveObject>(m_editor->document().findAs<DrawObject>(m_editor->selection().value(0)));
        QVERIFY(lens);
        // Schnittfläche: x zwischen 45 und 60
        QVERIFY2(near(lens->boundingRect(), QRectF(45, 40 - std::sqrt(400 - 12.5 * 12.5), 15, 2 * std::sqrt(400 - 12.5 * 12.5)), 0.3),
                 qPrintable(describe(lens->boundingRect())));
        QCOMPARE(lens->style().fill, Fill::solid(Color(255, 210, 77)));
        // Nur linker Kreis ohne Schnitt: Mondsichel
        const int count = int(m_editor->pickableObjects().size());
        click(screen(QPointF(30, 40)));
        const auto* moon = m_editor->document().findAs<DrawObject>(m_editor->selection().value(0));
        QCOMPARE(int(m_editor->pickableObjects().size()), count + 1);
        QVERIFY2(near(moon->boundingRect(), QRectF(20, 20, 32.5, 40), 0.3), qPrintable(describe(moon->boundingRect())));
        QVERIFY(!moon->path().contains(QPointF(52.5, 40)));
        // Ausserhalb aller Formen: nichts
        click(screen(QPointF(150, 150)));
        QCOMPARE(int(m_editor->pickableObjects().size()), count + 1);
        m_canvas->setTool(ToolId::Pick);
    }

    // Kurs "alle Werkzeuge": Text an Pfad (Kreis), Initiale, Aufzählungszeichen
    void textOnPathAndParagraphExtras()
    {
        m_canvas->resize(800, 600);
        auto circle = std::make_unique<EllipseObject>(QSizeF(60, 60));
        circle->setTransform(QTransform::fromTranslate(40, 40));
        const NodeId ring = m_editor->addObject(std::move(circle), QStringLiteral("E"));
        auto text = std::make_unique<TextObject>(TextObject::Kind::Artistic);
        text->setPlainText(QStringLiteral("RUNDHERUM"), CharFormat(QStringLiteral("Arial"), 24));
        text->setTransform(QTransform::fromTranslate(150, 150));
        const NodeId tid = m_editor->addObject(std::move(text), QStringLiteral("T"));
        auto t = [this, tid] { return m_editor->document().findAs<TextObject>(tid); };
        m_editor->setSelection({tid, ring});
        QVERIFY(m_editor->canFitSelectedTextToPath());
        QVERIFY(m_editor->fitSelectedTextToPath());
        QVERIFY(t()->onPath());
        // Die Buchstaben liegen jetzt am Kreis: alles nahe am Kreisrand (Radius 30 um 70,70)
        const QRectF b = t()->boundingRect();
        QVERIFY2(QRectF(25, 25, 90, 90).contains(b), qPrintable(describe(b)));
        QVERIFY(!QRectF(150, 140, 100, 40).intersects(b));
        screenshot(QStringLiteral("70-text-an-pfad"));
        // Kreis verschieben: Text folgt
        m_editor->setSelection({ring});
        m_editor->transformSelection(QTransform::fromTranslate(100, 0), QStringLiteral("M"));
        QVERIFY2(std::abs(t()->boundingRect().center().x() - b.center().x() - 100.0) < 1.0, qPrintable(describe(t()->boundingRect())));
        // Abstand vom Pfad vergrössert das Ganze, Speichern/Laden, begradigen
        m_editor->setSelection({tid});
        const double w = t()->boundingRect().width();
        m_editor->setTextPathParams(std::nullopt, 5.0, std::nullopt);
        QVERIFY(t()->boundingRect().width() > w + 5.0);
        auto back = serial::fromJson(serial::toJson(*t()), serial::Ids::Keep);
        QCOMPARE(node_cast<TextObject>(back.get())->pathObject(), ring);
        m_editor->straightenSelectedText();
        QVERIFY(!t()->onPath());
        QVERIFY(t()->boundingRect().height() < 15.0);

        // Initiale über drei Zeilen und Aufzählungszeichen (Mengentext)
        auto para = std::make_unique<TextObject>(TextObject::Kind::Paragraph, QSizeF(80, 60));
        para->setPlainText(QStringLiteral("Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore."),
                           CharFormat(QStringLiteral("Arial"), 10));
        para->setTransform(QTransform::fromTranslate(20, 150));
        const NodeId pid = m_editor->addObject(std::move(para), QStringLiteral("P"));
        auto p = [this, pid] { return m_editor->document().findAs<TextObject>(pid); };
        const QRectF plain = p()->path().boundingRect();
        m_editor->setSelection({pid});
        m_editor->setTextDropCap(3);
        QCOMPARE(p()->paragraphs().first().dropCap, 3);
        // Die Initiale reicht über drei Zeilen: das erste Zeichen ist viel grösser als die übrigen
        QPainterPath capArea;
        capArea.addRect(QRectF(20, 150, 6, 20));
        const QRectF cap = p()->path().intersected(capArea).boundingRect();
        QVERIFY2(cap.height() > 8.0, qPrintable(describe(cap)));
        QVERIFY(p()->path().boundingRect() != plain);
        m_editor->setTextDropCap(0);
        m_editor->setTextBullets(true);
        QVERIFY(p()->paragraphs().first().bullet);
        // Aufzählung: hängender Einzug, der Punkt steht links vor dem Text
        QVERIFY2(p()->path().boundingRect().left() < plain.left() + 1.5, qPrintable(describe(p()->path().boundingRect())));
        auto back2 = serial::fromJson(serial::toJson(*p()), serial::Ids::Keep);
        QVERIFY(node_cast<TextObject>(back2.get())->paragraphs().first().bullet);
        screenshot(QStringLiteral("71-aufzaehlung"));
    }

    // Kurs "alle Werkzeuge": Perfekte Formen mit Glyphengriff
    void perfectShapes()
    {
        m_canvas->resize(800, 600);
        // Jede Form ergibt eine Fläche im Einheitsquadrat (Legenden dürfen hinausragen)
        for (int k = 0; k < perfect::count(); ++k) {
            const QPainterPath p = perfect::unitPath(k, perfect::info(k).glyph);
            QVERIFY2(!p.isEmpty(), qPrintable(perfect::info(k).name));
            if (perfect::info(k).category != perfect::Category::Callout)
                QVERIFY2(QRectF(-0.01, -0.01, 1.02, 1.02).contains(p.boundingRect()), qPrintable(perfect::info(k).name));
        }
        // Pfeil nach rechts aufziehen
        const int arrow = perfect::kinds(perfect::Category::Arrow).first();
        PerfectShapeTool::setCurrentKind(perfect::Category::Arrow, arrow);
        m_canvas->setTool(ToolId::PerfectArrow);
        drag(screen(QPointF(20, 20)), screen(QPointF(80, 40)));
        const auto* shape = node_cast<PerfectShapeObject>(m_editor->document().findAs<DrawObject>(m_editor->selection().value(0)));
        QVERIFY(shape && shape->kind() == arrow);
        QVERIFY(near(shape->boundingRect(), QRectF(20, 20, 60, 20), 0.6));
        // Glyphengriff (Beginn der Spitze bei 60 %, Schaft ab 30 %) ziehen: Schaft dünner
        const QPointF handle = shape->transform().map(shape->glyphLocal());
        QVERIFY(QLineF(handle, QPointF(56, 26)).length() < 0.6);
        drag(screen(handle), screen(QPointF(56, 28)));
        shape = node_cast<PerfectShapeObject>(m_editor->document().findAs<DrawObject>(m_editor->selection().value(0)));
        QVERIFY2(std::abs(shape->glyph().y() - 0.4) < 0.03, qPrintable(QString::number(shape->glyph().y())));
        QVERIFY(!shape->path().contains(QPointF(30, 26.5))); // ausserhalb des schmaleren Schafts
        // Skalieren behält die Form, Speichern/Laden, Form wechseln über die Eigenschaft
        m_editor->transformSelection(xform::scaling(2, 1, QPointF(20, 20)), QStringLiteral("S"));
        QVERIFY(near(m_editor->document().findAs<DrawObject>(shape->id())->boundingRect(), QRectF(20, 20, 120, 20), 0.6));
        auto back = serial::fromJson(serial::toJson(*m_editor->document().findAs<DrawObject>(shape->id())), serial::Ids::Keep);
        const auto* backShape = node_cast<PerfectShapeObject>(back.get());
        QVERIFY(backShape && std::abs(backShape->glyph().y() - 0.4) < 0.03);
        m_editor->setObjectProperty(QStringLiteral("shape"), 1, QStringLiteral("Form"));
        QCOMPARE(node_cast<PerfectShapeObject>(m_editor->document().findAs<DrawObject>(shape->id()))->kind(), perfect::kinds(perfect::Category::Arrow)[1]);
        // Legende: Zeigerspitze ausserhalb der Fläche
        const int callout = perfect::kinds(perfect::Category::Callout).first();
        PerfectShapeTool::setCurrentKind(perfect::Category::Callout, callout);
        m_canvas->setTool(ToolId::PerfectCallout);
        drag(screen(QPointF(100, 100)), screen(QPointF(140, 120)));
        const auto* c = m_editor->document().findAs<DrawObject>(m_editor->selection().value(0));
        QVERIFY(c && c->boundingRect().bottom() > 125.0); // Zeiger ragt unten heraus
        screenshot(QStringLiteral("72-perfekte-formen"));
        m_canvas->setTool(ToolId::Pick);
    }

    // Kurs "alle Werkzeuge": Muster- und Texturfüllung, Mustertransparenz
    void patternAndTextureFills()
    {
        m_canvas->resize(800, 600);
        auto pixel = [this](const QPointF& doc) {
            QImage img(3, 3, QImage::Format_ARGB32_Premultiplied);
            img.fill(Qt::white);
            QPainter p(&img);
            m_canvas->renderPage(p, 0, 8.0, QPointF(1.5, 1.5) - doc * 8.0, true);
            p.end();
            return img.pixelColor(1, 1);
        };
        auto rect = std::make_unique<RectangleObject>(QSizeF(40, 40));
        rect->setTransform(QTransform::fromTranslate(20, 20));
        Style s;
        s.stroke.reset();
        rect->setStyle(s);
        const NodeId id = m_editor->addObject(std::move(rect), QStringLiteral("R"));
        m_editor->setSelection({id});
        // Schachbrett 10 mm: links oben Vordergrund (rot), daneben Hintergrund (gelb)
        PatternFill chess;
        chess.index = 0;
        chess.fg = Color(255, 0, 0);
        chess.bg = Color(255, 255, 0);
        chess.tile = 10.0;
        m_editor->setSelectionFill(Fill::patternFill(chess));
        QCOMPARE(pixel(QPointF(22, 22)), QColor(255, 0, 0));
        QCOMPARE(pixel(QPointF(27, 22)), QColor(255, 255, 0));
        QCOMPARE(pixel(QPointF(32, 22)), QColor(255, 0, 0)); // nächste Kachel
        auto back = serial::fromJson(serial::toJson(*m_editor->document().findAs<DrawObject>(id)), serial::Ids::Keep);
        QCOMPARE(node_cast<DrawObject>(back.get())->style().fill, Fill::patternFill(chess));
        // Textur: Farben zwischen den beiden Texturfarben, nicht einfarbig
        PatternFill clouds;
        clouds.kind = PatternFill::Kind::Texture;
        clouds.fg = Color(0, 0, 255);
        clouds.bg = Color(255, 255, 255);
        const QImage tex = clouds.image();
        QVERIFY(!tex.isNull() && tex.width() == 256);
        QVERIFY(tex.pixelColor(10, 10) != tex.pixelColor(200, 130));
        for (int k = 0; k < patterns::textureCount(); ++k) {
            clouds.index = k;
            QVERIFY(!clouds.image().isNull());
        }
        // Dialog liefert das gewählte Muster
        PatternFillDialog dialog(chess);
        dialog.list()->setCurrentRow(3);
        dialog.tileBox()->setValue(4.0);
        QCOMPARE(dialog.pattern().index, 3);
        QCOMPARE(dialog.pattern().tile, 4.0);
        // Mustertransparenz: Vordergrund deckend, Hintergrund durchsichtig (blau dahinter)
        m_editor->setSelectionFill(Fill::solid(Color(255, 0, 0)));
        auto blue = std::make_unique<RectangleObject>(QSizeF(40, 40));
        blue->setTransform(QTransform::fromTranslate(20, 20));
        Style bs;
        bs.fill = Fill::solid(Color(0, 0, 255));
        bs.stroke.reset();
        blue->setStyle(bs);
        const NodeId behind = m_editor->addObject(std::move(blue), QStringLiteral("B"));
        m_editor->setSelection({behind});
        m_editor->arrangeSelection(Editor::Arrange::ToLayerBack);
        Ramp mask;
        mask.type = Ramp::Type::Pattern;
        mask.from = 0;
        mask.to = 100;
        mask.pattern = chess;
        m_editor->setObjectEffect(id, Effect::Transparency, mask, QStringLiteral("T"));
        QCOMPARE(pixel(QPointF(22, 22)), QColor(255, 0, 0)); // Vordergrund: rot deckend
        QCOMPARE(pixel(QPointF(27, 22)), QColor(0, 0, 255)); // Hintergrund: blau scheint durch
        QCOMPARE(Ramp::fromJson(mask.toJson()), mask);
        screenshot(QStringLiteral("73-muster"));
    }

    // Kurs "alle Werkzeuge": künstlerische Medien Druck und Sprühdose
    void mediaPressureAndSprayer()
    {
        m_canvas->resize(800, 600);
        auto pixel = [this](const QPointF& doc) {
            QImage img(3, 3, QImage::Format_ARGB32_Premultiplied);
            img.fill(Qt::white);
            QPainter p(&img);
            m_canvas->renderPage(p, 0, 8.0, QPointF(1.5, 1.5) - doc * 8.0, true);
            p.end();
            return img.pixelColor(1, 1);
        };
        QPainterPath spine(QPointF(0, 0));
        spine.lineTo(100, 0);
        // Druck: in der Mitte schmal
        ArtisticMedia pressure;
        pressure.kind = ArtisticMedia::Kind::Pressure;
        pressure.width = 10.0;
        pressure.pressure = {1.0, 0.2, 1.0};
        const QPainterPath outline = pathops::mediaOutline(spine, pressure);
        auto heightAt = [&outline](double x) {
            QPainterPath slice;
            slice.addRect(QRectF(x - 0.5, -20, 1.0, 40));
            return outline.intersected(slice).boundingRect().height();
        };
        QVERIFY2(heightAt(50) < 3.0 && heightAt(5) > 8.0, qPrintable(QString::number(heightAt(50)) + QLatin1Char(' ') + QString::number(heightAt(5))));
        QCOMPARE(ArtisticMediaTool::resamplePressure({0.0, 1.0}, 3), (QList<double>{0.0, 0.5, 1.0}));
        // Mit der Maus gezeichnet: Pfeil ab während des Zugs macht das Ende schmaler
        ArtisticMedia toolMedia = pressure;
        toolMedia.pressure.clear();
        m_canvas->setTool(ToolId::ArtisticMedia);
        m_canvas->mediaTool().setMedia(toolMedia);
        press(screen(QPointF(20, 100)));
        for (int i = 1; i <= 10; ++i) {
            if (i == 5)
                for (int k = 0; k < 6; ++k)
                    QTest::keyClick(m_canvas.get(), Qt::Key_Down);
            move(screen(QPointF(20 + 6 * i, 100)));
        }
        release(screen(QPointF(80, 100)));
        const auto* drawn = node_cast<CurveObject>(m_editor->document().findAs<DrawObject>(m_editor->selection().value(0)));
        QVERIFY(drawn && drawn->media().kind == ArtisticMedia::Kind::Pressure);
        QVERIFY(drawn->media().pressure.first() > 0.9 && drawn->media().pressure.last() < 0.5);

        // Sprühdose: farbige Objekte entlang der Linie, als Gruppe in Kurven umwandelbar
        ArtisticMedia spray;
        spray.kind = ArtisticMedia::Kind::Sprayer;
        spray.sprayList = 0;
        spray.width = 6.0;
        spray.spacing = 10.0;
        const QList<pathops::SprayPiece> pieces = pathops::sprayPieces(spine, spray);
        QCOMPARE(pieces.size(), 11);
        SubPath sp;
        sp.nodes << CurveNode{QPointF(20, 150)} << CurveNode{QPointF(120, 150)};
        auto curve = std::make_unique<CurveObject>(QList<SubPath>{sp});
        curve->setMedia(spray);
        const NodeId id = m_editor->addObject(std::move(curve), QStringLiteral("S"));
        const QRectF bounds = m_editor->document().findAs<DrawObject>(id)->boundingRect();
        QVERIFY2(bounds.left() < 20 && bounds.right() > 120 && bounds.height() < 12, qPrintable(describe(bounds)));
        QColor seen = pixel(QPointF(20, 150));
        QVERIFY2(seen != QColor(Qt::white) && seen != QColor(Qt::black), qPrintable(seen.name()));
        auto back = serial::fromJson(serial::toJson(*m_editor->document().findAs<DrawObject>(id)), serial::Ids::Keep);
        QCOMPARE(node_cast<CurveObject>(back.get())->media(), spray);
        m_editor->setSelection({id});
        m_editor->convertSelectionToCurves();
        const auto* group = node_cast<GroupObject>(m_editor->document().findAs<DrawObject>(m_editor->selection().value(0)));
        QVERIFY(group && group->childCount() == 11);
        screenshot(QStringLiteral("74-medien"));
        m_canvas->setTool(ToolId::Pick);
    }

    // Kurs "alle Werkzeuge": Farbpipette – Farbe setzen (wie vom Desktop) und zur Palette hinzufügen
    void pipetteDesktopAndPalette()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        QSettings().remove(QStringLiteral("palette/custom"));
        ColorPalette palette;
        const int before = palette.colorCount();
        m_canvas->setTool(ToolId::ColorPipette);
        auto* pip = static_cast<ColorPipetteTool*>(m_canvas->currentTool());
        pip->setColor(Color(12, 34, 56));
        QVERIFY(pip->applying());
        ColorPalette::addCustomColor(*pip->color());
        QCOMPARE(palette.colorCount(), before + 1);
        QCOMPARE(palette.colorAt(before), std::optional<Color>(Color(12, 34, 56)));
        ColorPalette::addCustomColor(Color(12, 34, 56)); // doppelt: nichts
        QCOMPARE(palette.colorCount(), before + 1);
        ColorPalette second;
        QCOMPARE(second.colorCount(), before + 1); // gespeichert
        // Mit der aufgenommenen Farbe ein Objekt füllen
        auto rect = std::make_unique<RectangleObject>(QSizeF(20, 20));
        rect->setTransform(QTransform::fromTranslate(20, 20));
        const NodeId id = m_editor->addObject(std::move(rect), QStringLiteral("R"));
        click(screen(QPointF(30, 30)));
        QCOMPARE(m_editor->document().findAs<DrawObject>(id)->style().fill, Fill::solid(Color(12, 34, 56)));
        QSettings().remove(QStringLiteral("palette/custom"));
        m_canvas->setTool(ToolId::Pick);
    }

    // Export: alle Bildformate, eigene PSD/PCX/GIF-Codecs, "Nur markierte Objekte"
    void exportFormatsAndSelection()
    {
        QTemporaryDir dir;
        auto rect = [this](const QRectF& r, const Color& c) {
            auto o = std::make_unique<RectangleObject>(r.size());
            o->setTransform(QTransform::fromTranslate(r.left(), r.top()));
            Style s;
            s.fill = Fill::solid(c);
            s.stroke.reset();
            o->setStyle(s);
            return m_editor->addObject(std::move(o), QStringLiteral("R"));
        };
        const NodeId red = rect(QRectF(20, 20, 40, 20), Color(255, 0, 0));
        rect(QRectF(100, 100, 30, 30), Color(0, 0, 255));
        m_editor->setSelection({red});
        // Dialog: "Nur markierte Objekte" ist bei Auswahl angehakt, Bereich = Rechteck des Objekts
        ExportDialog dialog(ExportDialog::Kind::Bitmap, QStringLiteral("png"), *m_canvas);
        QVERIFY(dialog.selectionOnlyBox()->isChecked());
        dialog.dpiBox()->setValue(254); // 10 px je mm
        output::ExportOptions o = dialog.options();
        QCOMPARE(o.only, QList<NodeId>{red});
        QVERIFY(near(output::exportArea(*m_canvas, o), QRectF(20, 20, 40, 20), 0.01));
        // Jedes Format schreiben und wieder lesen: 400 x 200 Pixel, rot. Angeboten wird nur, was Qt
        // wirklich schreiben kann; PNG und JPEG gibt es immer, TIFF mit den Plugins, die TANGENTA mitliefert.
        const QStringList suffixes = output::imageExportSuffixes();
        QVERIFY(suffixes.contains(QStringLiteral("png")) && suffixes.contains(QStringLiteral("jpg")));
#ifdef Q_OS_WIN
        QVERIFY(suffixes.contains(QStringLiteral("tif")));
#endif
        // Die .deb-Pakete bauen ihr TIFF-Plugin selbst und verlangen es (packaging/linux/build-deb.sh)
        if (qEnvironmentVariable("VEK_REQUIRE_TIFF") == QLatin1String("1"))
            QVERIFY(suffixes.contains(QStringLiteral("tif")));
        if (!suffixes.contains(QStringLiteral("tif")))
            qInfo("TIFF-Export nicht verfügbar: Qt kann sein TIFF-Plugin nicht laden");
        for (const QString& suffix : suffixes) {
            const QString path = dir.filePath(QStringLiteral("rot.") + suffix);
            QString error;
            QVERIFY2(output::exportImage(*m_canvas, path, o, &error), qPrintable(suffix + QLatin1Char(' ') + error));
            QFile f(path);
            QVERIFY(f.open(QIODevice::ReadOnly));
            const QByteArray bytes = f.readAll();
            QImage back = codecs::decode(bytes);
            if (back.isNull())
                back = QImage::fromData(bytes);
            QVERIFY2(!back.isNull(), qPrintable(suffix));
            if (suffix != QLatin1String("ico")) {
                QCOMPARE(back.size(), QSize(400, 200));
                const QColor c = back.pixelColor(200, 100);
                QVERIFY2(c.red() > 240 && c.green() < 20 && c.blue() < 20, qPrintable(suffix + QLatin1Char(' ') + c.name()));
            }
            // Importieren wie eine Datei
            QVERIFY2(!imagepool::add(bytes).isEmpty(), qPrintable(suffix));
        }
        QVERIFY(imagepool::importSuffixes().contains(QStringLiteral("psd")));
        QVERIFY(imagepool::importSuffixes().contains(QStringLiteral("pcx")));
        // Transparenz in PNG und GIF: ausserhalb der Form durchsichtig, Graustufen, Schwarzweiss
        o.only = {red};
        o.transparent = true;
        const QRectF area(10, 10, 60, 40);
        Q_UNUSED(area);
        QImage gray = (o.colorMode = output::ExportOptions::ColorMode::Grayscale, o.transparent = false, output::renderImage(*m_canvas, o));
        QCOMPARE(gray.format(), QImage::Format_Grayscale8);
        o.colorMode = output::ExportOptions::ColorMode::BlackWhite;
        QCOMPARE(output::renderImage(*m_canvas, o).format(), QImage::Format_Mono);
        // GIF mit vielen Farben (Verlauf) bleibt lesbar und ähnlich
        QImage ramp(256, 64, QImage::Format_ARGB32);
        for (int x = 0; x < 256; ++x)
            for (int y = 0; y < 64; ++y)
                ramp.setPixel(x, y, qRgba(x, 255 - x, y * 4, y < 8 ? 0 : 255));
        const QImage gif = QImage::fromData(codecs::writeGif(ramp), "GIF");
        QVERIFY(!gif.isNull() && gif.size() == ramp.size());
        QVERIFY(std::abs(gif.pixelColor(100, 40).red() - 100) < 24);
        QCOMPARE(gif.pixelColor(10, 2).alpha(), 0);
        // PSD mit 16 Bit und Alpha
        QImage deep(8, 4, QImage::Format_RGBA64);
        deep.fill(QColor::fromRgba64(QRgba64::fromRgba64(65535, 1000, 0, 32768)));
        const QImage psd = codecs::readPsd(codecs::writePsd(deep));
        QCOMPARE(psd.format(), QImage::Format_RGBA64);
        QCOMPARE(psd.pixelColor(3, 2).rgba64().green(), quint16(1000));
        // PDF und SVG nur mit der Auswahl
        o = dialog.options();
        QString error;
        QVERIFY(output::exportPdf(*m_canvas, dir.filePath(QStringLiteral("a.pdf")), o, &error));
        const QByteArray svg = output::areaSvg(*m_canvas, o);
        QVERIFY(svg.contains("#ff0000") && !svg.contains("#0000ff"));
    }

    // Import: SVG als bearbeitbare Objekte, Export und wieder Import
    void svgImportAsObjects()
    {
        const QByteArray svgData =
            "<?xml version=\"1.0\"?>\n"
            "<svg xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" width=\"100mm\" height=\"50mm\" viewBox=\"0 0 200 100\">\n"
            "  <style>.blau{fill:#0000ff}</style>\n"
            "  <defs>\n"
            "    <linearGradient id=\"v\" x1=\"0\" y1=\"0\" x2=\"1\" y2=\"0\"><stop offset=\"0\" stop-color=\"red\"/><stop offset=\"1\" stop-color=\"#00ff00\"/></linearGradient>\n"
            "    <rect id=\"klein\" width=\"10\" height=\"10\" fill=\"yellow\"/>\n"
            "  </defs>\n"
            "  <rect x=\"10\" y=\"10\" width=\"40\" height=\"20\" fill=\"url(#v)\" stroke=\"black\" stroke-width=\"2\"/>\n"
            "  <g transform=\"translate(100,0) scale(2)\">\n"
            "    <circle class=\"blau\" cx=\"10\" cy=\"10\" r=\"5\"/>\n"
            "    <path d=\"M 0 30 L 20 30 A 10 10 0 0 1 0 30 Z\" fill=\"none\" stroke=\"rgb(255,0,0)\"/>\n"
            "  </g>\n"
            "  <use xlink:href=\"#klein\" x=\"180\" y=\"80\"/>\n"
            "  <text x=\"10\" y=\"90\" font-size=\"12\" font-family=\"Arial\" fill=\"#333\">Hallo SVG</text>\n"
            "</svg>\n";
        QString error;
        svg::Import imp = svg::read(svgData, &error);
        QVERIFY2(imp.group, qPrintable(error));
        QVERIFY(std::abs(imp.size.width() - 100.0) < 0.01 && std::abs(imp.size.height() - 50.0) < 0.01);
        QList<const DrawObject*> all;
        std::function<void(const Node&)> walk = [&](const Node& n) {
            for (int i = 0; i < n.childCount(); ++i) {
                if (const auto* o = node_cast<DrawObject>(n.child(i))) {
                    if (!node_cast<GroupObject>(o))
                        all << o;
                    walk(*o);
                }
            }
        };
        walk(*imp.group);
        QCOMPARE(all.size(), 5);
        // Rechteck: viewBox 200 -> 100 mm, also halbe Grösse; Verlauf rot -> grün, Kontur 1 mm
        const DrawObject* rect = all[0];
        QVERIFY2(near(rect->boundingRect(), QRectF(5, 5, 20, 10), 0.01), qPrintable(describe(rect->boundingRect())));
        QCOMPARE(rect->style().fill.type, Fill::Type::Linear);
        QCOMPARE(rect->style().fill.stops.first().color, Color(255, 0, 0));
        QVERIFY(std::abs(rect->style().strokeWidth - 1.0) < 1e-9);
        // Kreis in skalierter Gruppe: Mitte (100+20)/2 = 60, Radius 5 mm; Farbe aus der Klasse
        QVERIFY(near(all[1]->boundingRect(), QRectF(55, 5, 10, 10), 0.01));
        QCOMPARE(all[1]->style().fill, Fill::solid(Color(0, 0, 255)));
        // Pfad mit Bogen: offen gezeichnet, nur Kontur
        QVERIFY(all[2]->style().fill.isNone() && all[2]->style().stroke == Color(255, 0, 0));
        QVERIFY(all[2]->boundingRect().height() > 4.0);
        // <use> auf das gelbe Rechteck bei (180,80) -> (90,40) mm
        QVERIFY(near(all[3]->boundingRect(), QRectF(90, 40, 5, 5), 0.01));
        // Text bleibt Text
        const auto* text = node_cast<TextObject>(all[4]);
        QVERIFY(text && text->plainText() == QStringLiteral("Hallo SVG"));
        // In das Dokument
        const NodeId g = m_editor->importGroup(std::move(imp.group), true, QStringLiteral("Importieren"));
        QVERIFY(node_cast<GroupObject>(m_editor->document().findAs<DrawObject>(g)));
        // Eigener SVG-Export lässt sich wieder einlesen
        output::ExportOptions o;
        o.only = {g};
        const svg::Import again = svg::read(output::areaSvg(*m_canvas, o), &error);
        QVERIFY2(again.group && again.group->childCount() > 0, qPrintable(error));
        // Pfaddaten mit kompakten Bogen-Flags
        const QPainterPath arc = svg::parsePathData(QStringLiteral("M0,0a5,5 0 015,5"));
        QVERIFY(QLineF(arc.currentPosition(), QPointF(5, 5)).length() < 1e-6);
    }

    // Export EPS/AI, Import PDF/AI (eigener Export wieder eingelesen)
    void epsImportViaGhostscript()
    {
        if (pdf::ghostscript().isEmpty())
            QSKIP("Ghostscript fehlt");
        auto rect = std::make_unique<RectangleObject>(QSizeF(40, 20));
        rect->setTransform(QTransform::fromTranslate(20, 30));
        Style s;
        s.fill = Fill::solid(Color(255, 0, 0));
        s.stroke.reset();
        rect->setStyle(s);
        const NodeId id = m_editor->addObject(std::move(rect), QStringLiteral("R"));
        output::ExportOptions o;
        o.only = {id};
        QString error;
        const QByteArray pdfData = pdf::postscriptToPdf(output::areaEps(*m_canvas, o), &error);
        QVERIFY2(!pdfData.isEmpty(), qPrintable(error));
        const std::vector<svg::Import> pages = pdf::read(pdfData, &error);
        QVERIFY2(pages.size() == 1 && pages.front().group, qPrintable(error));
        QVERIFY2(std::abs(pages.front().size.width() - 40) < 0.6, qPrintable(QString::number(pages.front().size.width())));
        qInfo("Ghostscript: %s", qPrintable(pdf::ghostscript()));
        // Zweiter Aufruf in derselben Sitzung (Instanz sauber freigegeben)
        QVERIFY(!pdf::postscriptToPdf(output::areaEps(*m_canvas, o), &error).isEmpty());
    }

    void epsKeepsTransparency()
    {
        if (pdf::ghostscript().isEmpty())
            QSKIP("Ghostscript fehlt");
        // Rot deckend, darüber Blau mit 50 % Transparenz, halb überlappend
        auto red = std::make_unique<RectangleObject>(QSizeF(40, 20));
        red->setTransform(QTransform::fromTranslate(20, 30));
        Style s;
        s.fill = Fill::solid(Color(255, 0, 0));
        s.stroke.reset();
        red->setStyle(s);
        const NodeId a = m_editor->addObject(std::move(red), QStringLiteral("R"));
        auto blue = std::make_unique<RectangleObject>(QSizeF(20, 20));
        blue->setTransform(QTransform::fromTranslate(50, 30));
        s.fill = Fill::solid(Color(0, 0, 255));
        blue->setStyle(s);
        blue->setEffect(Effect::Transparency, Ramp::uniform(50.0));
        const NodeId b = m_editor->addObject(std::move(blue), QStringLiteral("B"));
        output::ExportOptions o;
        o.only = {a, b};
        o.flattenDpi = 150;
        const QByteArray eps = output::areaEps(*m_canvas, o);
        QVERIFY(eps.contains("eoclip") || eps.contains("clip newpath"));
        // Über Ghostscript zurück und rendern: Mischfarbe in der Überlappung
        QString error;
        const QByteArray pdfData = pdf::postscriptToPdf(eps, &error);
        QVERIFY2(!pdfData.isEmpty(), qPrintable(error));
        std::vector<svg::Import> pages = pdf::read(pdfData, &error);
        QVERIFY2(pages.size() == 1 && pages.front().group, qPrintable(error));
        m_editor->setDocument(std::make_unique<Document>(), QString());
        const NodeId g = m_editor->importGroup(std::move(pages.front().group), true, QStringLiteral("I"));
        output::ExportOptions r;
        r.only = {g};
        r.dpi = 25.4; // 1 Pixel = 1 mm
        const QImage img = output::renderImage(*m_canvas, r);
        QVERIFY(img.width() >= 49 && img.width() <= 51);
        const QColor redOnly = img.pixelColor(15, 10), mixed = img.pixelColor(35, 10), blueOnly = img.pixelColor(45, 10);
        QVERIFY2(redOnly.red() > 240 && redOnly.blue() < 15, qPrintable(redOnly.name()));
        QVERIFY2(std::abs(mixed.red() - 128) < 20 && std::abs(mixed.blue() - 128) < 20 && mixed.green() < 20, qPrintable(mixed.name()));
        QVERIFY2(std::abs(blueOnly.red() - 128) < 20 && blueOnly.blue() > 240, qPrintable(blueOnly.name()));
    }

    void ttfGlyphExport()
    {
        // Ring aus zwei Kreisen (Loch) plus überlappender Balken, mit gerader Füllregel
        auto ring = std::make_unique<EllipseObject>(QSizeF(30, 30));
        ring->setTransform(QTransform::fromTranslate(20, 20));
        Style s;
        s.fill = Fill::none();
        s.stroke = Color(0, 0, 0);
        s.strokeWidth = 4.0;
        ring->setStyle(s);
        const NodeId a = m_editor->addObject(std::move(ring), QStringLiteral("O"));
        auto bar = std::make_unique<RectangleObject>(QSizeF(4, 40));
        bar->setTransform(QTransform::fromTranslate(33, 15));
        Style bs;
        bs.fill = Fill::solid(Color(0, 0, 0));
        bs.stroke.reset();
        bar->setStyle(bs);
        const NodeId b = m_editor->addObject(std::move(bar), QStringLiteral("I"));
        output::ExportOptions o;
        o.only = {a, b};
        const QPainterPath outline = output::selectionOutline(*m_canvas, o);
        QVERIFY(!outline.isEmpty());
        // Mitte des Rings ist leer, Ring und Balken gefüllt (Koordinaten relativ zum Bereich)
        const QRectF r = outline.boundingRect();
        QVERIFY2(std::abs(r.height() - 40) < 0.5, qPrintable(QString::number(r.height())));
        ttf::Font font;
        font.family = QStringLiteral("Tangenta Test");
        font.glyphs << ttf::glyphFromOutline(outline, U'Q', 700);
        const QByteArray data = ttf::write(font);
        QRawFont raw(data, 100);
        QVERIFY(raw.isValid());
        QCOMPARE(raw.familyName(), QStringLiteral("Tangenta Test"));
        const QList<quint32> glyphs = raw.glyphIndexesForString(QStringLiteral("Q "));
        QCOMPARE(glyphs.size(), 2);
        QVERIFY(glyphs[0] != 0 && glyphs[1] != 0);
        const QPainterPath gp = raw.pathForGlyph(glyphs[0]);
        // 700 von 1000 Einheiten bei 100 px = 70 px hoch, auf der Grundlinie (y nach unten negativ)
        QVERIFY2(std::abs(gp.boundingRect().height() - 70) < 1.0, qPrintable(QString::number(gp.boundingRect().height())));
        QVERIFY(std::abs(gp.boundingRect().bottom()) < 1.0);
        // Loch im Ring (links vom Balken), Ring und Balken gefüllt
        const double k = 70.0 / 40.0;
        auto at = [&](double x, double y) { return QPointF(gp.boundingRect().left() + (x - r.left()) * k, gp.boundingRect().top() + (y - r.top()) * k); };
        const QPointF ringLeft = outline.boundingRect().topLeft();
        Q_UNUSED(ringLeft);
        QVERIFY(gp.contains(at(r.left() + 1, r.center().y())));       // Ring links
        QVERIFY(!gp.contains(at(r.left() + 8, r.center().y())));      // Loch
        QVERIFY(gp.contains(at(r.center().x(), r.center().y())));     // Balken
        QVERIFY(gp.contains(at(r.center().x(), r.top() + 1)));        // Balken oben überstehend
        // Zurücklesen und ein zweites Zeichen ergänzen
        QString error;
        std::optional<ttf::Font> back = ttf::read(data, &error);
        QVERIFY2(back, qPrintable(error));
        QCOMPARE(back->family, QStringLiteral("Tangenta Test"));
        QCOMPARE(back->glyphs.size(), 2); // Q und Leerzeichen
        QPainterPath tri;
        tri.addPolygon(QPolygonF({QPointF(0, 10), QPointF(5, 0), QPointF(10, 10)}));
        tri.closeSubpath();
        back->glyphs << ttf::glyphFromOutline(tri, U'ä', 500);
        const QByteArray two = ttf::write(*back);
        if (const QString out = qEnvironmentVariable("VEK_TTF_OUT"); !out.isEmpty()) {
            QFile f(out);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(two);
        }
        QRawFont raw2(two, 100);
        QVERIFY(raw2.isValid());
        const QList<quint32> g2 = raw2.glyphIndexesForString(QStringLiteral("Qä"));
        QVERIFY(g2.size() == 2 && g2[0] && g2[1]);
        // Q unverändert (Umwandlung quadratisch -> kubisch -> quadratisch verlustfrei bis auf Rundung)
        QVERIFY(std::abs(raw2.pathForGlyph(g2[0]).boundingRect().height() - 70) < 1.0);
        QVERIFY(std::abs(raw2.pathForGlyph(g2[1]).boundingRect().height() - 50) < 1.0);
    }

    // PDF mit eingebetteter TrueType-Schrift, die nicht installiert ist (aus ttf::write)
    static QByteArray pdfWithEmbeddedFont()
    {
        ttf::Font font;
        font.family = QStringLiteral("Tangenta Probe");
        QPainterPath a; // Dreieck
        a.addPolygon(QPolygonF({QPointF(0, 10), QPointF(5, 0), QPointF(10, 10)}));
        a.closeSubpath();
        QPainterPath b;
        b.addRect(QRectF(0, 0, 8, 10));
        font.glyphs << ttf::glyphFromOutline(a, U'A', 700) << ttf::glyphFromOutline(b, U'B', 700);
        const QByteArray ttfData = ttf::write(font);
        const QByteArray content = "BT /F1 50 Tf 20 100 Td (AB) Tj ET";
        QByteArray pdf = "%PDF-1.4\n";
        pdf += "1 0 obj << /Type /Catalog /Pages 2 0 R >> endobj\n";
        pdf += "2 0 obj << /Type /Pages /Kids [3 0 R] /Count 1 >> endobj\n";
        pdf += "3 0 obj << /Type /Page /Parent 2 0 R /MediaBox [0 0 300 200] /Contents 4 0 R /Resources << /Font << /F1 5 0 R >> >> >> endobj\n";
        pdf += "4 0 obj << /Length " + QByteArray::number(content.size()) + " >> stream\n" + content + "\nendstream endobj\n";
        pdf += "5 0 obj << /Type /Font /Subtype /TrueType /BaseFont /ABCDEF+TangentaProbe-Bold /FirstChar 65 /LastChar 66 "
               "/Widths [800 900] /Encoding /WinAnsiEncoding /FontDescriptor 6 0 R >> endobj\n";
        pdf += "6 0 obj << /Type /FontDescriptor /FontName /ABCDEF+TangentaProbe-Bold /Flags 32 /FontBBox [0 0 1000 1000] "
               "/ItalicAngle 0 /Ascent 800 /Descent -200 /CapHeight 700 /StemV 80 /FontFile2 7 0 R >> endobj\n";
        pdf += "7 0 obj << /Length " + QByteArray::number(ttfData.size()) + " /Length1 " + QByteArray::number(ttfData.size()) + " >> stream\n" + ttfData
               + "\nendstream endobj\n";
        pdf += "trailer << /Root 1 0 R >>\n%%EOF\n";
        return pdf;
    }

    void pdfEmbeddedFontAsCurves()
    {
        QString error;
        std::vector<svg::Import> pages = pdf::read(pdfWithEmbeddedFont(), &error);
        QVERIFY2(pages.size() == 1 && pages.front().group, qPrintable(error));
        QCOMPARE(pages.front().fonts.size(), std::size_t(1));
        ImportedFont& f = pages.front().fonts.front();
        QCOMPARE(f.baseName, QStringLiteral("TangentaProbe-Bold"));
        QCOMPARE(f.family, QStringLiteral("Tangenta Probe"));
        QVERIFY(f.bold);
        QVERIFY(f.embedded);
        QVERIFY(f.installed.isEmpty());
        QCOMPARE(f.widths.value(U'A'), 800.0);
        QVERIFY(f.glyphs.contains(U'A') && f.glyphs.contains(U'B'));
        // Umriss in 1/1000 Geviert: 700 hoch
        QVERIFY2(std::abs(f.glyphs.value(U'B').boundingRect().height() - 700) < 2, qPrintable(QString::number(f.glyphs.value(U'B').boundingRect().height())));
        QCOMPARE(f.texts.size(), 1);
        QCOMPARE(f.texts.first()->plainText(), QStringLiteral("AB"));
        QVERIFY(f.curves.size() == 1 && f.curves.front());
        // Kurven: 50 pt Schrift, 700/1000 hoch = 35 pt = 12,35 mm; B beginnt nach 800/1000*50 pt
        const QRectF bounds = f.curves.front()->boundingRect();
        QVERIFY2(std::abs(bounds.height() - 35 * 25.4 / 72) < 0.2, qPrintable(QString::number(bounds.height())));
        // Anwenden: Text wird durch Kurven ersetzt
        fontmatch::apply(f, {fontmatch::Choice::Kind::Curves, {}});
        QCOMPARE(pages.front().group->childCount(), 1);
        QVERIFY(node_cast<CurveObject>(pages.front().group->child(0)) != nullptr);
        QVERIFY(f.texts.first() == nullptr);
        // Ersatzschrift statt Kurven
        std::vector<svg::Import> again = pdf::read(pdfWithEmbeddedFont(), &error);
        ImportedFont& g = again.front().fonts.front();
        const QList<fontmatch::Candidate> candidates = fontmatch::suggest(g, 5);
        QVERIFY(!candidates.isEmpty());
        QVERIFY(candidates.first().shape >= 0 && candidates.first().widths >= 0);
        fontmatch::apply(g, {fontmatch::Choice::Kind::Family, candidates.first().family});
        QCOMPARE(g.texts.first()->paragraphs().first().runs.first().format.family, candidates.first().family);
        QVERIFY(g.texts.first()->paragraphs().first().runs.first().format.bold);
        // Ersatzschrift steht auf derselben Grundlinie und ist so lang wie im PDF
        const QRectF exact = g.curves.front()->boundingRect(), replaced = g.texts.first()->path().boundingRect();
        QVERIFY2(std::abs(exact.bottom() - replaced.bottom()) < 0.4, qPrintable(QStringLiteral("%1 / %2").arg(exact.bottom()).arg(replaced.bottom())));
        const double setWidth = g.texts.first()->transform().mapRect(g.texts.first()->frameRect()).width();
        QVERIFY2(std::abs(setWidth - g.runWidths.first()) < 0.3, qPrintable(QStringLiteral("%1 / %2").arg(setWidth).arg(g.runWidths.first())));
        QVERIFY(std::abs(g.runWidths.first() - 1.7 * 50 * 25.4 / 72) < 0.05); // 800 + 900 Einheiten bei 50 pt
    }

    // Type-1-Programme (.t1 und .pfb mit eexec, Unterprogrammen, Flex, seac)
    void type1FontPrograms()
    {
        QStringList files = qEnvironmentVariable("VEK_TYPE1_FILES").split(QLatin1Char(';'), Qt::SkipEmptyParts);
        for (const char* f : {"/usr/share/fonts/type1/urw-base35/NimbusRoman-Regular.t1", "/usr/share/fonts/X11/Type1/C059-Bold.pfb"})
            if (QFile::exists(QLatin1String(f)))
                files << QLatin1String(f);
        if (files.isEmpty())
            QSKIP("keine Type-1-Schriften (VEK_TYPE1_FILES)");
        for (const QString& path : std::as_const(files)) {
            QFile file(path);
            QVERIFY(file.open(QIODevice::ReadOnly));
            const std::unique_ptr<fontprog::Program> p = fontprog::Program::load(file.readAll());
            QVERIFY2(p, qPrintable(path));
            QVERIFY2(p->glyphCount() > 200, qPrintable(QString::number(p->glyphCount())));
            const QRectF h = p->fontMatrix().map(p->glyph(p->gidForName(QStringLiteral("H")))).boundingRect();
            QVERIFY2(h.height() > 0.6 && h.height() < 0.75, qPrintable(path + QStringLiteral(" H %1").arg(h.height())));
            // seac: ä aus a und Trema
            const QRectF a = p->fontMatrix().map(p->glyph(p->gidForName(QStringLiteral("a")))).boundingRect();
            const QRectF ae = p->fontMatrix().map(p->glyph(p->gidForName(QStringLiteral("adieresis")))).boundingRect();
            QVERIFY2(ae.height() > a.height() * 1.15, qPrintable(path + QStringLiteral(" a %1 ä %2").arg(a.height()).arg(ae.height())));
            qInfo("%s: %d Glyphen, %s", qPrintable(QFileInfo(path).fileName()), p->glyphCount(), qPrintable(p->fontName()));
            if (const QString shots = qEnvironmentVariable("VEK_SCREENSHOT_DIR"); !shots.isEmpty()) {
                QImage img(900, 120, QImage::Format_RGB32);
                img.fill(Qt::white);
                QPainter painter(&img);
                painter.setRenderHint(QPainter::Antialiasing);
                double x = 10;
                for (const char* name : {"H", "a", "m", "b", "u", "r", "g", "e", "f", "o", "n", "s", "t", "i", "v", "space", "adieresis", "odieresis",
                                         "udieresis", "germandbls", "Q", "fi", "ampersand"}) {
                    const QPainterPath g = p->glyph(p->gidForName(QLatin1String(name)));
                    painter.fillPath((p->fontMatrix() * QTransform(80, 0, 0, -80, x, 95)).map(g), Qt::black);
                    x += (QLatin1String(name) == QLatin1String("space") ? 0.25 : p->fontMatrix().mapRect(g.boundingRect()).right() + 0.05) * 80;
                }
                painter.end();
                img.save(shots + QStringLiteral("/type1-%1.png").arg(QFileInfo(path).completeBaseName()));
            }
        }
    }

    void fontMatchDialogOffersCurvesAndFonts()
    {
        QString error;
        std::vector<svg::Import> pages = pdf::read(pdfWithEmbeddedFont(), &error);
        QVERIFY(FontMatchDialog::needed(pages.front().fonts));
        FontMatchDialog dialog(pages.front().fonts);
        QCOMPARE(dialog.rowCount(), 1);
        QComboBox* box = dialog.choiceBox(0);
        QVERIFY(box->count() >= 3);
        // Erfundene Schrift passt zu nichts gut: Vorschlag "Als Kurven"
        QCOMPARE(dialog.choice(0).kind, fontmatch::Choice::Kind::Curves);
        if (const QString shots = qEnvironmentVariable("VEK_SCREENSHOT_DIR"); !shots.isEmpty()) {
            dialog.show();
            QCoreApplication::processEvents();
            dialog.grab().save(shots + QStringLiteral("/schriftersetzung.png"));
        }
        // Zweiter Eintrag nach den Kurven: ähnlichste installierte Schrift
        box->setCurrentIndex(1);
        QCOMPARE(dialog.choice(0).kind, fontmatch::Choice::Kind::Family);
        dialog.apply();
        QVERIFY(node_cast<TextObject>(pages.front().group->child(0)) != nullptr);
        QCOMPARE(node_cast<TextObject>(pages.front().group->child(0))->paragraphs().first().runs.first().format.family, dialog.choice(0).family);
    }

    void fontNamesAndWebLinks()
    {
        bool bold = false, italic = false;
        QCOMPARE(fontmatch::familyFromBaseName(QStringLiteral("ABCDEF+TimesNewRomanPS-BoldItalicMT"), &bold, &italic), QStringLiteral("Times New Roman"));
        QVERIFY(bold && italic);
        QCOMPARE(fontmatch::familyFromBaseName(QStringLiteral("Arial,Bold"), &bold, &italic), QStringLiteral("Arial"));
        QVERIFY(bold && !italic);
        QCOMPARE(fontmatch::familyFromBaseName(QStringLiteral("OpenSans-Italic"), &bold, &italic), QStringLiteral("Open Sans"));
        QVERIFY(!bold && italic);
        QCOMPARE(fontmatch::normalized(QStringLiteral("Arial MT")), fontmatch::normalized(QStringLiteral("Arial")));
        const auto google = fontmatch::googleFont(QStringLiteral("Open Sans"));
        QVERIFY(google);
        QCOMPARE(google->url.toString(), QStringLiteral("https://fonts.google.com/specimen/Open+Sans"));
        QVERIFY(!fontmatch::googleFont(QStringLiteral("Tangenta Probe")));
        const QList<fontmatch::WebFont> alt = fontmatch::freeAlternatives(QStringLiteral("Calibri"));
        QVERIFY(!alt.isEmpty() && alt.first().family == QLatin1String("Carlito"));
        QVERIFY(fontmatch::searchUrl(QStringLiteral("Frutiger")).toString().contains(QLatin1String("Frutiger")));
#ifdef Q_OS_WIN
        QCOMPARE(fontmatch::installedFamily(QStringLiteral("Helvetica")), QStringLiteral("Arial"));
        QCOMPARE(fontmatch::installedFamily(QStringLiteral("TimesNewRomanPSMT")), QStringLiteral("Times New Roman"));
#endif
    }

    // Ghostscript bettet NimbusSans (Helvetica-Nachbau) als CFF-Teilmenge ein: CFF-Leser und Vergleich
    void pdfCffFontMatchesArial()
    {
        if (pdf::ghostscript().isEmpty())
            QSKIP("Ghostscript fehlt");
        const QByteArray ps = "%!PS\n/NimbusSans-Regular findfont 40 scalefont setfont 72 300 moveto (Hamburgefonstiv Quaxi) show showpage\n";
        QString error;
        const QByteArray pdfData = pdf::postscriptToPdf(ps, &error);
        QVERIFY2(!pdfData.isEmpty(), qPrintable(error));
        std::vector<svg::Import> pages = pdf::read(pdfData, &error);
        QVERIFY2(!pages.empty() && !pages.front().fonts.empty(), qPrintable(error));
        ImportedFont& f = pages.front().fonts.front();
        qInfo("Schrift im PDF: %s (eingebettet %d, %d Umrisse)", qPrintable(f.baseName), int(f.embedded), int(f.glyphs.size()));
        QVERIFY(f.embedded);
        if (const QString shots = qEnvironmentVariable("VEK_SCREENSHOT_DIR"); !shots.isEmpty() && f.curves.front()) {
            m_editor->setDocument(std::make_unique<Document>(), QString());
            const NodeId id = m_editor->addObject(std::move(f.curves.front()), QStringLiteral("K"));
            output::ExportOptions o;
            o.only = {id};
            o.dpi = 200;
            output::renderImage(*m_canvas, o).save(shots + QStringLiteral("/cff-kurven.png"));
        }
        // H: Versalhöhe einer Grotesk um 700/1000
        const QRectF h = f.glyphs.value(U'H').boundingRect();
        QVERIFY2(h.height() > 650 && h.height() < 760, qPrintable(QString::number(h.height())));
        QVERIFY(f.glyphs.value(U'o').boundingRect().height() > 450);
        QElapsedTimer t;
        t.start();
        const QList<fontmatch::Candidate> c = fontmatch::suggest(f, 5);
        QStringList names;
        for (const auto& x : c)
            names << QStringLiteral("%1 %2%").arg(x.family).arg(int(std::lround(x.score * 100)));
        qInfo("Vorschläge (%lld ms): %s", t.elapsed(), qPrintable(names.join(QStringLiteral(", "))));
        QVERIFY(!c.isEmpty());
        if (QFontDatabase::hasFamily(QStringLiteral("Arial"))) {
            QStringList top;
            for (int i = 0; i < std::min<qsizetype>(3, c.size()); ++i)
                top << c[i].family;
            QVERIFY2(top.contains(QStringLiteral("Arial")), qPrintable(top.join(QStringLiteral(", "))));
        }
    }

    void impositionDialogBookletPdf()
    {
        // 16 A5-Seiten mit grosser Seitennummer
        m_editor->setDocument(std::make_unique<Document>(), QString());
        m_editor->setPageSize(QSizeF(148, 210), true);
        for (int i = 1; i < 16; ++i)
            m_editor->insertPage(i);
        for (int i = 0; i < 16; ++i) {
            m_editor->setCurrentPageIndex(i);
            auto t = std::make_unique<TextObject>(TextObject::Kind::Artistic);
            t->setPlainText(QString::number(i + 1), CharFormat(QStringLiteral("Arial"), 200));
            t->setTransform(QTransform::fromTranslate(30, 40));
            m_editor->addObject(std::move(t), QStringLiteral("N"));
            auto frame = std::make_unique<RectangleObject>(QSizeF(148, 210));
            Style s;
            s.fill = Fill::none();
            s.stroke = Color(0, 120, 200);
            s.strokeWidth = 2;
            frame->setStyle(s);
            m_editor->addObject(std::move(frame), QStringLiteral("R"));
        }
        QSettings().remove(QStringLiteral("impose"));
        ImpositionDialog dialog(*m_canvas, QStringLiteral("Heft"));
        dialog.layoutBox()->setCurrentIndex(dialog.layoutBox()->findData(int(impose::Layout::Booklet)));
        dialog.paperBox()->setCurrentIndex(0); // A4
        QVERIFY2(dialog.result().ok(), qPrintable(dialog.result().error));
        QCOMPARE(dialog.result().sheets.size(), 4);
        QCOMPARE(dialog.result().sheet, QSizeF(297, 210));
        if (const QString shots = qEnvironmentVariable("VEK_SCREENSHOT_DIR"); !shots.isEmpty()) {
            dialog.show();
            QCoreApplication::processEvents();
            dialog.grab().save(shots + QStringLiteral("/ausschiessen.png"));
        }
        // Als PDF: 8 Bogenseiten A4 quer
        QTemporaryDir dir;
        QString pdfPath = dir.filePath(QStringLiteral("heft.pdf"));
        if (const QString keep = qEnvironmentVariable("VEK_PDF_OUT"); !keep.isEmpty())
            pdfPath = keep;
        QString error;
        QVERIFY2(output::exportImposedPdf(*m_canvas, pdfPath, dialog.result(), dialog.pages(), QStringLiteral("Heft"), &error), qPrintable(error));
        QFile f(pdfPath);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const std::vector<svg::Import> sheets = pdf::read(f.readAll(), &error);
        QCOMPARE(sheets.size(), std::size_t(8));
        QVERIFY2(std::abs(sheets.front().size.width() - 297) < 0.5 && std::abs(sheets.front().size.height() - 210) < 0.5, qPrintable(QStringLiteral("%1 x %2").arg(sheets.front().size.width()).arg(sheets.front().size.height())));
        // Falzbogen auf SRA3 mit Anschnitt: 2 Bogen
        dialog.layoutBox()->setCurrentIndex(dialog.layoutBox()->findData(int(impose::Layout::Signature8)));
        dialog.paperBox()->setCurrentIndex(2); // SRA3
        QVERIFY2(dialog.result().ok(), qPrintable(dialog.result().error));
        QCOMPARE(dialog.result().sheets.size(), 2);
        if (const QString shots = qEnvironmentVariable("VEK_SCREENSHOT_DIR"); !shots.isEmpty()) {
            // Bogen in Druckqualität (Vorderseite 1), wie er aus dem Drucker kommt
            QImage sheet(QSize(320 * 2, 450 * 2), QImage::Format_RGB32);
            sheet.fill(Qt::white);
            QPainter p(&sheet);
            output::paintImposedSide(*m_canvas, p, dialog.result(), dialog.result().sheets[0].front, dialog.pages(), 2.0, QStringLiteral("Heft – Bogen 1"));
            p.end();
            sheet.save(shots + QStringLiteral("/falzbogen-vorn.png"));
            dialog.grab().save(shots + QStringLiteral("/ausschiessen-falzbogen.png"));
        }
        QSettings().remove(QStringLiteral("impose"));
    }

    void pdfExportClassic()
    {
        if (pdf::ghostscript().isEmpty())
            QSKIP("Ghostscript fehlt");
        // Seite mit Text, rotem Rechteck über den Rand und einem Bild
        m_editor->setDocument(std::make_unique<Document>(), QString());
        auto t = std::make_unique<TextObject>(TextObject::Kind::Artistic);
        t->setPlainText(QStringLiteral("Grüezi TANGENTA"), CharFormat(QStringLiteral("Arial"), 36));
        t->setTransform(QTransform::fromTranslate(20, 30));
        Style ts;
        ts.fill = Fill::solid(Color(0, 0, 0));
        ts.stroke.reset();
        t->setStyle(ts);
        m_editor->addObject(std::move(t), QStringLiteral("T"));
        auto rect = std::make_unique<RectangleObject>(QSizeF(80, 40));
        rect->setTransform(QTransform::fromTranslate(-5, 200));
        Style rs;
        rs.fill = Fill::solid(Color(255, 0, 0));
        rs.stroke.reset();
        rect->setStyle(rs);
        m_editor->addObject(std::move(rect), QStringLiteral("R"));
        QImage pixels(30, 20, QImage::Format_RGB32);
        pixels.fill(QColor(0, 128, 255));
        auto img = std::make_unique<ImageObject>(imagepool::addImage(pixels), QSizeF(30, 20));
        img->setTransform(QTransform::fromTranslate(120, 100));
        m_editor->addObject(std::move(img), QStringLiteral("I"));

        QTemporaryDir dir;
        QString error;
        auto grep = [](const QString& path) {
            // Alle (auch komprimierten) Objekte als Text
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly))
                return QByteArray();
            const QByteArray d = f.readAll();
            QByteArray all = d;
            for (qsizetype at = 0; (at = d.indexOf("stream", at)) >= 0; at += 6) {
                qsizetype start = at + 6;
                if (start < d.size() && d[start] == '\r')
                    ++start;
                if (start < d.size() && d[start] == '\n')
                    ++start;
                const qsizetype end = d.indexOf("endstream", start);
                if (end < 0)
                    break;
                QByteArray raw = d.mid(start, end - start);
                if (raw.isEmpty() || quint8(raw[0]) != 0x78)
                    continue; // nur zlib-Ströme
                QByteArray buf(qsizetype(4) << 20, '\0');
                // zlib-Strom ohne Kopf? Qt hat qUncompress mit 4-Byte-Längenpräfix
                QByteArray prefixed(4, '\0');
                const quint32 n = quint32(buf.size());
                prefixed[0] = char(n >> 24);
                prefixed[1] = char(n >> 16);
                prefixed[2] = char(n >> 8);
                prefixed[3] = char(n);
                const QByteArray out = qUncompress(prefixed + raw);
                all += out;
            }
            return all;
        };
        // Druckvorstufe: CMYK, Anschnitt 3 mm, Schnittmarken, Autor, Lesezeichen
        PdfSettings s = PdfSettings::preset(QStringLiteral("Druckvorstufe"));
        s.author = QStringLiteral("Roland Käser");
        s.keywords = QStringLiteral("Test");
        s.bookmarks = true;
        s.thumbnails = true;
        const QString press = dir.filePath(QStringLiteral("druck.pdf"));
        QStringList notes;
        QVERIFY2(pdfexport::exportPdf(*m_canvas, press, s, &error, &notes), qPrintable(error));
        QVERIFY(notes.isEmpty());
        const QByteArray a = grep(press);
        QVERIFY(a.contains("/TrimBox") && a.contains("/BleedBox"));
        QVERIFY(a.contains("/Author"));
        QVERIFY(a.contains("/Outlines"));
        QVERIFY(a.contains("/Thumb"));
        QVERIFY2(!a.contains("/DeviceRGB"), "CMYK-Ausgabe enthält RGB");
        // Seite = A4 + 2 x (3 mm Anschnitt + 15 mm Markenrand)
        std::vector<svg::Import> back = pdf::read([&] { QFile f(press); return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray(); }(), &error);
        QVERIFY2(!back.empty(), qPrintable(error));
        QVERIFY2(std::abs(back.front().size.width() - (210 + 36)) < 0.5, qPrintable(QString::number(back.front().size.width())));
        // Text ist echte Schrift (eingebettet, nicht nur Kurven)
        QVERIFY2(!back.front().fonts.empty(), "kein Text im PDF");
        QVERIFY(back.front().fonts.front().embedded);
        // Kennwortschutz
        PdfSettings sec = PdfSettings::preset(QStringLiteral("Dokumentverteilung"));
        sec.openPassword = true;
        sec.openPasswordText = QStringLiteral("offen");
        sec.permissionPassword = true;
        sec.permissionPasswordText = QStringLiteral("geheim");
        sec.printing = PdfSettings::Printing::LowResolution;
        sec.allowCopy = false;
        const QString locked = dir.filePath(QStringLiteral("geschuetzt.pdf"));
        QVERIFY2(pdfexport::exportPdf(*m_canvas, locked, sec, &error), qPrintable(error));
        QVERIFY(grep(locked).contains("/Encrypt"));
        // PDF/A-2b und PDF/X-3
        PdfSettings archive = PdfSettings::preset(QStringLiteral("Archivieren (RGB)"));
        archive.compatibility = PdfSettings::Compatibility::PdfA2b;
        const QString pdfa = dir.filePath(QStringLiteral("archiv.pdf"));
        QVERIFY2(pdfexport::exportPdf(*m_canvas, pdfa, archive, &error), qPrintable(error));
        QVERIFY(grep(pdfa).contains("pdfaid:part"));
        const QString pdfx = dir.filePath(QStringLiteral("x3.pdf"));
        QVERIFY2(pdfexport::exportPdf(*m_canvas, pdfx, PdfSettings::preset(QStringLiteral("PDF/X-3")), &error), qPrintable(error));
        const QByteArray x = grep(pdfx);
        QVERIFY(x.contains("GTS_PDFXVersion") && x.contains("GTS_PDFX"));
        // Vorabkontrolle: rotes Rechteck ragt über den Rand, Anschnitt aus
        PdfSettings plain;
        const QList<pdfexport::Issue> issues = pdfexport::preflight(*m_canvas, plain);
        QVERIFY(std::any_of(issues.begin(), issues.end(), [](const pdfexport::Issue& i) { return i.title.contains(QStringLiteral("Seitenrand")); }));
        // Kennwort mit PDF/X: Fehler
        PdfSettings bad = PdfSettings::preset(QStringLiteral("PDF/X-1a"));
        bad.openPassword = true;
        bad.openPasswordText = QStringLiteral("x");
        const QList<pdfexport::Issue> badIssues = pdfexport::preflight(*m_canvas, bad);
        QVERIFY(std::any_of(badIssues.begin(), badIssues.end(), [](const pdfexport::Issue& i) { return i.severity == pdfexport::Issue::Severity::Error; }));
        // Dialog: Registerkarten, Vorabkontrolle im Titel
        PdfSettingsDialog dialog(*m_canvas, press, plain, QStringLiteral("Dokumentverteilung"));
        QCOMPARE(dialog.tabs()->count(), 7);
        QVERIFY(dialog.tabs()->tabText(6).contains(QStringLiteral("Problem")));
        dialog.presetBox()->setCurrentText(QStringLiteral("Druckvorstufe"));
        QCOMPARE(dialog.settings().colors, PdfSettings::Colors::Cmyk);
        QVERIFY(dialog.settings().bleedLimit);
        if (const QString shots = qEnvironmentVariable("VEK_SCREENSHOT_DIR"); !shots.isEmpty()) {
            dialog.show();
            for (int i = 0; i < dialog.tabs()->count(); ++i) {
                dialog.tabs()->setCurrentIndex(i);
                QCoreApplication::processEvents();
                dialog.grab().save(shots + QStringLiteral("/pdf-tab%1.png").arg(i + 1));
            }
            if (const QString keep = qEnvironmentVariable("VEK_PDF_OUT"); !keep.isEmpty())
                QFile::copy(press, keep);
        }
    }

    void dxfExportFillsCurvesImages()
    {
        // Verlaufsellipse, halbdurchsichtiges Rechteck mit dicker Kontur, Bild
        auto ell = std::make_unique<EllipseObject>(QSizeF(30, 20));
        ell->setTransform(QTransform::fromTranslate(10, 10));
        Style es;
        es.fill = Fill::gradient(Fill::Type::Linear, Color(255, 0, 0), Color(0, 0, 255));
        es.stroke.reset();
        ell->setStyle(es);
        const NodeId a = m_editor->addObject(std::move(ell), QStringLiteral("E"));
        auto rect = std::make_unique<RectangleObject>(QSizeF(20, 20));
        rect->setTransform(QTransform::fromTranslate(30, 15));
        Style rs;
        rs.fill = Fill::solid(Color(0, 160, 0));
        rs.stroke = Color(0, 0, 0);
        rs.strokeWidth = 0.35;
        rect->setStyle(rs);
        rect->setEffect(Effect::Transparency, Ramp::uniform(50.0));
        const NodeId b = m_editor->addObject(std::move(rect), QStringLiteral("R"));
        QImage pixels(20, 10, QImage::Format_ARGB32);
        pixels.fill(QColor(255, 200, 0));
        auto img = std::make_unique<ImageObject>(imagepool::addImage(pixels), QSizeF(20, 10));
        img->setTransform(QTransform::fromScale(1.0, 1.0) * QTransform::fromTranslate(60, 10));
        const NodeId c = m_editor->addObject(std::move(img), QStringLiteral("I"));
        QTemporaryDir dir;
        QString out = dir.filePath(QStringLiteral("zeichnung.dxf"));
        if (const QString keep = qEnvironmentVariable("VEK_DXF_OUT"); !keep.isEmpty())
            out = keep;
        output::ExportOptions o;
        o.only = {a, b, c};
        QString error;
        QVERIFY2(output::exportDxf(*m_canvas, out, o, &error), qPrintable(error));
        QFile f(out);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray data = f.readAll();
        QVERIFY(data.contains("AC1018"));
        QVERIFY(data.contains("\nHATCH\n") && data.contains("\nLINEAR\n"));     // Verlauf als Schraffur
        QVERIFY(data.contains("\n440\n33554559\n") || data.contains("\n440\n33554560\n")); // 50 % Deckung
        QVERIFY(data.contains("\nIMAGE\n") && data.contains("\nIMAGEDEF\n"));
        QVERIFY(data.contains("\n 72\n4\n"));                                       // Splinekante (Ellipse exakt)
        QVERIFY(!data.contains("@"));                                               // alle Marken ersetzt
        const QString png = QFileInfo(out).dir().filePath(QFileInfo(out).completeBaseName() + QStringLiteral("_bild1.png"));
        QVERIFY2(QFile::exists(png), qPrintable(png));
        // Zurücklesen: Verlaufsfläche, durchsichtiges Rechteck, Bild
        svg::Import back = dxf::read(data, &error, QFileInfo(out).absolutePath());
        QVERIFY2(back.group, qPrintable(error));
        bool gradient = false, translucent = false, image = false;
        std::function<void(const Node&)> walk = [&](const Node& n) {
            for (int i = 0; i < n.childCount(); ++i) {
                if (const auto* d = node_cast<DrawObject>(n.child(i))) {
                    gradient |= d->style().fill.type == Fill::Type::Linear;
                    translucent |= d->transparency().type == Ramp::Type::Uniform && std::abs(d->transparency().from - 50) < 2;
                    image |= node_cast<ImageObject>(n.child(i)) != nullptr;
                }
                walk(*n.child(i));
            }
        };
        walk(*back.group);
        QVERIFY(gradient);
        QVERIFY(translucent);
        QVERIFY(image);
    }

    void dxfRoundTrip()
    {
        auto rect = std::make_unique<RectangleObject>(QSizeF(40, 20));
        rect->setTransform(QTransform::fromTranslate(20, 30));
        Style s;
        s.fill = Fill::none();
        s.stroke = Color(255, 0, 0);
        s.strokeWidth = 0.5;
        rect->setStyle(s);
        const NodeId id = m_editor->addObject(std::move(rect), QStringLiteral("R"));
        output::ExportOptions o;
        o.only = {id};
        const QByteArray data = output::areaDxf(*m_canvas, o);
        QVERIFY(data.contains("AC1018") && data.contains("LWPOLYLINE"));
        QString error;
        svg::Import back = dxf::read(data, &error);
        QVERIFY2(back.group && back.group->childCount() == 1, qPrintable(error));
        QVERIFY(std::abs(back.size.width() - 40) < 0.1 && std::abs(back.size.height() - 20) < 0.1);
        const auto* layer = node_cast<GroupObject>(back.group->child(0));
        QVERIFY(layer && !layer->name().isEmpty()); // Ebene des Dokuments
        const auto* curve = node_cast<DrawObject>(layer->child(0));
        QVERIFY(curve && curve->style().stroke == Color(255, 0, 0));
        // Handgeschriebenes DXF: Bogen-Polylinie, Kreis, Text, Block mit Einfügung, Zoll
        const QByteArray hand = "0\nSECTION\n2\nHEADER\n9\n$INSUNITS\n70\n1\n0\nENDSEC\n"
                                "0\nSECTION\n2\nTABLES\n0\nTABLE\n2\nLAYER\n0\nLAYER\n2\nWAND\n62\n5\n0\nENDTAB\n0\nENDSEC\n"
                                "0\nSECTION\n2\nBLOCKS\n0\nBLOCK\n2\nB1\n10\n0\n20\n0\n0\nCIRCLE\n8\n0\n10\n0\n20\n0\n40\n0.5\n0\nENDBLK\n0\nENDSEC\n"
                                "0\nSECTION\n2\nENTITIES\n"
                                "0\nLWPOLYLINE\n8\nWAND\n90\n2\n70\n0\n10\n0\n20\n0\n10\n2\n20\n0\n42\n1\n"
                                "0\nCIRCLE\n8\nWAND\n62\n1\n10\n4\n20\n1\n40\n1\n"
                                "0\nTEXT\n8\n0\n10\n0\n20\n3\n40\n0.2\n1\nHallo\n"
                                "0\nINSERT\n8\nWAND\n2\nB1\n10\n6\n20\n0\n41\n2\n42\n2\n"
                                "0\nENDSEC\n0\nEOF\n";
        svg::Import h = dxf::read(hand, &error);
        QVERIFY2(h.group && h.group->childCount() == 2, qPrintable(error));
        // Ausdehnung in Zoll: x 0..7 (Block-Kreis r=1 bei 6), y -1..3.2 -> 7 Zoll = 177.8 mm
        QVERIFY2(std::abs(h.size.width() - 177.8) < 1.0, qPrintable(QString::number(h.size.width())));
        int curves = 0, texts = 0;
        bool red = false;
        std::function<void(const Node&)> walk = [&](const Node& n) {
            for (int i = 0; i < n.childCount(); ++i) {
                if (node_cast<TextObject>(n.child(i)))
                    ++texts;
                else if (const auto* d = node_cast<CurveObject>(n.child(i))) {
                    ++curves;
                    red |= d->style().stroke == Color(255, 0, 0);
                }
                walk(*n.child(i));
            }
        };
        walk(*h.group);
        QCOMPARE(curves, 3);
        QCOMPARE(texts, 1);
        QVERIFY(red);
        QCOMPARE(dxf::nearestAci(QColor(255, 0, 0)), 1);
        QCOMPARE(dxf::aciColor(5), QColor(0, 0, 255));
    }

    void epsAiPdfRoundTrip()
    {
        QTemporaryDir dir;
        auto rect = std::make_unique<RectangleObject>(QSizeF(40, 20));
        rect->setTransform(QTransform::fromTranslate(20, 30));
        Style s;
        s.fill = Fill::solid(Color(255, 0, 0));
        s.stroke = Color(0, 0, 255);
        s.strokeWidth = 1.0;
        rect->setStyle(s);
        const NodeId id = m_editor->addObject(std::move(rect), QStringLiteral("R"));
        auto ell = std::make_unique<EllipseObject>(QSizeF(30, 30));
        ell->setTransform(QTransform::fromTranslate(80, 30));
        Style es;
        es.fill = Fill::gradient(Fill::Type::Linear, Color(0, 0, 0), Color(255, 255, 255));
        es.stroke.reset();
        ell->setStyle(es);
        m_editor->addObject(std::move(ell), QStringLiteral("E"));
        output::ExportOptions o;
        // EPS: Kopf, Rechteck als Pfad mit roter Füllung, Verlauf als Schattierung
        const QByteArray eps = output::areaEps(*m_canvas, o);
        QVERIFY(eps.startsWith("%!PS-Adobe-3.0 EPSF-3.0"));
        QVERIFY(eps.contains("%%BoundingBox: 0 0 596 842"));
        QVERIFY(eps.contains("1 0 0 rg") && eps.contains("fill"));
        QVERIFY(eps.contains("0 0 1 rg") && eps.contains("stroke"));
        QVERIFY(eps.contains("shfill"));
        // Nur markierte Objekte: Begrenzung = Rechteck samt halber Kontur (41 x 21 mm)
        o.only = {id};
        const QByteArray part = output::areaEps(*m_canvas, o);
        QVERIFY2(part.contains("%%BoundingBox: 0 0 117 60"), part.left(200).constData());
        // AI (PDF-kompatibel) schreiben und als Objekte zurücklesen
        o.only.clear();
        const QString ai = dir.filePath(QStringLiteral("test.ai"));
        QString error;
        QVERIFY(output::exportAi(*m_canvas, ai, o, &error));
        QFile f(ai);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const std::vector<svg::Import> pages = pdf::read(f.readAll(), &error);
        QVERIFY2(pages.size() == 1 && pages.front().group, qPrintable(error));
        QVERIFY(std::abs(pages.front().size.width() - 210.0) < 0.5);
        // Ein rotes Objekt an der Stelle des Rechtecks
        bool found = false;
        std::function<void(const Node&)> walk = [&](const Node& n) {
            for (int i = 0; i < n.childCount(); ++i)
                if (const auto* d = node_cast<DrawObject>(n.child(i))) {
                    if (d->style().fill == Fill::solid(Color(255, 0, 0)) && near(d->boundingRect(), QRectF(20, 30, 40, 20), 0.6))
                        found = true;
                    walk(*d);
                }
        };
        walk(*pages.front().group);
        QVERIFY(found);
        // Einfaches handgeschriebenes PDF mit Text und CMYK
        const QByteArray content = "q 0 0 1 0 k 10 10 100 50 re f Q BT /F1 24 Tf 20 200 Td (Hallo) Tj ET";
        QByteArray pdfData = "%PDF-1.4\n1 0 obj << /Type /Catalog /Pages 2 0 R >> endobj\n"
                             "2 0 obj << /Type /Pages /Kids [3 0 R] /Count 1 >> endobj\n"
                             "3 0 obj << /Type /Page /Parent 2 0 R /MediaBox [0 0 300 300] /Contents 4 0 R "
                             "/Resources << /Font << /F1 5 0 R >> >> >> endobj\n"
                             "4 0 obj << /Length " + QByteArray::number(content.size()) + " >> stream\n" + content + "\nendstream endobj\n"
                             "5 0 obj << /Type /Font /Subtype /Type1 /BaseFont /Helvetica >> endobj\ntrailer << /Root 1 0 R >>\n%%EOF\n";
        const std::vector<svg::Import> simple = pdf::read(pdfData, &error);
        QVERIFY2(simple.size() == 1, qPrintable(error));
        const svg::Import& page = simple.front();
        QCOMPARE(page.group->childCount(), 2);
        const auto* yellow = node_cast<DrawObject>(page.group->child(0));
        QCOMPARE(yellow->style().fill, Fill::solid(Color(255, 255, 0)));
        QVERIFY2(near(yellow->boundingRect(), QRectF(10 * 25.4 / 72, (300 - 60) * 25.4 / 72, 100 * 25.4 / 72, 50 * 25.4 / 72), 0.05),
                 qPrintable(describe(yellow->boundingRect())));
        const auto* hello = node_cast<TextObject>(page.group->child(1));
        QVERIFY(hello && hello->plainText() == QStringLiteral("Hallo"));
    }

    // Sichtprüfung des Vektorimports an echten Dateien, nur mit VEK_IMPORT_FILES (durch ; getrennt)
    void importRealFiles()
    {
        const QStringList files = qEnvironmentVariable("VEK_IMPORT_FILES").split(QLatin1Char(';'), Qt::SkipEmptyParts);
        if (files.isEmpty())
            QSKIP("nur mit VEK_IMPORT_FILES");
        const QString shots = qEnvironmentVariable("VEK_SCREENSHOT_DIR");
        int index = 0;
        for (const QString& path : files) {
            QFile f(path);
            QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(path));
            QByteArray data = f.readAll();
            QString error;
            std::vector<svg::Import> pages;
            QElapsedTimer t;
            t.start();
            if (path.endsWith(QLatin1String(".svg"), Qt::CaseInsensitive))
                pages.push_back(svg::read(data, &error));
            else if (path.endsWith(QLatin1String(".dxf"), Qt::CaseInsensitive))
                pages.push_back(dxf::read(data, &error, QFileInfo(path).absolutePath()));
            else
                pages = pdf::read(data, &error);
            QVERIFY2(!pages.empty() && pages.front().group, qPrintable(path + QLatin1Char(' ') + error));
            qInfo("%s: %d Objekte, %.0f x %.0f mm, %lld ms", qPrintable(path), int(pages.front().group->childCount()),
                  pages.front().size.width(), pages.front().size.height(), t.elapsed());
            for (const ImportedFont& f : pages.front().fonts)
                qInfo("  Schrift %s: Familie %s, installiert '%s', eingebettet %d, %d Texte", qPrintable(f.baseName), qPrintable(f.family),
                      qPrintable(f.installed), int(f.embedded), int(f.texts.size()));
            if (!shots.isEmpty() && FontMatchDialog::needed(pages.front().fonts)) {
                FontMatchDialog dialog(pages.front().fonts);
                dialog.show();
                QCoreApplication::processEvents();
                dialog.grab().save(shots + QStringLiteral("/schriften-%1.png").arg(index + 1));
                dialog.apply();
            }
            m_editor->setDocument(std::make_unique<Document>(), QString());
            const NodeId g = m_editor->importGroup(std::move(pages.front().group), true, QStringLiteral("I"));
            QVERIFY(g != InvalidNodeId);
            if (!shots.isEmpty()) {
                output::ExportOptions o;
                o.only = {g};
                o.dpi = 96;
                output::renderImage(*m_canvas, o).save(shots + QStringLiteral("/import-%1.png").arg(index + 1));
            }
            // Optional gleich als DXF weitergeben (Prüfung mit fremden Lesern)
            if (const QString dxfDir = qEnvironmentVariable("VEK_DXF_DIR"); !dxfDir.isEmpty()) {
                output::ExportOptions o;
                o.only = {g};
                QVERIFY2(output::exportDxf(*m_canvas, dxfDir + QStringLiteral("/import-%1.dxf").arg(index + 1), o, &error), qPrintable(error));
            }
            ++index;
        }
    }

    void connectorsFollowObjects()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        Options::instance().resetWorkspace();
        Options::instance().attach(m_editor.get());
        const NodeId left = drawRect(QRectF(20, 40, 30, 20));
        const NodeId right = drawRect(QRectF(120, 100, 30, 20));

        // Von der rechten Seitenmitte des einen zur linken Seitenmitte des anderen
        m_canvas->setTool(ToolId::ConnectorAngle);
        drag(screen(QPointF(50, 50)), screen(QPointF(120, 110)));
        const auto* c = m_editor->document().findAs<ConnectorObject>(m_editor->selection().value(0));
        QVERIFY(c);
        QCOMPARE(c->start().target, left);
        QCOMPARE(c->end().target, right);
        QPainterPath path = c->path();
        QVERIFY2(QLineF(QPointF(path.elementAt(0)), QPointF(50, 50)).length() < 0.6, "beginnt an der rechten Seitenmitte");
        // Rechtwinklig: alle Strecken waagrecht oder senkrecht
        for (int i = 1; i < path.elementCount(); ++i) {
            const QPointF a = path.elementAt(i - 1), b = path.elementAt(i);
            QVERIFY(std::abs(a.x() - b.x()) < 1e-6 || std::abs(a.y() - b.y()) < 1e-6);
        }

        // Objekt verschieben: die Verbindung folgt
        m_editor->setSelection({right});
        m_editor->transformSelection(xform::translation(QPointF(0, 50)), QStringLiteral("V"));
        c = m_editor->document().findAs<ConnectorObject>(c->id());
        path = c->path();
        QVERIFY2(std::abs(QPointF(path.elementAt(path.elementCount() - 1)).y() - 160.0) < 0.6,
                 qPrintable(QString::number(QPointF(path.elementAt(path.elementCount() - 1)).y())));
        m_editor->document().undo();
        c = m_editor->document().findAs<ConnectorObject>(c->id());
        QVERIFY(std::abs(QPointF(c->path().elementAt(c->path().elementCount() - 1)).y() - 110.0) < 0.6);

        // Ziel löschen: Ende bleibt frei an Ort und Stelle; Speichern behält die Verbindung
        auto back = serial::fromJson(serial::toJson(*c), serial::Ids::Keep);
        QCOMPARE(node_cast<ConnectorObject>(back.get())->end().target, right);
        m_editor->setSelection({right});
        m_editor->deleteSelection();
        c = m_editor->document().findAs<ConnectorObject>(c->id());
        QVERIFY(c && c->end().target == 0 && c->start().target == left);
        QVERIFY(QLineF(c->end().point, QPointF(120, 110)).length() < 0.6);

        // Verbindung trennen
        m_editor->setSelection({c->id()});
        QVERIFY(m_editor->canBreakConnectors());
        m_editor->breakConnectors();
        QVERIFY(!m_editor->document().findAs<ConnectorObject>(c->id())->isAttached());
        screenshot(QStringLiteral("68-verbindung"));
    }

    void smartDrawingRecognizesShapes()
    {
        const double px = 0.1;
        auto wobble = [](double i) { return std::sin(i * 1.7) * 0.3; };
        // Rechteck aus vier ungenauen Kanten
        QList<QPointF> rect;
        for (int i = 0; i <= 40; ++i) rect << QPointF(20 + i, 20 + wobble(i));
        for (int i = 0; i <= 25; ++i) rect << QPointF(60 + wobble(i), 20 + i);
        for (int i = 0; i <= 40; ++i) rect << QPointF(60 - i, 45 + wobble(i));
        for (int i = 0; i <= 24; ++i) rect << QPointF(20 + wobble(i), 45 - i);
        auto r = SmartDrawingTool::recognize(rect, px);
        QVERIFY(node_cast<RectangleObject>(r.get()));
        QVERIFY2(near(r->boundingRect(), QRectF(20, 20, 40, 25), 1.0), qPrintable(describe(r->boundingRect())));
        // Ellipse
        QList<QPointF> ellipse;
        for (int i = 0; i < 90; ++i) {
            const double a = i * 2 * M_PI / 90;
            ellipse << QPointF(100 + 30 * std::cos(a) + wobble(i), 60 + 15 * std::sin(a));
        }
        auto e = SmartDrawingTool::recognize(ellipse, px);
        QVERIFY(node_cast<EllipseObject>(e.get()));
        QVERIFY2(near(e->boundingRect(), QRectF(70, 45, 60, 30), 1.0), qPrintable(describe(e->boundingRect())));
        // Dreieck
        QList<QPointF> tri;
        for (int i = 0; i <= 30; ++i) tri << QPointF(20 + i, 100 + wobble(i));
        for (int i = 0; i <= 30; ++i) tri << QPointF(50 - i * 0.5, 100 - i);
        for (int i = 0; i <= 30; ++i) tri << QPointF(35 - i * 0.5, 70 + i);
        auto t3 = SmartDrawingTool::recognize(tri, px);
        const auto* tc = node_cast<CurveObject>(t3.get());
        QVERIFY(tc && tc->subPaths().first().closed && tc->subPaths().first().nodes.size() == 3);
        // Gerade Linie
        QList<QPointF> line;
        for (int i = 0; i <= 50; ++i) line << QPointF(20 + i, 150 + wobble(i) * 0.5);
        auto l = SmartDrawingTool::recognize(line, px);
        const auto* lc = node_cast<CurveObject>(l.get());
        QVERIFY(lc && lc->subPaths().first().nodes.size() == 2);

        // Mit dem Werkzeug: nach der Verzögerung entsteht das Objekt
        m_canvas->setTool(ToolId::SmartDrawing);
        press(screen(QPointF(120, 120)));
        for (int i = 1; i <= 30; ++i) move(screen(QPointF(120 + i * 2, 120)));
        release(screen(QPointF(180, 120)));
        QVERIFY(QTest::qWaitFor([this] { return m_editor->document().undoText() == QStringLiteral("Smart-Zeichnen"); }, 3000));
    }

    void interactiveFillDragsGradient()
    {
        const NodeId id = drawRect(QRectF(20, 40, 100, 50));
        m_canvas->setTool(ToolId::InteractiveFill);
        drag(screen(QPointF(30, 65)), screen(QPointF(110, 65)), {}, QStringLiteral("15-interaktive-fuellung-vorschau"));
        const auto* r = m_editor->document().findAs<DrawObject>(id);
        const Fill& f = r->style().fill;
        QCOMPARE(f.type, Fill::Type::Linear);
        QVERIFY2(qAbs(f.start.x() - 0.1) < 0.02 && qAbs(f.end.x() - 0.9) < 0.02,
                 qPrintable(QStringLiteral("%1 %2").arg(f.start.x()).arg(f.end.x())));
        QCOMPARE(f.stops.first().color, Color(0, 0, 0));
        QCOMPARE(f.stops.last().color, Color(255, 255, 255));

        // Endpunkt anklicken, dann Palettenfarbe: färbt diesen Endpunkt
        click(screen(QPointF(110, 65)));
        QVERIFY(m_canvas->offerPaletteColor(Color(255, 0, 0), true));
        QCOMPARE(r->style().fill.stops.last().color, Color(255, 0, 0));
        screenshot(QStringLiteral("16-interaktive-fuellung"));
    }

    void brandIconRenders()
    {
        QSvgRenderer renderer(QStringLiteral(":/brand/tangenta.svg"));
        QVERIFY(renderer.isValid());
        // Jede kleine Grösse hat ihren eigenen Schnitt, nicht das skalierte SVG
        for (int size : {16, 24, 32, 48, 64, 128, 256}) {
            const QImage image(QStringLiteral(":/brand/tangenta-%1.png").arg(size));
            QCOMPARE(image.size(), QSize(size, size));
            QCOMPARE(qAlpha(image.pixel(size / 2, size / 2)), 255);  // Platte deckt die Mitte
        }
    }

    void allIconsRender()
    {
        const QStringList names = QDir(QStringLiteral(":/icons")).entryList({QStringLiteral("*.svg")});
        QVERIFY(names.size() > 40);
        // Musterbogen: jedes Symbol in 22 px (Werkzeugleiste) und ausgegraut
        const int columns = 8;
        QImage sheet(columns * 150, ((names.size() + columns - 1) / columns) * 34, QImage::Format_ARGB32);
        sheet.fill(QColor(0xf0, 0xf0, 0xf0));
        QPainter p(&sheet);
        for (int i = 0; i < names.size(); ++i) {
            QSvgRenderer renderer(QStringLiteral(":/icons/") + names[i]);
            QVERIFY2(renderer.isValid(), qPrintable(names[i]));
            const QString name = names[i].chopped(4);
            const QIcon ic = icon(name);
            const QPoint at((i % columns) * 150 + 4, (i / columns) * 34 + 4);
            p.drawPixmap(at, ic.pixmap(22));
            p.drawPixmap(at + QPoint(26, 0), ic.pixmap(22, QIcon::Disabled));
            p.drawText(at + QPoint(52, 15), name);
            // Bei jeder Skalierung aus dem SVG gerendert, also in voller Pixelzahl
            for (qreal scale : {1.0, 1.25, 1.5, 1.75, 2.5}) {
                const QPixmap pm = ic.pixmap(QSize(22, 22), scale);
                QCOMPARE(pm.width(), int(std::ceil(22 * scale)));
                QCOMPARE(pm.devicePixelRatio(), scale);
            }
        }
        p.end();
        const QString shots = qEnvironmentVariable("VEK_SCREENSHOT_DIR");
        if (!shots.isEmpty())
            sheet.save(shots + QStringLiteral("/20-symbole.png"));
    }

    void colorPaletteExpands()
    {
        ColorPalette palette;
        palette.resize(palette.width(), 400);
        palette.show();
        QVERIFY(QTest::qWaitForWindowExposed(&palette));
        palette.expand();
        QWidget* popup = QApplication::activePopupWidget();
        QVERIFY(popup);
        QVERIFY(popup->width() > 3 * ColorPalette::Cell); // mehrere Spalten
        const QString shots = qEnvironmentVariable("VEK_SCREENSHOT_DIR");
        if (!shots.isEmpty())
            popup->grab().save(shots + QStringLiteral("/17-farbleiste-aufgeklappt.png"));
        popup->close();
    }

    void colorDialogModels()
    {
        ColorDialog dlg(QStringLiteral("Gleichmässige Füllung"), Color(0, 0, 0));
        dlg.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dlg));
        auto* hex = dlg.findChild<QLineEdit*>();
        QVERIFY(hex);
        hex->setText(QStringLiteral("#9999FF"));
        emit hex->editingFinished();
        QCOMPARE(dlg.color(), Color(153, 153, 255));
        // Name aus der Palette wird erkannt
        bool named = false;
        for (QComboBox* c : dlg.findChildren<QComboBox*>())
            named = named || c->currentText() == QStringLiteral("Pastellblau");
        QVERIFY(named);
        const QString shots = qEnvironmentVariable("VEK_SCREENSHOT_DIR");
        if (!shots.isEmpty())
            dlg.grab().save(shots + QStringLiteral("/18-farbdialog.png"));
    }

    void gradientDialogTypes()
    {
        GradientDialog dlg(Fill::solid(Color(0, 64, 128)), QSizeF(100, 50));
        dlg.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dlg));
        QCOMPARE(dlg.fill().type, Fill::Type::Linear);
        QCOMPARE(dlg.fill().stops.first().color, Color(0, 64, 128));
        QComboBox* type = dlg.findChildren<QComboBox*>().first();
        type->setCurrentIndex(1);
        QCOMPARE(dlg.fill().type, Fill::Type::Radial);
        const QString shots = qEnvironmentVariable("VEK_SCREENSHOT_DIR");
        if (!shots.isEmpty())
            dlg.grab().save(shots + QStringLiteral("/19-verlaufsdialog.png"));
    }

    void mainWindowOpensDocument()
    {
        // Dokument mit zwei Seiten und einem Objekt auf der Masterseite speichern und im
        // Hauptfenster öffnen
        drawRect(QRectF(20, 40, 100, 50));
        m_editor->setSelectionFill(Fill::solid(Color(255, 128, 0)));
        drawRect(QRectF(0, 0, 210, 12));
        m_editor->setSelectionFill(Fill::solid(Color(0, 64, 128)));
        m_editor->moveSelectionToMaster();
        m_editor->insertPage(1);
        m_editor->setCurrentPageIndex(0);
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("probe.vek"));
        QVERIFY(io::save(m_editor->document(), path));

        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        MainWindow window;
        window.resize(1400, 900);
        QVERIFY(window.openFile(path));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        Canvas* canvas = window.canvas();
        Editor& ed = canvas->editor();
        canvas->fitPage();
        QVERIFY(!window.isWindowModified());
        const QPoint canvasTop = canvas->mapTo(&window, QPoint());

        // Kurven- und Beschneiden-Gruppe als Ausklapplisten
        QCOMPARE(window.findChildren<FlyoutButton*>().size(), 14); // Beschneiden, Zoom, Kurven, Rechteck, Ellipse, Polygon, Bemassung, Verbindung, Kontur, Füllung, interaktiv
        for (const QString& name : {QStringLiteral("Freihand\tF5"), QStringLiteral("2-Punkt-Linie"), QStringLiteral("Bézier"),
                                    QStringLiteral("Künstlerische Medien\tI"), QStringLiteral("Beschneiden"),
                                    QStringLiteral("Messer"), QStringLiteral("Radierer\tX")}) {
            bool found = false;
            for (QAction* a : window.findChildren<QAction*>())
                found = found || (a->text() == name && a->isEnabled());
            QVERIFY2(found, qPrintable(name));
        }
        // Wahl im Flyout: der Knopf zeigt danach dieses Werkzeug
        canvas->setTool(ToolId::ArtisticMedia);
        bool showsMedia = false;
        for (FlyoutButton* b : window.findChildren<FlyoutButton*>())
            showsMedia = showsMedia || (b->defaultAction() && b->defaultAction()->text().startsWith(QStringLiteral("Künstlerische")));
        QVERIFY(showsMedia);
        QApplication::processEvents();
        if (!qEnvironmentVariable("VEK_SCREENSHOT_DIR").isEmpty())
            window.grab().save(qEnvironmentVariable("VEK_SCREENSHOT_DIR") + QStringLiteral("/12-medien-leiste.png"));
        canvas->setTool(ToolId::Pick);
        QApplication::processEvents();
        if (!qEnvironmentVariable("VEK_SCREENSHOT_DIR").isEmpty())
            window.grab().save(qEnvironmentVariable("VEK_SCREENSHOT_DIR") + QStringLiteral("/13-seitenleiste.png"));
        // Sternwerkzeug ohne Auswahl: Leiste mit Spitzen und Schärfe für neue Sterne
        canvas->setTool(ToolId::Star);
        QApplication::processEvents();
        bool sharpness = false;
        for (QDoubleSpinBox* s : window.findChildren<QDoubleSpinBox*>())
            sharpness = sharpness || (s->isVisible() && s->toolTip().startsWith(QStringLiteral("Schärfe der Spitzen")));
        QVERIFY(sharpness);
        if (!qEnvironmentVariable("VEK_SCREENSHOT_DIR").isEmpty())
            window.grab().save(qEnvironmentVariable("VEK_SCREENSHOT_DIR") + QStringLiteral("/15-stern-leiste.png"));
        canvas->setTool(ToolId::Pick);

        // Hilfslinie aus dem oberen Lineal auf die Seite ziehen
        Ruler* top = nullptr;
        for (Ruler* r : window.findChildren<Ruler*>()) {
            if (r->height() < r->width())
                top = r;
        }
        QVERIFY(top);
        const QPoint target = canvas->toScreen(QPointF(50, 150)).toPoint();
        const QPoint inRuler = top->mapFromGlobal(canvas->mapToGlobal(target));
        QTest::mousePress(top, Qt::LeftButton, {}, QPoint(inRuler.x(), 10));
        QTest::mouseMove(top, inRuler);
        QTest::mouseRelease(top, Qt::LeftButton, {}, inRuler);
        QCOMPARE(ed.guides().size(), 1);
        QVERIFY(ed.guides().first().isHorizontal());
        QVERIFY(qAbs(ed.guides().first().anchor.y() - 150.0) < 0.5);

        // Hilfslinie gewählt: Eigenschaftsleiste zeigt Winkel; Eingabe dreht um den Ankerpunkt
        ed.setSelectedGuide(0);
        QApplication::processEvents();
        QDoubleSpinBox* angle = nullptr;
        for (QDoubleSpinBox* s : window.findChildren<QDoubleSpinBox*>()) {
            if (s->isVisible() && s->toolTip().startsWith(QStringLiteral("Winkel der Hilfslinie")))
                angle = s;
        }
        QVERIFY(angle);
        angle->setValue(30.0);
        emit angle->editingFinished();
        QCOMPARE(ed.guides().first().angle, 30.0);
        QVERIFY(qAbs(ed.guides().first().anchor.y() - 150.0) < 0.5);
        QApplication::processEvents();
        if (!qEnvironmentVariable("VEK_SCREENSHOT_DIR").isEmpty())
            window.grab().save(qEnvironmentVariable("VEK_SCREENSHOT_DIR") + QStringLiteral("/11-hilfslinie-30-grad.png"));
        ed.setSelectedGuide(-1);

        // Rechteck wählen: Eigenschaftsleiste zeigt Eckenradien, Eingabe rundet alle Ecken
        const DrawObject* rect = ed.objectAt(QPointF(50, 60), 0.5);
        QVERIFY(rect);
        ed.setSelection({rect->id()});
        QApplication::processEvents();
        QDoubleSpinBox* radius = nullptr;
        for (QDoubleSpinBox* s : window.findChildren<QDoubleSpinBox*>()) {
            if (s->isVisible() && s->toolTip().startsWith(QStringLiteral("Eckenradius oben links")))
                radius = s;
        }
        QVERIFY(radius);
        radius->setValue(8.0);
        emit radius->editingFinished();
        auto* r = ed.document().findAs<RectangleObject>(rect->id());
        QCOMPARE(r->cornerRadius(RectangleObject::BottomRight), 8.0);
        QApplication::processEvents();
        // Objektleiste: Flächen ohne Pfeilspitzen, dafür Mengentext umbrechen,
        // Anordnung und In Kurven
        auto visibleTips = [&window] {
            QStringList tips;
            for (QWidget* w : window.findChildren<QWidget*>()) {
                if (w->isVisible() && (qobject_cast<QComboBox*>(w) || qobject_cast<QToolButton*>(w)))
                    tips << w->toolTip();
            }
            return tips.join(QLatin1Char('|'));
        };
        QString tips = visibleTips();
        for (const QString& t : {QStringLiteral("Linienstil"), QStringLiteral("Konturbreite"), QStringLiteral("Mengentext umbrechen"),
                                 QStringLiteral("Nach vorn auf der Ebene"), QStringLiteral("In Kurven konvertieren")})
            QVERIFY2(tips.contains(t), qPrintable(tips));
        QVERIFY2(!tips.contains(QStringLiteral("Pfeilspitze")), qPrintable(tips));
        // Die Eigenschaftsleiste wechselt den Inhalt, die Zeichenfläche bleibt an ihrem Platz
        QCOMPARE(canvas->mapTo(&window, QPoint()), canvasTop);

        // Linie mit offenen Enden: Pfeilspitzen am Anfang und am Ende
        canvas->setTool(ToolId::Line);
        const QPoint from = canvas->toScreen(QPointF(30, 150)).toPoint();
        const QPoint to = canvas->toScreen(QPointF(90, 170)).toPoint();
        QTest::mousePress(canvas, Qt::LeftButton, {}, from, 1000);
        QTest::mouseMove(canvas, to);
        QTest::mouseRelease(canvas, Qt::LeftButton, {}, to, 10);
        QApplication::processEvents();
        tips = visibleTips();
        QVERIFY2(tips.contains(QStringLiteral("Pfeilspitze am Anfang")) && tips.contains(QStringLiteral("Pfeilspitze am Ende"))
                     && !tips.contains(QStringLiteral("Mengentext")), qPrintable(tips));
        if (!qEnvironmentVariable("VEK_SCREENSHOT_DIR").isEmpty())
            window.grab().save(qEnvironmentVariable("VEK_SCREENSHOT_DIR") + QStringLiteral("/41-linienleiste.png"));
        canvas->setTool(ToolId::Pick);
        ed.setSelection({rect->id()});
        QApplication::processEvents();
        if (!qEnvironmentVariable("VEK_SCREENSHOT_DIR").isEmpty())
            window.grab().save(qEnvironmentVariable("VEK_SCREENSHOT_DIR") + QStringLiteral("/42-rechteckleiste.png"));

        // Polygon: Pfeilknopf am Eckenfeld ändert das gewählte Polygon sofort
        canvas->setTool(ToolId::Polygon);
        const QPoint p1 = canvas->toScreen(QPointF(120, 150)).toPoint();
        const QPoint p2 = canvas->toScreen(QPointF(160, 190)).toPoint();
        QTest::mousePress(canvas, Qt::LeftButton, {}, p1, 1000);
        QTest::mouseMove(canvas, p2);
        QTest::mouseRelease(canvas, Qt::LeftButton, {}, p2, 10);
        QApplication::processEvents();
        const NodeId polygonId = ed.selection().value(0);
        QVERIFY(ed.document().findAs<PolygonObject>(polygonId));
        QDoubleSpinBox* corners = nullptr;
        for (QDoubleSpinBox* s : window.findChildren<QDoubleSpinBox*>()) {
            if (s->isVisible() && s->toolTip().startsWith(QStringLiteral("Punkte oder Seiten")))
                corners = s;
        }
        QVERIFY(corners);
        corners->stepUp();
        QApplication::processEvents();
        QCOMPARE(ed.document().findAs<PolygonObject>(polygonId)->points(), 6);
        canvas->setTool(ToolId::Pick);

        // Textleiste: beim Tippen Schrift und Grösse, für ausgewählten Text das Ganze
        canvas->setTool(ToolId::Text);
        const QPoint textAt = canvas->toScreen(QPointF(30, 230)).toPoint();
        QTest::mouseClick(canvas, Qt::LeftButton, {}, textAt, 1000);
        QTest::keyClicks(canvas, QStringLiteral("Titel"));
        QApplication::processEvents();
        auto visibleWidget = [&window](auto* dummy, const QString& tip) {
            using W = std::remove_pointer_t<decltype(dummy)>;
            for (W* w : window.findChildren<W*>()) {
                if (w->isVisible() && w->toolTip().startsWith(tip))
                    return w;
            }
            return static_cast<W*>(nullptr);
        };
        QVERIFY(visibleWidget(static_cast<FontBox*>(nullptr), QStringLiteral("Schriftart")));
        if (!qEnvironmentVariable("VEK_SCREENSHOT_DIR").isEmpty())
            window.grab().save(qEnvironmentVariable("VEK_SCREENSHOT_DIR") + QStringLiteral("/43-textleiste.png"));
        QTest::keyClick(canvas, Qt::Key_Escape);
        const NodeId textId = ed.selection().value(0);
        QVERIFY(ed.document().findAs<TextObject>(textId));
        canvas->setTool(ToolId::Pick);
        ed.setSelection({textId});
        QApplication::processEvents();
        QToolButton* boldButton = visibleWidget(static_cast<QToolButton*>(nullptr), QStringLiteral("Fett"));
        QVERIFY(boldButton);
        boldButton->click();
        QVERIFY(ed.document().findAs<TextObject>(textId)->paragraphs().first().runs.first().format.bold);
        QComboBox* sizeBox = visibleWidget(static_cast<QComboBox*>(nullptr), QStringLiteral("Schriftgrösse"));
        QVERIFY(sizeBox);
        sizeBox->setEditText(QStringLiteral("36"));
        emit sizeBox->lineEdit()->editingFinished();
        QCOMPARE(ed.document().findAs<TextObject>(textId)->paragraphs().first().runs.first().format.sizePt, 36.0);
        QApplication::processEvents();
        if (!qEnvironmentVariable("VEK_SCREENSHOT_DIR").isEmpty())
            window.grab().save(qEnvironmentVariable("VEK_SCREENSHOT_DIR") + QStringLiteral("/44-text-ausgewaehlt.png"));

        // Mengentext: Klick ins Lineal über dem Rahmen setzt einen Tabulator, der Text springt dorthin
        canvas->setTool(ToolId::Text);
        const QPoint frameFrom = canvas->toScreen(QPointF(20, 250)).toPoint();
        const QPoint frameTo = canvas->toScreen(QPointF(150, 280)).toPoint();
        QTest::mousePress(canvas, Qt::LeftButton, {}, frameFrom, 1000);
        QTest::mouseMove(canvas, frameTo);
        QTest::mouseRelease(canvas, Qt::LeftButton, {}, frameTo, 10);
        QVERIFY(canvas->textTool().editing());
        QTest::keyClicks(canvas, QStringLiteral("Name"));
        QTest::keyClick(canvas, Qt::Key_Tab);
        QTest::keyClicks(canvas, QStringLiteral("Wert"));
        Ruler* ruler = nullptr;
        for (Ruler* r : window.findChildren<Ruler*>()) {
            if (r->width() > r->height())
                ruler = r;
        }
        QVERIFY(ruler);
        const int rulerX = ruler->mapFromGlobal(canvas->mapToGlobal(canvas->toScreen(QPointF(80, 0)).toPoint())).x();
        QTest::mouseClick(ruler, Qt::LeftButton, {}, QPoint(rulerX, Ruler::Thickness - 4));
        double frameLeft = 0.0;
        double frameRight = 0.0;
        QList<double> tabs;
        QVERIFY(canvas->textTool().rulerFrame(&frameLeft, &frameRight, &tabs));
        QCOMPARE(tabs.size(), 1);
        QVERIFY2(std::abs(tabs.first() - 60.0) < 1.0, qPrintable(QString::number(tabs.first())));
        if (!qEnvironmentVariable("VEK_SCREENSHOT_DIR").isEmpty())
            window.grab().save(qEnvironmentVariable("VEK_SCREENSHOT_DIR") + QStringLiteral("/45-tabulator.png"));
        QTest::keyClick(canvas, Qt::Key_Escape);
        const auto* tabbed = ed.document().findAs<TextObject>(ed.selection().value(0));
        QVERIFY(tabbed && tabbed->paragraphs().first().tabs.size() == 1);
        // "Wert" beginnt am Tabulator (60 mm nach dem Rahmen)
        QVERIFY(tabbed->localPath().boundingRect().right() > 60.0);
        canvas->setTool(ToolId::Pick);

        // In die Breite rechnen: 297/2 ergibt die halbe A4-Querbreite, Seitenverhältnis bleibt
        QDoubleSpinBox* width = nullptr;
        for (QDoubleSpinBox* s : window.findChildren<QDoubleSpinBox*>()) {
            if (s->isVisible() && s->toolTip().startsWith(QStringLiteral("Breite")))
                width = s;
        }
        QVERIFY(width);
        const QRectF before = ed.selectionBounds();
        width->setFocus();
        width->selectAll();
        QTest::keyClicks(width, QStringLiteral("297/2"));
        QTest::keyClick(width, Qt::Key_Return);
        QVERIFY2(qAbs(ed.selectionBounds().width() - 148.5) < 1e-6, qPrintable(QString::number(ed.selectionBounds().width())));
        QVERIFY(qAbs(ed.selectionBounds().height() - before.height() * 148.5 / before.width()) < 1e-6);
        QApplication::processEvents();

        const QString shots = qEnvironmentVariable("VEK_SCREENSHOT_DIR");
        if (!shots.isEmpty())
            window.grab().save(shots + QStringLiteral("/00-hauptfenster.png"));

        // Strg+Bild ab / Strg+Bild auf ordnen; Rechtsklick öffnet das Kontextmenü
        ed.duplicateSelection(); // zweites Objekt auf der Ebene, damit es etwas zu ordnen gibt
        const Node* layer = ed.currentPage().child(0);
        const NodeId topmost = layer->child(layer->childCount() - 1)->id();
        ed.setSelection({topmost});
        window.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        canvas->setFocus();
        auto byShortcut = [&window](const QKeySequence& key) -> QAction* {
            for (QAction* a : window.findChildren<QAction*>()) {
                if (a->shortcut() == key)
                    return a;
            }
            return nullptr;
        };
        QAction* back = byShortcut(QKeySequence(Qt::CTRL | Qt::Key_PageDown));
        QAction* forward = byShortcut(QKeySequence(Qt::CTRL | Qt::Key_PageUp));
        QVERIFY(back && forward);
        QVERIFY(!forward->isEnabled()); // liegt schon ganz vorn
        back->trigger();
        QCOMPARE(ed.document().undoText(), QStringLiteral("Eins nach hinten"));
        QVERIFY(forward->isEnabled());
        forward->trigger();
        QCOMPARE(ed.document().undoText(), QStringLiteral("Eins nach vorn"));
        QCOMPARE(layer->child(layer->childCount() - 1)->id(), topmost);
        // Das Menü läuft modal; ein Zeitgeber wartet, bis es offen ist, merkt sich die Einträge
        // und schliesst es wieder
        QStringList texts;
        QTimer poll;
        poll.setInterval(20);
        QObject::connect(&poll, &QTimer::timeout, [&] {
            auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            if (!menu || !menu->isVisible())
                return;
            for (QAction* a : menu->actions())
                texts << a->text();
            if (!qEnvironmentVariable("VEK_SCREENSHOT_DIR").isEmpty())
                menu->grab().save(qEnvironmentVariable("VEK_SCREENSHOT_DIR") + QStringLiteral("/14-kontextmenue.png"));
            menu->close();
            poll.stop();
        });
        poll.start();
        const QPoint at = canvas->toScreen(ed.selectionBounds().center()).toPoint();
        QTest::mouseClick(canvas, Qt::RightButton, {}, at);
        QVERIFY2(texts.contains(QStringLiteral("In &Kurven konvertieren")), qPrintable(texts.join(QLatin1Char('|'))));
        QVERIFY(texts.contains(QStringLiteral("Objekt &sperren")));
        QVERIFY(texts.contains(QStringLiteral("A&nordnung")));
    }

    // SuperTrace wie im Video: Strichgrafik eines Monitor-Symbols
    void imageSearchLikeThurWiki()
    {
        using namespace imagesearch;
        // Ohne Schlüssel: Commons, Openverse, Icons8; der Magnific-Schlüssel schaltet auch Flaticon frei
        QCOMPARE(available({}), QStringList({QStringLiteral("commons"), QStringLiteral("openverse"), QStringLiteral("icons8")}));
        const QHash<QString, QString> keys{{QStringLiteral("magnific"), QStringLiteral("K")}, {QStringLiteral("icons8"), QStringLiteral("T")}};
        QVERIFY(available(keys).contains(QStringLiteral("flaticon")));
        QVERIFY(available(keys).contains(QStringLiteral("magnific")));
        QCOMPARE(optionKey(QStringLiteral("flaticon")), QStringLiteral("global/imageSearch/key/magnific"));

        // Abrufe: Commons zweimal (gross und Kachel), Suchbegriff kodiert, Schlüssel im Kopf
        const QList<Request> commons = requests(QStringLiteral("commons"), QStringLiteral("server rack"), 6, 2, {});
        QCOMPARE(commons.size(), 2);
        QCOMPARE(QUrlQuery(commons[0].url).queryItemValue(QStringLiteral("gsroffset")), QStringLiteral("6"));
        QCOMPARE(QUrlQuery(commons[0].url).queryItemValue(QStringLiteral("gsrsearch"), QUrl::FullyDecoded),
                 QStringLiteral("filetype:bitmap server rack"));
        const QList<Request> flaticon = requests(QStringLiteral("flaticon"), QStringLiteral("monitor"), 12, 1, keys);
        QCOMPARE(flaticon.size(), 1);
        QVERIFY(flaticon[0].url.toString().startsWith(QStringLiteral("https://api.magnific.com/v1/icons")));
        QVERIFY(flaticon[0].headers.contains(qMakePair(QByteArray("x-magnific-api-key"), QByteArray("K"))));
        QVERIFY(QUrlQuery(requests(QStringLiteral("icons8"), QStringLiteral("monitor"), 12, 1, keys)[0].url).hasQueryItem(QStringLiteral("token")));
        QVERIFY(!QUrlQuery(requests(QStringLiteral("icons8"), QStringLiteral("monitor"), 12, 1, {})[0].url).hasQueryItem(QStringLiteral("token")));

        // Auswerten: Antworten wie von den Diensten (gekürzt)
        auto one = [](const QString& source, const QByteArray& json, const QHash<QString, QString>& k = {},
                      const QHash<QString, QByteArray>& extra = {}) {
            QHash<QString, QByteArray> replies = extra;
            replies.insert(source, json);
            return parse(source, replies, k);
        };
        const QList<Hit> c = one(QStringLiteral("commons"),
            sample::k0,
            {}, {{QStringLiteral("commons_preview"), sample::k1}});
        QCOMPARE(c.size(), 2);
        QCOMPARE(c[0].title, QStringLiteral("A.jpg")); // Reihenfolge der Suche
        QCOMPARE(c[1].author, QStringLiteral("Anna"));
        QCOMPARE(c[1].license, QStringLiteral("CC BY 4.0"));
        QCOMPARE(c[1].preview, QUrl(QStringLiteral("https://u/330px-B.jpg")));
        // Wählbare Auflösungen: Original zuerst, dann die festen Commons-Breiten unter der Originalbreite
        QCOMPARE(c[1].image, QUrl(QStringLiteral("https://u/B.jpg")));
        QCOMPARE(c[1].sizes.size(), 5);
        QCOMPARE(sizeLabel(c[1].sizes[0]), QStringLiteral("Original · 4000 × 3000"));
        QCOMPARE(c[1].sizes[1].url, QUrl(QStringLiteral("https://u/3840px-B.jpg")));
        QCOMPARE(sizeLabel(c[1].sizes[1]), QStringLiteral("3840 × 2880"));
        QCOMPARE(c[1].sizes[4].url, QUrl(QStringLiteral("https://u/960px-B.jpg")));
        QCOMPARE(c[0].sizes.size(), 1); // kleines Bild ohne verkleinerte Fassung
        QCOMPARE(c[0].preview, c[0].image);
        QVERIFY(c[0].freeToUse);

        const QList<Hit> ov = one(QStringLiteral("openverse"),
            sample::k2);
        QCOMPARE(ov.size(), 1);
        QCOMPARE(ov[0].license, QStringLiteral("BY-SA 2.0"));
        QCOMPARE(ov[0].preview, QUrl(QStringLiteral("https://f/r.jpg"))); // Bild beim Anbieter, nicht Openverses Vorschau
        QCOMPARE(ov[0].fallbackPreview, QUrl(QStringLiteral("https://o/t")));

        const QByteArray icons8Json = sample::k3;
        const QList<Hit> i8 = one(QStringLiteral("icons8"), icons8Json);
        QCOMPARE(i8.size(), 1); // animierte weggelassen
        QCOMPARE(i8[0].preview, QUrl(QStringLiteral("https://img.icons8.com/?size=100&id=39210&format=png")));
        QVERIFY(!i8[0].vector);
        QVERIFY(i8[0].condition.contains(QStringLiteral("icons8.com")));
        QCOMPARE(choices(i8[0]).size(), 1); // ohne Abo nur das kleine PNG
        const Hit i8paid = one(QStringLiteral("icons8"), icons8Json, keys)[0];
        QVERIFY(i8paid.vector); // mit Schlüssel als SVG
        QVERIFY(i8paid.sizes[0].vector);
        QCOMPARE(i8paid.sizes[1].url, QUrl(QStringLiteral("https://img.icons8.com/?size=1024&id=39210&format=png")));

        const QList<Hit> fl = one(QStringLiteral("flaticon"),
            sample::k4, keys);
        QCOMPARE(fl.size(), 2);
        QVERIFY(fl[0].freeToUse && fl[0].vector);
        QCOMPARE(fl[0].id, QStringLiteral("30955"));
        QCOMPARE(fl[0].license, QStringLiteral("Flaticon frei (Lineal)"));
        QVERIFY(!fl[1].freeToUse);
        QCOMPARE(sizeLabel(fl[0].sizes[0]), QStringLiteral("SVG (Vektor)"));
        QCOMPARE(fl[0].sizes[1].url, QUrl(QStringLiteral("https://api.magnific.com/v1/icons/30955/download?format=png&png_size=512")));

        const QList<Hit> mg = one(QStringLiteral("magnific"),
            sample::k5, keys);
        QCOMPARE(mg.size(), 2);
        QCOMPARE(mg[0].width, 626);
        QVERIFY(mg[0].freeToUse);
        QVERIFY(!mg[1].freeToUse);
        QVERIFY(insertable(mg[1])); // Premium über das eigene Abo
        // Schlüsseltest: verständliche Meldungen je Fehlerart
        QVERIFY(keyProblem(QStringLiteral("pexels"), 200, "{}", QString(), false).isEmpty());
        const QString bad = keyProblem(QStringLiteral("pexels"), 401, R"({"status":401,"message":"Invalid API key"})", QString(), false);
        QVERIFY(bad.contains(QStringLiteral("kennt diesen Schlüssel nicht")));
        QVERIFY(bad.contains(QStringLiteral("„Invalid API key“")));
        QVERIFY(keyProblem(QStringLiteral("pixabay"), 400, "[ERROR 400] Invalid or missing API key (https://pixabay.com/api/docs/).", QString(), false)
                    .contains(QStringLiteral("kennt diesen Schlüssel nicht")));
        QVERIFY(keyProblem(QStringLiteral("unsplash"), 401, R"({"errors":["OAuth error: The access token is invalid"]})", QString(), false)
                    .contains(QStringLiteral("OAuth error")));
        QVERIFY(keyProblem(QStringLiteral("unsplash"), 429, "", QString(), false).contains(QStringLiteral("Abrufkontingent")));
        QVERIFY(keyProblem(QStringLiteral("magnific"), 0, "", QStringLiteral("Host not found"), false).contains(QStringLiteral("Keine Verbindung")));
        QVERIFY(keyProblem(QStringLiteral("magnific"), 0, "", QString(), true).contains(QStringLiteral("12 Sekunden")));
        QVERIFY(keyProblem(QStringLiteral("getty"), 503, "", QString(), false).contains(QStringLiteral("Störung")));
        QVERIFY(insertable(fl[1]));
        // Treffer je Abruf: so viele, wie die Quelle hergibt
        QCOMPARE(QUrlQuery(requests(QStringLiteral("openverse"), QStringLiteral("x"), 40, 1, {})[0].url).queryItemValue(QStringLiteral("page_size")), QStringLiteral("20"));
        QCOMPARE(QUrlQuery(requests(QStringLiteral("magnific"), QStringLiteral("x"), 100, 1, keys)[0].url).queryItemValue(QStringLiteral("limit")), QStringLiteral("100"));
        QCOMPARE(QUrlQuery(requests(QStringLiteral("commons"), QStringLiteral("x"), 100, 2, {})[0].url).queryItemValue(QStringLiteral("gsroffset")), QStringLiteral("50"));

        const QByteArray pixabayJson = sample::k6;
        QCOMPARE(one(QStringLiteral("pixabay"), pixabayJson)[0].image,
                 QUrl(QStringLiteral("https://p/l.jpg")));
        QCOMPARE(sizeLabel(one(QStringLiteral("pixabay"), pixabayJson)[0].sizes[0]), QStringLiteral("bis 1280 Pixel"));
        const QByteArray unsplashJson = sample::k7;
        const Hit us = one(QStringLiteral("unsplash"), unsplashJson)[0];
        QCOMPARE(us.title, QStringLiteral("desk"));
        QCOMPARE(us.sizes.size(), 4);
        QCOMPARE(us.sizes[0].url, QUrl(QStringLiteral("https://s/f")));
        QCOMPARE(us.sizes[2].url, QUrl(QStringLiteral("https://s/raw?ixid=1&fm=jpg&q=90&w=1920")));
        QCOMPARE(sizeLabel(us.sizes[3]), QStringLiteral("1080 × 648"));
        const QByteArray pexelsJson = sample::k8;
        const Hit px = one(QStringLiteral("pexels"), pexelsJson)[0];
        QCOMPARE(px.author, QStringLiteral("P"));
        QCOMPARE(sizeLabel(px.sizes[0]), QStringLiteral("Original · 4000 × 6000"));
        QCOMPARE(px.sizes[1].url, QUrl(QStringLiteral("https://x/o.jpeg?auto=compress&cs=tinysrgb&w=3840")));
        QCOMPARE(sizeLabel(px.sizes.last()), QStringLiteral("433 × 650"));
        const QList<Hit> getty = one(QStringLiteral("getty"),
            sample::k9);
        QCOMPARE(getty[0].preview, QUrl(QStringLiteral("https://g/s")));
        QCOMPARE(getty[0].image, QUrl(QStringLiteral("https://g/l")));
        QVERIFY(!getty[0].freeToUse);

        // Einfügen: SVG als bearbeitbare Kurve, PNG als Bitmap; Nachweis im Objektnamen
        const Hit icon = fl[0];
        const QByteArray svgData = sample::k10;
        const NodeId curveId = insert(*m_editor, icon, svgData);
        QVERIFY(curveId != InvalidNodeId);
        const Node* inserted = m_editor->document().find(curveId);
        QVERIFY(m_editor->document().findAs<CurveObject>(curveId));
        QCOMPARE(inserted->name(), attribution(icon));
        QVERIFY(inserted->name().contains(QStringLiteral("Flaticon")));
        QVERIFY(inserted->name().contains(QStringLiteral("Freepik")));
        QImage png(20, 20, QImage::Format_ARGB32);
        png.fill(Qt::red);
        QByteArray pngData;
        QBuffer buffer(&pngData);
        buffer.open(QIODevice::WriteOnly);
        png.save(&buffer, "PNG");
        const NodeId imageId = insert(*m_editor, i8[0], pngData);
        const auto* image = m_editor->document().findAs<ImageObject>(imageId);
        QVERIFY(image);
        QVERIFY(image->name().contains(QStringLiteral("Icons8")));
        QCOMPARE(insert(*m_editor, i8[0], QByteArray("kein Bild")), InvalidNodeId);
        m_editor->document().undo();
        m_editor->document().undo();

        // Dialog: lizenzpflichtige Treffer nur ohne "nur frei verwendbare", nie einfügbar
        Options::instance().set(QStringLiteral("global/imageSearch/onlyFree"), true);
        Options::instance().set(QStringLiteral("global/imageSearch/kind"), 0);
        ImageSearchDialog dialog;
        const QStringList checked = dialog.checkedSources();
        QVERIFY(checked.contains(QStringLiteral("commons")));
        QVERIFY(checked.contains(QStringLiteral("icons8")));
        QVERIFY(!checked.contains(QStringLiteral("getty")));
        dialog.showResults(c + getty + i8 + fl, {QStringLiteral("Openverse drosselt gerade (429).")});
        QCOMPARE(dialog.visibleCount(), 4); // 2 Commons, 1 Icons8, 1 freies Flaticon
        for (QCheckBox* box : dialog.findChildren<QCheckBox*>())
            if (box->text() == QStringLiteral("nur frei verwendbare"))
                box->setChecked(false);
        QCOMPARE(dialog.visibleCount(), 6);
        int gettyRow = -1;
        auto* list = dialog.findChild<QListWidget*>();
        for (int row = 0; row < list->count(); ++row)
            if (list->item(row)->text().contains(QStringLiteral("Getty Images")))
                gettyRow = row;
        QVERIFY(gettyRow >= 0);
        QVERIFY(!dialog.canInsert(gettyRow));
        QVERIFY(dialog.canInsert(0));
        // Auflösung je Treffer wählbar, grösste vorgewählt; lizenzpflichtige ohne Auswahl
        list->setCurrentRow(1); // Commons B
        QCOMPARE(dialog.sizeBox()->count(), 5);
        QCOMPARE(dialog.sizeBox()->currentIndex(), 0);
        QCOMPARE(dialog.sizeBox()->currentText(), QStringLiteral("Original · 4000 × 3000"));
        QVERIFY(dialog.sizeBox()->isEnabled());
        list->setCurrentRow(gettyRow);
        QVERIFY(!dialog.sizeBox()->isEnabled());
        list->setCurrentRow(1);
        // Kachel: Titel über Quelle und Lizenz
        QVERIFY(list->item(1)->text().startsWith(QStringLiteral("B\nWikimedia Commons\n"))); // ohne .jpg
        // Globus oben rechts öffnet die Seite beim Anbieter (statt Browser hier abgefangen)
        QUrl opened;
        dialog.setUrlOpener([&](const QUrl& url) { opened = url; });
        dialog.resize(1000, 720);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        list->viewport()->repaint();
        const QRect globe = dialog.globeRect(1);
        QVERIFY(!globe.isEmpty());
        QVERIFY(list->visualItemRect(list->item(1)).contains(globe));
        QTest::mouseClick(list->viewport(), Qt::LeftButton, {}, globe.center());
        QCOMPARE(opened, QUrl(QStringLiteral("https://c/B")));
        QVERIFY(!dialog.viewer() || !dialog.viewer()->isVisible());
        // Doppelklick neben dem Globus: Betrachter mit Titel; Pfeil rechts zeigt das nächste Bild
        const QPoint middle = list->visualItemRect(list->item(1)).center();
        QTest::mouseDClick(list->viewport(), Qt::LeftButton, {}, middle);
        QVERIFY(dialog.viewer() && dialog.viewer()->isVisible());
        QCOMPARE(dialog.viewer()->windowTitle(), QStringLiteral("B.jpg"));
        QVERIFY(!dialog.viewer()->findChild<QLabel*>()->pixmap().isNull()); // sofort die Kachel
        QTest::keyClick(dialog.viewer(), Qt::Key_Right);
        QCOMPARE(list->currentRow(), 2);
        QTest::keyClick(dialog.viewer(), Qt::Key_Left);
        QCOMPARE(list->currentRow(), 1);
        if (const QString shots = qEnvironmentVariable("VEK_SCREENSHOT_DIR"); !shots.isEmpty())
            dialog.viewer()->grab().save(shots + QStringLiteral("/55-bildersuche-betrachter.png"));
        dialog.viewer()->hide();
        list->setCurrentRow(1);
        // Anzeige über den Kacheln eingrenzen: nur Icons8 und Flaticon, "Alle" teilweise gewählt
        QCheckBox* all = nullptr;
        for (QCheckBox* box : dialog.findChildren<QCheckBox*>())
            if (box->text() == QStringLiteral("Alle"))
                all = box;
        QVERIFY(all);
        QCOMPARE(all->checkState(), Qt::Checked);
        int viewBoxes = 0;
        for (QCheckBox* box : dialog.findChildren<QCheckBox*>())
            viewBoxes += box->text().contains(QRegularExpression(QStringLiteral("\\(\\d+\\)$"))) ? 1 : 0;
        QCOMPARE(viewBoxes, 4); // Commons (2), Icons8 (1), Flaticon (2), Getty (1)
        dialog.showSources({QStringLiteral("icons8"), QStringLiteral("flaticon")});
        QCOMPARE(dialog.visibleCount(), 3);
        QCOMPARE(all->checkState(), Qt::PartiallyChecked);
        all->click(); // teilweise -> alle
        QCOMPARE(all->checkState(), Qt::Checked);
        QCOMPARE(dialog.visibleCount(), 6);
        all->click(); // keine
        QCOMPARE(dialog.visibleCount(), 0);
        all->click();
        QCOMPARE(dialog.visibleCount(), 6);
        // Nur Symbole: Commons und Getty verschwinden
        for (QComboBox* box : dialog.findChildren<QComboBox*>())
            box->setCurrentIndex(2);
        QCOMPARE(dialog.visibleCount(), 3);
        QVERIFY(!dialog.checkedSources().contains(QStringLiteral("commons")));
        const QString dir = qEnvironmentVariable("VEK_SCREENSHOT_DIR");
        if (!dir.isEmpty()) {
            for (QComboBox* box : dialog.findChildren<QComboBox*>())
                box->setCurrentIndex(0);
            dialog.grab().save(dir + QStringLiteral("/52-bildersuche.png"));
        }
        Options::instance().resetWorkspace();

        // Optionen > Global > Bildersuche: je Schlüsselfeld ein Test-Knopf; leeres Feld meldet sich sofort
        OptionsDialog options(*m_editor, nullptr);
        QVERIFY(options.selectPage(QStringLiteral("Global/Bildersuche")));
        auto* testPexels = options.findChild<QPushButton*>(QStringLiteral("test-pexels"));
        QVERIFY(testPexels);
        QVERIFY(options.findChild<QPushButton*>(QStringLiteral("test-magnific")));
        QVERIFY(!options.findChild<QPushButton*>(QStringLiteral("test-commons")));
        testPexels->click();
        auto* resultPexels = options.findChild<QLabel*>(QStringLiteral("result-pexels"));
        QVERIFY(resultPexels->text().contains(QStringLiteral("Kein Schlüssel")));
        if (!dir.isEmpty())
            options.grab().save(dir + QStringLiteral("/54-bildersuche-schluessel.png"));
    }

    // Echte Abfrage ohne Schlüssel (Commons, Openverse, Icons8): nur mit VEK_NETWORK_TESTS=1
    void imageSearchLive()
    {
        if (qEnvironmentVariable("VEK_NETWORK_TESTS") != QLatin1String("1"))
            QSKIP("Netzwerktest: VEK_NETWORK_TESTS=1 setzen");
        using namespace imagesearch;
        ImageSearch search;
        QList<Hit> hits;
        QStringList complaints;
        bool done = false;
        connect(&search, &ImageSearch::finished, this, [&](const QList<Hit>& h, const QStringList& c) {
            hits = h;
            complaints = c;
            done = true;
        });
        // Abbrechen (Fenster schliessen): nichts läuft weiter, keine Rückmeldung mehr
        search.search(QStringLiteral("monitor"), {QStringLiteral("commons"), QStringLiteral("icons8")}, 1, 6);
        QVERIFY(search.running() > 0);
        search.cancel();
        QCOMPARE(search.running(), 0);
        QTest::qWait(300);
        QVERIFY(!done);
        // Gesamt-Zeitgrenze: was bis dahin nicht geantwortet hat, wird übersprungen
        search.setSearchDeadline(1);
        search.search(QStringLiteral("monitor"), {QStringLiteral("commons"), QStringLiteral("icons8")}, 1, 6);
        QTRY_VERIFY_WITH_TIMEOUT(done, 3000);
        QVERIFY2(complaints.join(u'|').contains(QStringLiteral("übersprungen")), qPrintable(complaints.join(u'|')));
        QCOMPARE(search.running(), 0);
        done = false;
        search.setSearchDeadline(ImageSearch::kSearchDeadlineMs);
        QElapsedTimer clock;
        clock.start();
        search.search(QStringLiteral("monitor"), {QStringLiteral("commons"), QStringLiteral("openverse"), QStringLiteral("icons8")}, 1, 6);
        QTRY_VERIFY_WITH_TIMEOUT(done, 20000);
        QVERIFY(clock.elapsed() < ImageSearch::kSearchDeadlineMs + 2000);
        qInfo() << "Treffer" << hits.size() << "Klagen" << complaints << clock.elapsed() << "ms";
        {
            // Fenster schliessen, während Suche und Vorschaubilder noch laufen: kein Absturz, nichts bleibt hängen
            auto* closing = new ImageSearchDialog;
            closing->setTerm(QStringLiteral("monitor"));
            closing->search();
            QTRY_VERIFY_WITH_TIMEOUT(closing->visibleCount() > 0, 20000);
            delete closing;
            QTest::qWait(1000);
            QVERIFY(!QApplication::overrideCursor());
        }
        QSet<QString> seen;
        for (const Hit& h : hits)
            seen << h.source;
        qInfo() << "Quellen" << seen;
        QVERIFY2(seen.contains(QStringLiteral("commons")), qPrintable(complaints.join(u'|')));
        QVERIFY2(seen.contains(QStringLiteral("icons8")), qPrintable(complaints.join(u'|')));
        // Ein Symbol von Icons8 herunterladen und einfügen
        const auto icon = std::find_if(hits.begin(), hits.end(), [](const Hit& h) { return h.source == QLatin1String("icons8"); });
        QByteArray data;
        QString error;
        bool fetched = false;
        search.download(*icon, [&](const QByteArray& d, const QString& e) {
            data = d;
            error = e;
            fetched = true;
        });
        QTRY_VERIFY_WITH_TIMEOUT(fetched, 20000);
        QVERIFY2(!data.isEmpty(), qPrintable(error));
        const NodeId id = insert(*m_editor, *icon, data);
        QVERIFY(m_editor->document().findAs<ImageObject>(id));
        m_editor->document().undo();
        // Ein Commons-Foto
        const auto photo = std::find_if(hits.begin(), hits.end(), [](const Hit& h) { return h.source == QLatin1String("commons"); });
        fetched = false;
        search.download(*photo, [&](const QByteArray& d, const QString& e) {
            data = d;
            error = e;
            fetched = true;
        });
        QTRY_VERIFY_WITH_TIMEOUT(fetched, 60000);
        QVERIFY2(!QImage::fromData(data).isNull(), qPrintable(error));
        qInfo() << "Commons-Foto" << QImage::fromData(data).size() << photo->title << photo->license;
        QCOMPARE(QImage::fromData(data).width(), choices(*photo).first().width); // grösste Fassung
        // Kleinste Fassung
        fetched = false;
        search.download(*photo, [&](const QByteArray& d, const QString& e) {
            data = d;
            error = e;
            fetched = true;
        }, int(choices(*photo).size()) - 1);
        QTRY_VERIFY_WITH_TIMEOUT(fetched, 30000);
        QVERIFY2(!QImage::fromData(data).isNull(), qPrintable(error));
        QCOMPARE(QImage::fromData(data).width(), choices(*photo).last().width);
        qInfo() << "Fassungen" << [&] { QStringList l; for (const Size& s : choices(*photo)) l << sizeLabel(s); return l; }();

        // Schlüsseltest gegen die echten Dienste: ein falscher Schlüssel und die eigenen (VEK_KEY_<QUELLE>)
        auto testKey = [&](const QString& src, const QString& key) -> bool {
            bool ok = false, answered = false;
            QString message;
            search.testKey(src, key, [&](bool o, const QString& m) {
                ok = o;
                message = m;
                answered = true;
            });
            if (!QTest::qWaitFor([&] { return answered; }, 20000))
                return false;
            qInfo().noquote() << "Test" << src << (ok ? "ok:" : "FEHLER:") << message;
            return ok;
        };
        QVERIFY(!testKey(QStringLiteral("pexels"), QStringLiteral("falsch123")));
        QVERIFY(!testKey(QStringLiteral("unsplash"), QStringLiteral("falsch 123")));
        for (const QString& src : {QStringLiteral("pixabay"), QStringLiteral("pexels"), QStringLiteral("unsplash"), QStringLiteral("magnific")}) {
            const QString key = qEnvironmentVariable(qPrintable(QStringLiteral("VEK_KEY_") + src.toUpper()));
            if (!key.isEmpty())
                QVERIFY(testKey(src, key));
        }

        // Magnific-Premium über das eigene Abo: nur mit VEK_MAGNIFIC_KEY (ein echter, abgerechneter Download)
        const QString magnificKey = qEnvironmentVariable("VEK_MAGNIFIC_KEY");
        if (!magnificKey.isEmpty()) {
            search.setKeys({{QStringLiteral("magnific"), magnificKey}});
            done = false;
            search.search(QStringLiteral("Schaffhausen"), {QStringLiteral("magnific")}, 1, 100);
            QTRY_VERIFY_WITH_TIMEOUT(done, 20000);
            qInfo() << "Magnific-Treffer" << hits.size();
            QVERIFY(hits.size() > 50);
            const auto premium = std::find_if(hits.begin(), hits.end(), [](const Hit& h) { return h.subscription; });
            QVERIFY(premium != hits.end());
            fetched = false;
            search.download(*premium, [&](const QByteArray& d, const QString& e) {
                data = d;
                error = e;
                fetched = true;
            });
            QTRY_VERIFY_WITH_TIMEOUT(fetched, 60000);
            QVERIFY2(!QImage::fromData(data).isNull(), qPrintable(error));
            qInfo() << "Magnific-Premium" << QImage::fromData(data).size() << "Vorschau" << premium->width << premium->height;
            QVERIFY(QImage::fromData(data).width() > premium->width);
            search.setKeys({});
        }

        const QString dir = qEnvironmentVariable("VEK_SCREENSHOT_DIR");
        if (!dir.isEmpty()) {
            ImageSearchDialog dialog;
            dialog.resize(1000, 720);
            dialog.setTerm(QStringLiteral("monitor"));
            dialog.search();
            QVERIFY(!QApplication::overrideCursor()); // Sanduhr nur im Dialog, nie für die ganze Anwendung
            QTRY_VERIFY_WITH_TIMEOUT(dialog.visibleCount() > 0, 30000);
            QTest::qWait(4000); // Vorschaubilder
            if (auto* list = dialog.findChild<QListWidget*>())
                list->setCurrentRow(0);
            dialog.grab().save(dir + QStringLiteral("/53-bildersuche-live.png"));
            // Betrachter mit dem ersten Commons-Foto (grössere Fassung aus dem Netz)
            auto* list = dialog.findChild<QListWidget*>();
            for (int row = 0; row < list->count(); ++row)
                if (list->item(row)->text().contains(QStringLiteral("Wikimedia Commons"))) {
                    dialog.showViewer(row);
                    break;
                }
            QTest::qWait(5000);
            if (dialog.viewer())
                dialog.viewer()->grab().save(dir + QStringLiteral("/56-bildersuche-betrachter-live.png"));
        }
    }

    // CDR-Import: beschädigte und ungewöhnliche Dateien (Testdateien aus LibreOffice, MPL-2.0) dürfen
    // nicht abstürzen; Ergebnis ist entweder eine Zeichnung oder eine Fehlermeldung
    void cdrImportIsRobust()
    {
        for (const char* name : {"EDB-35217-2.cdr", "fdo65220-2.cdr"}) {
            QFile file(QStringLiteral(VEK_SOURCE_DIR "/tests/data/") + QLatin1String(name));
            QVERIFY(file.open(QIODevice::ReadOnly));
            const QByteArray data = file.readAll();
            QString error;
            const std::vector<svg::Import> pages = cdr::read(data, &error);
            QVERIFY2(!pages.empty() || !error.isEmpty(), name);
        }
        QVERIFY(!cdr::isSupported(QByteArray("kein CDR")));
        QString error;
        QVERIFY(cdr::read(QByteArray::fromHex("524946460000000043445239"), &error).empty());
        QVERIFY(!error.isEmpty());
    }

    // Echte Zeichnungen (nur lokal, VEK_CDR_DIR): jede Datei öffnet sich mit Objekten; mit
    // VEK_SCREENSHOT_DIR wird die erste Seite als Bild abgelegt
    void cdrImportLocalFiles()
    {
        const QString dir = qEnvironmentVariable("VEK_CDR_DIR");
        if (dir.isEmpty())
            QSKIP("CDR-Dateien: VEK_CDR_DIR setzen");
        const QString shots = qEnvironmentVariable("VEK_SCREENSHOT_DIR");
        const QFileInfoList files = QDir(dir).entryInfoList({QStringLiteral("*.cdr"), QStringLiteral("*.cmx")}, QDir::Files, QDir::Name);
        QVERIFY(!files.isEmpty());
        for (const QFileInfo& info : files) {
            QFile file(info.absoluteFilePath());
            QVERIFY(file.open(QIODevice::ReadOnly));
            QElapsedTimer timer;
            timer.start();
            QString error;
            std::vector<svg::Import> pages = cdr::read(file.readAll(), &error);
            QVERIFY2(!pages.empty(), qPrintable(info.fileName() + QStringLiteral(": ") + error));
            int objects = 0;
            for (const svg::Import& page : pages)
                objects += page.group ? page.group->childCount() : 0;
            qInfo().noquote() << info.fileName() << pages.size() << "Seite(n)," << objects << "Objekte," << pages.front().fonts.size()
                              << "Schriften," << timer.elapsed() << "ms," << pages.front().size.width() << "x" << pages.front().size.height() << "mm";
            QVERIFY2(objects > 0, qPrintable(info.fileName()));
            if (shots.isEmpty())
                continue;
            m_editor->setDocument(std::make_unique<Document>(), QString());
            m_editor->setPageSize(pages.front().size, true);
            m_editor->importGroup(std::move(pages.front().group), false, QStringLiteral("CDR"));
            output::ExportOptions o;
            o.dpi = 50;
            output::renderImage(*m_canvas, o).save(shots + QStringLiteral("/cdr-") + info.completeBaseName() + QStringLiteral(".png"));
        }
    }

    // Bildschirmfotos für README und Webseite (docs/screenshots): nur mit VEK_SHOWCASE_DIR.
    // Mit VEK_AI_LIVE=1 gestaltet die KI (Claude über das Abo) einen Teil des Plakats selbst.
    void showcase()
    {
        const QString dir = qEnvironmentVariable("VEK_SHOWCASE_DIR");
        if (dir.isEmpty())
            QSKIP("Schaufenster: VEK_SHOWCASE_DIR setzen");
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        QDir().mkpath(dir);
        MainWindow window;
        window.resize(1600, 1000);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        Editor& ed = window.canvas()->editor();
        AiTools tools(ed, *window.canvas(), &window);
        auto run = [&](const QString& name, const QJsonObject& input) {
            AiTools::Result result;
            bool got = false;
            tools.call(name, input, true, [&](const AiTools::Result& r) {
                result = r;
                got = true;
            });
            (void)QTest::qWaitFor([&] { return got; }, 60000);
            return result;
        };
        auto showDock = [&](const QString& title) {
            for (QTabBar* bar : window.findChildren<QTabBar*>())
                for (int i = 0; i < bar->count(); ++i)
                    if (bar->tabText(i) == title)
                        bar->setCurrentIndex(i);
        };
        // 1. Plakat aus dem SVG, Überschrift dazu
        QFile file(QStringLiteral(VEK_SOURCE_DIR "/docs/showcase/plakat.svg"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        AiTools::Result r = run(QStringLiteral("svg_einfuegen"), {{QStringLiteral("svg"), QString::fromUtf8(file.readAll())},
                                                                  {QStringLiteral("x"), 0}, {QStringLiteral("y"), 0},
                                                                  {QStringLiteral("breite"), 210}, {QStringLiteral("name"), QStringLiteral("Plakat")}});
        QVERIFY2(!r.error, qPrintable(r.text));
        r = run(QStringLiteral("skript"), {{QStringLiteral("code"), QStringLiteral(
            "var t = tg.text(18, 40, 'SOMMERNACHT', 46, 'Arial Black'); t.fill = '#fff3e0'; t.name = 'Titel';"
            "var s = tg.text(20, 63, 'Open Air am See · 21. Juni 2027', 16, 'Arial'); s.fill = '#ffd9b0';"
            "var b = tg.rectangle(20, 270, 72, 13, 6.5); b.fill = '#ffd166'; b.stroke = null;"
            "var k = tg.text(29, 273.4, 'TICKETS AB 29 CHF', 12, 'Arial Black'); k.fill = '#2a1f45'; tg.select([t]); t.id")}});
        QVERIFY2(!r.error, qPrintable(r.text));
        window.canvas()->fitPage();
        QTest::qWait(300);
        window.grab().save(dir + QStringLiteral("/01-hauptfenster.png"));

        // 2. KI-Assistent (live) oder nur das Fenster; der Objekt-Manager macht dem KI-Fenster Platz
        for (QDockWidget* dock : window.findChildren<QDockWidget*>())
            dock->setVisible(dock->objectName() == QLatin1String("ai"));
        if (auto* aiDock = window.findChild<QDockWidget*>(QStringLiteral("ai")))
            window.resizeDocks({aiDock}, {470}, Qt::Horizontal);
        showDock(QStringLiteral("KI"));
        auto* panel = window.findChild<AiPanel*>();
        if (qEnvironmentVariable("VEK_AI_LIVE") == QLatin1String("1")) {
            ai::saveAccounts({{QStringLiteral("cc"), QStringLiteral("claude_code"), QString(), QString(), QString(), QStringLiteral("sonnet")}});
            Options::instance().set(QStringLiteral("global/ai/account"), QStringLiteral("cc"));
            panel->reloadAccounts();
            panel->chat().reset();
            bool finished = false;
            connect(&panel->chat(), &AiChat::finished, this, [&] { finished = true; });
            connect(&panel->chat(), &AiChat::failed, this, [&] { finished = true; });
            panel->findChild<QCheckBox*>(QStringLiteral("ai-maywrite"))->setChecked(true);
            panel->send(QStringLiteral("Setz über den See drei kleine leuchtende Sterne in den Himmel (gelb, je etwa 6 mm) "
                                       "und einen dezenten Mond links oben neben dem Titel. Schau dir das Ergebnis an."));
            QTRY_VERIFY_WITH_TIMEOUT(finished, 300000);
        }
        ed.clearSelection();
        QTest::qWait(300);
        window.grab().save(dir + QStringLiteral("/02-ki-assistent.png"));

        // 3. SuperTrace: das Plakat als Bitmap nachzeichnen
        output::ExportOptions o;
        o.dpi = 60;
        const QImage bitmap = output::renderImage(*window.canvas(), o);
        TraceDialog trace(bitmap, trace::Preset::Clipart, &window);
        trace.resize(1300, 820);
        trace.show();
        QTest::qWait(4000);
        trace.grab().save(dir + QStringLiteral("/03-supertrace.png"));
        trace.close();

        // 4. Bildersuche (Wikimedia Commons, Icons8)
        if (qEnvironmentVariable("VEK_NETWORK_TESTS") == QLatin1String("1")) {
            ImageSearchDialog search(&window);
            search.resize(1100, 760);
            search.show();
            search.setTerm(QStringLiteral("Matterhorn"));
            search.search();
            QTRY_VERIFY_WITH_TIMEOUT(search.visibleCount() > 8, 30000);
            QTest::qWait(6000);
            if (auto* list = search.findChild<QListWidget*>())
                list->setCurrentRow(1);
            search.grab().save(dir + QStringLiteral("/04-bildersuche.png"));
        }

        // 5. KI-Zugänge
        OptionsDialog options(ed, &window);
        options.resize(1000, 640);
        options.selectPage(QStringLiteral("Global/KI"));
        options.show();
        QTest::qWait(200);
        options.grab().save(dir + QStringLiteral("/05-ki-zugaenge.png"));
        options.hide();

        // 6. Andockfenster: Objekt-Manager und Objekteigenschaften, Titel ausgewählt
        for (QDockWidget* dock : window.findChildren<QDockWidget*>())
            dock->setVisible(dock->objectName() == QLatin1String("objectManager")
                             || dock->objectName() == QLatin1String("objectProperties"));
        if (auto* manager = window.findChild<QDockWidget*>(QStringLiteral("objectManager")))
            window.resizeDocks({manager}, {380}, Qt::Horizontal);
        r = run(QStringLiteral("skript"), {{QStringLiteral("code"), QStringLiteral(
            "tg.select(tg.objects().filter(function (o) { return o.name == 'Titel'; })); tg.selection().length")}});
        QVERIFY2(!r.error, qPrintable(r.text));
        window.canvas()->fitPage();
        QTest::qWait(300);
        window.grab().save(dir + QStringLiteral("/06-andockfenster.png"));
        ed.clearSelection();

        // 7. Ausschiessen: das Plakat als Mehrfachnutzen auf einem A3-Bogen
        {
            QSettings().remove(QStringLiteral("impose"));
            ImpositionDialog impose(*window.canvas(), QStringLiteral("Plakat"), &window);
            impose.layoutBox()->setCurrentIndex(impose.layoutBox()->findData(int(impose::Layout::StepRepeat)));
            impose.paperBox()->setCurrentIndex(impose.paperBox()->findText(QStringLiteral("A3"), Qt::MatchStartsWith));
            impose.resize(1300, 700);
            impose.show();
            QTest::qWait(500);
            impose.grab().save(dir + QStringLiteral("/07-ausschiessen.png"));
        }

        // 8. Optionen: Einrasten an Objekten
        options.selectPage(QStringLiteral("Arbeitsbereich/An Objekten ausrichten"));
        options.show();
        QTest::qWait(200);
        options.grab().save(dir + QStringLiteral("/08-optionen.png"));
        Options::instance().resetWorkspace();
    }

    // Chatfenster mit Claude über das Abo: Bild suchen, einfügen, Text dazu (VEK_AI_LIVE=1)
    void aiPanelClaudeCodeLive()
    {
        if (qEnvironmentVariable("VEK_AI_LIVE") != QLatin1String("1"))
            QSKIP("KI-Test: VEK_AI_LIVE=1 setzen");
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        ai::saveAccounts({{QStringLiteral("cc"), QStringLiteral("claude_code"), QString(), QString(), QString(), QStringLiteral("sonnet")}});
        Options::instance().set(QStringLiteral("global/ai/account"), QStringLiteral("cc"));
        MainWindow window;
        window.resize(1500, 950);
        Editor& ed = window.canvas()->editor();
        auto* panel = window.findChild<AiPanel*>();
        panel->reloadAccounts();
        panel->chat().reset();
        bool finished = false;
        QString summary;
        connect(&panel->chat(), &AiChat::finished, this, [&](const QString& s) {
            summary = s;
            finished = true;
        });
        connect(&panel->chat(), &AiChat::failed, this, [&](const QString& s) {
            summary = QStringLiteral("FEHLER ") + s;
            finished = true;
        });
        panel->findChild<QCheckBox*>(QStringLiteral("ai-maywrite"))->setChecked(true);
        panel->send(QStringLiteral("Suche ein freies Foto vom Munot in Schaffhausen, füge es oben links bei 20/20 mm mit 120 mm Breite "
                                   "ein und schreib darunter in 18 pt den Titel „Munot, Schaffhausen“."));
        QTRY_VERIFY_WITH_TIMEOUT(finished, 300000);
        qInfo().noquote() << summary;
        qInfo().noquote() << panel->transcriptText().right(1500);
        QVERIFY2(!summary.startsWith(QStringLiteral("FEHLER")), qPrintable(summary));
        bool image = false, text = false;
        for (const DrawObject* o : ed.pickableObjects()) {
            if (dynamic_cast<const ImageObject*>(o) && std::abs(o->boundingRect().width() - 120) < 5)
                image = true;
            if (const auto* t = dynamic_cast<const TextObject*>(o); t && t->plainText().contains(QStringLiteral("Munot")))
                text = true;
        }
        QVERIFY(image);
        QVERIFY(text);
        QVERIFY(ed.document().undoText().startsWith(QStringLiteral("KI: ")));
        const QString dir = qEnvironmentVariable("VEK_SCREENSHOT_DIR");
        if (!dir.isEmpty()) {
            for (QDockWidget* dock : window.findChildren<QDockWidget*>())
                if (dock->objectName() == QLatin1String("ai")) {
                    dock->show();
                    dock->raise();
                }
            QTest::qWait(300);
            window.grab().save(dir + QStringLiteral("/60-ki-munot.png"));
        }
        Options::instance().resetWorkspace();
    }

    // Agentenschleife mit einem nachgestellten OpenAI-kompatiblen Server: Runde 1 ruft "skript",
    // Runde 2 antwortet; das Chatfenster zeigt Frage, Schritt und Antwort; ein Strg+Z nimmt alles zurück
    void aiChatLoopWithFakeProvider()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        QTcpServer fake;
        QVERIFY(fake.listen(QHostAddress::LocalHost, 0));
        QList<QJsonObject> requests;
        connect(&fake, &QTcpServer::newConnection, this, [&] {
            QTcpSocket* socket = fake.nextPendingConnection();
            auto buffer = std::make_shared<QByteArray>();
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket, buffer] {
                *buffer += socket->readAll();
                const qsizetype head = buffer->indexOf("\r\n\r\n");
                if (head < 0)
                    return;
                const QByteArray headers = buffer->left(head).toLower();
                const qsizetype at = headers.indexOf("content-length:");
                const int length = headers.mid(at + 15, headers.indexOf('\r', at) - at - 15).trimmed().toInt();
                if (buffer->size() < head + 4 + length)
                    return;
                const QJsonObject body = QJsonDocument::fromJson(buffer->mid(head + 4, length)).object();
                buffer->clear();
                requests << body;
                QByteArray sse;
                if (requests.size() == 1)
                    sse = "data: {\"choices\":[{\"delta\":{\"content\":\"Ich zeichne es.\"}}]}\n\n"
                          "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"c1\",\"function\":{\"name\":\"skript\","
                          "\"arguments\":\"{\\\"code\\\":\\\"var r = tg.rectangle(20,20,30,30); r.fill='#0000ff'; r.id\\\"}\"}}]},\"finish_reason\":\"tool_calls\"}]}\n\n"
                          "data: {\"choices\":[],\"usage\":{\"prompt_tokens\":100,\"completion_tokens\":20}}\n\ndata: [DONE]\n\n";
                else
                    sse = "data: {\"choices\":[{\"delta\":{\"content\":\"Fertig: blaues **Quadrat**.\"},\"finish_reason\":\"stop\"}]}\n\n"
                          "data: {\"choices\":[],\"usage\":{\"prompt_tokens\":150,\"completion_tokens\":10}}\n\ndata: [DONE]\n\n";
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nContent-Length: " + QByteArray::number(sse.size()) +
                              "\r\nConnection: close\r\n\r\n" + sse);
                socket->disconnectFromHost();
            });
        });
        const ai::Account fakeAccount{QStringLiteral("f1"), QStringLiteral("litellm"), QStringLiteral("Testserver"),
                                      QStringLiteral("http://127.0.0.1:%1/v1").arg(fake.serverPort()), QString(), QStringLiteral("test-modell")};
        ai::saveAccounts({fakeAccount});
        Options::instance().set(QStringLiteral("global/ai/account"), QStringLiteral("f1"));

        MainWindow window;
        Editor& ed = window.canvas()->editor();
        auto* panel = window.findChild<AiPanel*>();
        QVERIFY(panel);
        panel->reloadAccounts();
        panel->chat().reset();
        QCOMPARE(panel->chat().account().id, QStringLiteral("f1"));
        const int undoBefore = ed.document().undoCount();
        const int objectsBefore = int(ed.pickableObjects().size());
        bool finished = false;
        QString summary;
        connect(&panel->chat(), &AiChat::finished, this, [&](const QString& s) {
            summary = s;
            finished = true;
        });
        connect(&panel->chat(), &AiChat::failed, this, [&](const QString& s) {
            summary = s;
            finished = true;
        });
        panel->findChild<QCheckBox*>(QStringLiteral("ai-maywrite"))->setChecked(true);
        panel->send(QStringLiteral("Zeichne ein blaues Quadrat"));
        QTRY_VERIFY_WITH_TIMEOUT(finished, 20000);
        QVERIFY2(summary.contains(QStringLiteral("test-modell")), qPrintable(summary));
        QVERIFY(summary.contains(QStringLiteral("1 Schritt")));
        QCOMPARE(requests.size(), 2);
        // Runde 1: System, Frage, Werkzeuge; Runde 2 zusätzlich Werkzeugaufruf und Ergebnis
        QCOMPARE(requests[0].value(QStringLiteral("messages")).toArray()[0].toObject().value(QStringLiteral("role")).toString(), QStringLiteral("system"));
        QVERIFY(requests[0].value(QStringLiteral("messages")).toArray()[0].toObject().value(QStringLiteral("content")).toString()
                    .contains(QStringLiteral("TANGENTA")));
        QVERIFY(!requests[0].value(QStringLiteral("tools")).toArray().isEmpty());
        const QJsonArray second = requests[1].value(QStringLiteral("messages")).toArray();
        QCOMPARE(second.last().toObject().value(QStringLiteral("role")).toString(), QStringLiteral("tool"));
        QVERIFY(second.last().toObject().value(QStringLiteral("content")).toString().contains(QStringLiteral("Wert: ")));
        // Ergebnis im Dokument, ein einziger Rückgängig-Schritt
        QCOMPARE(int(ed.pickableObjects().size()), objectsBefore + 1);
        QCOMPARE(ed.document().undoCount(), undoBefore + 1);
        QVERIFY(ed.document().undoText().startsWith(QStringLiteral("KI: Zeichne ein blaues")));
        // Verlauf im Fenster
        const QString shown = panel->transcriptText();
        QVERIFY(shown.contains(QStringLiteral("Zeichne ein blaues Quadrat")));
        QVERIFY(shown.contains(QStringLiteral("Zeichnen (Skript)")));
        QVERIFY(shown.contains(QStringLiteral("Fertig: blaues Quadrat.")));
        const QString dir = qEnvironmentVariable("VEK_SCREENSHOT_DIR");
        if (!dir.isEmpty()) {
            window.resize(1400, 900);
            for (QDockWidget* dock : window.findChildren<QDockWidget*>())
                if (dock->objectName() == QLatin1String("ai")) {
                    dock->show();
                    dock->raise();
                }
            QTest::qWait(200);
            window.grab().save(dir + QStringLiteral("/59-ki-fenster.png"));
            QWidget shot;
            panel->setParent(&shot);
            shot.resize(440, 760);
            panel->resize(440, 760);
            panel->show();
            QTest::qWait(100);
            panel->grab().save(dir + QStringLiteral("/61-ki-panel.png"));
            panel->setParent(nullptr);
            window.findChild<QDockWidget*>(QStringLiteral("ai"))->setWidget(panel);
        }
        // Ohne "darf ändern": Schreibwerkzeuge werden gar nicht angeboten
        requests.clear();
        finished = false;
        panel->findChild<QCheckBox*>(QStringLiteral("ai-maywrite"))->setChecked(false);
        panel->send(QStringLiteral("Und noch eins"));
        QTRY_VERIFY_WITH_TIMEOUT(finished, 20000);
        QStringList offered;
        for (const QJsonValue& v : requests[0].value(QStringLiteral("tools")).toArray())
            offered << v.toObject().value(QStringLiteral("function")).toObject().value(QStringLiteral("name")).toString();
        QVERIFY(!offered.contains(QStringLiteral("skript")));
        QVERIFY(offered.contains(QStringLiteral("dokument_ansehen")));
        // Frühere Frage geht als Text mit
        QVERIFY(QJsonDocument(requests[0]).toJson().contains("Zeichne ein blaues Quadrat"));
        // Zurücksetzen auf die Anfangszustände
        ed.document().undo();
        QCOMPARE(int(ed.pickableObjects().size()), objectsBefore);
        panel->chat().reset();
        Options::instance().resetWorkspace();
    }

    // Werkzeuge der KI am echten Hauptfenster, Rückgängig als ein Schritt, MCP-Schnittstelle
    void aiToolsAndMcp()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        MainWindow window;
        Editor& ed = window.canvas()->editor();
        AiTools tools(ed, *window.canvas(), &window);
        auto run = [&](const QString& name, const QJsonObject& input, bool mayWrite = true) {
            AiTools::Result result;
            bool got = false;
            tools.call(name, input, mayWrite, [&](const AiTools::Result& r) {
                result = r;
                got = true;
            });
            if (!QTest::qWaitFor([&] { return got; }, 20000))
                result = {QStringLiteral("keine Antwort"), {}, true};
            return result;
        };
        auto names = [](const QList<ai::Tool>& list) {
            QStringList out;
            for (const ai::Tool& t : list)
                out << t.name;
            return out;
        };
        QVERIFY(names(tools.tools(false)).contains(QStringLiteral("dokument_ansehen")));
        QVERIFY(!names(tools.tools(false)).contains(QStringLiteral("skript")));
        QVERIFY(names(tools.tools(true)).contains(QStringLiteral("skript")));
        QVERIFY(run(QStringLiteral("skript"), {{QStringLiteral("code"), QStringLiteral("1")}}, false).error); // ohne "darf ändern"

        // Zeichnen per Skript; die ganze KI-Antwort wird EIN Rückgängig-Schritt
        const std::size_t mark = ed.document().undoMark();
        const int undoBefore = ed.document().undoCount();
        AiTools::Result r = run(QStringLiteral("skript"),
                                {{QStringLiteral("code"), QStringLiteral("var r = tg.rectangle(10, 20, 50, 30); r.fill = '#ff0000'; r.name = 'Rot'; r.id")}});
        QVERIFY2(!r.error, qPrintable(r.text));
        const int rectId = r.text.section(QStringLiteral("Wert: "), 1).section(QLatin1Char('\n'), 0, 0).toInt();
        QVERIFY(rectId > 0);
        r = run(QStringLiteral("skript"), {{QStringLiteral("code"), QStringLiteral("tg.nichtDa(")}});
        QVERIFY(r.error);
        QVERIFY(r.text.contains(QStringLiteral("nichts geändert")));

        r = run(QStringLiteral("dokument_ansehen"), {});
        QVERIFY2(!r.error, qPrintable(r.text));
        const QJsonObject doc = QJsonDocument::fromJson(r.text.section(QLatin1Char('{'), 0, 0).isEmpty() ? r.text.toUtf8()
                                                                                                         : r.text.mid(r.text.indexOf(QLatin1Char('{'))).toUtf8()).object();
        QCOMPARE(doc.value(QStringLiteral("seiten")).toInt(), 1);
        QJsonObject rect;
        for (const QJsonValue& v : doc.value(QStringLiteral("objekte")).toArray())
            if (v.toObject().value(QStringLiteral("id")).toInt() == rectId)
                rect = v.toObject();
        QCOMPARE(rect.value(QStringLiteral("fuellung")).toString(), QStringLiteral("#ff0000"));
        QCOMPARE(rect.value(QStringLiteral("breite")).toDouble(), 50.0);
        QCOMPARE(rect.value(QStringLiteral("name")).toString(), QStringLiteral("Rot"));
        QCOMPARE(ed.document().undoCount(), undoBefore + 1); // Lesen legt keinen Schritt an
        QVERIFY(!run(QStringLiteral("objekt_ansehen"), {{QStringLiteral("id"), rectId}}).error);

        // SVG an Ort und Stelle, Breite mit Seitenverhältnis
        r = run(QStringLiteral("svg_einfuegen"),
                {{QStringLiteral("svg"), QStringLiteral("<svg xmlns='http://www.w3.org/2000/svg' width='20' height='10'><rect width='20' height='10' fill='#0000ff'/></svg>")},
                 {QStringLiteral("x"), 100}, {QStringLiteral("y"), 50}, {QStringLiteral("breite"), 40}, {QStringLiteral("name"), QStringLiteral("Blau")}});
        QVERIFY2(!r.error, qPrintable(r.text));
        QCOMPARE(qRound(ed.selectionBounds().width()), 40);
        QCOMPARE(qRound(ed.selectionBounds().height()), 20);
        QCOMPARE(qRound(ed.selectionBounds().left()), 100);
        const NodeId svgId = ed.selection().first();

        // Menübefehle
        const QStringList commands = tools.commands();
        QVERIFY(!commands.isEmpty());
        QString group;
        for (const QString& c : commands)
            if (c.endsWith(QStringLiteral("> Gruppieren")))
                group = c;
        QVERIFY2(!group.isEmpty(), qPrintable(commands.join(u'|')));
        QVERIFY(std::none_of(commands.begin(), commands.end(), [](const QString& c) { return c.endsWith(QStringLiteral("…")); }));
        r = run(QStringLiteral("befehl"), {{QStringLiteral("befehl"), QStringLiteral("Gruppieren")},
                                           {QStringLiteral("ids"), QJsonArray{rectId, int(svgId)}}});
        QVERIFY2(!r.error, qPrintable(r.text));
        QCOMPARE(ed.selection().size(), 1);
        QVERIFY(ed.document().findAs<GroupObject>(ed.selection().first()));
        QVERIFY(run(QStringLiteral("befehl"), {{QStringLiteral("befehl"), QStringLiteral("Gibtsnicht")}}).error);

        // Seite als Bild
        r = run(QStringLiteral("seite_ansehen"), {{QStringLiteral("breite_px"), 600}});
        QVERIFY2(!r.error, qPrintable(r.text));
        QCOMPARE(r.images.size(), 1);
        const QImage view = QImage::fromData(r.images.first());
        QVERIFY(std::max(view.width(), view.height()) <= 600 && std::max(view.width(), view.height()) >= 590);

        // Nachzeichnen eines Bitmaps
        QImage bitmap(80, 80, QImage::Format_RGB32);
        bitmap.fill(Qt::white);
        {
            QPainter p(&bitmap);
            p.fillRect(20, 20, 40, 40, Qt::black);
        }
        const NodeId imageId = ed.importImage(bitmap);
        r = run(QStringLiteral("nachzeichnen"), {{QStringLiteral("id"), int(imageId)}, {QStringLiteral("art"), QStringLiteral("strichgrafik")}});
        QVERIFY2(!r.error, qPrintable(r.text));
        QVERIFY(r.text.contains(QStringLiteral("Formen")));

        // Export als PNG, kein Überschreiben ohne Erlaubnis
        const QString file = QDir::temp().filePath(QStringLiteral("tangenta-ki-export.png"));
        QFile::remove(file);
        r = run(QStringLiteral("exportieren"), {{QStringLiteral("pfad"), file}, {QStringLiteral("dpi"), 50}});
        QVERIFY2(!r.error, qPrintable(r.text));
        QVERIFY(QFile::exists(file));
        QVERIFY(run(QStringLiteral("exportieren"), {{QStringLiteral("pfad"), file}}).error);
        QFile::remove(file);
        QVERIFY(run(QStringLiteral("seite_wechseln"), {{QStringLiteral("seite"), 9}}).error);

        // Alles seit der Marke wird ein Schritt; Rückgängig nimmt die ganze KI-Antwort zurück
        QVERIFY(ed.document().mergeUndo(mark, QStringLiteral("KI: Test")));
        QCOMPARE(ed.document().undoCount(), undoBefore + 1);
        QCOMPARE(ed.document().undoText(), QStringLiteral("KI: Test"));
        ed.document().undo();
        QVERIFY(!ed.document().find(NodeId(rectId)));
        QVERIFY(!ed.document().find(imageId));
        ed.document().redo();
        QVERIFY(ed.document().find(NodeId(rectId)));

        // MCP: JSON-RPC direkt und über HTTP mit Token
        McpServer server(tools, [] { return true; });
        auto rpc = [&](const QJsonObject& msg) {
            int status = 0;
            QByteArray body;
            bool got = false;
            server.handle(QJsonDocument(msg).toJson(), [&](int s, const QByteArray& b) {
                status = s;
                body = b;
                got = true;
            });
            (void)QTest::qWaitFor([&] { return got; }, 20000);
            return std::make_pair(status, QJsonDocument::fromJson(body).object());
        };
        auto [s1, init] = rpc({{QStringLiteral("jsonrpc"), QStringLiteral("2.0")}, {QStringLiteral("id"), 1}, {QStringLiteral("method"), QStringLiteral("initialize")},
                               {QStringLiteral("params"), QJsonObject{{QStringLiteral("protocolVersion"), QStringLiteral("2025-06-18")}}}});
        QCOMPARE(s1, 200);
        QCOMPARE(init.value(QStringLiteral("result")).toObject().value(QStringLiteral("protocolVersion")).toString(), QStringLiteral("2025-06-18"));
        QCOMPARE(rpc({{QStringLiteral("jsonrpc"), QStringLiteral("2.0")}, {QStringLiteral("method"), QStringLiteral("notifications/initialized")}}).first, 202);
        auto [s2, listed] = rpc({{QStringLiteral("jsonrpc"), QStringLiteral("2.0")}, {QStringLiteral("id"), 2}, {QStringLiteral("method"), QStringLiteral("tools/list")}});
        QStringList mcpTools;
        for (const QJsonValue& v : listed.value(QStringLiteral("result")).toObject().value(QStringLiteral("tools")).toArray())
            mcpTools << v.toObject().value(QStringLiteral("name")).toString();
        QVERIFY(mcpTools.contains(QStringLiteral("skript")));
        QVERIFY(mcpTools.contains(QStringLiteral("seite_ansehen")));
        auto [s3, called] = rpc({{QStringLiteral("jsonrpc"), QStringLiteral("2.0")}, {QStringLiteral("id"), 3}, {QStringLiteral("method"), QStringLiteral("tools/call")},
                                 {QStringLiteral("params"), QJsonObject{{QStringLiteral("name"), QStringLiteral("seite_ansehen")},
                                                                        {QStringLiteral("arguments"), QJsonObject{{QStringLiteral("breite_px"), 300}}}}}});
        const QJsonArray content = called.value(QStringLiteral("result")).toObject().value(QStringLiteral("content")).toArray();
        QCOMPARE(content.size(), 2);
        QCOMPARE(content[1].toObject().value(QStringLiteral("type")).toString(), QStringLiteral("image"));
        QVERIFY(!called.value(QStringLiteral("result")).toObject().value(QStringLiteral("isError")).toBool());

        QVERIFY(server.start());
        QVERIFY(server.url().startsWith(QStringLiteral("http://127.0.0.1:")));
        QNetworkAccessManager net;
        auto post = [&](bool withToken) {
            QNetworkRequest request{QUrl(server.url())};
            request.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/json"));
            if (withToken)
                request.setRawHeader("Authorization", "Bearer " + server.token().toLatin1());
            QNetworkReply* reply = net.post(request, QByteArrayLiteral(R"({"jsonrpc":"2.0","id":7,"method":"ping"})"));
            (void)QTest::qWaitFor([reply] { return reply->isFinished(); }, 10000);
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            reply->deleteLater();
            return status;
        };
        QCOMPARE(post(true), 200);
        QCOMPARE(post(false), 401);
    }

    // Claude Code zeichnet über die MCP-Schnittstelle (eigenes Abo): nur mit VEK_AI_LIVE=1
    void aiClaudeCodeDrawsLive()
    {
        if (qEnvironmentVariable("VEK_AI_LIVE") != QLatin1String("1"))
            QSKIP("KI-Test: VEK_AI_LIVE=1 setzen");
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        MainWindow window;
        Editor& ed = window.canvas()->editor();
        AiTools tools(ed, *window.canvas(), &window);
        McpServer server(tools, [] { return true; });
        QVERIFY(server.start());
        QStringList used;
        connect(&server, &McpServer::toolStarted, this, [&](const QString& name) { used << name; });
        ClaudeCode cc;
        QString answer, error;
        bool done = false;
        connect(&cc, &ClaudeCode::finished, this, [&](const QString& a) {
            answer = a;
            done = true;
        });
        connect(&cc, &ClaudeCode::failed, this, [&](const QString& e) {
            error = e;
            done = true;
        });
        ClaudeCode::Request request;
        request.system = QStringLiteral("Du arbeitest in TANGENTA, einem Vektorgrafikprogramm. Benutze die Werkzeuge. Antworte kurz auf Deutsch.");
        request.model = QStringLiteral("sonnet");
        request.mcpUrl = server.url();
        request.mcpToken = server.token();
        request.keepSession = false;
        request.directory = QDir::temp().filePath(QStringLiteral("tangenta-ki"));
        cc.ask(QStringLiteral("Zeichne einen roten Kreis mit 40 mm Durchmesser genau in die Mitte der Seite."), request);
        QTRY_VERIFY_WITH_TIMEOUT(done, 240000);
        qInfo().noquote() << "Werkzeuge:" << used << "Antwort:" << answer.left(300) << error;
        QVERIFY2(error.isEmpty(), qPrintable(error));
        const Page& page = ed.currentPage();
        bool circle = false;
        for (const DrawObject* o : ed.pickableObjects()) {
            const QRectF b = o->boundingRect();
            if (std::abs(b.width() - 40) < 3 && std::abs(b.height() - 40) < 3 && std::abs(b.center().x() - page.size().width() / 2) < 3)
                circle = true;
        }
        QVERIFY2(circle, "kein Kreis von 40 mm in der Seitenmitte");
        const QString dir = qEnvironmentVariable("VEK_SCREENSHOT_DIR");
        if (!dir.isEmpty())
            window.grab().save(dir + QStringLiteral("/58-ki-kreis.png"));
    }

    // KI-Zugänge: Anfragen wie die Anbieter sie erwarten, Stream-Ereignisse, Claude Code, Optionen
    void aiConnectors()
    {
        using namespace ai;
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest")); // sonst kein QSettings
        // Zugänge: JSON hin und zurück, unbekannte Arten fallen weg
        QList<Account> list{{QStringLiteral("a1"), QStringLiteral("claude_code"), QString(), QString(), QString(), QStringLiteral("opus")},
                            {QStringLiteral("a2"), QStringLiteral("claude"), QStringLiteral("Firma"), QString(), QStringLiteral("sk-x"), QString()}};
        QJsonArray json = toJson(list);
        json.append(QJsonObject{{QStringLiteral("id"), QStringLiteral("zz")}, {QStringLiteral("kind"), QStringLiteral("gibtsnicht")}});
        const QList<Account> back = accountsFromJson(json);
        QCOMPARE(back.size(), 2);
        QCOMPARE(back[1].key, QStringLiteral("sk-x"));
        QCOMPARE(back[0].displayName(), QStringLiteral("Claude (Abo über Claude Code)"));
        QCOMPARE(back[1].displayName(), QStringLiteral("Firma"));
        saveAccounts(list);
        Options::instance().set(QStringLiteral("global/ai/account"), QStringLiteral("a2"));
        QCOMPARE(currentAccount().id, QStringLiteral("a2"));
        QVERIFY(!Provider::create(list[0])); // Claude Code läuft nicht über Provider

        // Verlauf: Frage, Antwort mit zwei Werkzeugaufrufen, zwei Ergebnisse (eins mit Bild)
        Message q;
        q.text = QStringLiteral("Zeichne ein Rechteck");
        Message say{Message::Role::Assistant, QStringLiteral("Gern."), {}, {}, {}, false, {}};
        Message use1{Message::Role::ToolUse, {}, QStringLiteral("dokument_ansehen"), {}, QStringLiteral("t1"), false, {}};
        Message use2{Message::Role::ToolUse, {}, QStringLiteral("skript"), QJsonObject{{QStringLiteral("code"), QStringLiteral("x")}},
                     QStringLiteral("t2"), false, {}};
        Message res1{Message::Role::ToolResult, QStringLiteral("1 Seite"), QStringLiteral("dokument_ansehen"), {}, QStringLiteral("t1"), false,
                     {QByteArray("PNG")}};
        Message res2{Message::Role::ToolResult, QStringLiteral("Fehler"), QStringLiteral("skript"), {}, QStringLiteral("t2"), true, {}};
        const QList<Message> history{q, say, use1, use2, res1, res2};
        const QList<ai::Tool> tools{{QStringLiteral("skript"), QStringLiteral("JS"), QJsonObject{}, true}};

        // Anthropic: Text und tool_use in EINER Assistent-Nachricht, Ergebnisse in EINER Benutzer-Nachricht
        std::unique_ptr<Provider> claude(Provider::create(list[1]));
        QJsonObject body = claude->requestBody(QStringLiteral("SYS"), history, tools);
        QCOMPARE(body.value(QStringLiteral("model")).toString(), QStringLiteral("claude-sonnet-5-5"));
        QCOMPARE(body.value(QStringLiteral("system")).toString(), QStringLiteral("SYS"));
        QJsonArray messages = body.value(QStringLiteral("messages")).toArray();
        QCOMPARE(messages.size(), 3);
        const QJsonArray assistant = messages[1].toObject().value(QStringLiteral("content")).toArray();
        QCOMPARE(assistant.size(), 3);
        QCOMPARE(assistant[2].toObject().value(QStringLiteral("input")).toObject().value(QStringLiteral("code")).toString(), QStringLiteral("x"));
        QVERIFY(assistant[1].toObject().value(QStringLiteral("input")).isObject()); // leer = {} statt []
        const QJsonArray results = messages[2].toObject().value(QStringLiteral("content")).toArray();
        QCOMPARE(results.size(), 2);
        QCOMPARE(results[0].toObject().value(QStringLiteral("content")).toArray()[1].toObject().value(QStringLiteral("type")).toString(),
                 QStringLiteral("image"));
        QVERIFY(results[1].toObject().value(QStringLiteral("is_error")).toBool());
        QCOMPARE(body.value(QStringLiteral("tools")).toArray()[0].toObject().value(QStringLiteral("input_schema")).toObject()
                     .value(QStringLiteral("type")).toString(), QStringLiteral("object"));

        // Anthropic-Stream: Text stückweise, Werkzeug mit JSON in Teilen
        QString streamed;
        Reply got;
        connect(claude.get(), &Provider::text, this, [&](const QString& piece) { streamed += piece; });
        connect(claude.get(), &Provider::done, this, [&](const Reply& r) { got = r; });
        claude->round(QStringLiteral("S"), {q}, {}); // setzt den Zustand zurück (Netz wird gleich abgebrochen)
        claude->abort();
        // round()/abort() lassen den Provider "fertig"; feed() ohne laufende Runde testet nur die Auswertung:
        std::unique_ptr<Provider> parser(Provider::create(list[1]));
        connect(parser.get(), &Provider::text, this, [&](const QString& piece) { streamed += piece; });
        connect(parser.get(), &Provider::done, this, [&](const Reply& r) { got = r; });
        parser->feed("event: message_start\ndata: {\"type\":\"message_start\",\"message\":{\"usage\":{\"input_tokens\":12}}}\n\n"
                     "event: content_block_start\ndata: {\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"text\"}}\n\n"
                     "event: content_block_delta\ndata: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"Hal\"}}\n\n"
                     "event: content_block_delta\ndata: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"lo\"}}\n\n"
                     "event: content_block_start\ndata: {\"type\":\"content_block_start\",\"index\":1,\"content_block\":{\"type\":\"tool_use\",\"id\":\"tu1\",\"name\":\"skript\"}}\n\n"
                     "event: content_block_delta\ndata: {\"type\":\"content_block_delta\",\"index\":1,\"delta\":{\"type\":\"input_json_delta\",\"partial_json\":\"{\\\"co\"}}\n\n"
                     "event: content_block_delta\ndata: {\"type\":\"content_block_delta\",\"index\":1,\"delta\":{\"type\":\"input_json_delta\",\"partial_json\":\"de\\\":\\\"1\\\"}\"}}\n\n"
                     "event: content_block_stop\ndata: {\"type\":\"content_block_stop\",\"index\":1}\n\n"
                     "event: message_delta\ndata: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"tool_use\"},\"usage\":{\"output_tokens\":7}}\n\n");
        QVERIFY(got.stop.isEmpty()); // noch nicht fertig
        parser->feed("event: message_stop\ndata: {\"type\":\"message_stop\"}\n\n");
        QCOMPARE(streamed, QStringLiteral("Hallo"));
        QCOMPARE(got.text, QStringLiteral("Hallo"));
        QCOMPARE(got.stop, QStringLiteral("tools"));
        QCOMPARE(got.calls.size(), 1);
        QCOMPARE(got.calls[0].input.value(QStringLiteral("code")).toString(), QStringLiteral("1"));
        QCOMPARE(got.tokensIn, 12);
        QCOMPARE(got.tokensOut, 7);

        // OpenAI-kompatibel: Werkzeugaufrufe am Assistenten, je Ergebnis eine tool-Nachricht, Bild danach als Benutzer
        const Account openai{QStringLiteral("o"), QStringLiteral("openai"), QString(), QString(), QStringLiteral("k"), QStringLiteral("gpt-4o")};
        std::unique_ptr<Provider> gpt(Provider::create(openai));
        body = gpt->requestBody(QStringLiteral("SYS"), history, tools);
        messages = body.value(QStringLiteral("messages")).toArray();
        QCOMPARE(messages[0].toObject().value(QStringLiteral("role")).toString(), QStringLiteral("system"));
        QCOMPARE(messages[2].toObject().value(QStringLiteral("tool_calls")).toArray().size(), 2);
        QCOMPARE(messages[3].toObject().value(QStringLiteral("role")).toString(), QStringLiteral("tool"));
        QVERIFY(messages[4].toObject().value(QStringLiteral("content")).toString().startsWith(QStringLiteral("FEHLER: ")));
        QCOMPARE(messages[5].toObject().value(QStringLiteral("role")).toString(), QStringLiteral("user")); // Bild
        QCOMPARE(messages.size(), 6);
        // DeepSeek sieht keine Bilder
        std::unique_ptr<Provider> deep(Provider::create({QStringLiteral("d"), QStringLiteral("deepseek"), {}, {}, QStringLiteral("k"), {}}));
        QCOMPARE(deep->requestBody(QStringLiteral("S"), history, tools).value(QStringLiteral("messages")).toArray().size(), 5);
        Reply gotGpt;
        connect(gpt.get(), &Provider::done, this, [&](const Reply& r) { gotGpt = r; });
        gpt->feed("data: {\"choices\":[{\"delta\":{\"content\":\"Ok\"}}]}\n\n"
                  "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"c1\",\"function\":{\"name\":\"skript\",\"arguments\":\"{\\\"co\"}}]}}]}\n\n"
                  "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":\"de\\\":2}\"}}]},\"finish_reason\":\"tool_calls\"}]}\n\n"
                  "data: {\"choices\":[],\"usage\":{\"prompt_tokens\":5,\"completion_tokens\":3}}\n\n"
                  "data: [DONE]\n\n");
        QCOMPARE(gotGpt.text, QStringLiteral("Ok"));
        QCOMPARE(gotGpt.calls.size(), 1);
        QCOMPARE(gotGpt.calls[0].id, QStringLiteral("c1"));
        QCOMPARE(gotGpt.calls[0].input.value(QStringLiteral("code")).toInt(), 2);
        QCOMPARE(gotGpt.stop, QStringLiteral("tools"));
        QCOMPARE(gotGpt.tokensIn, 5);

        // Verständliche Fehler
        QVERIFY(explainFailure(QStringLiteral("Claude"), 401, R"({"error":{"message":"invalid x-api-key"}})", QString())
                    .contains(QStringLiteral("„invalid x-api-key“")));
        QVERIFY(explainFailure(QStringLiteral("Claude"), 529, "", QString()).contains(QStringLiteral("überlastet")));
        QVERIFY(explainFailure(QStringLiteral("Ollama"), 0, "", QStringLiteral("Connection refused")).contains(QStringLiteral("Keine Verbindung")));
        QVERIFY(explainFailure(QStringLiteral("Gemini"), 404, "", QString()).contains(QStringLiteral("Modell")));

        // Claude Code: keine eigenen Werkzeuge/Einstellungen, MCP mit Token, Sitzung fortsetzen
        ClaudeCode::Request request;
        request.system = QStringLiteral("SYS");
        request.model = QStringLiteral("sonnet");
        request.mcpUrl = QStringLiteral("http://127.0.0.1:5000/mcp");
        request.mcpToken = QStringLiteral("geheim");
        request.session = QStringLiteral("s-1");
        const QStringList args = ClaudeCode::arguments(QStringLiteral("Frage"), request);
        QCOMPARE(args.at(args.indexOf(QStringLiteral("--tools")) + 1), QString());
        QCOMPARE(args.at(args.indexOf(QStringLiteral("--setting-sources")) + 1), QString());
        QVERIFY(args.contains(QStringLiteral("--strict-mcp-config")));
        QCOMPARE(args.at(args.indexOf(QStringLiteral("--model")) + 1), QStringLiteral("sonnet"));
        QCOMPARE(args.at(args.indexOf(QStringLiteral("--resume")) + 1), QStringLiteral("s-1"));
        QCOMPARE(args.at(args.indexOf(QStringLiteral("--allowedTools")) + 1), QStringLiteral("mcp__tangenta"));
        const QJsonObject mcp = QJsonDocument::fromJson(args.at(args.indexOf(QStringLiteral("--mcp-config")) + 1).toUtf8()).object();
        QCOMPARE(mcp.value(QStringLiteral("mcpServers")).toObject().value(QStringLiteral("tangenta")).toObject()
                     .value(QStringLiteral("headers")).toObject().value(QStringLiteral("Authorization")).toString(),
                 QStringLiteral("Bearer geheim"));
        QVERIFY(!args.contains(QStringLiteral("Frage"))); // Frage kommt über die Standardeingabe

        ClaudeCode cc;
        QString ccText, ccAnswer, ccSession, ccUsage;
        QStringList ccTools;
        connect(&cc, &ClaudeCode::text, this, [&](const QString& p) { ccText += p; });
        connect(&cc, &ClaudeCode::toolComing, this, [&](const QString& n) { ccTools << n; });
        connect(&cc, &ClaudeCode::toolDone, this, [&](const QString& n, bool ok, const QString&) { ccTools << n + (ok ? QStringLiteral(":ok") : QStringLiteral(":fehler")); });
        connect(&cc, &ClaudeCode::usage, this, [&](const QString& u) { ccUsage = u; });
        connect(&cc, &ClaudeCode::finished, this, [&](const QString& a, const QString& s) {
            ccAnswer = a;
            ccSession = s;
        });
        cc.line(R"({"type":"system","subtype":"init","session_id":"abc"})");
        cc.line(R"({"type":"stream_event","event":{"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"Ich zeichne."}}})");
        cc.line(R"({"type":"stream_event","event":{"type":"content_block_start","index":1,"content_block":{"type":"tool_use","id":"u1","name":"mcp__tangenta__skript"}}})");
        cc.line(R"({"type":"user","message":{"content":[{"type":"tool_result","tool_use_id":"u1","content":[{"type":"text","text":"1 Objekt"}]}]}})");
        cc.line(R"({"type":"stream_event","event":{"type":"message_start","message":{}}})");
        cc.line(R"({"type":"stream_event","event":{"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"Fertig."}}})");
        cc.line(R"({"type":"rate_limit_event","rate_limit_info":{"unifiedWindows":{"five_hour":{"utilization":0.12},"seven_day":{"utilization":0.3}}}})");
        cc.line(R"({"type":"result","subtype":"success","is_error":false,"result":"Fertig.","session_id":"abc","usage":{"input_tokens":3,"output_tokens":4}})");
        QCOMPARE(ccTools, QStringList({QStringLiteral("skript"), QStringLiteral("skript:ok")}));
        QCOMPARE(ccAnswer, QStringLiteral("Ich zeichne.\n\nFertig."));
        QCOMPARE(ccSession, QStringLiteral("abc"));
        QVERIFY(ccUsage.contains(QStringLiteral("12 %")));
        QString ccError;
        ClaudeCode cc2;
        connect(&cc2, &ClaudeCode::failed, this, [&](const QString& e) { ccError = e; });
        cc2.line(R"({"type":"result","subtype":"success","is_error":true,"result":"Invalid API key · Please run /login"})");
        QVERIFY(ccError.contains(QStringLiteral("nicht angemeldet")));

        // Web: lesbarer Text ohne Skripte, kein Zugriff aufs eigene Netz
        QString title;
        const QString text = WebSearch::htmlToText(QStringLiteral("<html><head><title>Munot &amp; Rhein</title><script>var x=1;</script>"
                                                                  "<style>p{}</style></head><body><nav>Menü</nav><h1>Munot</h1><p>Rundbau "
                                                                  "aus dem 16. Jahrhundert.</p></body></html>"), &title);
        QCOMPARE(title, QStringLiteral("Munot & Rhein"));
        QVERIFY(text.contains(QStringLiteral("Rundbau aus dem 16. Jahrhundert.")));
        QVERIFY(!text.contains(QStringLiteral("var x")));
        QVERIFY(!text.contains(QStringLiteral("Menü")));
        QVERIFY(WebSearch::isPrivate(QUrl(QStringLiteral("http://127.0.0.1:8080/x"))));
        QVERIFY(WebSearch::isPrivate(QUrl(QStringLiteral("http://192.168.42.10/"))));
        QVERIFY(WebSearch::isPrivate(QUrl(QStringLiteral("http://localhost/"))));
        QVERIFY(!WebSearch::isPrivate(QUrl(QStringLiteral("https://www.munot.ch/"))));

        // Optionen > Global > KI: Liste, Knöpfe; Anmelden nur bei Claude Code
        {
            OptionsDialog options(*m_editor, nullptr);
            QVERIFY(options.selectPage(QStringLiteral("Global/KI")));
            auto* accountsList = options.findChild<QListWidget*>(QStringLiteral("ai-accounts"));
            QVERIFY(accountsList);
            QCOMPARE(accountsList->count(), 2);
            QVERIFY(accountsList->item(1)->text().startsWith(QStringLiteral("★ Firma")));
            accountsList->setCurrentRow(0);
            QVERIFY(options.findChild<QPushButton*>(QStringLiteral("ai-login"))->isEnabled());
            accountsList->setCurrentRow(1);
            QVERIFY(!options.findChild<QPushButton*>(QStringLiteral("ai-login"))->isEnabled());
            options.findChild<QPushButton*>(QStringLiteral("ai-default"))->click();
            options.findChild<QPushButton*>(QStringLiteral("ai-remove"))->click();
            QCOMPARE(accountsList->count(), 1);
            QVERIFY(options.selectPage(QStringLiteral("Global/Websuche")));
            QVERIFY(options.findChild<QPushButton*>(QStringLiteral("web-test")));
            const QString dir = qEnvironmentVariable("VEK_SCREENSHOT_DIR");
            if (!dir.isEmpty()) {
                options.selectPage(QStringLiteral("Global/KI"));
                options.grab().save(dir + QStringLiteral("/57-ki-zugaenge.png"));
            }
            options.accept();
            QCOMPARE(accounts().size(), 1);
        }
        Options::instance().resetWorkspace();
    }

    // Echter Abruf über Claude Code mit dem eigenen Abo: nur mit VEK_AI_LIVE=1
    void aiClaudeCodeLive()
    {
        if (qEnvironmentVariable("VEK_AI_LIVE") != QLatin1String("1"))
            QSKIP("KI-Test: VEK_AI_LIVE=1 setzen");
        bool found = false, loggedIn = false, answered = false;
        QString info;
        ClaudeCode::status(this, [&](bool f, bool l, const QString& i) {
            found = f;
            loggedIn = l;
            info = i;
            answered = true;
        });
        QTRY_VERIFY_WITH_TIMEOUT(answered, 30000);
        qInfo().noquote() << "Status:" << info;
        if (!found || !loggedIn)
            QSKIP("Claude Code fehlt oder ist nicht angemeldet");
        ClaudeCode cc;
        bool ok = false;
        QString message;
        answered = false;
        cc.test(QStringLiteral("haiku"), [&](bool o, const QString& m) {
            ok = o;
            message = m;
            answered = true;
        });
        QTRY_VERIFY_WITH_TIMEOUT(answered, 120000);
        qInfo().noquote() << message;
        QVERIFY2(ok, qPrintable(message));
        QVERIFY(message.contains(QStringLiteral("bereit"), Qt::CaseInsensitive));
    }

    void superTraceDialogClassic()
    {
        QImage source(320, 320, QImage::Format_RGB32);
        source.fill(Qt::white);
        {
            QPainter p(&source);
            p.setRenderHint(QPainter::Antialiasing);
            QPainterPath monitor;
            monitor.addRoundedRect(QRectF(10.3, 20.2, 300, 230), 20, 20);
            monitor.addRect(QRectF(32.3, 45.2, 256, 152));
            monitor.addEllipse(QPointF(160.3, 220.2), 12, 12);
            monitor.addRect(QRectF(135.3, 250.2, 50, 35));
            monitor.addRoundedRect(QRectF(85.3, 285.2, 150, 15), 7, 7);
            p.fillPath(monitor.simplified(), Qt::black);
        }
        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        source.save(&buffer, "PNG");
        const NodeId id = m_editor->importImage(png, QPointF(100, 100));
        QVERIFY(id != InvalidNodeId);
        const auto* image = m_editor->document().findAs<ImageObject>(id);

        TraceDialog dialog(image->image(), trace::Preset::LineArt);
        dialog.recompute();
        QCOMPARE(dialog.result().colors, 1);
        QCOMPARE(dialog.result().shapes.size(), 1);
        QVERIFY2(dialog.result().nodes < 50, qPrintable(QString::number(dialog.result().nodes)));
        const QString dir = qEnvironmentVariable("VEK_SCREENSHOT_DIR");
        if (!dir.isEmpty())
            dialog.grab().save(dir + QStringLiteral("/49-supertrace-strichgrafik.png"));

        // Regler ändern rechnet die Vorschau kurz danach selbst neu (ohne Bildtyp-Wechsel)
        const int nodesBefore = dialog.result().nodes;
        const QList<SubPath> pathsBefore = dialog.result().shapes.first().paths;
        const QList<QSlider*> sliders = dialog.findChildren<QSlider*>();
        auto smoothingSlider = std::find_if(sliders.begin(), sliders.end(), [&](QSlider* s) {
            return s->value() == dialog.settings().smoothing && s->maximum() == 100;
        });
        QVERIFY(smoothingSlider != sliders.end());
        (*smoothingSlider)->setValue(100);
        QCOMPARE(dialog.settings().smoothing, 100);
        QTRY_VERIFY(!(dialog.result().shapes.first().paths == pathsBefore));
        QCOMPARE(dialog.result().shapes.size(), 1);
        dialog.undo();
        QCOMPARE(dialog.result().nodes, nodesBefore);

        // Einstellungen rückgängig und wiederholen
        trace::Settings smoother = dialog.settings();
        smoother.smoothing = 80;
        dialog.setSettings(smoother);
        dialog.recompute();
        QVERIFY(dialog.canUndo());
        dialog.undo();
        QCOMPARE(dialog.settings().smoothing, 25);
        QVERIFY(dialog.canRedo());
        dialog.redo();
        QCOMPARE(dialog.settings().smoothing, 80);
        dialog.reset();
        dialog.recompute();
        QCOMPARE(dialog.settings().smoothing, 25);

        // Eine Kurve: liegt ohne Gruppe deckungsgleich über dem Bild
        const NodeId curve = m_editor->vectorizeImage(id, dialog.result(), false);
        const auto* traced = m_editor->document().findAs<CurveObject>(curve);
        QVERIFY(traced);
        QCOMPARE(traced->subPaths().size(), 3);
        QVERIFY2(near(traced->boundingRect(), m_editor->document().findAs<ImageObject>(id)->boundingRect().adjusted(
                                                  25.4 * 10.3 / 96, 25.4 * 20.2 / 96, -25.4 * 9.7 / 96, -25.4 * 19.8 / 96), 0.3),
                 qPrintable(describe(traced->boundingRect())));
        m_editor->document().undo();

        // Logo mit Farben: zusammenführen ergibt eine Farbe; farblich gruppieren ergibt Untergruppen
        TraceDialog logo(m_editor->document().findAs<ImageObject>(id)->image(), trace::Preset::Logo);
        logo.recompute();
        QCOMPARE(logo.result().colors, 2);
        logo.setPreviewMode(2);
        if (!dir.isEmpty())
            logo.grab().save(dir + QStringLiteral("/50-supertrace-drahtmodell.png"));
        auto* tabs = logo.findChild<QTabWidget*>();
        QVERIFY(tabs);
        tabs->setCurrentIndex(1);
        auto* colorList = logo.findChild<QListWidget*>();
        QVERIFY(colorList);
        QCOMPARE(colorList->count(), 2);
        logo.setPreviewMode(0);
        if (!dir.isEmpty())
            logo.grab().save(dir + QStringLiteral("/51-supertrace-farben.png"));
        trace::Settings grouped = logo.settings();
        grouped.groupByColor = true;
        logo.setSettings(grouped);
        logo.recompute();
        const NodeId group = m_editor->vectorizeImage(id, logo.result(), false);
        const auto* top = m_editor->document().findAs<GroupObject>(group);
        QVERIFY(top);
        QCOMPARE(top->childCount(), 2);
        for (int i = 0; i < top->childCount(); ++i)
            QVERIFY(node_cast<GroupObject>(top->child(i)));
        m_editor->document().undo();
        logo.mergeColors({logo.result().palette[0].source, logo.result().palette[1].source});
        logo.recompute();
        QCOMPARE(logo.result().colors, 1);
        QVERIFY(logo.canUndo());
    }

    // Sichtprüfung des Vektorisierers an einer echten Datei, nur mit VEK_TRACE_FILE
    void traceRealImage()
    {
        const QString path = qEnvironmentVariable("VEK_TRACE_FILE");
        if (path.isEmpty())
            QSKIP("nur mit VEK_TRACE_FILE");
        const QImage image(path);
        QVERIFY(!image.isNull());
        for (trace::Preset preset : {trace::Preset::Logo, trace::Preset::Clipart, trace::Preset::HighQuality}) {
            TraceDialog dialog(image, preset);
            dialog.resize(900, 600);
            QElapsedTimer t;
            t.start();
            dialog.recompute();
            qInfo("TRACE %s: %d Farben, %d Knoten, %lld ms", qPrintable(trace::presetName(preset)), dialog.result().colors,
                  dialog.result().nodes, t.elapsed());
            if (!qEnvironmentVariable("VEK_SCREENSHOT_DIR").isEmpty())
                dialog.grab().save(qEnvironmentVariable("VEK_SCREENSHOT_DIR") + QStringLiteral("/trace-%1.png").arg(int(preset)));
        }
    }

    // Bildschirmfotos der Bilddialoge an einer echten Datei, nur mit VEK_ADJUST_FILE
    void imageDialogsOnRealImage()
    {
        const QString path = qEnvironmentVariable("VEK_ADJUST_FILE");
        const QString dir = qEnvironmentVariable("VEK_SCREENSHOT_DIR");
        if (path.isEmpty() || dir.isEmpty())
            QSKIP("nur mit VEK_ADJUST_FILE und VEK_SCREENSHOT_DIR");
        const QImage image(path);
        ImageAdjustDialog adjustDialog(image);
        adjust::Basic b;
        b.temperature = 40;
        b.saturation = 20;
        b.shadows = 30;
        adjustDialog.setBasic(b);
        adjustDialog.grab().save(dir + QStringLiteral("/49-bildanpassung.png"));
        ToneCurveDialog curve(image);
        curve.curve()->setPoints({QPointF(0, 0), QPointF(100, 160), QPointF(255, 255)});
        curve.grab().save(dir + QStringLiteral("/50-tonkurve.png"));
    }

    // Zeitmessung, nur mit VEK_BENCH: Bildaufbau der Zeichenfläche und des ganzen Fensters
    // Bildschirmfoto: Lineale in cm mit Ursprung, Gitter und Hilfslinien-Voreinstellung, nur mit VEK_SCREENSHOT_DIR
    void rulerScreenshot()
    {
        const QString dir = qEnvironmentVariable("VEK_SCREENSHOT_DIR");
        if (dir.isEmpty())
            QSKIP("nur mit VEK_SCREENSHOT_DIR");
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        Options::instance().resetWorkspace();
        MainWindow window;
        window.resize(1300, 900);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        Editor& ed = window.canvas()->editor();
        Options::instance().apply({{QStringLiteral("doc/rulers/units"), 1}, {QStringLiteral("doc/rulers/originX"), 105.0},
                                   {QStringLiteral("doc/rulers/originY"), 148.5}, {QStringLiteral("doc/grid/show"), true},
                                   {QStringLiteral("doc/grid/mode"), 1}, {QStringLiteral("doc/grid/h"), 10.0},
                                   {QStringLiteral("doc/grid/v"), 10.0}, {QStringLiteral("doc/guides/presets"), QStringLiteral("cm,columns")}});
        ed.replaceGuides(Options::instance().presetGuides(ed.currentPage().size()), {});
        QApplication::processEvents();
        window.grab().save(dir + QStringLiteral("/66-lineale-cm.png"));
        Options::instance().resetWorkspace();
    }

    // Andockfenster als senkrechte Reiter, Farbpalette fest rechts daneben
    void dockersTabbedBesidePalette()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        MainWindow window;
        window.resize(1400, 900);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto* palette = window.findChild<ColorPalette*>();
        QVERIFY(palette);
        QVERIFY(!qobject_cast<QDockWidget*>(palette->parentWidget())); // keine Andock-Seite
        QDockWidget* shaping = nullptr;
        QDockWidget* manager = nullptr;
        for (QDockWidget* d : window.findChildren<QDockWidget*>()) {
            if (d->objectName() == QLatin1String("shaping"))
                shaping = d;
            if (d->objectName() == QLatin1String("objectManager"))
                manager = d;
        }
        QVERIFY(shaping && manager);
        shaping->show();
        QApplication::processEvents();
        QVERIFY(window.tabifiedDockWidgets(manager).contains(shaping));
        QCOMPARE(window.tabPosition(Qt::RightDockWidgetArea), QTabWidget::East);
        // Palette rechts von beiden Andockfenstern
        const int paletteX = palette->mapTo(&window, QPoint(0, 0)).x();
        QVERIFY(paletteX > shaping->mapTo(&window, QPoint(shaping->width(), 0)).x() - 2);
        QVERIFY(palette->height() > 400);
        if (!qEnvironmentVariable("VEK_SCREENSHOT_DIR").isEmpty())
            window.grab().save(qEnvironmentVariable("VEK_SCREENSHOT_DIR") + QStringLiteral("/65-andockfenster.png"));
    }

    // Bildschirmfotos der Effektleisten, nur mit VEK_SCREENSHOT_DIR
    void effectBarsScreenshot()
    {
        const QString dir = qEnvironmentVariable("VEK_SCREENSHOT_DIR");
        if (dir.isEmpty())
            QSKIP("nur mit VEK_SCREENSHOT_DIR");
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        MainWindow window;
        window.resize(1600, 900);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        Editor& ed = window.canvas()->editor();
        auto rect = std::make_unique<RectangleObject>(QSizeF(60, 40));
        rect->setTransform(QTransform::fromTranslate(30, 30));
        const NodeId id = ed.addObject(std::move(rect), QStringLiteral("R"));
        auto shot = [&](ToolId tool, const QString& name) {
            window.canvas()->setTool(tool);
            ed.setSelection({id});
            QApplication::processEvents();
            window.grab(QRect(0, 0, 1600, 130)).save(dir + QLatin1Char('/') + name + QStringLiteral(".png"));
        };
        shot(ToolId::Transparency, QStringLiteral("56-leiste-transparenz"));
        ed.setSelectionEffect(Effect::Blur, [](Ramp& r) { r = EffectTool::defaultRamp(Effect::Blur, Ramp::Type::Radial, r); }, QStringLiteral("B"));
        shot(ToolId::Blur, QStringLiteral("57-leiste-unschaerfe"));
        shot(ToolId::Pick, QStringLiteral("58-objektleiste-unschaerfe"));
    }

    // Zeitmessung Mengentext ziehen, nur mit VEK_BENCH
    void textDragTiming()
    {
        if (qEnvironmentVariableIsEmpty("VEK_BENCH"))
            QSKIP("nur mit VEK_BENCH");
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        MainWindow window;
        window.resize(1600, 1000);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        Canvas* canvas = window.canvas();
        Editor& ed = canvas->editor();
        canvas->fitPage();
        QString story;
        for (int i = 0; i < 12; ++i)
            story += QStringLiteral("Vorgaben vom 26.10.2026, umgesetzt im neuen Rechner; der alte bleibt unberuehrt. ");
        QList<NodeId> frames;
        for (int i = 0; i < 6; ++i) {
            auto t = std::make_unique<TextObject>(TextObject::Kind::Paragraph, QSizeF(80, 70));
            t->setPlainText(i == 3 || i == 4 ? QString() : story, CharFormat{QStringLiteral("Arial"), 12.0});
            t->setTransform(QTransform::fromTranslate(10 + (i % 2) * 95, 10 + (i / 2) * 85));
            frames << ed.addObject(std::move(t), QStringLiteral("Text"));
        }
        QVERIFY(ed.linkTextFrame(frames[2], frames[3]));
        QVERIFY(ed.linkTextFrame(frames[3], frames[4]));
        canvas->setTool(ToolId::Pick);
        QElapsedTimer t;
        auto report = [&t](const char* what, int n) {
            qInfo("BENCH %-28s %7.2f ms", what, double(t.nsecsElapsed()) / 1e6 / n);
        };
        for (NodeId id : {frames[0], frames[3]}) {
            ed.setSelection({id});
            QApplication::processEvents();
            const QPoint c = canvas->toScreen(ed.selectionBounds().center()).toPoint();
            t.start();
            QTest::mousePress(canvas, Qt::LeftButton, {}, c, 1000);
            for (int i = 1; i <= 20; ++i) {
                QTest::mouseMove(canvas, c + QPoint(i * 3, i * 2));
                canvas->repaint();
            }
            report(id == frames[0] ? "text drag step" : "chained text drag step", 20);
            t.restart();
            QTest::mouseRelease(canvas, Qt::LeftButton, {}, c + QPoint(60, 40), 10);
            QApplication::processEvents();
            report("text drop", 1);
        }
    }

    // Zeitmessung Unschärfe-Verlauf auf einem grossen Foto ziehen, nur mit VEK_BENCH
    void blurHandleTiming()
    {
        if (qEnvironmentVariableIsEmpty("VEK_BENCH"))
            QSKIP("nur mit VEK_BENCH");
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        MainWindow window;
        window.resize(1600, 1000);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        Canvas* canvas = window.canvas();
        Editor& ed = canvas->editor();
        canvas->fitPage();
        QImage photo(5600, 3728, QImage::Format_RGB32);
        QPainter painter(&photo);
        painter.fillRect(photo.rect(), QLinearGradient(0, 0, 5600, 3728));
        painter.fillRect(QRect(1000, 800, 2000, 1500), Qt::darkGreen);
        painter.end();
        const NodeId id = ed.importImage(photo, QPointF(105, 148));
        ed.setSelection({id});
        canvas->setTool(ToolId::Blur);
        const QRectF b = ed.selectionBounds();
        auto at = [canvas](double x, double y) { return canvas->toScreen(QPointF(x, y)).toPoint(); };
        QTest::mousePress(canvas, Qt::LeftButton, {}, at(b.left() + 10, b.center().y()), 1000);
        QTest::mouseMove(canvas, at(b.right() - 10, b.center().y()));
        QTest::mouseRelease(canvas, Qt::LeftButton, {}, at(b.right() - 10, b.center().y()), 10);
        QApplication::processEvents();
        QElapsedTimer t;
        t.start();
        canvas->repaint();
        qInfo("BENCH %-28s %7.2f ms", "blurred photo repaint", double(t.nsecsElapsed()) / 1e6);
        t.restart();
        QTest::mousePress(canvas, Qt::LeftButton, {}, at(b.left() + 10, b.center().y()), 1000);
        for (int i = 1; i <= 10; ++i) {
            QTest::mouseMove(canvas, at(b.left() + 10 + i * 3, b.center().y() + i));
            canvas->repaint();
        }
        qInfo("BENCH %-28s %7.2f ms", "blur handle drag step", double(t.nsecsElapsed()) / 1e6 / 10);
        QTest::mouseRelease(canvas, Qt::LeftButton, {}, at(b.left() + 40, b.center().y() + 10), 10);
    }

    void renderTiming()
    {
        if (qEnvironmentVariableIsEmpty("VEK_BENCH"))
            QSKIP("nur mit VEK_BENCH");
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        MainWindow window;
        window.resize(1600, 1000);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        Canvas* canvas = window.canvas();
        Editor& ed = canvas->editor();
        canvas->fitPage();
        auto at = [canvas](double x, double y) { return canvas->toScreen(QPointF(x, y)).toPoint(); };
        auto dragOn = [canvas](QPoint a, QPoint b) {
            QTest::mousePress(canvas, Qt::LeftButton, {}, a, 1000);
            for (int i = 1; i <= 8; ++i)
                QTest::mouseMove(canvas, a + (b - a) * i / 8 + QPoint(0, (i % 2) * 3));
            QTest::mouseRelease(canvas, Qt::LeftButton, {}, b, 10);
        };
        for (int i = 0; i < 120; ++i) {
            canvas->setTool(ToolId::Rectangle);
            const double x = 10 + (i % 12) * 16, y = 10 + (i / 12) * 14;
            dragOn(at(x, y), at(x + 12, y + 10));
            ed.setSelectionFill(Fill::solid(Color(i * 2, 100, 200 - i)));
        }
        for (int i = 0; i < 30; ++i) {
            canvas->setTool(ToolId::ArtisticMedia);
            dragOn(at(10 + (i % 6) * 32, 160 + (i / 6) * 22), at(35 + (i % 6) * 32, 175 + (i / 6) * 22));
        }
        for (int i = 0; i < 20; ++i) {
            canvas->setTool(ToolId::Star);
            dragOn(at(10 + (i % 10) * 19, 275 + (i / 10) * 10), at(25 + (i % 10) * 19, 284 + (i / 10) * 10));
        }
        canvas->setTool(ToolId::Pick);
        ed.clearSelection();
        QApplication::processEvents();

        QElapsedTimer t;
        auto report = [&t](const char* what, int n) {
            qInfo("BENCH %-28s %7.2f ms", what, double(t.nsecsElapsed()) / 1e6 / n);
        };
        t.start();
        for (int i = 0; i < 30; ++i)
            canvas->repaint();
        report("canvas repaint", 30);

        t.restart();
        for (int i = 0; i < 100; ++i) {
            QTest::mouseMove(canvas, QPoint(100 + i * 3, 300 + (i % 7)));
            QApplication::processEvents();
        }
        report("hover move", 100);

        ed.selectAll();
        const QList<NodeId> all = ed.selection();
        ed.setSelection({all.value(5)});
        QApplication::processEvents();
        t.restart();
        for (int i = 0; i < 10; ++i) {
            ed.setSelection({all.value(5 + (i % 2))});
            QApplication::processEvents();
        }
        report("selection same type", 10);
        t.restart();
        for (int i = 0; i < 10; ++i) {
            ed.setSelection({all.value(i % 2 ? 5 : int(all.size()) - 1)});
            QApplication::processEvents();
        }
        report("selection other type", 10);
        t.restart();
        for (int i = 0; i < 10; ++i) {
            ed.setSelection({all.value(i % 2 ? 5 : 150)});
            QApplication::processEvents();
        }
        report("selection media curve", 10);
        ed.setSelection({all.value(5)});
        QApplication::processEvents();

        const QPoint c = canvas->toScreen(ed.selectionBounds().center()).toPoint();
        t.restart();
        QTest::mousePress(canvas, Qt::LeftButton, {}, c, 1000);
        for (int i = 1; i <= 40; ++i) {
            QTest::mouseMove(canvas, c + QPoint(i * 4, i * 2));
            canvas->repaint();
        }
        QTest::mouseRelease(canvas, Qt::LeftButton, {}, c + QPoint(160, 80), 10);
        report("drag step", 40);

        t.restart();
        for (int i = 0; i < 10; ++i)
            window.repaint();
        report("window repaint", 10);
        if (!qEnvironmentVariable("VEK_SCREENSHOT_DIR").isEmpty())
            window.grab().save(qEnvironmentVariable("VEK_SCREENSHOT_DIR") + QStringLiteral("/40-objektleiste.png"));
    }
};

QTEST_MAIN(TestTools)
#include "tst_tools.moc"
