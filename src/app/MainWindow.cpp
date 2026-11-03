#include "MainWindow.h"

#include "Canvas.h"
#include "ColorDialog.h"
#include "ColorPalette.h"
#include "DocumentIO.h"
#include "FlyoutButton.h"
#include "GradientDialog.h"
#include "ImageObject.h"
#include "DisplayDepth.h"
#include "ImageDialogs.h"
#include "ImageSearch.h"
#include "ImageSearchDialog.h"
#include "ImagePool.h"
#include "TraceDialog.h"
#include "Icons.h"
#include "ObjectManager.h"
#include "Options.h"
#include "RectangleObject.h"
#include "EllipseObject.h"
#include "OptionsDialog.h"
#include "CutTools.h"
#include "ShapingDocker.h"
#include "EffectDockers.h"
#include "OutlineDialog.h"
#include "PatternDialog.h"
#include "ExportDialog.h"
#include "SvgImport.h"
#include "PdfImport.h"
#include "CdrImport.h"
#include "DxfImport.h"
#include "FontFile.h"
#include "FontMatchDialog.h"
#include "ImpositionDialog.h"
#include "PdfExport.h"
#include "PdfSettingsDialog.h"
#include "PickTool.h"
#include "PropertyBar.h"
#include "Output.h"
#include "SpellChecker.h"
#include "SpellDialog.h"
#include "AiPanel.h"
#include "ScriptDocker.h"
#include "AlignDialog.h"
#include "TransformDocker.h"
#include "EditDockers.h"
#include "ShapeTool.h"
#include "CloneObject.h"
#include "SuperClipObject.h"
#include "PerspectiveTool.h"
#include "TableObject.h"
#include "TableTool.h"
#include "BitmapArt.h"
#include "BitmapFilters.h"
#include "TextTool.h"
#include "Language.h"
#include "UiScale.h"
#include "Ruler.h"

#include <QActionGroup>
#include <QKeyEvent>
#include <QPushButton>
#include <QInputDialog>
#include <QUrl>
#include <QDesktopServices>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QPrintDialog>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QImageReader>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QProcess>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QScreen>
#include <QSettings>
#include <QDir>
#include <QStandardPaths>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTimer>
#include <QStatusBar>
#include <QTabBar>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <cmath>

#ifdef Q_OS_WIN
#include <shlobj.h>
#endif

