// Bedienung der Werkzeuge mit echten Mausereignissen auf der Zeichenfläche.
// Mit VEK_SCREENSHOT_DIR=<Ordner> legen die Tests Bildschirmfotos der Zwischenstände ab.

#include "BaselineGrid.h"
#include "CorrectionDialogs.h"
#include "Canvas.h"
#include "BitmapEffect.h"
#include "EffectDockers.h"
#include "ImageDialogs.h"
#include "ImagePool.h"
#include "SuperClipObject.h"
#include <QListWidget>
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
#include "NodePopup.h"
#include "ShapeTool.h"
#include "ArtisticMediaTool.h"
#include "CutTools.h"
#include "FlyoutButton.h"
#include "GroupObject.h"
#include "BitmapFilters.h"
#include "Language.h"
#include "MainWindow.h"
#include "CustomToolbars.h"
#include "TangentaStyle.h"
#include "Splash.h"
#include <QMenuBar>
#include <QStyleFactory>
#include "CutoutAssistant.h"
#include "Cutout.h"
#include <QDialogButtonBox>
#include <QDropEvent>
#include "PickTool.h"
#include "PolygonObject.h"
#include "RectangleObject.h"
#include "TextObject.h"
#include "TextTool.h"
#include "TraceDialog.h"
#include "Serialization.h"
#include "EffectTool.h"
#include "ShapingDocker.h"
#include "CornersDocker.h"
#include "OptionsDialog.h"
#include "Options.h"
#include "EllipseObject.h"
#include "SmartDrawingTool.h"
#include "ConnectorObject.h"
#include "DimensionObject.h"
#include "TableObject.h"
#include "MeshTool.h"
#include <QTemporaryDir>
#include <QScrollArea>
#include <QMimeData>
#include <QClipboard>
#include <QMdiSubWindow>
#include <QMdiArea>
#include "Output.h"
#include "AlignDialog.h"
#include "BlendObject.h"
#include "InteractiveTools.h"
#include "InteractiveFillTool.h"
#include "OutlineDialog.h"
#include "ShapingTools.h"
#include "PerfectShapeTool.h"
#include "PatternDialog.h"
#include "QrCodeDialog.h"
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
#include "AiImageDialog.h"
#include "Matting.h"
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
#include <QCompleter>
#include <QLineEdit>
#include <QMenu>
#include <QTimer>
#include <QToolBar>
#include <QSlider>
#include <QUrlQuery>
#include <QSet>

#include <QtTest>
#include <QPlainTextEdit>
#include <QTreeWidget>
#include <QContextMenuEvent>
#include <QTranslator>
#include <QGroupBox>
#include <QAbstractButton>
#include <QMenuBar>
#include <QDockWidget>
#include <QPushButton>
#include <QCheckBox>

