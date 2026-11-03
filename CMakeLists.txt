cmake_minimum_required(VERSION 3.21)
project(Tangenta VERSION 0.86 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.7 REQUIRED COMPONENTS Core Gui Widgets Svg PrintSupport Qml Network Test)
# Übersetzungen der Oberfläche; ohne Linguist-Werkzeuge (Debian: qt6-tools-dev) nur deutsch
find_package(Qt6 QUIET COMPONENTS LinguistTools)
if(NOT Qt6LinguistTools_FOUND)
    message(WARNING "Qt6 LinguistTools fehlen (Debian/Ubuntu: qt6-tools-dev): Oberfläche nur deutsch")
endif()

option(VEK_BUILD_TESTS "Build unit tests" ON)
option(VEK_BUILD_APP "Build the application" ON)

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# --- miniz (ZIP-Container des Dateiformats) ---------------------------------
add_library(miniz STATIC third_party/miniz/miniz.c)
target_include_directories(miniz PUBLIC third_party/miniz)
if(NOT MSVC)
    target_compile_options(miniz PRIVATE -w)
endif()

# --- Potrace 1.16 (Kurvenanpassung der Vektorisierung; GPL 2 oder neuer) -----
# Nur die Kurvenstufe (trace.c, curve.c, Brücke vek_fit.c); die Umrisse liefert der Vektorisierer selbst
add_library(potrace STATIC third_party/potrace/trace.c third_party/potrace/curve.c third_party/potrace/vek_fit.c)
target_include_directories(potrace PUBLIC third_party/potrace)
set_target_properties(potrace PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(NOT MSVC)
    target_compile_options(potrace PRIVATE -w)
endif()

# --- LibRaw (Kamera-Rohdaten wie NEF; LGPL 2.1 oder CDDL 1.0) ------------------
# Alle Quellen ausser den Sammeldateien *_ph.cpp, wie in LibRaws Makefile.mingw
file(GLOB_RECURSE LIBRAW_SOURCES CONFIGURE_DEPENDS third_party/libraw/src/*.cpp)
add_library(libraw STATIC ${LIBRAW_SOURCES})
target_include_directories(libraw PUBLIC third_party/libraw)
target_compile_definitions(libraw PUBLIC LIBRAW_NODLL LIBRAW_NOTHREADS)
# gnu++14 wie LibRaws Makefile (M_PI u. a.)
set_target_properties(libraw PROPERTIES CXX_STANDARD 14 CXX_EXTENSIONS ON POSITION_INDEPENDENT_CODE ON)
if(NOT MSVC)
    target_compile_options(libraw PRIVATE -w)
endif()
if(WIN32)
    target_link_libraries(libraw PUBLIC ws2_32)
endif()
# Entrastern auf allen Kernen (LibRaw nutzt OpenMP in AHD und anderen Verfahren)
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_link_libraries(libraw PUBLIC OpenMP::OpenMP_CXX)
    # LibRaw schaltet OpenMP unter Windows mit GCC sonst ab
    target_compile_definitions(libraw PUBLIC LIBRAW_FORCE_OPENMP)
endif()

# --- Hunspell (Rechtschreibprüfung; MPL 1.1 / GPL 2 / LGPL 2.1) ----------------
file(GLOB HUNSPELL_SOURCES CONFIGURE_DEPENDS third_party/hunspell/src/*.cxx)
add_library(hunspell STATIC ${HUNSPELL_SOURCES})
target_include_directories(hunspell PUBLIC third_party/hunspell/src)
target_compile_definitions(hunspell PUBLIC HUNSPELL_STATIC)
set_target_properties(hunspell PROPERTIES CXX_STANDARD 11 POSITION_INDEPENDENT_CODE ON)
if(NOT MSVC)
    target_compile_options(hunspell PRIVATE -w)
endif()
# --- CDR-Import: libcdr, librevenge, Little CMS (beim Konfigurieren geladen) -----------
include(third_party/cdr-support/cdr.cmake)

# Wörterbücher: neben das Programm (Windows), nach share/tangenta (Linux); Entwicklungs-
# fassungen finden sie zusätzlich im Quellbaum
set(VEK_DICTIONARY_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/resources/dictionaries)

# --- Dokumentkern: kennt keine Widgets ----------------------------------------
add_library(vekcore STATIC
    src/core/Color.cpp
    src/core/Style.cpp
    src/core/Node.cpp
    src/core/Page.cpp
    src/core/DrawObject.cpp
    src/core/RectangleObject.cpp
    src/core/DimensionObject.cpp
    src/core/TableObject.cpp
    src/core/AutoCorrect.cpp
    src/core/Align.cpp
    src/core/CloneObject.cpp
    src/core/Effects.cpp
    src/core/BitmapFilters.cpp
    src/core/BitmapArt.cpp
    src/core/SuperClipObject.cpp
    src/core/BlendObject.cpp
    src/core/SpellChecker.cpp
    src/core/ConnectorObject.cpp
    src/core/CurveObject.cpp
    src/core/PathOps.cpp
    src/core/Outline.cpp
    src/core/EllipseObject.cpp
    src/core/PolygonObject.cpp
    src/core/BlurBake.cpp
    src/core/Ramp.cpp
    src/core/TextFlow.cpp
    src/core/TextObject.cpp
    src/core/Hyphenator.cpp
    src/core/ImageObject.cpp
    src/core/ImageAdjust.cpp
    src/core/ImagePool.cpp
    src/core/RawImage.cpp
    src/core/Vectorizer.cpp
    src/core/GroupObject.cpp
    src/core/Serialization.cpp
    src/core/History.cpp
    src/core/Transaction.cpp
    src/core/Document.cpp
    src/core/PerfectShape.cpp
    src/core/Patterns.cpp
    src/core/ImageCodecs.cpp
    src/core/SvgImport.cpp
    src/core/PdfImport.cpp
    src/core/DxfImport.cpp
    src/core/FontFile.cpp
    src/core/FontPrograms.cpp
    src/core/FontMatch.cpp
    src/core/Imposition.cpp
    src/core/DocumentIO.cpp
    src/core/CdrImport.cpp
)
target_include_directories(vekcore PUBLIC src/core)
target_link_libraries(vekcore PUBLIC Qt6::Core Qt6::Gui PRIVATE miniz libraw hunspell potrace vek_cdr)
target_compile_definitions(vekcore PUBLIC VEK_DICTIONARY_SOURCE="${VEK_DICTIONARY_SOURCE}")
# Deutsche Trennmuster (hyph-utf8, MIT) für die Silbentrennung im Mengentext
qt_add_resources(vekcore "hyphenation" PREFIX "/hyphenation" BASE resources/hyphenation
    FILES resources/hyphenation/hyph-de-1996.pat)
# Google-Fonts-Familien (tools/google-fonts-list.py) für die Schriftersetzung beim PDF-Import
qt_add_resources(vekcore "fonts" PREFIX "/fonts" BASE resources/fonts FILES resources/fonts/googlefonts.tsv)

# --- Anwendung -----------------------------------------------------------------
if(VEK_BUILD_APP)
    # Alles ausser main() als Bibliothek, damit die UI-Tests dieselben Klassen benutzen
    add_library(vekapp STATIC
        src/app/Editor.cpp
        src/app/EditorShaping.cpp
        src/app/EditorEffects.cpp
        src/app/EffectDockers.cpp
        src/app/SuperClipBar.cpp
        src/app/DrawingTools.cpp
        src/app/OutlineDialog.cpp
        src/app/ShapingTools.cpp
        src/app/PerfectShapeTool.cpp
        src/app/PatternDialog.cpp
        src/app/ExportDialog.cpp
        src/app/VectorRecorder.cpp
        src/app/FontMatchDialog.cpp
        src/app/ImpositionDialog.cpp
        src/app/PdfSettings.cpp
        src/app/PdfExport.cpp
        src/app/PdfSettingsDialog.cpp
        src/app/Options.cpp
        src/app/OptionsDialog.cpp
        src/app/Labels.cpp
        src/app/DimensionTool.cpp
        src/app/ConnectorTool.cpp
        src/app/SmartDrawingTool.cpp
        src/app/TableTool.cpp
        src/app/MeshTool.cpp
        src/app/Output.cpp
        src/app/SpellDialog.cpp
        src/app/FontBox.cpp
        src/app/ScriptHost.cpp
        src/app/ScriptDocker.cpp
        src/app/AlignDialog.cpp
        src/app/TransformDocker.cpp
        src/app/EditDockers.cpp
        src/app/PipetteTools.cpp
        src/app/PerspectiveTool.cpp
        src/app/ShadowTool.cpp
        src/app/InteractiveTools.cpp
        src/app/ShapingDocker.cpp
        src/app/MainWindow.cpp
        src/app/Canvas.cpp
        src/app/Ruler.cpp
        src/app/SnapEngine.cpp
        src/app/PickTool.cpp
        src/app/ZoomTool.cpp
        src/app/RectangleTool.cpp
        src/app/PropertyBar.cpp
        src/app/ObjectManager.cpp
        src/app/ColorPalette.cpp
        src/app/ExpressionSpinBox.cpp
        src/app/Blur.cpp
        src/app/ArtisticMediaTool.cpp
        src/app/EffectTool.cpp
        src/app/DisplayDepth.cpp
        src/app/TextTool.cpp
        src/app/TraceDialog.cpp
        src/app/Ai.cpp
        src/app/AiTools.cpp
        src/app/AiChat.cpp
        src/app/AiPanel.cpp
        src/app/McpServer.cpp
        src/app/ClaudeCode.cpp
        src/app/WebSearch.cpp
        src/app/ImageSearch.cpp
        src/app/ImageSearchDialog.cpp
        src/app/ImageDialogs.cpp
        src/app/BezierTool.cpp
        src/app/CutTools.cpp
        src/app/FlyoutButton.cpp
        src/app/UiScale.cpp
        src/app/Language.cpp
        src/app/FreehandTool.cpp
        src/app/ShapeTool.cpp
        src/app/ColorDialog.cpp
        src/app/GradientDialog.cpp
        src/app/InteractiveFillTool.cpp
        src/app/Palette.cpp
        src/app/Icons.cpp
    )
    target_include_directories(vekapp PUBLIC src/app)
    target_link_libraries(vekapp PUBLIC vekcore Qt6::Widgets Qt6::Svg Qt6::PrintSupport Qt6::Qml Qt6::Network)
    if(WIN32)
        # Farbtiefe des Bildschirms (DisplayDepth)
        target_link_libraries(vekapp PRIVATE dxgi dxguid)
    endif()

    # Symbole (eigene Zeichnungen aus tools/make-icons.py) fest eingebettet
    file(GLOB VEK_ICONS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} resources/icons/*.svg)
    qt_add_resources(vekapp "icons" PREFIX "/icons" BASE resources/icons FILES ${VEK_ICONS})

    # Programmzeichen (tools/make-logo.py): SVG plus eigene Schnitte für kleine Grössen
    # Übersetzungen der Oberfläche (Quelltexte deutsch); .ts nachführen mit
    # cmake --build <build> --target update_translations, die .qm landen als :/i18n/tangenta_<sprache>.qm
    if(Qt6LinguistTools_FOUND)
    qt_add_translations(TARGETS vekapp
        SOURCE_TARGETS vekcore vekapp
        TS_FILES translations/tangenta_en.ts translations/tangenta_fr.ts translations/tangenta_it.ts
        RESOURCE_PREFIX /i18n
        LUPDATE_OPTIONS -source-language de -locations none -no-obsolete)
    endif()
    qt_add_resources(vekapp "dxf" PREFIX "/dxf" BASE resources/dxf FILES resources/dxf/acad2004.dxf)
    qt_add_resources(vekapp "brand" PREFIX "/brand" BASE resources/brand FILES
        resources/brand/tangenta.svg
        resources/brand/tangenta-16.png resources/brand/tangenta-24.png
        resources/brand/tangenta-32.png resources/brand/tangenta-48.png
        resources/brand/tangenta-64.png resources/brand/tangenta-128.png
        resources/brand/tangenta-256.png)

    if(WIN32)
        set(TANGENTA_RC ${CMAKE_CURRENT_SOURCE_DIR}/resources/brand/tangenta.rc)
    endif()
    qt_add_executable(tangenta src/app/main.cpp ${TANGENTA_RC})
    target_link_libraries(tangenta PRIVATE vekapp)
    target_compile_definitions(tangenta PRIVATE VEK_VERSION="${PROJECT_VERSION}")
    set_target_properties(tangenta PROPERTIES WIN32_EXECUTABLE ON)

    if(UNIX AND NOT APPLE)
        include(GNUInstallDirs)
        install(TARGETS tangenta RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
        install(FILES resources/brand/tangenta.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
        install(DIRECTORY resources/dictionaries/ DESTINATION ${CMAKE_INSTALL_DATADIR}/tangenta/dictionaries)
        install(FILES resources/brand/tangenta.svg
                DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
        foreach(size 16 24 32 48 64 128 256 512)
            install(FILES resources/brand/tangenta-${size}.png
                    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps
                    RENAME tangenta.png)
        endforeach()
    endif()
endif()

if(VEK_BUILD_TESTS)
    enable_testing()
    qt_add_executable(tst_core tests/tst_core.cpp)
    target_link_libraries(tst_core PRIVATE vekcore Qt6::Test)
    add_test(NAME tst_core COMMAND tst_core)
    # offscreen findet unter Windows ohne Schriftverzeichnis keine Systemschriften
    set(VEK_TEST_ENV "QT_QPA_PLATFORM=offscreen")
    if(WIN32)
        list(APPEND VEK_TEST_ENV "QT_QPA_FONTDIR=$ENV{SystemRoot}/Fonts")
    endif()
    set_tests_properties(tst_core PROPERTIES ENVIRONMENT "${VEK_TEST_ENV}")

    if(VEK_BUILD_APP)
        qt_add_executable(tst_tools tests/tst_tools.cpp)
        target_link_libraries(tst_tools PRIVATE vekapp Qt6::Test)
        target_compile_definitions(tst_tools PRIVATE VEK_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
        add_test(NAME tst_tools COMMAND tst_tools)
        set_tests_properties(tst_tools PROPERTIES ENVIRONMENT "${VEK_TEST_ENV}")
    endif()
endif()