namespace vek {

// Füllung und Kontur der Auswahl in der Statusleiste, wie unten rechts
class StyleIndicator : public QWidget {
public:
    explicit StyleIndicator(QWidget* parent = nullptr) : QWidget(parent)
    {
        setMinimumWidth(300);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    }
    void setStyle(const std::optional<Style>& style)
    {
        m_style = style;
        update();
    }
    QSize sizeHint() const override { return QSize(300, 2 * fontMetrics().height() + 4); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        const int lineH = height() / 2;
        auto describe = [](const std::optional<Color>& c) {
            return c ? QStringLiteral("R:%1 G:%2 B:%3 (%4)").arg(c->r).arg(c->g).arg(c->b).arg(c->toHex().toUpper())
                     : MainWindow::tr("Keine");
        };
        auto row = [&](int y, const QString& iconName, const std::optional<Color>& c, const QString& extra) {
            p.drawPixmap(0, y + (lineH - 16) / 2, icon(iconName).pixmap(16));
            const QRect swatch(22, y + (lineH - 12) / 2, 12, 12);
            if (c) {
                p.fillRect(swatch, c->toQColor());
            } else {
                p.fillRect(swatch, Qt::white);
                p.setPen(Qt::black);
                p.drawLine(swatch.topLeft(), swatch.bottomRight());
                p.drawLine(swatch.topRight(), swatch.bottomLeft());
            }
            p.setPen(QColor(120, 120, 120));
            p.drawRect(swatch.adjusted(0, 0, -1, -1));
            p.setPen(palette().color(QPalette::WindowText));
            p.drawText(QRect(40, y, width() - 40, lineH), Qt::AlignVCenter, describe(c) + extra);
        };
        if (!m_style)
            return;
        row(0, QStringLiteral("tool-fill"), m_style->fill.representativeColor(),
            m_style->fill.isGradient() ? MainWindow::tr("  Farbverlauf") : QString());
        row(lineH, QStringLiteral("tool-outline"), m_style->stroke,
            m_style->stroke ? QStringLiteral("  %1 mm").arg(QLocale().toString(m_style->strokeWidth, 'f', 3))
                            : QString());
    }

private:
    std::optional<Style> m_style;
};

namespace {

QString fileFilter()
{
    return MainWindow::tr("TANGENTA-Dokument (*.%1)").arg(QLatin1String(io::FileSuffix));
}

QString number(double v, int decimals = 3)
{
    return QLocale().toString(v, 'f', decimals);
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    {
        // Wörterbücher: neben dem Programm, unter share/tangenta (Linux), sonst im Quellbaum
        const QString app = QCoreApplication::applicationDirPath();
        SpellChecker& sc = SpellChecker::instance();
        if (sc.searchPaths().isEmpty())
            sc.setSearchPaths({app + QStringLiteral("/dictionaries"), app + QStringLiteral("/../share/tangenta/dictionaries"),
                               QStringLiteral(VEK_DICTIONARY_SOURCE)});
        sc.setUserDictionary(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                             + QStringLiteral("/benutzerwoerterbuch.txt"));
    }
    setDockOptions(QMainWindow::AllowTabbedDocks | QMainWindow::AnimatedDocks);
    // Leisten im klassischen Stil: flach, feine Trennlinien, kompakte Felder mit dünnem Rand.
    // Nur Leisten sind gemeint; Dialoge behalten den Systemstil.
    setStyleSheet(QStringLiteral(R"(
        QToolBar { background: #f2f3f5; border: none; border-bottom: 1px solid #d5d8dc; spacing: 1px; padding: 1px 3px; }
        QToolBar::separator { background: #cdd1d6; width: 1px; height: 1px; margin: 4px 5px; }
        QToolBar QToolButton { border: 1px solid transparent; border-radius: 2px; padding: 2px; background: transparent; }
        QToolBar QToolButton:hover { background: #e4ecf6; border-color: #b8cbe2; }
        QToolBar QToolButton:pressed, QToolBar QToolButton:checked { background: #d3e0f0; border-color: #91afd6; }
        QToolBar QToolButton:disabled { background: transparent; border-color: transparent; }
        QToolBar QFrame#barSeparator { background: #cdd1d6; }
        QToolBar QLabel { color: #3a3d42; }
        QToolBar QAbstractSpinBox, QToolBar QComboBox {
            background: #ffffff; border: 1px solid #b3b9c1; border-radius: 2px;
            padding: 0px 3px; min-height: 15px; selection-background-color: #3d7bd6; }
        QToolBar QAbstractSpinBox[stacked="true"] { padding: 1px 3px; min-height: 17px; }
        QToolBar QAbstractSpinBox[stacked="true"] { min-height: 13px; max-height: 13px; }
        QToolBar QAbstractSpinBox:focus, QToolBar QComboBox:focus, QToolBar QComboBox:on { border-color: #3d7bd6; }
        QToolBar QAbstractSpinBox:disabled, QToolBar QComboBox:disabled { background: #f2f3f5; color: #9aa0a8; border-color: #cdd1d6; }
        QToolBar QComboBox { padding-right: 16px; }
        QToolBar QComboBox::drop-down { width: 15px; border: none; border-left: 1px solid #dde0e4; }
        QToolBar QComboBox::down-arrow { image: url(:/icons/arrow-down.svg); width: 12px; height: 12px; }
        QToolBar QAbstractSpinBox::up-button, QToolBar QAbstractSpinBox::down-button {
            width: 13px; border: none; border-left: 1px solid #dde0e4; background: #f7f8f9; }
        QToolBar QAbstractSpinBox::up-button { subcontrol-position: top right; }
        QToolBar QAbstractSpinBox::down-button { subcontrol-position: bottom right; }
        QToolBar QAbstractSpinBox::up-button:hover, QToolBar QAbstractSpinBox::down-button:hover { background: #e4ecf6; }
        QToolBar QAbstractSpinBox::up-arrow { image: url(:/icons/arrow-up.svg); width: 10px; height: 10px; }
        QToolBar QAbstractSpinBox::down-arrow { image: url(:/icons/arrow-down.svg); width: 10px; height: 10px; }
        QToolBar QToolButton::menu-indicator { image: none; width: 0px; }
        QMenu { background: #ffffff; border: 1px solid #a9b0b9; padding: 3px 0px; }
        QMenu::item { padding: 4px 26px 4px 8px; color: #1d1d1d; border: 1px solid transparent; margin: 0px 3px; }
        QMenu::item:selected { background: #dfeaf7; border-color: #a9c3e3; border-radius: 2px; }
        QMenu::item:disabled { color: #9aa0a8; }
        QMenu::icon { padding-left: 6px; }
        QMenu::separator { height: 1px; background: #e1e4e8; margin: 3px 8px; }
        QMenu#flyout::item { padding: 5px 30px 5px 8px; }
        QMenu#flyout::indicator { width: 0px; }
        QMenu#flyout::icon:checked { background: #d3e0f0; border: 1px solid #91afd6; border-radius: 2px; }
    )"));
    setCentralWidget(createCanvasArea());
    createMenusAndToolbars();
    createToolbox();
    createDockers();

    // Farbpalette ganz rechts, ausserhalb der Andockfenster: als feste Leiste im
    // rechten Leistenbereich, der immer rechts vom Andockbereich liegt. Andockfenster bilden davor
    // senkrechte Reiter; die Palette gehört nie dazu.
    auto* colors = new ColorPalette(this);
    colors->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    auto* paletteBar = new QToolBar(tr("Farbpalette"), this);
    paletteBar->setObjectName(QStringLiteral("palette"));
    paletteBar->setMovable(false);
    paletteBar->setFloatable(false);
    paletteBar->setContextMenuPolicy(Qt::PreventContextMenu);
    paletteBar->setStyleSheet(QStringLiteral("QToolBar#palette { padding: 0px; spacing: 0px; border: none; }"));
    paletteBar->addWidget(colors);
    addToolBar(Qt::RightToolBarArea, paletteBar);
    // Erst dem Werkzeug anbieten (z.B. Farbstopp der interaktiven Füllung), sonst auf die Auswahl
    connect(colors, &ColorPalette::fillChosen, this, [this](std::optional<Color> c) {
        if (!m_canvas->offerPaletteColor(c, true))
            m_editor.setSelectionFill(c ? Fill::solid(*c) : Fill::none());
    });
    connect(colors, &ColorPalette::strokeChosen, this, [this](std::optional<Color> c) {
        if (!m_canvas->offerPaletteColor(c, false))
            m_editor.setSelectionStroke(c);
    });

    // Statusleiste: Mausposition | Objektinfo | Füllung/Kontur
    m_positionLabel = new QLabel(this);
    m_positionLabel->setMinimumWidth(180);
    m_infoLabel = new QLabel(this);
    m_infoLabel->setAlignment(Qt::AlignCenter);
    m_styleIndicator = new StyleIndicator(this);
    statusBar()->addWidget(m_positionLabel);
    statusBar()->addWidget(m_infoLabel, 1);
    statusBar()->addPermanentWidget(m_styleIndicator);

    connect(m_canvas, &Canvas::cursorMoved, this, [this](const QPointF& doc) {
        if (std::isnan(doc.x()))
            m_positionLabel->clear();
        else
            m_positionLabel->setText(Options::instance().positionText(doc));
    });
    connect(m_canvas, &Canvas::viewChanged, this, &MainWindow::updateZoomCombo);
    connect(m_canvas, &Canvas::contextMenuRequested, this, &MainWindow::showObjectContextMenu);
    connect(m_canvas, &Canvas::guideContextMenuRequested, this, &MainWindow::showGuideContextMenu);
    connect(&m_editor, &Editor::documentChanged, this, [this] {
        rebuildPageTabs();
        updateActions();
        updateStatus();
    });
    connect(&m_editor, &Editor::documentReplaced, this, [this] {
        rebuildPageTabs();
        updateActions();
        updateTitle();
        updateStatus();
    });
    connect(&m_editor, &Editor::selectionChanged, this, [this] {
        updateActions();
        updateStatus();
    });
    connect(&m_editor, &Editor::currentPageChanged, this, [this] {
        rebuildPageTabs();
        updateStatus();
    });
    connect(&m_editor, &Editor::modifiedChanged, this, [this] { updateTitle(); });
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, &MainWindow::updateActions);

    rebuildPageTabs();
    updateActions();
    updateTitle();
    updateStatus();

    // Optionen: Vorgaben der Menübefehle merken, eigene Tastenkürzel und Einstellungen anwenden
    Options::instance().attach(&m_editor);
    connect(&Options::instance(), &Options::changed, this, &MainWindow::applyOptions);
    applyShortcuts();
    applyOptions();

    QSettings settings;
    m_editor.shapeDefaults().blurSteps = settings.value(QStringLiteral("bake/steps"), 48).toInt();
    m_editor.shapeDefaults().blurBands = settings.value(QStringLiteral("bake/bands"), 32).toInt();
    if (!restoreGeometry(settings.value(QStringLiteral("window/geometry")).toByteArray()))
        resize(1400, 900);
    restoreState(settings.value(QStringLiteral("window/state")).toByteArray(), 3); // 3: Palette nicht mehr als Andockfenster
    m_canvas->setFocus();
}

MainWindow::~MainWindow()
{
    // Ein Feld der Eigenschaftsleiste übernimmt seine Eingabe beim Fokusverlust und schreibt dabei
    // in m_editor. Das muss jetzt geschehen: Beim Schliessen in ~QWidget gibt es m_editor nicht mehr.
    if (QWidget* f = QApplication::focusWidget(); f && isAncestorOf(f))
        f->clearFocus();
}

QWidget* MainWindow::createCanvasArea()
{
    m_canvas = new Canvas(m_editor, this);
    m_hRuler = new Ruler(Qt::Horizontal, *m_canvas, this);
    m_vRuler = new Ruler(Qt::Vertical, *m_canvas, this);
    m_rulerCorner = new RulerCorner(this, m_canvas);
    connect(m_rulerCorner, &RulerCorner::optionsRequested, this, [this] {
        OptionsDialog dialog(m_editor, this, this);
        dialog.selectPage(tr("Dokument/Lineale"));
        dialog.exec();
    });

    auto* area = new QWidget(this);
    auto* grid = new QGridLayout(area);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(0);
    grid->addWidget(m_rulerCorner, 0, 0);
    grid->addWidget(m_hRuler, 0, 1);
    grid->addWidget(m_vRuler, 1, 0);
    grid->addWidget(m_canvas, 1, 1);
    grid->addWidget(createPageNavigator(), 2, 0, 1, 2);
    grid->setRowStretch(1, 1);
    grid->setColumnStretch(1, 1);
    return area;
}

QWidget* MainWindow::createPageNavigator()
{
    auto* bar = new QWidget(this);
    auto* row = new QHBoxLayout(bar);
    row->setContentsMargins(4, 1, 4, 1);
    row->setSpacing(1);
    auto button = [&](const QString& iconName, const QString& tip, auto slot) {
        auto* b = new QToolButton(bar);
        b->setIcon(icon(iconName));
        b->setIconSize(QSize(16, 16));
        b->setToolTip(tip);
        b->setAutoRaise(true);
        connect(b, &QToolButton::clicked, this, slot);
        row->addWidget(b);
        return b;
    };
    button(QStringLiteral("page-insert"), tr("Seite davor einfügen"),
           [this] { m_editor.insertPage(m_editor.currentPageIndex()); });
    button(QStringLiteral("nav-first"), tr("Erste Seite"), [this] { m_editor.setCurrentPageIndex(0); });
    button(QStringLiteral("nav-prev"), tr("Vorige Seite"),
           [this] { m_editor.setCurrentPageIndex(m_editor.currentPageIndex() - 1); });
    m_pageLabel = new QLabel(bar);
    m_pageLabel->setMinimumWidth(60);
    m_pageLabel->setAlignment(Qt::AlignCenter);
    row->addWidget(m_pageLabel);
    button(QStringLiteral("nav-next"), tr("Nächste Seite"),
           [this] { m_editor.setCurrentPageIndex(m_editor.currentPageIndex() + 1); });
    button(QStringLiteral("nav-last"), tr("Letzte Seite"),
           [this] { m_editor.setCurrentPageIndex(m_editor.document().pageCount() - 1); });
    button(QStringLiteral("page-insert"), tr("Seite danach einfügen"),
           [this] { m_editor.insertPage(m_editor.currentPageIndex() + 1); });

    m_pageTabs = new QTabBar(bar);
    m_pageTabs->setShape(QTabBar::RoundedSouth);
    m_pageTabs->setExpanding(false);
    m_pageTabs->setDrawBase(false);
    m_pageTabs->setContextMenuPolicy(Qt::CustomContextMenu);
    row->addSpacing(6);
    row->addWidget(m_pageTabs);
    row->addStretch(1);
    connect(m_pageTabs, &QTabBar::currentChanged, this, [this](int index) {
        if (index >= 0)
            m_editor.setCurrentPageIndex(index);
    });
    connect(m_pageTabs, &QTabBar::customContextMenuRequested, this, [this](const QPoint& pos) {
        const int index = m_pageTabs->tabAt(pos);
        if (index < 0)
            return;
        QMenu menu;
        menu.addAction(tr("Seite davor einfügen"), this, [this, index] { m_editor.insertPage(index); });
        menu.addAction(tr("Seite danach einfügen"), this, [this, index] { m_editor.insertPage(index + 1); });
        QAction* del = menu.addAction(tr("Seite löschen"), this, [this, index] { m_editor.deletePage(index); });
        del->setEnabled(m_editor.document().pageCount() > 1);
        menu.exec(m_pageTabs->mapToGlobal(pos));
    });
    return bar;
}

void MainWindow::createMenusAndToolbars()
{
    auto action = [this](const QString& iconName, const QString& text, const QKeySequence& key, auto slot) {
        auto* a = new QAction(iconName.isEmpty() ? QIcon() : icon(iconName), text, this);
        if (!key.isEmpty())
            a->setShortcut(key);
        connect(a, &QAction::triggered, this, slot);
        return a;
    };

    QAction* newDoc = action(QStringLiteral("doc-new"), tr("&Neu"), QKeySequence::New, [this] { newDocument(); });
    QAction* openDoc = action(QStringLiteral("doc-open"), tr("Ö&ffnen …"), QKeySequence::Open, [this] { open(); });
    QAction* saveDoc = action(QStringLiteral("doc-save"), tr("&Speichern"), QKeySequence::Save, [this] { save(); });
    QAction* saveDocAs = action(QString(), tr("Speichern &unter …"), QKeySequence::SaveAs, [this] { saveAs(); });
    QAction* importDoc = action(QStringLiteral("doc-import"), tr("&Importieren …"), QKeySequence(Qt::CTRL | Qt::Key_I),
                                [this] { importFiles(); });
    QAction* imageSearch = action(QString(), tr("&Bildersuche …"), QKeySequence(), [this] { searchImages(); });
    QAction* exportDoc = action(QStringLiteral("doc-export"), tr("&Exportieren …"), QKeySequence(Qt::CTRL | Qt::Key_E),
                                [this] { exportFile(); });
    QAction* html = action(QString(), tr("Im Web veröffentlichen: &HTML …"), QKeySequence(), [this] { publishHtml(); });
    QAction* pdf = action(QStringLiteral("doc-pdf"), tr("Als &PDF freigeben …"), QKeySequence(), [this] { sharePdf(); });
    QAction* printDoc = action(QStringLiteral("doc-print"), tr("&Drucken …"), QKeySequence::Print, [this] { printDocument(); });
    QAction* preview = action(QString(), tr("Druck&vorschau …"), QKeySequence(), [this] { printPreview(); });
    QAction* imposeDoc = action(QStringLiteral("doc-impose"), tr("&Ausschiessen …"), QKeySequence(), [this] {
        ImpositionDialog dialog(*m_canvas, m_editor.filePath().isEmpty() ? tr("Unbenannt") : QFileInfo(m_editor.filePath()).completeBaseName(), this);
        dialog.exec();
    });
    // Alt+F4 statt Strg+Q: Strg+Q ist "In Kurven umwandeln"
    QAction* quit = action(QString(), tr("&Beenden"), QKeySequence(Qt::ALT | Qt::Key_F4), [this] { close(); });

    m_undo = action(QStringLiteral("edit-undo"), tr("&Rückgängig"), QKeySequence::Undo,
                    [this] { m_editor.document().undo(); });
    m_redo = action(QStringLiteral("edit-redo"), tr("&Wiederholen"), QKeySequence::Redo,
                    [this] { m_editor.document().redo(); });
    QAction* cut = action(QStringLiteral("edit-cut"), tr("&Ausschneiden"), QKeySequence::Cut,
                          [this] { m_editor.cutSelection(); });
    QAction* copy = action(QStringLiteral("edit-copy"), tr("&Kopieren"), QKeySequence::Copy,
                           [this] { m_editor.copySelection(); });
    m_paste = action(QStringLiteral("edit-paste"), tr("&Einfügen"), QKeySequence::Paste, [this] { m_editor.paste(); });
    QAction* duplicate = action(QString(), tr("&Duplizieren"), QKeySequence(Qt::CTRL | Qt::Key_D),
                                [this] { m_editor.duplicateSelection(); });
    QAction* del = action(QStringLiteral("edit-delete"), tr("&Löschen"), QKeySequence::Delete, [this] { deleteSelectionOrGuide(); });
    QAction* selectAll = action(QString(), tr("Alles &auswählen"), QKeySequence::SelectAll,
                                [this] { m_editor.selectAll(); });
    m_selectionActions = {cut, copy, duplicate};
    m_cut = cut;
    m_copy = copy;
    m_delete = del;

    // --- Menüs ---
    QMenu* file = menuBar()->addMenu(tr("&Datei"));
    file->addActions({newDoc, openDoc, saveDoc, saveDocAs});
    file->addSeparator();
    file->addActions({importDoc, imageSearch, exportDoc, html, pdf});
    file->addSeparator();
    file->addActions({printDoc, preview, imposeDoc});
    file->addSeparator();
    file->addAction(quit);

    QMenu* edit = menuBar()->addMenu(tr("&Bearbeiten"));
    edit->addActions({m_undo, m_redo});
    // Wiederholen: letzten Befehl auf die aktuelle Auswahl
    QAction* repeat = edit->addAction(icon(QStringLiteral("edit-repeat")), tr("W&iederholen"), QKeySequence(Qt::CTRL | Qt::Key_R),
                                      this, [this] { m_editor.repeatLast(); });
    connect(edit, &QMenu::aboutToShow, this, [this, repeat] {
        repeat->setEnabled(m_editor.canRepeat());
        repeat->setText(m_editor.canRepeat() ? tr("W&iederholen: %1").arg(m_editor.repeatLabel()) : tr("W&iederholen"));
    });
    edit->addSeparator();
    edit->addActions({cut, copy, m_paste, del});
    QMenu* symbolMenu = edit->addMenu(icon(QStringLiteral("symbol")), tr("S&ymbol"));
    m_selectionActions << symbolMenu->addAction(tr("&Neues Symbol …"), this, [this] {
        bool ok = false;
        const QString name = QInputDialog::getText(this, tr("Neues Symbol"), tr("Name:"), QLineEdit::Normal,
                                                   tr("Symbol %1").arg(m_editor.symbols().size() + 1), &ok);
        if (ok && !name.trimmed().isEmpty())
            m_editor.createSymbol(name.trimmed());
    });
    m_selectionActions << symbolMenu->addAction(tr("In &Objekte zurückwandeln"), this, [this] { m_editor.convertClonesToObjects(); });
    symbolMenu->addSeparator();
    symbolMenu->addAction(tr("Symbol-&Manager"), this, [this] {
        m_symbolDock->show();
        m_symbolDock->raise();
    });
    edit->addSeparator();
    edit->addAction(duplicate);
    m_selectionActions << edit->addAction(icon(QStringLiteral("edit-clone")), tr("&Klonen"), this, [this] { m_editor.cloneSelection(); });
    m_selectionActions << edit->addAction(tr("&Eigenschaften kopieren von …"), this, [this] { copyPropertiesDialog(); });
    QAction* multi = edit->addAction(tr("&Vielfach duplizieren …"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D), this,
                                     [this] { multiDuplicate(); });
    m_selectionActions << multi;
    edit->addSeparator();
    QMenu* selectMenu = edit->addMenu(tr("Alles a&uswählen"));
    selectMenu->addAction(selectAll);
    selectMenu->addAction(tr("&Text"), this, [this] { m_editor.selectAllText(); });
    QAction* allNodes = selectMenu->addAction(tr("&Knoten"), QKeySequence(QKeyCombination(Qt::CTRL | Qt::SHIFT | Qt::ALT, Qt::Key_A)), this, [this] {
        ShapeTool& shape = m_canvas->shapeTool();
        if (const CurveObject* c = shape.curve()) {
            QList<NodeRef> all;
            for (int p = 0; p < c->subPaths().size(); ++p)
                for (int n = 0; n < c->subPaths()[p].nodes.size(); ++n)
                    all << NodeRef{p, n};
            shape.setSelectedNodes(all);
            m_canvas->update();
        }
    });
    connect(selectMenu, &QMenu::aboutToShow, this, [this, allNodes] {
        allNodes->setEnabled(m_canvas->toolId() == ToolId::Shape && m_canvas->shapeTool().curve());
    });
    QMenu* findMenu = edit->addMenu(tr("&Suchen und ersetzen"));
    findMenu->addAction(tr("Objekte &suchen …"), this, [this] { findObjectsDialog(); });
    findMenu->addAction(tr("Text &ersetzen …"), this, [this] { replaceTextDialog(); });
    edit->addSeparator();
    edit->addAction(icon(QStringLiteral("object-properties")), tr("&Objekteigenschaften"), QKeySequence(Qt::ALT | Qt::Key_Return), this, [this] {
        m_propertiesDock->show();
        m_propertiesDock->raise();
    });

    QMenu* view = menuBar()->addMenu(tr("&Ansicht"));
    // Darstellungsarten (Reihenfolge des Menüs, Wert = doc/general/viewMode)
    {
        auto* modes = new QActionGroup(view);
        const QList<std::pair<QString, int>> entries{{tr("Ein&facher Umriss"), 2}, {tr("&Umriss"), 3}, {tr("&Entwurf"), 0},
                                                    {tr("&Normal"), 4}, {tr("Er&weitert"), 1}, {tr("&Pixel"), 5}};
        QList<QAction*> modeActions;
        for (const auto& [text, value] : entries) {
            QAction* a = view->addAction(text);
            a->setCheckable(true);
            a->setData(value);
            modes->addAction(a);
            modeActions << a;
            connect(a, &QAction::triggered, this, [v = value] { Options::instance().set(QStringLiteral("doc/general/viewMode"), v); });
        }
        connect(view, &QMenu::aboutToShow, this, [modeActions] {
            const int now = Options::instance().integer(QStringLiteral("doc/general/viewMode"));
            for (QAction* a : modeActions)
                a->setChecked(a->data().toInt() == now);
        });
        view->addSeparator();
        view->addAction(icon(QStringLiteral("view-fullscreen")), tr("&Ganzseitenvorschau"), QKeySequence(Qt::Key_F9), this,
                        [this] { showFullPreview(false); });
        m_selectionActions << view->addAction(tr("&Vorschau markierter Objekte"), this, [this] { showFullPreview(true); });
        view->addSeparator();
    }
    view->addAction(action(QStringLiteral("zoom-in"), tr("Ver&grössern"), QKeySequence::ZoomIn,
                           [this] { m_canvas->zoomIn(); }));
    view->addAction(action(QStringLiteral("zoom-out"), tr("Ver&kleinern"), QKeySequence::ZoomOut,
                           [this] { m_canvas->zoomOut(); }));
    view->addAction(action(QStringLiteral("zoom-page"), tr("&Seite einpassen"), QKeySequence(Qt::SHIFT | Qt::Key_F4),
                           [this] { m_canvas->fitPage(); }));
    view->addSeparator();
    // Lineale, Hilfslinien, Gitter und "Ausrichten an" sind Optionen (Extras > Optionen)
    auto optionToggle = [this](QMenu* menu, const QString& text, const QString& key) {
        QAction* a = menu->addAction(text);
        a->setCheckable(true);
        a->setData(key);
        connect(a, &QAction::triggered, this, [key](bool on) { Options::instance().set(key, on); });
        return a;
    };
    m_rulersAction = optionToggle(view, tr("&Lineale"), QStringLiteral("ws/custom/bar/rulers"));
    m_guidesAction = optionToggle(view, tr("&Hilfslinien"), QStringLiteral("doc/guides/show"));
    m_snapActions << optionToggle(view, tr("&Gitter"), QStringLiteral("doc/grid/show"));

    // "Ausrichten an"
    auto* snapMenu = new QMenu(tr("A&usrichten an"), this);
    m_snapActions << optionToggle(snapMenu, tr("&Hilfslinien"), QStringLiteral("doc/guides/snap"))
                  << optionToggle(snapMenu, tr("&Objekte"), QStringLiteral("ws/snap/enabled"))
                  << optionToggle(snapMenu, tr("&Seite"), QStringLiteral("ws/snap/page"))
                  << optionToggle(snapMenu, tr("G&itter"), QStringLiteral("doc/grid/snap"));
    snapMenu->addSeparator();
    QAction* dyn = optionToggle(snapMenu, tr("&Dynamische Hilfslinien"), QStringLiteral("ws/dynguides/enabled"));
    dyn->setShortcut(QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_D));
    QAction* alignGuides = optionToggle(snapMenu, tr("Aus&richtungshilfslinien"), QStringLiteral("ws/alignguides/enabled"));
    alignGuides->setShortcut(QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_A));
    m_snapActions << dyn << alignGuides;
    m_snapActions[2]->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Z)); // An Objekten ausrichten
    snapMenu->addSeparator();
    snapMenu->addAction(tr("Einstellungen …"), this, [this] {
        OptionsDialog dialog(m_editor, this, this);
        dialog.selectPage(tr("Arbeitsbereich/An Objekten ausrichten"));
        dialog.exec();
    });
    view->addMenu(snapMenu);

    // Was der Bildschirm kann und wie TANGENTA Bilder darauf ausgibt
    view->addSeparator();
    view->addAction(tr("&Farbtiefe des Bildschirms …"), this, [this] {
        QStringList lines;
        for (QScreen* screen : QGuiApplication::screens()) {
            const DisplayDepth d = displayDepth(screen);
            lines << tr("%1: %2 Bit je Farbkanal%3 (%4)%5")
                         .arg(d.name)
                         .arg(d.bitsPerColor)
                         .arg(d.hdr ? tr(", HDR") : QString(), d.source,
                              screen == this->screen() ? tr(" – dieses Fenster") : QString());
        }
        QMessageBox::information(
            this, tr("Farbtiefe des Bildschirms"),
            lines.join(QLatin1Char('\n'))
                + tr("\n\nBilder rechnet TANGENTA intern mit 16 Bit je Kanal (Kamera-Rohdaten, 16-Bit-TIFF/PNG). "
                     "Am Bildschirm werden feinere Abstufungen als 8 Bit gerastert, damit Verläufe keine "
                     "Stufen zeigen."));
    });

    // Oberflächengrösse: wirkt nach einem Neustart
    view->addSeparator();
    QMenu* sizeMenu = view->addMenu(tr("&Oberflächengrösse"));
    const InterfaceScale current = interfaceScale();
    QAction* fit = sizeMenu->addAction(
        tr("An &Bildschirmgrösse anpassen (jetzt %1 %)").arg(qRound(devicePixelRatioF() * 100)));
    fit->setCheckable(true);
    fit->setChecked(current.fitScreen);
    fit->setToolTip(tr("Symbole, Leisten und Farbpalette sind auf jedem Bildschirm gleich gross"));
    connect(fit, &QAction::toggled, this, [this](bool on) {
        InterfaceScale s = interfaceScale();
        s.fitScreen = on;
        setInterfaceScale(s);
        offerRestart();
    });
    sizeMenu->addSeparator();
    auto* steps = new QActionGroup(sizeMenu);
    for (double step : interfaceScaleSteps()) {
        QAction* a = sizeMenu->addAction(tr("%1 %").arg(qRound(step * 100)));
        a->setCheckable(true);
        a->setChecked(std::abs(step - current.factor) < 1e-6);
        steps->addAction(a);
        connect(a, &QAction::triggered, this, [this, step] {
            InterfaceScale s = interfaceScale();
            s.factor = step;
            setInterfaceScale(s);
            offerRestart();
        });
    }

    // Sprache: wirkt nach einem Neustart. "Language" steht immer dabei, damit man auch aus
    // einer fremden Sprache zurückfindet.
    const QString languageTitle = tr("&Sprache");
    QMenu* languageMenu = view->addMenu(activeLanguage() == QLatin1String("en")
                                            ? languageTitle
                                            : languageTitle + QStringLiteral(" (Language)"));
    auto* languageGroup = new QActionGroup(languageMenu);
    QAction* systemLanguage = languageMenu->addAction(tr("Wie das &System"));
    systemLanguage->setData(QString());
    languageMenu->addSeparator();
    for (const Language& l : languages()) {
        QAction* a = languageMenu->addAction(l.name);
        a->setData(l.code);
    }
    for (QAction* a : languageMenu->actions()) {
        if (a->isSeparator())
            continue;
        a->setCheckable(true);
        a->setChecked(a->data().toString() == languageSetting());
        languageGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, code = a->data().toString()] {
            if (code == languageSetting())
                return;
            setLanguageSetting(code);
            offerRestart(tr("Sprache"), tr("Die neue Sprache gilt ab dem nächsten Start. Jetzt neu starten?"));
        });
    }

    QMenu* layoutMenu = menuBar()->addMenu(tr("&Layout"));
    layoutMenu->addAction(tr("Seite &einfügen"), this, [this] { m_editor.insertPage(m_editor.currentPageIndex() + 1); });
    layoutMenu->addAction(tr("Seite &löschen"), this, [this] { m_editor.deletePage(m_editor.currentPageIndex()); });

    // Menü "Anordnen"
    QMenu* object = menuBar()->addMenu(tr("An&ordnen"));
    m_selectionActions << object->addAction(tr("&Gruppieren"), QKeySequence(Qt::CTRL | Qt::Key_G), this,
                                            [this] { m_editor.groupSelection(); });
    m_selectionActions << object->addAction(tr("Gruppierung &aufheben"), QKeySequence(Qt::CTRL | Qt::Key_U), this,
                                            [this] { m_editor.ungroupSelection(); });
    m_selectionActions << object->addAction(tr("Gruppierung a&ller Objekte aufheben"), this, [this] { m_editor.ungroupAll(); });
    object->addSeparator();
    m_toCurves = object->addAction(icon(QStringLiteral("to-curves")), tr("In &Kurven konvertieren"),
                                   QKeySequence(Qt::CTRL | Qt::Key_Q), this, [this] { m_editor.convertSelectionToCurves(); });
    m_combine = object->addAction(icon(QStringLiteral("combine")), tr("&Kombinieren"), QKeySequence(Qt::CTRL | Qt::Key_L),
                                  this, [this] { m_editor.combineSelection(); });
    m_breakApart = object->addAction(icon(QStringLiteral("break-apart")), tr("Kom&bination aufheben"),
                                     QKeySequence(Qt::CTRL | Qt::Key_K), this, [this] {
                                         // Überblendung/Kontur/Extrusion trennen, sonst Kombination aufheben
                                         if (!m_editor.separateEffects())
                                             m_editor.breakApartSelection();
                                     });
    m_selectionActions << object->addAction(icon(QStringLiteral("outline-to-object")), tr("&Umriss in Objekt umwandeln"),
                                            QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Q), this,
                                            [this] { m_editor.outlineToObject(); });
    m_selectionActions << object->addAction(icon(QStringLiteral("join-curves")), tr("Kurven &verbinden"), this,
                                            [this] { m_editor.joinCurves(Options::instance().number(QStringLiteral("ws/edit/joinTolerance"))); });
    // Ausrichten und verteilen (Anordnen > Ausrichten und verteilen)
    {
        QMenu* alignMenu = object->addMenu(icon(QStringLiteral("align-dialog")), tr("Aus&richten und verteilen"));
        using H = align::H;
        using V = align::V;
        struct Entry {
            QString iconName;
            QString text;
            QKeySequence key;
            H h;
            V v;
            Editor::AlignTo to;
        };
        const QList<Entry> entries{
            {QStringLiteral("obj-align-left"), tr("&Linksbündig ausrichten"), QKeySequence(Qt::Key_L), H::Left, V::None, Editor::AlignTo::ActiveObjects},
            {QStringLiteral("obj-align-right"), tr("&Rechtsbündig ausrichten"), QKeySequence(Qt::Key_R), H::Right, V::None, Editor::AlignTo::ActiveObjects},
            {QStringLiteral("obj-align-top"), tr("&Oben ausrichten"), QKeySequence(Qt::Key_T), H::None, V::Top, Editor::AlignTo::ActiveObjects},
            {QStringLiteral("obj-align-bottom"), tr("&Unten ausrichten"), QKeySequence(Qt::Key_B), H::None, V::Bottom, Editor::AlignTo::ActiveObjects},
            {QStringLiteral("obj-align-hcenter"), tr("&Horizontal zentrieren"), QKeySequence(Qt::Key_E), H::Center, V::None, Editor::AlignTo::ActiveObjects},
            {QStringLiteral("obj-align-vcenter"), tr("&Vertikal zentrieren"), QKeySequence(Qt::Key_C), H::None, V::Center, Editor::AlignTo::ActiveObjects},
            {QStringLiteral("obj-align-page"), tr("&Mittelpunkt auf Seite"), QKeySequence(Qt::Key_P), H::Center, V::Center, Editor::AlignTo::PageEdge},
            {QStringLiteral("obj-align-page-h"), tr("Mittelpunkt &horizontal auf Seite"), QKeySequence(), H::Center, V::None, Editor::AlignTo::PageEdge},
            {QStringLiteral("obj-align-page-v"), tr("Mittelpunkt v&ertikal auf Seite"), QKeySequence(), H::None, V::Center, Editor::AlignTo::PageEdge},
        };
        for (int i = 0; i < entries.size(); ++i) {
            const Entry& e = entries[i];
            if (i == 6)
                alignMenu->addSeparator();
            QAction* a = alignMenu->addAction(icon(e.iconName), e.text, this,
                                              [this, h = e.h, v = e.v, to = e.to] { m_editor.alignSelection(h, v, to); });
            a->setShortcut(e.key);
            m_selectionActions << a;
        }
        alignMenu->addSeparator();
        QAction* dialog = alignMenu->addAction(icon(QStringLiteral("align-dialog")), tr("&Ausrichten und verteilen …"), this,
                                               [this] { showAlignDialog(0); });
        dialog->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A));
    }
    // Kommen mit dem Text- und dem Verbindungswerkzeug
    m_wrapText = new QAction(icon(QStringLiteral("wrap-text")), tr("&Mengentext umbrechen – folgt mit dem Textwerkzeug"), this);
    m_wrapText->setEnabled(false);
    m_breakConnector = new QAction(icon(QStringLiteral("break-connector")),
                                   tr("&Verbindung trennen"), this);
    m_breakConnector->setEnabled(false);
    connect(m_breakConnector, &QAction::triggered, this, [this] { m_editor.breakConnectors(); });
    object->addSeparator();

    // Anordnung mit den Kurzbefehlen
    // Formen: direkt auf die Auswahl oder über das Andockfenster
    QMenu* shaping = object->addMenu(icon(QStringLiteral("shape-weld")), tr("&Formen"));
    using Op = Editor::ShapeOp;
    for (auto [op, iconName] : {std::pair{Op::Weld, QStringLiteral("shape-weld")}, std::pair{Op::Trim, QStringLiteral("shape-trim")},
                                std::pair{Op::Intersect, QStringLiteral("shape-intersect")},
                                std::pair{Op::Simplify, QStringLiteral("shape-simplify")},
                                std::pair{Op::FrontMinusBack, QStringLiteral("shape-front-minus-back")},
                                std::pair{Op::BackMinusFront, QStringLiteral("shape-back-minus-front")},
                                std::pair{Op::Boundary, QStringLiteral("shape-boundary")}}) {
        m_shapeActions.insert(int(op), shaping->addAction(icon(iconName), Editor::shapeOpName(op), this,
                                                         [this, op] { m_editor.shapeSelection(op); }));
    }
    shaping->addSeparator();
    shaping->addAction(tr("F&ormen …"), this, [this] {
        m_shapingDock->show();
        m_shapingDock->raise();
    });
    m_arrangeMenu = object->addMenu(icon(QStringLiteral("arrange-front")), tr("A&nordnung"));
    auto arrange = [this](Editor::Arrange how, const QString& iconName, const QString& text, const QKeySequence& key) {
        QAction* a = m_arrangeMenu->addAction(icon(iconName), text, key, this, [this, how] { m_editor.arrangeSelection(how); });
        m_arrangeActions.insert(int(how), a);
    };
    using A = Editor::Arrange;
    arrange(A::ToPageFront, QStringLiteral("arrange-page-front"), tr("Nach &vorn auf der Seite"),
            QKeySequence(Qt::CTRL | Qt::Key_Home));
    arrange(A::ToPageBack, QStringLiteral("arrange-page-back"), tr("Nach &hinten auf der Seite"),
            QKeySequence(Qt::CTRL | Qt::Key_End));
    m_arrangeMenu->addSeparator();
    arrange(A::ToLayerFront, QStringLiteral("arrange-front"), tr("Nach vorn auf der &Ebene"),
            QKeySequence(Qt::SHIFT | Qt::Key_PageUp));
    arrange(A::ToLayerBack, QStringLiteral("arrange-back"), tr("Nach hinten auf der E&bene"),
            QKeySequence(Qt::SHIFT | Qt::Key_PageDown));
    arrange(A::ForwardOne, QStringLiteral("arrange-forward"), tr("&Eins nach vorn"), QKeySequence(Qt::CTRL | Qt::Key_PageUp));
    arrange(A::BackOne, QStringLiteral("arrange-backward"), tr("Eins nach h&inten"),
            QKeySequence(Qt::CTRL | Qt::Key_PageDown));
    m_arrangeMenu->addSeparator();
    m_placeActions << m_arrangeMenu->addAction(icon(QStringLiteral("arrange-before")), tr("V&or …"), this, [this] {
        m_canvas->setTool(ToolId::Pick);
        m_canvas->pickTool().beginPlaceRelative(true);
    });
    m_placeActions << m_arrangeMenu->addAction(icon(QStringLiteral("arrange-behind")), tr("Hin&ter …"), this, [this] {
        m_canvas->setTool(ToolId::Pick);
        m_canvas->pickTool().beginPlaceRelative(false);
    });
    m_arrangeMenu->addSeparator();
    arrange(A::Reverse, QStringLiteral("arrange-reverse"), tr("&Reihenfolge umkehren"), QKeySequence());

    m_lock = object->addAction(icon(QStringLiteral("lock")), tr("Objekt &sperren"), this,
                               [this] { m_editor.setSelectionLocked(true); });
    m_unlock = object->addAction(icon(QStringLiteral("unlock")), tr("Objekt &entsperren"), this,
                                 [this] { m_editor.setSelectionLocked(false); });
    object->addAction(icon(QStringLiteral("unlock-all")), tr("Alle Objekte &freigeben"), this, [this] { m_editor.unlockAll(); });
    object->addSeparator();
    m_selectionActions << object->addAction(icon(QStringLiteral("mirror-h")), tr("Waagrecht spiegeln"), this,
                                            [this] { m_editor.mirrorSelection(Qt::Horizontal); });
    m_selectionActions << object->addAction(icon(QStringLiteral("mirror-v")), tr("Senkrecht spiegeln"), this,
                                            [this] { m_editor.mirrorSelection(Qt::Vertical); });
    object->addSeparator();
    m_selectionActions << object->addAction(tr("Auf &Masterseite verschieben"), this,
                                            [this] { m_editor.moveSelectionToMaster(); });
    m_selectionActions << object->addAction(tr("Auf aktuelle &Seite verschieben"), this,
                                            [this] { m_editor.moveSelectionToCurrentPage(); });

    // Füllung und Kontur: Menü Objekt und Ausklappmenüs der Hilfsmittelleiste
    m_fillActions = {
        action(QStringLiteral("fill-uniform"), tr("&Gleichmässige Füllung …"), QKeySequence(Qt::SHIFT | Qt::Key_F11),
               [this] { editUniformFill(); }),
        action(QStringLiteral("fill-linear"), tr("&Farbverlauf …"), QKeySequence(Qt::Key_F11),
               [this] { editFountainFill(); }),
        action(QStringLiteral("fill-pattern"), tr("&Musterfüllung …"), QKeySequence(), [this] { editPatternFill(PatternFill::Kind::TwoColor); }),
        action(QStringLiteral("fill-texture"), tr("&Texturfüllung …"), QKeySequence(), [this] { editPatternFill(PatternFill::Kind::Texture); }),
        action(QStringLiteral("fill-none"), tr("&Keine Füllung"), QKeySequence(),
               [this] { m_editor.setSelectionFill(Fill::none()); }),
    };
    m_outlineActions = {
        action(QStringLiteral("outline-pen"), tr("Kontur&stift …"), QKeySequence(Qt::Key_F12), [this] { editOutlinePen(); }),
        action(QStringLiteral("tool-outline"), tr("&Konturfarbe …"), QKeySequence(Qt::SHIFT | Qt::Key_F12),
               [this] { editOutlineColor(); }),
        action(QStringLiteral("fill-none"), tr("K&eine Kontur"), QKeySequence(),
               [this] { m_editor.setSelectionStroke(std::nullopt); }),
    };
    for (const auto& [text, width] : {std::pair{tr("Haarlinie"), 0.076}, std::pair{tr("0,25 mm"), 0.25},
                                      std::pair{tr("0,5 mm"), 0.5}, std::pair{tr("1 mm"), 1.0}, std::pair{tr("2 mm"), 2.0}}) {
        const double w = width;
        m_outlineActions << action(QStringLiteral("outline-width"), text, QKeySequence(),
                                   [this, w] { m_editor.setSelectionStrokeWidth(w); });
    }
    object->addSeparator();
    QMenu* fillMenu = object->addMenu(icon(QStringLiteral("tool-fill")), tr("&Füllung"));
    fillMenu->addActions(m_fillActions);
    QMenu* outlineMenu = object->addMenu(icon(QStringLiteral("tool-outline")), tr("&Kontur"));
    outlineMenu->addActions(m_outlineActions);

    // Oben im Menü: Änderungen, Ausrichten und verteilen, Anordnung
    {
        QMenu* changes = new QMenu(tr("Ä&nderungen"), object);
        changes->setIcon(icon(QStringLiteral("xf-position")));
        const QList<std::tuple<QString, QString, QKeySequence>> pages{
            {QStringLiteral("xf-position"), tr("&Position"), QKeySequence(Qt::ALT | Qt::Key_F7)},
            {QStringLiteral("xf-rotate"), tr("&Drehen"), QKeySequence(Qt::ALT | Qt::Key_F8)},
            {QStringLiteral("xf-scale"), tr("&Skalieren und spiegeln"), QKeySequence(Qt::ALT | Qt::Key_F9)},
            {QStringLiteral("xf-size"), tr("&Grösse"), QKeySequence(Qt::ALT | Qt::Key_F10)},
            {QStringLiteral("xf-skew"), tr("&Neigung"), QKeySequence()}};
        for (int i = 0; i < int(pages.size()); ++i) {
            const auto& [iconName, text, key] = pages[i];
            QAction* a = changes->addAction(icon(iconName), text, this, [this, i] { showTransformDocker(i); });
            a->setShortcut(key);
        }
        QAction* clear = new QAction(icon(QStringLiteral("xf-clear")), tr("Änderungen &zurücknehmen"), object);
        connect(clear, &QAction::triggered, this, [this] { m_editor.clearTransformations(); });
        m_selectionActions << clear;
        QAction* first = object->actions().value(0);
        QAction* alignAct = nullptr;
        for (QAction* a : object->actions())
            if (a->menu() && a->text() == tr("Aus&richten und verteilen"))
                alignAct = a;
        auto separator = [object] {
            auto* s = new QAction(object);
            s->setSeparator(true);
            return s;
        };
        QList<QAction*> top{changes->menuAction(), clear, separator()};
        if (alignAct) {
            object->removeAction(alignAct);
            top << alignAct;
        }
        object->removeAction(m_arrangeMenu->menuAction());
        top << m_arrangeMenu->menuAction() << separator();
        object->insertActions(first, top);
    }

    // Effekte (vorerst für Bitmaps)
    QMenu* effects = menuBar()->addMenu(tr("Effe&kte"));
    // Für jedes Objekt: interaktive Werkzeuge mit Verläufen (linear, radial, konisch)
    effects->addAction(icon(QStringLiteral("tool-blur")), tr("&Blur (Unschärfe) …"), this,
                       [this] { m_canvas->setTool(ToolId::Blur); })
        ->setStatusTip(tr("Unschärfe gleichmässig oder als Verlauf über das Objekt ziehen"));
    effects->addAction(icon(QStringLiteral("tool-transparency")), tr("&Transparenz …"), this,
                       [this] { m_canvas->setTool(ToolId::Transparency); })
        ->setStatusTip(tr("Transparenz gleichmässig oder als Verlauf über das Objekt ziehen"));
    effects->addSeparator();
    QMenu* adjustMenu = effects->addMenu(tr("&Anpassen"));
    auto effectAction = [this](QMenu* menu, const QString& text, const QKeySequence& key, auto slot) {
        QAction* a = menu->addAction(text, this, slot);
        if (!key.isEmpty())
            a->setShortcut(key);
        m_bitmapActions << a;
        return a;
    };
    effectAction(adjustMenu, tr("&Kontrastverbesserung"), {}, [this] {
        applyImageEffect(tr("Kontrastverbesserung"), [](const QImage& i) { return adjust::stretchContrast(i); });
    });
    effectAction(adjustMenu, tr("&Tonkurve …"), {}, [this] {
        if (const ImageObject* image = m_editor.selectedImage()) {
            ToneCurveDialog dialog(image->image(), this);
            runImageDialog(tr("Tonkurve"), dialog);
        }
    });
    adjustMenu->addSeparator();
    effectAction(adjustMenu, tr("&Helligkeit/Kontrast/Intensität …"), QKeySequence(Qt::CTRL | Qt::Key_B), [this] {
        if (const ImageObject* image = m_editor.selectedImage()) {
            EffectDialog dialog(tr("Helligkeit/Kontrast/Intensität"), image->image(),
                                {{tr("Helligkeit:")}, {tr("Kontrast:")}, {tr("Intensität:")}},
                                [](const QImage& i, const QList<int>& v) {
                                    return adjust::brightnessContrastIntensity(i, v[0], v[1], v[2]);
                                },
                                this);
            runImageDialog(tr("Helligkeit/Kontrast/Intensität"), dialog);
        }
    });
    effectAction(adjustMenu, tr("&Farbbalance …"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B), [this] {
        if (const ImageObject* image = m_editor.selectedImage()) {
            EffectDialog dialog(tr("Farbbalance"), image->image(),
                                {{tr("Cyan – Rot:")}, {tr("Magenta – Grün:")}, {tr("Gelb – Blau:")}},
                                [](const QImage& i, const QList<int>& v) { return adjust::colorBalance(i, v[0], v[1], v[2]); },
                                this);
            runImageDialog(tr("Farbbalance"), dialog);
        }
    });
    effectAction(adjustMenu, tr("&Gamma …"), {}, [this] {
        if (const ImageObject* image = m_editor.selectedImage()) {
            EffectDialog dialog(tr("Gamma"), image->image(), {{tr("Gamma-Wert (×100):"), 10, 500, 100}},
                                [](const QImage& i, const QList<int>& v) { return adjust::gamma(i, v[0] / 100.0); }, this);
            runImageDialog(tr("Gamma"), dialog);
        }
    });
    adjustMenu->addSeparator();
    effectAction(adjustMenu, tr("Farbton/&Sättigung/Helligkeit …"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_U), [this] {
        if (const ImageObject* image = m_editor.selectedImage()) {
            EffectDialog dialog(tr("Farbton/Sättigung/Helligkeit"), image->image(),
                                {{tr("Farbton:"), -180, 180, 0}, {tr("Sättigung:")}, {tr("Helligkeit:")}},
                                [](const QImage& i, const QList<int>& v) {
                                    return adjust::hueSaturationLightness(i, v[0], v[1], v[2]);
                                },
                                this);
            runImageDialog(tr("Farbton/Sättigung/Helligkeit"), dialog);
        }
    });
    effectAction(adjustMenu, tr("Sättigung &reduzieren"), {}, [this] {
        applyImageEffect(tr("Sättigung reduzieren"), [](const QImage& i) { return adjust::desaturate(i); });
    });
    QMenu* correction = effects->addMenu(tr("&Korrektur"));
    effectAction(correction, tr("&Invertieren"), {}, [this] {
        applyImageEffect(tr("Invertieren"), [](const QImage& i) { return adjust::invert(i); });
    });
    effectAction(correction, tr("&Abstufen …"), {}, [this] {
        if (const ImageObject* image = m_editor.selectedImage()) {
            EffectDialog dialog(tr("Abstufen"), image->image(), {{tr("Stufen:"), 2, 32, 4}},
                                [](const QImage& i, const QList<int>& v) { return adjust::posterize(i, v[0]); }, this);
            runImageDialog(tr("Abstufen"), dialog);
        }
    });

    // Bitmaps
    // Effekte: Überblendung, Kontur, Hülle, Extrudieren, Linse, Abschrägung, …
    effects->addSeparator();
    effects->addAction(icon(QStringLiteral("tool-blend")), tr("Ü&berblendung"), this, [this] { m_canvas->setTool(ToolId::Blend); });
    effects->addAction(icon(QStringLiteral("tool-contour")), tr("&Kontur", "Effekt"), QKeySequence(Qt::CTRL | Qt::Key_F9), this,
                       [this] { m_canvas->setTool(ToolId::Contour); });
    effects->addAction(icon(QStringLiteral("tool-envelope")), tr("&Hülle"), QKeySequence(Qt::CTRL | Qt::Key_F7), this, [this] {
        m_envelopeDock->show();
        m_envelopeDock->raise();
    });
    effects->addAction(icon(QStringLiteral("tool-extrude")), tr("E&xtrudieren"), this, [this] { m_canvas->setTool(ToolId::Extrude); });
    effects->addAction(icon(QStringLiteral("tool-distort")), tr("&Verzerren"), this, [this] { m_canvas->setTool(ToolId::Distort); });
    m_selectionActions << effects->addAction(icon(QStringLiteral("fx-bevel")), tr("Ab&schrägung …"), this, [this] { bevelDialog(); });
    QAction* lens = effects->addAction(icon(QStringLiteral("fx-lens")), tr("&Linse …"), QKeySequence(Qt::ALT | Qt::Key_F3), this,
                                       [this] { lensDialog(); });
    m_selectionActions << lens;
    m_selectionActions << effects->addAction(icon(QStringLiteral("fx-perspective")), tr("&Perspektive hinzufügen"), this,
                                             [this] { m_canvas->setTool(ToolId::Perspective); });
    QMenu* superclip = effects->addMenu(icon(QStringLiteral("fx-superclip")), tr("Super&Clip"));
    m_selectionActions << superclip->addAction(tr("In &Rahmen platzieren …"), this, [this] {
        m_canvas->setTool(ToolId::Pick);
        m_canvas->pickTool().beginPickTarget([this](NodeId frame) { m_editor.placeInsideFrame(frame); });
        statusBar()->showMessage(tr("Rahmen anklicken, in den die Auswahl kommt"), 8000);
    });
    m_selectionActions << superclip->addAction(tr("Inhalt e&xtrahieren"), this, [this] { m_editor.extractContents(); });
    m_selectionActions << superclip->addAction(icon(QStringLiteral("clip-edit")), tr("SuperClip &bearbeiten"), this,
                                               [this] { m_editor.selectSuperClipContents(); });
    superclip->addAction(icon(QStringLiteral("clip-finish")), tr("Bearbeitung &beenden"), this, [this] { m_editor.endSuperClipEdit(); });
    superclip->addSeparator();
    m_selectionActions << superclip->addAction(tr("&Leeren SuperClip-Rahmen erstellen"), this, [this] { m_editor.createEmptySuperClipFrame(); });
    QAction* lockClip = superclip->addAction(tr("Inhalt in SuperClip &sperren"));
    lockClip->setCheckable(true);
    connect(lockClip, &QAction::toggled, this, [this](bool on) { m_editor.setSuperClipContentsLocked(on); });
    connect(superclip, &QMenu::aboutToShow, this, [this, lockClip] {
        const QList<const DrawObject*> objects = m_editor.selectedObjects();
        const auto* clip = objects.size() == 1 ? node_cast<SuperClipObject>(objects.first()) : nullptr;
        const QSignalBlocker block(lockClip);
        lockClip->setEnabled(clip);
        lockClip->setChecked(clip && clip->contentsLocked());
    });
    superclip->addSeparator();
    using Fit = Editor::ClipFit;
    for (const auto& [fit, text] : {std::pair{Fit::Center, tr("Inhalt &zentrieren")}, std::pair{Fit::FitProportional, tr("Inhalt proportional &einpassen")},
                                    std::pair{Fit::FillProportional, tr("Rahmen proportional &füllen")}, std::pair{Fit::Stretch, tr("Inhalt auf Rahmengrösse &strecken")}})
        m_selectionActions << superclip->addAction(text, this, [this, f = fit] { m_editor.fitSuperClipContents(f); });
    effects->addSeparator();
    m_selectionActions << effects->addAction(icon(QStringLiteral("fx-clear")), tr("Effekt l&öschen"), this, [this] { m_editor.clearSelectionEffects(); });
    auto pickEffects = [this](const QString& label) {
        m_canvas->setTool(ToolId::Pick);
        m_canvas->pickTool().beginPickTarget([this, label](NodeId source) { m_editor.copyEffectsFrom(source, label); });
        statusBar()->showMessage(tr("Objekt anklicken, dessen Effekte übernommen werden"), 8000);
    };
    QMenu* copyFx = effects->addMenu(tr("Effekt &kopieren"));
    m_selectionActions << copyFx->addAction(tr("Effekte von …"), this, [pickEffects] { pickEffects(tr("Effekt kopieren")); });
    QMenu* cloneFx = effects->addMenu(tr("Effekt kl&onen"));
    m_selectionActions << cloneFx->addAction(tr("Effekte von …"), this, [pickEffects] { pickEffects(tr("Effekt klonen")); });

    QMenu* bitmaps = menuBar()->addMenu(tr("&Bitmaps"));
    m_bitmapActions << bitmaps->addAction(tr("&Automatische Anpassung"), this, [this] {
        applyImageEffect(tr("Automatische Anpassung"), [](const QImage& i) { return adjust::autoAdjust(i); });
    });
    m_bitmapActions << bitmaps->addAction(icon(QStringLiteral("bitmap-edit")), tr("Bildanpassungs&editor …"), this,
                                          [this] { editBitmap(); });
    m_bitmapActions << bitmaps->addAction(tr("Bild &geraderichten …"), this, [this] {
        if (const ImageObject* image = m_editor.selectedImage()) {
            EffectDialog dialog(tr("Bild geraderichten"), image->image(),
                                {SliderSpec{tr("Drehwinkel (0,1°):"), -450, 450, 0}, SliderSpec{tr("Zuschneiden (1/0):"), 0, 1, 1}},
                                [](const QImage& i, const QList<int>& v) { return filters::straighten(i, v[0] / 10.0, v[1] == 1); }, this);
            runImageDialog(tr("Bild geraderichten"), dialog);
        }
    });
    bitmaps->addSeparator();
    m_bitmapActions << bitmaps->addAction(icon(QStringLiteral("bitmap-trace")), tr("&Blitzvektorisierung"), this,
                                          [this] { traceImage(-1, true); });
    QMenu* outlineTrace = bitmaps->addMenu(tr("&Umrissvektorisierung"));
    for (trace::Preset p : trace::presets())
        m_bitmapActions << outlineTrace->addAction(trace::presetName(p) + QStringLiteral(" …"), this,
                                                   [this, p] { traceImage(int(p), false); });
    addBitmapCommands(bitmaps);

    // Text: Mengentextrahmen verketten
    QMenu* textMenu = menuBar()->addMenu(tr("&Text"));
    // Text an Pfad, Aufzählung, Initiale
    m_selectionActions << textMenu->addAction(icon(QStringLiteral("text-on-path")), tr("Text an &Pfad anpassen"), this, [this] {
        if (m_editor.fitSelectedTextToPath())
            return;
        // Nur der Text ausgewählt: als Nächstes den Pfad anklicken
        const QList<const DrawObject*> sel = m_editor.selectedObjects();
        if (sel.size() != 1 || !node_cast<TextObject>(sel.first()))
            return;
        const NodeId text = sel.first()->id();
        m_canvas->setTool(ToolId::Pick);
        m_canvas->pickTool().beginPickTarget([this, text](NodeId path) { m_editor.fitTextToPath(text, path); });
        statusBar()->showMessage(tr("Objekt anklicken, an dessen Umriss der Text laufen soll"), 8000);
    });
    m_selectionActions << textMenu->addAction(tr("Text &begradigen"), this, [this] { m_editor.straightenSelectedText(); });
    textMenu->addSeparator();
    auto paragraphCommand = [this](const std::function<void(TextTool&)>& editing, const std::function<void()>& selected) {
        TextTool& tool = m_canvas->textTool();
        if (tool.editing())
            editing(tool);
        else
            selected();
    };
    QAction* bullets = textMenu->addAction(icon(QStringLiteral("text-bullets")), tr("&Aufzählungszeichen"));
    bullets->setCheckable(true);
    connect(bullets, &QAction::triggered, this, [paragraphCommand, this](bool on) {
        paragraphCommand([on](TextTool& t) { t.setBullet(on); }, [this, on] { m_editor.setTextBullets(on); });
    });
    QAction* dropCap = textMenu->addAction(icon(QStringLiteral("text-dropcap")), tr("&Initiale"));
    dropCap->setCheckable(true);
    connect(dropCap, &QAction::triggered, this, [paragraphCommand, this](bool on) {
        paragraphCommand([on](TextTool& t) { t.setDropCap(on ? 3 : 0); }, [this, on] { m_editor.setTextDropCap(on ? 3 : 0); });
    });
    connect(textMenu, &QMenu::aboutToShow, this, [this, bullets, dropCap] {
        const QList<const TextObject*> texts = m_editor.selectedTexts();
        const TextParagraph* first = !texts.isEmpty() && !texts.first()->paragraphs().isEmpty() ? &texts.first()->paragraphs().first() : nullptr;
        bullets->setChecked(first && first->bullet);
        dropCap->setChecked(first && first->dropCap > 0);
    });
    textMenu->addSeparator();
    QMenu* frameMenu = textMenu->addMenu(tr("&Mengentextrahmen"));
    m_linkText = frameMenu->addAction(tr("&Verknüpfen"), this, [this] { m_editor.linkSelectedTextFrames(); });
    m_linkText->setStatusTip(tr("Ausgewählte Mengentextrahmen in Auswahlreihenfolge verketten"));
    m_unlinkText = frameMenu->addAction(tr("Verknüpfung &aufheben"), this, [this] { m_editor.unlinkSelectedTextFrames(); });
    m_unlinkText->setStatusTip(tr("Jeder Rahmen behält den Text, der gerade darin steht"));
    textMenu->addSeparator();
    QAction* spell = textMenu->addAction(tr("&Rechtschreibprüfung …"), this, [this] {
        m_canvas->textTool().finishEditing();
        SpellDialog dialog(m_editor, this);
        dialog.exec();
    });
    spell->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F12));

    createTableMenu();

    // Extras
    QMenu* extras = menuBar()->addMenu(tr("E&xtras"));
    QAction* options = extras->addAction(tr("&Optionen …"), this, &MainWindow::openOptions);
    options->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_J));
    options->setStatusTip(tr("Einstellungen für Arbeitsbereich, Dokument und Programm"));
    QAction* assistant = extras->addAction(tr("&KI-Assistent"), this, [this] {
        m_aiDock->show();
        m_aiDock->raise();
        if (auto* input = m_aiPanel->findChild<QWidget*>(QStringLiteral("ai-input")))
            input->setFocus();
    });
    assistant->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_K));
    assistant->setStatusTip(tr("Mit der KI im Dokument arbeiten (Zugänge unter Optionen > Global > KI)"));
    // Skripte (JavaScript) statt VBA
    QMenu* scriptMenu = extras->addMenu(tr("&Skripte"));
    connect(scriptMenu, &QMenu::aboutToShow, this, [this, scriptMenu] {
        scriptMenu->clear();
        scriptMenu->addAction(tr("Skript-&Editor"), this, [this] {
            m_scriptDock->show();
            m_scriptDock->raise();
        });
        scriptMenu->addAction(tr("Skript &ausführen …"), this, [this] {
            const QString path = QFileDialog::getOpenFileName(this, tr("Skript ausführen"), ScriptDocker::scriptFolder(),
                                                              tr("JavaScript (*.js)"));
            if (path.isEmpty())
                return;
            if (!m_scriptDocker->runFile(path)) {
                m_scriptDock->show();
                m_scriptDock->raise();
            }
        });
        const QStringList scripts = ScriptDocker::scripts();
        if (!scripts.isEmpty())
            scriptMenu->addSeparator();
        for (const QString& path : scripts)
            scriptMenu->addAction(QFileInfo(path).completeBaseName(), this, [this, path] {
                if (!m_scriptDocker->runFile(path)) {
                    m_scriptDock->show();
                    m_scriptDock->raise();
                }
            });
        scriptMenu->addSeparator();
        scriptMenu->addAction(tr("Skript&ordner öffnen"), this, [] {
            QDir().mkpath(ScriptDocker::scriptFolder());
            QDesktopServices::openUrl(QUrl::fromLocalFile(ScriptDocker::scriptFolder()));
        });
    });
    scriptMenu->addAction(tr("Skript-&Editor")); // Platzhalter, damit das Menü aufklappbar ist

    QMenu* window = menuBar()->addMenu(tr("&Fenster"));
    m_dockerMenu = window->addMenu(tr("&Andockfenster"));

    // --- Standardleiste ---
    QToolBar* standard = addToolBar(tr("Standard"));
    m_standardBar = standard;
    standard->setObjectName(QStringLiteral("standard"));
    standard->setMovable(false);
    standard->setIconSize(QSize(20, 20));
    standard->addActions({newDoc, openDoc, saveDoc});
    standard->addSeparator();
    standard->addActions({cut, copy, m_paste});
    standard->addSeparator();
    standard->addActions({m_undo, m_redo});
    standard->addSeparator();

    m_zoomCombo = new QComboBox(standard);
    m_zoomCombo->setEditable(true);
    m_zoomCombo->setInsertPolicy(QComboBox::NoInsert);
    m_zoomCombo->setMinimumWidth(110);
    m_zoomCombo->setToolTip(tr("Zoomstufe"));
    m_zoomCombo->addItem(tr("Seite einpassen"), -1.0);
    for (double z : {10.0, 25.0, 50.0, 75.0, 100.0, 200.0, 400.0, 800.0, 1600.0})
        m_zoomCombo->addItem(QStringLiteral("%1 %").arg(z), z);
    standard->addWidget(m_zoomCombo);
    connect(m_zoomCombo, &QComboBox::activated, this, [this](int index) {
        const double z = m_zoomCombo->itemData(index).toDouble();
        if (z < 0)
            m_canvas->fitPage();
        else
            m_canvas->setZoomPercent(z);
        m_canvas->setFocus();
    });
    connect(m_zoomCombo->lineEdit(), &QLineEdit::editingFinished, this, [this] {
        QString text = m_zoomCombo->currentText();
        text.remove(QLatin1Char('%')).replace(QLatin1Char(','), QLatin1Char('.'));
        bool ok = false;
        const double z = text.trimmed().toDouble(&ok);
        if (ok && z > 0)
            m_canvas->setZoomPercent(z);
        m_canvas->setFocus();
    });

    standard->addSeparator();
    auto* snapButton = new QToolButton(standard);
    snapButton->setText(tr("Ausrichten an"));
    snapButton->setIcon(icon(QStringLiteral("snap")));
    snapButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    snapButton->setPopupMode(QToolButton::InstantPopup);
    snapButton->setMenu(snapMenu);
    standard->addWidget(snapButton);

    // --- Eigenschaftsleiste in eigener Zeile ---
    addToolBarBreak();
    auto* propertyBar = new PropertyBar(m_editor, *m_canvas, this);
    connect(propertyBar, &PropertyBar::traceRequested, this, &MainWindow::traceImage);
    connect(propertyBar, &PropertyBar::alignDialogRequested, this, [this] { showAlignDialog(0); });
    connect(propertyBar, &PropertyBar::editBitmapRequested, this, &MainWindow::editBitmap);
    addToolBar(propertyBar);
    m_propertyBar = propertyBar;
}

void MainWindow::createToolbox()
{
    QToolBar* tools = new QToolBar(tr("Hilfsmittel"), this);
    m_toolbox = tools;
    tools->setObjectName(QStringLiteral("toolbox"));
    tools->setMovable(false);
    tools->setIconSize(QSize(22, 22));
    addToolBar(Qt::LeftToolBarArea, tools);

    auto* group = new QActionGroup(this);
    // Werkzeug als Aktion; key erscheint rechts in der Ausklappliste
    auto tool = [&](ToolId id, const QString& iconName, const QString& name, const QString& key = QString()) {
        auto* a = new QAction(icon(iconName), key.isEmpty() ? name : name + QLatin1Char('\t') + key, this);
        a->setToolTip(key.isEmpty() ? name : tr("%1 (%2)").arg(name, key));
        a->setCheckable(true);
        connect(a, &QAction::triggered, this, [this, id] { m_canvas->setTool(id); });
        group->addAction(a);
        m_toolActions.insert(static_cast<int>(id), a);
        return a;
    };
    auto flyout = [&](const QList<QAction*>& actions) {
        tools->addWidget(new FlyoutButton(FlyoutButton::Kind::ToolGroup, actions, tools));
    };
    // Reihenfolge und Gruppen
    tools->addAction(tool(ToolId::Pick, QStringLiteral("tool-pick"), tr("Auswahl"), tr("Leertaste")));
    // Form mit den Formgebungswerkzeugen
    flyout({tool(ToolId::Shape, QStringLiteral("tool-shape"), tr("Form"), QStringLiteral("F10")),
            tool(ToolId::Smear, QStringLiteral("tool-smear"), tr("Verschmieren")),
            tool(ToolId::Twirl, QStringLiteral("tool-twirl"), tr("Wirbel")),
            tool(ToolId::Attract, QStringLiteral("tool-attract"), tr("Anziehen")),
            tool(ToolId::Repel, QStringLiteral("tool-repel"), tr("Abstossen")),
            tool(ToolId::Smudge, QStringLiteral("tool-smudge"), tr("Verwischen")),
            tool(ToolId::Roughen, QStringLiteral("tool-roughen"), tr("Aufrauen")),
            tool(ToolId::FreeTransform, QStringLiteral("tool-free-transform"), tr("Freie Transformation"))});
    flyout({tool(ToolId::Crop, QStringLiteral("tool-crop"), tr("Beschneiden")),
            tool(ToolId::Knife, QStringLiteral("tool-knife"), tr("Messer")),
            tool(ToolId::Eraser, QStringLiteral("tool-eraser"), tr("Radierer"), QStringLiteral("X")),
            tool(ToolId::VirtualDelete, QStringLiteral("tool-virtual-delete"), tr("Virtuelles Segment löschen"))});
    flyout({tool(ToolId::Zoom, QStringLiteral("tool-zoom"), tr("Zoom"), QStringLiteral("Z")),
            tool(ToolId::Pan, QStringLiteral("tool-pan"), tr("Schwenken"), QStringLiteral("H"))});
    flyout({tool(ToolId::Freehand, QStringLiteral("tool-freehand"), tr("Freihand"), QStringLiteral("F5")),
            tool(ToolId::Line, QStringLiteral("tool-line"), tr("2-Punkt-Linie")),
            tool(ToolId::Bezier, QStringLiteral("tool-bezier"), tr("Bézier")),
            tool(ToolId::ArtisticMedia, QStringLiteral("tool-media"), tr("Künstlerische Medien"), QStringLiteral("I")),
            tool(ToolId::SmartDrawing, QStringLiteral("tool-smart"), tr("Smart-Zeichnen"), tr("Umschalt+S")),
            tool(ToolId::Pen, QStringLiteral("tool-pen"), tr("Stift")),
            tool(ToolId::BSpline, QStringLiteral("tool-bspline"), tr("B-Spline")),
            tool(ToolId::Polyline, QStringLiteral("tool-polyline"), tr("Polylinie")),
            tool(ToolId::ThreePointCurve, QStringLiteral("tool-3point"), tr("3-Punkt-Kurve"))});
    tools->addSeparator();
    flyout({tool(ToolId::Rectangle, QStringLiteral("tool-rectangle"), tr("Rechteck"), QStringLiteral("F6")),
            tool(ToolId::ThreePointRectangle, QStringLiteral("tool-rectangle-3point"), tr("3-Punkt-Rechteck"))});
    flyout({tool(ToolId::Ellipse, QStringLiteral("tool-ellipse"), tr("Ellipse"), QStringLiteral("F7")),
            tool(ToolId::ThreePointEllipse, QStringLiteral("tool-ellipse-3point"), tr("3-Punkt-Ellipse"))});
    flyout({tool(ToolId::Polygon, QStringLiteral("tool-polygon"), tr("Polygon"), QStringLiteral("Y")),
            tool(ToolId::Star, QStringLiteral("tool-star"), tr("Stern")),
            tool(ToolId::ComplexStar, QStringLiteral("tool-complex-star"), tr("Komplexer Stern")),
            tool(ToolId::GraphPaper, QStringLiteral("tool-graph-paper"), tr("Millimeterpapier"), QStringLiteral("D")),
            tool(ToolId::Spiral, QStringLiteral("tool-spiral"), tr("Spirale"), QStringLiteral("A"))});
    // Perfekte Formen
    flyout({tool(ToolId::PerfectBasic, QStringLiteral("perfect-basic"), tr("Grundformen")),
            tool(ToolId::PerfectArrow, QStringLiteral("perfect-arrow"), tr("Pfeilformen")),
            tool(ToolId::PerfectFlowchart, QStringLiteral("perfect-flowchart"), tr("Flussdiagrammformen")),
            tool(ToolId::PerfectBanner, QStringLiteral("perfect-banner"), tr("Banner- und Sternformen")),
            tool(ToolId::PerfectCallout, QStringLiteral("perfect-callout"), tr("Legendenformen"))});
    tools->addAction(tool(ToolId::Text, QStringLiteral("tool-text"), tr("Text"), QStringLiteral("F8")));
    tools->addAction(tool(ToolId::Table, QStringLiteral("tool-table"), tr("Tabelle")));
    flyout({tool(ToolId::DimensionParallel, QStringLiteral("tool-dimension"), tr("Parallelbemassung")),
            tool(ToolId::DimensionHV, QStringLiteral("tool-dimension-hv"), tr("Horizontal- oder Vertikalbemassung")),
            tool(ToolId::DimensionAngle, QStringLiteral("tool-dimension-angle"), tr("Winkelbemassung"))});
    flyout({tool(ToolId::Connector, QStringLiteral("tool-connector"), tr("Gerade Verbindung")),
            tool(ToolId::ConnectorAngle, QStringLiteral("tool-connector-angle"), tr("Rechtwinklige Verbindung")),
            tool(ToolId::ConnectorRound, QStringLiteral("tool-connector-round"), tr("Abgerundete rechtwinklige Verbindung"))});
    flyout({tool(ToolId::ColorPipette, QStringLiteral("tool-pipette"), tr("Farbpipette")),
            tool(ToolId::AttributePipette, QStringLiteral("tool-attr-pipette"), tr("Attributpipette"))});
    tools->addSeparator();
    flyoutButton(tools, QStringLiteral("tool-outline"), tr("Kontur"), m_outlineActions);
    flyoutButton(tools, QStringLiteral("tool-fill"), tr("Füllung"), m_fillActions);
    flyout({tool(ToolId::InteractiveFill, QStringLiteral("tool-interactive-fill"), tr("Interaktive Füllung"),
                 QStringLiteral("G")),
            tool(ToolId::Mesh, QStringLiteral("tool-mesh"), tr("Maschenfüllung"), QStringLiteral("M")),
            tool(ToolId::SmartFill, QStringLiteral("tool-smart-fill"), tr("Smart-Füllung")),
            tool(ToolId::Blend, QStringLiteral("tool-blend"), tr("Überblendung")),
            tool(ToolId::Contour, QStringLiteral("tool-contour"), tr("Kontur")),
            tool(ToolId::Distort, QStringLiteral("tool-distort"), tr("Verzerren")),
            tool(ToolId::DropShadow, QStringLiteral("tool-shadow"), tr("Hinterlegter Schatten")),
            tool(ToolId::Envelope, QStringLiteral("tool-envelope"), tr("Hülle")),
            tool(ToolId::Extrude, QStringLiteral("tool-extrude"), tr("Extrusion")),
            tool(ToolId::Transparency, QStringLiteral("tool-transparency"), tr("Transparenz")),
            tool(ToolId::Blur, QStringLiteral("tool-blur"), tr("Blur (Unschärfe)"))});

    m_toolActions.value(static_cast<int>(ToolId::Pick))->setChecked(true);
    connect(m_canvas, &Canvas::toolChanged, this, [this](ToolId id) {
        if (QAction* a = m_toolActions.value(static_cast<int>(id)))
            a->setChecked(true);
    });
}

QToolButton* MainWindow::flyoutButton(QToolBar* bar, const QString& iconName, const QString& tooltip,
                                      const QList<QAction*>& actions)
{
    // Ausklappliste: Klick öffnet die Liste
    auto* button = new FlyoutButton(FlyoutButton::Kind::Commands, actions, bar);
    button->setIcon(icon(iconName));
    button->setToolTip(tooltip);
    bar->addWidget(button);
    return button;
}

void MainWindow::editUniformFill()
{
    const QList<const DrawObject*> objects = m_editor.selectedObjects();
    const Fill current = objects.isEmpty() ? m_editor.defaultStyle().fill : objects.first()->style().fill;
    if (const std::optional<Color> c = ColorDialog::getColor(this, tr("Gleichmässige Füllung"),
                                                             current.representativeColor().value_or(Color(0, 0, 0))))
        m_editor.setSelectionFill(Fill::solid(*c));
}

void MainWindow::editFountainFill()
{
    const QList<const DrawObject*> objects = m_editor.selectedObjects();
    const Fill current = objects.isEmpty() ? m_editor.defaultStyle().fill : objects.first()->style().fill;
    if (const std::optional<Fill> f = GradientDialog::getFill(this, current, m_editor.selectionLocalSize()))
        m_editor.setSelectionFill(*f);
}

void MainWindow::editPatternFill(PatternFill::Kind kind)
{
    const QList<const DrawObject*> objects = m_editor.selectedObjects();
    PatternFill p;
    if (!objects.isEmpty() && objects.first()->style().fill.isPattern())
        p = objects.first()->style().fill.pattern;
    else
        p.kind = kind;
    PatternFillDialog dialog(p, this);
    dialog.setKind(p.kind == kind || objects.isEmpty() || !objects.first()->style().fill.isPattern() ? kind : p.kind);
    if (dialog.exec() == QDialog::Accepted)
        m_editor.setSelectionFill(Fill::patternFill(dialog.pattern()));
}

void MainWindow::editOutlinePen()
{
    const QList<const DrawObject*> objects = m_editor.selectedObjects();
    OutlinePenDialog dialog(objects.isEmpty() ? m_editor.defaultStyle() : objects.first()->style(), this);
    if (dialog.exec() == QDialog::Accepted)
        m_editor.setSelectionOutlinePen(dialog.style());
}

void MainWindow::editOutlineColor()
{
    const QList<const DrawObject*> objects = m_editor.selectedObjects();
    const Style style = objects.isEmpty() ? m_editor.defaultStyle() : objects.first()->style();
    if (const std::optional<Color> c = ColorDialog::getColor(this, tr("Konturfarbe"), style.stroke.value_or(Color(0, 0, 0))))
        m_editor.setSelectionStroke(*c);
}

QDockWidget* MainWindow::addDocker(const QString& title, const QString& objectName, QWidget* content)
{
    auto* dock = new QDockWidget(title, this);
    dock->setObjectName(objectName);
    dock->setWidget(content);
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, dock);
    m_dockerMenu->addAction(dock->toggleViewAction());
    return dock;
}

void MainWindow::createDockers()
{
    // Mehrere Andockfenster als Reiter, die Reiter senkrecht rechts daneben
    setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowTabbedDocks);
    setTabPosition(Qt::RightDockWidgetArea, QTabWidget::East);
    setTabPosition(Qt::LeftDockWidgetArea, QTabWidget::West);
    m_firstDocker = addDocker(tr("Objekt-Manager"), QStringLiteral("objectManager"), new ObjectManager(m_editor, this));
    m_shapingDock = addDocker(tr("Formen"), QStringLiteral("shaping"), new ShapingDocker(m_editor, *m_canvas, this));
    m_propertiesDock = addDocker(tr("Objekteigenschaften"), QStringLiteral("objectProperties"), new ObjectPropertiesDocker(m_editor, this));
    m_symbolDock = addDocker(tr("Symbol-Manager"), QStringLiteral("symbols"), new SymbolDocker(m_editor, this));
    m_transformDocker = new TransformDocker(m_editor, this);
    m_transformDock = addDocker(tr("Änderungen"), QStringLiteral("transform"), m_transformDocker);
    m_scriptDocker = new ScriptDocker(m_editor, this);
    m_scriptDock = addDocker(tr("Skripte"), QStringLiteral("scripts"), m_scriptDocker);
    m_lensDocker = new LensDocker(m_editor, this);
    m_lensDock = addDocker(tr("Linse"), QStringLiteral("lens"), m_lensDocker);
    m_envelopeDocker = new EnvelopeDocker(m_editor, *m_canvas, this);
    m_envelopeDock = addDocker(tr("Hülle"), QStringLiteral("envelope"), m_envelopeDocker);
    // KI-Assistent (Extras > KI-Assistent)
    m_aiPanel = new AiPanel(m_editor, *m_canvas, this, this);
    m_aiDock = addDocker(tr("KI"), QStringLiteral("ai"), m_aiPanel);
    connect(m_aiPanel, &AiPanel::optionsRequested, this, [this] {
        OptionsDialog dialog(m_editor, this);
        dialog.selectPage(tr("Global/KI"));
        dialog.exec();
        m_aiPanel->reloadAccounts();
    });
    tabifyDockWidget(m_firstDocker, m_shapingDock);
    tabifyDockWidget(m_firstDocker, m_lensDock);
    tabifyDockWidget(m_firstDocker, m_envelopeDock);
    m_shapingDock->hide();
    m_lensDock->hide();
    m_envelopeDock->hide();
    tabifyDockWidget(m_firstDocker, m_aiDock);
    m_aiDock->hide();
    m_firstDocker->raise();
}

void MainWindow::rebuildPageTabs()
{
    QSignalBlocker block(m_pageTabs);
    const Document& doc = m_editor.document();
    while (m_pageTabs->count() > doc.pageCount())
        m_pageTabs->removeTab(m_pageTabs->count() - 1);
    for (int i = 0; i < doc.pageCount(); ++i) {
        const QString title = displayName(doc.page(i));
        if (i < m_pageTabs->count())
            m_pageTabs->setTabText(i, title);
        else
            m_pageTabs->addTab(title);
    }
    m_pageTabs->setCurrentIndex(m_editor.currentPageIndex());
    m_pageLabel->setText(tr("%1 von %2").arg(m_editor.currentPageIndex() + 1).arg(doc.pageCount()));
}

void MainWindow::updateActions()
{
    const Document& doc = m_editor.document();
    m_undo->setEnabled(doc.canUndo());
    m_redo->setEnabled(doc.canRedo());
    m_undo->setText(doc.canUndo() ? tr("&Rückgängig: %1").arg(doc.undoText()) : tr("&Rückgängig"));
    m_redo->setText(doc.canRedo() ? tr("&Wiederholen: %1").arg(doc.redoText()) : tr("&Wiederholen"));
    m_paste->setEnabled(Editor::canPaste());
    const bool hasSelection = !m_editor.selection().isEmpty();
    for (QAction* a : std::as_const(m_selectionActions))
        a->setEnabled(hasSelection);
    // Gesperrte Objekte lassen sich kopieren, aber nicht ausschneiden oder löschen
    const bool editable = m_editor.selectionHasUnlocked();
    m_cut->setEnabled(editable);
    m_toCurves->setEnabled(m_editor.canConvertSelectionToCurves());
    m_combine->setEnabled(m_editor.canCombineSelection());
    m_breakApart->setEnabled(m_editor.canBreakApartSelection());
    m_lock->setEnabled(editable);
    m_unlock->setEnabled(m_editor.selectionHasLocked());
    for (auto it = m_arrangeActions.cbegin(); it != m_arrangeActions.cend(); ++it)
        it.value()->setEnabled(m_editor.canArrange(Editor::Arrange(it.key())));
    for (QAction* a : std::as_const(m_placeActions))
        a->setEnabled(editable);
    for (auto it = m_shapeActions.cbegin(); it != m_shapeActions.cend(); ++it)
        it.value()->setEnabled(m_editor.canShapeSelection(Editor::ShapeOp(it.key())));
    m_breakConnector->setEnabled(m_editor.canBreakConnectors());
    m_linkText->setEnabled(m_editor.canLinkSelectedTextFrames());
    m_unlinkText->setEnabled(m_editor.canUnlinkSelectedTextFrames());
    const bool image = m_editor.selectedImage() && editable;
    for (QAction* a : std::as_const(m_bitmapActions))
        a->setEnabled(image);
}

void MainWindow::traceImage(int preset, bool quick)
{
    const ImageObject* image = m_editor.selectedImage();
    if (!image)
        return;
    const NodeId id = image->id();
    Options& options = Options::instance();
    if (quick) {
        // Blitzvektorisierung ohne Dialog; Bildtyp aus Optionen > SuperTrace (0 = zuletzt benutzt)
        const int choice = options.integer(QStringLiteral("ws/trace/quick"));
        const int quickPreset = choice > 0 ? choice - 1 : options.integer(QStringLiteral("ws/trace/lastPreset"));
        trace::Settings settings = trace::presetSettings(trace::Preset(std::clamp(quickPreset, 0, int(trace::presets().size()) - 1)));
        settings.maxSide = 400 + 12 * std::clamp(options.integer(QStringLiteral("ws/trace/performance")), 0, 100);
        QApplication::setOverrideCursor(Qt::WaitCursor);
        const trace::Result result = trace::run(image->image(), settings);
        QApplication::restoreOverrideCursor();
        m_editor.vectorizeImage(id, result, false);
        return;
    }
    TraceDialog dialog(image->image(), trace::Preset(preset), this);
    connect(&dialog, &TraceDialog::optionsRequested, this, [this, &dialog] {
        OptionsDialog optionsDialog(m_editor, this, &dialog);
        optionsDialog.selectPage(tr("Arbeitsbereich/SuperTrace"));
        optionsDialog.exec();
    });
    if (dialog.exec() == QDialog::Accepted) {
        options.set(QStringLiteral("ws/trace/lastPreset"), int(dialog.preset()));
        m_editor.vectorizeImage(id, dialog.result(), dialog.deleteOriginal());
    }
}

void MainWindow::editBitmap()
{
    if (const ImageObject* image = m_editor.selectedImage()) {
        ImageAdjustDialog dialog(image->image(), this);
        runImageDialog(tr("Bildanpassung"), dialog);
    }
}

void MainWindow::applyImageEffect(const QString& label, const std::function<QImage(const QImage&)>& fn)
{
    const ImageObject* image = m_editor.selectedImage();
    if (!image)
        return;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const QImage result = fn(image->image());
    QApplication::restoreOverrideCursor();
    m_editor.setImagePixels(image->id(), result, label);
}

void MainWindow::runImageDialog(const QString& label, ImagePreviewDialog& dialog)
{
    const ImageObject* image = m_editor.selectedImage();
    if (!image)
        return;
    const NodeId id = image->id();
    if (dialog.exec() != QDialog::Accepted)
        return;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const QImage result = dialog.result();
    QApplication::restoreOverrideCursor();
    m_editor.setImagePixels(id, result, label);
}

void MainWindow::offerRestart(const QString& title, const QString& question)
{
    const auto answer = QMessageBox::question(
        this, title.isEmpty() ? tr("Oberflächengrösse") : title,
        question.isEmpty() ? tr("Die neue Oberflächengrösse gilt ab dem nächsten Start. Jetzt neu starten?") : question);
    if (answer != QMessageBox::Yes)
        return;
    // Erst schliessen (fragt nach ungespeicherten Änderungen), dann neu starten
    const QStringList arguments{m_editor.filePath()};
    if (close())
        QProcess::startDetached(QApplication::applicationFilePath(),
                                m_editor.filePath().isEmpty() ? QStringList() : arguments);
}

void MainWindow::showObjectContextMenu(const QPoint& globalPos)
{
    updateActions();
    QMenu menu(this); // erbt das Stylesheet des Fensters
    menu.addActions({m_toCurves, m_breakApart, m_wrapText, m_breakConnector});
    if (m_linkText->isEnabled() || m_unlinkText->isEnabled()) {
        menu.addSeparator();
        menu.addAction(m_linkText->isEnabled() ? m_linkText : m_unlinkText);
    }
    menu.addSeparator();
    menu.addActions({m_undo, m_cut, m_copy, m_delete});
    // Wie gewohnt nur der passende der beiden Einträge
    menu.addAction(m_editor.selectionHasUnlocked() ? m_lock : m_unlock);
    menu.addSeparator();
    menu.addMenu(m_arrangeMenu);
    menu.exec(globalPos);
}

void MainWindow::updateTitle()
{
    const QString name = m_editor.filePath().isEmpty() ? tr("Unbenannt") : QFileInfo(m_editor.filePath()).fileName();
    setWindowTitle(tr("%1[*] – TANGENTA").arg(name));
    setWindowModified(m_editor.document().isModified());
}

void MainWindow::updateStatus()
{
    const QList<const DrawObject*> objects = m_editor.selectedObjects();
    if (objects.isEmpty()) {
        const QSizeF size = m_editor.currentPage().size();
        m_infoLabel->setText(tr("%1 — %2 × %3 mm")
                                 .arg(displayName(m_editor.currentPage()))
                                 .arg(number(size.width(), 1), number(size.height(), 1)));
        std::optional<Style> defaults = m_editor.defaultStyle();
        m_styleIndicator->setStyle(defaults);
        return;
    }
    if (objects.size() == 1) {
        const bool onMaster = m_editor.document().pageOf(objects.first()->id()) == &m_editor.document().masterPage();
        m_infoLabel->setText(tr("%1 auf %2%3")
                                 .arg(displayName(*objects.first()), displayName(*objects.first()->parent()),
                                      onMaster ? tr(" (Masterseite)") : QString()));
    } else {
        m_infoLabel->setText(tr("%1 Objekte ausgewählt").arg(objects.size()));
    }
    m_styleIndicator->setStyle(objects.first()->style());
}

void MainWindow::updateZoomCombo()
{
    if (m_zoomCombo->lineEdit()->hasFocus())
        return;
    m_zoomCombo->setEditText(QStringLiteral("%1 %").arg(std::lround(m_canvas->zoomPercent())));
}

namespace {

// Kurzinfos ausblenden (Optionen > Anzeige)
class TooltipFilter : public QObject {
public:
    using QObject::QObject;
    bool eventFilter(QObject*, QEvent* e) override { return e->type() == QEvent::ToolTip; }
};

QString plainMenuText(QString text)
{
    text.remove(QLatin1Char('&'));
    const int tab = text.indexOf(QLatin1Char('\t'));
    if (tab >= 0)
        text.truncate(tab);
    return text.trimmed();
}

void forEachMenuAction(QMenu* menu, const QString& path, const std::function<void(const QString&, QAction*)>& fn)
{
    for (QAction* a : menu->actions()) {
        if (a->isSeparator())
            continue;
        if (a->menu())
            forEachMenuAction(a->menu(), path + QLatin1Char('/') + plainMenuText(a->text()), fn);
        else
            fn(path + QLatin1Char('/') + plainMenuText(a->text()), a);
    }
}

} // namespace

void MainWindow::openOptions()
{
    OptionsDialog dialog(m_editor, this, this);
    dialog.exec();
}

void MainWindow::applyShortcuts()
{
    const QJsonObject custom =
        QJsonDocument::fromJson(Options::instance().text(QStringLiteral("ws/custom/shortcuts")).toUtf8()).object();
    for (QAction* top : menuBar()->actions()) {
        if (!top->menu())
            continue;
        forEachMenuAction(top->menu(), plainMenuText(top->text()), [&custom](const QString& path, QAction* a) {
            if (!a->property("defaultShortcut").isValid())
                a->setProperty("defaultShortcut", a->shortcut().toString());
            a->setShortcut(custom.contains(path) ? QKeySequence(custom.value(path).toString())
                                                 : QKeySequence(a->property("defaultShortcut").toString()));
        });
    }
}

void MainWindow::applyOptions()
{
    const Options& o = Options::instance();
    m_editor.document().setUndoLimit(o.integer(QStringLiteral("ws/general/undo")));
    RectangleObject::setScaleCorners(o.flag(QStringLiteral("ws/tools/rect/scaleCorners")));
    {
        // Dateizuordnung nur nachführen, wenn sie im Optionsdialog geändert wurde (beim ersten
        // Start wird nichts in die Registry geschrieben)
        QSettings settings;
        const QString wanted = o.value(QStringLiteral("global/filters/associate")).toString();
        if (!settings.contains(QStringLiteral("files/associated"))) {
            settings.setValue(QStringLiteral("files/associated"), wanted);
        } else if (settings.value(QStringLiteral("files/associated")).toString() != wanted) {
            registerFileTypes(wanted.split(QLatin1Char(','), Qt::SkipEmptyParts));
            settings.setValue(QStringLiteral("files/associated"), wanted);
        }
    }
    // Rechtschreibung in der Sprache der Blitzkorrektur
    static const QStringList kSpellLanguages{QStringLiteral("de_CH"), QStringLiteral("de_DE"), QStringLiteral("en_US"),
                                             QStringLiteral("fr_FR")};
    SpellChecker::instance().setLanguage(kSpellLanguages.value(o.integer(QStringLiteral("ws/text/autocorrect/language")),
                                                               kSpellLanguages.first()));
    // Leisten
    menuBar()->setVisible(o.flag(QStringLiteral("ws/custom/bar/menu")));
    statusBar()->setVisible(o.flag(QStringLiteral("ws/custom/bar/status")));
    if (m_standardBar)
        m_standardBar->setVisible(o.flag(QStringLiteral("ws/custom/bar/standard")));
    if (m_propertyBar)
        m_propertyBar->setVisible(o.flag(QStringLiteral("ws/custom/bar/property")));
    if (m_toolbox)
        m_toolbox->setVisible(o.flag(QStringLiteral("ws/custom/bar/toolbox")));
    const bool rulers = o.flag(QStringLiteral("ws/custom/bar/rulers"));
    m_hRuler->setVisible(rulers);
    m_vRuler->setVisible(rulers);
    m_rulerCorner->setVisible(rulers);
    // Menühäkchen
    for (QAction* a : QList<QAction*>{m_rulersAction, m_guidesAction} + m_snapActions) {
        if (a) {
            const QSignalBlocker block(a);
            a->setChecked(o.flag(a->data().toString()));
        }
    }
    // Hilfslinien und Einrasten
    m_canvas->setGuidesVisible(o.flag(QStringLiteral("doc/guides/show")));
    SnapSettings& snap = m_editor.snapSettings();
    snap.guides = o.flag(QStringLiteral("doc/guides/snap"));
    snap.objects = o.flag(QStringLiteral("ws/snap/enabled"));
    snap.page = o.flag(QStringLiteral("ws/snap/page"));
    m_editor.setTreatAllObjectsAsFilled(o.flag(QStringLiteral("ws/tools/pick/treatFilled")));
    // Werkzeuge
    ShapeDefaults& d = m_editor.shapeDefaults();
    d.ellipseKind = EllipseObject::Kind(std::clamp(o.integer(QStringLiteral("ws/tools/ellipse/kind")), 0, 2));
    d.startAngle = o.number(QStringLiteral("ws/tools/ellipse/start"));
    d.endAngle = o.number(QStringLiteral("ws/tools/ellipse/end"));
    d.clockwise = o.integer(QStringLiteral("ws/tools/ellipse/clockwise")) == 1;
    m_canvas->eraserTool().setWidth(o.number(QStringLiteral("ws/tools/eraser/width")));
    // Kurzinfos
    if (!o.flag(QStringLiteral("ws/display/tooltips")) && !m_tooltipFilter) {
        m_tooltipFilter = new TooltipFilter(this);
        qApp->installEventFilter(m_tooltipFilter);
    } else if (o.flag(QStringLiteral("ws/display/tooltips")) && m_tooltipFilter) {
        qApp->removeEventFilter(m_tooltipFilter);
        delete m_tooltipFilter;
        m_tooltipFilter = nullptr;
    }
    // Automatische Sicherung
    if (!m_autosaveTimer) {
        m_autosaveTimer = new QTimer(this);
        connect(m_autosaveTimer, &QTimer::timeout, this, &MainWindow::autosave);
    }
    const int minutes = std::max(1, o.integer(QStringLiteral("ws/save/autosaveMinutes")));
    if (o.flag(QStringLiteral("ws/save/autosave"))) {
        if (!m_autosaveTimer->isActive() || m_autosaveTimer->interval() != minutes * 60000)
            m_autosaveTimer->start(minutes * 60000);
    } else {
        m_autosaveTimer->stop();
    }
    applyShortcuts();
    m_canvas->update();
    m_hRuler->update();
    m_vRuler->update();
}

void MainWindow::autosave()
{
    if (!m_editor.document().isModified())
        return;
    const Options& o = Options::instance();
    QString folder = o.integer(QStringLiteral("ws/save/folderMode")) == 1 ? o.text(QStringLiteral("ws/save/folder")) : QString();
    if (folder.isEmpty() || !QDir(folder).exists())
        folder = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString name = m_editor.filePath().isEmpty() ? tr("Unbenannt") : QFileInfo(m_editor.filePath()).completeBaseName();
    const QString path = QDir(folder).filePath(QStringLiteral("AutoBackup_of_%1.%2").arg(name, QLatin1String(io::FileSuffix)));
    QString error;
    if (io::save(m_editor.document(), path, &error))
        statusBar()->showMessage(tr("Automatische Sicherung: %1").arg(path), 3000);
}

void MainWindow::showGuideContextMenu(int index, const QPoint& globalPos)
{
    QMenu menu(this);
    QAction* local = menu.addAction(icon(QStringLiteral("page-current")), tr("Nur auf &dieser Seite"));
    local->setCheckable(true);
    local->setChecked(m_editor.isGuidePageSpecific(index));
    connect(local, &QAction::toggled, this, [this, index](bool on) { m_editor.setGuidePageSpecific(index, on); });
    menu.addSeparator();
    menu.addAction(icon(QStringLiteral("edit-delete")), tr("&Löschen"), this, [this] { deleteSelectionOrGuide(); });
    menu.exec(globalPos);
}

void MainWindow::deleteSelectionOrGuide()
{
    if (m_canvas->selectedGuide() >= 0) {
        m_editor.removeGuide(m_canvas->selectedGuide());
        m_canvas->setSelectedGuide(-1);
    } else {
        m_editor.deleteSelection();
    }
}

bool MainWindow::maybeSave()
{
    if (!m_editor.document().isModified())
        return true;
    const auto answer =
        QMessageBox::warning(this, tr("TANGENTA"), tr("Das Dokument wurde geändert. Änderungen speichern?"),
                             QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (answer == QMessageBox::Save)
        return save();
    return answer == QMessageBox::Discard;
}

bool MainWindow::save()
{
    if (m_editor.filePath().isEmpty())
        return saveAs();
    // Sicherungskopie beim Speichern: "Backup_of_<Name>" neben der Datei
    const QFileInfo info(m_editor.filePath());
    if (Options::instance().flag(QStringLiteral("ws/save/backupOnSave")) && info.exists()) {
        const QString backup = info.absoluteDir().filePath(QStringLiteral("Backup_of_") + info.fileName());
        QFile::remove(backup);
        QFile::copy(info.absoluteFilePath(), backup);
    }
    QString error;
    if (!io::save(m_editor.document(), m_editor.filePath(), &error)) {
        QMessageBox::critical(this, tr("Speichern fehlgeschlagen"), error);
        return false;
    }
    m_editor.document().setClean();
    updateTitle();
    rememberLastFile(m_editor.filePath());
    statusBar()->showMessage(tr("Gespeichert: %1").arg(m_editor.filePath()), 3000);
    return true;
}

bool MainWindow::saveAs()
{
    QSettings settings;
    QString path = QFileDialog::getSaveFileName(this, tr("Speichern unter"),
                                                settings.value(QStringLiteral("files/lastDir")).toString(), fileFilter());
    if (path.isEmpty())
        return false;
    if (QFileInfo(path).suffix().isEmpty())
        path += QLatin1Char('.') + QLatin1String(io::FileSuffix);
    settings.setValue(QStringLiteral("files/lastDir"), QFileInfo(path).absolutePath());
    m_editor.setFilePath(path);
    return save();
}

void MainWindow::newDocument()
{
    if (!maybeSave())
        return;
    Options& o = Options::instance();
    if (!o.flag(QStringLiteral("ws/general/newDocDialog"))) {
        createDocument(Page::A4, 1);
        return;
    }
    // "Neues Dokument erstellen"
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Neues Dokument erstellen"));
    auto* form = new QFormLayout(&dialog);
    struct Format {
        QString name;
        QSizeF size;
    };
    const QList<Format> formats{{QStringLiteral("A3"), {297, 420}}, {QStringLiteral("A4"), {210, 297}},
                                {QStringLiteral("A5"), {148, 210}}, {QStringLiteral("A6"), {105, 148}},
                                {tr("Letter"), {215.9, 279.4}}, {tr("Legal"), {215.9, 355.6}}, {tr("Eigene"), {}}};
    auto* format = new QComboBox(&dialog);
    for (const Format& f : formats)
        format->addItem(f.name);
    format->setCurrentIndex(1);
    auto spin = [&dialog](double v) {
        auto* s = new QDoubleSpinBox(&dialog);
        s->setRange(1, 5000);
        s->setDecimals(1);
        s->setSuffix(QStringLiteral(" mm"));
        s->setValue(v);
        return s;
    };
    auto* width = spin(210);
    auto* height = spin(297);
    auto* landscape = new QCheckBox(tr("Querformat"), &dialog);
    auto* pages = new QSpinBox(&dialog);
    pages->setRange(1, 999);
    auto* again = new QCheckBox(tr("Dieses Dialogfeld immer anzeigen"), &dialog);
    again->setChecked(true);
    connect(format, &QComboBox::currentIndexChanged, &dialog, [=](int i) {
        const QSizeF s = formats.value(i).size;
        if (s.isEmpty())
            return;
        const bool quer = landscape->isChecked();
        width->setValue(quer ? s.height() : s.width());
        height->setValue(quer ? s.width() : s.height());
    });
    connect(landscape, &QCheckBox::toggled, &dialog, [=](bool quer) {
        const double w = width->value(), h = height->value();
        if ((quer && w < h) || (!quer && w > h)) {
            width->setValue(h);
            height->setValue(w);
        }
    });
    form->addRow(tr("Seitenformat:"), format);
    form->addRow(tr("Breite:"), width);
    form->addRow(tr("Höhe:"), height);
    form->addRow(QString(), landscape);
    form->addRow(tr("Seitenanzahl:"), pages);
    form->addRow(QString(), again);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form->addRow(buttons);
    if (dialog.exec() != QDialog::Accepted)
        return;
    if (!again->isChecked())
        o.set(QStringLiteral("ws/general/newDocDialog"), false);
    createDocument(QSizeF(width->value(), height->value()), pages->value());
}

void MainWindow::createDocument(const QSizeF& pageSize, int pages)
{
    m_editor.setDocument(std::make_unique<Document>(pageSize), QString());
    for (int i = 1; i < pages; ++i)
        m_editor.insertPage(i);
    m_editor.setCurrentPageIndex(0);
    m_editor.document().setClean();
    updateTitle();
}

void MainWindow::applyStartup()
{
    QSettings settings;
    switch (Options::instance().integer(QStringLiteral("ws/general/startup"))) {
    case 3: { // Neues Dokument aus Vorlage: Datei öffnen, aber als unbenanntes Dokument
        const QString path = QFileDialog::getOpenFileName(this, tr("Neu aus Vorlage"),
                                                          settings.value(QStringLiteral("files/lastDir")).toString(), fileFilter());
        if (!path.isEmpty() && openFile(path)) {
            m_editor.setFilePath(QString());
            updateTitle();
        }
        break;
    }
    case 4:
        open();
        break;
    case 5: {
        const QString last = settings.value(QStringLiteral("files/lastFile")).toString();
        if (!last.isEmpty() && QFileInfo::exists(last))
            openFile(last);
        break;
    }
    case 2:
        if (Options::instance().flag(QStringLiteral("ws/general/newDocDialog")))
            newDocument();
        break;
    default: // Begrüssungsbildschirm gibt es nicht; "Nichts" = leeres Dokument wie bisher
        break;
    }
}

void MainWindow::registerFileTypes(const QStringList& suffixes)
{
#ifdef Q_OS_WIN
    QSettings classes(QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes"), QSettings::NativeFormat);
    const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    const QString command = QStringLiteral("\"%1\" \"%2\"").arg(exe, QStringLiteral("%1"));
    for (const QString& progId : {QStringLiteral("Tangenta.Document"), QStringLiteral("Tangenta.Image")}) {
        classes.setValue(progId + QStringLiteral("/."), progId == QLatin1String("Tangenta.Document")
                                                            ? tr("TANGENTA-Zeichnung") : tr("Bild (TANGENTA)"));
        classes.setValue(progId + QStringLiteral("/DefaultIcon/."), exe + QStringLiteral(",0"));
        classes.setValue(progId + QStringLiteral("/shell/open/command/."), command);
    }
    for (const QString& suffix : {QStringLiteral("vek"), QStringLiteral("svg"), QStringLiteral("nef"), QStringLiteral("png")}) {
        const QString ext = QStringLiteral(".") + suffix;
        const bool on = suffixes.contains(suffix);
        if (suffix == QLatin1String("vek")) {
            // Eigenes Format: Standardprogramm
            if (on)
                classes.setValue(ext + QStringLiteral("/."), QStringLiteral("Tangenta.Document"));
            else if (classes.value(ext + QStringLiteral("/.")).toString() == QLatin1String("Tangenta.Document"))
                classes.remove(ext + QStringLiteral("/."));
        } else if (on) {
            classes.setValue(ext + QStringLiteral("/OpenWithProgids/Tangenta.Image"), QString());
        } else {
            classes.remove(ext + QStringLiteral("/OpenWithProgids/Tangenta.Image"));
        }
    }
    classes.sync();
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
#else
    Q_UNUSED(suffixes);
#endif
}

void MainWindow::rememberLastFile(const QString& path)
{
    if (!path.isEmpty())
        QSettings().setValue(QStringLiteral("files/lastFile"), QFileInfo(path).absoluteFilePath());
}

void MainWindow::open()
{
    QSettings settings;
    const QString path = QFileDialog::getOpenFileName(this, tr("Öffnen"),
                                                      settings.value(QStringLiteral("files/lastDir")).toString(),
                                                      openFilter());
    if (path.isEmpty())
        return;
    settings.setValue(QStringLiteral("files/lastDir"), QFileInfo(path).absolutePath());
    // Fremdformate und Bilder fragen in openFile() selbst nach dem Speichern
    if (QFileInfo(path).suffix().toLower() == QLatin1String(io::FileSuffix) && !maybeSave())
        return;
    openFile(path);
}

QString MainWindow::openFilter()
{
    auto patterns = [](const QStringList& suffixes) {
        QStringList out;
        for (const QString& s : suffixes)
            out << QStringLiteral("*.") + s;
        return out.join(QLatin1Char(' '));
    };
    QStringList images;
    for (const QString& suffix : imagepool::importSuffixes())
        if (!vectorImportSuffixes().contains(suffix) && suffix != QLatin1String(io::FileSuffix))
            images << suffix;
    const QStringList all = QStringList{QLatin1String(io::FileSuffix)} + vectorImportSuffixes() + images;
    return QStringList{tr("Alle unterstützten Dateien (%1)").arg(patterns(all)), fileFilter(),
                       tr("Zeichnungen aus anderen Programmen (%1)")
                           .arg(patterns({QStringLiteral("cdr"), QStringLiteral("cmx")})),
                       tr("PDF und Illustrator (%1)").arg(patterns({QStringLiteral("pdf"), QStringLiteral("ai")})),
                       tr("SVG, auch aus Inkscape (%1)").arg(patterns({QStringLiteral("svg")})),
                       tr("PostScript und DXF (%1)")
                           .arg(patterns({QStringLiteral("eps"), QStringLiteral("ps"), QStringLiteral("dxf")})),
                       tr("Bilder (%1)").arg(patterns(images)), tr("Alle Dateien (*)")}
        .join(QStringLiteral(";;"));
}

QStringList MainWindow::vectorImportSuffixes()
{
    return {QStringLiteral("svg"), QStringLiteral("pdf"), QStringLiteral("ai"), QStringLiteral("eps"), QStringLiteral("ps"), QStringLiteral("dxf"),
            QStringLiteral("cdr"), QStringLiteral("cmx")};
}

bool MainWindow::importVectorFile(const QString& path, bool asNewDocument)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QString suffix = QFileInfo(path).suffix().toLower();
    QString error;
    std::vector<svg::Import> pages;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QByteArray data = file.readAll();
    if (suffix == QLatin1String("svg")) {
        pages.push_back(svg::read(data, &error));
    } else if (suffix == QLatin1String("dxf")) {
        pages.push_back(dxf::read(data, &error, QFileInfo(path).absolutePath()));
    } else if (suffix == QLatin1String("cdr") || suffix == QLatin1String("cmx")) {
        pages = cdr::read(data, &error);
    } else {
        // PDF und PDF-kompatibles AI direkt; EPS, PostScript und altes AI über Ghostscript
        if (!data.left(1024).contains("%PDF"))
            data = pdf::postscriptToPdf(data, &error);
        if (!data.isEmpty())
            pages = pdf::read(data, &error);
    }
    QApplication::restoreOverrideCursor();
    if (pages.empty() || !pages.front().group || pages.front().group->childCount() == 0) {
        QMessageBox::warning(this, tr("Importieren"), tr("%1 liess sich nicht lesen.\n\n%2").arg(path, error));
        return false;
    }
    // Fehlende Schriften: ersetzen oder als Kurven (vor dem Einfügen, Texte liegen noch in den Gruppen)
    if (FontMatchDialog::needed(pages.front().fonts)) {
        FontMatchDialog dialog(pages.front().fonts, this);
        if (dialog.exec() != QDialog::Accepted)
            return false;
        dialog.apply();
    }
    if (asNewDocument) {
        // Jede Seite der Datei wird eine Seite des Dokuments
        const QSizeF size = pages.front().size.isValid() && pages.front().size.width() > 1 ? pages.front().size : Page::A4;
        createDocument(size, int(pages.size()));
        for (int i = 0; i < int(pages.size()); ++i) {
            m_editor.setCurrentPageIndex(i);
            if (pages[std::size_t(i)].group && pages[std::size_t(i)].group->childCount() > 0)
                m_editor.importGroup(std::move(pages[std::size_t(i)].group), false, tr("Importieren"));
        }
        m_editor.setCurrentPageIndex(0);
    } else {
        m_editor.importGroup(std::move(pages.front().group), true, tr("Importieren"));
    }
    return true;
}

void MainWindow::searchImages()
{
    ImageSearchDialog dialog(this);
    connect(&dialog, &ImageSearchDialog::optionsRequested, this, [this, &dialog] {
        OptionsDialog optionsDialog(m_editor, this, &dialog);
        optionsDialog.selectPage(tr("Global/Bildersuche"));
        optionsDialog.exec();
    });
    if (dialog.exec() != QDialog::Accepted)
        return;
    if (imagesearch::insert(m_editor, dialog.hit(), dialog.data()) == InvalidNodeId) {
        QMessageBox::warning(this, tr("Bildersuche"), tr("Das heruntergeladene Bild lässt sich nicht lesen."));
        return;
    }
    // Der Nachweis steht im Objektnamen (Objekt-Manager); die Bedingung hier noch einmal
    statusBar()->showMessage(imagesearch::attribution(dialog.hit()) + QStringLiteral(" – ") + dialog.hit().condition, 15000);
}

void MainWindow::importFiles()
{
    QSettings settings;
    QStringList images, vectors;
    for (const QString& suffix : imagepool::importSuffixes())
        if (!vectorImportSuffixes().contains(suffix))
            images << QStringLiteral("*.") + suffix;
    for (const QString& suffix : vectorImportSuffixes())
        vectors << QStringLiteral("*.") + suffix;
    const QStringList paths = QFileDialog::getOpenFileNames(
        this, tr("Importieren"), settings.value(QStringLiteral("files/importDir")).toString(),
        tr("Alle unterstützten Formate (%1 %2);;Vektorgrafik (%2);;Bilder (%1)").arg(images.join(QLatin1Char(' ')), vectors.join(QLatin1Char(' '))));
    if (paths.isEmpty())
        return;
    settings.setValue(QStringLiteral("files/importDir"), QFileInfo(paths.first()).absolutePath());
    for (const QString& path : paths) {
        if (vectorImportSuffixes().contains(QFileInfo(path).suffix().toLower())) {
            importVectorFile(path, false);
            continue;
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly) || m_editor.importImage(file.readAll()) == InvalidNodeId)
            QMessageBox::warning(this, tr("Importieren"), tr("%1 ist kein lesbares Bild.").arg(path));
    }
}

QList<int> MainWindow::allPages() const
{
    QList<int> pages;
    for (int i = 0; i < m_editor.document().pageCount(); ++i)
        pages << i;
    return pages;
}

namespace {

// Ganzseitenvorschau (F9): nur die Zeichnung, bildschirmfüllend; Bild auf/ab
// blättert, jede andere Taste oder ein Klick schliesst
class FullPreview final : public QWidget {
public:
    FullPreview(Canvas& canvas, QList<NodeId> only) : QWidget(nullptr, Qt::Window | Qt::FramelessWindowHint), m_canvas(canvas), m_only(std::move(only))
    {
        setAttribute(Qt::WA_DeleteOnClose);
        setWindowTitle(QObject::tr("Ganzseitenvorschau"));
        m_page = canvas.editor().currentPageIndex();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.fillRect(rect(), QColor(0x30, 0x30, 0x30));
        const QSizeF mm = m_canvas.editor().document().page(m_page).size();
        const double pxPerMm = std::min((width() - 40) / mm.width(), (height() - 40) / mm.height());
        const QPointF origin((width() - mm.width() * pxPerMm) / 2.0, (height() - mm.height() * pxPerMm) / 2.0);
        p.fillRect(QRectF(origin, mm * pxPerMm), Qt::white);
        m_canvas.setRenderFilter(m_only);
        m_canvas.renderPage(p, m_page, pxPerMm, origin, true);
        m_canvas.setRenderFilter({});
    }
    void keyPressEvent(QKeyEvent* e) override
    {
        const int count = m_canvas.editor().document().pageCount();
        if (e->key() == Qt::Key_PageDown && m_page + 1 < count) {
            ++m_page;
            update();
        } else if (e->key() == Qt::Key_PageUp && m_page > 0) {
            --m_page;
            update();
        } else {
            close();
        }
    }
    void mousePressEvent(QMouseEvent*) override { close(); }

private:
    Canvas& m_canvas;
    QList<NodeId> m_only;
    int m_page = 0;
};

} // namespace

void MainWindow::tableCommand(const QString& label, const std::function<void(TableObject&, const TableRange&)>& fn)
{
    TableTool& tool = m_canvas->tableTool();
    const bool editing = m_canvas->toolId() == ToolId::Table && tool.editing();
    NodeId id = InvalidNodeId;
    TableRange range;
    if (editing) {
        id = tool.target();
        const TableObject::Range r = tool.selection();
        range = {r.row0, r.col0, r.row1, r.col1};
        tool.finishEditing();
    } else {
        const QList<const DrawObject*> objects = m_editor.selectedObjects();
        const auto* table = objects.size() == 1 ? node_cast<TableObject>(objects.first()) : nullptr;
        if (!table)
            return;
        id = table->id();
        range = {0, 0, table->rows() - 1, table->columns() - 1};
    }
    m_editor.modifyTable(id, [&](TableObject& t) { fn(t, range); }, label);
    if (editing && m_editor.document().findAs<TableObject>(id)) {
        const auto* t = m_editor.document().findAs<TableObject>(id);
        tool.beginEditing(id, std::min(range.row0, t->rows() - 1), std::min(range.col0, t->columns() - 1));
    }
}

void MainWindow::convertToBitmap()
{
    const QList<const DrawObject*> objects = m_editor.selectedObjects();
    if (objects.isEmpty())
        return;
    // In Bitmap konvertieren: Auflösung, transparenter Hintergrund
    QDialog dialog(this);
    dialog.setWindowTitle(tr("In Bitmap konvertieren"));
    auto* form = new QFormLayout(&dialog);
    auto* dpi = new QSpinBox(&dialog);
    dpi->setRange(36, 2400);
    dpi->setSuffix(tr(" dpi"));
    dpi->setValue(Options::instance().integer(QStringLiteral("doc/page/resolution")));
    auto* transparent = new QCheckBox(tr("Transparenter Hintergrund"), &dialog);
    transparent->setChecked(true);
    form->addRow(tr("Auflösung:"), dpi);
    form->addRow(transparent);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form->addRow(buttons);
    if (dialog.exec() != QDialog::Accepted)
        return;
    QRectF bounds;
    for (const DrawObject* o : objects) {
        const double pad = o->style().stroke ? o->style().strokeWidth / 2.0 : 0.0;
        const QRectF r = o->boundingRect().adjusted(-pad, -pad, pad, pad);
        bounds = bounds.isNull() ? r : bounds.united(r);
    }
    const double pxPerMm = dpi->value() / 25.4;
    const QSize size(std::max(1, int(std::ceil(bounds.width() * pxPerMm))), std::max(1, int(std::ceil(bounds.height() * pxPerMm))));
    QImage pixels(size, QImage::Format_ARGB32_Premultiplied);
    pixels.fill(transparent->isChecked() ? Qt::transparent : Qt::white);
    pixels.setDotsPerMeterX(int(std::lround(pxPerMm * 1000)));
    pixels.setDotsPerMeterY(int(std::lround(pxPerMm * 1000)));
    QApplication::setOverrideCursor(Qt::WaitCursor);
    {
        QPainter p(&pixels);
        m_canvas->setRenderFilter(m_editor.selection());
        m_canvas->renderPage(p, m_editor.currentPageIndex(), pxPerMm, -bounds.topLeft() * pxPerMm, false);
        m_canvas->setRenderFilter({});
    }
    m_editor.replaceSelectionWithImage(pixels, QRectF(bounds.topLeft(), QSizeF(size.width() / pxPerMm, size.height() / pxPerMm)),
                                       tr("In Bitmap konvertieren"));
    QApplication::restoreOverrideCursor();
}

void MainWindow::addBitmapCommands(QMenu* bitmaps)
{
    // Befehle oben im Menü
    QAction* first = bitmaps->actions().value(0);
    QList<QAction*> top;
    auto* toBitmap = new QAction(icon(QStringLiteral("to-bitmap")), tr("In Bitmap &konvertieren …"), bitmaps);
    connect(toBitmap, &QAction::triggered, this, [this] { convertToBitmap(); });
    m_selectionActions << toBitmap;
    auto* sep = new QAction(bitmaps);
    sep->setSeparator(true);
    top << toBitmap << sep;
    bitmaps->insertActions(first, top);

    bitmaps->addSeparator();
    m_bitmapActions << bitmaps->addAction(tr("Bitmap &zuschneiden"), this, [this] {
        if (const ImageObject* image = m_editor.selectedImage())
            m_editor.cropBitmapToFrame(image->id());
    });
    m_bitmapActions << bitmaps->addAction(tr("Bitmap-&Farbmaske …"), this, [this] {
        const ImageObject* image = m_editor.selectedImage();
        if (!image)
            return;
        QDialog dialog(this);
        dialog.setWindowTitle(tr("Bitmap-Farbmaske"));
        auto* form = new QFormLayout(&dialog);
        auto* colorButton = new QPushButton(&dialog);
        Color color(255, 255, 255);
        auto show = [colorButton](const Color& c) {
            colorButton->setText(c.toHex());
            colorButton->setStyleSheet(QStringLiteral("background:%1").arg(c.toQColor().name()));
        };
        show(color);
        connect(colorButton, &QPushButton::clicked, &dialog, [&] {
            if (const std::optional<Color> c = ColorDialog::getColor(&dialog, tr("Farbe ausblenden"), color)) {
                color = *c;
                show(color);
            }
        });
        auto* tolerance = new QSpinBox(&dialog);
        tolerance->setRange(0, 100);
        tolerance->setSuffix(QStringLiteral(" %"));
        tolerance->setValue(10);
        form->addRow(tr("Farbe ausblenden:"), colorButton);
        form->addRow(tr("Toleranz:"), tolerance);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        form->addRow(buttons);
        if (dialog.exec() == QDialog::Accepted)
            applyImageEffect(tr("Bitmap-Farbmaske"), [color, t = tolerance->value()](const QImage& i) { return filters::colorMask(i, {color}, t); });
    });
    QMenu* mode = bitmaps->addMenu(tr("&Modus"));
    m_bitmapActions << mode->addAction(tr("&Schwarzweiss (1 Bit) …"), this, [this] {
        if (const ImageObject* image = m_editor.selectedImage()) {
            EffectDialog dialog(tr("Schwarzweiss"), image->image(), {SliderSpec{tr("Schwelle:"), 0, 255, 128}},
                                [](const QImage& i, const QList<int>& v) { return filters::toBlackWhite(i, v[0]); }, this);
            runImageDialog(tr("Modus Schwarzweiss"), dialog);
        }
    });
    for (int bits : {8, 16})
        m_bitmapActions << mode->addAction(tr("&Graustufen (%1 Bit)").arg(bits), this, [this, bits] {
            applyImageEffect(tr("Modus Graustufen"), [bits](const QImage& i) { return filters::toGrayscale(i, bits); });
        });
    for (int bits : {8, 16})
        m_bitmapActions << mode->addAction(tr("&Duoton (%1 Bit) …").arg(bits), this, [this, bits] {
            if (const ImageObject* image = m_editor.selectedImage()) {
                EffectDialog dialog(tr("Duoton"), image->image(),
                                    {ColorSpec{tr("Farbe 1 (Tiefen):"), Color(0, 0, 0)}, ColorSpec{tr("Farbe 2:"), Color(150, 95, 40)}},
                                    {SliderSpec{tr("Gewichtung Farbe 2:"), 0, 100, 50}},
                                    [bits](const QImage& i, const QList<int>& v, const QList<Color>& c) {
                                        return filters::duotone(i, c[0], c[1], v[0], bits);
                                    },
                                    this);
                runImageDialog(tr("Modus Duoton"), dialog);
            }
        });
    m_bitmapActions << bitmaps->addAction(tr("Bitmap &neu aufbauen …"), this, [this] {
        const ImageObject* image = m_editor.selectedImage();
        if (!image)
            return;
        const QImage pixels = image->image();
        QDialog dialog(this);
        dialog.setWindowTitle(tr("Bitmap neu aufbauen"));
        auto* form = new QFormLayout(&dialog);
        const double mmW = image->size().width();
        auto* dpi = new QSpinBox(&dialog);
        dpi->setRange(10, 4800);
        dpi->setSuffix(tr(" dpi"));
        dpi->setValue(int(std::lround(pixels.width() / (mmW / 25.4))));
        auto* info = new QLabel(&dialog);
        auto update = [&] {
            const double f = dpi->value() / (pixels.width() / (mmW / 25.4));
            info->setText(tr("%1 × %2 Pixel").arg(std::lround(pixels.width() * f)).arg(std::lround(pixels.height() * f)));
        };
        connect(dpi, &QSpinBox::valueChanged, &dialog, update);
        update();
        form->addRow(tr("Auflösung:"), dpi);
        form->addRow(tr("Neue Grösse:"), info);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        form->addRow(buttons);
        if (dialog.exec() != QDialog::Accepted)
            return;
        const double f = dpi->value() / (pixels.width() / (mmW / 25.4));
        applyImageEffect(tr("Bitmap neu aufbauen"), [f](const QImage& i) {
            return i.scaled(std::max(1, int(std::lround(i.width() * f))), std::max(1, int(std::lround(i.height() * f))), Qt::IgnoreAspectRatio,
                            Qt::SmoothTransformation);
        });
    });
    bitmaps->addSeparator();

    // Filtergruppen; jeder Filter mit Vorschau-Dialog
    struct Filter {
        QString name;
        QList<SliderSpec> sliders;
        std::function<QImage(const QImage&, const QList<int>&)> fn;
        QList<ColorSpec> colors = {}; // mit Farbfeldern: colorFn statt fn
        EffectDialog::ColorFunction colorFn = {};
        QList<int> pixels = {}; // Regler in Pixeln: für die verkleinerte Vorschau mitskalieren
    };
    using F = Filter;
    using V = const QList<int>&;
    const QList<std::pair<QString, QList<F>>> groups{
        {tr("&3D-Effekte"),
         {F{tr("3D-Drehung"), {{tr("Senkrecht (Grad):"), -75, 75, 20}, {tr("Waagrecht (Grad):"), -75, 75, 20}},
            [](const QImage& i, V v) { return filters::rotate3d(i, v[0], v[1]); }},
          F{tr("Perspektive"), {{tr("Waagrecht (%):"), -100, 100, 40}, {tr("Senkrecht (%):"), -100, 100, 0}},
            [](const QImage& i, V v) { return filters::perspective(i, v[0], v[1]); }},
          F{tr("Sphäre"), {{tr("Stärke:"), 0, 100, 60}}, [](const QImage& i, V v) { return filters::sphere(i, v[0]); }}}},
        {tr("Künstlerische &Striche"),
         {F{tr("Posterisieren"), {{tr("Stufen:"), 2, 32, 6}}, [](const QImage& i, V v) { return filters::posterize(i, v[0]); }},
          F{tr("Kohle"), {{tr("Grösse:"), 1, 20, 3}, {tr("Kante:"), 0, 100, 50}}, [](const QImage& i, V v) { return filters::charcoal(i, v[0], v[1]); },
            {}, {}, {0}},
          F{tr("Impressionismus"), {{tr("Strichgrösse (Pixel):"), 3, 100, 16}, {tr("Färbung:"), 0, 100, 30}, {tr("Helligkeit:"), -100, 100, 0}},
            [](const QImage& i, V v) { return filters::impressionism(i, v[0], v[1], v[2]); }, {}, {}, {0}},
          F{tr("Wasserfarben"), {{tr("Pinselgrösse:"), 1, 50, 5}, {tr("Körnung:"), 0, 100, 50}, {tr("Wassermenge:"), 0, 100, 50}, {tr("Helligkeit:"), -100, 100, 0}},
            [](const QImage& i, V v) { return filters::watercolor(i, v[0], v[1], v[2], v[3]); }, {}, {}, {0}},
          F{tr("Papierschnitzel"), {{tr("Dichte:"), 1, 100, 50}, {tr("Strichbreite:"), 1, 100, 50}, {tr("Farbig (1/0):"), 0, 1, 0}},
            [](const QImage& i, V v) { return filters::scraperboard(i, v[0], v[1], v[2] == 1); }},
          F{tr("Wellenpapier"), {{tr("Pinseldruck:"), 0, 100, 50}, {tr("Farbig (1/0):"), 0, 1, 1}},
            [](const QImage& i, V v) { return filters::wavyPaper(i, v[0], v[1] == 1); }}}},
        {tr("&Unschärfe"),
         {F{tr("Glätten"), {{tr("Stärke:"), 1, 100, 40}}, [](const QImage& i, V v) { return filters::smooth(i, v[0]); }},
          F{tr("Weichzeichnen"), {{tr("Prozent:"), 1, 100, 50}}, [](const QImage& i, V v) { return filters::soften(i, v[0]); }}}},
        {tr("Kr&eativ"),
         {F{tr("Gewebe"), {{tr("Grösse (Pixel):"), 3, 200, 16}, {tr("Komplexität:"), 0, 100, 50}, {tr("Helligkeit:"), -100, 100, 0}, {tr("Drehung (Grad):"), -90, 90, 0}},
            [](const QImage& i, V v) { return filters::fabric(i, v[0], v[1], v[2], v[3]); }, {}, {}, {0}},
          F{tr("Glasblock"), {{tr("Blockbreite (Pixel):"), 4, 2000, 60}, {tr("Blockhöhe (Pixel):"), 4, 2000, 60}},
            [](const QImage& i, V v) { return filters::glassBlock(i, v[0], v[1]); }, {}, {}, {0, 1}},
          F{tr("Rauchglas"), {{tr("Färbung (%):"), 0, 100, 50}, {tr("Unschärfe (%):"), 0, 100, 20}}, {},
            {ColorSpec{tr("Farbe:"), Color(32, 32, 32)}},
            [](const QImage& i, V v, const QList<Color>& c) { return filters::smokedGlass(i, c[0], v[0], v[1]); }}}},
    };
    for (const auto& [groupName, list] : groups) {
        QMenu* group = bitmaps->addMenu(groupName);
        m_bitmapActions << group->menuAction();
        for (const F& f : list) {
            m_bitmapActions << group->addAction(f.name + QStringLiteral(" …"), this, [this, f] {
                if (const ImageObject* image = m_editor.selectedImage()) {
                    // Pixelwerte auf die Grösse des bearbeiteten Bildes (Vorschau oder Original) umrechnen
                    const int fullWidth = std::max(1, image->image().width());
                    const QList<int> pixels = f.pixels;
                    auto scaled = [fullWidth, pixels](const QImage& i, QList<int> v) {
                        const double k = double(i.width()) / fullWidth;
                        if (k < 1.0)
                            for (int index : pixels)
                                v[index] = std::max(1, int(std::lround(v[index] * k)));
                        return v;
                    };
                    EffectDialog::ColorFunction fn = f.colorFn;
                    if (!fn)
                        fn = [plain = f.fn](const QImage& i, V v, const QList<Color>&) { return plain(i, v); };
                    EffectDialog dialog(f.name, image->image(), f.colors, f.sliders,
                                        [fn, scaled](const QImage& i, V v, const QList<Color>& c) { return fn(i, scaled(i, v), c); }, this);
                    runImageDialog(f.name, dialog);
                }
            });
        }
    }
}

void MainWindow::createTableMenu()
{
    // Tabelle
    QMenu* menu = menuBar()->addMenu(tr("Ta&belle"));
    menu->addAction(icon(QStringLiteral("tool-table")), tr("&Neue Tabelle erstellen …"), this, [this] {
        QDialog dialog(this);
        dialog.setWindowTitle(tr("Neue Tabelle erstellen"));
        auto* form = new QFormLayout(&dialog);
        auto* rows = new QSpinBox(&dialog);
        rows->setRange(1, 200);
        rows->setValue(Options::instance().integer(QStringLiteral("ws/tools/table/rows")));
        auto* cols = new QSpinBox(&dialog);
        cols->setRange(1, 200);
        cols->setValue(Options::instance().integer(QStringLiteral("ws/tools/table/cols")));
        auto mm = [&dialog](double v) {
            auto* s = new QDoubleSpinBox(&dialog);
            s->setRange(1, 5000);
            s->setSuffix(QStringLiteral(" mm"));
            s->setValue(v);
            return s;
        };
        auto* height = mm(60);
        auto* width = mm(120);
        form->addRow(tr("Zeilen:"), rows);
        form->addRow(tr("Spalten:"), cols);
        form->addRow(tr("Höhe:"), height);
        form->addRow(tr("Breite:"), width);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        form->addRow(buttons);
        if (dialog.exec() != QDialog::Accepted)
            return;
        auto table = std::make_unique<TableObject>(QSizeF(width->value(), height->value()), rows->value(), cols->value());
        const QPointF c = QRectF(QPointF(0, 0), m_editor.currentPage().size()).center();
        table->setTransform(QTransform::fromTranslate(c.x() - width->value() / 2, c.y() - height->value() / 2));
        m_editor.addObject(std::move(table), tr("Tabelle erstellen"));
    });
    auto separatorChoice = [this](const QString& title) -> std::optional<QString> {
        QStringList items{tr("Kommas"), tr("Tabulatoren"), tr("Absätze (eine Spalte)"), tr("Benutzerdefiniert")};
        bool ok = false;
        const QString pick = QInputDialog::getItem(this, title, tr("Spalten trennen durch:"), items, 1, false, &ok);
        if (!ok)
            return std::nullopt;
        const int i = int(items.indexOf(pick));
        if (i == 0)
            return QStringLiteral(",");
        if (i == 1)
            return QStringLiteral("\t");
        if (i == 2)
            return QString();
        const QString own = QInputDialog::getText(this, title, tr("Trennzeichen:"), QLineEdit::Normal, QStringLiteral(";"), &ok);
        return ok ? std::optional<QString>(own) : std::nullopt;
    };
    QAction* toTable = menu->addAction(tr("&Text in Tabelle konvertieren …"), this, [this, separatorChoice] {
        if (const auto sep = separatorChoice(tr("Text in Tabelle konvertieren")))
            m_editor.textToTable(*sep);
    });
    menu->addSeparator();

    using T = TableObject;
    QMenu* insert = menu->addMenu(tr("&Einfügen", "Tabelle: Zeilen oder Spalten einfügen"));
    insert->addAction(tr("Zeile &oben"), this, [this] { tableCommand(tr("Zeile einfügen"), [](T& t, const TableRange& r) { t.insertRow(r.row0); }); });
    insert->addAction(tr("Zeile &unten"), this, [this] { tableCommand(tr("Zeile einfügen"), [](T& t, const TableRange& r) { t.insertRow(r.row1 + 1); }); });
    insert->addAction(tr("Spalte &links"), this, [this] { tableCommand(tr("Spalte einfügen"), [](T& t, const TableRange& r) { t.insertColumn(r.col0); }); });
    insert->addAction(tr("Spalte &rechts"), this, [this] { tableCommand(tr("Spalte einfügen"), [](T& t, const TableRange& r) { t.insertColumn(r.col1 + 1); }); });
    insert->addSeparator();
    insert->addAction(tr("&Zeilen einfügen …"), this, [this] {
        bool ok = false;
        const int n = QInputDialog::getInt(this, tr("Zeilen einfügen"), tr("Anzahl Zeilen (unterhalb):"), 1, 1, 100, 1, &ok);
        if (ok)
            tableCommand(tr("Zeilen einfügen"), [n](T& t, const TableRange& r) {
                for (int i = 0; i < n; ++i)
                    t.insertRow(r.row1 + 1);
            });
    });
    insert->addAction(tr("&Spalten einfügen …"), this, [this] {
        bool ok = false;
        const int n = QInputDialog::getInt(this, tr("Spalten einfügen"), tr("Anzahl Spalten (rechts):"), 1, 1, 100, 1, &ok);
        if (ok)
            tableCommand(tr("Spalten einfügen"), [n](T& t, const TableRange& r) {
                for (int i = 0; i < n; ++i)
                    t.insertColumn(r.col1 + 1);
            });
    });

    QMenu* select = menu->addMenu(tr("A&uswählen"));
    auto selectWith = [this](auto make) {
        TableTool& tool = m_canvas->tableTool();
        if (m_canvas->toolId() != ToolId::Table || !tool.editing()) {
            const QList<const DrawObject*> objects = m_editor.selectedObjects();
            const auto* table = objects.size() == 1 ? node_cast<TableObject>(objects.first()) : nullptr;
            if (!table)
                return;
            m_canvas->setTool(ToolId::Table);
            tool.beginEditing(table->id(), 0, 0);
        }
        const auto* t = m_editor.document().findAs<TableObject>(tool.target());
        if (t)
            tool.selectRange(make(*t, tool.row(), tool.column()));
    };
    select->addAction(tr("&Tabelle"), this, [selectWith] { selectWith([](const T& t, int, int) { return T::Range{0, 0, t.rows() - 1, t.columns() - 1}; }); });
    select->addAction(tr("&Zeile"), this, [selectWith] { selectWith([](const T& t, int r, int) { return T::Range{r, 0, r, t.columns() - 1}; }); });
    select->addAction(tr("&Spalte"), this, [selectWith] { selectWith([](const T& t, int, int c) { return T::Range{0, c, t.rows() - 1, c}; }); });

    QMenu* remove = menu->addMenu(tr("&Löschen"));
    remove->addAction(tr("&Zeile"), this, [this] {
        tableCommand(tr("Zeile löschen"), [](T& t, const TableRange& r) {
            for (int i = r.row1; i >= r.row0; --i)
                t.removeRow(i);
        });
    });
    remove->addAction(tr("&Spalte"), this, [this] {
        tableCommand(tr("Spalte löschen"), [](T& t, const TableRange& r) {
            for (int i = r.col1; i >= r.col0; --i)
                t.removeColumn(i);
        });
    });
    remove->addAction(tr("&Tabelle"), this, [this] {
        TableTool& tool = m_canvas->tableTool();
        NodeId id = tool.editing() ? tool.target() : InvalidNodeId;
        tool.finishEditing();
        if (id)
            m_editor.setSelection({id});
        m_editor.deleteSelection();
    });

    QMenu* distribute = menu->addMenu(tr("&Verteilen"));
    distribute->addAction(tr("Zeilen &gleichmässig verteilen"), this, [this] {
        tableCommand(tr("Zeilen verteilen"), [](T& t, const TableRange& r) {
            r.row0 == r.row1 ? t.distributeRows(0, t.rows() - 1) : t.distributeRows(r.row0, r.row1);
        });
    });
    distribute->addAction(tr("Spalten gleichmässig &verteilen"), this, [this] {
        tableCommand(tr("Spalten verteilen"), [](T& t, const TableRange& r) {
            r.col0 == r.col1 ? t.distributeColumns(0, t.columns() - 1) : t.distributeColumns(r.col0, r.col1);
        });
    });
    menu->addSeparator();
    menu->addAction(icon(QStringLiteral("table-merge")), tr("Zellen &verbinden"), QKeySequence(Qt::CTRL | Qt::Key_M), this, [this] {
        tableCommand(tr("Zellen verbinden"), [](T& t, const TableRange& r) { t.merge(T::Range{r.row0, r.col0, r.row1, r.col1}); });
    });
    auto splitInto = [this](bool intoRows) {
        bool ok = false;
        const int n = QInputDialog::getInt(this, intoRows ? tr("In Zeilen aufteilen") : tr("In Spalten aufteilen"),
                                           intoRows ? tr("Anzahl Zeilen:") : tr("Anzahl Spalten:"), 2, 2, 50, 1, &ok);
        if (ok)
            tableCommand(intoRows ? tr("In Zeilen aufteilen") : tr("In Spalten aufteilen"), [n, intoRows](T& t, const TableRange& r) {
                intoRows ? t.splitIntoRows(r.row0, r.col0, n) : t.splitIntoColumns(r.row0, r.col0, n);
            });
    };
    menu->addAction(tr("In &Zeilen aufteilen …"), this, [splitInto] { splitInto(true); });
    menu->addAction(tr("In &Spalten aufteilen …"), this, [splitInto] { splitInto(false); });
    menu->addAction(tr("Zellen &teilen"), this, [this] {
        tableCommand(tr("Zellen teilen"), [](T& t, const TableRange& r) {
            for (int row = r.row0; row <= r.row1; ++row)
                for (int col = r.col0; col <= r.col1; ++col)
                    if (!t.isCovered(row, col))
                        t.unmerge(row, col);
        });
    });
    menu->addSeparator();
    QAction* toText = menu->addAction(tr("Tabelle in Te&xt konvertieren …"), this, [this, separatorChoice] {
        m_canvas->tableTool().finishEditing();
        if (const auto sep = separatorChoice(tr("Tabelle in Text konvertieren")))
            m_editor.tableToText(*sep);
    });
    connect(menu, &QMenu::aboutToShow, this, [this, toTable, toText, insert, select, remove, distribute] {
        const QList<const DrawObject*> objects = m_editor.selectedObjects();
        const bool text = objects.size() == 1 && node_cast<TextObject>(objects.first());
        const bool table = (objects.size() == 1 && node_cast<TableObject>(objects.first()))
                           || (m_canvas->toolId() == ToolId::Table && m_canvas->tableTool().editing());
        toTable->setEnabled(text);
        toText->setEnabled(table);
        for (QMenu* m : {insert, select, remove, distribute})
            m->setEnabled(table);
    });
}

void MainWindow::showFullPreview(bool selectedOnly)
{
    auto* preview = new FullPreview(*m_canvas, selectedOnly ? m_editor.selection() : QList<NodeId>());
    preview->showFullScreen();
}

void MainWindow::lensDialog()
{
    // Andockfenster "Linse" (Alt+F3)
    m_lensDock->show();
    m_lensDock->raise();
}

void MainWindow::bevelDialog()
{
    const QList<const DrawObject*> objects = m_editor.selectedObjects();
    if (objects.isEmpty())
        return;
    Bevel bevel = objects.first()->bevel();
    if (!bevel.isActive())
        bevel.style = Bevel::Style::Soft;
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Abschrägung"));
    auto* form = new QFormLayout(&dialog);
    auto* style = new QComboBox(&dialog);
    style->addItems({tr("Keine"), tr("Weiche Kante"), tr("Relief")});
    style->setCurrentIndex(int(bevel.style));
    auto spin = [&dialog](double min, double max, double v, const QString& suffix) {
        auto* s = new QDoubleSpinBox(&dialog);
        s->setRange(min, max);
        s->setValue(v);
        s->setSuffix(suffix);
        return s;
    };
    auto* width = spin(0.1, 100, bevel.width, QStringLiteral(" mm"));
    auto* intensity = spin(0, 100, bevel.intensity, QStringLiteral(" %"));
    auto* direction = spin(0, 360, bevel.direction, QStringLiteral(" °"));
    auto* altitude = spin(1, 89, bevel.altitude, QStringLiteral(" °"));
    form->addRow(tr("Stil:"), style);
    form->addRow(tr("Abstand:"), width);
    form->addRow(tr("Intensität:"), intensity);
    form->addRow(tr("Lichtrichtung:"), direction);
    form->addRow(tr("Lichthöhe:"), altitude);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form->addRow(buttons);
    if (dialog.exec() != QDialog::Accepted)
        return;
    bevel.style = Bevel::Style(style->currentIndex());
    bevel.width = width->value();
    bevel.intensity = intensity->value();
    bevel.direction = direction->value();
    bevel.altitude = altitude->value();
    m_editor.setSelectionBevel(bevel);
}

void MainWindow::multiDuplicate()
{
    if (m_editor.selection().isEmpty())
        return;
    // Vielfach duplizieren: Anzahl und Versatz je Kopie
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Vielfach duplizieren"));
    auto* form = new QFormLayout(&dialog);
    auto* count = new QSpinBox(&dialog);
    count->setRange(1, 1000);
    count->setValue(3);
    auto offset = [&dialog](double v) {
        auto* s = new QDoubleSpinBox(&dialog);
        s->setRange(-10000, 10000);
        s->setDecimals(3);
        s->setSuffix(QStringLiteral(" mm"));
        s->setValue(v);
        return s;
    };
    const QPointF d = m_editor.duplicateOffset();
    auto* dx = offset(d.x());
    auto* dy = offset(d.y());
    form->addRow(tr("Anzahl Kopien:"), count);
    form->addRow(tr("Versatz waagrecht:"), dx);
    form->addRow(tr("Versatz senkrecht:"), dy);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form->addRow(buttons);
    if (dialog.exec() == QDialog::Accepted)
        m_editor.transformSelectionCopies(QTransform::fromTranslate(dx->value(), dy->value()), count->value(), tr("Vielfach duplizieren"));
}

void MainWindow::copyPropertiesDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Eigenschaften kopieren"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* pen = new QCheckBox(tr("Umrissstift (Breite, Linienstil, Pfeile)"), &dialog);
    auto* color = new QCheckBox(tr("Umrissfarbe"), &dialog);
    auto* fill = new QCheckBox(tr("Füllung"), &dialog);
    auto* text = new QCheckBox(tr("Texteigenschaften"), &dialog);
    fill->setChecked(true);
    for (QCheckBox* c : {pen, color, fill, text})
        layout->addWidget(c);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const int what = (pen->isChecked() ? Editor::CopyOutlinePen : 0) | (color->isChecked() ? Editor::CopyOutlineColor : 0)
                     | (fill->isChecked() ? Editor::CopyFill : 0) | (text->isChecked() ? Editor::CopyText : 0);
    // Danach das Objekt anklicken, von dem kopiert wird
    m_canvas->setTool(ToolId::Pick);
    m_canvas->pickTool().beginPickTarget([this, what](NodeId source) { m_editor.copyPropertiesFrom(source, what); });
    statusBar()->showMessage(tr("Objekt anklicken, von dem die Eigenschaften kopiert werden"), 8000);
}