#include <numbers>
#include "DrawingTools.h"
#include <QSvgGenerator>
#include "PropertyBar.h"
#include <QRadioButton>
#include "PagesDocker.h"
#include "MosaicDialogs.h"
#include "BlockShadowTool.h"
#include "MediaDocker.h"
#include "MediaLibrary.h"

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

    // Grundlinienraster: oben/Abstand/unten in mm, Griffe neben der Seite ziehen (dehnen, einzelne
    // Linie, ganzes Raster), Doppelklick setzt zurück, ältere Dateien mit Abstand in pt
    void baselineGridDragging()
    {
        Options& o = Options::instance();
        o.attach(m_editor.get());
        o.apply({{QStringLiteral("doc/grid/baseline/show"), true}, {QStringLiteral("doc/grid/baseline/top"), 20.0},
                 {QStringLiteral("doc/grid/baseline/step"), 10.0}, {QStringLiteral("doc/grid/baseline/bottom"), 17.0}});
        BaselineGrid g = BaselineGrid::fromOptions();
        // A4: 20, 30, …, 280 (297 - 17 = 280)
        QCOMPARE(g.count(297.0), 27);
        QCOMPARE(g.lines(297.0).last(), 280.0);

        const QRectF page = m_canvas->viewTransform().mapRect(QRectF(0, 0, 210, 297));
        auto handle = [&](double y) { return QPoint(int(page.left()) - 5, int(std::round(m_canvas->toScreen(QPointF(0, y)).y()))); };
        QCOMPARE(m_canvas->baselineHandleAt(handle(20.0)), 0);
        QCOMPARE(m_canvas->baselineHandleAt(handle(50.0)), 3);
        QCOMPARE(m_canvas->baselineHandleAt(QPointF(page.center().x(), handle(50.0).y())), -1); // auf der Seite: frei

        // Letzte Linie nach oben: Raster staucht, oberste bleibt, Anzahl bleibt
        drag(handle(280.0), handle(254.0), {}, QStringLiteral("grundlinien-dehnen"));
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Grundlinienraster dehnen"));
        g = BaselineGrid::fromOptions();
        QCOMPARE(g.top, 20.0);
        QVERIFY2(qAbs(g.step - 9.0) < 0.05, qPrintable(QString::number(g.step)));
        QCOMPARE(g.count(297.0), 27);
        QVERIFY(qAbs(g.lines(297.0).last() - 254.0) < 1.0);

        // Linie dazwischen: nur sie
        const double before = g.line(5);
        drag(handle(before), handle(before + 6.0));
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Grundlinie verschieben"));
        g = BaselineGrid::fromOptions();
        QVERIFY(g.moved.contains(5));
        QVERIFY2(qAbs(g.line(5) - before - 6.0) < 0.6, qPrintable(QString::number(g.line(5))));
        QCOMPARE(g.line(4), BaselineGrid::fromOptions().regular(4));
        screenshot(QStringLiteral("grundlinie-verschoben"));

        // Umschalt: ganzes Raster; die verschobene Linie wandert mit
        const double top = g.top;
        drag(handle(g.line(2)), handle(g.line(2) + 5.0), Qt::ShiftModifier);
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Grundlinienraster verschieben"));
        g = BaselineGrid::fromOptions();
        QVERIFY2(qAbs(g.top - top - 5.0) < 0.6, qPrintable(QString::number(g.top)));
        QVERIFY(g.moved.contains(5));

        // Doppelklick auf den Griff: Linie zurück
        QTest::mouseDClick(m_canvas.get(), Qt::LeftButton, {}, handle(g.line(5)));
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Grundlinie zurücksetzen"));
        QVERIFY(BaselineGrid::fromOptions().moved.isEmpty());

        // Einrasten an der Grundlinie
        o.set(QStringLiteral("doc/grid/baseline/snap"), true);
        {
            SnapEngine snap(*m_canvas, {});
            const double y = BaselineGrid::fromOptions().line(3);
            const SnapResult r = snap.snap(QPointF(100, y + 0.3));
            QVERIFY(r.snapped());
            QVERIFY(qAbs(r.point.y() - y) < 1e-9);
        }

        // Text rastet mit seiner Grundlinie ein (Einrastart «Textbasislinie»)
        {
            auto text = std::make_unique<TextObject>(TextObject::Kind::Artistic);
            text->setPlainText(QStringLiteral("Titel"), CharFormat(QStringLiteral("Arial"), 36));
            text->applyTransform(xform::translation(QPointF(40, 100)));
            const NodeId id = m_editor->addObject(std::move(text), QStringLiteral("T"));
            m_editor->setSelection({id});
            const auto* t = m_editor->document().findAs<TextObject>(id);
            const QList<QLineF> base = t->baselines();
            QCOMPARE(base.size(), 1);
            const BaselineGrid grid = BaselineGrid::fromOptions();
            int target = 0;
            while (grid.line(target) < base.first().y1() + 4.0)
                ++target;
            // Mitte des Textes so ziehen, dass die Grundlinie knapp neben der Linie landet; die Grundlinie
            // geht vor, auch wenn Mitte oder Rand des Rahmens näher an einer Linie lägen
            for (const double miss : {0.4, -1.1, 1.6, -2.2}) {
                const auto* moved = m_editor->document().findAs<TextObject>(id);
                const double before = moved->baselines().first().y1();
                const QPointF from = moved->boundingRect().center();
                const QPointF to = from + QPointF(3.0, grid.line(target) - before + miss);
                drag(screen(from), screen(to));
                const double y = m_editor->document().findAs<TextObject>(id)->baselines().first().y1();
                QVERIFY2(qAbs(y - grid.line(target)) < 1e-6,
                         qPrintable(QStringLiteral("%1 vs %2 (daneben %3)").arg(y).arg(grid.line(target)).arg(miss)));
                ++target;
            }
        }

        // Ältere Datei: nur der Abstand in pt
        QJsonObject old = m_editor->document().masterPage().options();
        old.remove(QStringLiteral("doc/grid/baseline/step"));
        old.insert(QStringLiteral("doc/grid/baseline/spacing"), 36.0);
        {
            Transaction tx(m_editor->document(), QStringLiteral("alt"));
            tx.modify<Page>(m_editor->document().masterPage().id(), [&old](Page& pg) { pg.setOptions(old); });
            tx.commit();
        }
        QVERIFY(qAbs(o.number(QStringLiteral("doc/grid/baseline/step")) - 12.7) < 1e-9);
        o.attach(nullptr);
    }

    // Grundlinienraster wirkt auf Mengentext: Zeilen liegen auf den Linien, auch beim Bearbeiten,
    // verkettet im Folgerahmen und nach dem Verschieben; je Rahmen abschaltbar
    void baselineGridAlignsParagraphText()
    {
        Options& o = Options::instance();
        o.attach(m_editor.get());
        o.apply({{QStringLiteral("doc/grid/baseline/show"), true}, {QStringLiteral("doc/grid/baseline/top"), 20.0},
                 {QStringLiteral("doc/grid/baseline/step"), 7.0}, {QStringLiteral("doc/grid/baseline/bottom"), 15.0}});
        const BaselineGrid grid = BaselineGrid::fromOptions();
        auto offGrid = [&grid](const QList<QLineF>& lines) {
            QStringList bad;
            for (const QLineF& l : lines) {
                const double k = (l.y1() - grid.top) / grid.step;
                if (qAbs(k - std::round(k)) > 0.002)
                    bad << QString::number(l.y1(), 'f', 3);
            }
            return bad.join(QLatin1Char(' '));
        };
        const QString story = QStringLiteral(
            "Am Samstag beginnt das Sommerfest um 14 Uhr mit dem Kinderprogramm auf der Wiese. Ab 17 Uhr spielt "
            "die Musikgesellschaft, danach gibt es Grilladen und Salate. Um 21 Uhr zeigen wir den Film im Freien, "
            "bei Regen in der Turnhalle. Der Eintritt ist frei, Kollekte am Ausgang.");
        auto text = std::make_unique<TextObject>(TextObject::Kind::Paragraph, QSizeF(80, 45));
        text->setPlainText(story, CharFormat(QStringLiteral("Arial"), 12));
        text->applyTransform(xform::translation(QPointF(25, 63.4)));
        Style black;
        black.fill = Fill::solid(Color(0, 0, 0));
        black.stroke.reset();
        text->setStyle(black);
        const NodeId id = m_editor->addObject(std::move(text), QStringLiteral("T"));
        auto next = std::make_unique<TextObject>(TextObject::Kind::Paragraph, QSizeF(80, 60));
        next->applyTransform(xform::translation(QPointF(115, 151.8)));
        const NodeId follower = m_editor->addObject(std::move(next), QStringLiteral("T2"));
        {
            Transaction tx(m_editor->document(), QStringLiteral("Verknüpfen"));
            tx.modify<TextObject>(id, [follower](TextObject& t) { t.setNextFrame(follower); });
            tx.commit();
        }
        const auto* t = m_editor->document().findAs<TextObject>(id);
        QVERIFY(t->baselines().size() >= 4);
        QVERIFY2(offGrid(t->baselines()).isEmpty(), qPrintable(offGrid(t->baselines())));
        // Folgerahmen: seine Zeilen liegen ebenfalls auf den Linien (Seitenkoordinaten)
        const auto* f = m_editor->document().findAs<TextObject>(follower);
        QVERIFY(!f->pieces().isEmpty());
        QVERIFY(!f->baselineGridLines().isEmpty());
        const double firstLocal = f->baselineGridLines().first();
        QVERIFY2(offGrid({QLineF(0, firstLocal + 151.8, 1, firstLocal + 151.8)}).isEmpty(), qPrintable(QString::number(firstLocal)));
        QVERIFY(firstLocal >= 0.0 && firstLocal < grid.step);
        m_editor->setSelection({id});
        screenshot(QStringLiteral("grundlinien-mengentext"));

        // Bearbeiten: Arbeitskopie setzt dieselben Zeilen, nach dem Tippen weiter auf dem Raster
        TextTool& tool = m_canvas->textTool();
        m_canvas->setTool(ToolId::Text);
        tool.beginEditing(id);
        QVERIFY(tool.editing() && tool.followsBaselineGrid());
        tool.typeText(QStringLiteral(" Bitte Becher mitbringen."));
        screenshot(QStringLiteral("grundlinien-mengentext-bearbeiten"));
        tool.finishEditing();
        t = m_editor->document().findAs<TextObject>(id);
        QVERIFY(t->plainText().contains(QStringLiteral("Becher")));
        QVERIFY2(offGrid(t->baselines()).isEmpty(), qPrintable(offGrid(t->baselines())));

        // Verschieben um einen Bruchteil: die Zeilen rasten neu ein
        {
            Transaction tx(m_editor->document(), QStringLiteral("Schieben"));
            tx.modify<TextObject>(id, [](TextObject& o2) { o2.applyTransform(xform::translation(QPointF(0, 2.35))); });
            tx.commit();
        }
        t = m_editor->document().findAs<TextObject>(id);
        QVERIFY2(offGrid(t->baselines()).isEmpty(), qPrintable(offGrid(t->baselines())));

        // Leiste: ausschalten wirkt auf die ganze Kette
        m_canvas->setTool(ToolId::Pick);
        m_editor->setSelection({id});
        m_editor->setTextBaselineGrid(false);
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Am Grundlinienraster ausrichten"));
        QVERIFY(!m_editor->document().findAs<TextObject>(id)->followsBaselineGrid());
        QVERIFY(!m_editor->document().findAs<TextObject>(follower)->followsBaselineGrid());
        QVERIFY(!offGrid(m_editor->document().findAs<TextObject>(id)->baselines()).isEmpty());
        m_editor->document().undo();
        QVERIFY(offGrid(m_editor->document().findAs<TextObject>(id)->baselines()).isEmpty());

        // Raster ausblenden: das Layout bleibt (beim Einblenden wurde «Mengentext am Gitter» festgehalten)
        QVERIFY(m_editor->document().masterPage().options().value(QStringLiteral("doc/grid/baseline/text")).toBool());
        o.set(QStringLiteral("doc/grid/baseline/show"), false);
        QVERIFY(offGrid(m_editor->document().findAs<TextObject>(id)->baselines()).isEmpty());
        o.set(QStringLiteral("doc/grid/baseline/show"), true);

        // Option aus: natürlicher Abstand trotz sichtbarem Raster (bleibt ausdrücklich gespeichert)
        o.set(QStringLiteral("doc/grid/baseline/text"), false);
        QCOMPARE(m_editor->document().masterPage().options().value(QStringLiteral("doc/grid/baseline/text")).toBool(true), false);
        QVERIFY(!offGrid(m_editor->document().findAs<TextObject>(id)->baselines()).isEmpty());
        o.attach(nullptr);
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

    void freehandSmoothingReducesNodes()
    {
        // Zittriger Bogen: mit Freihandglättung 100 deutlich weniger Knoten als ohne Glättung
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest")); // sonst kein QSettings
        for (const char* key : {"ws/tools/freehand/corner", "ws/tools/freehand/straight", "ws/tools/freehand/autoJoin"})
            Options::instance().set(QString::fromLatin1(key), 5); // Vorgaben (andere Tests ändern sie)
        auto drawShaky = [&](int smoothing) {
            Options::instance().set(QStringLiteral("ws/tools/freehand/smoothing"), smoothing);
            m_canvas->setTool(ToolId::Freehand);
            const QPoint from = screen(QPointF(30, 150));
            press(from);
            // etwa alle 3 Bildschirmpunkte ein Mauspunkt, unabhängig von der Vergrösserung
            const int steps = std::max(60, int(M_PI * 35.0 / m_canvas->pixelsToDoc(3.0)));
            for (int i = 1; i <= steps; ++i) {
                const double a = M_PI * i / steps;
                const int wobble = (i % 2 ? 2 : -2) * (i % 3 ? 1 : 2); // Bildschirmpunkte: Zittern der Hand
                // direkt senden: QTest::mouseMove fasst unter Windows dicht folgende Bewegungen zusammen
                const QPointF pos = screen(QPointF(30 + 40 * (1 - std::cos(a)), 150 - 30 * std::sin(a))) + QPoint(wobble, -wobble);
                QMouseEvent ev(QEvent::MouseMove, pos, m_canvas->mapToGlobal(pos), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(m_canvas.get(), &ev);
            }
            release(screen(QPointF(110, 150)));
            const int nodes = m_editor->document().findAs<CurveObject>(m_editor->selection().value(0))->nodeCount();
            m_editor->deleteSelection(); // sonst setzt der nächste Strich am Ende dieser Linie an
            return nodes;
        };
        const int raw = drawShaky(0);
        const int smooth = drawShaky(100);
        Options::instance().set(QStringLiteral("ws/tools/freehand/smoothing"), 50);
        QVERIFY2(smooth * 2 <= raw && smooth <= 12, qPrintable(QStringLiteral("%1 -> %2").arg(raw).arg(smooth)));
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
        m_canvas->nudge(1, 0, 0); // Schubsen nach rechts (Pfeiltaste)
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
        m_canvas->nudge(1, 0, 0);
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

        // Gruppe (z.B. Text in Kurven) mit einer Kurve kombinieren: der Inhalt der Gruppe zählt
        m_editor->groupSelection();
        const NodeId third = drawRect(QRectF(20, 90, 30, 30));
        m_editor->selectAll();
        QVERIFY(m_editor->canCombineSelection());
        m_editor->combineSelection();
        const auto* all = m_editor->document().findAs<CurveObject>(m_editor->selection().value(0));
        QVERIFY(all);
        QCOMPARE(all->subPaths().size(), 3);
        QVERIFY(!m_editor->document().find(third));
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
        m_canvas->nudge(1, 0, 0); // Pfeil rechts
        QVERIFY2(std::abs(m_editor->selectionBounds().left() - start.x() - 0.5) < 1e-6,
                 qPrintable(describe(m_editor->selectionBounds())));
        m_canvas->nudge(0, 1, 1); // Super-Schubsen (Umschalt+Pfeil): ×2
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

    void textHeightAndEnvelopeAfterCurves()
    {
        // Video 30: Höhe eines Grafiktexts über die Eigenschaftsleiste streckt die Buchstaben
        auto ink = [this](NodeId id) {
            output::ExportOptions o;
            o.only = {id};
            o.dpi = 200;
            o.transparent = true;
            const QImage img = output::renderImage(*m_canvas, o);
            int n = 0;
            for (int y = 0; y < img.height(); ++y)
                for (int x = 0; x < img.width(); ++x)
                    n += qAlpha(img.pixel(x, y)) > 128;
            return std::pair{img.size(), n};
        };
        auto text = std::make_unique<TextObject>(TextObject::Kind::Artistic);
        text->setPlainText(QStringLiteral("HALLO"), CharFormat(QStringLiteral("Arial"), 72));
        text->applyTransform(xform::translation(QPointF(20, 40)));
        const NodeId t = m_editor->addObject(std::move(text), QStringLiteral("T"));
        m_editor->setSelection({t});
        const QRectF before = m_editor->selectionBounds();
        const double glyphs0 = m_editor->document().findAs<TextObject>(t)->path().boundingRect().height();
        const auto [size0, ink0] = ink(t);
        m_editor->setSelectionBounds(QRectF(before.left(), before.top(), before.width(), 2 * before.height()));
        const auto* scaled = m_editor->document().findAs<TextObject>(t);
        QVERIFY2(std::abs(scaled->path().boundingRect().height() - 2 * glyphs0) < 0.5,
                 qPrintable(QStringLiteral("%1 -> %2, Rahmen %3 -> %4").arg(glyphs0).arg(scaled->path().boundingRect().height())
                                .arg(before.height()).arg(m_editor->selectionBounds().height())));
        const auto [size1, ink1] = ink(t);
        QVERIFY2(std::abs(size1.height() - 2 * size0.height()) <= 2, qPrintable(QStringLiteral("%1 -> %2").arg(size0.height()).arg(size1.height())));
        QVERIFY2(ink1 > 1.3 * ink0, qPrintable(QStringLiteral("%1 -> %2").arg(ink0).arg(ink1)));

        // Video 30: Text in Kurven (Strg+Q, Strg+L), dann «Hülle aus Objekt»: die Form wird verformt gezeichnet
        m_editor->setSelection({t});
        m_editor->convertSelectionToCurves();
        m_editor->combineSelection();
        QCOMPARE(m_editor->selection().size(), 1);
        const NodeId c = m_editor->selection().first();
        QVERIFY(m_editor->document().findAs<CurveObject>(c));
        const QRectF cb = m_editor->selectionBounds();
        auto arc = std::make_unique<EllipseObject>(QSizeF(cb.width(), cb.height() + 40));
        arc->applyTransform(xform::translation(QPointF(cb.left(), cb.top() - 20)));
        const NodeId target = m_editor->addObject(std::move(arc), QStringLiteral("E"));
        // Die Hülle aus einer Ellipse wird in den Rahmen der Kurve eingepasst: die Ecken des Rahmens bleiben leer
        const QRectF plain = m_editor->document().findAs<CurveObject>(c)->path().boundingRect();
        const QRectF corner(plain.topLeft(), QSizeF(plain.width() * 0.04, plain.height() * 0.04));
        QVERIFY(m_editor->document().findAs<CurveObject>(c)->path().intersects(corner)); // Ecke des H
        m_editor->setSelection({c});
        QVERIFY(m_editor->applyEnvelopeShape(m_editor->document().findAs<DrawObject>(target)->path(), true,
                                             Envelope::Mode::Unconstrained, false, Envelope::Mapping::Putty));
        const auto* enveloped = m_editor->document().findAs<CurveObject>(c);
        QVERIFY(enveloped->hasEnvelope());
        QVERIFY(!enveloped->path().intersects(corner));
        // Gezeichnet wird die verformte Form (nicht erst ein Duplikat)
        output::ExportOptions shotOptions;
        shotOptions.area = plain;
        shotOptions.dpi = 300;
        const QImage shot = output::renderImage(*m_canvas, shotOptions);
        const QRect cornerPx(0, 0, int(shot.width() * 0.04), int(shot.height() * 0.04));
        int darkPx = 0;
        for (int y = cornerPx.top(); y <= cornerPx.bottom(); ++y)
            for (int x = cornerPx.left(); x <= cornerPx.right(); ++x)
                darkPx += qGray(shot.pixel(x, y)) < 128;
        QCOMPARE(darkPx, 0);
        // Verlauf und Kontur auf dem verformten Objekt
        m_editor->setSelection({c});
        m_editor->setSelectionStrokeWidth(1.0);
        QCOMPARE(m_editor->document().findAs<CurveObject>(c)->style().strokeWidth, 1.0);
        QVERIFY(m_editor->document().findAs<CurveObject>(c)->hasEnvelope());
        m_editor->setSelection({c, target});
        m_editor->deleteSelection();
    }

    void decimalPointAndEnterInNumberFields()
    {
        // Video 29: «2.5» im Umrissstift-Dialog ergab 25 mm (Punkt als Tausendertrenner der Sprache);
        // die Eingabetaste schloss den Dialog, statt den Wert zu übernehmen
        const QLocale previous;
        QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));
        Style style;
        style.stroke = Color(0, 0, 0);
        style.strokeWidth = 0.2;
        OutlinePenDialog dialog(style);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        auto* width = dialog.findChild<ExpressionSpinBox*>();
        QVERIFY(width);
        for (const auto& [typed, expected] : {std::pair{QStringLiteral("2.5"), 2.5}, std::pair{QStringLiteral("0,7"), 0.7},
                                              std::pair{QStringLiteral("1.25 mm"), 1.25}}) {
            width->setFocus();
            width->selectAll();
            QTest::keyClicks(width, typed);
            QTest::keyClick(width, Qt::Key_Return);
            QVERIFY2(dialog.isVisible(), qPrintable(typed)); // erste Eingabetaste übernimmt nur
            QCOMPARE(width->value(), expected);
        }
        QTest::keyClick(width, Qt::Key_Return); // Wert steht: jetzt wie OK
        QVERIFY(!dialog.isVisible());
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QCOMPARE(dialog.style().strokeWidth, 1.25);
        // Alle übrigen Zahlenfelder der Programmoberfläche sind solche Felder
        MainWindow window;
        for (QDoubleSpinBox* box : window.findChildren<QDoubleSpinBox*>())
            QVERIFY2(qobject_cast<ExpressionSpinBox*>(box), qPrintable(box->objectName() + QLatin1Char(' ') + box->toolTip()));
        QLocale::setDefault(previous);
    }

    // Hilfe-Menü und Startbild
    void helpMenuAndSplash()
    {
        MainWindow window;
        QAction* about = window.findChild<QAction*>(QStringLiteral("about"));
        QVERIFY(about);
        QVERIFY(!about->icon().isNull());
        const QPixmap one = splash::render(1.0, QStringLiteral("Wird gestartet"));
        QCOMPARE(one.size(), splash::size());
        const QPixmap two = splash::render(2.0);
        QCOMPARE(two.size(), splash::size() * 2);
        // nicht leer: Name hell auf dunklem Grund
        const QImage img = one.toImage();
        int bright = 0;
        for (int x = 156; x < 440; ++x)
            for (int y = 70; y < 110; ++y)
                bright += qGray(img.pixel(x, y)) > 200;
        QVERIFY(bright > 300);
        if (qEnvironmentVariableIsSet("VEK_SPLASH_SHOT"))
            one.save(qEnvironmentVariable("VEK_SPLASH_SHOT"));
    }

    // Programmstil: Griff an verschiebbaren Leisten, eingerücktes Dokumentsymbol, flache Fensterknöpfe.
    // VEK_STYLE_SHOT=datei.png legt ein Bild des Fensters ab.
    void tangentaStyle()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        const QString key = QStringLiteral("toolbars/custom");
        QSettings().remove(key);
        const QString previous = QApplication::style()->name();
        QApplication::setStyle(new TangentaStyle);
        auto cleanup = qScopeGuard([key, previous] {
            QSettings().remove(key);
            QApplication::setStyle(QStyleFactory::create(previous));
        });
        MainWindow window;
        window.resize(1100, 700);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto* bars = window.findChild<CustomToolbars*>();
        QVERIFY(bars);
        CustomToolBar* bar = bars->create(QStringLiteral("Meine Leiste"));
        QTest::qWait(100);
        QCOMPARE(bar->style()->pixelMetric(QStyle::PM_ToolBarHandleExtent, nullptr, bar), 9);

        QMenuBar* menu = window.menuBar();
        QWidget* left = menu->cornerWidget(Qt::TopLeftCorner);
        QWidget* right = menu->cornerWidget(Qt::TopRightCorner);
        QVERIFY(left && left->isVisible());
        QVERIFY(right && right->isVisible());
        QCOMPARE(left->width(), TangentaStyle::MenuBarIconWidth);
        QCOMPARE(right->sizeHint().width(), 3 * 28 + 2);
        // Das Symbol beginnt dort, wo die Symbole der Standardleiste beginnen
        QToolBar* standard = nullptr;
        for (QToolBar* tb : window.findChildren<QToolBar*>())
            if (tb->isVisible() && !tb->isFloating() && window.toolBarArea(tb) == Qt::TopToolBarArea
                && (!standard || tb->geometry().top() < standard->geometry().top()))
                standard = tb;
        QVERIFY(standard);
        QToolButton* first = nullptr;
        for (QToolButton* b : standard->findChildren<QToolButton*>())
            if (b->isVisible() && b->parentWidget() == standard && (!first || b->x() < first->x()))
                first = b;
        QVERIFY(first);
        const int iconX = first->mapTo(&window, QPoint(0, 0)).x() + (first->width() - first->iconSize().width()) / 2;
        const int labelX = left->mapTo(&window, QPoint(0, 0)).x() + TangentaStyle::DocumentIconInset;
        qInfo("Symbol Menüleiste x=%d, Standardleiste x=%d", labelX, iconX);
        QVERIFY(qAbs(labelX - iconX) <= 2);
        if (qEnvironmentVariableIsSet("VEK_STYLE_SHOT"))
        {
            window.grab().save(qEnvironmentVariable("VEK_STYLE_SHOT"));
            bar->grab().save(qEnvironmentVariable("VEK_STYLE_SHOT") + QStringLiteral("-bar.png"));
        }
    }

    // Jeder Menüeintrag hat ein Symbol (Untermenüs eingeschlossen; dynamische Listen wie zuletzt
    // geöffnete Dateien, Fensterliste und Sprachen ausgenommen). VEK_MENU_DUMP=datei schreibt die
    // Einträge ohne Symbol als Liste "Pfad<TAB>Text".
    void cutoutAssistantDialog()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        QSettings().remove(QStringLiteral("cutoutAssistant"));
        // Orange Scheibe vor Himmel
        QImage photo(480, 360, QImage::Format_RGB32);
        for (int y = 0; y < 360; ++y)
            for (int x = 0; x < 480; ++x)
                photo.setPixel(x, y, qRgb(90 + x / 8, 140 + y / 10, 230));
        {
            QPainter p(&photo);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(230, 120, 30));
            p.drawEllipse(QPointF(240, 180), 100, 100);
        }
        CutoutAssistant dialog(photo);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        auto* ok = dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok);
        QVERIFY(!ok->isEnabled()); // ohne Füllung kein Ergebnis

        // Marker rund um die Kante, dann innen füllen
        dialog.setTool(CutoutAssistant::Tool::Marker);
        dialog.setBrushSize(16);
        QList<QPointF> ring;
        for (int i = 0; i <= 120; ++i)
            ring << QPointF(240 + 100 * std::cos(i * 2 * M_PI / 120), 180 + 100 * std::sin(i * 2 * M_PI / 120));
        dialog.stroke(ring);
        dialog.setTool(CutoutAssistant::Tool::Fill);
        dialog.fillAt(QPoint(240, 180));
        QVERIFY(ok->isEnabled());
        const QImage tri = dialog.trimapImage();
        QCOMPARE(qGray(tri.pixel(240, 180)), int(cutout::Foreground));
        QCOMPARE(qGray(tri.pixel(340, 180)), int(cutout::Unknown));
        QCOMPARE(qGray(tri.pixel(10, 10)), int(cutout::Background));

        dialog.preview(true);
        QVERIFY(!dialog.alpha().isNull());
        QCOMPARE(qGray(dialog.alpha().pixel(240, 180)), 255);
        QVERIFY(qGray(dialog.alpha().pixel(335, 180)) > 230); // innen am Rand: Motiv
        QVERIFY(qGray(dialog.alpha().pixel(346, 180)) < 25);  // aussen im Band: Himmel
        dialog.grab().save(QDir::tempPath() + QStringLiteral("/cutout-assistant.png"));

        // Detail entfernen: ein Stück der Scheibe weg
        dialog.setTool(CutoutAssistant::Tool::Remove);
        dialog.setBrushSize(30);
        dialog.stroke({QPointF(240, 100), QPointF(240, 130)});
        dialog.preview(true);
        QCOMPARE(qGray(dialog.alpha().pixel(240, 115)), 0);
        dialog.undo();
        // Rückgängig holt das schon berechnete Ergebnis zurück, ohne neu zu rechnen
        QVERIFY(!dialog.working());
        QCOMPARE(qGray(dialog.alpha().pixel(240, 115)), 255);
        QCOMPARE(qGray(dialog.trimapImage().pixel(240, 115)), int(cutout::Foreground));
        dialog.redo();
        QCOMPARE(qGray(dialog.trimapImage().pixel(240, 115)), int(cutout::Background));
        dialog.undo();

        const QImage result = dialog.result();
        QCOMPARE(result.size(), photo.size());
        QVERIFY(result.hasAlphaChannel());
        QCOMPARE(qAlpha(result.pixel(10, 10)), 0);
        QCOMPARE(qAlpha(result.pixel(240, 180)), 255);
        QCOMPARE(qAlpha(result.pixel(240, 115)), 255); // Entfernen wurde rückgängig gemacht
        QVERIFY(!dialog.keepOriginal());

        // Leck: offene Markierung füllt fast alles
        dialog.reset();
        dialog.setTool(CutoutAssistant::Tool::Fill);
        dialog.fillAt(QPoint(10, 10));
        QVERIFY(dialog.findChildren<QLabel*>().size() > 0);
        QSettings().remove(QStringLiteral("cutoutAssistant"));
    }

    void customToolbars()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        const QString key = QStringLiteral("toolbars/custom");
        QSettings().remove(key);
        auto cleanup = qScopeGuard([key] { QSettings().remove(key); });
        auto drop = [](QWidget* target, QMimeData* mime, const QPoint& pos) {
            QDragEnterEvent enter(pos, Qt::CopyAction | Qt::MoveAction | Qt::LinkAction, mime, Qt::LeftButton, Qt::NoModifier);
            QCoreApplication::sendEvent(target, &enter);
            QDropEvent dropEvent(QPointF(pos), Qt::CopyAction | Qt::MoveAction | Qt::LinkAction, mime, Qt::LeftButton,
                                 Qt::NoModifier);
            QCoreApplication::sendEvent(target, &dropEvent);
            const Qt::DropAction result = dropEvent.isAccepted() ? dropEvent.dropAction() : Qt::IgnoreAction;
            delete mime;
            return result;
        };
        QStringList expected;
        {
            MainWindow window;
            window.show();
            QVERIFY(QTest::qWaitForWindowExposed(&window));
            auto* bars = window.findChild<CustomToolbars*>();
            QVERIFY(bars);
            QCOMPARE(bars->bars().size(), 0);
            QList<CustomToolbars::Command> file;
            for (const CustomToolbars::Command& c : bars->commands())
                if (c.path.startsWith(QStringLiteral("Datei/")))
                    file << c;
            QVERIFY(file.size() > 4);

            // Neue Leiste: leer, mit Platz zum Ablegen
            CustomToolBar* bar = bars->create(QStringLiteral("Meine Leiste"));
            QTest::qWait(50);
            QVERIFY(bar->isVisible());
            QVERIFY(bar->width() >= 150);

            // Aus Optionen > Befehle ziehen (Daten der Befehlsliste)
            OptionsDialog options(*m_editor, &window, &window);
            QVERIFY(options.selectPage(QStringLiteral("Arbeitsbereich/Anpassung/Befehle")));
            auto* list = options.findChild<QListWidget*>(QStringLiteral("commandList"));
            QVERIFY(list && list->count() > 0);
            QVERIFY(list->dragEnabled());
            auto* fromList = list->model()->mimeData({list->model()->index(0, 0)});
            QVERIFY(fromList);
            QAction* first = CustomToolbars::actionFrom(fromList);
            QVERIFY(first);
            QCOMPARE(drop(bar, fromList, QPoint(10, bar->height() / 2)), Qt::CopyAction);
            QCOMPARE(bar->actions(), QList<QAction*>{first});

            // Hinten anfügen, dann einen Befehl vorne einfügen
            QAction* a = file[1].action == first ? file[2].action : file[1].action;
            QAction* b = file[3].action == first ? file[4].action : file[3].action;
            QTest::qWait(20);
            drop(bar, CustomToolbars::mimeFor(a), QPoint(bar->width() - 5, bar->height() / 2));
            QTest::qWait(20);
            const QPoint beforeFirst = bar->itemWidget(0)->geometry().topLeft() + QPoint(2, 2);
            drop(bar, CustomToolbars::mimeFor(b), beforeFirst);
            QCOMPARE(bar->actions(), (QList<QAction*>{b, first, a}));
            // Derselbe Befehl kommt nicht doppelt in die Leiste
            QTest::qWait(20);
            drop(bar, CustomToolbars::mimeFor(a), bar->itemWidget(0)->geometry().topLeft() + QPoint(2, 2));
            QCOMPARE(bar->actions(), (QList<QAction*>{a, b, first}));

            // In der Leiste umordnen: hinten nach vorn
            QTest::qWait(20);
            QCOMPARE(drop(bar, CustomToolbars::mimeFor(first, bar->id(), 2), bar->itemWidget(0)->geometry().topLeft() + QPoint(2, 2)),
                     Qt::LinkAction);
            QCOMPARE(bar->actions(), (QList<QAction*>{first, a, b}));
            bar->insertSeparatorAt(1);
            QCOMPARE(bar->itemCount(), 4);
            QVERIFY(bar->actions()[1]->isSeparator());

            // In eine zweite Leiste ziehen: dort eingefügt, die Quelle entfernt den Befehl selbst
            CustomToolBar* second = bars->create(QStringLiteral("Zweite"));
            QTest::qWait(50);
            QCOMPARE(drop(second, CustomToolbars::mimeFor(b, bar->id(), 3), QPoint(10, second->height() / 2)), Qt::MoveAction);
            QCOMPARE(second->actions(), QList<QAction*>{b});
            bar->removeAt(3);
            QCOMPARE(bar->itemCount(), 3);
            QCOMPARE(bars->barsContaining(first), QStringList{QStringLiteral("Meine Leiste")});

            // Fremde Daten werden nicht angenommen
            auto* foreign = new QMimeData;
            foreign->setText(QStringLiteral("Hallo"));
            QCOMPARE(drop(bar, foreign, QPoint(5, 5)), Qt::IgnoreAction);
            QCOMPARE(bar->itemCount(), 3);

            // Optionen offen: nicht modal, übriges Hauptfenster gesperrt, Leisten im Anpassen-Modus
            bool checked = false;
            QTimer::singleShot(100, &options, [&] {
                checked = !options.isModal() && options.isVisible() && bars->customizing()
                          && !window.menuBar()->isEnabled() && bar->isEnabled();
                options.reject();
            });
            options.exec();
            QVERIFY(checked);
            QVERIFY(!bars->customizing());
            QVERIFY(window.menuBar()->isEnabled());

            bars->rename(second, QStringLiteral("Zweite Leiste"));
            for (CustomToolBar* b2 : bars->bars()) {
                QStringList items;
                for (QAction* x : b2->actions())
                    items << (x->isSeparator() ? QStringLiteral("-") : bars->commandFor(x).path);
                expected << b2->windowTitle() + QLatin1Char(':') + items.join(QLatin1Char('|'));
            }
            window.grab().save(QDir::tempPath() + QStringLiteral("/custom-toolbars.png"));
        }
        // Neustart: Leisten mit Inhalt wieder da
        {
            MainWindow window;
            auto* bars = window.findChild<CustomToolbars*>();
            QStringList loaded;
            for (CustomToolBar* b2 : bars->bars()) {
                QStringList items;
                for (QAction* x : b2->actions())
                    items << (x->isSeparator() ? QStringLiteral("-") : bars->commandFor(x).path);
                loaded << b2->windowTitle() + QLatin1Char(':') + items.join(QLatin1Char('|'));
            }
            QCOMPARE(loaded, expected);
            // Gefunden auch über die Position, wenn der Pfad (Sprache) nicht passt
            const CustomToolbars::Command c = bars->commandFor(bars->bars().first()->actions().first());
            QCOMPARE(bars->resolve(QStringLiteral("Fichier/Inconnu"), c.index), c.action);
            bars->remove(bars->bars().last());
            QCOMPARE(bars->bars().size(), 1);
        }
        {
            MainWindow window;
            QCOMPARE(window.findChild<CustomToolbars*>()->bars().size(), 1);
        }
    }

    void everyMenuEntryHasIcon_data()
    {
        QTest::addColumn<QString>("language");
        QTest::newRow("de") << QString();
        QTest::newRow("en") << QStringLiteral("en");
        QTest::newRow("fr") << QStringLiteral("fr");
    }

    void everyMenuEntryHasIcon()
    {
        // Die Zuordnung vergleicht übersetzte Texte: in jeder Sprache prüfen
        QFETCH(QString, language);
        QTranslator translator;
        if (!language.isEmpty()) {
            QVERIFY(translator.load(QStringLiteral(":/i18n/tangenta_") + language));
            QCoreApplication::installTranslator(&translator);
        }
        auto restore = qScopeGuard([&translator] { QCoreApplication::removeTranslator(&translator); });
        MainWindow window;
        QStringList missing;
        std::function<void(QMenu*, const QString&)> walk = [&](QMenu* menu, const QString& path) {
            emit menu->aboutToShow(); // Menüs, die sich beim Öffnen füllen
            for (QAction* a : menu->actions()) {
                if (a->isSeparator() || a->property("dynamicEntry").toBool())
                    continue;
                const QString text = a->text().remove(QLatin1Char('&'));
                if (QMenu* sub = a->menu()) {
                    if (!sub->property("dynamicMenu").toBool())
                        walk(sub, path + QLatin1Char('/') + text);
                    if (a->icon().isNull() && sub->icon().isNull())
                        missing << path + QLatin1Char('\t') + text + QStringLiteral(" >");
                    continue;
                }
                if (a->icon().isNull())
                    missing << path + QLatin1Char('\t') + text;
            }
        };
        for (QAction* top : window.menuBar()->actions())
            if (QMenu* m = top->menu())
                walk(m, top->text().remove(QLatin1Char('&')));
        // Bildschirmfotos einiger Menüs (Symbole im Zusammenhang ansehen)
        if (const QString dir = qEnvironmentVariable("VEK_SCREENSHOT_DIR"); !dir.isEmpty() && language.isEmpty()) {
            int n = 0;
            for (QAction* top : window.menuBar()->actions()) {
                QMenu* m = top->menu();
                if (!m)
                    continue;
                m->popup(QPoint(0, 0));
                QCoreApplication::processEvents();
                m->grab().save(QStringLiteral("%1/menu-%2.png").arg(dir).arg(++n, 2, 10, QLatin1Char('0')));
                m->hide();
            }
        }
        if (const QString dump = qEnvironmentVariable("VEK_MENU_DUMP"); !dump.isEmpty()) {
            QFile f(dump);
            if (f.open(QIODevice::WriteOnly | QIODevice::Text))
                f.write(missing.join(QLatin1Char('\n')).toUtf8());
        }
        QVERIFY2(missing.isEmpty() || qEnvironmentVariableIsSet("VEK_MENU_DUMP"),
                 qPrintable(QStringLiteral("%1 Einträge ohne Symbol, z.B. %2").arg(missing.size()).arg(missing.value(0))));
    }

    // Fehler aus den Videos 34 bis 40
    void enterAppliesInEveryNumberField()
    {
        // Video 36/39: In Effekt- und Vektorisierungsdialogen (QSpinBox) schloss die Eingabetaste den
        // Dialog sofort; jetzt übernimmt sie zuerst den Wert
        installEnterAppliesFilter();
        QImage pixels(20, 20, QImage::Format_RGB32);
        pixels.fill(Qt::gray);
        EffectDialog dialog(QStringLiteral("Test"), pixels, {SliderSpec{QStringLiteral("Wert:"), 0, 100, 10}},
                            [](const QImage& i, const QList<int>&) { return i; });
        dialog.show();
        dialog.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&dialog));
        auto* spin = dialog.findChild<QSpinBox*>();
        QVERIFY(spin);
        spin->setFocus();
        QTRY_VERIFY(spin->hasFocus());
        spin->selectAll();
        QTest::keyClicks(spin, QStringLiteral("42"));
        QTest::keyClick(spin, Qt::Key_Return);
        QVERIFY(dialog.isVisible());
        QCOMPARE(dialog.values().value(0), 42);
        QTest::keyClick(spin, Qt::Key_Return); // Wert steht: jetzt wie OK
        QVERIFY(!dialog.isVisible());
        QCOMPARE(dialog.QDialog::result(), int(QDialog::Accepted));
    }

    // MDI: jedes Dokument ein eigenes Fenster, Fenster > Liste, Wechsel behält Seite und Auswahl
    void multipleDocumentWindows()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        QTemporaryDir dir;
        MainWindow window;
        window.resize(1200, 800);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        Editor& ed = window.canvas()->editor();
        auto* mdi = window.findChild<QMdiArea*>();
        QVERIFY(mdi);
        QCOMPARE(mdi->subWindowList().size(), 1);

        // Ein leeres, unberührtes Dokument wird beim Öffnen ersetzt
        const QString file = dir.filePath(QStringLiteral("zwei.vek"));
        {
            Document other;
            QVERIFY(io::save(other, file));
        }
        QVERIFY(window.openFile(file));
        QCOMPARE(mdi->subWindowList().size(), 1);
        QCOMPARE(ed.documentCount(), 1);

        // Mit Inhalt öffnet ein weiteres Dokument ein weiteres Fenster
        const NodeId first = ed.addObject(std::make_unique<RectangleObject>(QSizeF(20, 10)), QStringLiteral("R"));
        ed.setSelection({first});
        const QString file2 = dir.filePath(QStringLiteral("drei.vek"));
        {
            Document third;
            QVERIFY(io::save(third, file2));
        }
        QVERIFY(window.openFile(file2));
        QCOMPARE(mdi->subWindowList().size(), 2);
        QCOMPARE(ed.documentCount(), 2);
        QCOMPARE(ed.activeDocumentIndex(), 1);
        QCOMPARE(ed.filePath(), file2);
        QVERIFY(!ed.document().find(first));
        // Die Zeichenfläche liegt im aktiven Fenster
        QCOMPARE(mdi->activeSubWindow(), mdi->subWindowList().at(1));
        QVERIFY(mdi->subWindowList().at(1)->isAncestorOf(window.canvas()));

        // Zurück: Dokument, Auswahl und Fensterinhalt wechseln mit
        mdi->setActiveSubWindow(mdi->subWindowList().at(0));
        QCOMPARE(ed.activeDocumentIndex(), 0);
        QCOMPARE(ed.filePath(), file);
        QVERIFY(ed.document().find(first));
        QCOMPARE(ed.selection(), QList<NodeId>{first});
        QVERIFY(mdi->subWindowList().at(0)->isAncestorOf(window.canvas()));

        // Schon offene Datei: nur nach vorne holen
        QVERIFY(window.openFile(file2));
        QCOMPARE(mdi->subWindowList().size(), 2);
        QCOMPARE(ed.activeDocumentIndex(), 1);

        // Fenster-Menü listet beide Dokumente, das aktive mit Häkchen
        QMenu* windowMenu = nullptr;
        for (QAction* a : window.menuBar()->actions())
            if (a->menu() && a->text().remove(QLatin1Char('&')) == QStringLiteral("Fenster"))
                windowMenu = a->menu();
        QVERIFY(windowMenu);
        emit windowMenu->aboutToShow();
        QStringList listed;
        for (QAction* a : windowMenu->actions())
            if (a->isCheckable() && a->text().startsWith(QLatin1Char('&')))
                listed << a->text() + (a->isChecked() ? QStringLiteral(" *") : QString());
        QCOMPARE(listed.size(), 2);
        QVERIFY2(listed.at(1).endsWith(QStringLiteral(" *")), qPrintable(listed.join(QLatin1Char('|'))));

        // Überlappend und nebeneinander
        mdi->cascadeSubWindows();
        mdi->tileSubWindows();
        QVERIFY(!mdi->subWindowList().at(0)->isMaximized());
        shot(window, QStringLiteral("98-mdi-nebeneinander"));

        // Schliessen des unveränderten Fensters: das andere wird aktiv
        ed.document().setClean();
        mdi->subWindowList().at(1)->close();
        QTRY_COMPARE(mdi->subWindowList().size(), 1);
        QCOMPARE(ed.documentCount(), 1);
        QCOMPARE(ed.filePath(), file);
        QVERIFY(ed.document().find(first));
        QVERIFY(mdi->subWindowList().at(0)->isAncestorOf(window.canvas()));
        ed.document().setClean();
    }

    // Zwischenablage: Bild aus einem anderen Programm, SVG, Text; Kopieren liefert SVG/PNG/Text
    void clipboardForeignFormats()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        MainWindow window;
        window.resize(1200, 800);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        Editor& ed = window.canvas()->editor();
        QClipboard* clipboard = QApplication::clipboard();
        auto pasteAction = [&window]() -> QAction* {
            for (QAction* a : window.findChildren<QAction*>())
                if (a->shortcut() == QKeySequence(QKeySequence::Paste))
                    return a;
            return nullptr;
        };
        QAction* paste = pasteAction();
        QVERIFY(paste);
        auto countObjects = [&ed] {
            int n = 0;
            const Page& page = ed.currentPage();
            for (int l = 0; l < page.childCount(); ++l)
                n += page.child(l)->childCount();
            return n;
        };

        // Bildschirmfoto als Bitmap (wie PicPick): wird eingefügt und auf die Seite verkleinert
        QImage shot(3000, 1000, QImage::Format_RGB32);
        shot.fill(Qt::darkCyan);
        clipboard->setImage(shot);
        QTRY_VERIFY(Editor::canPaste());
        window.findChild<QAction*>(); // Aktionen aktualisieren sich über dataChanged
        paste->trigger();
        QCOMPARE(countObjects(), 1);
        QCOMPARE(ed.selection().size(), 1);
        const auto* image = ed.document().findAs<ImageObject>(ed.selection().first());
        QVERIFY(image);
        QVERIFY(image->boundingRect().width() <= ed.currentPage().size().width() + 0.01);
        // Ein Rückgängig-Schritt für Einfügen und Verkleinern
        ed.document().undo();
        QCOMPARE(countObjects(), 0);

        // SVG
        auto* svgMime = new QMimeData;
        svgMime->setData(QStringLiteral("image/svg+xml"),
                         "<svg xmlns='http://www.w3.org/2000/svg' width='40mm' height='20mm' viewBox='0 0 40 20'>"
                         "<rect x='0' y='0' width='40' height='20' fill='#ff0000'/></svg>");
        clipboard->setMimeData(svgMime);
        paste->trigger();
        QCOMPARE(countObjects(), 1);

        // Text wird Grafiktext
        clipboard->setText(QStringLiteral("Hallo Zwischenablage"));
        paste->trigger();
        QCOMPARE(countObjects(), 2);
        const auto* text = ed.document().findAs<TextObject>(ed.selection().value(0));
        QVERIFY(text);
        QCOMPARE(text->plainText(), QStringLiteral("Hallo Zwischenablage"));

        // Kopieren: eigenes Format plus SVG, PNG, Bitmap und Text für andere Programme
        QAction* copy = nullptr;
        for (QAction* a : window.findChildren<QAction*>())
            if (a->shortcut() == QKeySequence(QKeySequence::Copy))
                copy = a;
        QVERIFY(copy);
        copy->trigger();
        const QMimeData* copied = clipboard->mimeData();
        QVERIFY(copied->hasFormat(Editor::clipboardMime()));
        QVERIFY(copied->hasFormat(QStringLiteral("image/svg+xml")));
        QVERIFY(copied->hasFormat(QStringLiteral("image/png")));
        QVERIFY(copied->hasImage());
        QCOMPARE(copied->text(), QStringLiteral("Hallo Zwischenablage"));
        // und wieder einfügen: eigenes Format hat Vorrang
        paste->trigger();
        QCOMPARE(countObjects(), 3);
        QVERIFY(ed.document().findAs<TextObject>(ed.selection().value(0)));

        // Datei in der Zwischenablage (Explorer: Kopieren)
        QTemporaryDir dir;
        const QString png = dir.filePath(QStringLiteral("bild.png"));
        QVERIFY(shot.scaled(200, 100).save(png));
        auto* files = new QMimeData;
        files->setUrls({QUrl::fromLocalFile(png)});
        clipboard->setMimeData(files);
        paste->trigger();
        QCOMPARE(countObjects(), 4);
        QVERIFY(ed.document().findAs<ImageObject>(ed.selection().value(0)));
        ed.document().setClean();
    }

    // Die Eigenschaftsleiste darf das Fenster nicht über den Bildschirm hinaus verbreitern
    void propertyBarNeverWidensWindow()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        MainWindow window;
        window.resize(1000, 700);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        // Leiste kann in den Test-Optionen ausgeblendet sein
        auto* bar = window.findChild<PropertyBar*>();
        QVERIFY(bar);
        bar->show();
        Editor& ed = window.canvas()->editor();
        auto t = std::make_unique<TextObject>(TextObject::Kind::Artistic);
        t->setPlainText(QStringLiteral("Text"), CharFormat(QStringLiteral("Arial"), 24));
        const NodeId text = ed.addObject(std::move(t), QStringLiteral("T"));
        const NodeId rect = ed.addObject(std::make_unique<RectangleObject>(QSizeF(20, 10)), QStringLiteral("R"));
        QImage pixels(40, 40, QImage::Format_RGB32);
        pixels.fill(Qt::red);
        const NodeId image = ed.importImage(pixels);
        int widest = 0, tallest = 0;
        QString culprit;
        for (const QList<NodeId>& selection : QList<QList<NodeId>>{{}, {text}, {rect}, {image}, {text, rect}}) {
            ed.setSelection(selection);
            for (int tool = 0; tool < int(ToolId::Count); ++tool) {
                window.canvas()->setTool(ToolId(tool));
                QCoreApplication::processEvents();
                const QSize min = window.minimumSizeHint().expandedTo(window.minimumSize());
                if (min.width() > widest || min.height() > tallest)
                    culprit = QStringLiteral("Werkzeug %1, Auswahl %2: %3 x %4").arg(tool).arg(selection.size()).arg(min.width()).arg(min.height());
                widest = std::max(widest, min.width());
                tallest = std::max(tallest, min.height());
            }
        }
        window.canvas()->setTool(ToolId::Pick);
        // Alle Andockfenster offen (gestapelt bzw. als Reiter)
        for (QDockWidget* dock : window.findChildren<QDockWidget*>()) {
            dock->show();
            QCoreApplication::processEvents();
            const QSize min = window.minimumSizeHint().expandedTo(window.minimumSize());
            if (min.width() > widest || min.height() > tallest)
                culprit = QStringLiteral("Andockfenster %1: %2 x %3").arg(dock->windowTitle()).arg(min.width()).arg(min.height());
            widest = std::max(widest, min.width());
            tallest = std::max(tallest, min.height());
        }
        // Auch auf kleinen Bildschirmen (offscreen 800 x 600) bleibt das Fenster im Bildschirm
        const QRect avail = window.screen()->availableGeometry();
        qInfo() << "Mindestgrösse höchstens" << widest << tallest << "Bildschirm" << avail.size();
        QVERIFY2(widest <= std::max(avail.width(), 1000) && tallest <= std::max(avail.height(), 700), qPrintable(culprit));
        QVERIFY(window.frameGeometry().width() <= std::max(avail.width(), 1000));
        // Schriftgrösse: Feld breit genug für "1234.5 pt" (neuer Text: die Werkzeuge oben können
        // die Auswahl verändert haben)
        auto t2 = std::make_unique<TextObject>(TextObject::Kind::Artistic);
        t2->setPlainText(QStringLiteral("Text"), CharFormat(QStringLiteral("Arial"), 24));
        ed.setSelection({ed.addObject(std::move(t2), QStringLiteral("T2"))});
        QCoreApplication::processEvents();
        QComboBox* size = nullptr;
        QCoreApplication::processEvents();
        for (QComboBox* box : window.findChildren<QComboBox*>())
            if (box->isVisibleTo(&window) && box->toolTip() == QStringLiteral("Schriftgrösse in Punkt"))
                size = box;
        QVERIFY(size);
        const int needed = size->fontMetrics().horizontalAdvance(QStringLiteral("1234.5 pt"));
        QVERIFY2(size->lineEdit()->width() >= needed, qPrintable(QStringLiteral("%1 < %2").arg(size->lineEdit()->width()).arg(needed)));
        ed.document().setClean();
    }

    void videoFixes34to40()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        MainWindow window;
        window.resize(1200, 800);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        Canvas* canvas = window.canvas();
        Editor& ed = canvas->editor();
        Document& doc = ed.document();

        // Video 38: Mehrseitenansicht ein/aus ist nur Ansicht: kein Rückgängig-Schritt, nicht geändert
        doc.setClean();
        const int undo = doc.undoCount();
        canvas->setMultiPageView(true);
        QVERIFY(canvas->multiPageView());
        QVERIFY(!doc.isModified());
        QCOMPARE(doc.undoCount(), undo);
        canvas->setMultiPageView(false);
        QVERIFY(!doc.isModified());

        // Video 36: Begrenzung schliesst Löcher (Ring -> volle Scheibe)
        QPainterPath ring;
        ring.addEllipse(QPointF(60, 60), 30, 30);
        ring.addEllipse(QPointF(60, 60), 15, 15);
        auto curve = CurveObject::fromPath(ring);
        const NodeId ringId = ed.addObject(std::move(curve), QStringLiteral("Ring"));
        ed.setSelection({ringId});
        QVERIFY(ed.shapeSelection(Editor::ShapeOp::Boundary));
        const auto* outline = doc.findAs<DrawObject>(ed.selection().value(0));
        QVERIFY(outline && outline->id() != ringId);
        QPainterPath area = outline->path();
        area.setFillRule(Qt::OddEvenFill);
        QVERIFY(area.contains(QPointF(60, 60)));

        // Video 39: Punktraster rechnet die Rasterweite in mm der platzierten Grösse
        QImage pixels(1536, 1024, QImage::Format_RGB32);
        pixels.fill(Qt::darkGreen);
        auto image = std::make_unique<ImageObject>(imagepool::addImage(pixels), QSizeF(406.4, 270.9));
        image->setTransform(QTransform::fromScale(63.0 / 406.4, 63.0 / 406.4) * QTransform::fromTranslate(10, 10));
        const NodeId imageId = ed.addObject(std::move(image), QStringLiteral("Bild"));
        ed.setSelection({imageId});
        const std::optional<MosaicSource> src = mosaicSource(*canvas, 150.0, true);
        QVERIFY(src);
        QVERIFY2(std::abs(src->size.width() - 63.0) < 0.01, qPrintable(QString::number(src->size.width())));
        QVERIFY(near(src->toPage.mapRect(QRectF(QPointF(0, 0), src->size)), doc.findAs<DrawObject>(imageId)->boundingRect(), 0.01));
    }

    void textToolFollowsResize()
    {
        // Video 30: Höhe eines Grafiktexts ändern, während das Textwerkzeug ihn bearbeitet: die Buchstaben
        // der Arbeitskopie folgen (vorher nur der Auswahlrahmen)
        MainWindow window;
        window.resize(1200, 800);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        Canvas* canvas = window.canvas();
        Editor& ed = canvas->editor();
        auto text = std::make_unique<TextObject>(TextObject::Kind::Artistic);
        text->setPlainText(QStringLiteral("HALLO"), CharFormat(QStringLiteral("Arial"), 48));
        text->setStyle([] { Style s; s.fill = Fill::solid(Color(0, 0, 0)); return s; }());
        text->applyTransform(xform::translation(QPointF(30, 40)));
        const NodeId t = ed.addObject(std::move(text), QStringLiteral("T"));
        canvas->setTool(ToolId::Text);
        canvas->textTool().beginEditing(t);
        auto dark = [&] {
            QImage img = canvas->grab().toImage();
            int n = 0;
            for (int y = 0; y < img.height(); y += 2)
                for (int x = 0; x < img.width(); x += 2)
                    n += qGray(img.pixel(x, y)) < 60;
            return n;
        };
        const int before = dark();
        const QRectF b = ed.document().findAs<TextObject>(t)->boundingRect();
        ed.setSelection({t});
        ed.setSelectionBounds(QRectF(b.left(), b.top(), b.width(), 2 * b.height()));
        const int after = dark();
        QVERIFY2(after > 1.6 * before, qPrintable(QStringLiteral("%1 -> %2").arg(before).arg(after)));
        canvas->textTool().finishEditing();
        QVERIFY(std::abs(ed.document().findAs<TextObject>(t)->boundingRect().height() - 2 * b.height()) < 0.01);
    }

    void envelopeOnScreenAfterCurves()
    {
        // Video 30: Text in Kurven, dann «Hülle aus Objekt» über die Eigenschaftsleiste: auf dem Bildschirm
        // erscheint die verformte Form sofort, Füllung und Kontur wirken danach weiter
        MainWindow window;
        window.resize(1200, 800);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        Canvas* canvas = window.canvas();
        Editor& ed = canvas->editor();
        auto text = std::make_unique<TextObject>(TextObject::Kind::Artistic);
        text->setPlainText(QStringLiteral("HALLO"), CharFormat(QStringLiteral("Arial"), 60));
        Style black;
        black.fill = Fill::solid(Color(0, 0, 0));
        text->setStyle(black);
        text->applyTransform(xform::translation(QPointF(30, 40)));
        const NodeId t = ed.addObject(std::move(text), QStringLiteral("T"));
        ed.setSelection({t});
        ed.convertSelectionToCurves();
        ed.combineSelection();
        const NodeId c = ed.selection().first();
        const QRectF plain = ed.document().findAs<DrawObject>(c)->path().boundingRect();
        auto oval = std::make_unique<EllipseObject>(QSizeF(plain.width(), plain.height() * 2));
        oval->applyTransform(xform::translation(QPointF(plain.left(), plain.bottom() + 20)));
        const NodeId target = ed.addObject(std::move(oval), QStringLiteral("E"));
        ed.setSelection({c});
        canvas->setTool(ToolId::Envelope);
        QApplication::processEvents();
        auto darkInCorner = [&] {
            const QImage img = canvas->grab().toImage();
            const QRectF r = canvas->viewTransform().mapRect(QRectF(plain.topLeft(), QSizeF(plain.width() * 0.05, plain.height() * 0.2)));
            int n = 0;
            for (int y = int(r.top()) + 1; y < int(r.bottom()); ++y)
                for (int x = int(r.left()) + 1; x < int(r.right()); ++x)
                    n += qGray(img.pixel(x, y)) < 100;
            return n;
        };
        QVERIFY(darkInCorner() > 0); // Ecke des H
        // Wie der Knopf «Hülle aus Objekt erstellen»: Zielobjekt anklicken
        canvas->setTool(ToolId::Pick);
        bool picked = false;
        canvas->pickTool().beginPickTarget([&](NodeId source) {
            ed.applyEnvelopeShape(ed.document().findAs<DrawObject>(source)->path(), true, Envelope::Mode::Unconstrained, false,
                                  Envelope::Mapping::Putty);
            canvas->setTool(ToolId::Envelope);
            picked = true;
        });
        const QPointF onOval = canvas->viewTransform().map(QPointF(plain.center().x(), plain.bottom() + 20 + plain.height()));
        QTest::mouseClick(canvas, Qt::LeftButton, {}, onOval.toPoint());
        QVERIFY(picked);
        QVERIFY(ed.document().findAs<DrawObject>(c)->hasEnvelope());
        // ohne Auswahl und Griffe nachsehen
        ed.setSelection({});
        canvas->setTool(ToolId::Pick);
        QApplication::processEvents();
        QCOMPARE(darkInCorner(), 0);
        // Füllung und Kontur auf der verformten Kurve
        ed.setSelection({c});
        ed.setSelectionStrokeWidth(0.5);
        const auto* curve = ed.document().findAs<CurveObject>(c);
        QCOMPARE(curve->style().strokeWidth, 0.5);
        QVERIFY(curve->hasEnvelope());
        ed.setSelection({c, target});
        ed.deleteSelection();
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
        // Die Liste öffnet sich bei der Schrift in der alphabetischen Liste, nicht bei "zuletzt benutzt"
        if (QFontDatabase::families().contains(QStringLiteral("Courier New"))) {
            box.setCurrentFamily(QStringLiteral("courier new"));
            QVERIFY(box.currentIndex() > 2);
            QCOMPARE(box.itemData(box.currentIndex()).toString(), QStringLiteral("Courier New"));
            // Suchen: Teil des Namens tippen, Vorschlag wählen
            QStringList chosen;
            connect(&box, &FontBox::familyActivated, this, [&chosen](const QString& f) { chosen << f; });
            box.completer()->setCompletionPrefix(QStringLiteral("ourier"));
            QVERIFY(box.completer()->completionCount() >= 1);
            QCOMPARE(box.completer()->currentCompletion(), QStringLiteral("Courier New"));
            emit box.completer()->activated(box.completer()->currentCompletion());
            QCOMPARE(chosen, QStringList{QStringLiteral("Courier New")});
            // Unbekannter Text stellt die aktuelle Schrift wieder her
            box.lineEdit()->setText(QStringLiteral("xyz"));
            emit box.lineEdit()->editingFinished();
            QCOMPARE(box.currentText(), QStringLiteral("Courier New"));
            QCOMPARE(chosen.size(), 1);
            // Genau getippter Name mit weiteren Treffern (Arial, Arial Black …): schliesst sich die
            // Trefferliste durch einen Klick daneben, gilt der getippte Name
            // eine Schrift, deren Name in weiteren steckt (Arial -> Arial Black …)
            const QStringList names = FontBox::families(m_editor->document());
            QString prefix;
            for (const QString& f : names)
                if (prefix.isEmpty() && f != QStringLiteral("Courier New")
                    && std::count_if(names.begin(), names.end(), [&f](const QString& g) { return g.contains(f, Qt::CaseInsensitive); }) >= 2)
                    prefix = f;
            if (!prefix.isEmpty()) {
                box.show();
                box.lineEdit()->setText(prefix.toLower());
                box.completer()->setCompletionPrefix(prefix.toLower());
                box.completer()->complete();
                QVERIFY(box.completer()->completionCount() >= 2);
                box.completer()->popup()->hide();
                QTRY_COMPARE(chosen.last(), prefix);
                box.hide();
            }
        }
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

    void toolShortcutsAreCommands()
    {
        // Werkzeugtasten sind vorbelegte Tastaturbefehle (Optionen > Befehle > Hilfsmittel) und frei belegbar
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest")); // sonst kein QSettings
        Options::instance().set(QStringLiteral("ws/custom/shortcuts"),
                                QStringLiteral(R"({"Hilfsmittel/Ellipse":"Ctrl+Shift+F7"})"));
        {
            MainWindow window;
            window.show();
            window.activateWindow();
            QVERIFY(QTest::qWaitForWindowActive(&window));
            auto* canvas = window.findChild<Canvas*>();
            QVERIFY(canvas);
            canvas->setFocus();
            QTest::keyClick(canvas, Qt::Key_F6);
            QCOMPARE(canvas->toolId(), ToolId::Rectangle);
            QTest::keyClick(canvas, Qt::Key_F7); // Vorgabe F7 ist abgelöst
            QCOMPARE(canvas->toolId(), ToolId::Rectangle);
            QTest::keyClick(canvas, Qt::Key_F7, Qt::ControlModifier | Qt::ShiftModifier);
            QCOMPARE(canvas->toolId(), ToolId::Ellipse);
            QTest::keyClick(canvas, Qt::Key_Y);
            QCOMPARE(canvas->toolId(), ToolId::Polygon);
            QAction* ellipse = nullptr;
            for (QAction* a : window.findChild<QMenu*>(QStringLiteral("toolCommands"))->actions())
                if (a->property("toolName").toString() == QStringLiteral("Ellipse"))
                    ellipse = a;
            QVERIFY(ellipse);
            QVERIFY(ellipse->toolTip().contains(QKeySequence(QStringLiteral("Ctrl+Shift+F7")).toString(QKeySequence::NativeText)));
            // Leertaste und Schubsen sind ebenfalls Befehle der Arbeitsfläche
            QTest::keyClick(canvas, Qt::Key_Space);
            QCOMPARE(canvas->toolId(), ToolId::Pick);
            QTest::keyClick(canvas, Qt::Key_Space);
            QCOMPARE(canvas->toolId(), ToolId::Polygon);
            QTest::keyClick(canvas, Qt::Key_F6);
            QTest::mouseMove(canvas, QPoint(100, 100));
            QTest::mousePress(canvas, Qt::LeftButton, {}, QPoint(100, 100));
            QTest::mouseMove(canvas, QPoint(200, 160));
            QTest::mouseRelease(canvas, Qt::LeftButton, {}, QPoint(200, 160));
            auto& editor = window.findChild<Canvas*>()->editor();
            QCOMPARE(editor.selection().size(), 1);
            canvas->setTool(ToolId::Pick);
            const double left = editor.selectionBounds().left();
            QTest::keyClick(canvas, Qt::Key_Right);
            QVERIFY(std::abs(editor.selectionBounds().left() - left - editor.nudge()) < 1e-6);
            const int count = int(editor.pickableObjects().size());
            QTest::keyClick(canvas, Qt::Key_Plus, Qt::KeypadModifier);
            QCOMPARE(int(editor.pickableObjects().size()), count + 1);
        }
        Options::instance().set(QStringLiteral("ws/custom/shortcuts"), QString());
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
        // Zwischenstufen werden nur neu berechnet, wenn sich Start, Ende oder Schritte ändern
        {
            const auto* bl = node_cast<BlendObject>(obj(blend->id()));
            QVERIFY(bl->intermediates().first() == bl->intermediates().first());
            const auto before = bl->intermediates().first();
            m_editor->setSelection({b});
            m_editor->transformSelection(QTransform::fromTranslate(0, 10), QStringLiteral("Verschieben"));
            const auto after = node_cast<BlendObject>(obj(blend->id()))->intermediates().first();
            QVERIFY(after != before);
            QVERIFY(after->boundingRect().center().y() > before->boundingRect().center().y());
            QVERIFY(node_cast<BlendObject>(obj(blend->id()))->boundingRect().bottom() > 34.0);
            m_editor->setSelection({blend->id()});
        }
        // Strg+K ist bei einer Überblendung wählbar
        QVERIFY(m_editor->canBreakApartSelection());
        // Trennen (Strg+K): Gruppe aus 11 Objekten
        QVERIFY(m_editor->separateEffects());
        QCOMPARE(m_editor->document().find(m_editor->selection().first())->childCount(), 11);
        Q_UNUSED(a);

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
    void qrCodeDialogInsertsAndEdits()
    {
        // Dialog: WLAN-Code mit Feldern
        QrCodeObject start;
        QrCodeDialog dialog(start, true);
        QVERIFY(dialog.isValid());
        QVERIFY(dialog.info().contains(QStringLiteral("Version")));
        dialog.kindBox()->setCurrentIndex(int(qr::Kind::Wifi));
        auto* ssid = qobject_cast<QLineEdit*>(dialog.field(QStringLiteral("ssid")));
        auto* password = qobject_cast<QLineEdit*>(dialog.field(QStringLiteral("password")));
        auto* security = qobject_cast<QComboBox*>(dialog.field(QStringLiteral("security")));
        QVERIFY(ssid && password && security);
        ssid->setText(QStringLiteral("Gast"));
        password->setText(QStringLiteral("sommer26"));
        security->setCurrentIndex(0);
        QrCodeObject result;
        dialog.applyTo(result);
        QCOMPARE(result.payload(), QStringLiteral("WIFI:T:WPA;S:Gast;P:sommer26;;"));
        if (const QString shots = qEnvironmentVariable("VEK_SCREENSHOT_DIR"); !shots.isEmpty()) {
            dialog.adjustSize();
            dialog.grab().save(shots + QStringLiteral("/95-qrcode-dialog.png"));
        }
        QCOMPARE(result.size(), QSizeF(30, 30));
        // Zu langer Inhalt sperrt OK
        dialog.kindBox()->setCurrentIndex(int(qr::Kind::Text));
        qobject_cast<QPlainTextEdit*>(dialog.field(QStringLiteral("text")))->setPlainText(QString(4000, u'x'));
        QVERIFY(!dialog.isValid());

        // Einfügen über den Editor, dann Bearbeiten als ein Undo-Schritt
        auto code = std::make_unique<QrCodeObject>();
        code->setSize(QSizeF(25, 25));
        const NodeId id = m_editor->addObject(std::move(code), QStringLiteral("QR"));
        const auto* placed = m_editor->document().findAs<QrCodeObject>(id);
        QVERIFY(placed);
        QCOMPARE(placed->payload(), QStringLiteral("https://tangenta.ch"));
        QCOMPARE(placed->boundingRect().size(), QSizeF(25, 25));
        QrCodeDialog edit(*placed, false);
        edit.kindBox()->setCurrentIndex(int(qr::Kind::Phone));
        qobject_cast<QLineEdit*>(edit.field(QStringLiteral("number")))->setText(QStringLiteral("052 123 45 67"));
        m_editor->modifyObject(id, [&edit](DrawObject& o) { edit.applyTo(*node_cast<QrCodeObject>(&o)); }, QStringLiteral("QR"));
        QCOMPARE(m_editor->document().findAs<QrCodeObject>(id)->payload(), QStringLiteral("tel:0521234567"));
        QCOMPARE(m_editor->document().findAs<QrCodeObject>(id)->size(), QSizeF(25, 25));
        m_editor->document().undo();
        QCOMPARE(m_editor->document().findAs<QrCodeObject>(id)->payload(), QStringLiteral("https://tangenta.ch"));
        // Leiste: Fehlerkorrektur über setObjectProperty
        m_editor->setSelection({id});
        m_editor->setObjectProperty(QStringLiteral("qrEcc"), 3, QStringLiteral("Fehlerkorrektur"));
        QCOMPARE(m_editor->document().findAs<QrCodeObject>(id)->errorCorrection(), 3);
    }

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

    // Ohne Schriftdaten (CDR): Art, Gewicht und Breite aus Name bzw. Google-Fonts-Verzeichnis gegen die
    // OS/2-Tabelle der installierten Schriften; höchstens 70 %, weil nur vermutet
    void fontMatchWithoutFontData()
    {
        const QString sans = QStringLiteral("Arial"), serif = QStringLiteral("Times New Roman");
        if (!QFontDatabase::hasFamily(sans) || !QFontDatabase::hasFamily(serif))
            QSKIP("Arial und Times New Roman nötig");
        auto scoreOf = [](const QList<fontmatch::Candidate>& c, const QString& family) {
            for (const fontmatch::Candidate& x : c)
                if (x.family == family)
                    return x.score;
            return -1.0;
        };
        auto top = [](const QList<fontmatch::Candidate>& c) {
            QStringList names;
            for (int i = 0; i < std::min<qsizetype>(5, c.size()); ++i)
                names << QStringLiteral("%1 %2%").arg(c[i].family).arg(int(std::lround(c[i].score * 100)));
            return names.join(QStringLiteral(", "));
        };
        auto suggest = [](const QString& name, bool bold = false) {
            ImportedFont f;
            f.family = f.baseName = name;
            f.bold = bold;
            return fontmatch::suggest(f, 1000);
        };
        const QList<fontmatch::Candidate> garamond = suggest(QStringLiteral("Garamond Premier Pro"));
        qInfo("Garamond Premier Pro: %s", qPrintable(top(garamond)));
        QVERIFY(garamond.first().traits >= 0 && garamond.first().widths < 0);
        QVERIFY2(scoreOf(garamond, serif) > scoreOf(garamond, sans), qPrintable(top(garamond)));
        // Ohne bekanntes Gegenstück nur eine Vermutung: höchstens 70 %
        const QList<fontmatch::Candidate> minion = suggest(QStringLiteral("Minion Pro"));
        qInfo("Minion Pro: %s", qPrintable(top(minion)));
        QVERIFY2(minion.first().score <= 0.701 && scoreOf(minion, serif) > scoreOf(minion, sans), qPrintable(top(minion)));

        const QList<fontmatch::Candidate> helvetica = suggest(QStringLiteral("Helvetica Neue"));
        qInfo("Helvetica Neue: %s", qPrintable(top(helvetica)));
        QVERIFY2(scoreOf(helvetica, sans) > scoreOf(helvetica, serif), qPrintable(top(helvetica)));
        QVERIFY2(helvetica.first().family == sans, qPrintable(top(helvetica))); // bewährte Ersatzschrift zuerst

        // Gewicht aus dem Namen: Black passt zu Arial Black besser als zu Arial
        const QList<fontmatch::Candidate> black = suggest(QStringLiteral("Helvetica Black"));
        if (QFontDatabase::hasFamily(QStringLiteral("Arial Black")))
            QVERIFY2(scoreOf(black, QStringLiteral("Arial Black")) > scoreOf(black, sans), qPrintable(top(black)));

        // Art aus dem Namen: Courier ist dicktengleich
        const QList<fontmatch::Candidate> courier = suggest(QStringLiteral("Courier Std"));
        if (QFontDatabase::hasFamily(QStringLiteral("Courier New")))
            QVERIFY2(scoreOf(courier, QStringLiteral("Courier New")) > scoreOf(courier, sans), qPrintable(top(courier)));
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

    // CDR schreiben und mit dem Import zurücklesen: gleiche Seitenzahl, gleiches Bild (auch die
    // Stapelreihenfolge), keine Markennamen aus der Vorlage
    void cdrRoundTrip_data()
    {
        QTest::addColumn<bool>("version13");
        QTest::newRow("X6") << false;
        QTest::newRow("X3") << true;
    }

    void cdrRoundTrip()
    {
        QFETCH(bool, version13);
        auto rect = std::make_unique<RectangleObject>(QSizeF(60, 40));
        rect->setTransform(QTransform::fromTranslate(20, 30));
        Style rs;
        rs.fill = Fill::solid(Color(220, 0, 0));
        rs.stroke = Color(0, 0, 255);
        rs.strokeWidth = 2.0;
        rect->setStyle(rs);
        m_editor->addObject(std::move(rect), QStringLiteral("R"));
        auto ell = std::make_unique<EllipseObject>(QSizeF(50, 50));
        ell->setTransform(QTransform::fromTranslate(60, 50));
        Style es;
        es.fill = Fill::gradient(Fill::Type::Linear, Color(255, 255, 0), Color(0, 128, 0));
        es.stroke.reset();
        ell->setStyle(es);
        m_editor->addObject(std::move(ell), QStringLiteral("E"));
        auto disc = std::make_unique<EllipseObject>(QSizeF(60, 60));
        disc->setTransform(QTransform::fromTranslate(30, 150));
        Style ds;
        ds.fill = Fill::gradient(Fill::Type::Radial, Color(255, 255, 255), Color(0, 0, 160));
        ds.stroke.reset();
        disc->setStyle(ds);
        m_editor->addObject(std::move(disc), QStringLiteral("D"));
        auto text = std::make_unique<TextObject>(TextObject::Kind::Artistic);
        text->setPlainText(QStringLiteral("Tangenta"), CharFormat{QStringLiteral("Arial"), 36.0});
        text->setTransform(QTransform::fromTranslate(110, 160));
        Style ts;
        ts.fill = Fill::solid(Color(0, 0, 0));
        ts.stroke.reset();
        text->setStyle(ts);
        m_editor->addObject(std::move(text), QStringLiteral("T"));
        output::ExportOptions o;
        o.dpi = 30;
        const QImage before = output::renderImage(*m_canvas, o);
        QString error;
        const QByteArray data = output::areaCdr(*m_canvas, o, &error, version13);
        QVERIFY2(!data.isEmpty(), qPrintable(error));
        if (const QString keep = qEnvironmentVariable("VEK_CDR_OUT"); !keep.isEmpty()) {
            QFile f(keep + (version13 ? QStringLiteral("-x3.cdr") : QStringLiteral("-x6.cdr")));
            QVERIFY(f.open(QIODevice::WriteOnly) && f.write(data) == data.size());
        }
        QVERIFY(version13 ? data.startsWith("RIFF") && data.mid(8, 4) == "CDRD" : data.startsWith("PK"));
        QVERIFY(cdr::isSupported(data));
        // Markenname der Vorlage darf nicht vorkommen (klein, ASCII und UTF-16)
        const QByteArray word = QByteArray::fromHex("636f72656c");
        const QByteArray lower = data.toLower();
        QByteArray wide;
        for (char ch : word)
            wide.append(ch).append(char(0));
        QVERIFY(!lower.contains(word) && !lower.contains(wide));
        // X6 ist gepackt: die Vorlage selbst prüfen
        for (const char* name : {":/cdr/x3-template.riff", ":/cdr/x6-template.riff"}) {
            QFile f{QString::fromLatin1(name)};
            QVERIFY(f.open(QIODevice::ReadOnly));
            const QByteArray tpl = f.readAll().toLower();
            QVERIFY2(!tpl.contains(word) && !tpl.contains(wide), name);
        }
        std::vector<svg::Import> pages = cdr::read(data, &error);
        QVERIFY2(pages.size() == 1 && pages.front().group, qPrintable(error));
        QVERIFY(std::abs(pages.front().size.width() - 210) < 0.1 && std::abs(pages.front().size.height() - 297) < 0.1);
        m_editor->setDocument(std::make_unique<Document>(), QString());
        m_editor->setPageSize(pages.front().size, true);
        m_editor->importGroup(std::move(pages.front().group), false, QStringLiteral("CDR"));
        const QImage after = output::renderImage(*m_canvas, o);
        QCOMPARE(after.size(), before.size());
        // Mittlere Abweichung je Kanal und Bildpunkt; Kanten und Verläufe weichen etwas ab
        double sum = 0;
        for (int y = 0; y < before.height(); ++y)
            for (int x = 0; x < before.width(); ++x) {
                const QRgb a = before.pixel(x, y), b = after.pixel(x, y);
                sum += std::abs(qRed(a) - qRed(b)) + std::abs(qGreen(a) - qGreen(b)) + std::abs(qBlue(a) - qBlue(b));
            }
        const double mean = sum / (3.0 * before.width() * before.height());
        QVERIFY2(mean < 2.0, qPrintable(QString::number(mean)));
        // Ellipse liegt über dem Rechteck: Überschneidung bei (70, 60) mm ist nicht rot
        const QPoint overlap(int(70 / 25.4 * 30), int(60 / 25.4 * 30));
        QVERIFY(qRed(after.pixel(overlap)) > 100 && qGreen(after.pixel(overlap)) > 100);
        QCOMPARE(after.pixel(overlap), before.pixel(overlap));
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
        for (const QString& name : {QStringLiteral("Freihand"), QStringLiteral("2-Punkt-Linie"), QStringLiteral("Bézier"),
                                    QStringLiteral("Künstlerische Medien"), QStringLiteral("Beschneiden"),
                                    QStringLiteral("Messer"), QStringLiteral("Radierer")}) {
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

    // Rückmeldung ROKA 04.11.: Bild auf/ab blättert, Kontextmenü gruppiert, Strg+Bild auf/ab über
    // Ebenengrenzen, geöffnete Dateien ohne äussere Gruppe, Fenster passt auf den Bildschirm
    void pagesGroupsAndStacking()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        QSettings().remove(QStringLiteral("window/geometry"));
        MainWindow window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        Editor& ed = window.canvas()->editor();
        Canvas* canvas = window.canvas();
        const QRect avail = window.screen()->availableGeometry();
        // Kleiner Laptop (1366 x 768 abzüglich Taskleiste): das Fenster muss sich so klein machen lassen
        QVERIFY2(window.minimumSizeHint().height() <= 700, qPrintable(QString::number(window.minimumSizeHint().height())));
        QVERIFY2(window.frameGeometry().height() <= avail.height() && window.frameGeometry().top() >= avail.top(),
                 qPrintable(QStringLiteral("Fenster %1, Bildschirm %2, Mindesthöhe %3")
                                .arg(window.frameGeometry().height()).arg(avail.height()).arg(window.minimumSizeHint().height())));

        auto byShortcut = [&window](const QKeySequence& key) -> QAction* {
            for (QAction* a : window.findChildren<QAction*>())
                if (a->shortcut() == key)
                    return a;
            return nullptr;
        };
        // Bild ab / Bild auf blättern
        ed.insertPages(1, 2);
        ed.setCurrentPageIndex(0);
        QAction* next = byShortcut(QKeySequence(Qt::Key_PageDown));
        QAction* prev = byShortcut(QKeySequence(Qt::Key_PageUp));
        QVERIFY(next && prev);
        next->trigger();
        QCOMPARE(ed.currentPageIndex(), 1);
        next->trigger();
        next->trigger();
        QCOMPARE(ed.currentPageIndex(), 2);
        prev->trigger();
        QCOMPARE(ed.currentPageIndex(), 1);
        ed.setCurrentPageIndex(0);

        // Kontextmenü: Gruppieren nur bei mehreren Objekten, Gruppierung aufheben nur bei Gruppen
        auto add = [&ed](const QRectF& r) {
            auto o = std::make_unique<RectangleObject>(r.size());
            o->setTransform(QTransform::fromTranslate(r.left(), r.top()));
            return ed.addObject(std::move(o), QStringLiteral("R"));
        };
        const NodeId a = add(QRectF(20, 20, 30, 20));
        const NodeId b = add(QRectF(40, 30, 30, 20));
        QMap<QString, bool> entries;
        QTimer poll;
        poll.setInterval(20);
        QObject::connect(&poll, &QTimer::timeout, [&] {
            auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            if (!menu || !menu->isVisible())
                return;
            for (QAction* act : menu->actions())
                entries.insert(act->text(), act->isEnabled());
            menu->close();
            poll.stop();
        });
        auto openMenu = [&] {
            entries.clear();
            poll.start();
            QTest::mouseClick(canvas, Qt::RightButton, {}, canvas->toScreen(ed.selectionBounds().center()).toPoint());
            QTRY_VERIFY(!entries.isEmpty());
        };
        const QString group = QStringLiteral("&Gruppieren"), ungroup = QStringLiteral("Gruppierung &aufheben");
        ed.setSelection({a, b});
        openMenu();
        QVERIFY(entries.contains(group) && entries.contains(ungroup));
        QVERIFY(entries.value(group));
        QVERIFY(!entries.value(ungroup));
        ed.groupSelection();
        openMenu();
        QVERIFY(!entries.value(group));
        QVERIFY(entries.value(ungroup));
        ed.ungroupSelection();

        // Strg+Bild auf: zuoberst in der Ebene geht es in die nächste Ebene darüber und weiter
        const Node* page = &ed.currentPage();
        const NodeId lower = page->child(page->childCount() - 1)->id();
        NodeId upper = InvalidNodeId;
        {
            Transaction tx(ed.document(), QStringLiteral("Ebene"));
            auto layer = std::make_unique<Layer>();
            layer->setName(QStringLiteral("Oben"));
            upper = tx.insert(page->id(), -1, std::move(layer));
            tx.commit();
        }
        const NodeId c = add(QRectF(60, 40, 20, 20)); // landet in der neuen, obersten Ebene
        QCOMPARE(ed.document().find(c)->parent()->id(), upper);
        QAction* forward = byShortcut(QKeySequence(Qt::CTRL | Qt::Key_PageUp));
        QAction* back = byShortcut(QKeySequence(Qt::CTRL | Qt::Key_PageDown));
        QVERIFY(forward && back);
        ed.setSelection({b}); // zuoberst in der unteren Ebene
        QApplication::processEvents();
        QVERIFY(forward->isEnabled());
        forward->trigger();
        QCOMPARE(ed.document().find(b)->parent()->id(), upper);
        QCOMPARE(ed.document().find(b)->indexInParent(), 0); // zuunterst in der oberen Ebene
        forward->trigger();
        QCOMPARE(ed.document().find(b)->indexInParent(), 1); // vor c
        QApplication::processEvents();
        QVERIFY(!forward->isEnabled());
        back->trigger();
        back->trigger();
        QCOMPARE(ed.document().find(b)->parent()->id(), lower); // wieder unten, dort zuoberst
        QCOMPARE(ed.document().find(b)->indexInParent(), ed.document().find(lower)->childCount() - 1);
        back->trigger();
        QCOMPARE(ed.document().find(b)->indexInParent(), ed.document().find(a)->indexInParent() - 1);

        // Als neues Dokument geöffnet: Objekte einzeln auf der Ebene, nicht in einer Seitengruppe
        const QString svgPath = QDir::temp().filePath(QStringLiteral("vek-open-test.svg"));
        {
            QFile f(svgPath);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("<svg xmlns='http://www.w3.org/2000/svg' width='100mm' height='100mm' viewBox='0 0 100 100'>"
                    "<rect x='10' y='10' width='20' height='20' fill='#0B2545'/><g><circle cx='60' cy='60' r='10'/>"
                    "<circle cx='80' cy='60' r='10'/></g></svg>");
        }
        ed.document().setClean();
        QVERIFY(window.openFile(svgPath));
        const Node* layer = ed.document().page(0).child(ed.document().page(0).childCount() - 1);
        QCOMPARE(layer->childCount(), 2);
        QVERIFY(!ed.document().findAs<GroupObject>(layer->child(0)->id()));
        QVERIFY(ed.document().findAs<GroupObject>(layer->child(1)->id()));
        QFile::remove(svgPath);
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
        // Langer Titel: an der Wortgrenze gekürzt mit «…», doppelte Leerzeichen zusammengefasst
        Hit longTitle = icon;
        longTitle.title = QStringLiteral("Bergsee im Herbst mit  üppigem Lärchenwald, Spiegelung der Gipfel und Wolken über dem "
                                         "Tal bei Sonnenaufgang");
        const QString shortName = attribution(longTitle);
        QVERIFY2(shortName.contains(QStringLiteral("mit üppigem")), qPrintable(shortName));
        QVERIFY2(shortName.contains(QStringLiteral("… von ")) || shortName.contains(QStringLiteral("… (")), qPrintable(shortName));
        QVERIFY(!shortName.contains(QStringLiteral("Sonnenaufgang")));
        QVERIFY(!shortName.contains(QStringLiteral("  ")));
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
        // Escape im Suchfenster schliesst nur die Vorschau, das Suchfenster bleibt offen
        QTest::keyClick(&dialog, Qt::Key_Escape);
        QVERIFY(!dialog.viewer()->isVisible());
        QVERIFY(dialog.isVisible());
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
        // Schlüssel verdeckt; «Test» schiebt den Fokus nicht ins nächste Schlüsselfeld
        options.show();
        QVERIFY(QTest::qWaitForWindowExposed(&options));
        auto* testUnsplash = options.findChild<QPushButton*>(QStringLiteral("test-unsplash"));
        QVERIFY(testUnsplash);
        testUnsplash->setFocus();
        QTest::mouseClick(testUnsplash, Qt::LeftButton);
        QWidget* focus = QApplication::focusWidget();
        QVERIFY(focus);
        QVERIFY2(focus == testUnsplash || focus->toolTip() == imagesearch::source(QStringLiteral("unsplash"))->hint,
                 qPrintable(focus->toolTip()));
        for (QLineEdit* e : options.findChildren<QLineEdit*>())
            if (e->echoMode() != QLineEdit::Normal)
                QCOMPARE(e->echoMode(), QLineEdit::Password);
        options.hide();
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
    void openDialogOffersForeignFormats()
    {
        const QString filter = MainWindow::openFilter();
        for (const char* pattern : {"*.vek", "*.cdr", "*.cmx", "*.pdf", "*.ai", "*.svg", "*.png"})
            QVERIFY2(filter.section(QStringLiteral(";;"), 0, 0).contains(QLatin1String(pattern)), pattern);
    }

    void inkscapeSvgImports()
    {
        // so speichert Inkscape: mm-Masse, Ebenen als Gruppen, eigene Namensräume
        const QByteArray data =
            "<?xml version='1.0' encoding='UTF-8' standalone='no'?>\n"
            "<svg width='100mm' height='50mm' viewBox='0 0 100 50' version='1.1' id='svg1'\n"
            "   inkscape:version='1.3.2 (091e20e, 2023-11-25)' sodipodi:docname='test.svg'\n"
            "   xmlns:inkscape='http://www.inkscape.org/namespaces/inkscape'\n"
            "   xmlns:sodipodi='http://sodipodi.sourceforge.net/DTD/sodipodi-0.dtd'\n"
            "   xmlns='http://www.w3.org/2000/svg' xmlns:svg='http://www.w3.org/2000/svg'>\n"
            "  <sodipodi:namedview id='namedview1' pagecolor='#ffffff' inkscape:document-units='mm' />\n"
            "  <defs id='defs1' />\n"
            "  <g inkscape:label='Ebene 1' inkscape:groupmode='layer' id='layer1'>\n"
            "    <rect style='fill:#ff0000;stroke:none' id='rect1' width='40' height='20' x='10' y='10' />\n"
            "    <path style='fill:none;stroke:#000000;stroke-width:0.5' d='m 60,10 30,30' id='path1'\n"
            "       sodipodi:nodetypes='cc' />\n"
            "  </g>\n"
            "</svg>\n";
        QString error;
        const svg::Import imp = svg::read(data, &error);
        QVERIFY2(imp.group, qPrintable(error));
        QCOMPARE(qRound(imp.size.width()), 100);
        QCOMPARE(qRound(imp.size.height()), 50);
        int shapes = 0;
        std::function<void(const Node&)> count = [&](const Node& n) {
            if (dynamic_cast<const GroupObject*>(&n)) {
                for (int i = 0; i < n.childCount(); ++i)
                    count(*n.child(i));
            } else {
                ++shapes;
            }
        };
        count(*imp.group);
        QCOMPARE(shapes, 2);
        QVERIFY(imp.group->boundingRect().contains(QRectF(10, 10, 80, 30).adjusted(1, 1, -1, -1)));
    }

    void bitmapMenuAsRequested()
    {
        MainWindow window;
        QMenu* bitmaps = nullptr;
        for (QAction* a : window.menuBar()->actions())
            if (a->text() == MainWindow::tr("&Bitmaps"))
                bitmaps = a->menu();
        QVERIFY(bitmaps);
        auto names = [](QMenu* menu) {
            QStringList list;
            for (QAction* a : menu->actions())
                if (!a->isSeparator())
                    list << a->text().remove(QLatin1Char('&')).remove(QStringLiteral(" …"));
            return list;
        };
        auto submenu = [&](const QString& title) -> QMenu* {
            for (QAction* a : bitmaps->actions())
                if (a->menu() && a->text().remove(QLatin1Char('&')) == title)
                    return a->menu();
            return nullptr;
        };
        const QStringList top = names(bitmaps);
        for (const char* item : {"In Bitmap konvertieren", "Automatische Anpassung", "Bild geraderichten", "Modus", "3D-Effekte",
                                 "Künstlerische Striche", "Unschärfe", "Farbtransformation", "Kontur", "Kreativ", "Verzerren", "Rauschen",
                                 "Schärfe", "Blitzvektorisierung"})
            QVERIFY2(top.contains(QString::fromUtf8(item)), item);
        for (const char* gone : {"Kamera", "Farbänderung"})
            QVERIFY2(!top.contains(QString::fromUtf8(gone)), gone);
        QCOMPARE(names(submenu(QStringLiteral("Modus"))),
                 QStringList({QStringLiteral("Schwarzweiss (1 Bit)"), QStringLiteral("Graustufen (8 Bit)"), QStringLiteral("Graustufen (16 Bit)"),
                              QStringLiteral("Duoton (8 Bit)"), QStringLiteral("Duoton (16 Bit)")}));
        QCOMPARE(names(submenu(QStringLiteral("3D-Effekte"))),
                 QStringList({QStringLiteral("3D-Drehung"), QStringLiteral("Perspektive"), QStringLiteral("Sphäre"), QStringLiteral("Zylinder"),
                              QStringLiteral("Seite aufrollen"), QStringLiteral("Relief"), QStringLiteral("Ein-/Ausbuchten")}));
        QCOMPARE(names(submenu(QStringLiteral("Künstlerische Striche"))),
                 QStringList({QStringLiteral("Posterisieren"), QStringLiteral("Kohle"), QStringLiteral("Impressionismus"),
                              QStringLiteral("Wasserfarben"), QStringLiteral("Papierschnitzel"), QStringLiteral("Wellenpapier"),
                              QStringLiteral("Skizzenblock"), QStringLiteral("Ölfarbe")}));
        QCOMPARE(names(submenu(QStringLiteral("Unschärfe"))),
                 QStringList({QStringLiteral("Glätten"), QStringLiteral("Weichzeichnen"), QStringLiteral("Gaußsche Unschärfe"),
                              QStringLiteral("Bewegungsunschärfe"), QStringLiteral("Kreisförmige Unschärfe"), QStringLiteral("Zoom-Unschärfe")}));
        QCOMPARE(names(submenu(QStringLiteral("Kreativ"))),
                 QStringList({QStringLiteral("Gewebe"), QStringLiteral("Glasblock"), QStringLiteral("Rahmen"), QStringLiteral("Rauchglas"),
                              QStringLiteral("Kristallisieren"), QStringLiteral("Mosaik"), QStringLiteral("Vignette")}));
        QCOMPARE(names(submenu(QStringLiteral("Verzerren"))),
                 QStringList({QStringLiteral("Verpixeln"), QStringLiteral("Welle"), QStringLiteral("Strudel"), QStringLiteral("Wind"),
                              QStringLiteral("Kacheln"), QStringLiteral("Streuen")}));
    }

    void bitmapGeometryAndModes()
    {
        // Verlauf mit Muster, damit Drehungen etwas verändern
        QImage img(120, 80, QImage::Format_RGBA64);
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
                img.setPixelColor(x, y, QColor((x * 2) % 256, (y * 3) % 256, ((x / 10 + y / 10) % 2) * 200));
        auto alpha = [](const QImage& i, int x, int y) { return i.pixelColor(x, y).alphaF(); };

        // Geraderichten mit Zuschneiden: gleiche Grösse, keine leeren Ecken
        const QImage straight = filters::straighten(img, 8.0, true);
        QCOMPARE(straight.size(), img.size());
        for (const QPoint& corner : {QPoint(0, 0), QPoint(119, 0), QPoint(0, 79), QPoint(119, 79)})
            QVERIFY(alpha(straight, corner.x(), corner.y()) > 0.95);
        // ohne Zuschneiden bleiben die Ecken leer
        QVERIFY(alpha(filters::straighten(img, 8.0, false), 0, 0) < 0.05);

        // 3D-Drehung 0/0 = unverändert, sonst leere Ränder
        const QImage same = filters::rotate3d(img, 0, 0);
        QCOMPARE(same.pixelColor(60, 40).rgb(), img.pixelColor(60, 40).rgb());
        const QImage turned = filters::rotate3d(img, 0, 50);
        QCOMPARE(turned.size(), img.size());
        QVERIFY(alpha(turned, 0, 40) < 0.05);
        QVERIFY(alpha(turned, 60, 40) > 0.95);

        // Perspektive waagrecht: rechte Kante kürzer -> Ecke rechts oben leer, links oben voll
        const QImage persp = filters::perspective(img, 60, 0);
        QVERIFY(alpha(persp, 118, 1) < 0.05);
        QVERIFY(alpha(persp, 2, 40) > 0.95);

        // Graustufen 8 Bit: nur 256 Stufen, 16 Bit feiner
        QImage fine(256, 1, QImage::Format_RGBA64);
        for (int x = 0; x < 256; ++x)
            fine.setPixelColor(x, 0, QColor::fromRgba64(qRgba64(quint16(x * 256 + 100), quint16(x * 256 + 100), quint16(x * 256 + 100), 65535)));
        const QImage g8 = filters::toGrayscale(fine, 8), g16 = filters::toGrayscale(fine, 16);
        auto raw = [](const QImage& i, int x) { return reinterpret_cast<const QRgba64*>(i.constScanLine(0))[x].red(); };
        QCOMPARE(int(raw(g8, 10)) % 257, 0);
        QVERIFY(int(raw(g16, 10)) % 257 != 0);

        // Duoton: Weiss bleibt Papier, Schwarz wird dunkel und mischt beide Farben
        QImage bw(2, 1, QImage::Format_RGBA64);
        bw.setPixelColor(0, 0, Qt::white);
        bw.setPixelColor(1, 0, Qt::black);
        const QImage duo = filters::duotone(bw, Color(0, 0, 0), Color(200, 100, 0), 50, 8);
        QCOMPARE(duo.pixelColor(0, 0).rgb(), QColor(Qt::white).rgb());
        QVERIFY(duo.pixelColor(1, 0).lightness() < 40);
    }

    void interfaceTranslations()
    {
        if (!QFile::exists(QStringLiteral(":/i18n/tangenta_en.qm")))
            QSKIP("ohne Qt LinguistTools gebaut");
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest")); // sonst kein QSettings
        Options::instance().attach(m_editor.get());
        int germanPages = 0;
        {
            OptionsDialog dialog(*m_editor, nullptr);
            germanPages = int(dialog.pagePaths().size());
        }
        for (const char* code : {"en", "fr", "it", "es", "pt", "zh"}) {
            QTranslator translator;
            QVERIFY2(translator.load(QStringLiteral(":/i18n/tangenta_") + QLatin1String(code)), code);
            QApplication::installTranslator(&translator);
            applyLanguageFont(QLatin1String(code));
            {
                // Kein Menüeintrag mehr deutsch
                MainWindow window;
                QStringList german;
                const QRegularExpression umlaut(QStringLiteral("[äöüÄÖÜ]"));
                std::function<void(QMenu*)> walk = [&](QMenu* menu) {
                    for (QAction* a : menu->actions()) {
                        if (a->menu())
                            walk(a->menu());
                        if (a->text().contains(umlaut))
                            german << a->text();
                    }
                };
                for (QAction* a : window.menuBar()->actions()) {
                    if (a->menu())
                        walk(a->menu());
                }
                QVERIFY2(german.isEmpty(), qPrintable(QLatin1String(code) + QStringLiteral(": ") + german.join(QStringLiteral(" | "))));

                // Optionen: Pfade passen zusammen (drei Wurzeln, gleich viele Seiten), Sprünge aus dem Menü treffen
                OptionsDialog dialog(*m_editor, nullptr);
                QSet<QString> roots;
                for (const QString& path : dialog.pagePaths())
                    roots.insert(path.section(QLatin1Char('/'), 0, 0));
                QCOMPARE(roots.size(), 3);
                QCOMPARE(int(dialog.pagePaths().size()), germanPages);
                for (const char* path : {"Dokument/Lineale", "Arbeitsbereich/An Objekten ausrichten", "Global/KI",
                                         "Arbeitsbereich/SuperTrace", "Global/Bildersuche"})
                    QVERIFY2(dialog.selectPage(MainWindow::tr(path)), qPrintable(QLatin1String(code) + QLatin1Char(' ') + MainWindow::tr(path)));
                // Übersicht übrig gebliebener deutscher Texte (nur auf Wunsch, Benutzerdaten zählen mit)
                if (qEnvironmentVariableIsSet("VEK_I18N_SCAN")) {
                    QSet<QString> left;
                    auto scan = [&](QWidget* root) {
                        for (QWidget* w : root->findChildren<QWidget*>()) {
                            QStringList texts{w->toolTip()};
                            if (auto* l = qobject_cast<QLabel*>(w))
                                texts << l->text();
                            if (auto* b = qobject_cast<QAbstractButton*>(w))
                                texts << b->text();
                            if (auto* c = qobject_cast<QComboBox*>(w))
                                for (int i = 0; i < c->count(); ++i)
                                    texts << c->itemText(i);
                            if (auto* g = qobject_cast<QGroupBox*>(w))
                                texts << g->title();
                            for (QAction* a : w->actions())
                                texts << a->text() << a->toolTip();
                            for (const QString& t : texts)
                                if (t.contains(umlaut))
                                    left << t.left(120);
                        }
                    };
                    scan(&window);
                    scan(&dialog);
                    QStringList sorted(left.begin(), left.end());
                    sorted.sort();
                    for (const QString& t : sorted)
                        qInfo().noquote() << "I18N" << code << t;
                }
                if (qEnvironmentVariableIsSet("VEK_SCREENSHOT_DIR")) {
                    const QString dir = qEnvironmentVariable("VEK_SCREENSHOT_DIR");
                    window.resize(1500, 900);
                    window.show();
                    QTest::qWait(200);
                    window.grab().save(dir + QStringLiteral("/90-sprache-%1.png").arg(QLatin1String(code)));
                    dialog.resize(1000, 700);
                    dialog.selectPage(MainWindow::tr("Global/KI"));
                    dialog.show();
                    QTest::qWait(200);
                    dialog.grab().save(dir + QStringLiteral("/91-optionen-%1.png").arg(QLatin1String(code)));
                }
            }
            QApplication::removeTranslator(&translator);
        }
        applyLanguageFont(QStringLiteral("de"));
    }

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
    // VEK_SCREENSHOT_DIR wird die erste Seite als Bild abgelegt (Auflösung VEK_CDR_DPI, sonst 50 dpi;
    // andere Seite mit VEK_CDR_PAGE, gezählt ab 1; VEK_CDR_FONTS=1 zeigt die Vorschläge für fehlende Schriften)
    void cdrImportLocalFiles()
    {
        const QString dir = qEnvironmentVariable("VEK_CDR_DIR");
        if (dir.isEmpty())
            QSKIP("CDR-Dateien: VEK_CDR_DIR setzen");
        const QString shots = qEnvironmentVariable("VEK_SCREENSHOT_DIR");
        const QFileInfoList files = QDir(dir).entryInfoList({QStringLiteral("*.cdr"), QStringLiteral("*.cmx")}, QDir::Files, QDir::Name);
        QVERIFY(!files.isEmpty());
        QStringList failed;
        for (const QFileInfo& info : files) {
            QFile file(info.absoluteFilePath());
            QVERIFY(file.open(QIODevice::ReadOnly));
            QElapsedTimer timer;
            timer.start();
            QString error;
            std::vector<svg::Import> pages = cdr::read(file.readAll(), &error);
            int objects = 0;
            for (const svg::Import& page : pages)
                objects += page.group ? page.group->childCount() : 0;
            if (pages.empty() || objects == 0) {
                // weiterlesen, damit ein Lauf über viele Dateien alle Ausfälle auf einmal zeigt
                qWarning().noquote() << "FEHLER" << info.fileName() << error;
                failed << info.fileName();
                continue;
            }
            qInfo().noquote() << info.fileName() << pages.size() << "Seite(n)," << objects << "Objekte," << pages.front().fonts.size()
                              << "Schriften," << timer.elapsed() << "ms," << pages.front().size.width() << "x" << pages.front().size.height() << "mm";
            // VEK_CDR_FONTS=1: Vorschläge der Schriftersetzung für fehlende Schriften
            if (qEnvironmentVariableIsSet("VEK_CDR_FONTS"))
                for (const ImportedFont& f : pages.front().fonts) {
                    if (!f.installed.isEmpty() || f.texts.isEmpty())
                        continue;
                    QStringList names;
                    for (const fontmatch::Candidate& c : fontmatch::suggest(f, 4))
                        names << QStringLiteral("%1 %2%").arg(c.family).arg(int(std::lround(c.score * 100)));
                    qInfo().noquote() << "  fehlt" << f.family << (f.bold ? "fett" : "") << "Texte" << f.texts.size() << "->"
                                      << names.join(QStringLiteral(", "));
                }
            if (shots.isEmpty())
                continue;
            const size_t index = std::clamp<size_t>(size_t(std::max(1, qEnvironmentVariableIntValue("VEK_CDR_PAGE"))), 1, pages.size()) - 1;
            m_editor->setDocument(std::make_unique<Document>(), QString());
            m_editor->setPageSize(pages[index].size, true);
            m_editor->importGroup(std::move(pages[index].group), false, QStringLiteral("CDR"));
            output::ExportOptions o;
            o.dpi = qEnvironmentVariableIsSet("VEK_CDR_DPI") ? qEnvironmentVariableIntValue("VEK_CDR_DPI") : 50;
            output::renderImage(*m_canvas, o).save(shots + QStringLiteral("/cdr-") + info.completeBaseName() + QStringLiteral(".png"));
        }
        QVERIFY2(failed.isEmpty(), qPrintable(failed.join(QStringLiteral(", "))));
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
        o.dpi = 75;
        const QImage bitmap = output::renderImage(*window.canvas(), o);
        TraceDialog trace(bitmap, trace::Preset::HighQuality, &window);
        trace.resize(1300, 820);
        trace.show();
        QTest::qWait(8000);
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
        // Ganze Seite und dazu vergrössert die eigenen Objekte der KI (Gruppe aus Rechteck und SVG)
        QCOMPARE(r.images.size(), 2);
        const QImage view = QImage::fromData(r.images.first());
        QVERIFY(std::max(view.width(), view.height()) <= 600 && std::max(view.width(), view.height()) >= 590);
        QVERIFY2(r.text.contains(QStringLiteral("vergrössert")), qPrintable(r.text));
        QVERIFY(tools.ownObjects().contains(ed.selection().first()));
        r = run(QStringLiteral("dokument_ansehen"), {});
        QVERIFY2(r.text.startsWith(QStringLiteral("Von dir in diesem Gespräch angelegt: %1.").arg(ed.selection().first())),
                 qPrintable(r.text.left(200)));
        // Ausschnitt: nur dieser Bereich, ein Bild
        r = run(QStringLiteral("seite_ansehen"), {{QStringLiteral("breite_px"), 400}, {QStringLiteral("ausschnitt"), QJsonArray{10, 20, 40, 20}}});
        QVERIFY2(!r.error, qPrintable(r.text));
        QCOMPARE(r.images.size(), 1);
        const QImage cutView = QImage::fromData(r.images.first());
        QCOMPARE(cutView.width(), 400);
        QCOMPARE(cutView.height(), 200);
        QCOMPARE(QColor(cutView.pixel(100, 100)), QColor(Qt::red)); // Rechteck ab (10, 20)
        tools.forgetOwnObjects();
        QVERIFY(!run(QStringLiteral("dokument_ansehen"), {}).text.startsWith(QStringLiteral("Von dir")));

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
        QVERIFY(content.size() >= 2); // Text, Seite und gegebenenfalls die vergrösserten eigenen Objekte
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
    // Bild mit KI optimieren: Antwort lesen (auch eingebettet), gesperrte Bereiche, Stärke, Bild in der
    // Benutzer-Nachricht an Anthropic und OpenAI, Dialog rechnet mit den Werten auf dem Original
    void aiImageOptimize()
    {
        using namespace ai;
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        aiopt::Settings all;
        const QString reply = QStringLiteral(
            "Gern:\n```json\n{\"temperature\": -12, \"tint\": 4, \"saturation\": 250, \"brightness\": 8.6, \"contrast\": 10,"
            " \"highlights\": -20, \"shadows\": 15, \"midtones\": 5, \"stretch\": true, \"comment\": \"Kühler, Schatten offen.\"}\n```");
        aiopt::Suggestion sg = aiopt::parse(reply, all);
        QVERIFY(sg.ok);
        QCOMPARE(sg.basic.temperature, -12);
        QCOMPARE(sg.basic.saturation, 100); // begrenzt
        QCOMPARE(sg.basic.brightness, 9);
        QVERIFY(sg.stretch);
        QCOMPARE(sg.comment, QStringLiteral("Kühler, Schatten offen."));
        aiopt::Settings locked;
        locked.color = false;
        locked.tone = false;
        sg = aiopt::parse(reply, locked);
        QCOMPARE(sg.basic.temperature, 0);
        QCOMPARE(sg.basic.contrast, 0);
        QVERIFY(!sg.stretch);
        QCOMPARE(sg.basic.saturation, 100);
        QVERIFY(!aiopt::parse(QStringLiteral("Das Bild ist schön."), all).ok);
        QCOMPARE(aiopt::scaled(sg.basic, 50).saturation, 50);

        // Frage nennt Ziel, gesperrte Regler, Wunsch und Statistik
        QImage photo(200, 120, QImage::Format_RGB32);
        for (int y = 0; y < photo.height(); ++y)
            for (int x = 0; x < photo.width(); ++x)
                photo.setPixel(x, y, qRgb(60 + x / 4, 70 + y / 4, 90)); // flau, bläulich
        locked.wish = QStringLiteral("Himmel blauer");
        const QString q = aiopt::question(locked, photo, QStringLiteral("Deutsch"));
        QVERIFY(q.contains(QStringLiteral("temperature, tint")));
        QVERIFY(q.contains(QStringLiteral("Himmel blauer")));
        QVERIFY(q.contains(QStringLiteral("luminance p1/p50/p99")));
        QVERIFY(q.contains(QStringLiteral("Deutsch")));

        // Bild in der Benutzer-Nachricht
        Message m;
        m.text = QStringLiteral("Optimise");
        m.images = {QByteArray("PNG")};
        const Account claudeAcc{QStringLiteral("c"), QStringLiteral("claude"), QString(), QString(), QStringLiteral("k"), QString()};
        std::unique_ptr<Provider> claude(Provider::create(claudeAcc));
        QJsonArray content = claude->requestBody(aiopt::systemPrompt(), {m}, {}).value(QStringLiteral("messages")).toArray()[0]
                                 .toObject().value(QStringLiteral("content")).toArray();
        QCOMPARE(content.size(), 2);
        QCOMPARE(content[1].toObject().value(QStringLiteral("type")).toString(), QStringLiteral("image"));
        const Account gptAcc{QStringLiteral("o"), QStringLiteral("openai"), QString(), QString(), QStringLiteral("k"), QString()};
        std::unique_ptr<Provider> gpt(Provider::create(gptAcc));
        const QJsonArray messages = gpt->requestBody(aiopt::systemPrompt(), {m}, {}).value(QStringLiteral("messages")).toArray();
        content = messages[1].toObject().value(QStringLiteral("content")).toArray();
        QCOMPARE(content[1].toObject().value(QStringLiteral("image_url")).toObject().value(QStringLiteral("url")).toString(),
                 QStringLiteral("data:image/png;base64,") + QString::fromLatin1(QByteArray("PNG").toBase64()));

        // Dialog: Vorschlag in die Regler, Stärke 0 = Original, sonst verändert
        // Nur Claude Code: Auswahl gesperrt, „Einrichten …“ meldet sich; nach dem Anlegen frei
        saveAccounts({{QStringLiteral("cc"), QStringLiteral("claude_code"), QString(), QString(), QString(), QStringLiteral("sonnet")}});
        {
            AiImageDialog none(photo);
            auto* account = none.findChild<QComboBox*>(QStringLiteral("account"));
            auto* setup = none.findChild<QPushButton*>(QStringLiteral("accountSetup"));
            QVERIFY(account && setup);
            QVERIFY(!account->isEnabled());
            QSignalSpy requested(&none, &AiImageDialog::accountSetupRequested);
            setup->click();
            QCOMPARE(requested.size(), 1);
            saveAccounts({gptAcc});
            none.reloadAccounts();
            QVERIFY(account->isEnabled());
            QCOMPARE(account->count(), 1);
        }
        // KI bearbeitet das Bild: Auftrag, Gemini-Format, Varianten, Übertragen oder Ersetzen, Stärke
        {
            aiopt::EditSettings es;
            es.tasks = aiopt::CastTask | aiopt::SkyTask;
            es.wish = QStringLiteral("Boot kräftiger");
            const QString prompt = aiopt::editPrompt(es);
            QVERIFY(prompt.contains(QStringLiteral("white balance")) && prompt.contains(QStringLiteral("sky")));
            QVERIFY(prompt.contains(QStringLiteral("Boot kräftiger")) && prompt.contains(QStringLiteral("same framing")));
            QVERIFY(!prompt.contains(QStringLiteral("sharpen")));
            QVERIFY(!aiopt::changesContent(es.tasks));
            QVERIFY(aiopt::changesContent(aiopt::SharpenTask));
            QCOMPARE(aiopt::imageModels(QStringLiteral("openai")).first(), QStringLiteral("gpt-image-2"));
            QVERIFY(aiopt::imageModels(QStringLiteral("claude")).isEmpty());
            QCOMPARE(aiopt::geminiEndpoint(QStringLiteral("https://generativelanguage.googleapis.com/v1beta/openai/"),
                                           QStringLiteral("gemini-2.5-flash-image")),
                     QStringLiteral("https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash-image:generateContent"));
            const QJsonObject req = QJsonDocument::fromJson(aiopt::geminiRequest(QStringLiteral("p"), "PNG")).object();
            const QJsonArray parts = req[QStringLiteral("contents")].toArray()[0].toObject()[QStringLiteral("parts")].toArray();
            QCOMPARE(parts[1].toObject()[QStringLiteral("inline_data")].toObject()[QStringLiteral("data")].toString(),
                     QString::fromLatin1(QByteArray("PNG").toBase64()));
            QByteArray png;
            QBuffer pngBuffer(&png);
            pngBuffer.open(QIODevice::WriteOnly);
            photo.save(&pngBuffer, "PNG");
            const QByteArray reply = QJsonDocument(QJsonObject{{QStringLiteral("candidates"), QJsonArray{QJsonObject{
                {QStringLiteral("content"), QJsonObject{{QStringLiteral("parts"), QJsonArray{
                    QJsonObject{{QStringLiteral("text"), QStringLiteral("Hier")}},
                    QJsonObject{{QStringLiteral("inlineData"), QJsonObject{{QStringLiteral("mimeType"), QStringLiteral("image/png")},
                                                                           {QStringLiteral("data"), QString::fromLatin1(png.toBase64())}}}}}}}}}}}}).toJson();
            QCOMPARE(aiopt::geminiImage(reply).size(), photo.size());
            QString said;
            QVERIFY(aiopt::geminiImage(R"({"candidates":[{"content":{"parts":[{"text":"Geht nicht"}]}}]})", &said).isNull());
            QCOMPARE(said, QStringLiteral("Geht nicht"));

            saveAccounts({gptAcc});
            AiImageDialog edit(photo);
            edit.setMethod(AiImageDialog::Method::Edit);
            edit.setApplyMode(AiImageDialog::Apply::Transfer);
            auto* model = edit.findChild<QComboBox*>(QStringLiteral("imageModel"));
            QVERIFY(model && model->currentText().startsWith(QStringLiteral("gpt-image")));
            QVERIFY(edit.findChild<QComboBox*>(QStringLiteral("account"))->isEnabled());
            // «KI-Ergebnis»: wärmer, etwas heller, kleiner als das Original
            QImage warm = photo.convertToFormat(QImage::Format_RGB32).scaled(photo.width() / 2, photo.height() / 2);
            for (int y = 0; y < warm.height(); ++y)
                for (int x = 0; x < warm.width(); ++x) {
                    const QRgb c = warm.pixel(x, y);
                    warm.setPixel(x, y, qRgb(std::min(255, qRed(c) + 40), std::min(255, qGreen(c) + 15), std::max(0, qBlue(c) - 20)));
                }
            edit.addEditedImage(warm, QStringLiteral("Test"));
            QCOMPARE(edit.variantCount(), 1);
            const QImage transferred = edit.result();
            QCOMPARE(transferred.size(), photo.size());
            const QRgb before = photo.pixel(100, 60), after = transferred.pixel(100, 60);
            QVERIFY2(qRed(after) > qRed(before) + 20 && qBlue(after) < qBlue(before),
                     qPrintable(QStringLiteral("%1 -> %2").arg(before, 0, 16).arg(after, 0, 16)));
            edit.findChild<QSlider*>()->setValue(0); // Stärke 0 = Original
            QCOMPARE(edit.result().convertToFormat(QImage::Format_RGB32).pixel(100, 60), photo.convertToFormat(QImage::Format_RGB32).pixel(100, 60));
            edit.findChild<QSlider*>()->setValue(100);
            // Schärfen verändert Inhalte: Übernehmen schaltet aufs KI-Bild
            for (QCheckBox* box : edit.findChildren<QCheckBox*>())
                if (box->property("task").toInt() == aiopt::SharpenTask)
                    box->setChecked(true);
            QCOMPARE(edit.applyMode(), AiImageDialog::Apply::Replace);
            const QImage replaced = edit.result();
            const QRgb expectedPixel = warm.scaled(photo.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation).pixel(100, 60);
            QVERIFY(std::abs(qRed(replaced.pixel(100, 60)) - qRed(expectedPixel)) <= 2);
            edit.addEditedImage(photo, QStringLiteral("Zweite"));
            QCOMPARE(edit.variantCount(), 2);
            QVERIFY(edit.findChild<QComboBox*>(QStringLiteral("variant"))->isVisibleTo(&edit));
            edit.setMethod(AiImageDialog::Method::Values);
        }
        saveAccounts({gptAcc});
        AiImageDialog dialog(photo);
        dialog.setMethod(AiImageDialog::Method::Values);
        sg = aiopt::parse(reply, all);
        dialog.setSuggestion(sg);
        QCOMPARE(dialog.basic().temperature, -12);
        const QImage changed = dialog.result();
        QVERIFY(changed.pixel(100, 60) != photo.convertToFormat(changed.format()).pixel(100, 60));
        dialog.findChild<QSlider*>()->setValue(0); // Stärke
        dialog.setSuggestion(aiopt::Suggestion{true, sg.basic, false, {}, {}});
        QCOMPARE(dialog.result().convertToFormat(QImage::Format_RGB32).pixel(100, 60), photo.pixel(100, 60));
        // Freistellen mit fertiger Maske: Hintergrund durchsichtig, Motiv unverändert
        QImage mask(photo.size(), QImage::Format_Grayscale8);
        mask.fill(0);
        for (int y = 30; y < 90; ++y)
            for (int x = 50; x < 150; ++x)
                mask.setPixel(x, y, qRgb(255, 255, 255));
        dialog.setCutoutMask(mask);
        dialog.setCutout(true);
        if (dialog.cutoutMethod() == AiImageDialog::CutoutMethod::Local) {
            const QImage cut = dialog.result().convertToFormat(QImage::Format_ARGB32);
            QCOMPARE(qAlpha(cut.pixel(10, 10)), 0);
            QCOMPARE(qAlpha(cut.pixel(100, 60)), 255);
            QCOMPARE(cut.pixel(100, 60) & 0xffffff, photo.pixel(100, 60) & 0xffffff);
        }
        dialog.setCutout(false);
        // Cloud-Hilfen: hochkant -> 1024x1536, Rand weg, Originalgrösse zurück
        QCOMPARE(aiopt::cloudSize(QSize(1000, 1500)), QSize(1024, 1536));
        QCOMPARE(aiopt::cloudSize(QSize(1920, 1080)), QSize(1536, 1024));
        QRect inner;
        const QImage padded = aiopt::padForCloud(photo, &inner);
        QCOMPARE(padded.size(), QSize(1536, 1024));
        QVERIFY(inner.width() <= 1536 && inner.height() <= 1024 && (inner.width() == 1536 || inner.height() == 1024));
        const QImage back = aiopt::unpadFromCloud(padded.scaled(768, 512), inner, photo.size());
        QCOMPARE(back.size(), photo.size());
        QVERIFY(qAbs(qRed(back.pixel(100, 60)) - qRed(photo.pixel(100, 60))) < 12);

        dialog.resize(900, 760);
        dialog.show();
        QCoreApplication::processEvents();
        if (const QString dir = qEnvironmentVariable("VEK_SCREENSHOT_DIR"); !dir.isEmpty())
            dialog.grab().save(dir + QStringLiteral("/ki-bild-optimieren.png"));
        saveAccounts({});
    }

    // Bild freistellen im Dialog mit dem echten Modell (nur mit VEK_ONNXRUNTIME und Modell);
    // VEK_CUTOUT_FILE = eigenes Foto, sonst eine Scheibe
    void cutoutDialogLocal()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        if (!matting::runtimeAvailable() || !matting::modelPresent(matting::defaultModel()))
            QSKIP("ONNX Runtime oder Modell fehlt");
        QImage photo(qEnvironmentVariable("VEK_CUTOUT_FILE"));
        if (photo.isNull()) {
            photo = QImage(400, 300, QImage::Format_RGB32);
            photo.fill(qRgb(150, 150, 150));
            QPainter p(&photo);
            p.setBrush(QColor(220, 30, 30));
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPointF(200, 150), 90, 90);
        }
        QSettings().setValue(QStringLiteral("aiOptimize/cutoutMethod"), 0);
        AiImageDialog dialog(photo, nullptr, true);
        dialog.resize(1000, 800);
        dialog.show();
        QTRY_VERIFY_WITH_TIMEOUT(dialog.cutout(), 2000);
        QTRY_VERIFY_WITH_TIMEOUT(!dialog.cutoutWorking(), 120000);
        const QImage cut = dialog.result().convertToFormat(QImage::Format_ARGB32);
        QCOMPARE(cut.size(), photo.size());
        QCOMPARE(qAlpha(cut.pixel(3, 3)), 0); // Ecke = Hintergrund
        QCOMPARE(qAlpha(cut.pixel(cut.width() / 2, cut.height() / 2)), 255);
        if (const QString dir = qEnvironmentVariable("VEK_SCREENSHOT_DIR"); !dir.isEmpty()) {
            QTest::qWait(400);
            dialog.grab().save(dir + QStringLiteral("/freistellen-dialog.png"));
            QImage shown(cut.size(), QImage::Format_ARGB32);
            shown.fill(QColor(255, 0, 255));
            QPainter(&shown).drawImage(0, 0, cut);
            shown.save(dir + QStringLiteral("/freistellen-ergebnis.png"));
        }
    }

    // KI bearbeitet das Bild, echter Abruf (nur mit VEK_OPENAI_KEY und VEK_EDIT_FILE; kostet einige Rappen).
    // VEK_EDIT_OUT=datei.png legt das übertragene Ergebnis ab, VEK_EDIT_MODEL wählt das Bildmodell.
    void aiImageEditLive()
    {
        const QString key = qEnvironmentVariable("VEK_OPENAI_KEY");
        if (key.isEmpty() || !qEnvironmentVariableIsSet("VEK_EDIT_FILE"))
            QSKIP("nur mit VEK_OPENAI_KEY und VEK_EDIT_FILE");
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        const QImage photo(qEnvironmentVariable("VEK_EDIT_FILE"));
        QVERIFY(!photo.isNull());
        ai::saveAccounts({ai::Account{QStringLiteral("o"), QStringLiteral("openai"), QStringLiteral("OpenAI"), QString(), key, QString()}});
        auto cleanup = qScopeGuard([] { ai::saveAccounts({}); }); // Schlüssel nicht liegen lassen
        AiImageDialog dialog(photo);
        dialog.setMethod(AiImageDialog::Method::Edit);
        dialog.setApplyMode(AiImageDialog::Apply::Transfer);
        if (qEnvironmentVariableIsSet("VEK_EDIT_MODEL"))
            dialog.findChild<QComboBox*>(QStringLiteral("imageModel"))->setCurrentText(qEnvironmentVariable("VEK_EDIT_MODEL"));
        QElapsedTimer timer;
        timer.start();
        dialog.editImage();
        QVERIFY(dialog.asking());
        QTRY_VERIFY_WITH_TIMEOUT(!dialog.asking(), 360000);
        qInfo().noquote() << "Antwort nach" << timer.elapsed() / 1000 << "s:" << dialog.findChild<QLabel*>(QStringLiteral("status"))->text();
        QCOMPARE(dialog.variantCount(), 1);
        const QImage result = dialog.result();
        QCOMPARE(result.size(), photo.size());
        if (qEnvironmentVariableIsSet("VEK_EDIT_OUT"))
            result.save(qEnvironmentVariable("VEK_EDIT_OUT"));
    }

    // Freistellen über OpenAI, echter Abruf (nur mit VEK_OPENAI_KEY; kostet einige Rappen)
    void cutoutDialogCloud()
    {
        const QString key = qEnvironmentVariable("VEK_OPENAI_KEY");
        if (key.isEmpty())
            QSKIP("nur mit VEK_OPENAI_KEY");
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        QImage photo(qEnvironmentVariable("VEK_CUTOUT_FILE"));
        QVERIFY(!photo.isNull());
        ai::saveAccounts({ai::Account{QStringLiteral("o"), QStringLiteral("openai"), QStringLiteral("OpenAI"), QString(), key, QString()}});
        auto cleanup = qScopeGuard([] { ai::saveAccounts({}); }); // Schlüssel nicht liegen lassen
        QSettings().setValue(QStringLiteral("aiOptimize/cutoutMethod"), 1);
        AiImageDialog dialog(photo, nullptr, true);
        dialog.resize(1000, 800);
        dialog.show();
        QTRY_VERIFY_WITH_TIMEOUT(dialog.cutout(), 2000);
        QTRY_VERIFY_WITH_TIMEOUT(!dialog.cutoutWorking(), 300000);
        QSettings().setValue(QStringLiteral("aiOptimize/cutoutMethod"), 0);
        const QImage cut = dialog.result().convertToFormat(QImage::Format_ARGB32);
        QCOMPARE(cut.size(), photo.size());
        QCOMPARE(qAlpha(cut.pixel(3, 3)), 0);
        if (const QString dir = qEnvironmentVariable("VEK_SCREENSHOT_DIR"); !dir.isEmpty()) {
            QImage shown(cut.size(), QImage::Format_ARGB32);
            shown.fill(QColor(255, 0, 255));
            QPainter(&shown).drawImage(0, 0, cut);
            shown.save(dir + QStringLiteral("/freistellen-cloud.png"));
        }
    }

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
        // DeepSeek: deepseek-v4-pro sieht keine Bilder, deepseek-flash schon
        std::unique_ptr<Provider> deep(Provider::create({QStringLiteral("d"), QStringLiteral("deepseek"), {}, {}, QStringLiteral("k"), QStringLiteral("deepseek-v4-pro")}));
        QCOMPARE(deep->requestBody(QStringLiteral("S"), history, tools).value(QStringLiteral("messages")).toArray().size(), 5);
        std::unique_ptr<Provider> flash(Provider::create({QStringLiteral("d"), QStringLiteral("deepseek"), {}, {}, QStringLiteral("k"), QStringLiteral("deepseek-flash")}));
        QCOMPARE(flash->requestBody(QStringLiteral("S"), history, tools).value(QStringLiteral("messages")).toArray().size(), 6);
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
            // Dialog «KI-Zugang»: Schlüssel immer verdeckt; beim Wechsel der Art wächst das Fenster mit dem Hinweis
            bool checked = false;
            QTimer::singleShot(0, this, [&] {
                auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
                QVERIFY(dialog);
                auto* key = dialog->findChild<QLineEdit*>(QStringLiteral("key"));
                QCOMPARE(key->echoMode(), QLineEdit::Password);
                auto* kindBox = dialog->findChild<QComboBox*>(QStringLiteral("kind"));
                auto* hint = dialog->findChild<QLabel*>(QStringLiteral("hint"));
                auto* label = dialog->findChild<QLineEdit*>(QStringLiteral("label"));
                for (const char* k : {"deepseek", "claude_code", "openai", "claude_code"}) {
                    kindBox->setCurrentIndex(kindBox->findData(QLatin1String(k)));
                    QVERIFY2(hint->height() >= hint->heightForWidth(hint->width()), k);
                    QVERIFY2(hint->geometry().bottom() < label->geometry().top(), k);
                }
                QVERIFY(hint->text().contains(QStringLiteral("Claude Code")));
                checked = true;
                dialog->reject();
            });
            options.findChild<QPushButton*>(QStringLiteral("ai-edit"))->click();
            QVERIFY(checked);
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

    // --- Kopie beim Ziehen, Duplizieren an Ort und Stelle ---------------------------------------

    // Rechtsklick während des linken Ziehens: Original bleibt, Kopie in der neuen Lage
    void pickRightClickDuringDragLeavesCopy()
    {
        const NodeId original = drawRect(QRectF(20, 40, 30, 20));
        const int before = int(m_editor->pickableObjects().size());
        const QPoint from = screen(QPointF(35, 50));
        const QPoint to = screen(QPointF(75, 50));
        press(from);
        for (int i = 1; i <= 4; ++i)
            move(from + (to - from) * i / 8);
        QTest::mousePress(m_canvas.get(), Qt::RightButton, {}, from + (to - from) / 2, 10);
        QTest::mouseRelease(m_canvas.get(), Qt::RightButton, {}, from + (to - from) / 2, 10);
        for (int i = 5; i <= 8; ++i)
            move(from + (to - from) * i / 8);
        screenshot(QStringLiteral("90-kopie-beim-ziehen"));
        release(to);
        QCOMPARE(int(m_editor->pickableObjects().size()), before + 1);
        QVERIFY2(near(m_editor->document().findAs<DrawObject>(original)->boundingRect(), QRectF(20, 40, 30, 20)),
                 qPrintable(describe(m_editor->document().findAs<DrawObject>(original)->boundingRect())));
        QVERIFY(!m_editor->selection().contains(original));
        QVERIFY2(near(bounds(), QRectF(60, 40, 30, 20)), qPrintable(describe(bounds())));
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Kopieren"));
        // Ein Undo-Schritt für die ganze Geste
        m_editor->document().undo();
        QCOMPARE(int(m_editor->pickableObjects().size()), before);
        QVERIFY(near(m_editor->document().findAs<DrawObject>(original)->boundingRect(), QRectF(20, 40, 30, 20)));

        // Auch beim Skalieren an einem Griff: Kopie in 150 %, Original unverändert
        m_editor->setSelection({original});
        const QPoint corner = handle(Qt::BottomRightCorner);
        const QPoint target = corner + screen(QPointF(15, 10)) - screen(QPointF(0, 0));
        press(corner);
        move(corner + (target - corner) / 2);
        QTest::mousePress(m_canvas.get(), Qt::RightButton, {}, corner + (target - corner) / 2, 10);
        QTest::mouseRelease(m_canvas.get(), Qt::RightButton, {}, corner + (target - corner) / 2, 10);
        move(target);
        release(target);
        QCOMPARE(int(m_editor->pickableObjects().size()), before + 1);
        QVERIFY(near(m_editor->document().findAs<DrawObject>(original)->boundingRect(), QRectF(20, 40, 30, 20)));
        QVERIFY2(near(bounds(), QRectF(20, 40, 45, 30)), qPrintable(describe(bounds())));
    }

    // Leertaste während des Ziehens stempelt eine Kopie und zieht weiter
    void pickSpaceStampsCopyWhileDragging()
    {
        const NodeId original = drawRect(QRectF(20, 40, 30, 20));
        const int before = int(m_editor->pickableObjects().size());
        const QPoint from = screen(QPointF(35, 50));
        const QPoint mid = screen(QPointF(55, 50));
        const QPoint to = screen(QPointF(75, 50));
        press(from);
        for (int i = 1; i <= 4; ++i)
            move(from + (mid - from) * i / 4);
        QTest::keyClick(m_canvas.get(), Qt::Key_Space);
        QCOMPARE(m_canvas->toolId(), ToolId::Pick); // Leertaste wechselt beim Ziehen nicht das Werkzeug
        for (int i = 1; i <= 4; ++i)
            move(mid + (to - mid) * i / 4);
        screenshot(QStringLiteral("91-stempel-beim-ziehen"));
        release(to);
        QCOMPARE(int(m_editor->pickableObjects().size()), before + 1);
        // Original am Ziel, Stempel an der Zwischenstelle
        QVERIFY2(near(m_editor->document().findAs<DrawObject>(original)->boundingRect(), QRectF(60, 40, 30, 20)),
                 qPrintable(describe(m_editor->document().findAs<DrawObject>(original)->boundingRect())));
        QCOMPARE(m_editor->selection(), QList<NodeId>{original});
        bool stamped = false;
        for (const DrawObject* o : m_editor->pickableObjects())
            stamped = stamped || (o->id() != original && near(o->boundingRect(), QRectF(40, 40, 30, 20)));
        QVERIFY(stamped);
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Verschieben"));
        m_editor->document().undo();
        QCOMPARE(int(m_editor->pickableObjects().size()), before);
        QVERIFY(near(m_editor->document().findAs<DrawObject>(original)->boundingRect(), QRectF(20, 40, 30, 20)));
        // Ohne Ziehen schaltet die Leertaste weiterhin das Werkzeug
        m_canvas->togglePickTool(); // Befehl der Leertaste
        QVERIFY(m_canvas->toolId() != ToolId::Pick);
    }

    // Ziffernblock +: Kopie ohne Versatz, die Kopie ist danach gewählt
    void numpadPlusDuplicatesInPlace()
    {
        const NodeId original = drawRect(QRectF(20, 40, 30, 20));
        const int before = int(m_editor->pickableObjects().size());
        m_editor->duplicateInPlace(); // Befehl "An Ort und Stelle duplizieren" (Ziffernblock +)
        QCOMPARE(int(m_editor->pickableObjects().size()), before + 1);
        QCOMPARE(m_editor->selection().size(), 1);
        QVERIFY(m_editor->selection().first() != original);
        QVERIFY(near(bounds(), m_editor->document().findAs<DrawObject>(original)->boundingRect(), 1e-9));
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("An Ort und Stelle duplizieren"));
        // Plus der Haupttastatur dupliziert nicht
        QTest::keyClick(m_canvas.get(), Qt::Key_Plus);
        QCOMPARE(int(m_editor->pickableObjects().size()), before + 1);
    }

    // --- Knotenfenster der interaktiven Füllung --------------------------------------------------

    void fillNodePopupEditsStops()
    {
        const NodeId id = drawRect(QRectF(20, 40, 100, 50));
        m_canvas->setTool(ToolId::InteractiveFill);
        drag(screen(QPointF(30, 65)), screen(QPointF(110, 65)));
        auto fill = [this, id] { return m_editor->document().findAs<DrawObject>(id)->style().fill; };
        QCOMPARE(fill().stops.size(), 2);
        auto& tool = *static_cast<InteractiveFillTool*>(m_canvas->currentTool());

        // Klick auf den Endpunkt: Fenster daneben, Endpunkt gewählt, nichts verschoben
        const Fill beforeClick = fill();
        const QString undoBefore = m_editor->document().undoText();
        click(screen(QPointF(110, 65)));
        QVERIFY(tool.popup() && tool.popup()->isVisible());
        QCOMPARE(tool.selectedStop(), 1);
        QCOMPARE(fill(), beforeClick);
        QCOMPARE(m_editor->document().undoText(), undoBefore);
        QVERIFY(!tool.popup()->positionField()->isVisible()); // Endpunkt: keine Lage
        QVERIFY(QRect(QPoint(), m_canvas->size()).contains(tool.popup()->geometry()));
        screenshot(QStringLiteral("92-knotenfenster"));

        // Farbknopf: Farbwähler öffnet sich, Rot wählen
        QTimer::singleShot(50, [] {
            auto* dialog = qobject_cast<ColorDialog*>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            dialog->setColor(Color(255, 0, 0));
            dialog->accept();
        });
        QTest::mouseClick(tool.popup()->colorButton(), Qt::LeftButton);
        QCOMPARE(fill().stops.last().color, Color(255, 0, 0));
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Verlaufsfarbe"));
        // Transparenz des Knotens
        tool.popup()->slider()->setValue(50);
        QVERIFY2(std::abs(int(fill().stops.last().color.a) - 128) <= 1, qPrintable(QString::number(fill().stops.last().color.a)));
        QCOMPARE(fill().stops.last().color.r, std::uint8_t(255));

        // Doppelklick auf die Linie: Zwischenknoten mit der Farbe an dieser Stelle
        const QPoint onLine = screen(QPointF(50, 65));
        QTest::mouseClick(m_canvas.get(), Qt::LeftButton, {}, onLine, 1000);
        QTest::mouseDClick(m_canvas.get(), Qt::LeftButton, {}, onLine, 10);
        QCOMPARE(fill().stops.size(), 3);
        QVERIFY2(std::abs(fill().stops[1].position - 0.25) < 0.02, qPrintable(QString::number(fill().stops[1].position)));
        QCOMPARE(tool.selectedStop(), 1);
        QVERIFY(tool.popup()->isVisible() && tool.popup()->positionField()->isVisible());
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Verlaufsknoten einfügen"));
        screenshot(QStringLiteral("93-zwischenknoten"));
        // Lage im Fenster eintippen
        ExpressionSpinBox* position = tool.popup()->positionField();
        position->selectAll();
        QTest::keyClicks(position, QStringLiteral("60"));
        QTest::keyClick(position, Qt::Key_Return);
        QVERIFY2(std::abs(fill().stops[1].position - 0.6) < 1e-6, qPrintable(QString::number(fill().stops[1].position)));
        // Palettenfarbe färbt den gewählten Zwischenknoten
        QVERIFY(m_canvas->offerPaletteColor(Color(0, 0, 255), true));
        QCOMPARE(fill().stops[1].color, Color(0, 0, 255));
        // Zwischenknoten ziehen
        drag(screen(QPointF(78, 65)), screen(QPointF(90, 65)));
        QVERIFY2(std::abs(fill().stops[1].position - 0.75) < 0.02, qPrintable(QString::number(fill().stops[1].position)));
        // Rechtsklick entfernt ihn
        QTest::mouseClick(m_canvas.get(), Qt::RightButton, {}, screen(QPointF(90, 65)), 1000);
        QCOMPARE(fill().stops.size(), 2);
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Verlaufsknoten löschen"));
        m_editor->document().undo();
        QCOMPARE(fill().stops.size(), 3);
        m_editor->document().undo(); // Lage
        m_editor->document().undo(); // Palettenfarbe
        m_editor->document().undo(); // Lage im Fenster
        m_editor->document().undo(); // eingefügt
        QCOMPARE(fill().stops.size(), 2);

        // Farbe aus der Palette auf den Anfangspunkt bzw. auf die Linie ziehen
        auto dropColor = [this](const QColor& c, const QPoint& at) {
            QMimeData mime;
            mime.setColorData(c);
            QDragEnterEvent enter(at, Qt::CopyAction, &mime, Qt::LeftButton, {});
            QApplication::sendEvent(m_canvas.get(), &enter);
            QDropEvent drop(QPointF(at), Qt::CopyAction, &mime, Qt::LeftButton, {});
            QApplication::sendEvent(m_canvas.get(), &drop);
            return enter.isAccepted() && drop.isAccepted();
        };
        QVERIFY(dropColor(QColor(0, 255, 0), screen(QPointF(30, 65))));
        QCOMPARE(fill().stops.first().color, Color(0, 255, 0));
        QVERIFY(dropColor(QColor(255, 255, 0), screen(QPointF(90, 65))));
        QCOMPARE(fill().stops.size(), 3);
        QCOMPARE(fill().stops[1].color, Color(255, 255, 0));
        QVERIFY(std::abs(fill().stops[1].position - 0.75) < 0.02);

        // Pipette im Fenster: Farbe von der Zeichenfläche (hier das grüne Ende)
        click(screen(QPointF(110, 65)));
        QVERIFY(tool.popup()->isVisible());
        QTest::mouseClick(tool.popup()->eyedropperButton(), Qt::LeftButton);
        click(screen(QPointF(21, 41)));
        const Color picked = fill().stops.last().color;
        QVERIFY2(picked.g > 200 && picked.r < 60 && picked.b < 60, qPrintable(picked.toHex()));

        // Ohne Werkzeug: Palettenfarbe auf ein Objekt gezogen füllt es
        m_canvas->setTool(ToolId::Pick);
        QVERIFY(!tool.popup()->isVisible());
        QVERIFY(dropColor(QColor(10, 20, 30), screen(QPointF(60, 80))));
        QCOMPARE(fill().type, Fill::Type::Solid);
        QCOMPARE(fill().color, Color(10, 20, 30));
    }

    // Interaktive Transparenz: Klick auf einen Endpunkt zeigt den Regler für seinen Wert
    void transparencyNodePopup()
    {
        const NodeId id = drawRect(QRectF(20, 40, 100, 50));
        m_canvas->setTool(ToolId::Transparency);
        drag(screen(QPointF(30, 65)), screen(QPointF(110, 65)));
        auto ramp = [this, id] { return m_editor->document().findAs<DrawObject>(id)->transparency(); };
        QVERIFY(ramp().isGradient());
        auto& tool = *static_cast<EffectTool*>(m_canvas->currentTool());
        const Ramp before = ramp();
        click(screen(QPointF(110, 65)));
        QVERIFY(tool.popup() && tool.popup()->isVisible());
        QCOMPARE(ramp(), before); // Klick verschiebt nichts
        QCOMPARE(tool.popup()->slider()->value(), int(std::lround(before.to)));
        tool.popup()->slider()->setValue(40);
        QCOMPARE(ramp().to, 40.0);
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Knotentransparenz"));
        click(screen(QPointF(30, 65)));
        QCOMPARE(tool.popup()->slider()->value(), int(std::lround(ramp().from)));
        tool.popup()->slider()->setValue(25);
        QCOMPARE(ramp().from, 25.0);
        QCOMPARE(ramp().to, 40.0);
    }

    // --- Kombination aufheben bei Grafiktext ------------------------------------------------------

    void breakApartArtisticText()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        MainWindow window;
        window.resize(1400, 900);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        Canvas* canvas = window.canvas();
        Editor& ed = canvas->editor();
        auto text = std::make_unique<TextObject>(TextObject::Kind::Artistic);
        QList<TextParagraph> paragraphs;
        TextParagraph first;
        first.runs << TextRun{QStringLiteral("THE BEST"), CharFormat(QStringLiteral("Arial"), 36, true)};
        TextParagraph second;
        second.runs << TextRun{QStringLiteral("BEAUTY "), CharFormat(QStringLiteral("Arial"), 24)}
                    << TextRun{QStringLiteral("CENTERS"), CharFormat(QStringLiteral("Times New Roman"), 30, false, true, false, Color(200, 0, 0))};
        second.align = TextParagraph::Align::Center;
        paragraphs << first << second;
        text->setParagraphs(paragraphs);
        Style style;
        style.fill = Fill::solid(Color(0, 0, 255));
        text->setStyle(style);
        text->setTransform(QTransform().translate(40, 60).rotate(10).scale(1.2, 1.0));
        const QPainterPath originalPath = text->path();
        const NodeId id = ed.addObject(std::move(text), QStringLiteral("T"));
        canvas->setFocus();
        QApplication::processEvents();

        auto union_ = [&ed] {
            QRectF r;
            for (NodeId n : ed.selection())
                r |= ed.document().findAs<DrawObject>(n)->path().boundingRect();
            return r;
        };
        auto texts = [&ed] {
            QStringList list;
            for (NodeId n : ed.selection())
                list << ed.document().findAs<TextObject>(n)->plainText();
            return list;
        };
        // Glyphen sitzen genau dort, wo sie waren: Pfade decken sich (bis auf Rundung)
        auto samePlace = [&ed](const QPainterPath& reference) {
            QPainterPath all;
            for (NodeId n : ed.selection())
                all.addPath(ed.document().findAs<DrawObject>(n)->path());
            const QRectF a = all.boundingRect();
            const QRectF b = reference.boundingRect();
            return std::abs(a.left() - b.left()) < 0.05 && std::abs(a.top() - b.top()) < 0.05
                   && std::abs(a.right() - b.right()) < 0.05 && std::abs(a.bottom() - b.bottom()) < 0.05;
        };

        // Zwei Zeilen: je ein Textobjekt
        QTest::keyClick(canvas, Qt::Key_K, Qt::ControlModifier);
        QCOMPARE(ed.document().undoText(), QStringLiteral("Kombination aufheben"));
        QCOMPARE(texts(), (QStringList{QStringLiteral("THE BEST"), QStringLiteral("BEAUTY CENTERS")}));
        QVERIFY(!ed.document().find(id));
        QVERIFY2(samePlace(originalPath), qPrintable(describe(union_()) + QLatin1Char(' ') + describe(originalPath.boundingRect())));
        const auto* line2 = ed.document().findAs<TextObject>(ed.selection().last());
        QVERIFY(line2 && line2->kind() == TextObject::Kind::Artistic);
        QCOMPARE(line2->paragraphs().first().runs.size(), 2); // Formate bleiben
        QCOMPARE(line2->paragraphs().first().runs.last().format.color, std::optional<Color>(Color(200, 0, 0)));
        QCOMPARE(line2->style().fill.color, Color(0, 0, 255));
        // Lage jeder Zeile: die Glyphen der zweiten Zeile decken den unteren Teil des Originals
        QVERIFY(std::abs(line2->path().boundingRect().bottom() - originalPath.boundingRect().bottom()) < 0.05);

        // Eine Zeile: je ein Objekt pro Wort
        const QPainterPath linePath = line2->path();
        ed.setSelection({line2->id()});
        QTest::keyClick(canvas, Qt::Key_K, Qt::ControlModifier);
        QCOMPARE(texts(), (QStringList{QStringLiteral("BEAUTY"), QStringLiteral("CENTERS")}));
        QVERIFY2(samePlace(linePath), qPrintable(describe(union_()) + QLatin1Char(' ') + describe(linePath.boundingRect())));

        // Ein Wort: je ein Objekt pro Zeichen
        const NodeId word = ed.selection().first();
        const QPainterPath wordPath = ed.document().findAs<DrawObject>(word)->path();
        ed.setSelection({word});
        QTest::keyClick(canvas, Qt::Key_K, Qt::ControlModifier);
        QCOMPARE(texts(), (QStringList{QStringLiteral("B"), QStringLiteral("E"), QStringLiteral("A"), QStringLiteral("U"),
                                       QStringLiteral("T"), QStringLiteral("Y")}));
        QVERIFY2(samePlace(wordPath), qPrintable(describe(union_()) + QLatin1Char(' ') + describe(wordPath.boundingRect())));
        // Ein Zeichen lässt sich nicht weiter teilen
        ed.setSelection({ed.selection().first()});
        QVERIFY(!ed.canBreakApartSelection());
        // Rückgängig stellt das Wort wieder her
        ed.document().undo();
        QVERIFY(ed.document().find(word));
    }

    // --- Einheiten in der Eigenschaftsleiste -------------------------------------------------------

    void propertyBarUnits()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        MainWindow window;
        window.resize(1600, 900);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        Canvas* canvas = window.canvas();
        Editor& ed = canvas->editor();
        auto visibleSpin = [&window](const QString& tip) -> ExpressionSpinBox* {
            for (auto* s : window.findChildren<ExpressionSpinBox*>())
                if (s->isVisible() && s->toolTip().startsWith(tip))
                    return s;
            return nullptr;
        };
        // Ohne Auswahl: Einheitenliste mit allen Einheiten
        ed.clearSelection();
        QApplication::processEvents();
        QComboBox* units = nullptr;
        for (QComboBox* c : window.findChildren<QComboBox*>())
            if (c->objectName() == QLatin1String("documentUnits") && c->isVisible())
                units = c;
        QVERIFY(units);
        QCOMPARE(units->count(), int(Options::unitNames().size()));
        QCOMPARE(units->currentIndex(), 0);
        ExpressionSpinBox* pageWidth = visibleSpin(QStringLiteral("Seitenbreite"));
        QVERIFY(pageWidth && pageWidth->text().endsWith(QStringLiteral(" mm")));
        // Punkt wählen: Dokumenteinstellung, Lineale und Felder folgen
        units->setCurrentIndex(4);
        emit units->activated(4);
        QApplication::processEvents();
        QCOMPARE(Options::instance().rulerUnit(true), 4);
        QCOMPARE(Options::instance().rulerUnit(false), 4);
        QVERIFY(std::abs(Options::instance().rulerValue(25.4, true) - 72.0) < 1e-9);
        QVERIFY2(pageWidth->text().endsWith(QStringLiteral(" pt")), qPrintable(pageWidth->text()));
        QVERIFY(std::abs(pageWidth->valueFromText(pageWidth->text()) - ed.currentPage().size().width()) < 0.01);
        if (!qEnvironmentVariable("VEK_SCREENSHOT_DIR").isEmpty())
            window.grab().save(qEnvironmentVariable("VEK_SCREENSHOT_DIR") + QStringLiteral("/94-einheiten-punkt.png"));

        // Rechteck 100 x 50 mm: Breite in pt, Eingabe in pt und mit anderer Einheit
        auto r = std::make_unique<RectangleObject>(QSizeF(100, 50));
        r->setTransform(QTransform::fromTranslate(20, 30));
        Style s;
        s.stroke = Color(0, 0, 0);
        s.strokeWidth = 0.5;
        r->setStyle(s);
        const NodeId id = ed.addObject(std::move(r), QStringLiteral("R"));
        QApplication::processEvents();
        ExpressionSpinBox* width = visibleSpin(QStringLiteral("Breite"));
        QVERIFY(width);
        QCOMPARE(width->text(), QLocale().toString(100.0 / 25.4 * 72.0, 'f', 3) + QStringLiteral(" pt"));
        auto type = [](QAbstractSpinBox* spin, const QString& text) {
            spin->setFocus();
            spin->selectAll();
            QTest::keyClicks(spin, text);
            QTest::keyClick(spin, Qt::Key_Return);
            QApplication::processEvents();
        };
        type(width, QStringLiteral("144"));
        QVERIFY2(std::abs(ed.document().findAs<DrawObject>(id)->boundingRect().width() - 50.8) < 1e-3,
                 qPrintable(QString::number(ed.document().findAs<DrawObject>(id)->boundingRect().width())));
        type(width, QStringLiteral("20 mm"));
        QVERIFY(std::abs(ed.document().findAs<DrawObject>(id)->boundingRect().width() - 20.0) < 1e-3);
        type(width, QStringLiteral("2*0,5 in"));
        QVERIFY(std::abs(ed.document().findAs<DrawObject>(id)->boundingRect().width() - 25.4) < 1e-3);
        QVERIFY(std::abs(evaluateLength(QStringLiteral("3 cm"), 4).value() - 30.0) < 1e-9);
        QVERIFY(std::abs(evaluateLength(QStringLiteral("72"), 4).value() - 25.4) < 1e-9);
        QVERIFY(std::abs(evaluateLength(QStringLiteral("2\""), 0).value() - 50.8) < 1e-9);
        QVERIFY(!evaluateLength(QStringLiteral("5 xy"), 0));
        ExpressionSpinBox* x = visibleSpin(QStringLiteral("Lage x"));
        QVERIFY(x && x->text().endsWith(QStringLiteral(" pt")));

        // Konturbreite: in pt angezeigt, pt-Stufen in der Liste, Eingabe in pt
        QComboBox* outline = nullptr;
        for (QComboBox* c : window.findChildren<QComboBox*>())
            if (c->isVisible() && c->toolTip() == QStringLiteral("Konturbreite"))
                outline = c;
        QVERIFY(outline);
        QVERIFY2(outline->currentText().endsWith(QStringLiteral(" pt")), qPrintable(outline->currentText()));
        QVERIFY(outline->findText(QStringLiteral("8 pt")) >= 0);
        QVERIFY(outline->findText(QLocale().toString(0.75) + QStringLiteral(" pt")) >= 0);
        QVERIFY(outline->findText(QStringLiteral("Haarlinie")) >= 0);
        const int eight = outline->findText(QStringLiteral("8 pt"));
        outline->setCurrentIndex(eight);
        emit outline->activated(eight);
        QVERIFY(std::abs(ed.document().findAs<DrawObject>(id)->style().strokeWidth - 8 * 25.4 / 72) < 1e-9);
        outline->lineEdit()->selectAll();
        QTest::keyClicks(outline->lineEdit(), QStringLiteral("2"));
        QTest::keyClick(outline->lineEdit(), Qt::Key_Return);
        QVERIFY2(std::abs(ed.document().findAs<DrawObject>(id)->style().strokeWidth - 2 * 25.4 / 72) < 1e-9,
                 qPrintable(QString::number(ed.document().findAs<DrawObject>(id)->style().strokeWidth)));
        QApplication::processEvents();
        QCOMPARE(outline->currentText(), QStringLiteral("2 pt"));
        if (!qEnvironmentVariable("VEK_SCREENSHOT_DIR").isEmpty())
            window.grab().save(qEnvironmentVariable("VEK_SCREENSHOT_DIR") + QStringLiteral("/95-objekt-in-punkt.png"));

        // Zurück auf mm: Felder zeigen wieder mm
        ed.clearSelection();
        QApplication::processEvents();
        units->setCurrentIndex(0);
        emit units->activated(0);
        QApplication::processEvents();
        QVERIFY(pageWidth->text().endsWith(QStringLiteral(" mm")));
        ed.setSelection({id});
        QApplication::processEvents();
        QVERIFY(visibleSpin(QStringLiteral("Breite"))->text().endsWith(QStringLiteral(" mm")));
        // Menü: An Ort und Stelle duplizieren mit Ziffernblock +
        bool found = false;
        for (QAction* a : window.findChildren<QAction*>())
            found = found || (a->text() == QStringLiteral("An &Ort und Stelle duplizieren")
                              && a->shortcut() == QKeySequence(Qt::KeypadModifier | Qt::Key_Plus));
        QVERIFY(found);
    }
    // --- Formwerkzeug an Grundformen, Knoten transformieren, Andockfenster "Ecken" -------------

    void shapeToolRoundsRectangleCorners()
    {
        const NodeId id = drawRect(QRectF(20, 40, 100, 50));
        auto rect = [&] { return m_editor->document().findAs<RectangleObject>(id); };
        m_canvas->setTool(ToolId::Shape);
        ShapeTool& shape = m_canvas->shapeTool();
        QVERIFY(!shape.curve());
        QVERIFY(shape.basicShape());
        QCOMPARE(shape.basicShapeNodes().size(), 4);
        // Ecke oben links entlang der Kante ziehen: alle vier Ecken gleich (Schloss zu)
        drag(screen(QPointF(20, 40)), screen(QPointF(32, 41)), {}, QStringLiteral("80-form-rechteck-ecken-vorschau"));
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Eckenrundung bearbeiten"));
        QVERIFY(node_cast<RectangleObject>(m_editor->document().findAs<DrawObject>(id)));
        for (int c = 0; c < 4; ++c)
            QVERIFY2(std::abs(rect()->cornerRadius(RectangleObject::Corner(c)) - 12.0) < kTol,
                     qPrintable(QString::number(rect()->cornerRadius(RectangleObject::Corner(c)))));
        // Eigenschaftsleiste liest denselben Wert
        QVERIFY(std::abs(rect()->propertyValue(QStringLiteral("radiusTopRight")).toDouble() - 12.0) < kTol);
        // Knoten sitzt jetzt am Beginn der Rundung
        QVERIFY(QLineF(shape.basicShapeNodes()[1], QPointF(120 - rect()->cornerRadius(RectangleObject::TopRight), 40)).length() < kTol);
        // Ein Schritt rückgängig
        m_editor->document().undo();
        QCOMPARE(rect()->cornerRadius(RectangleObject::TopLeft), 0.0);
        m_editor->document().redo();

        // Schloss offen: nur die gezogene Ecke
        m_editor->setObjectProperty(QStringLiteral("cornersLinked"), false, QStringLiteral("Ecken"));
        const QPointF br = shape.basicShapeNodes()[2];
        drag(screen(br), screen(QPointF(120 - 22, 90)));
        QVERIFY2(std::abs(rect()->cornerRadius(RectangleObject::BottomRight) - 22.0) < kTol,
                 qPrintable(QString::number(rect()->cornerRadius(RectangleObject::BottomRight))));
        QVERIFY(std::abs(rect()->cornerRadius(RectangleObject::TopLeft) - 12.0) < kTol);
        screenshot(QStringLiteral("81-form-rechteck-ecken"));

        // Gedrehtes Rechteck: Ziehen entlang der gedrehten Kante, Rechteck bleibt Rechteck
        m_editor->setObjectProperty(QStringLiteral("cornersLinked"), true, QStringLiteral("Ecken"));
        m_editor->setObjectProperty(QStringLiteral("radiusTopLeft"), 0.0, QStringLiteral("Ecken"));
        m_canvas->setTool(ToolId::Pick);
        m_editor->setSelection({id});
        m_editor->rotateSelectionTo(30);
        m_canvas->setTool(ToolId::Shape);
        const QTransform t = rect()->transform();
        const QPointF corner = t.map(QPointF(0, 0));
        const QPointF along = t.map(QPointF(15, 0));
        drag(screen(corner), screen(along));
        QVERIFY(rect());
        QVERIFY2(std::abs(rect()->cornerRadius(RectangleObject::BottomLeft) - 15.0) < 1.0,
                 qPrintable(QString::number(rect()->cornerRadius(RectangleObject::BottomLeft))));
        // Eckentyp wird beachtet: abgeschrägt ergibt Geraden statt Bögen
        m_editor->setObjectProperty(QStringLiteral("cornerType"), 2, QStringLiteral("Eckentyp"));
        QVERIFY(rect()->localPath().elementCount() > 4);
    }

    void shapeToolMakesPieAndArcFromEllipse()
    {
        m_canvas->setTool(ToolId::Ellipse);
        drag(screen(QPointF(40, 40)), screen(QPointF(140, 140)));
        const NodeId id = m_editor->selection().value(0);
        auto ellipse = [&] { return m_editor->document().findAs<EllipseObject>(id); };
        QVERIFY(ellipse());
        m_canvas->setTool(ToolId::Shape);
        ShapeTool& shape = m_canvas->shapeTool();
        QCOMPARE(shape.basicShapeNodes().size(), 1);
        const QPointF node = shape.basicShapeNodes()[0];
        QVERIFY(QLineF(node, QPointF(140, 90)).length() < kTol);
        // Knoten innen gegen den Uhrzeigersinn nach oben ziehen: Kreissegment
        drag(screen(node), screen(QPointF(90, 60)), {}, QStringLiteral("82-form-ellipse-vorschau"));
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Ellipse bearbeiten"));
        QCOMPARE(ellipse()->kind(), EllipseObject::Kind::Pie);
        QVERIFY2(std::abs(ellipse()->startAngle() - 90.0) < 2.0, qPrintable(QString::number(ellipse()->startAngle())));
        QVERIFY2(std::abs(ellipse()->endAngle()) < 2.0, qPrintable(QString::number(ellipse()->endAngle())));
        // Die Lücke liegt im überstrichenen Viertel oben rechts
        QVERIFY(!ellipse()->path().contains(QPointF(110, 70)));
        QVERIFY(ellipse()->path().contains(QPointF(70, 110)));
        QCOMPARE(shape.basicShapeNodes().size(), 2);
        screenshot(QStringLiteral("83-form-ellipse-segment"));

        // Anfangsknoten nach aussen ziehen: Bogen, Winkel in der Eigenschaftsleiste
        drag(screen(shape.basicShapeNodes()[0]), screen(QPointF(90 - 45, 90 - 45 - 10)));
        QCOMPARE(ellipse()->kind(), EllipseObject::Kind::Arc);
        const double expected = std::atan2(45.0 + 10.0, -45.0) * 180.0 / std::numbers::pi;
        QVERIFY2(std::abs(ellipse()->propertyValue(QStringLiteral("startAngle")).toDouble() - expected) < 2.0,
                 qPrintable(QString::number(ellipse()->startAngle())));
        m_editor->document().undo();
        QCOMPARE(ellipse()->kind(), EllipseObject::Kind::Pie);
    }

    void shapeToolEditsPolygonSymmetrically()
    {
        m_canvas->setTool(ToolId::Star);
        drag(screen(QPointF(40, 40)), screen(QPointF(140, 140)));
        const NodeId id = m_editor->selection().value(0);
        auto poly = [&] { return m_editor->document().findAs<PolygonObject>(id); };
        QVERIFY(poly() && poly()->kind() == PolygonObject::Kind::Star);
        const int n = poly()->points();
        m_canvas->setTool(ToolId::Shape);
        ShapeTool& shape = m_canvas->shapeTool();
        QCOMPARE(shape.basicShapeNodes().size(), 2 * n);
        const QList<QPointF> before = shape.basicShapeNodes();
        const QPointF center = poly()->boundingRect().center();
        auto radiusOf = [&](const QPointF& p) { return QLineF(p, poly()->transform().map(QPointF(poly()->size().width() / 2, poly()->size().height() / 2))).length(); };
        const double r0 = radiusOf(before[0]);
        Q_UNUSED(center);
        // Spitze nach aussen ziehen: alle Spitzen wandern mit, die Einbuchtungen bleiben
        drag(screen(before[0]), screen(before[0] + QPointF(0, -10)), {}, QStringLiteral("84-form-stern-vorschau"));
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Polygon bearbeiten"));
        QVERIFY(poly());
        const QList<QPointF> after = shape.basicShapeNodes();
        for (int j = 0; j < 2 * n; j += 2)
            QVERIFY2(std::abs(radiusOf(after[j]) - (r0 + 10.0)) < kTol, qPrintable(QStringLiteral("%1: %2").arg(j).arg(radiusOf(after[j]))));
        for (int j = 1; j < 2 * n; j += 2)
            QVERIFY(QLineF(after[j], before[j]).length() < 1e-6);
        screenshot(QStringLiteral("85-form-stern"));

        // Polygon: Seitenmitte nach innen ziehen ergibt ein geschwungenes Polygon
        m_editor->clearSelection();
        m_canvas->setTool(ToolId::Polygon);
        drag(screen(QPointF(40, 160)), screen(QPointF(140, 260)));
        const NodeId pid = m_editor->selection().value(0);
        m_canvas->setTool(ToolId::Shape);
        const QList<QPointF> nodes = shape.basicShapeNodes();
        const QPointF mid = nodes[1];
        const auto* drawn = m_editor->document().findAs<PolygonObject>(pid);
        const QPointF c = drawn->transform().map(QPointF(drawn->size().width() / 2, drawn->size().height() / 2));
        drag(screen(mid), screen(mid + (c - mid) * 0.4));
        const auto* p = m_editor->document().findAs<PolygonObject>(pid);
        QVERIFY(p->nodeShape().innerMoved());
        QCOMPARE(p->nodeShape().outerScale, 1.0);
        bool curved = false;
        for (int i = 0; i < p->localPath().elementCount(); ++i)
            curved = curved || p->localPath().elementAt(i).type == QPainterPath::CurveToElement;
        QVERIFY(curved);
        // Alle Seitenmitten folgen
        const QList<QPointF> moved = shape.basicShapeNodes();
        for (int j = 1; j < moved.size(); j += 2)
            QVERIFY(QLineF(moved[j], c).length() < QLineF(nodes[j], c).length() - 5.0);
        screenshot(QStringLiteral("86-form-polygon-geschwungen"));
    }

    void shapeToolTransformsSelectedNodes()
    {
        drawRect(QRectF(40, 60, 60, 40));
        m_editor->convertSelectionToCurves();
        const NodeId id = m_editor->selection().value(0);
        auto curve = [&] { return m_editor->document().findAs<CurveObject>(id); };
        QVERIFY(curve());
        m_canvas->setTool(ToolId::Shape);
        ShapeTool& shape = m_canvas->shapeTool();
        // Zwei Knoten wählen (Rahmen um die rechte Seite)
        drag(screen(QPointF(95, 50)), screen(QPointF(110, 110)));
        QCOMPARE(shape.selectedNodes().size(), 2);
        QVERIFY(shape.canTransformNodes());
        auto pointsOf = [&](const QList<NodeRef>& refs) {
            QList<QPointF> out;
            for (NodeRef r : refs)
                out << curve()->transform().map(curve()->node(r).point);
            return out;
        };
        const QList<NodeRef> sel = shape.selectedNodes();
        const QList<QPointF> before = pointsOf(sel);
        QVERIFY(near(shape.selectedNodesBounds(), QRectF(100, 60, 0, 40)));

        // Drehen: Eckgriff um 90° um die Mitte führen
        shape.setNodeTransform(ShapeTool::NodeTransform::Rotate);
        const QPointF c = m_canvas->toScreen(shape.selectedNodesBounds().center());
        const QPointF h = shape.handlePosition(0);
        const QPointF target = c + QPointF(-(h - c).y(), (h - c).x()); // +90° (im Bildschirm im Uhrzeigersinn)
        drag(h.toPoint(), target.toPoint(), {}, QStringLiteral("87-form-knoten-drehen-vorschau"));
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Knoten drehen"));
        const QList<QPointF> rotated = pointsOf(sel);
        const QPointF m(100, 80);
        for (int i = 0; i < 2; ++i) {
            const QPointF d = before[i] - m;
            const QPointF expected = m + QPointF(-d.y(), d.x());
            QVERIFY2(QLineF(rotated[i], expected).length() < 1.0,
                     qPrintable(QStringLiteral("%1,%2").arg(rotated[i].x()).arg(rotated[i].y())));
        }
        // Die übrigen Knoten bleiben stehen
        QVERIFY(QLineF(curve()->transform().map(curve()->node(NodeRef{0, 0}).point), QPointF(40, 60)).length() < kTol);
        screenshot(QStringLiteral("88-form-knoten-gedreht"));

        // Skalieren: rechte Seite von der linken aus auf das Doppelte strecken
        shape.setNodeTransform(ShapeTool::NodeTransform::Scale);
        const QRectF b = shape.selectedNodesBounds(); // jetzt waagrecht: 80..120 bei y = 80
        QVERIFY2(near(b, QRectF(80, 80, 40, 0), 1.0), qPrintable(describe(b)));
        const QPointF right = shape.handlePosition(5);
        const QPointF left = shape.handlePosition(7);
        drag(right.toPoint(), (right + (right - left)).toPoint());
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Knoten skalieren"));
        QVERIFY2(std::abs(shape.selectedNodesBounds().width() - 80.0 - 18.0 / m_canvas->viewTransform().m11()) < 2.0,
                 qPrintable(describe(shape.selectedNodesBounds())));

        // Spiegeln
        const QList<QPointF> pre = pointsOf(sel);
        shape.reflectNodes(Qt::Horizontal);
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Knoten spiegeln"));
        const QList<QPointF> mirrored = pointsOf(sel);
        QVERIFY(QLineF(mirrored[0], QPointF(2 * shape.selectedNodesBounds().center().x() - pre[0].x(), pre[0].y())).length() < 1e-6);
        m_editor->document().undo();
        QCOMPARE(pointsOf(sel), pre);
    }

    void groupStyleChangesKeepOtherValues()
    {
        // Kontur einer Gruppe entfernen darf die Füllungen der Kinder nicht löschen (und umgekehrt)
        const NodeId a = drawRect(QRectF(20, 20, 20, 20));
        m_editor->setSelectionFill(Fill::solid(Color(255, 0, 0)));
        const NodeId b = drawRect(QRectF(50, 20, 20, 20));
        m_editor->setSelectionFill(Fill::solid(Color(0, 0, 255)));
        m_editor->setSelection({a, b});
        m_editor->groupSelection();
        m_editor->setSelectionStroke(std::nullopt);
        auto style = [&](NodeId id) { return m_editor->document().findAs<DrawObject>(id)->style(); };
        QCOMPARE(style(a).fill, Fill::solid(Color(255, 0, 0)));
        QCOMPARE(style(b).fill, Fill::solid(Color(0, 0, 255)));
        QVERIFY(!style(a).stroke && !style(b).stroke);
        m_editor->setSelectionStroke(Color(0, 128, 0));
        m_editor->setSelectionFill(Fill::solid(Color(255, 255, 0)));
        QCOMPARE(style(a).stroke, std::optional<Color>(Color(0, 128, 0)));
        QCOMPARE(style(b).fill, Fill::solid(Color(255, 255, 0)));
    }

    void shapeToolRotatesSingleNodeAroundCurve()
    {
        // Wie beim Vorbild: ein einzelner Knoten dreht sich um die Mitte aller Knoten der Kurve
        drawRect(QRectF(40, 40, 40, 40));
        m_editor->convertSelectionToCurves();
        const NodeId id = m_editor->selection().value(0);
        auto curve = [&] { return m_editor->document().findAs<CurveObject>(id); };
        m_canvas->setTool(ToolId::Shape);
        ShapeTool& shape = m_canvas->shapeTool();
        click(screen(QPointF(80, 80)));
        QCOMPARE(shape.selectedNodes().size(), 1);
        QVERIFY(shape.canTransformNodes());
        QVERIFY(near(shape.selectedNodesBounds(), QRectF(40, 40, 40, 40)));
        const NodeRef r = shape.selectedNodes().first();
        shape.setNodeTransform(ShapeTool::NodeTransform::Rotate);
        // Griff unten rechts auf den gegenüberliegenden Griff ziehen = 180°
        drag(shape.handlePosition(2).toPoint(), shape.handlePosition(0).toPoint());
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Knoten drehen"));
        const QPointF moved = curve()->transform().map(curve()->node(r).point);
        QVERIFY2(QLineF(moved, QPointF(40, 40)).length() < 0.5, qPrintable(QStringLiteral("%1,%2").arg(moved.x()).arg(moved.y())));
    }

    void cornersDockerFilletsTriangle()
    {
        m_canvas->setTool(ToolId::Polygon);
        drag(screen(QPointF(40, 40)), screen(QPointF(140, 140)));
        m_editor->setObjectProperty(QStringLiteral("points"), 3.0, QStringLiteral("Punkte"));
        const NodeId id = m_editor->selection().value(0);
        const auto* tri = m_editor->document().findAs<PolygonObject>(id);
        QList<QPointF> v;
        const QList<QPointF> ring = tri->shapeNodes();
        for (int j = 0; j < ring.size(); j += 2)
            v << tri->transform().map(ring[j]);
        QCOMPARE(v.size(), 3);

        CornersDocker docker(*m_editor, *m_canvas);
        QVERIFY(docker.applyButton()->isEnabled());
        docker.setOperation(pathops::CornerOp::Fillet);
        docker.setRadius(8.0);
        QTest::mouseClick(docker.applyButton(), Qt::LeftButton);
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Ecken"));
        const auto* c = m_editor->document().findAs<CurveObject>(m_editor->selection().value(0));
        QVERIFY(c);
        QCOMPARE(c->nodeCount(), 9);
        // Jede Ecke ist ein Kreisbogen mit 8 mm Radius, tangential an beide Kanten
        const QPainterPath path = c->path();
        for (int i = 0; i < 3; ++i) {
            const QPointF a = v[(i + 2) % 3] - v[i], b = v[(i + 1) % 3] - v[i];
            const QPointF ua = a / QLineF(QPointF(), a).length(), ub = b / QLineF(QPointF(), b).length();
            const double theta = std::acos(std::clamp(QPointF::dotProduct(ua, ub), -1.0, 1.0));
            const QPointF bis = (ua + ub) / QLineF(QPointF(), ua + ub).length();
            const QPointF center = v[i] + bis * (8.0 / std::sin(theta / 2.0));
            const double d = 8.0 / std::tan(theta / 2.0);
            double worst = 0.0;
            for (int k = 0; k <= 3000; ++k) {
                const QPointF p = path.pointAtPercent(k / 3000.0);
                if (QLineF(p, v[i]).length() < d * 0.95)
                    worst = std::max(worst, std::abs(QLineF(p, center).length() - 8.0));
            }
            QVERIFY2(worst < 0.05, qPrintable(QStringLiteral("Ecke %1: %2").arg(i).arg(worst)));
        }
        if (!qEnvironmentVariable("VEK_SCREENSHOT_DIR").isEmpty())
            docker.grab().save(qEnvironmentVariable("VEK_SCREENSHOT_DIR") + QStringLiteral("/89-ecken-docker.png"));
        screenshot(QStringLiteral("90-ecken-dreieck"));
        // Ein Schritt rückgängig: wieder das Polygon
        m_editor->document().undo();
        QVERIFY(m_editor->document().findAs<PolygonObject>(id));

        // Nur gewählte Knoten (Formwerkzeug): Quadrat als Kurve, eine Ecke abschrägen
        m_editor->clearSelection();
        drawRect(QRectF(40, 160, 50, 50));
        m_editor->convertSelectionToCurves();
        const NodeId sq = m_editor->selection().value(0);
        m_canvas->setTool(ToolId::Shape);
        click(screen(QPointF(90, 210)));
        QCOMPARE(m_canvas->shapeTool().selectedNodes().size(), 1);
        docker.setOperation(pathops::CornerOp::Chamfer);
        docker.setRadius(10.0);
        QVERIFY(docker.apply());
        const auto* s = m_editor->document().findAs<CurveObject>(sq);
        QCOMPARE(s->nodeCount(), 5);
        QVERIFY(!s->path().contains(QPointF(88, 208)));
        QVERIFY(s->path().contains(QPointF(42, 162)));
    }



    // --- Mischmodi, Innenschatten, gedrehte Extrusion -----------------------------------------
    void blendModesRender()
    {
        m_canvas->resize(800, 600);
        auto add = [this](const QRectF& r, const Color& c) {
            auto o = std::make_unique<RectangleObject>(r.size());
            o->setTransform(QTransform::fromTranslate(r.left(), r.top()));
            Style s;
            s.fill = Fill::solid(c);
            s.stroke.reset();
            o->setStyle(s);
            return m_editor->addObject(std::move(o), QStringLiteral("R"));
        };
        const NodeId bottom = add(QRectF(20, 20, 40, 40), Color(255, 255, 0));
        const NodeId top = add(QRectF(40, 40, 40, 40), Color(0, 255, 255));
        Q_UNUSED(bottom);
        auto pixel = [this](const QPointF& doc) {
            QImage img(3, 3, QImage::Format_ARGB32_Premultiplied);
            img.fill(Qt::white);
            QPainter p(&img);
            m_canvas->renderPage(p, 0, 8.0, QPointF(1.5, 1.5) - doc * 8.0, true);
            p.end();
            return img.pixelColor(1, 1);
        };
        auto close = [](const QColor& a, const QColor& b, int tol = 3) {
            return std::abs(a.red() - b.red()) <= tol && std::abs(a.green() - b.green()) <= tol && std::abs(a.blue() - b.blue()) <= tol;
        };
        // Svg (Vektorgerät, kein Raster): Untergrund wird neu gezeichnet, Ergebnis als Bild
        auto svgPixel = [this](const QPointF& doc) {
            QBuffer buffer;
            buffer.open(QIODevice::WriteOnly);
            QSvgGenerator gen;
            gen.setOutputDevice(&buffer);
            gen.setSize(QSize(420, 594));
            gen.setViewBox(QRectF(0, 0, 420, 594));
            {
                QPainter p(&gen);
                m_canvas->renderPage(p, 0, 2.0, QPointF(0, 0), true);
            }
            QSvgRenderer svg(buffer.data());
            QImage img(420, 594, QImage::Format_ARGB32_Premultiplied);
            img.fill(Qt::white);
            QPainter p(&img);
            svg.render(&p);
            p.end();
            return img.pixelColor((doc * 2.0).toPoint());
        };
        PropertyBar bar(*m_editor, *m_canvas);
        bar.show();
        m_editor->setSelection({top});
        m_canvas->setTool(ToolId::Transparency);
        QApplication::processEvents();
        QComboBox* mode = nullptr;
        for (QComboBox* c : bar.findChildren<QComboBox*>())
            if (c->toolTip() == QStringLiteral("Mischmodus") && c->isVisibleTo(&bar))
                mode = c;
        QVERIFY(mode);
        QVERIFY(mode->count() >= 27);
        QCOMPARE(mode->currentData().toInt(), int(BlendMode::Normal));
        auto choose = [mode](BlendMode m) {
            const int i = mode->findData(int(m));
            mode->setCurrentIndex(i);
            emit mode->activated(i);
        };
        // Normal: Cyan deckt
        QVERIFY(close(pixel(QPointF(50, 50)), QColor(0, 255, 255)));
        // Multiplizieren (QPainter-Modus): Gelb x Cyan = Grün, über Weiss bleibt Cyan
        choose(BlendMode::Multiply);
        QCOMPARE(m_editor->document().findAs<DrawObject>(top)->blendMode(), BlendMode::Multiply);
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Mischmodus"));
        QVERIFY2(close(pixel(QPointF(50, 50)), QColor(0, 255, 0)), qPrintable(pixel(QPointF(50, 50)).name()));
        QVERIFY(close(pixel(QPointF(70, 70)), QColor(0, 255, 255)));
        QVERIFY(close(pixel(QPointF(30, 30)), QColor(255, 255, 0)));
        screenshot(QStringLiteral("mischmodus-multiplizieren"));
        QVERIFY2(close(svgPixel(QPointF(50, 50)), QColor(0, 255, 0), 6), qPrintable(svgPixel(QPointF(50, 50)).name()));
        // SVG-Export: als Vektor mit mix-blend-mode
        QTemporaryDir dir;
        auto exported = [this, &dir] {
            const QString path = dir.filePath(QStringLiteral("blend.svg"));
            if (!output::exportSvg(*m_canvas, path, 0))
                return QByteArray();
            QFile f(path);
            f.open(QIODevice::ReadOnly);
            return f.readAll();
        };
        QByteArray svgText = exported();
        QVERIFY2(svgText.contains("style=\"mix-blend-mode:multiply\""), svgText.constData());
        QVERIFY(!svgText.contains("0.531") && !svgText.contains("<image"));
        // Subtrahieren (eigene Rechnung, Untergrund aus dem Gerät): Gelb - Cyan = Rot, Weiss - Cyan = Rot
        choose(BlendMode::Subtract);
        QVERIFY2(close(pixel(QPointF(50, 50)), QColor(255, 0, 0)), qPrintable(pixel(QPointF(50, 50)).name()));
        QVERIFY(close(pixel(QPointF(70, 70)), QColor(255, 0, 0)));
        // ... am Bildschirm (Widget) und im SVG gleich
        const QColor shown = m_canvas->grab().toImage().pixelColor(screen(QPointF(50, 50)));
        QVERIFY2(close(shown, QColor(255, 0, 0)), qPrintable(shown.name()));
        QVERIFY2(close(svgPixel(QPointF(50, 50)), QColor(255, 0, 0), 6), qPrintable(svgPixel(QPointF(50, 50)).name()));
        QVERIFY(close(svgPixel(QPointF(30, 30)), QColor(255, 255, 0), 6));
        // ... ohne CSS-Entsprechung gerastert
        svgText = exported();
        QVERIFY(!svgText.contains("mix-blend-mode") && svgText.contains("<image"));
        // PDF: Untergrund neu gezeichnet und mit dem Objekt gerastert (Qt schreibt keine /BM)
        output::ExportOptions pdf;
        const QString pdfPath = dir.filePath(QStringLiteral("blend.pdf"));
        QVERIFY(output::exportPdf(*m_canvas, pdfPath, pdf));
        QVERIFY(QFileInfo(pdfPath).size() > 1000);
        screenshot(QStringLiteral("mischmodus-subtrahieren"));
        // Farbton: Farbton von Cyan, Sättigung/Helligkeit des gelben Untergrunds
        choose(BlendMode::Hue);
        const QColor hue = pixel(QPointF(50, 50));
        QVERIFY2(close(hue, QColor(161, 255, 255)), qPrintable(hue.name())); // W3C: Farbton 180°, Helligkeit des Gelbs
        // Mit Transparenz: Multiplizieren zur Hälfte
        choose(BlendMode::Multiply);
        m_editor->setSelectionEffect(Effect::Transparency, [](Ramp& r) { r = Ramp::uniform(50); }, QStringLiteral("Transparenz"));
        QVERIFY2(close(pixel(QPointF(50, 50)), QColor(128, 255, 0)), qPrintable(pixel(QPointF(50, 50)).name()));
        // In der Gruppe: der Untergrund ausserhalb der Gruppe zählt (SVG neu gezeichnet)
        m_editor->setSelection({top});
        m_editor->groupSelection();
        QVERIFY2(close(svgPixel(QPointF(50, 50)), QColor(128, 255, 0), 8), qPrintable(svgPixel(QPointF(50, 50)).name()));
        // Rückgängig
        while (m_editor->document().undoText() != QStringLiteral("Mischmodus"))
            m_editor->document().undo();
        m_editor->document().undo();
        QCOMPARE(m_editor->document().findAs<DrawObject>(top)->blendMode(), BlendMode::Hue);
        m_canvas->setTool(ToolId::Pick);
    }

    void innerShadowAndShadowBlend()
    {
        m_canvas->resize(800, 600);
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
        PropertyBar bar(*m_editor, *m_canvas);
        bar.show();
        m_editor->setSelection({id});
        m_canvas->setTool(ToolId::DropShadow);
        QApplication::processEvents();
        auto combo = [&bar](const QString& tip) -> QComboBox* {
            for (QComboBox* c : bar.findChildren<QComboBox*>())
                if (c->toolTip() == tip && c->isVisibleTo(&bar))
                    return c;
            return nullptr;
        };
        QComboBox* presets = combo(QStringLiteral("Voreinstellungen"));
        QVERIFY(presets);
        // Vorlage "Innenkante": rundum nach innen, Multiplizieren
        const int edge = presets->findText(QStringLiteral("Innenkante"));
        QVERIFY(edge > 0);
        emit presets->activated(edge);
        QApplication::processEvents();
        QVERIFY(obj()->shadow().active && obj()->shadow().inner);
        QCOMPARE(obj()->shadow().blend, BlendMode::Multiply);
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Innenschatten"));
        // Innen an der Kante dunkler, in der Mitte und aussen unverändert
        const QColor inside = pixel(QPointF(40.4, 55)), middle = pixel(QPointF(55, 55)), outside = pixel(QPointF(39.4, 55));
        QVERIFY2(inside.green() < 215 && inside.blue() < 10, qPrintable(inside.name()));
        QVERIFY2(middle.green() >= 250 && middle.red() >= 250, qPrintable(middle.name()));
        QCOMPARE(outside, QColor(Qt::white));
        screenshot(QStringLiteral("innenschatten-kante"));
        // Art in der Leiste: Innen
        QComboBox* kind = combo(QStringLiteral("Schattenart"));
        QVERIFY(kind);
        QCOMPARE(kind->currentIndex(), 1);
        // "Innen oben links": Schatten an der oberen/linken Innenkante, rechts unten hell
        emit presets->activated(presets->findText(QStringLiteral("Innen oben links")));
        QVERIFY(obj()->shadow().offset.x() > 0 && obj()->shadow().offset.y() > 0);
        const QColor topLeft = pixel(QPointF(41, 55)), bottomRight = pixel(QPointF(69, 55));
        QVERIFY2(topLeft.green() + 30 < bottomRight.green(), qPrintable(topLeft.name() + QLatin1Char(' ') + bottomRight.name()));
        // Speichern/Laden
        auto back = serial::fromJson(serial::toJson(*obj()), serial::Ids::Keep);
        QCOMPARE(node_cast<DrawObject>(back.get())->shadow(), obj()->shadow());
        // Hinterlegter Schatten mit Mischmodus "Negativ multiplizieren": weisser Schatten auf
        // Schwarz hellt auf, Normal mit Schwarz dunkelt ab
        emit kind->activated(0);
        QVERIFY(!obj()->shadow().inner);
        DropShadow d = obj()->shadow();
        d.offset = QPointF(5, 5);
        d.feather = 0;
        d.opacity = 100;
        d.color = Color(0, 0, 255);
        m_editor->setSelectionShadow(d, QStringLiteral("Schatten"));
        QApplication::processEvents();
        QComboBox* mode = combo(QStringLiteral("Mischmodus des Schattens"));
        QVERIFY(mode);
        QCOMPARE(mode->currentData().toInt(), int(BlendMode::Multiply));
        // Multiplizieren über Weiss: Blau
        QVERIFY2(pixel(QPointF(73, 73)) == QColor(0, 0, 255), qPrintable(pixel(QPointF(73, 73)).name()));
        emit mode->activated(mode->findData(int(BlendMode::Screen)));
        QCOMPARE(obj()->shadow().blend, BlendMode::Screen);
        // Negativ multiplizieren über Weiss bleibt Weiss
        QVERIFY2(pixel(QPointF(73, 73)) == QColor(Qt::white), qPrintable(pixel(QPointF(73, 73)).name()));
        m_canvas->setTool(ToolId::Pick);
    }

    void extrusionRotation()
    {
        m_canvas->resize(800, 600);
        auto o = std::make_unique<RectangleObject>(QSizeF(30, 20));
        o->setTransform(QTransform::fromTranslate(60, 60));
        Style s;
        s.fill = Fill::solid(Color(0, 0, 255));
        s.stroke.reset();
        o->setStyle(s);
        const NodeId id = m_editor->addObject(std::move(o), QStringLiteral("R"));
        auto obj = [this, id] { return m_editor->document().findAs<DrawObject>(id); };
        Extrusion e;
        e.active = true;
        e.vanish = QPointF(40, -40);
        e.depth = 30;
        m_editor->setObjectExtrusion(id, e, QStringLiteral("Extrusion"));
        const QList<Canvas::ExtrusionFace> flat = Canvas::extrusionModel(*obj(), obj()->extrusion());
        auto pixel = [this](const QPointF& doc) {
            QImage img(3, 3, QImage::Format_ARGB32_Premultiplied);
            img.fill(Qt::white);
            QPainter p(&img);
            m_canvas->renderPage(p, 0, 8.0, QPointF(1.5, 1.5) - doc * 8.0, true);
            p.end();
            return img.pixelColor(1, 1);
        };
        QCOMPARE(pixel(QPointF(85, 78)), QColor(0, 0, 255));
        PropertyBar bar(*m_editor, *m_canvas);
        bar.show();
        m_editor->setSelection({id});
        m_canvas->setTool(ToolId::Extrude);
        QApplication::processEvents();
        QDoubleSpinBox* ry = nullptr;
        for (QDoubleSpinBox* sp : bar.findChildren<QDoubleSpinBox*>())
            if (sp->toolTip().startsWith(QStringLiteral("Drehung um die senkrechte Achse")) && sp->isVisibleTo(&bar))
                ry = sp;
        QVERIFY(ry);
        ry->setValue(60);
        emit ry->editingFinished();
        QCOMPARE(obj()->extrusion().rotY, 60.0);
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Extrusion drehen"));
        screenshot(QStringLiteral("extrusion-gedreht"));
        // Geometrie: andere Flächen, die rechte Seite der Vorderfläche weicht nach hinten
        const QList<Canvas::ExtrusionFace> turned = Canvas::extrusionModel(*obj(), obj()->extrusion());
        QVERIFY(!turned.isEmpty());
        QVERIFY(turned.first().polygon != flat.first().polygon);
        const QTransform face = obj()->extrusion().faceTransform(obj()->boundingRect().center());
        QVERIFY(QLineF(face.map(QPointF(90, 70)), QPointF(90, 70)).length() > 5);
        QVERIFY(QLineF(face.map(QPointF(75, 70)), QPointF(75, 70)).length() < 1e-6); // die Mitte bleibt
        // Gezeichnet: rechts unten kein Blau der Vorderfläche mehr, die Mitte bleibt blau
        QVERIFY2(pixel(QPointF(85, 78)) != QColor(0, 0, 255), qPrintable(pixel(QPointF(85, 78)).name()));
        QCOMPARE(pixel(QPointF(72, 70)), QColor(0, 0, 255));
        // Kugel: nach oben ziehen kippt um x
        ExtrusionTrackball* ball = nullptr;
        for (ExtrusionTrackball* b : bar.findChildren<ExtrusionTrackball*>())
            if (b->isVisibleTo(&bar))
                ball = b;
        QVERIFY(ball);
        QCOMPARE(ball->rotY(), 60.0);
        const QPoint c = ball->rect().center();
        QTest::mousePress(ball, Qt::LeftButton, {}, c);
        QTest::mouseMove(ball, c - QPoint(0, 5));
        QTest::mouseRelease(ball, Qt::LeftButton, {}, c - QPoint(0, 10));
        QCOMPARE(obj()->extrusion().rotX, 10 * ExtrusionTrackball::kDegreesPerPixel);
        QCOMPARE(obj()->extrusion().rotY, 60.0);
        screenshot(QStringLiteral("extrusion-kugel"));
        // Speichern/Laden
        auto back = serial::fromJson(serial::toJson(*obj()), serial::Ids::Keep);
        QCOMPARE(node_cast<DrawObject>(back.get())->extrusion(), obj()->extrusion());
        // Trennen: die Vorderfläche wird mitgedreht (perspektivisch als Kurve)
        m_editor->setSelection({id});
        QPolygonF corners;
        for (const QPointF& q : {QPointF(60, 60), QPointF(90, 60), QPointF(90, 80), QPointF(60, 80)})
            corners << obj()->extrusion().project(obj()->boundingRect().center(), q, 0.0);
        const QRectF expected = corners.boundingRect();
        QVERIFY(m_editor->separateEffects());
        const auto* group = m_editor->document().findAs<GroupObject>(m_editor->selection().value(0));
        QVERIFY(group);
        bool foundFace = false;
        for (int i = 0; i < group->childCount(); ++i) {
            const auto* d = node_cast<DrawObject>(group->child(i));
            if (d && d->style().fill == Fill::solid(Color(0, 0, 255))) {
                foundFace = true;
                QVERIFY2(near(d->boundingRect(), expected, 0.3), qPrintable(describe(d->boundingRect()) + describe(expected)));
                QVERIFY(!d->extrusion().active);
            }
        }
        QVERIFY(foundFace);
        // Rückgängig bis vor die Drehung
        m_editor->document().undo();
        m_editor->document().undo();
        m_editor->document().undo();
        QVERIFY(!obj()->extrusion().isRotated());
        m_canvas->setTool(ToolId::Pick);
    }

    // Leisten und Beispiele für Mischmodus, Innenschatten, gedrehte Extrusion, nur mit VEK_SCREENSHOT_DIR
    void renderFeaturesScreenshot()
    {
        const QString dir = qEnvironmentVariable("VEK_SCREENSHOT_DIR");
        if (dir.isEmpty())
            QSKIP("nur mit VEK_SCREENSHOT_DIR");
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        MainWindow window;
        window.resize(1600, 900);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        Canvas* canvas = window.canvas();
        Editor& ed = canvas->editor();
        auto add = [&ed](std::unique_ptr<DrawObject> o, const QPointF& at, const Color& fill) {
            o->setTransform(QTransform::fromTranslate(at.x(), at.y()) * o->transform());
            Style s;
            s.fill = Fill::solid(fill);
            s.stroke.reset();
            o->setStyle(s);
            return ed.addObject(std::move(o), QStringLiteral("O"));
        };
        // Mischmodi: drei Kreise über einem Verlauf
        auto band = std::make_unique<RectangleObject>(QSizeF(180, 50));
        const NodeId bandId = add(std::move(band), QPointF(15, 15), Color(255, 200, 0));
        ed.setSelection({bandId});
        Fill grad = Fill::solid(Color(0, 0, 0));
        grad.type = Fill::Type::Linear;
        grad.stops = {GradientStop{0.0, Color(255, 230, 0)}, GradientStop{1.0, Color(30, 90, 200)}};
        ed.setSelectionFill(grad);
        const BlendMode modes[] = {BlendMode::Multiply, BlendMode::Screen, BlendMode::Overlay, BlendMode::Difference, BlendMode::Hue};
        NodeId firstCircle = 0;
        for (int i = 0; i < 5; ++i) {
            const NodeId c = add(std::make_unique<EllipseObject>(QSizeF(30, 30)), QPointF(22 + i * 35, 25), Color(220, 30, 60));
            if (i == 0)
                firstCircle = c;
            ed.setSelection({c});
            ed.setSelectionBlendMode(modes[i]);
        }
        // Innenschatten auf Text
        auto text = std::make_unique<TextObject>(TextObject::Kind::Artistic);
        text->setPlainText(QStringLiteral("DSN"), CharFormat(QStringLiteral("Arial Black"), 120));
        const NodeId t = add(std::move(text), QPointF(20, 110), Color(240, 120, 40));
        DropShadow inner;
        inner.active = true;
        inner.inner = true;
        inner.offset = QPointF(0, 0);
        inner.feather = 2.5;
        inner.opacity = 70;
        ed.setObjectShadow(t, inner, QStringLiteral("Innenschatten"));
        // Gedrehte Extrusion
        const NodeId box = add(std::make_unique<RectangleObject>(QSizeF(50, 35)), QPointF(120, 200), Color(40, 120, 220));
        Extrusion e;
        e.active = true;
        e.vanish = QPointF(60, -60);
        e.depth = 25;
        e.color = Color(120, 160, 220);
        e.rotX = 25;
        e.rotY = -35;
        ed.setObjectExtrusion(box, e, QStringLiteral("Extrusion"));
        auto shot = [&](ToolId tool, NodeId sel, const QString& name) {
            canvas->setTool(tool);
            ed.setSelection({sel});
            QApplication::processEvents();
            window.grab(QRect(0, 0, 1600, 130)).save(dir + QLatin1Char('/') + name + QStringLiteral(".png"));
        };
        shot(ToolId::DropShadow, t, QStringLiteral("render-leiste-schatten"));
        shot(ToolId::Extrude, box, QStringLiteral("render-leiste-extrusion"));
        shot(ToolId::Transparency, firstCircle, QStringLiteral("render-leiste-transparenz"));
        ed.clearSelection();
        canvas->setTool(ToolId::Pick);
        QApplication::processEvents();
        output::ExportOptions o;
        o.dpi = 150;
        output::renderImage(*canvas, o).save(dir + QStringLiteral("/render-beispiele.png"));
        // Dasselbe als SVG (mix-blend-mode) und zurück gerastert
        QFile svg(dir + QStringLiteral("/render-beispiele.svg"));
        QVERIFY(svg.open(QIODevice::WriteOnly));
        svg.write(output::areaSvg(*canvas, o));
    }

    // --- Smart-Füllung mit offenen Linien, Schnittmenge mit offenen Quellen, glatter Zickzack ---
private:
    NodeId addOpenLine(const QList<QPointF>& points, bool curve = false)
    {
        SubPath sp;
        for (int i = 0; i < points.size(); ++i) {
            CurveNode n{points[i]};
            if (curve && i > 0) {
                // weiche Kurve: Steuerpunkte senkrecht versetzt
                const QPointF d = points[i] - points[i - 1];
                n.curveIn = true;
                n.c1 = points[i - 1] + d / 3.0 + QPointF(-d.y(), d.x()) * 0.3;
                n.c2 = points[i - 1] + d * (2.0 / 3.0) - QPointF(-d.y(), d.x()) * 0.3;
            }
            sp.nodes << n;
        }
        auto c = std::make_unique<CurveObject>(QList<SubPath>{sp});
        Style s;
        s.fill = Fill();
        c->setStyle(s);
        return m_editor->addObject(std::move(c), QStringLiteral("Linie"));
    }
    static double filledArea(const DrawObject* o)
    {
        double sum = 0;
        for (const QPolygonF& poly : vek::pathops::flattenFine(o->path(), 0.05)) {
            double a = 0;
            for (int i = 0; i < poly.size(); ++i)
                a += poly[i].x() * poly[(i + 1) % poly.size()].y() - poly[(i + 1) % poly.size()].x() * poly[i].y();
            sum += a / 2;
        }
        return std::abs(sum);
    }

private slots:
    void smartFillWithOpenLines()
    {
        m_canvas->resize(800, 600);
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        Options::instance().resetWorkspace();
        // Windrad: Kreis um (40, 40) mit r = 20, zwei offene Linien kreuz und quer darüber
        auto e = std::make_unique<EllipseObject>(QSizeF(40, 40));
        e->setTransform(QTransform::fromTranslate(20, 20));
        const NodeId circle = m_editor->addObject(std::move(e), QStringLiteral("E"));
        addOpenLine({QPointF(15, 40), QPointF(65, 40)});
        const NodeId vertical = addOpenLine({QPointF(40, 15), QPointF(40, 65)});
        // Senkrechte Linie in einer Gruppe: Gruppen zählen mit
        m_editor->setSelection({vertical});
        m_editor->groupSelection();
        const int before = int(m_editor->pickableObjects().size());
        m_canvas->setTool(ToolId::SmartFill);
        move(screen(QPointF(50, 30)));
        auto* tool = static_cast<SmartFillTool*>(m_canvas->currentTool());
        QVERIFY(!tool->regionAt(QPointF(50, 30)).isEmpty());
        click(screen(QPointF(50, 30)));
        QCOMPARE(int(m_editor->pickableObjects().size()), before + 1);
        const auto* slice = node_cast<CurveObject>(m_editor->document().findAs<DrawObject>(m_editor->selection().value(0)));
        QVERIFY(slice);
        QVERIFY2(std::abs(filledArea(slice) - M_PI * 400 / 4) < 1.0, qPrintable(QString::number(filledArea(slice))));
        QVERIFY2(near(slice->boundingRect(), QRectF(40, 20, 20, 20), 0.05), qPrintable(describe(slice->boundingRect())));
        QCOMPARE(slice->style().fill, Fill::solid(Color(255, 210, 77)));
        // Zuoberst, das Original bleibt
        QCOMPARE(m_editor->pickableObjects().last()->id(), slice->id());
        QVERIFY(m_editor->document().findAs<DrawObject>(circle));
        // Klick ausserhalb jeder Masche (zwischen den Stummeln draussen): nichts
        click(screen(QPointF(18, 30)));
        QCOMPARE(int(m_editor->pickableObjects().size()), before + 1);

        // Rechteck, von einer offenen Kurve in zwei Felder geteilt; Kontur "Angeben"
        Options::instance().set(QStringLiteral("ws/tools/smartFill/outlineMode"), 1);
        Options::instance().set(QStringLiteral("ws/tools/smartFill/outlineWidth"), 0.5);
        Options::instance().set(QStringLiteral("ws/tools/smartFill/outlineColor"), QStringLiteral("#0000FF"));
        Options::instance().set(QStringLiteral("ws/tools/smartFill/fill"), QStringLiteral("#FF0000"));
        m_canvas->setTool(ToolId::Pick);
        auto r = std::make_unique<RectangleObject>(QSizeF(100, 60));
        r->setTransform(QTransform::fromTranslate(100, 20));
        m_editor->addObject(std::move(r), QStringLiteral("R"));
        // Kurve beginnt knapp (0,1 mm) neben dem Rand: Lücke wird geschlossen
        addOpenLine({QPointF(100.1, 50), QPointF(150, 40), QPointF(200, 50)}, true);
        m_canvas->setTool(ToolId::SmartFill);
        click(screen(QPointF(180, 25)));
        const auto* panel = node_cast<CurveObject>(m_editor->document().findAs<DrawObject>(m_editor->selection().value(0)));
        QVERIFY(panel && panel->id() != slice->id());
        const double a = filledArea(panel);
        QVERIFY2(a > 1000 && a < 5000, qPrintable(QString::number(a)));
        QVERIFY(panel->boundingRect().bottom() < 70);
        QVERIFY(std::any_of(panel->subPaths().first().nodes.begin(), panel->subPaths().first().nodes.end(),
                            [](const CurveNode& n) { return n.curveIn; }));
        QCOMPARE(panel->style().fill, Fill::solid(Color(255, 0, 0)));
        QCOMPARE(panel->style().stroke, std::optional<Color>(Color(0, 0, 255)));
        QCOMPARE(panel->style().strokeWidth, 0.5);
        // Das untere Feld ergänzt das obere zum Rechteck
        click(screen(QPointF(180, 75)));
        const auto* lower = m_editor->document().findAs<DrawObject>(m_editor->selection().value(0));
        QVERIFY(lower && lower->id() != panel->id());
        QVERIFY2(std::abs(filledArea(lower) + a - 6000) < 2.0, qPrintable(QString::number(filledArea(lower) + a)));
        screenshot(QStringLiteral("smartfill-open-lines"));
        // Ohne Füllung und ohne Kontur
        Options::instance().set(QStringLiteral("ws/tools/smartFill/fillMode"), 2);
        Options::instance().set(QStringLiteral("ws/tools/smartFill/outlineMode"), 2);
        const Style none = SmartFillTool::fillStyle(Style());
        QVERIFY(none.fill.type == Fill::Type::None && !none.stroke);
        Options::instance().resetWorkspace();
        m_canvas->setTool(ToolId::Pick);
    }

    void smartFillPropertyBar()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        Options::instance().resetWorkspace();
        PropertyBar bar(*m_editor, *m_canvas);
        m_canvas->setTool(ToolId::SmartFill);
        auto* fillMode = bar.findChild<QComboBox*>(QStringLiteral("fillMode"));
        auto* outlineMode = bar.findChild<QComboBox*>(QStringLiteral("outlineMode"));
        QVERIFY(fillMode && outlineMode);
        QCOMPARE(fillMode->currentIndex(), 1);
        QCOMPARE(outlineMode->count(), 3);
        // Kontur auf "Keine" stellen
        outlineMode->setCurrentIndex(2);
        emit outlineMode->activated(2);
        QCOMPARE(Options::instance().integer(QStringLiteral("ws/tools/smartFill/outlineMode")), 2);
        QVERIFY(!SmartFillTool::fillStyle(Style()).stroke);
        m_canvas->setTool(ToolId::Pick);
        Options::instance().resetWorkspace();
    }

    void intersectWithOpenSourceLines()
    {
        // Linienschar (Quelle, zuerst gewählt) mit einem Rechteck (Ziel) schneiden: es bleiben die
        // Linienstücke im Rechteck, als offene Linien
        auto r = std::make_unique<RectangleObject>(QSizeF(40, 40));
        r->setTransform(QTransform::fromTranslate(30, 30));
        const NodeId rect = m_editor->addObject(std::move(r), QStringLiteral("R"));
        QList<NodeId> lines;
        for (int k = 0; k < 3; ++k)
            lines << addOpenLine({QPointF(10, 40 + k * 10), QPointF(90, 40 + k * 10)});
        m_editor->setSelection(lines);
        m_editor->groupSelection();
        const NodeId group = m_editor->selection().value(0);
        const int before = int(m_editor->pickableObjects().size());
        m_editor->setSelection({group, rect});
        QVERIFY(m_editor->shapeSelection(Editor::ShapeOp::Intersect));
        QCOMPARE(int(m_editor->pickableObjects().size()), before + 1);
        const auto* cut = node_cast<CurveObject>(m_editor->document().findAs<DrawObject>(m_editor->selection().value(0)));
        QVERIFY(cut);
        QCOMPARE(int(cut->subPaths().size()), 3);
        for (const SubPath& sp : cut->subPaths())
            QVERIFY(!sp.closed);
        QVERIFY2(near(cut->boundingRect(), QRectF(30, 40, 40, 20), 0.05), qPrintable(describe(cut->boundingRect())));
        m_editor->document().undo();
        // Umgekehrte Reihenfolge (Linien als Ziel) wie bisher
        m_editor->setSelection({rect, group});
        QVERIFY(m_editor->shapeSelection(Editor::ShapeOp::Intersect));
        const auto* cut2 = m_editor->document().findAs<DrawObject>(m_editor->selection().value(0));
        QVERIFY(cut2 && near(cut2->boundingRect(), QRectF(30, 40, 40, 20), 0.05));
    }

    void distortZipperSmoothRandomLocal()
    {
        m_canvas->resize(800, 600);
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        Options::instance().resetWorkspace();
        const QList<SubPath> base{SubPath{{CurveNode{QPointF(0, 50)}, CurveNode{QPointF(100, 50)}}, false}};
        // Spitz: nur Geraden
        const auto sharp = DistortTool::distort(base, DistortTool::Mode::Zipper, 5, 10, QPointF(50, 50));
        QVERIFY(std::none_of(sharp.first().nodes.begin(), sharp.first().nodes.end(), [](const CurveNode& n) { return n.curveIn; }));
        // Glatt: dieselben Spitzen, aber Bézierwellen mit Tangenten parallel zur Linie
        DistortTool::ZipperOptions smooth;
        smooth.smooth = true;
        const auto wave = DistortTool::distort(base, DistortTool::Mode::Zipper, 5, 10, QPointF(50, 50), smooth);
        QCOMPARE(wave.first().nodes.size(), sharp.first().nodes.size());
        for (int i = 1; i < wave.first().nodes.size(); ++i) {
            const CurveNode& n = wave.first().nodes[i];
            QVERIFY(n.curveIn && n.kind == NodeKind::Smooth);
            QVERIFY(std::abs(n.c2.y() - n.point.y()) < 1e-9); // waagrechte Tangente auf der Spitze
            QCOMPARE(n.point, sharp.first().nodes[i].point);
        }
        // Zufällig: Amplituden verschieden, aber wiederholbar
        DistortTool::ZipperOptions random;
        random.random = true;
        const auto r1 = DistortTool::distort(base, DistortTool::Mode::Zipper, 5, 10, QPointF(50, 50), random);
        const auto r2 = DistortTool::distort(base, DistortTool::Mode::Zipper, 5, 10, QPointF(50, 50), random);
        QCOMPARE(r1.first().nodes, r2.first().nodes);
        QSet<int> amplitudes;
        for (const CurveNode& n : r1.first().nodes) {
            QVERIFY(std::abs(n.point.y() - 50) <= 5 + 1e-9);
            amplitudes << int(std::round(std::abs(n.point.y() - 50) * 100));
        }
        QVERIFY(amplitudes.size() > 3);
        // Lokal: an der Mitte voll, zu den Enden hin abnehmend
        DistortTool::ZipperOptions local;
        local.local = true;
        const auto l = DistortTool::distort(base, DistortTool::Mode::Zipper, 5, 10, QPointF(50, 50), local);
        const QList<CurveNode>& ln = l.first().nodes;
        QVERIFY(std::abs(ln.first().point.y() - 50) < 0.01 && std::abs(ln.last().point.y() - 50) < 0.01);
        QVERIFY(std::abs(ln[5].point.y() - 50) > 4.9);
        // Kreis (4 Bézier-Segmente): Frequenz 2 ergibt 8 Zacken, nicht eine je abgeflachtem Stück
        QPainterPath circlePath;
        circlePath.addEllipse(QPointF(50, 50), 20, 20);
        const QList<SubPath> circle = CurveObject::fromPath(circlePath)->subPaths();
        const int segs = int(circle.first().nodes.size());
        const auto zig = DistortTool::distort(circle, DistortTool::Mode::Zipper, 3, 2, QPointF(50, 50));
        QCOMPARE(zig.first().nodes.size(), segs * 2);
        for (const CurveNode& n : zig.first().nodes) {
            const double r = QLineF(QPointF(50, 50), n.point).length();
            QVERIFY(std::abs(r - 20.0) > 2.9 && std::abs(r - 20.0) < 3.1); // Spitzen abwechselnd innen/aussen
        }

        // Mit der Maus: Linie wählen, Zickzack + Glatt in der Eigenschaftsleiste, vom Objekt wegziehen
        const NodeId line = addOpenLine({QPointF(20, 60), QPointF(120, 60)});
        Options::instance().set(QStringLiteral("ws/tools/distort/mode"), 1);
        PropertyBar bar(*m_editor, *m_canvas);
        m_canvas->setTool(ToolId::Distort);
        auto* smoothButton = bar.findChild<QToolButton*>(QStringLiteral("distort-smooth"));
        QVERIFY(smoothButton && bar.findChild<QToolButton*>(QStringLiteral("distort-random"))
                && bar.findChild<QToolButton*>(QStringLiteral("distort-local")));
        smoothButton->click();
        QVERIFY(Options::instance().flag(QStringLiteral("ws/tools/distort/smooth")));
        drag(screen(QPointF(70, 60)), screen(QPointF(70, 85)));
        const auto* distorted = node_cast<CurveObject>(m_editor->document().findAs<DrawObject>(line));
        QVERIFY(distorted);
        const QList<CurveNode>& dn = distorted->subPaths().first().nodes;
        QVERIFY(dn.size() > 5);
        QVERIFY(std::all_of(dn.begin() + 1, dn.end(), [](const CurveNode& n) { return n.curveIn; }));
        QVERIFY2(distorted->boundingRect().height() > 5, qPrintable(describe(distorted->boundingRect())));
        screenshot(QStringLiteral("distort-zipper-smooth"));
        m_canvas->setTool(ToolId::Pick);
        Options::instance().resetWorkspace();
    }

    // Seitenbefehle: mehrere einfügen, duplizieren (mit/ohne Inhalt), verschieben, Ausrichtung,
    // umbenennen; je ein Rückgängig-Schritt
    void pageCommandsUndoable()
    {
        Editor& ed = *m_editor;
        Document& doc = ed.document();
        auto rect = std::make_unique<RectangleObject>(QSizeF(30, 20));
        rect->setTransform(QTransform::fromTranslate(10, 10));
        const NodeId original = ed.addObject(std::move(rect), QStringLiteral("R"));
        const NodeId first = doc.page(0).id();

        ed.insertPages(1, 3);
        QCOMPARE(doc.pageCount(), 4);
        QCOMPARE(ed.currentPageIndex(), 1);
        QCOMPARE(doc.undoText(), QStringLiteral("Seiten einfügen"));
        doc.undo();
        QCOMPARE(doc.pageCount(), 1);
        doc.redo();

        // Duplizieren mit Inhalt: neue Seite danach, Objekt kopiert (neue Id)
        QCOMPARE(ed.duplicatePage(0, true, true), 1);
        QCOMPARE(doc.pageCount(), 5);
        QCOMPARE(ed.currentPageIndex(), 1);
        const Page& copy = doc.page(1);
        QCOMPARE(copy.childCount(), doc.page(0).childCount());
        QCOMPARE(copy.child(0)->childCount(), 1);
        QVERIFY(copy.child(0)->child(0)->id() != original);
        QVERIFY(near(node_cast<DrawObject>(copy.child(0)->child(0))->boundingRect(), QRectF(10, 10, 30, 20)));
        // Nur Ebenen, davor
        QCOMPARE(ed.duplicatePage(0, false, false), 0);
        QCOMPARE(doc.page(0).child(0)->childCount(), 0);
        QCOMPARE(doc.page(1).id(), first);
        doc.undo();
        doc.undo();
        QCOMPARE(doc.pageCount(), 4);

        // Verschieben: aktuelle Seite folgt ihrer Seite
        ed.setCurrentPageIndex(0);
        ed.movePage(0, 3);
        QCOMPARE(doc.page(3).id(), first);
        QCOMPARE(ed.currentPageIndex(), 3);
        QCOMPARE(doc.undoText(), QStringLiteral("Seite verschieben"));
        doc.undo();
        QCOMPARE(doc.page(0).id(), first);

        // Ausrichtung nur dieser Seite
        ed.setPageOrientation(2, true);
        QCOMPARE(doc.page(2).size(), QSizeF(297, 210));
        QCOMPARE(doc.page(1).size(), QSizeF(210, 297));
        QVERIFY(ed.isPageLandscape(2));
        doc.undo();
        QCOMPARE(doc.page(2).size(), QSizeF(210, 297));

        ed.renamePage(1, QStringLiteral("Über uns"));
        QCOMPARE(doc.page(1).name(), QStringLiteral("Über uns"));
        QCOMPARE(doc.undoText(), QStringLiteral("Seite umbenennen"));
        doc.undo();
        QVERIFY(doc.page(1).name().isEmpty());

        // Seitengrösse ziehen: Objekte bleiben an ihrer Stelle (Seite wächst nach links/oben)
        ed.setCurrentPageIndex(0);
        ed.resizePage(0, QRectF(-20, -10, 250, 320));
        QCOMPARE(doc.page(0).size(), QSizeF(250, 320));
        QVERIFY(near(doc.findAs<DrawObject>(original)->boundingRect(), QRectF(30, 20, 30, 20)));
        doc.undo();
        QCOMPARE(doc.page(0).size(), QSizeF(210, 297));
        QVERIFY(near(doc.findAs<DrawObject>(original)->boundingRect(), QRectF(10, 10, 30, 20)));
    }

    // Seite über das Kontextmenü des Seitenregisters umbenennen; Register per Ziehen umordnen
    void pageTabsRenameAndReorder()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        MainWindow window;
        window.resize(1300, 900);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        Editor& ed = window.canvas()->editor();
        Document& doc = ed.document();
        ed.insertPages(1, 2);
        ed.setCurrentPageIndex(0);
        QTabBar* tabs = nullptr;
        for (QTabBar* bar : window.findChildren<QTabBar*>()) {
            if (bar->shape() == QTabBar::RoundedSouth)
                tabs = bar;
        }
        QVERIFY(tabs);
        QCOMPARE(tabs->count(), 3);

        // Rechtsklick auf Register 2, "Seite umbenennen …", Namen eingeben (Feld mit "Seite 2" vorbelegt)
        QStringList texts;
        QString prefill;
        QTimer poll;
        poll.setInterval(20);
        int stage = 0;
        QObject::connect(&poll, &QTimer::timeout, [&] {
            if (stage == 0) {
                auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
                if (!menu || !menu->isVisible())
                    return;
                for (QAction* a : menu->actions()) {
                    texts << a->text();
                    if (a->text() == QStringLiteral("Seite &umbenennen …"))
                        menu->setActiveAction(a);
                }
                // Erst nach diesem Aufruf auslösen: der Befehl öffnet einen Dialog, und dieser
                // Zeitgeber muss ihn bedienen können
                QTimer::singleShot(0, menu, [menu] { QTest::keyClick(menu, Qt::Key_Return); });
                stage = 1;
            } else if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) {
                auto* edit = dialog->findChild<QLineEdit*>();
                if (!edit)
                    return;
                prefill = edit->text();
                edit->setText(QStringLiteral("Über uns"));
                QTest::keyClick(edit, Qt::Key_Return);
                poll.stop();
            }
        });
        poll.start();
        // Kontextmenü-Ereignis wie bei einem Rechtsklick (Linux löst es beim Drücken aus, Windows beim
        // Loslassen; direkt geschickt ist der Test auf beiden Systemen gleich)
        const QPoint tab2 = tabs->tabRect(1).center();
        QContextMenuEvent menuEvent(QContextMenuEvent::Mouse, tab2, tabs->mapToGlobal(tab2));
        QApplication::sendEvent(tabs, &menuEvent);
        QTRY_VERIFY2(doc.page(1).name() == QStringLiteral("Über uns"), qPrintable(QStringLiteral("stage %1: ").arg(stage) + texts.join(QLatin1Char('|'))));
        QVERIFY2(texts.contains(QStringLiteral("Seite d&uplizieren …")), qPrintable(texts.join(QLatin1Char('|'))));
        QCOMPARE(prefill, QStringLiteral("Seite 2"));
        QVERIFY(texts.contains(QStringLiteral("Seitenausrichtung &wechseln")));
        QCOMPARE(tabs->tabText(1), QStringLiteral("Über uns"));

        // Register 1 hinter Register 3 ziehen
        const NodeId first = doc.page(0).id();
        const NodeId second = doc.page(1).id();
        const int undoBefore = doc.undoCount();
        const QPoint from = tabs->tabRect(0).center();
        const QPoint to = tabs->tabRect(2).center() + QPoint(tabs->tabRect(2).width() / 3, 0);
        QTest::mousePress(tabs, Qt::LeftButton, {}, from);
        for (int i = 1; i <= 12; ++i)
            QTest::mouseMove(tabs, from + (to - from) * i / 12);
        QTest::mouseRelease(tabs, Qt::LeftButton, {}, to);
        QTRY_COMPARE(doc.page(2).id(), first);
        QCOMPARE(doc.page(0).id(), second);
        QCOMPARE(doc.undoCount(), undoBefore + 1);
        QTRY_COMPARE(tabs->tabText(0), QStringLiteral("Über uns"));
        QCOMPARE(tabs->tabText(2), QStringLiteral("Seite 3"));
        QCOMPARE(ed.currentPageIndex(), 2); // die gezogene, aktuelle Seite bleibt aktuell
        doc.undo();
        QCOMPARE(doc.page(0).id(), first);
        QTRY_COMPARE(tabs->tabText(1), QStringLiteral("Über uns"));
    }

    // Andockfenster "Seiten": Miniaturen, Suche, Rasteransicht, Ziehen zum Umordnen, Knöpfe
    void pagesDocker()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        MainWindow window;
        window.resize(1400, 900);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        Editor& ed = window.canvas()->editor();
        Document& doc = ed.document();
        ed.insertPages(1, 3);
        ed.renamePage(0, QStringLiteral("Titel"));
        ed.renamePage(3, QStringLiteral("Rückseite"));
        ed.setCurrentPageIndex(0);
        auto rect = std::make_unique<RectangleObject>(QSizeF(100, 100));
        Style red;
        red.fill = Fill::solid(Color(255, 0, 0));
        rect->setStyle(red);
        rect->setTransform(QTransform::fromTranslate(50, 50));
        ed.addObject(std::move(rect), QStringLiteral("R"));

        auto* dock = window.findChild<QDockWidget*>(QStringLiteral("pages"));
        QVERIFY(dock);
        QVERIFY(dock->toggleViewAction()->text().contains(QStringLiteral("Seiten")));
        dock->toggleViewAction()->trigger(); // Fenster > Andockfenster > Seiten
        dock->raise();
        QTRY_VERIFY(dock->isVisible());
        PagesDocker* docker = window.pagesDocker();
        QVERIFY(docker);
        PageThumbnailList* list = docker->list();
        docker->updateThumbnails();
        QCOMPARE(list->count(), 4);
        QCOMPARE(docker->thumbnailCount(), doc.pageCount());
        QCOMPARE(list->item(0)->text(), QStringLiteral("1: Titel"));
        QCOMPARE(list->item(1)->text(), QStringLiteral("Seite 2"));
        // Miniatur zeigt den Inhalt: rote Fläche auf Seite 1
        const QImage thumb = list->item(0)->icon().pixmap(docker->sizeSlider()->value()).toImage();
        bool hasRed = false;
        for (int y = 0; y < thumb.height() && !hasRed; ++y)
            for (int x = 0; x < thumb.width() && !hasRed; ++x)
                hasRed = qRed(thumb.pixel(x, y)) > 200 && qGreen(thumb.pixel(x, y)) < 60 && qAlpha(thumb.pixel(x, y)) > 200;
        QVERIFY(hasRed);
        // Neue Seite über den Knopf: eine Miniatur mehr
        auto* add = docker->findChild<QToolButton*>(QStringLiteral("addPage"));
        QVERIFY(add);
        QTest::mouseClick(add, Qt::LeftButton);
        QCOMPARE(doc.pageCount(), 5);
        QCOMPARE(ed.currentPageIndex(), 1);
        docker->updateThumbnails();
        QCOMPARE(docker->thumbnailCount(), 5);
        doc.undo();
        docker->updateThumbnails();
        QCOMPARE(list->count(), 4);

        // Suchfeld: nur passende Seiten
        QTest::keyClicks(docker->searchField(), QStringLiteral("ckseite"));
        int visible = 0;
        for (int i = 0; i < list->count(); ++i)
            visible += list->item(i)->isHidden() ? 0 : 1;
        QCOMPARE(visible, 1);
        docker->searchField()->clear();

        // Klick wählt die Seite, Doppelklick ebenso
        QTest::mouseClick(list->viewport(), Qt::LeftButton, {}, list->visualItemRect(list->item(2)).center());
        QCOMPARE(ed.currentPageIndex(), 2);
        QTest::mouseDClick(list->viewport(), Qt::LeftButton, {}, list->visualItemRect(list->item(1)).center());
        QCOMPARE(ed.currentPageIndex(), 1);

        // Seite 1 per Ziehen hinter Seite 3 (Listenansicht)
        const NodeId titel = doc.page(0).id();
        auto drag = [&](int fromRow, int toRow, bool afterTarget) {
            const QRect a = list->visualItemRect(list->item(fromRow));
            const QRect b = list->visualItemRect(list->item(toRow));
            const QPoint start = a.center();
            const QPoint end = docker->gridView() ? QPoint(afterTarget ? b.right() - 3 : b.left() + 3, b.center().y())
                                                  : QPoint(b.center().x(), afterTarget ? b.bottom() - 3 : b.top() + 3);
            QTest::mousePress(list->viewport(), Qt::LeftButton, {}, start);
            for (int i = 1; i <= 8; ++i)
                QTest::mouseMove(list->viewport(), start + (end - start) * i / 8);
            QTest::mouseRelease(list->viewport(), Qt::LeftButton, {}, end);
        };
        drag(0, 2, true);
        QCOMPARE(doc.page(2).id(), titel);
        QCOMPARE(list->item(2)->text(), QStringLiteral("3: Titel"));
        docker->updateThumbnails();
        QCOMPARE(docker->thumbnailCount(), 4);
        doc.undo();
        QCOMPARE(doc.page(0).id(), titel);

        // Rasteransicht über den Knopf, Miniaturgrösse über den Regler; Ziehen geht auch dort
        auto* grid = docker->findChild<QToolButton*>(QStringLiteral("gridView"));
        QTest::mouseClick(grid, Qt::LeftButton);
        QVERIFY(docker->gridView());
        docker->sizeSlider()->setValue(48);
        QCOMPARE(list->iconSize(), QSize(48, 48));
        QApplication::processEvents();
        drag(3, 0, false);
        QCOMPARE(doc.page(0).name(), QStringLiteral("Rückseite"));
        if (!qEnvironmentVariable("VEK_SCREENSHOT_DIR").isEmpty()) {
            docker->updateThumbnails();
            dock->grab().save(qEnvironmentVariable("VEK_SCREENSHOT_DIR") + QStringLiteral("/70-seiten-andockfenster.png"));
        }
        docker->setGridView(false);
    }

    // Mehrseitenansicht: alle Seiten im Zeichenfenster, Objekt von Seite 1 auf Seite 2 ziehen
    void multiPageViewDragBetweenPages()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        MainWindow window;
        window.resize(1500, 900);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        Canvas* canvas = window.canvas();
        Editor& ed = canvas->editor();
        Document& doc = ed.document();
        ed.insertPages(1, 2);
        ed.setCurrentPageIndex(0);
        auto rect = std::make_unique<RectangleObject>(QSizeF(40, 30));
        Style s;
        s.fill = Fill::solid(Color(0, 0, 255));
        rect->setStyle(s);
        rect->setTransform(QTransform::fromTranslate(20, 20));
        const NodeId id = ed.addObject(std::move(rect), QStringLiteral("R"));
        ed.clearSelection();
        canvas->setTool(ToolId::Pick);

        // Ansicht > Mehrseitenansicht
        auto* toggle = window.findChild<QAction*>(QStringLiteral("multiPageView"));
        QVERIFY(toggle);
        toggle->trigger();
        QVERIFY(canvas->multiPageView());
        QApplication::processEvents();
        QCOMPARE(canvas->pageLayout().size(), 3);
        // Alle drei Seiten sichtbar, nebeneinander, je unter dem Zeiger erkannt
        for (int i = 0; i < 3; ++i) {
            const QRectF r = canvas->viewTransform().mapRect(canvas->pageRect(i));
            QVERIFY2(QRectF(canvas->rect()).contains(r), qPrintable(describe(r)));
            QCOMPARE(canvas->pageAt(r.center()), i);
            QVERIFY(!canvas->pageLabelRect(i).isEmpty());
        }
        QVERIFY(canvas->pageRect(1).left() > canvas->pageRect(0).right());
        QCOMPARE(canvas->pageRect(1).top(), canvas->pageRect(0).top());
        // Seite 2 tatsächlich gezeichnet: Mitte weiss, Zwischenraum Arbeitsfläche
        const QImage shot = canvas->grab().toImage();
        const QPoint p2 = canvas->viewTransform().mapRect(canvas->pageRect(1)).center().toPoint();
        QCOMPARE(QColor(shot.pixel(p2 * shot.devicePixelRatio())), QColor(Qt::white));
        if (!qEnvironmentVariable("VEK_SCREENSHOT_DIR").isEmpty())
            window.grab().save(qEnvironmentVariable("VEK_SCREENSHOT_DIR") + QStringLiteral("/71-mehrseitenansicht.png"));

        // Rechteck an dieselbe Stelle auf Seite 2 ziehen
        const QPointF start = canvas->toScreen(QPointF(40, 35));
        const QPointF end = start + (canvas->toScreen(canvas->pageRect(1).topLeft()) - canvas->toScreen(QPointF(0, 0)));
        const int undoBefore = doc.undoCount();
        QTest::mousePress(canvas, Qt::LeftButton, {}, start.toPoint(), 1000);
        for (int i = 1; i <= 10; ++i)
            QTest::mouseMove(canvas, (start + (end - start) * i / 10.0).toPoint());
        QTest::mouseRelease(canvas, Qt::LeftButton, {}, end.toPoint(), 10);
        QCOMPARE(doc.pageOf(id), &doc.page(1));
        QCOMPARE(ed.currentPageIndex(), 1);
        QCOMPARE(ed.selection(), QList<NodeId>{id});
        QCOMPARE(doc.undoCount(), undoBefore + 1); // Ziehen und Seitenwechsel: ein Schritt
        const double tol = canvas->pixelsToDoc(2.0);
        const QRectF moved = doc.findAs<DrawObject>(id)->boundingRect();
        QVERIFY2(near(moved, QRectF(20, 20, 40, 30), tol), qPrintable(describe(moved)));
        // bleibt am Bildschirm, wo es losgelassen wurde
        QVERIFY((canvas->toScreen(moved.center()) - end).manhattanLength() < 4);
        doc.undo();
        QCOMPARE(doc.pageOf(id), &doc.page(0));
        QVERIFY(near(doc.findAs<DrawObject>(id)->boundingRect(), QRectF(20, 20, 40, 30)));

        // Klick auf Seite 3 macht sie aktuell, ohne dass sich die Ansicht verschiebt
        const QRectF page3 = canvas->viewTransform().mapRect(canvas->pageRect(2));
        QTest::mouseClick(canvas, Qt::LeftButton, {}, page3.center().toPoint());
        QCOMPARE(ed.currentPageIndex(), 2);
        QVERIFY(near(canvas->viewTransform().mapRect(canvas->pageRect(2)), page3, 0.5));
        // Ein neues Rechteck auf Seite 3 landet auf Seite 3
        canvas->setTool(ToolId::Rectangle);
        const QPoint a = (page3.topLeft() + QPointF(20, 20)).toPoint();
        QTest::mousePress(canvas, Qt::LeftButton, {}, a, 1000);
        QTest::mouseMove(canvas, a + QPoint(30, 30));
        QTest::mouseRelease(canvas, Qt::LeftButton, {}, a + QPoint(40, 40), 10);
        QCOMPARE(ed.selection().size(), 1);
        QCOMPARE(doc.pageOf(ed.selection().first()), &doc.page(2));

        // Doppelseiten: Bögen ohne Abstand
        Options::instance().set(QStringLiteral("doc/layout/facing"), true);
        QVERIFY(std::abs(canvas->pageRect(2).left() - canvas->pageRect(1).right()) < 1e-6);
        QVERIFY(canvas->pageRect(0).right() <= canvas->pageRect(1).left() + 1e-6);
        Options::instance().set(QStringLiteral("doc/layout/facing"), false);

        // Spaltenzahl 1: untereinander; Alle Seiten einpassen
        Options::instance().set(QStringLiteral("doc/layout/multiColumns"), 1);
        QCOMPARE(canvas->pageRect(1).left(), canvas->pageRect(0).left());
        QVERIFY(canvas->pageRect(1).top() > canvas->pageRect(0).bottom());
        canvas->fitAllPages();
        for (int i = 0; i < 3; ++i)
            QVERIFY(QRectF(canvas->rect()).contains(canvas->viewTransform().mapRect(canvas->pageRect(i))));

        // Zurück zur Einzelseitenansicht
        toggle->trigger();
        QVERIFY(!canvas->multiPageView());
        QCOMPARE(canvas->pageAt(canvas->toScreen(QPointF(10, 10))), ed.currentPageIndex());
    }

    // Mehrseitenansicht: Seitenname anklicken markiert die Seite, Kontextmenü, Grösse ziehen
    void multiPageViewLabelsAndResize()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        MainWindow window;
        window.resize(1500, 900);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        Canvas* canvas = window.canvas();
        Editor& ed = canvas->editor();
        Document& doc = ed.document();
        ed.insertPages(1, 1);
        ed.renamePage(1, QStringLiteral("Flyer"));
        ed.setCurrentPageIndex(0);
        canvas->setMultiPageView(true);
        QApplication::processEvents();
        QCOMPARE(canvas->pageTitle(1), QStringLiteral("2: Flyer"));

        QTest::mouseClick(canvas, Qt::LeftButton, {}, canvas->pageLabelRect(1).center().toPoint());
        QCOMPARE(ed.currentPageIndex(), 1);
        QVERIFY(canvas->pageSelected());
        auto rect = std::make_unique<RectangleObject>(QSizeF(20, 20));
        rect->setTransform(QTransform::fromTranslate(50, 50));
        const NodeId id = ed.addObject(std::move(rect), QStringLiteral("R"));
        ed.clearSelection();
        QVERIFY(canvas->pageSelected());

        // Rechte Kante 20 mm nach rechts, nur diese Seite (Schalter «Aktuelle Seite»)
        ed.setPageSizeForAllPages(false);
        const QRectF page = canvas->viewTransform().mapRect(QRectF(QPointF(0, 0), ed.currentPage().size()));
        const QPointF right(page.right(), page.center().y());
        QCOMPARE(canvas->pageHandleAt(right), 3);
        const double mm = canvas->zoom();
        QTest::mousePress(canvas, Qt::LeftButton, {}, right.toPoint(), 1000);
        QTest::mouseMove(canvas, (right + QPointF(10 * mm, 0)).toPoint());
        QTest::mouseRelease(canvas, Qt::LeftButton, {}, (right + QPointF(20 * mm, 0)).toPoint(), 10);
        const double tol = canvas->pixelsToDoc(1.5);
        QVERIFY2(std::abs(ed.currentPage().size().width() - 230) < tol, qPrintable(QString::number(ed.currentPage().size().width())));
        QCOMPARE(ed.currentPage().size().height(), 297.0);
        QCOMPARE(doc.page(0).size(), QSizeF(210, 297));
        QCOMPARE(doc.undoText(), QStringLiteral("Seitengrösse"));
        doc.undo();
        QCOMPARE(ed.currentPage().size(), QSizeF(210, 297));

        // Umschalt + Ecke unten rechts: Seite wächst ringsum, Objekt bleibt am Bildschirm stehen
        if (!canvas->pageSelected())
            QTest::mouseClick(canvas, Qt::LeftButton, {}, canvas->pageLabelRect(1).center().toPoint());
        QVERIFY(canvas->pageSelected());
        const QPointF objectBefore = canvas->toScreen(doc.findAs<DrawObject>(id)->boundingRect().topLeft());
        const QRectF page2 = canvas->viewTransform().mapRect(QRectF(QPointF(0, 0), ed.currentPage().size()));
        QCOMPARE(canvas->pageHandleAt(page2.bottomRight()), 4);
        QTest::mousePress(canvas, Qt::LeftButton, Qt::ShiftModifier, page2.bottomRight().toPoint(), 1000);
        QTest::mouseMove(canvas, (page2.bottomRight() + QPointF(10 * mm, 10 * mm)).toPoint());
        QTest::mouseRelease(canvas, Qt::LeftButton, Qt::ShiftModifier, (page2.bottomRight() + QPointF(10 * mm, 10 * mm)).toPoint(), 10);
        const QSizeF grown = ed.currentPage().size();
        QVERIFY2(std::abs(grown.width() - 230) < tol && std::abs(grown.height() - 317) < tol, qPrintable(describe(QRectF(QPointF(), grown))));
        const QRectF b = doc.findAs<DrawObject>(id)->boundingRect();
        QVERIFY2(near(b, QRectF(60, 60, 20, 20), tol), qPrintable(describe(b)));
        QVERIFY((canvas->toScreen(b.topLeft()) - objectBefore).manhattanLength() < 4);
        doc.undo();

        // Schalter «Alle Seiten»: Ziehen ändert alle Seiten und die Masterseite gleich
        ed.setPageSizeForAllPages(true);
        ed.resizePage(1, QRectF(0, 0, 230, 297));
        QCOMPARE(doc.page(0).size(), QSizeF(230, 297));
        QCOMPARE(doc.page(1).size(), QSizeF(230, 297));
        QCOMPARE(doc.masterPage().size(), QSizeF(230, 297));
        doc.undo();
        QCOMPARE(doc.page(0).size(), QSizeF(210, 297));

        // Rechtsklick auf den Seitennamen: Kontextmenü mit den Seitenbefehlen, hier Duplizieren
        QStringList texts;
        QTimer poll;
        poll.setInterval(20);
        int stage = 0;
        QObject::connect(&poll, &QTimer::timeout, [&] {
            if (stage == 0) {
                auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
                if (!menu || !menu->isVisible())
                    return;
                for (QAction* a : menu->actions()) {
                    texts << a->text();
                    if (a->text() == QStringLiteral("Seite d&uplizieren …"))
                        menu->setActiveAction(a);
                }
                // Erst nach diesem Aufruf auslösen: der Befehl öffnet einen Dialog, und dieser
                // Zeitgeber muss ihn bedienen können
                QTimer::singleShot(0, menu, [menu] { QTest::keyClick(menu, Qt::Key_Return); });
                stage = 1;
            } else if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) {
                auto* layersOnly = dialog->findChild<QRadioButton*>(QStringLiteral("layersOnly"));
                if (!layersOnly)
                    return;
                layersOnly->setChecked(true);
                dialog->accept();
                poll.stop();
            }
        });
        poll.start();
        QTest::mouseClick(canvas, Qt::RightButton, {}, canvas->pageLabelRect(0).center().toPoint());
        QTRY_COMPARE(doc.pageCount(), 3);
        QVERIFY2(texts.contains(QStringLiteral("Seite &umbenennen …")), qPrintable(texts.join(QLatin1Char('|'))));
        QCOMPARE(ed.currentPageIndex(), 1); // Kopie nach Seite 1
        QCOMPARE(doc.page(1).child(0)->childCount(), 0);
        QCOMPARE(canvas->pageLayout().size(), 3);
    }

    // Neues Dokument mit Mehrseitenansicht (Dialog "Neues Dokument erstellen")
    void newDocumentMultiPageView()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        Options::instance().set(QStringLiteral("ws/general/newDocDialog"), true);
        MainWindow window;
        window.resize(1300, 900);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTimer poll;
        poll.setInterval(20);
        QObject::connect(&poll, &QTimer::timeout, [&] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            auto* view = dialog->findChild<QComboBox*>(QStringLiteral("pageView"));
            auto* pages = dialog->findChild<QSpinBox*>();
            if (!view || !pages)
                return;
            view->setCurrentIndex(1);
            pages->setValue(4);
            dialog->accept();
            poll.stop();
        });
        poll.start();
        QAction* newDoc = nullptr;
        for (QAction* a : window.findChildren<QAction*>()) {
            if (a->text() == QStringLiteral("&Neu"))
                newDoc = a;
        }
        QVERIFY(newDoc);
        newDoc->trigger();
        Canvas* canvas = window.canvas();
        QCOMPARE(canvas->editor().document().pageCount(), 4);
        QVERIFY(canvas->multiPageView());
        QVERIFY(!canvas->editor().document().canUndo());
        QVERIFY(!canvas->editor().document().isModified());
        QCOMPARE(canvas->pageLayout().size(), 4);
        // Mehrseitenansicht wird mit dem Dokument gespeichert
        auto loaded = io::loadFromBytes(io::saveToBytes(canvas->editor().document()));
        QVERIFY(loaded);
        QVERIFY(loaded->masterPage().options().value(QStringLiteral("doc/layout/multiPage")).toBool());
        Options::instance().resetWorkspace();
    }

    // Überblendung an Pfad über das Menü, ganzer Pfad mit Rückgängig; neue Bitmap-Effektgruppen
    // wirken auf Bitmaps (Dialog) und auf Vektoren (Effektstapel)
    void blendAlongPathAndEffectGroups()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        Options::instance().set(QStringLiteral("global/bitmapfx/lastValues"), false);
        MainWindow window;
        window.resize(1200, 800);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        Editor& ed = window.canvas()->editor();
        auto circle = [&](const QPointF& at) {
            auto e = std::make_unique<EllipseObject>(QSizeF(10, 10));
            e->setTransform(QTransform::fromTranslate(at.x() - 5, at.y() - 5));
            Style s;
            s.fill = Fill::gradient(Fill::Type::Radial, Color(255, 255, 255), Color(0, 120, 160));
            s.stroke.reset();
            e->setStyle(s);
            return ed.addObject(std::move(e), QStringLiteral("Kreis"));
        };
        const NodeId a = circle(QPointF(30, 40)), b = circle(QPointF(130, 40));
        const NodeId blend = ed.blendObjects(a, b, 40);
        QPainterPath arc(QPointF(30, 120));
        arc.cubicTo(QPointF(30, 200), QPointF(130, 200), QPointF(130, 120));
        const NodeId path = ed.addObject(CurveObject::fromPath(arc), QStringLiteral("Pfad"));

        auto plain = [](QString text) { return text.remove(QLatin1Char('&')).remove(QStringLiteral(" …")); };
        auto findAction = [&](const QStringList& names) -> QAction* {
            QList<QAction*> actions = window.menuBar()->actions();
            QAction* found = nullptr;
            for (const QString& name : names) {
                found = nullptr;
                for (QAction* act : actions)
                    if (!act->isSeparator() && plain(act->text()) == name)
                        found = act;
                if (!found)
                    return nullptr;
                if (found->menu())
                    actions = found->menu()->actions();
            }
            return found;
        };
        auto obj = [&] { return ed.document().findAs<BlendObject>(blend); };
        ed.setSelection({blend, path});
        QAction* along = findAction({QStringLiteral("Effekte"), QStringLiteral("Überblendung an Pfad")});
        QVERIFY(along);
        const int undo = ed.document().undoCount();
        along->trigger();
        QVERIFY(obj()->pathObject());
        QCOMPARE(obj()->pathObject()->id(), path);
        QCOMPARE(ed.document().undoCount(), undo + 1);
        // Stufen liegen jetzt am Bogen, nicht mehr auf der Geraden y = 40
        QVERIFY(obj()->intermediates()[20]->boundingRect().center().y() > 150);
        // Über den ganzen Pfad: Start und Ende an den Pfadenden, ein Schritt zurück stellt sie wieder hin
        ed.setSelection({blend});
        ed.setObjectProperty(QStringLiteral("fullPath"), true, QStringLiteral("Ganzer Pfad"));
        QVERIFY(QLineF(obj()->start()->boundingRect().center(), QPointF(30, 120)).length() < 1e-6);
        ed.document().undo();
        QVERIFY(QLineF(obj()->start()->boundingRect().center(), QPointF(30, 40)).length() < 1e-6);
        QVERIFY(!obj()->fullPath());
        // Vom Pfad lösen: der Pfad liegt wieder für sich im Dokument
        QVERIFY(ed.detachBlendPath(blend));
        QVERIFY(!obj()->pathObject());
        QVERIFY(ed.document().findAs<DrawObject>(path));
        ed.document().undo();
        QVERIFY(obj()->pathObject());
        // Trennen behält Pfad und Stufen (Pfad, Start, 40 Stufen, Ende)
        ed.setSelection({blend});
        QVERIFY(ed.separateEffects());
        const auto* group = node_cast<GroupObject>(ed.document().find(ed.selection().first()));
        QVERIFY(group);
        QCOMPARE(group->childCount(), 43);
        ed.document().undo();

        // Neue Gruppen im Menü Bitmaps, Relief an einer Vektorform landet im Effektstapel
        for (const QStringList& entry : {QStringList{QStringLiteral("Verzerren"), QStringLiteral("Strudel")},
                                         QStringList{QStringLiteral("3D-Effekte"), QStringLiteral("Relief")},
                                         QStringList{QStringLiteral("3D-Effekte"), QStringLiteral("Ein-/Ausbuchten")},
                                         QStringList{QStringLiteral("Farbtransformation"), QStringLiteral("Solarisation")},
                                         QStringList{QStringLiteral("Kontur"), QStringLiteral("Kanten suchen")},
                                         QStringList{QStringLiteral("Rauschen"), QStringLiteral("Rauschen hinzufügen")},
                                         QStringList{QStringLiteral("Schärfe"), QStringLiteral("Unscharf maskieren")},
                                         QStringList{QStringLiteral("Unschärfe"), QStringLiteral("Gaußsche Unschärfe")}})
            QVERIFY2(findAction(QStringList{QStringLiteral("Bitmaps")} + entry), qPrintable(entry.join(QLatin1Char('>'))));
        ed.setSelection({a});
        bool seen = false;
        QTimer poll;
        poll.setInterval(20);
        QObject::connect(&poll, &QTimer::timeout, [&] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if (!dialog || !dialog->isVisible())
                return;
            poll.stop();
            seen = true;
            dialog->accept();
        });
        poll.start();
        findAction({QStringLiteral("Bitmaps"), QStringLiteral("3D-Effekte"), QStringLiteral("Relief")})->trigger();
        QVERIFY(seen);
        const auto* start = ed.document().findAs<DrawObject>(a);
        QCOMPARE(start->bitmapEffects().size(), 1);
        QCOMPARE(start->bitmapEffects().first().id, QStringLiteral("3d.emboss"));
    }

    // Bitmap-Effekte: Seite aufrollen, Zylinder (Bitmaps > 3D-Effekte) und weitere Korrekturen
    // (Effekte > Anpassen) öffnen ihre Dialoge, verändern die Bitmap und lassen sich rückgängig machen
    void bitmapCurlCylinderAndCorrections()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        Options::instance().set(QStringLiteral("global/bitmapfx/lastValues"), false);
        MainWindow window;
        window.resize(1200, 800);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        Editor& ed = window.canvas()->editor();
        QImage pixels(60, 40, QImage::Format_RGB32);
        for (int y = 0; y < pixels.height(); ++y)
            for (int x = 0; x < pixels.width(); ++x)
                pixels.setPixelColor(x, y, x < 30 ? QColor(200, 60 + y, 40) : QColor(40 + x, 150, 210 - y));
        auto object = std::make_unique<ImageObject>(imagepool::addImage(pixels), QSizeF(60, 40));
        object->setTransform(QTransform::fromTranslate(20, 20));
        const NodeId id = ed.addObject(std::move(object), QStringLiteral("Bild"));
        ed.setSelection({id});
        QVERIFY(ed.selectedImage());

        auto plain = [](QString text) { return text.remove(QLatin1Char('&')).remove(QStringLiteral(" …")); };
        auto findAction = [&](const QStringList& path) -> QAction* {
            QList<QAction*> actions = window.menuBar()->actions();
            QAction* found = nullptr;
            for (const QString& name : path) {
                found = nullptr;
                for (QAction* a : actions)
                    if (!a->isSeparator() && plain(a->text()) == name)
                        found = a;
                if (!found)
                    return nullptr;
                if (found->menu())
                    actions = found->menu()->actions();
            }
            return found;
        };
        // Eintrag auslösen, den modalen Dialog bedienen; liefert true, wenn der Dialog erschien
        auto run = [&](const QStringList& path, const std::function<void(QDialog*)>& drive) {
            QAction* action = findAction(path);
            if (!action)
                return false;
            bool seen = false;
            QTimer poll;
            poll.setInterval(20);
            QObject::connect(&poll, &QTimer::timeout, [&] {
                auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
                if (!dialog || !dialog->isVisible())
                    return;
                poll.stop();
                seen = true;
                drive(dialog);
                dialog->accept();
            });
            poll.start();
            action->trigger();
            return seen;
        };
        auto current = [&] { return ed.selectedImage()->image(); };
        auto check = [&](const QStringList& path, const std::function<void(QDialog*)>& drive) {
            const QImage before = current();
            const int undo = ed.document().undoCount();
            QVERIFY2(run(path, drive), qPrintable(path.join(QLatin1Char('>'))));
            QVERIFY2(current() != before, qPrintable(path.join(QLatin1Char('>'))));
            QCOMPARE(ed.document().undoCount(), undo + 1);
            ed.document().undo();
            QCOMPARE(current(), before);
            ed.setSelection({id});
        };
        const QString effects = QStringLiteral("Effekte"), adjustMenu = QStringLiteral("Anpassen");

        // Korrekturen stehen unter Effekte > Anpassen, nicht im Menü Bitmaps
        for (const char* name : {"Lokaler Ausgleich", "Proben-/Zielabgleich", "Selektive Farbe", "Farben ersetzen", "Kanalmixer"}) {
            QVERIFY2(findAction({effects, adjustMenu, QString::fromUtf8(name)}), name);
            QVERIFY2(!findAction({QStringLiteral("Bitmaps"), QString::fromUtf8(name)}), name);
        }

        check({effects, adjustMenu, QStringLiteral("Lokaler Ausgleich")}, [](QDialog* d) {
            auto* fx = qobject_cast<EffectDialog*>(d);
            QVERIFY(fx);
            fx->setValues({10, 10});
        });

        // Proben-/Zielabgleich: Pipette der Mitteltöne, dann in die Vorschau klicken (rote Hälfte)
        check({effects, adjustMenu, QStringLiteral("Proben-/Zielabgleich")}, [](QDialog* d) {
            auto* st = qobject_cast<SampleTargetDialog*>(d);
            QVERIFY(st);
            QTest::mouseClick(st->pickButton(1), Qt::LeftButton);
            QVERIFY(st->isPicking());
            QLabel* preview = st->previewLabel();
            QTest::mouseClick(preview, Qt::LeftButton, {}, preview->rect().center() - QPoint(15, 0));
            QVERIFY(!st->isPicking());
            const Color sample = st->samples()[1];
            QCOMPARE(int(sample.r), 200);
            QCOMPARE(int(sample.b), 40);
            const Color target = st->targets()[1];
            QCOMPARE(target.r, target.b); // neutrales Grau
        });

        check({effects, adjustMenu, QStringLiteral("Selektive Farbe")}, [](QDialog* d) {
            auto* sc = qobject_cast<SelectiveColorDialog*>(d);
            QVERIFY(sc);
            sc->setRange(int(adjust::ColorRange::Reds));
            const auto sliders = sc->findChildren<QSlider*>();
            QVERIFY(sliders.size() >= 4);
            sliders[0]->setValue(80); // Cyan in den Rottönen
            QCOMPARE(sc->settings().cmyk[0][0], 80);
            sc->setRange(int(adjust::ColorRange::Blues));
            QCOMPARE(sliders[0]->value(), 0);
        });

        // Farben ersetzen: alte Farbe mit der Pipette aus der blauen Hälfte
        check({effects, adjustMenu, QStringLiteral("Farben ersetzen")}, [](QDialog* d) {
            auto* rc = qobject_cast<ReplaceColorsDialog*>(d);
            QVERIFY(rc);
            QTest::mouseClick(rc->pickButton(), Qt::LeftButton);
            QLabel* preview = rc->previewLabel();
            QTest::mouseClick(preview, Qt::LeftButton, {}, preview->rect().center() + QPoint(15, 0));
            QCOMPARE(int(rc->settings().oldColor.g), 150);
            adjust::ReplaceColors s = rc->settings();
            s.newColor = Color(250, 200, 0);
            rc->setSettings(s);
        });

        check({effects, adjustMenu, QStringLiteral("Kanalmixer")}, [](QDialog* d) {
            auto* cm = qobject_cast<ChannelMixerDialog*>(d);
            QVERIFY(cm);
            cm->setOutputChannel(0);
            const auto sliders = cm->findChildren<QSlider*>();
            sliders[0]->setValue(0);
            sliders[2]->setValue(100); // Rot aus Blau
            QCOMPARE(cm->settings().rows[0][2], 100);
            QCOMPARE(cm->settings().rows[1][1], 100);
        });

        check({QStringLiteral("Bitmaps"), QStringLiteral("3D-Effekte"), QStringLiteral("Zylinder")}, [](QDialog* d) {
            auto* cy = qobject_cast<CylinderDialog*>(d);
            QVERIFY(cy);
            cy->setValues(90, false);
        });

        check({QStringLiteral("Bitmaps"), QStringLiteral("3D-Effekte"), QStringLiteral("Seite aufrollen")}, [](QDialog* d) {
            auto* pc = qobject_cast<PageCurlDialog*>(d);
            QVERIFY(pc);
            QTest::mouseClick(pc->cornerButton(filters::PageCurl::TopLeft), Qt::LeftButton);
            QCOMPARE(pc->settings().corner, filters::PageCurl::TopLeft);
            // Vorschau: Ecke oben links zeigt den (weissen) Hintergrund
            const QImage shown = pc->previewImage();
            QCOMPARE(shown.pixelColor(0, 0).rgb(), QColor(Qt::white).rgb());
            if (const QString dir = qEnvironmentVariable("VEK_SCREENSHOT_DIR"); !dir.isEmpty())
                pc->grab().save(dir + QStringLiteral("/fx-pagecurl-dialog.png"));
        });
        // Endergebnis einmal angewendet lassen: Ecke oben links ist Hintergrund, unten rechts unverändert
        QVERIFY(run({QStringLiteral("Bitmaps"), QStringLiteral("3D-Effekte"), QStringLiteral("Seite aufrollen")}, [](QDialog* d) {
            qobject_cast<PageCurlDialog*>(d)->cornerButton(filters::PageCurl::TopLeft)->click();
        }));
        const QImage result = current();
        QCOMPARE(result.pixelColor(0, 0).rgb(), QColor(Qt::white).rgb());
        QCOMPARE(result.pixelColor(59, 39).rgb(), pixels.pixelColor(59, 39).rgb());
        Options::instance().set(QStringLiteral("global/bitmapfx/lastValues"), true);
    }

    // --- Effektstapel: nicht-destruktive Bitmap-Effekte, Rahmen, SuperClip ---------------------

    void liveBitmapEffectOnRectangle()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        MainWindow window;
        window.resize(1300, 900);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        Canvas* canvas = window.canvas();
        Editor& ed = canvas->editor();
        Document& doc = ed.document();
        // Rechteck mit der Maus zeichnen, rot füllen, dicke schwarze Kontur
        canvas->setTool(ToolId::Rectangle);
        const auto at = [canvas](double x, double y) { return canvas->toScreen(QPointF(x, y)).toPoint(); };
        QTest::mousePress(canvas, Qt::LeftButton, {}, at(40, 40), 1000);
        for (int i = 1; i <= 4; ++i)
            QTest::mouseMove(canvas, at(40 + 15 * i, 40 + 10 * i));
        QTest::mouseRelease(canvas, Qt::LeftButton, {}, at(100, 80), 10);
        QCOMPARE(ed.selection().size(), 1);
        const NodeId rect = ed.selection().first();
        QVERIFY(doc.findAs<RectangleObject>(rect));
        ed.setSelectionFill(Fill::solid(Color(230, 20, 20)));
        ed.setSelectionStroke(Color(0, 0, 0));
        ed.setSelectionStrokeWidth(1.5);
        canvas->setTool(ToolId::Pick);
        ed.setSelection({});
        QTest::mouseClick(canvas, Qt::LeftButton, {}, at(70, 60), 1000);
        QCOMPARE(ed.selection(), QList<NodeId>{rect});

        // Bitmaps > Unschärfe > Weichzeichnen: Dialog bestätigen
        QMenu* bitmaps = nullptr;
        for (QAction* a : window.menuBar()->actions())
            if (a->text() == MainWindow::tr("&Bitmaps"))
                bitmaps = a->menu();
        QVERIFY(bitmaps);
        auto submenu = [](QMenu* menu, const QString& title) -> QMenu* {
            for (QAction* a : menu->actions())
                if (a->menu() && a->text().remove(QLatin1Char('&')) == title)
                    return a->menu();
            return nullptr;
        };
        auto entry = [](QMenu* menu, const QString& name) -> QAction* {
            for (QAction* a : menu->actions())
                if (a->text().remove(QLatin1Char('&')).remove(QStringLiteral(" …")) == name)
                    return a;
            return nullptr;
        };
        QMenu* blur = submenu(bitmaps, QStringLiteral("Unschärfe"));
        QVERIFY(blur);
        emit bitmaps->aboutToShow();
        QAction* soften = entry(blur, QStringLiteral("Weichzeichnen"));
        QVERIFY(soften);
        QVERIFY(soften->isEnabled()); // auch ohne Bitmap
        bool sawDialog = false;
        QTimer poll;
        poll.setInterval(20);
        QObject::connect(&poll, &QTimer::timeout, [&] {
            auto* dialog = qobject_cast<EffectDialog*>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            sawDialog = true;
            dialog->setValues({100});
            dialog->accept();
            poll.stop();
        });
        poll.start();
        soften->trigger();
        QVERIFY(sawDialog);
        // Objekt bleibt ein Rechteck, mit einem Effekt im Stapel; ein Rückgängig-Schritt
        const auto* obj = doc.findAs<RectangleObject>(rect);
        QVERIFY(obj);
        QCOMPARE(obj->bitmapEffects().size(), 1);
        QCOMPARE(obj->bitmapEffects().first().id, QStringLiteral("blur.soften"));
        QCOMPARE(obj->bitmapEffects().first().values, QList<int>{100});
        QCOMPARE(doc.undoText(), QStringLiteral("Weichzeichnen"));

        // Andockfenster "Effekte" zeigt den Stapel
        BitmapEffectsDocker* docker = window.bitmapEffectsDocker();
        QVERIFY(docker);
        QTRY_COMPARE(docker->list()->count(), 1);
        QCOMPARE(docker->list()->item(0)->text(), QStringLiteral("Weichzeichnen"));
        QCOMPARE(docker->list()->item(0)->checkState(), Qt::Checked);

        // Gerenderte Pixel: mit Effekt anders als ohne (Kante Rot/Schwarz wird weich)
        auto render = [&] {
            QImage img(QSize(360, 260), QImage::Format_ARGB32_Premultiplied);
            img.fill(Qt::white);
            QPainter p(&img);
            canvas->renderPage(p, 0, 4.0, QPointF(-120, -120), false);
            return img;
        };
        const QImage withEffect = render();
        if (!qEnvironmentVariable("VEK_SCREENSHOT_DIR").isEmpty()) {
            QApplication::processEvents();
            window.grab().save(qEnvironmentVariable("VEK_SCREENSHOT_DIR") + QStringLiteral("/fx-01-weichzeichnen.png"));
        }
        // Auge aus (Häkchen im Andockfenster)
        docker->list()->item(0)->setCheckState(Qt::Unchecked);
        QVERIFY(!doc.findAs<DrawObject>(rect)->bitmapEffects().first().enabled);
        QVERIFY(!doc.findAs<DrawObject>(rect)->hasBitmapEffects());
        const QImage without = render();
        QVERIFY(withEffect != without);
        int changed = 0;
        for (int y = 0; y < without.height(); ++y)
            for (int x = 0; x < without.width(); ++x)
                changed += withEffect.pixel(x, y) != without.pixel(x, y);
        QVERIFY2(changed > 50, qPrintable(QString::number(changed)));
        // Mitte bleibt rot, ausserhalb weiss
        QCOMPARE(QColor(withEffect.pixel(130, 100)).red(), 230);
        QCOMPARE(withEffect.pixel(5, 5), qRgb(255, 255, 255));
        // Rückgängig: Auge wieder an, gleiche Pixel wie vorher
        doc.undo();
        QVERIFY(doc.findAs<DrawObject>(rect)->bitmapEffects().first().enabled);
        QTRY_COMPARE(docker->list()->item(0)->checkState(), Qt::Checked);
        QVERIFY(render() == withEffect);
        // Rückgängig des Effekts selbst
        doc.undo();
        QVERIFY(doc.findAs<DrawObject>(rect)->bitmapEffects().isEmpty());
        QTRY_COMPARE(docker->list()->count(), 0);
        QVERIFY(render() == without);
        doc.redo();
        QCOMPARE(doc.findAs<DrawObject>(rect)->bitmapEffects().size(), 1);
        QTRY_COMPARE(docker->list()->count(), 1);

        // Zweiter Effekt, Reihenfolge, Bearbeiten mit gespeicherten Werten, Löschen
        BitmapEffect posterize = fx::defaults(*fx::find(QStringLiteral("art.posterize")));
        QVERIFY(ed.addBitmapEffect(posterize, QStringLiteral("Posterisieren")));
        QTRY_COMPARE(docker->list()->count(), 2);
        docker->moveEffect(1, -1);
        QCOMPARE(doc.findAs<DrawObject>(rect)->bitmapEffects().first().id, QStringLiteral("art.posterize"));
        QList<int> shownValues;
        QTimer poll2;
        poll2.setInterval(20);
        QObject::connect(&poll2, &QTimer::timeout, [&] {
            auto* dialog = qobject_cast<EffectDialog*>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            shownValues = dialog->values();
            dialog->setValues({40});
            dialog->accept();
            poll2.stop();
        });
        poll2.start();
        QVERIFY(docker->editEffect(1)); // Weichzeichnen
        QCOMPARE(shownValues, QList<int>{100});
        QCOMPARE(doc.findAs<DrawObject>(rect)->bitmapEffects()[1].values, QList<int>{40});
        docker->removeEffect(0);
        QCOMPARE(doc.findAs<DrawObject>(rect)->bitmapEffects().size(), 1);

        // Effekte anwenden: wird zum Bild an derselben Stelle
        const QRectF before = doc.findAs<DrawObject>(rect)->boundingRect();
        const NodeId image = docker->applyEffects();
        QVERIFY(image != InvalidNodeId);
        const auto* img = doc.findAs<ImageObject>(image);
        QVERIFY(img);
        QVERIFY(!doc.find(rect));
        QVERIFY(img->bitmapEffects().isEmpty());
        QVERIFY(img->boundingRect().contains(before));
        QVERIFY(img->boundingRect().width() < before.width() + 8);
        QCOMPARE(doc.undoText(), QStringLiteral("Effekte anwenden"));
        doc.undo();
        QVERIFY(doc.findAs<RectangleObject>(rect));
    }

    void frameEffectOnPhoto()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        MainWindow window;
        window.resize(1300, 900);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        Editor& ed = window.canvas()->editor();
        Document& doc = ed.document();
        // "Foto": Farbverlauf, voll deckend
        QImage pixels(300, 200, QImage::Format_RGB32);
        for (int y = 0; y < pixels.height(); ++y)
            for (int x = 0; x < pixels.width(); ++x)
                pixels.setPixel(x, y, qRgb(x * 255 / 300, 120, y * 255 / 200));
        auto photo = std::make_unique<ImageObject>(imagepool::addImage(pixels), QSizeF(60, 40));
        photo->setTransform(QTransform::fromTranslate(30, 30));
        const NodeId id = ed.addObject(std::move(photo), QStringLiteral("Foto"));
        ed.setSelection({id});

        QMenu* bitmaps = nullptr;
        for (QAction* a : window.menuBar()->actions())
            if (a->text() == MainWindow::tr("&Bitmaps"))
                bitmaps = a->menu();
        QMenu* creative = nullptr;
        for (QAction* a : bitmaps->actions())
            if (a->menu() && a->text().remove(QLatin1Char('&')) == QStringLiteral("Kreativ"))
                creative = a->menu();
        QVERIFY(creative);
        QAction* frame = nullptr;
        for (QAction* a : creative->actions())
            if (a->text() == QStringLiteral("Rahmen …"))
                frame = a;
        QVERIFY(frame);
        int templates = 0;
        QTimer poll;
        poll.setInterval(20);
        QObject::connect(&poll, &QTimer::timeout, [&] {
            auto* dialog = qobject_cast<FrameDialog*>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            templates = dialog->templates()->count();
            dialog->templates()->setCurrentRow(0); // gerissenes Papier
            dialog->modeBox()->setCurrentIndex(0);  // Maske
            if (!qEnvironmentVariable("VEK_SCREENSHOT_DIR").isEmpty())
                dialog->grab().save(qEnvironmentVariable("VEK_SCREENSHOT_DIR") + QStringLiteral("/fx-02-rahmen-dialog.png"));
            dialog->accept();
            poll.stop();
        });
        poll.start();
        frame->trigger();
        QCOMPARE(templates, 8);
        // Bitmap: Pixel direkt geändert (destruktiv), Ränder durchsichtig, Mitte unverändert
        const auto* img = doc.findAs<ImageObject>(id);
        QVERIFY(img->bitmapEffects().isEmpty());
        const QImage result = img->image().convertToFormat(QImage::Format_ARGB32);
        QCOMPARE(result.size(), pixels.size());
        QVERIFY(qAlpha(result.pixel(0, 0)) < 30);
        QVERIFY(qAlpha(result.pixel(150, 0)) < 80);
        QVERIFY(qAlpha(result.pixel(299, 100)) < 80);
        QCOMPARE(qAlpha(result.pixel(150, 100)), 255);
        QCOMPARE(result.pixel(150, 100) & 0xffffff, pixels.pixel(150, 100) & 0xffffff);
        int transparent = 0;
        for (int x = 0; x < 300; ++x)
            transparent += qAlpha(result.pixel(x, 10)) < 128;
        // Ausgefranst: in 10 px Tiefe teils Bild, teils durchsichtig
        QVERIFY2(transparent > 5 && transparent < 295, qPrintable(QString::number(transparent)));
        doc.undo();
        QCOMPARE(qAlpha(doc.findAs<ImageObject>(id)->image().convertToFormat(QImage::Format_ARGB32).pixel(0, 0)), 255);
    }

    void superClipFramedByLiveEffect()
    {
        // Rahmen mit Live-Effekt "Rahmen" (gerissen): der Inhalt wird an der gerasterten Kante
        // beschnitten, nicht am Rechteck
        auto addRect = [this](const QRectF& r, const Color& c) {
            auto o = std::make_unique<RectangleObject>(r.size());
            o->setTransform(QTransform::fromTranslate(r.left(), r.top()));
            Style s;
            s.fill = Fill::solid(c);
            s.stroke.reset();
            o->setStyle(s);
            return m_editor->addObject(std::move(o), QStringLiteral("R"));
        };
        const NodeId frame = addRect(QRectF(20, 20, 80, 60), Color(255, 255, 0));
        const NodeId content = addRect(QRectF(0, 0, 150, 120), Color(0, 0, 255));
        auto render = [this] {
            QImage img(QSize(480, 400), QImage::Format_ARGB32_Premultiplied);
            img.fill(Qt::white);
            QPainter p(&img);
            m_canvas->renderPage(p, 0, 4.0, QPointF(0, 0), false);
            return img;
        };
        m_editor->setSelection({content});
        const NodeId clip = m_editor->placeInsideFrame(frame);
        QVERIFY(clip != InvalidNodeId);
        // Ohne Effekt: Vektorpfad, Inhalt bis in die Ecke
        const QPoint corner(20 * 4 + 1, 20 * 4 + 1), middle(60 * 4, 50 * 4);
        QImage plain = render();
        QCOMPARE(plain.pixel(corner), qRgb(0, 0, 255));
        QCOMPARE(plain.pixel(middle), qRgb(0, 0, 255));
        // Rahmeneffekt am Rahmen (erstes Kind des SuperClip)
        m_editor->setSelection({frame});
        const BitmapEffect frameFx = fx::defaults(*fx::find(QStringLiteral("creative.frame")));
        QVERIFY(m_editor->addBitmapEffect(frameFx, QStringLiteral("Rahmen")));
        QVERIFY(m_editor->document().findAs<DrawObject>(frame)->hasBitmapEffects());
        const QImage framed = render();
        if (!qEnvironmentVariable("VEK_SCREENSHOT_DIR").isEmpty())
            framed.save(qEnvironmentVariable("VEK_SCREENSHOT_DIR") + QStringLiteral("/fx-03-superclip-rahmen.png"));
        QCOMPARE(framed.pixel(middle), qRgb(0, 0, 255));
        QVERIFY(qRed(framed.pixel(corner)) > 200); // Ecke abgerissen: Seite sichtbar
        QCOMPARE(framed.pixel(QPoint(10, 10)), qRgb(255, 255, 255));
        // Entlang der Kante (4 mm innen): teils Inhalt, teils weg – ausgefranst statt gerade
        int blue = 0, white = 0;
        for (int x = 25 * 4; x < 95 * 4; ++x) {
            const QRgb c = framed.pixel(x, 24 * 4);
            blue += c == qRgb(0, 0, 255);
            white += qRed(c) > 240 && qBlue(c) > 240;
        }
        QVERIFY2(blue > 10 && white > 10, qPrintable(QStringLiteral("%1 %2").arg(blue).arg(white)));
        // Vektorumriss bleibt das Rechteck
        QCOMPARE(m_editor->document().findAs<DrawObject>(clip)->boundingRect(), QRectF(20, 20, 80, 60));
        // Effekt aus: wieder gerade beschnitten
        m_editor->document().undo();
        QCOMPARE(render().pixel(corner), qRgb(0, 0, 255));
    }

    // --- Künstlerische Medien: Sprühdose, Pinsel, Strichbibliothek --------------------------
