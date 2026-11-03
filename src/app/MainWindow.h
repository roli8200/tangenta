#pragma once

#include "Editor.h"

#include "PdfSettings.h"

#include <QMainWindow>

#include <functional>

class QAction;
class QComboBox;
class QDockWidget;
class QLabel;
class QMenu;
class QTabBar;
class QToolBar;
class QToolButton;

namespace vek {

// Markierter Zellbereich für Tabellenbefehle (wie TableObject::Range)
struct TableRange {
    int row0 = 0, col0 = 0, row1 = 0, col1 = 0;
};

class Canvas;
class Ruler;
class RulerCorner;
class StyleIndicator;
enum class ToolId;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    bool openFile(const QString& path);
    // Filter für Datei/Öffnen: eigene Dokumente, Fremdformate (CDR, PDF, AI, SVG …) und Bilder
    static QString openFilter();
    // Optionen > Allgemein > "Beim Starten von TANGENTA" (nur ohne Datei in der Befehlszeile)
    void applyStartup();
    // Neues Dokument mit Seitengrösse/Seitenzahl (Dialog "Neues Dokument erstellen" je nach Option)
    void createDocument(const QSizeF& pageSize, int pages);
    Canvas* canvas() const { return m_canvas; }

protected:
    void closeEvent(QCloseEvent* e) override;

private:
    QWidget* createCanvasArea();
    QWidget* createPageNavigator();
    void createMenusAndToolbars();
    void createToolbox();
    void createDockers();
    QDockWidget* addDocker(const QString& title, const QString& objectName, QWidget* content);
    void rebuildPageTabs();
    void updateActions();
    void updateTitle();
    void updateStatus();
    void updateZoomCombo();
    bool maybeSave();
    bool save();
    bool saveAs();
    void newDocument();
    void open();
    void importFiles();
    void searchImages();
    void exportFile();
    bool exportGlyph(const QString& path);
    void sharePdf();
    bool writePdf(const QString& path, const PdfSettings& settings);
    void showAlignDialog(int tab);
    class AlignDialog* m_alignDialog = nullptr;
    class TransformDocker* m_transformDocker = nullptr;
    QDockWidget* m_transformDock = nullptr;
    void showTransformDocker(int page);
    QDockWidget* m_propertiesDock = nullptr;
    QDockWidget* m_symbolDock = nullptr;
    void multiDuplicate();
    void copyPropertiesDialog();
    void findObjectsDialog();
    void replaceTextDialog();
    void lensDialog();
    void editOutlinePen();
    // Vektorformate (SVG, DXF, PDF, AI, EPS): importieren bzw. als neues Dokument öffnen
    static QStringList vectorImportSuffixes();
    bool importVectorFile(const QString& path, bool asNewDocument);
    void editPatternFill(PatternFill::Kind kind);
    void showFullPreview(bool selectedOnly);
    void createTableMenu();
    void addBitmapCommands(QMenu* bitmaps);
    void convertToBitmap();
    // Tabellenbefehl auf die bearbeitete bzw. ausgewählte Tabelle mit dem markierten Bereich
    void tableCommand(const QString& label, const std::function<void(class TableObject&, const struct TableRange&)>& fn);
    void bevelDialog();
    class ScriptDocker* m_scriptDocker = nullptr;
    QDockWidget* m_scriptDock = nullptr;
    QDockWidget* m_aiDock = nullptr;
    class AiPanel* m_aiPanel = nullptr;
    class LensDocker* m_lensDocker = nullptr;
    QDockWidget* m_lensDock = nullptr;
    class EnvelopeDocker* m_envelopeDocker = nullptr;
    QDockWidget* m_envelopeDock = nullptr;
public:
    LensDocker* lensDocker() const { return m_lensDocker; }
    EnvelopeDocker* envelopeDocker() const { return m_envelopeDocker; }
private:
    void rememberLastFile(const QString& path);
    // Optionen > Global > Filter > Verknüpfen (Windows, Benutzer-Registry)
    void registerFileTypes(const QStringList& suffixes);
    void publishHtml();
    void printDocument();
    void printPreview();
    QList<int> allPages() const;
    // Bitmaps: vektorisieren (quick = Blitzvektorisierung ohne Dialog)
    void traceImage(int preset, bool quick);
    void editBitmap();
    // Effekt auf das ausgewählte Bild: direkt (fn) oder über einen Dialog
    void applyImageEffect(const QString& label, const std::function<QImage(const QImage&)>& fn);
    void runImageDialog(const QString& label, class ImagePreviewDialog& dialog);
    QList<QAction*> m_bitmapActions; // nur mit einem ausgewählten Bild
    void deleteSelectionOrGuide();
    void showGuideContextMenu(int index, const QPoint& globalPos);
    // Kontextmenü auf Objekten
    void showObjectContextMenu(const QPoint& globalPos);
    // Oberflächengrösse geändert: Neustart anbieten
    // Fragt nach einem Neustart; leer = Text für die Oberflächengrösse
    void offerRestart(const QString& title = QString(), const QString& question = QString());
    void editUniformFill();
    void editFountainFill();
    void editOutlineColor();
    // Optionen (Extras > Optionen) auf Fenster, Zeichenfläche und Werkzeuge übertragen
    void applyOptions();
    void applyShortcuts();
    void autosave();
    void openOptions();
    QToolButton* flyoutButton(QToolBar* bar, const QString& iconName, const QString& tooltip,
                              const QList<QAction*>& actions);