void MainWindow::findObjectsDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Objekte suchen"));
    auto* form = new QFormLayout(&dialog);
    auto* type = new QComboBox(&dialog);
    const QList<std::pair<QString, QString>> types{
        {tr("Alle Arten"), QString()}, {tr("Rechtecke"), QStringLiteral("rectangle")}, {tr("Ellipsen"), QStringLiteral("ellipse")},
        {tr("Polygone und Sterne"), QStringLiteral("polygon")}, {tr("Kurven"), QStringLiteral("curve")}, {tr("Text"), QStringLiteral("text")},
        {tr("Bilder"), QStringLiteral("image")}, {tr("Gruppen"), QStringLiteral("group")}, {tr("Tabellen"), QStringLiteral("table")},
        {tr("Klone und Symbole"), QStringLiteral("clone")}};
    for (const auto& [text, id] : types)
        type->addItem(text, id);
    auto* name = new QLineEdit(&dialog);
    auto* sameFill = new QCheckBox(tr("Gleiche Füllung wie das ausgewählte Objekt"), &dialog);
    auto* sameStroke = new QCheckBox(tr("Gleiche Kontur wie das ausgewählte Objekt"), &dialog);
    const QList<const DrawObject*> sel = m_editor.selectedObjects();
    sameFill->setEnabled(!sel.isEmpty());
    sameStroke->setEnabled(!sel.isEmpty());
    form->addRow(tr("Art:"), type);
    form->addRow(tr("Name enthält:"), name);
    form->addRow(sameFill);
    form->addRow(sameStroke);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Auswählen"));
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form->addRow(buttons);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const QString kind = type->currentData().toString();
    const QString part = name->text().trimmed();
    const Style ref = sel.isEmpty() ? Style() : sel.first()->style();
    const bool fillOn = sameFill->isChecked() && !sel.isEmpty(), strokeOn = sameStroke->isChecked() && !sel.isEmpty();
    const int n = m_editor.selectWhere([&](const DrawObject& o) {
        return (kind.isEmpty() || o.typeName() == kind) && (part.isEmpty() || o.name().contains(part, Qt::CaseInsensitive))
               && (!fillOn || o.style().fill == ref.fill)
               && (!strokeOn || (o.style().stroke == ref.stroke && o.style().strokeWidth == ref.strokeWidth));
    });
    statusBar()->showMessage(tr("%n Objekt(e) gefunden", nullptr, n), 6000);
}