private:
    QColor canvasPixel(const QPointF& doc) { return m_canvas->grab().toImage().pixelColor(screen(doc)); }
    void shot(QWidget& w, const QString& name)
    {
        const QString dir = qEnvironmentVariable("VEK_SCREENSHOT_DIR");
        if (!dir.isEmpty())
            w.grab().save(dir + QLatin1Char('/') + name + QStringLiteral(".png"));
    }

private slots:
    void mediaSprayerFromPropertyBar()
    {
        QTemporaryDir dir;
        MediaLibrary::instance().setFilePath(dir.filePath(QStringLiteral("medien.json")));
        PropertyBar bar(*m_editor, *m_canvas);
        bar.resize(1600, bar.sizeHint().height());
        bar.show();
        m_canvas->setTool(ToolId::ArtisticMedia);
        auto* sprayMode = bar.findChild<QToolButton*>(QStringLiteral("mediaMode%1").arg(int(ArtisticMedia::Kind::Sprayer)));
        QVERIFY(sprayMode && sprayMode->isEnabled());
        QTest::mouseClick(sprayMode, Qt::LeftButton);
        QCoreApplication::processEvents();
        QCOMPARE(m_canvas->mediaTool().media().kind, ArtisticMedia::Kind::Sprayer);
        // Sprühliste "Herzen" wählen, Grösse 6 mm, Abstand 7 mm
        QComboBox* lists = nullptr;
        for (QComboBox* c : bar.findChildren<QComboBox*>(QStringLiteral("sprayList")))
            if (c->isVisibleTo(&bar))
                lists = c;
        QVERIFY(lists);
        const int hearts = lists->findText(pathops::sprayListName(3));
        QVERIFY(hearts >= 0);
        lists->setCurrentIndex(hearts);
        emit lists->activated(hearts);
        QCOMPARE(m_canvas->mediaTool().media().sprayList, 3);
        ArtisticMedia media = m_canvas->mediaTool().media();
        media.width = 6.0;
        media.spacing = 7.0;
        m_canvas->mediaTool().setMedia(media);
        m_canvas->notifyToolState();
        shot(bar, QStringLiteral("fx-media-01-leiste-spruehdose"));

        // Freihandstrich mit der Maus: Herzen entlang der Spur
        press(screen(QPointF(30, 120)));
        for (int i = 1; i <= 40; ++i)
            move(screen(QPointF(30 + 3.0 * i, 120 - 25 * std::sin(i * M_PI / 20))));
        release(screen(QPointF(150, 120)));
        const NodeId id = m_editor->selection().value(0);
        const auto* c = m_editor->document().findAs<CurveObject>(id);
        QVERIFY(c && c->media().kind == ArtisticMedia::Kind::Sprayer && c->media().sprayList == 3);
        const ArtItem pieces = c->mediaPieces();
        QVERIFY2(pieces.size() >= 15, qPrintable(QString::number(pieces.size())));
        int colored = 0;
        for (const ArtPiece& p : pieces) {
            QVERIFY(p.path.boundingRect().width() <= 6.01);
            const QColor seen = canvasPixel(p.path.boundingRect().center());
            if (seen.red() > 180 && seen.green() < 170)
                ++colored; // rot/rosa
        }
        QVERIFY2(colored >= pieces.size() / 2, qPrintable(QString::number(colored)));
        QCOMPARE(MediaLibrary::instance().recent().value(0).media.sprayList, 3);
        screenshot(QStringLiteral("fx-media-02-spruehdose"));

        // Parameter wirken live auf den gewählten Strich: drei Objekte je Tupfer
        auto* count = bar.findChild<QDoubleSpinBox*>(QStringLiteral("sprayCount"));
        QVERIFY(count);
        count->setValue(3);
        emit count->editingFinished();
        QCOMPARE(m_editor->document().findAs<CurveObject>(id)->media().dabCount, 3);
        QCOMPARE(m_editor->document().findAs<CurveObject>(id)->mediaPieces().size(), 3 * pieces.size());

        // Strg+K trennt in eine Gruppe der Herzen und das Rückgrat
        QVERIFY(m_editor->canBreakApartSelection());
        m_editor->breakApartSelection();
        QCOMPARE(m_editor->selection().size(), 2);
        const auto* group = m_editor->document().findAs<GroupObject>(m_editor->selection().value(0));
        const auto* spine = m_editor->document().findAs<CurveObject>(m_editor->selection().value(1));
        QVERIFY(group && spine);
        QCOMPARE(group->childCount(), int(3 * pieces.size()));
        QVERIFY(!spine->media().isActive());
        m_editor->document().undo();
        QVERIFY(m_editor->document().findAs<CurveObject>(id));
        m_canvas->setTool(ToolId::Pick);
    }

    void mediaBrushFollowsNodeMove()
    {
        QTemporaryDir dir;
        MediaLibrary::instance().setFilePath(dir.filePath(QStringLiteral("medien.json")));
        PropertyBar bar(*m_editor, *m_canvas);
        m_canvas->setTool(ToolId::ArtisticMedia);
        auto* brushMode = bar.findChild<QToolButton*>(QStringLiteral("mediaMode%1").arg(int(ArtisticMedia::Kind::Brush)));
        QVERIFY(brushMode && brushMode->isEnabled());
        QTest::mouseClick(brushMode, Qt::LeftButton);
        QCOMPARE(m_canvas->mediaTool().media().kind, ArtisticMedia::Kind::Brush);
        QVERIFY(!m_canvas->mediaTool().media().art.isEmpty());
        // Regenbogen (eingebaut, gestreckt), 8 mm breit
        ArtisticMedia media = MediaLibrary::instance().brushes().at(0).media;
        media.width = 8.0;
        m_canvas->mediaTool().setMedia(media);
        m_canvas->setTool(ToolId::Pick);
        const NodeId line = addOpenLine({QPointF(30, 100), QPointF(130, 100)});
        m_editor->setSelection({line});
        QVERIFY(m_editor->applyMediaToSelection(media, QStringLiteral("Pinsel")));
        const NodeId brushId = m_editor->selection().value(0);
        const auto* c = m_editor->document().findAs<CurveObject>(brushId);
        QVERIFY(c && c->media().kind == ArtisticMedia::Kind::Brush);
        QVERIFY2(near(c->boundingRect(), QRectF(30, 96, 100, 8), 0.05), qPrintable(describe(c->boundingRect())));
        // Farben der Vorlage: oben rot, unten violett
        QColor top = canvasPixel(QPointF(80, 96.5)), bottom = canvasPixel(QPointF(80, 103.5));
        QVERIFY2(top.red() > 180 && top.blue() < 120, qPrintable(top.name()));
        QVERIFY2(bottom.blue() > 120 && bottom.green() < 100, qPrintable(bottom.name()));
        screenshot(QStringLiteral("fx-media-03-pinsel"));

        // Formwerkzeug: Endknoten nach unten ziehen, der Pinselstrich folgt
        m_canvas->setTool(ToolId::Shape);
        drag(screen(QPointF(130, 100)), screen(QPointF(130, 150)));
        const auto* moved = m_editor->document().findAs<CurveObject>(brushId);
        QVERIFY(moved);
        QVERIFY2(moved->boundingRect().bottom() > 150, qPrintable(describe(moved->boundingRect())));
        const QPointF mid(80, 125); // Mitte der schrägen Linie
        QColor seen = canvasPixel(mid);
        QVERIFY2(seen != QColor(Qt::white), qPrintable(seen.name()));
        screenshot(QStringLiteral("fx-media-04-pinsel-knoten"));
        m_canvas->setTool(ToolId::Pick);

        // Wiederholen: mehr Teile als beim Strecken
        const int stretched = int(moved->mediaPieces().size());
        m_editor->setSelection({moved->id()});
        ArtisticMedia repeat = MediaLibrary::instance().brushes().at(2).media; // Perlenkette
        repeat.width = 4.0;
        QVERIFY(m_editor->applyMediaToSelection(repeat, QStringLiteral("Pinsel")));
        const auto* beads = m_editor->document().findAs<CurveObject>(m_editor->selection().value(0));
        QVERIFY(beads->media().brushRepeat);
        QVERIFY2(beads->mediaPieces().size() > 3 * stretched, qPrintable(QString::number(beads->mediaPieces().size())));
        screenshot(QStringLiteral("fx-media-05-perlenkette"));
        // Strg+K: Gruppe der Teile und Rückgrat
        m_editor->breakApartSelection();
        QVERIFY(m_editor->document().findAs<GroupObject>(m_editor->selection().value(0)));
        QVERIFY(m_editor->document().findAs<CurveObject>(m_editor->selection().value(1)));
    }

    void mediaDockerSavesAndAppliesStroke()
    {
        QTemporaryDir dir;
        MediaLibrary::instance().setFilePath(dir.filePath(QStringLiteral("medien.json")));
        // Vorlage: grünes Rechteck mit oranger Ellipse darauf
        const NodeId rect = drawRect(QRectF(20, 30, 40, 10));
        m_editor->setSelectionFill(Fill::solid(Color(0, 160, 0)));
        m_editor->setSelectionStroke(std::nullopt);
        auto e = std::make_unique<EllipseObject>(QSizeF(10, 10));
        e->setTransform(QTransform::fromTranslate(60, 30));
        Style s;
        s.fill = Fill::solid(Color(255, 140, 0));
        s.stroke.reset();
        e->setStyle(s);
        const NodeId ellipse = m_editor->addObject(std::move(e), QStringLiteral("E"));
        m_editor->setSelection({rect, ellipse});

        MediaDocker docker(*m_editor, *m_canvas);
        docker.resize(260, 520);
        docker.show();
        QVERIFY(QTest::qWaitForWindowExposed(&docker));
        auto* save = docker.findChild<QPushButton*>(QStringLiteral("mediaSave"));
        auto* type = docker.findChild<QComboBox*>(QStringLiteral("mediaSaveType"));
        auto* name = docker.findChild<QLineEdit*>(QStringLiteral("mediaSaveName"));
        QVERIFY(save && type && name && save->isEnabled());
        type->setCurrentIndex(0); // Pinselstrich
        QTest::keyClicks(name, QStringLiteral("Grasland"));
        QTest::mouseClick(save, Qt::LeftButton);
        QCOMPARE(MediaLibrary::instance().user().size(), 1);
        const MediaStroke saved = MediaLibrary::instance().user().first();
        QCOMPARE(saved.name, QStringLiteral("Grasland"));
        QCOMPARE(saved.media.kind, ArtisticMedia::Kind::Brush);
        QCOMPARE(saved.media.art.value(0).size(), 2);
        QVERIFY(std::abs(saved.media.width - 10.0) < kTol); // Höhe der Vorlage
        QCOMPARE(docker.category(), MediaDocker::Category::User);
        QCOMPARE(docker.list()->count(), 1);
        // Auch als Sprühliste: je Objekt ein Eintrag
        type->setCurrentIndex(1);
        QTest::mouseClick(save, Qt::LeftButton);
        QCoreApplication::processEvents();
        QCOMPARE(MediaLibrary::instance().user().size(), 2);
        QCOMPARE(MediaLibrary::instance().user().at(1).media.art.size(), 2);
        QCOMPARE(docker.list()->count(), 2);
        shot(docker, QStringLiteral("fx-media-06-docker"));

        // Linie wählen und den eigenen Pinsel mit einem Klick anwenden
        const NodeId line = addOpenLine({QPointF(20, 120), QPointF(70, 90), QPointF(140, 130)}, true);
        m_editor->setSelection({line});
        QListWidgetItem* item = docker.list()->item(0);
        QVERIFY(item && item->text() == QStringLiteral("Grasland"));
        QTest::mouseClick(docker.list()->viewport(), Qt::LeftButton, {}, docker.list()->visualItemRect(item).center());
        QCoreApplication::processEvents();
        const auto* c = m_editor->document().findAs<CurveObject>(m_editor->selection().value(0));
        QVERIFY(c && c->media().kind == ArtisticMedia::Kind::Brush && c->media().artName == QStringLiteral("Grasland"));
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Künstlerische Medien"));
        const ArtItem pieces = c->mediaPieces();
        QCOMPARE(pieces.size(), 2);
        QCOMPARE(pieces[0].color, Color(0, 160, 0));
        QCOMPARE(pieces[1].color, Color(255, 140, 0));
        // Anfang grün, Ende orange auf der Zeichenfläche
        const QColor start = canvasPixel(QPointF(22, 119)), end = canvasPixel(c->spinePath().pointAtPercent(0.97));
        QVERIFY2(start.green() > 120 && start.red() < 80, qPrintable(start.name()));
        QVERIFY2(end.red() > 200 && end.green() > 100 && end.blue() < 80, qPrintable(end.name()));
        QCOMPARE(MediaLibrary::instance().recent().value(0).name, QStringLiteral("Grasland"));
        screenshot(QStringLiteral("fx-media-07-eigener-pinsel"));

        // Eigene Sprühliste auf ein Rechteck anwenden: wird zur Kurve mit Sprühstrich
        m_editor->setSelection({rect});
        QVERIFY(docker.apply(1));
        const auto* sprayed = m_editor->document().findAs<CurveObject>(m_editor->selection().value(0));
        QVERIFY(sprayed && sprayed->media().kind == ArtisticMedia::Kind::Sprayer && sprayed->media().art.size() == 2);
        screenshot(QStringLiteral("fx-media-08-eigene-spruehliste"));
        // Trennen
        m_editor->breakApartSelection();
        const auto* group = m_editor->document().findAs<GroupObject>(m_editor->selection().value(0));
        QVERIFY(group && group->childCount() >= 4);

        // Zuletzt verwendet zeigt beide; Löschen entfernt den eigenen Strich
        docker.setCategory(MediaDocker::Category::Recent);
        QVERIFY(docker.list()->count() >= 2);
        docker.setCategory(MediaDocker::Category::User);
        docker.list()->setCurrentRow(1);
        auto* del = docker.findChild<QPushButton*>(QStringLiteral("mediaDelete"));
        QVERIFY(del && del->isEnabled());
        QTest::mouseClick(del, Qt::LeftButton);
        QCoreApplication::processEvents();
        QCOMPARE(MediaLibrary::instance().user().size(), 1);
        QCOMPARE(docker.list()->count(), 1);
    }

    // --- Effekte: Blockschatten, Fotomosaik, Punktraster, Hüllenzuordnung, Abschrägung bis Mitte,
    // Linse Bitmap-Effekt -------------------------------------------------------------------------

    void blockShadowToolDragAndSeparate()
    {
        auto pixel = [this](const QPointF& doc) {
            QImage img(3, 3, QImage::Format_ARGB32_Premultiplied);
            img.fill(Qt::white);
            QPainter p(&img);
            m_canvas->renderPage(p, 0, 8.0, QPointF(1.5, 1.5) - doc * 8.0, true);
            p.end();
            return img.pixelColor(1, 1);
        };
        const NodeId r = drawRect(QRectF(40, 40, 30, 20));
        m_editor->setSelectionFill(Fill::solid(Color(255, 220, 0)));
        auto obj = [this, r] { return m_editor->document().findAs<DrawObject>(r); };
        // Werkzeug aus dem Flyout der interaktiven Werkzeuge
        m_canvas->setTool(ToolId::BlockShadow);
        QCOMPARE(m_canvas->toolId(), ToolId::BlockShadow);
        const int undoBefore = m_editor->document().undoCount();
        drag(screen(QPointF(55, 50)), screen(QPointF(70, 60)), {}, QStringLiteral("90-blockschatten-vorschau"));
        QVERIFY(obj()->blockShadow().active);
        const QPointF off = obj()->blockShadow().offset;
        QVERIFY2(std::abs(off.x() - 15) < kTol && std::abs(off.y() - 10) < kTol, qPrintable(QStringLiteral("%1 %2").arg(off.x()).arg(off.y())));
        QCOMPARE(m_editor->document().undoCount(), undoBefore + 1);
        QCOMPARE(m_editor->document().undoText(), QStringLiteral("Blockschatten"));
        screenshot(QStringLiteral("91-blockschatten"));
        // Schatten = Rechteck entlang des Zugs geschoben: schwarz neben und hinter dem Objekt
        QCOMPARE(pixel(QPointF(80, 65)), QColor(Qt::black));
        QCOMPARE(pixel(QPointF(68, 62)), QColor(Qt::black)); // Ecke des Sechsecks
        QCOMPARE(pixel(QPointF(45, 66)), QColor(Qt::white)); // ausserhalb der überstrichenen Fläche
        QCOMPARE(pixel(QPointF(50, 45)), QColor(255, 220, 0)); // Objekt liegt darüber
        // Folgt dem Objekt: Rechteck breiter machen, Schatten wächst mit
        m_editor->transformSelection(xform::scaling(2.0, 1.0, QPointF(40, 40)), QStringLiteral("Breiter"));
        QCOMPARE(pixel(QPointF(110, 65)), QColor(Qt::black));
        m_editor->document().undo();
        QCOMPARE(pixel(QPointF(110, 65)), QColor(Qt::white));
        // Endgriff ziehen: Richtung ändern (nach rechts)
        drag(screen(QPointF(70, 60)), screen(QPointF(75, 50)));
        QVERIFY2(std::abs(obj()->blockShadow().offset.x() - 20) < kTol && std::abs(obj()->blockShadow().offset.y()) < kTol,
                 qPrintable(QString::number(obj()->blockShadow().offset.y())));
        QCOMPARE(pixel(QPointF(88, 45)), QColor(Qt::black));
        // Eigenschaftsleiste: Tiefe und Richtung über die Felder
        BlockShadow s = obj()->blockShadow();
        s.setPolar(12, -90);
        s.color = Color(0, 0, 200);
        m_editor->setSelectionBlockShadow(s, QStringLiteral("Blockschatten"));
        QCOMPARE(pixel(QPointF(50, 68)), QColor(0, 0, 200));
        // Vektor in SVG: kein eingebettetes Bild
        const QByteArray svg = output::pageSvg(*m_canvas, 0);
        QVERIFY(!svg.contains("<image"));
        QVERIFY(svg.contains("#0000c8"));
        // Strg+K: Schatten als Kurve abtrennen
        QVERIFY(m_editor->separateEffects());
        const auto* group = m_editor->document().findAs<GroupObject>(m_editor->selection().value(0));
        QVERIFY(group);
        QCOMPARE(group->childCount(), 2);
        const auto* curve = node_cast<CurveObject>(group->child(0));
        QVERIFY(curve);
        QCOMPARE(curve->style().fill.representativeColor().value_or(Color()), Color(0, 0, 200));
        QVERIFY2(std::abs(curve->boundingRect().bottom() - 72) < 0.3, qPrintable(describe(curve->boundingRect())));
        QVERIFY(!obj()->blockShadow().active);
        QCOMPARE(pixel(QPointF(50, 68)), QColor(0, 0, 200));
        m_editor->document().undo();
        QVERIFY(obj()->blockShadow().active);
        // Speichern und laden
        auto loaded = io::loadFromBytes(io::saveToBytes(m_editor->document()));
        QVERIFY(loaded);
        QCOMPARE(loaded->findAs<DrawObject>(r)->blockShadow(), obj()->blockShadow());
        // Löschen über die Leiste: ein Undo-Schritt
        m_editor->setSelection({r});
        m_editor->setSelectionBlockShadow(BlockShadow(), QStringLiteral("Blockschatten löschen"));
        QVERIFY(!obj()->blockShadow().active);
        m_editor->document().undo();
        QVERIFY(obj()->blockShadow().active);
    }

    void photoMosaicAndPointillizeFromMenu()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        Options::instance().resetWorkspace();
        // Kachelordner mit vier einfarbigen Bildern
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QList<std::pair<QString, QColor>> tiles{{QStringLiteral("a-rot.png"), QColor(240, 20, 20)},
                                                       {QStringLiteral("b-blau.png"), QColor(20, 20, 240)},
                                                       {QStringLiteral("c-gelb.png"), QColor(240, 230, 20)},
                                                       {QStringLiteral("d-weiss.png"), QColor(250, 250, 250)}};
        for (const auto& [name, color] : tiles) {
            QImage t(32, 32, QImage::Format_RGB32);
            t.fill(color);
            {
                QPainter p(&t); // etwas Zeichnung in der Kachel
                p.fillRect(QRect(8, 8, 6, 6), color.darker(130));
            }
            QVERIFY(t.save(dir.filePath(name)));
        }
        MainWindow window;
        window.resize(1300, 900);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        Editor& ed = window.canvas()->editor();
        // Vorlage: links rot, rechts blau (60 x 30 Pixel)
        QImage src(60, 30, QImage::Format_ARGB32);
        src.fill(QColor(255, 0, 0));
        {
            QPainter p(&src);
            p.fillRect(QRect(30, 0, 30, 30), QColor(0, 0, 255));
        }
        const NodeId image = ed.importImage(src, QPointF(100, 100));
        QVERIFY(image);
        ed.setSelection({image});
        const QRectF imageBox = ed.document().findAs<DrawObject>(image)->boundingRect();
        auto action = [&window](const QString& text) {
            for (QAction* a : window.findChildren<QAction*>())
                if (a->text() == text)
                    return a;
            return static_cast<QAction*>(nullptr);
        };
        QAction* mosaicAction = action(QStringLiteral("&Fotomosaik …"));
        QAction* pointAction = action(QStringLiteral("P&unktraster …"));
        QVERIFY(mosaicAction && pointAction);
        QVERIFY(mosaicAction->isEnabled());

        // Fotomosaik: Ordner eintippen, 6 Spalten, OK
        bool seen = false;
        QString found;
        QTimer poll;
        poll.setInterval(20);
        QObject::disconnect(&poll, &QTimer::timeout, nullptr, nullptr);
        QObject::connect(&poll, &QTimer::timeout, [&] {
            auto* dialog = qobject_cast<PhotoMosaicDialog*>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            poll.stop();
            seen = true;
            QTest::keyClicks(dialog->folderEdit(), QDir::toNativeSeparators(dir.path()));
            // Fertig mit Eingabe: unter Linux offscreen hat der Dialog keinen Fokus, Tab löst dort nichts aus
            emit dialog->folderEdit()->editingFinished();
            found = dialog->foundLabel()->text();
            dialog->columnsBox()->setValue(6);
            dialog->tintBox()->setValue(0);
            dialog->neighborsBox()->setChecked(false);
            if (const QString shots = qEnvironmentVariable("VEK_SCREENSHOT_DIR"); !shots.isEmpty()) {
                QTest::qWait(200);
                dialog->grab().save(shots + QStringLiteral("/92-fotomosaik-dialog.png"));
            }
            dialog->accept();
        });
        poll.start();
        const int undoBefore = ed.document().undoCount();
        mosaicAction->trigger();
        QVERIFY(seen);
        QCOMPARE(found, QStringLiteral("4 Bild(er) gefunden"));
        const auto* group = ed.document().findAs<GroupObject>(ed.selection().value(0));
        QVERIFY(group);
        QCOMPARE(group->childCount(), 18); // 6 x 3 Zellen
        QVERIFY(ed.document().findAs<ImageObject>(image)); // Original bleibt darunter
        QVERIFY(near(group->boundingRect(), imageBox, 0.01));
        // Linke Zellen rot, rechte blau
        for (int i = 0; i < group->childCount(); ++i) {
            const auto* tile = node_cast<ImageObject>(group->child(i));
            QVERIFY(tile);
            const QColor mean = mosaics::meanColor(tile->displayImage());
            const bool left = tile->boundingRect().center().x() < imageBox.center().x();
            QVERIFY2(left ? mean.red() > 180 && mean.blue() < 60 : mean.blue() > 180 && mean.red() < 60, qPrintable(mean.name()));
        }
        QCOMPARE(ed.document().undoCount(), undoBefore + 1);
        QCOMPARE(ed.document().undoText(), QStringLiteral("Fotomosaik"));
        ed.document().undo();
        QVERIFY(!ed.document().findAs<GroupObject>(group->id()));
        // Ordner wird gemerkt
        QCOMPARE(QDir(Options::instance().text(QStringLiteral("ws/tools/photoMosaic/folder"))), QDir(dir.path()));

        // Ausgabe als ein Bild, Original ersetzen
        ed.setSelection({image});
        seen = false;
        QObject::disconnect(&poll, &QTimer::timeout, nullptr, nullptr);
        QObject::connect(&poll, &QTimer::timeout, [&] {
            auto* dialog = qobject_cast<PhotoMosaicDialog*>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            poll.stop();
            seen = true;
            dialog->outputBox()->setCurrentIndex(1);
            dialog->replaceBox()->setChecked(true);
            dialog->accept();
        });
        poll.start();
        mosaicAction->trigger();
        QVERIFY(seen);
        const auto* single = ed.document().findAs<ImageObject>(ed.selection().value(0));
        QVERIFY(single && single->id() != image);
        QVERIFY(!ed.document().findAs<ImageObject>(image));
        ed.document().undo();
        QVERIFY(ed.document().findAs<ImageObject>(image));

        // Punktraster: 2-mm-Raster, Ergebnis je Farbe eine Gruppe
        ed.setSelection({image});
        seen = false;
        int count = 0;
        QObject::disconnect(&poll, &QTimer::timeout, nullptr, nullptr);
        QObject::connect(&poll, &QTimer::timeout, [&] {
            auto* dialog = qobject_cast<PointillizeDialog*>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            poll.stop();
            seen = true;
            dialog->cellBox()->setValue(imageBox.width() / 6.0);
            dialog->groupingBox()->setCurrentIndex(1);
            count = int(dialog->dots().size());
            if (const QString shots = qEnvironmentVariable("VEK_SCREENSHOT_DIR"); !shots.isEmpty()) {
                QTest::qWait(300);
                dialog->grab().save(shots + QStringLiteral("/93-punktraster-dialog.png"));
            }
            dialog->accept();
        });
        poll.start();
        pointAction->trigger();
        QVERIFY(seen);
        QCOMPARE(count, 18);
        const auto* dots = ed.document().findAs<GroupObject>(ed.selection().value(0));
        QVERIFY(dots);
        QCOMPARE(dots->childCount(), 2); // rot und blau
        int curves = 0;
        for (int i = 0; i < dots->childCount(); ++i) {
            const auto* sub = node_cast<GroupObject>(dots->child(i));
            QVERIFY(sub);
            for (int k = 0; k < sub->childCount(); ++k) {
                const auto* c = node_cast<CurveObject>(sub->child(k));
                QVERIFY(c && !c->style().stroke);
                ++curves;
            }
        }
        QCOMPARE(curves, 18);
        QVERIFY(dots->boundingRect().width() < imageBox.width() + 0.01);
        if (const QString shots = qEnvironmentVariable("VEK_SCREENSHOT_DIR"); !shots.isEmpty()) {
            ed.setSelection({});
            QTest::qWait(100);
            window.canvas()->grab().save(shots + QStringLiteral("/94-punktraster.png"));
        }
        ed.document().undo();
        QVERIFY(!ed.document().findAs<GroupObject>(dots->id()));
        // Eigene Form: Bild und ein Stern ausgewählt
        auto star = std::make_unique<PolygonObject>(QSizeF(10, 10), PolygonObject::Kind::Star, 5, 50);
        star->setTransform(QTransform::fromTranslate(10, 10));
        const NodeId starId = ed.addObject(std::move(star), QStringLiteral("Stern"));
        ed.setSelection({image, starId});
        seen = false;
        bool customEnabled = false;
        QObject::disconnect(&poll, &QTimer::timeout, nullptr, nullptr);
        QObject::connect(&poll, &QTimer::timeout, [&] {
            auto* dialog = qobject_cast<PointillizeDialog*>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            poll.stop();
            seen = true;
            customEnabled = dialog->shapeBox()->currentIndex() == 2;
            dialog->cellBox()->setValue(imageBox.width() / 6.0);
            dialog->groupingBox()->setCurrentIndex(2);
            dialog->keepBox()->setChecked(false);
            dialog->accept();
        });
        poll.start();
        pointAction->trigger();
        QVERIFY(seen && customEnabled);
        const auto* combined = ed.document().findAs<GroupObject>(ed.selection().value(0));
        QVERIFY(combined);
        QCOMPARE(combined->childCount(), 2); // je Farbe ein kombiniertes Objekt
        QCOMPARE(node_cast<CurveObject>(combined->child(0))->subPaths().size(), 9);
        QVERIFY(!ed.document().findAs<ImageObject>(image)); // Original nicht behalten
        QVERIFY(ed.document().find(starId));                 // Formvorlage bleibt
        ed.document().undo();
        QVERIFY(ed.document().findAs<ImageObject>(image));
        Options::instance().resetWorkspace();
    }

    void envelopeMappingFromDocker()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        Options::instance().resetWorkspace();
        // Ellipse: ihr Umriss liegt fast ganz im Inneren der Hülle (die Zuordnungen unterscheiden
        // sich dort, auf dem Rand der Hülle sind Frei und Kitt gleich)
        auto ellipse = std::make_unique<EllipseObject>(QSizeF(60, 40));
        ellipse->setTransform(QTransform::fromTranslate(30, 30));
        const NodeId r = m_editor->addObject(std::move(ellipse), QStringLiteral("E"));
        m_editor->setSelection({r});
        auto obj = [this, r] { return m_editor->document().findAs<DrawObject>(r); };
        EnvelopeDocker docker(*m_editor, *m_canvas);
        QVERIFY(docker.mappingBox()->count() == 5);
        // Neue Hülle, oberen Mittelgriff hochziehen
        QVERIFY(m_editor->applyEnvelopeShape({}, false, Envelope::Mode::Unconstrained, true, EnvelopeDocker::mapping()));
        QList<Envelope> list = obj()->envelopes();
        list.last().moveHandle(1, list.last().handles()[1] + QPointF(10, -15));
        list.last().moveHandle(3, list.last().handles()[3] + QPointF(12, 0));
        m_editor->setObjectEnvelopes(r, list, QStringLiteral("Hülle"));
        const QPainterPath standard = obj()->path();
        // Zuordnung im Andockfenster wählen: andere Form, ein Undo-Schritt
        QSet<QString> shapes{QString::number(standard.boundingRect().width())};
        for (int i = 1; i < docker.mappingBox()->count(); ++i) {
            const int undoBefore = m_editor->document().undoCount();
            docker.mappingBox()->setCurrentIndex(i);
            emit docker.mappingBox()->activated(i);
            QCOMPARE(int(obj()->envelopes().last().mapping), docker.mappingBox()->itemData(i).toInt());
            QCOMPARE(m_editor->document().undoCount(), undoBefore + 1);
            QVERIFY2(obj()->path() != standard, qPrintable(QStringLiteral("%1 %2 %3").arg(i).arg(describe(obj()->path().boundingRect()), describe(standard.boundingRect()))));
        }
        QCOMPARE(obj()->envelopes().last().mapping, Envelope::Mapping::Vertical);
        // Vertikal: senkrechte Kanten bleiben gerade (rechte Kante nicht ausgebeult)
        QVERIFY(obj()->path().boundingRect().right() < 90.01);
        m_editor->document().undo();
        QCOMPARE(obj()->envelopes().last().mapping, Envelope::Mapping::Putty);
        // Neue Hüllen übernehmen die gewählte Zuordnung
        docker.mappingBox()->setCurrentIndex(docker.mappingBox()->findData(int(Envelope::Mapping::Horizontal)));
        emit docker.mappingBox()->activated(docker.mappingBox()->currentIndex());
        QCOMPARE(EnvelopeDocker::mapping(), Envelope::Mapping::Horizontal);
        QVERIFY(docker.apply());
        QCOMPARE(obj()->envelopes().last().mapping, Envelope::Mapping::Horizontal);
        Options::instance().resetWorkspace();
    }

    void bevelToCenterDialog()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("VektoreditorTest"));
        MainWindow window;
        window.resize(1300, 900);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        Editor& ed = window.canvas()->editor();
        Canvas& canvas = *window.canvas();
        auto rect = std::make_unique<RectangleObject>(QSizeF(40, 40));
        rect->setTransform(QTransform::fromTranslate(50, 50));
        Style st;
        st.fill = Fill::solid(Color(60, 160, 60));
        rect->setStyle(st);
        const NodeId r = ed.addObject(std::move(rect), QStringLiteral("R"));
        ed.setSelection({r});
        auto render = [&canvas] {
            QImage img(200, 200, QImage::Format_ARGB32_Premultiplied);
            img.fill(Qt::white);
            QPainter p(&img);
            canvas.renderPage(p, 0, 4.0, -QPointF(45, 45) * 4.0, true);
            return img;
        };
        QAction* bevelAction = nullptr;
        for (QAction* a : window.findChildren<QAction*>())
            if (a->text() == QStringLiteral("Ab&schrägung …"))
                bevelAction = a;
        QVERIFY(bevelAction);
        auto runDialog = [&](bool toCenter) {
            bool seen = false;
            QTimer poll;
            poll.setInterval(20);
            QObject::disconnect(&poll, &QTimer::timeout, nullptr, nullptr);
        QObject::connect(&poll, &QTimer::timeout, [&] {
                auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
                if (!dialog)
                    return;
                auto* center = dialog->findChild<QRadioButton*>(QStringLiteral("bevelToCenter"));
                if (!center)
                    return;
                poll.stop();
                seen = true;
                if (toCenter)
                    center->click();
                dialog->accept();
            });
            poll.start();
            bevelAction->trigger();
            return seen;
        };
        QVERIFY(runDialog(false));
        QVERIFY(ed.document().findAs<DrawObject>(r)->bevel().isActive());
        QVERIFY(!ed.document().findAs<DrawObject>(r)->bevel().toCenter);
        const QImage distance = render();
        QVERIFY(runDialog(true));
        QVERIFY(ed.document().findAs<DrawObject>(r)->bevel().toCenter);
        const QImage center = render();
        if (const QString shots = qEnvironmentVariable("VEK_SCREENSHOT_DIR"); !shots.isEmpty()) {
            distance.save(shots + QStringLiteral("/95-abschraegung-abstand.png"));
            center.save(shots + QStringLiteral("/96-abschraegung-bis-mitte.png"));
        }
        // Abstand: flache Mitte in der Füllfarbe; bis Mitte: auch die Mitte schattiert, mit Grat
        // auf der Diagonalen (oben links hell, unten rechts dunkel bis ganz innen)
        const QPoint mid(100, 100), nearMidTL(90, 92), nearMidBR(110, 108);
        QCOMPARE(distance.pixelColor(nearMidTL), QColor(60, 160, 60));
        QCOMPARE(distance.pixelColor(nearMidBR), QColor(60, 160, 60));
        QVERIFY(center.pixelColor(nearMidTL) != center.pixelColor(nearMidBR));
        QVERIFY(center != distance);
        Q_UNUSED(mid);
        ed.document().undo();
        QVERIFY(!ed.document().findAs<DrawObject>(r)->bevel().toCenter);
    }

    void lensBitmapEffectChangesBelow()
    {
        auto add = [this](std::unique_ptr<DrawObject> o, const QRectF& r, std::optional<Color> fill) {
            o->setTransform(QTransform::fromTranslate(r.left(), r.top()));
            Style s;
            s.fill = fill ? Fill::solid(*fill) : Fill::none();
            s.stroke.reset();
            o->setStyle(s);
            return m_editor->addObject(std::move(o), QStringLiteral("O"));
        };
        auto render = [this] {
            QImage img(160, 160, QImage::Format_ARGB32_Premultiplied);
            img.fill(Qt::white);
            QPainter p(&img);
            m_canvas->renderPage(p, 0, 4.0, -QPointF(100, 100) * 4.0, true);
            return img;
        };
        // Streifen unter einem Kreis
        for (int i = 0; i < 10; ++i)
            add(std::make_unique<RectangleObject>(QSizeF(2, 40)), QRectF(100 + 4 * i, 100, 2, 40), Color(0, 0, 0));
        const NodeId ball = add(std::make_unique<EllipseObject>(QSizeF(30, 30)), QRectF(105, 105, 30, 30), std::nullopt);
        m_editor->setSelection({ball});
        const QImage plain = render();
        LensDocker docker(*m_editor);
        docker.setLensType(Lens::Type::BitmapEffect);
        QVERIFY(docker.effectBox()->isVisibleTo(&docker));
        QVERIFY(!docker.frozenBox()->isEnabled());
        docker.effectBox()->setCurrentIndex(docker.effectBox()->findData(QStringLiteral("blur")));
        docker.amountBox()->setValue(60);
        docker.apply();
        const Lens l = m_editor->document().findAs<DrawObject>(ball)->lens();
        QCOMPARE(l.type, Lens::Type::BitmapEffect);
        QCOMPARE(l.bitmapEffect, QStringLiteral("blur"));
        const QImage blurred = render();
        // In der Linse: Streifen verwischt (grau statt schwarz/weiss); ausserhalb unverändert
        const QPoint inside(80, 80), outside(4, 4);
        QVERIFY(plain.pixelColor(inside) == QColor(Qt::black) || plain.pixelColor(inside) == QColor(Qt::white));
        const QColor g = blurred.pixelColor(inside);
        QVERIFY2(g.red() > 40 && g.red() < 215, qPrintable(g.name()));
        QCOMPARE(blurred.pixelColor(outside), plain.pixelColor(outside));
        screenshot(QStringLiteral("97-linse-bitmap-effekt"));
        if (const QString shots = qEnvironmentVariable("VEK_SCREENSHOT_DIR"); !shots.isEmpty())
            blurred.save(shots + QStringLiteral("/97-linse-bitmap-effekt.png"));
        // Gespeichert und geladen
        auto loaded = io::loadFromBytes(io::saveToBytes(m_editor->document()));
        QVERIFY(loaded);
        QCOMPARE(loaded->findAs<DrawObject>(ball)->lens(), l);
        // Docker zeigt die gewählte Linse wieder an
        m_editor->setSelection({});
        m_editor->setSelection({ball});
        QCOMPARE(docker.effectBox()->currentData().toString(), QStringLiteral("blur"));
        m_editor->document().undo();
        QVERIFY(!m_editor->document().findAs<DrawObject>(ball)->lens().isActive());
    }

};

QTEST_MAIN(TestTools)
#include "tst_tools.moc"