    Editor m_editor;
    Canvas* m_canvas = nullptr;
    Ruler* m_hRuler = nullptr;
    Ruler* m_vRuler = nullptr;
    RulerCorner* m_rulerCorner = nullptr;
    QTabBar* m_pageTabs = nullptr;
    QLabel* m_pageLabel = nullptr;
    QLabel* m_positionLabel = nullptr;
    QLabel* m_infoLabel = nullptr;
    StyleIndicator* m_styleIndicator = nullptr;
    QComboBox* m_zoomCombo = nullptr;
    QMenu* m_dockerMenu = nullptr;
    QAction* m_rulersAction = nullptr;
    QAction* m_guidesAction = nullptr;
    QList<QAction*> m_snapActions; // Hilfslinien, Objekte, Seite
    QToolBar* m_standardBar = nullptr;
    QToolBar* m_propertyBar = nullptr;
    QToolBar* m_toolbox = nullptr;
    class QTimer* m_autosaveTimer = nullptr;
    class QObject* m_tooltipFilter = nullptr;
    QDockWidget* m_firstDocker = nullptr;

    QAction* m_undo = nullptr;
    QAction* m_redo = nullptr;
    QAction* m_paste = nullptr;
    QHash<int, QAction*> m_toolActions;
    QList<QAction*> m_selectionActions;
    // Objektbefehle, die je nach Auswahl ein- oder ausgeschaltet werden
    QAction* m_cut = nullptr;
    QAction* m_copy = nullptr;
    QAction* m_delete = nullptr;
    QAction* m_toCurves = nullptr;
    QAction* m_combine = nullptr;
    QAction* m_breakApart = nullptr;
    QAction* m_lock = nullptr;
    QAction* m_unlock = nullptr;
    QAction* m_wrapText = nullptr;
    QAction* m_breakConnector = nullptr;
    QAction* m_linkText = nullptr;
    QAction* m_unlinkText = nullptr;
    QMenu* m_arrangeMenu = nullptr;
    QHash<int, QAction*> m_arrangeActions; // Editor::Arrange
    QHash<int, QAction*> m_shapeActions;   // Objekt > Formen
    QDockWidget* m_shapingDock = nullptr;
    QList<QAction*> m_placeActions;        // Vor..., Hinter...
    QList<QAction*> m_fillActions;
    QList<QAction*> m_outlineActions;
};

} // namespace vek