void MainWindow::replaceTextDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Text ersetzen"));
    auto* form = new QFormLayout(&dialog);
    auto* find = new QLineEdit(&dialog);
    auto* with = new QLineEdit(&dialog);
    auto* caseSensitive = new QCheckBox(tr("Gross-/Kleinschreibung beachten"), &dialog);
    auto* whole = new QCheckBox(tr("Nur ganze Wörter"), &dialog);
    form->addRow(tr("Suchen nach:"), find);
    form->addRow(tr("Ersetzen durch:"), with);
    form->addRow(caseSensitive);
    form->addRow(whole);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Alle ersetzen"));
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form->addRow(buttons);
    if (dialog.exec() != QDialog::Accepted)
        return;
    m_canvas->textTool().finishEditing();
    const int n = m_editor.replaceText(find->text(), with->text(), caseSensitive->isChecked(), whole->isChecked());
    statusBar()->showMessage(tr("%n Stelle(n) ersetzt", nullptr, n), 6000);
}

void MainWindow::showTransformDocker(int page)
{
    m_transformDocker->showPage(TransformDocker::Page(page));
    m_transformDock->show();
    m_transformDock->raise();
}

void MainWindow::showAlignDialog(int tab)
{
    if (!m_alignDialog)
        m_alignDialog = new AlignDialog(m_editor, this);
    if (tab == 1)
        m_alignDialog->showDistributeTab();
    else
        m_alignDialog->showAlignTab();
    m_alignDialog->show();
    m_alignDialog->raise();
    m_alignDialog->activateWindow();
}

void MainWindow::exportFile()
{
    QSettings settings;
    const QString base = m_editor.filePath().isEmpty() ? tr("Unbenannt") : QFileInfo(m_editor.filePath()).completeBaseName();
    const QString dir = settings.value(QStringLiteral("files/exportDir"), QDir::homePath()).toString();
    // Formate wie im klassischen Dialog "Exportieren"; die erste Endung gilt, wenn keine eingetippt wurde
    const QList<std::pair<QString, QStringList>> formats{
        {tr("PDF – Adobe Portable Document Format"), {QStringLiteral("pdf")}},
        {tr("SVG – Scalable Vector Graphics"), {QStringLiteral("svg")}},
        {tr("AI – Adobe Illustrator"), {QStringLiteral("ai")}},
        {tr("EPS – Encapsulated PostScript"), {QStringLiteral("eps")}},
        {tr("DXF – AutoCAD"), {QStringLiteral("dxf")}},
        {tr("TTF – TrueType-Schrift (Auswahl als Zeichen)"), {QStringLiteral("ttf")}},
        {tr("PNG – Portable Network Graphics"), {QStringLiteral("png")}},
        {tr("JPEG – Bitmaps"), {QStringLiteral("jpg"), QStringLiteral("jpeg")}},
        {tr("TIFF – Bitmap"), {QStringLiteral("tif"), QStringLiteral("tiff")}},
        {tr("WebP – Bild"), {QStringLiteral("webp")}},
        {tr("BMP – Windows Bitmap"), {QStringLiteral("bmp")}},
        {tr("GIF – CompuServe Bitmap"), {QStringLiteral("gif")}},
        {tr("ICO – Windows-Symbol"), {QStringLiteral("ico")}},
        {tr("PSD – Adobe Photoshop"), {QStringLiteral("psd")}},
        {tr("PCX – PaintBrush"), {QStringLiteral("pcx")}},
        {tr("HTML – alle Seiten"), {QStringLiteral("html"), QStringLiteral("htm")}},
    };
    QStringList filters;
    for (const auto& [name, suffixes] : formats) {
        QStringList patterns;
        for (const QString& sfx : suffixes)
            patterns << QStringLiteral("*.") + sfx;
        filters << QStringLiteral("%1 (%2)").arg(name, patterns.join(QLatin1Char(' ')));
    }
    QString chosen = filters.value(settings.value(QStringLiteral("files/exportFormat"), 0).toInt(), filters.first());
    const QString defaultSuffix = formats.value(int(filters.indexOf(chosen))).second.value(0, QStringLiteral("pdf"));
    QString path = QFileDialog::getSaveFileName(this, tr("Exportieren"), QDir(dir).filePath(base + QLatin1Char('.') + defaultSuffix),
                                                filters.join(QStringLiteral(";;")), &chosen);
    if (path.isEmpty())
        return;
    const int formatIndex = int(filters.indexOf(chosen));
    if (QFileInfo(path).suffix().isEmpty() && formatIndex >= 0)
        path += QLatin1Char('.') + formats[formatIndex].second.first();
    settings.setValue(QStringLiteral("files/exportDir"), QFileInfo(path).absolutePath());
    if (formatIndex >= 0)
        settings.setValue(QStringLiteral("files/exportFormat"), formatIndex);
    const QString suffix = QFileInfo(path).suffix().toLower();
    QString error;
    bool ok = false;
    if (suffix == QLatin1String("ttf")) {
        exportGlyph(path);
        return;
    }
    if (suffix == QLatin1String("html") || suffix == QLatin1String("htm")) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        ok = output::exportHtml(*m_canvas, path, base, &error);
    } else {
        const bool bitmap = output::imageExportSuffixes().contains(suffix == QLatin1String("jpeg") ? QStringLiteral("jpg")
                                                                    : suffix == QLatin1String("tiff") ? QStringLiteral("tif") : suffix);
        if (suffix == QLatin1String("pdf")) {
            // PDF wie "Als PDF freigeben": Einstellungen mit Voreinstellungen und Vorabkontrolle
            QString preset;
            PdfSettings settings = PdfSettings::last(&preset);
            if (!m_editor.selection().isEmpty())
                settings.range = PdfSettings::Range::Selection;
            PdfSettingsDialog dialog(*m_canvas, path, settings, preset, this);
            if (dialog.exec() == QDialog::Accepted)
                writePdf(path, dialog.settings());
            return;
        }
        const ExportDialog::Kind kind = suffix == QLatin1String("pdf") ? ExportDialog::Kind::Pdf
                                        : bitmap                         ? ExportDialog::Kind::Bitmap
                                                                         : ExportDialog::Kind::Vector;
        const QStringList vectorExports{QStringLiteral("svg"), QStringLiteral("eps"), QStringLiteral("ai"), QStringLiteral("dxf")};
        if (!bitmap && suffix != QLatin1String("pdf") && !vectorExports.contains(suffix)) {
            QMessageBox::warning(this, tr("Exportieren"), tr("Das Format .%1 kann TANGENTA nicht schreiben.").arg(suffix));
            return;
        }
        ExportDialog dialog(kind, suffix, *m_canvas, this);
        if (dialog.exec() != QDialog::Accepted)
            return;
        const output::ExportOptions options = dialog.options();
        QApplication::setOverrideCursor(Qt::WaitCursor);
        if (kind == ExportDialog::Kind::Pdf)
            ok = output::exportPdf(*m_canvas, path, options, &error);
        else if (bitmap)
            ok = output::exportImage(*m_canvas, path, options, &error);
        else if (suffix == QLatin1String("eps"))
            ok = output::exportEps(*m_canvas, path, options, &error);
        else if (suffix == QLatin1String("ai"))
            ok = output::exportAi(*m_canvas, path, options, &error);
        else if (suffix == QLatin1String("dxf"))
            ok = output::exportDxf(*m_canvas, path, options, &error);
        else
            ok = output::exportSvg(*m_canvas, path, options, &error);
    }
    QApplication::restoreOverrideCursor();
    if (!ok)
        QMessageBox::warning(this, tr("Exportieren"), tr("%1 konnte nicht exportiert werden.\n\n%2").arg(path, error));
    else
        statusBar()->showMessage(tr("Exportiert: %1").arg(QDir::toNativeSeparators(path)), 6000);
}

bool MainWindow::exportGlyph(const QString& path)
{
    if (m_editor.selection().isEmpty()) {
        QMessageBox::information(this, tr("Als Schriftzeichen exportieren"),
                                 tr("Zuerst die Objekte markieren, die das Zeichen bilden."));
        return false;
    }
    // Vorhandene TANGENTA-Schrift ergänzen
    std::optional<ttf::Font> existing;
    if (QFileInfo::exists(path)) {
        QFile f(path);
        QString error;
        if (f.open(QIODevice::ReadOnly))
            existing = ttf::read(f.readAll(), &error);
        if (!existing) {
            QMessageBox::warning(this, tr("Als Schriftzeichen exportieren"),
                                 tr("%1 lässt sich nicht ergänzen.\n\n%2").arg(QDir::toNativeSeparators(path), error));
            return false;
        }
    }
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Als Schriftzeichen exportieren"));
    auto* form = new QFormLayout(&dialog);
    auto* family = new QLineEdit(existing && !existing->family.isEmpty() ? existing->family : QFileInfo(path).completeBaseName(), &dialog);
    auto* character = new QLineEdit(QStringLiteral("A"), &dialog);
    character->setMaxLength(2);
    auto* height = new QSpinBox(&dialog);
    height->setRange(10, 2000);
    height->setValue(700);
    height->setSuffix(tr(" Einheiten"));
    auto* bottom = new QSpinBox(&dialog);
    bottom->setRange(-1000, 1000);
    bottom->setSuffix(tr(" Einheiten"));
    auto* bearing = new QSpinBox(&dialog);
    bearing->setRange(0, 500);
    bearing->setValue(50);
    bearing->setSuffix(tr(" Einheiten"));
    form->addRow(tr("Schriftname:"), family);
    form->addRow(tr("Zeichen:"), character);
    form->addRow(tr("Höhe des Objekts:"), height);
    form->addRow(tr("Unterkante (Grundlinie = 0):"), bottom);
    form->addRow(tr("Abstand links und rechts:"), bearing);
    auto* note = new QLabel(existing ? tr("Die Schrift enthält schon %n Zeichen; dieses wird ergänzt oder ersetzt.", nullptr,
                                          int(existing->glyphs.size()))
                                     : tr("Ein Geviert hat 1000 Einheiten; Versalien sind etwa 700 hoch."),
                            &dialog);
    note->setWordWrap(true);
    form->addRow(note);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form->addRow(buttons);
    if (dialog.exec() != QDialog::Accepted || character->text().isEmpty())
        return false;
    const QList<uint> ucs = character->text().toUcs4();
    output::ExportOptions options;
    options.page = m_editor.currentPageIndex();
    options.only = m_editor.selection();
    const QPainterPath outline = output::selectionOutline(*m_canvas, options);
    ttf::Font font = existing.value_or(ttf::Font{});
    font.family = family->text().trimmed().isEmpty() ? QStringLiteral("TANGENTA") : family->text().trimmed();
    const char32_t code = char32_t(ucs.value(0));
    font.glyphs.removeIf([&](const ttf::Glyph& g) { return g.code == code; });
    font.glyphs << ttf::glyphFromOutline(outline, code, height->value(), bottom->value(), bearing->value());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(ttf::write(font)) < 0) {
        QMessageBox::warning(this, tr("Als Schriftzeichen exportieren"), f.errorString());
        return false;
    }
    statusBar()->showMessage(tr("Zeichen »%1« in %2 geschrieben (%n Zeichen)", nullptr, int(font.glyphs.size()))
                                 .arg(character->text(), QDir::toNativeSeparators(path)),
                             6000);
    return true;
}

void MainWindow::sharePdf()
{
    QSettings settings;
    const QString base = m_editor.filePath().isEmpty() ? tr("Unbenannt") : QFileInfo(m_editor.filePath()).completeBaseName();
    const QString dir = settings.value(QStringLiteral("files/exportDir"), QDir::homePath()).toString();
    PdfSaveDialog dialog(*m_canvas, QDir(dir).filePath(base + QStringLiteral(".pdf")), this);
    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty())
        return;
    QString path = dialog.selectedFiles().first();
    if (QFileInfo(path).suffix().isEmpty())
        path += QStringLiteral(".pdf");
    settings.setValue(QStringLiteral("files/exportDir"), QFileInfo(path).absolutePath());
    PdfSettings::remember(dialog.settings(), dialog.presetName());
    writePdf(path, dialog.settings());
}

bool MainWindow::writePdf(const QString& path, const PdfSettings& settings)
{
    QString error;
    QStringList notes;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = pdfexport::exportPdf(*m_canvas, path, settings, &error, &notes);
    QApplication::restoreOverrideCursor();
    if (!ok) {
        QMessageBox::warning(this, tr("Als PDF freigeben"), tr("%1 konnte nicht geschrieben werden.\n\n%2").arg(QDir::toNativeSeparators(path), error));
        return false;
    }
    if (!notes.isEmpty())
        QMessageBox::information(this, tr("Als PDF freigeben"),
                                 tr("Das PDF ist geschrieben, aber ohne Ghostscript fehlt: %1.").arg(notes.join(QStringLiteral(", "))));
    statusBar()->showMessage(tr("PDF geschrieben: %1").arg(QDir::toNativeSeparators(path)), 6000);
    return true;
}

void MainWindow::publishHtml()
{
    QSettings settings;
    const QString base = m_editor.filePath().isEmpty() ? tr("Unbenannt") : QFileInfo(m_editor.filePath()).completeBaseName();
    const QString dir = settings.value(QStringLiteral("files/exportDir"), QDir::homePath()).toString();
    const QString path = QFileDialog::getSaveFileName(this, tr("Im Web veröffentlichen"),
                                                      QDir(dir).filePath(base + QStringLiteral(".html")), tr("HTML (*.html *.htm)"));
    if (path.isEmpty())
        return;
    settings.setValue(QStringLiteral("files/exportDir"), QFileInfo(path).absolutePath());
    QString error;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = output::exportHtml(*m_canvas, path, base, &error);
    QApplication::restoreOverrideCursor();
    if (!ok)
        QMessageBox::warning(this, tr("Im Web veröffentlichen"), error);
    else
        statusBar()->showMessage(tr("Veröffentlicht: %1").arg(QDir::toNativeSeparators(path)), 6000);
}

void MainWindow::printDocument()
{
    QPrinter printer(QPrinter::HighResolution);
    printer.setDocName(m_editor.filePath().isEmpty() ? tr("Unbenannt") : QFileInfo(m_editor.filePath()).completeBaseName());
    QPrintDialog dialog(&printer, this);
    dialog.setOption(QAbstractPrintDialog::PrintCurrentPage, true);
    dialog.setOption(QAbstractPrintDialog::PrintPageRange, true);
    dialog.setMinMax(1, m_editor.document().pageCount());
    if (Options::instance().flag(QStringLiteral("global/print/currentOnly")))
        dialog.setPrintRange(QAbstractPrintDialog::CurrentPage);
    if (dialog.exec() != QDialog::Accepted)
        return;
    QList<int> pages;
    switch (printer.printRange()) {
    case QPrinter::CurrentPage:
    case QPrinter::Selection:
        pages << m_editor.currentPageIndex();
        break;
    case QPrinter::PageRange:
        for (int i = std::max(1, printer.fromPage()); i <= std::min(printer.toPage(), m_editor.document().pageCount()); ++i)
            pages << i - 1;
        break;
    case QPrinter::AllPages:
        pages = allPages();
        break;
    }
    QString error;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = output::print(*m_canvas, printer, pages, &error);
    QApplication::restoreOverrideCursor();
    if (!ok)
        QMessageBox::warning(this, tr("Drucken"), error);
}

void MainWindow::printPreview()
{
    QPrinter printer(QPrinter::HighResolution);
    QPrintPreviewDialog dialog(&printer, this);
    dialog.setWindowTitle(tr("Druckvorschau"));
    const QList<int> pages = Options::instance().flag(QStringLiteral("global/print/currentOnly"))
                                 ? QList<int>{m_editor.currentPageIndex()}
                                 : allPages();
    connect(&dialog, &QPrintPreviewDialog::paintRequested, this, [this, pages](QPrinter* p) { output::print(*m_canvas, *p, pages); });
    dialog.exec();
}

bool MainWindow::openFile(const QString& path)
{
    // Bilder (auch Kamera-Rohdaten) öffnen sich als neues Dokument
    const QString suffix = QFileInfo(path).suffix().toLower();
    // Vektorgrafik öffnet sich als neues Dokument in ihrer Grösse
    if (vectorImportSuffixes().contains(suffix))
        return maybeSave() && importVectorFile(path, true);
    if (suffix != QLatin1String(io::FileSuffix) && imagepool::importSuffixes().contains(suffix)) {
        QFile file(path);
        if (!maybeSave() || !file.open(QIODevice::ReadOnly))
            return false;
        createDocument(Page::A4, 1);
        if (m_editor.importImage(file.readAll()) == InvalidNodeId) {
            QMessageBox::warning(this, tr("Öffnen"), tr("%1 ist kein lesbares Bild.").arg(path));
            return false;
        }
        return true;
    }
    QString error;
    std::unique_ptr<Document> doc = io::load(path, &error);
    if (!doc) {
        QMessageBox::critical(this, tr("Öffnen fehlgeschlagen"), tr("%1\n\n%2").arg(path, error));
        return false;
    }
    m_editor.setDocument(std::move(doc), path);
    rememberLastFile(path);
    return true;
}

void MainWindow::closeEvent(QCloseEvent* e)
{
    if (!maybeSave()) {
        e->ignore();
        return;
    }
    QSettings settings;
    settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("bake/steps"), m_editor.shapeDefaults().blurSteps);
    settings.setValue(QStringLiteral("bake/bands"), m_editor.shapeDefaults().blurBands);
    settings.setValue(QStringLiteral("window/state"), saveState(3));
    e->accept();
}

} // namespace vek
