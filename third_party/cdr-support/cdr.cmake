# CDR-Import: libcdr und librevenge (MPL-2.0) sowie Little CMS (MIT) werden beim Konfigurieren
# heruntergeladen, über die Prüfsumme geprüft, mit patches/patch.cmake angepasst und statisch gebaut.
# Statt ICU und Boost nutzen sie die kleinen Ersatzteile in include/ und src/.
include(FetchContent)

set(CDR_SUPPORT ${CMAKE_CURRENT_LIST_DIR})

FetchContent_Declare(vek_lcms2
    URL https://github.com/mm2/Little-CMS/releases/download/lcms2.16/lcms2-2.16.tar.gz
    URL_HASH SHA256=d873d34ad8b9b4cea010631f1a6228d2087475e4dc5e763eb81acc23d9d45a51
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR nicht-mit-cmake-bauen)
FetchContent_Declare(vek_librevenge
    URL https://dev-www.libreoffice.org/src/librevenge-0.0.5.tar.bz2
    URL_HASH SHA256=5892ca6796f7a2a93d580832e907e849b19d980b40d326a283b18877ab6de0c5
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    PATCH_COMMAND ${CMAKE_COMMAND} -DSOURCE=<SOURCE_DIR> -DKIND=revenge -P ${CDR_SUPPORT}/patches/patch.cmake
    SOURCE_SUBDIR nicht-mit-cmake-bauen)
FetchContent_Declare(vek_libcdr
    URL https://dev-www.libreoffice.org/src/libcdr/libcdr-0.1.8.tar.xz
    URL_HASH SHA256=ced677c8300b29c91d3004bb1dddf0b99761bf5544991c26c2ee8f427e87193c
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    PATCH_COMMAND ${CMAKE_COMMAND} -DSOURCE=<SOURCE_DIR> -DKIND=cdr -P ${CDR_SUPPORT}/patches/patch.cmake
    SOURCE_SUBDIR nicht-mit-cmake-bauen)
FetchContent_MakeAvailable(vek_lcms2 vek_librevenge vek_libcdr)

# Little CMS: Farbumrechnung CMYK/Lab -> RGB
file(GLOB VEK_LCMS2_SOURCES ${vek_lcms2_SOURCE_DIR}/src/*.c)
add_library(vek_lcms2 STATIC ${VEK_LCMS2_SOURCES})
target_include_directories(vek_lcms2 PUBLIC ${vek_lcms2_SOURCE_DIR}/include)
set_target_properties(vek_lcms2 PROPERTIES POSITION_INDEPENDENT_CODE ON)

# librevenge: nur Eigenschaften, Zeichenketten, Binärdaten und Datenströme (keine Generatoren)
set(VEK_REVENGE ${vek_librevenge_SOURCE_DIR}/src/lib)
add_library(vek_revenge STATIC
    ${VEK_REVENGE}/RVNGBinaryData.cpp ${VEK_REVENGE}/RVNGMemoryStream.cpp ${VEK_REVENGE}/RVNGOLEStream.cpp
    ${VEK_REVENGE}/RVNGProperty.cpp ${VEK_REVENGE}/RVNGPropertyList.cpp ${VEK_REVENGE}/RVNGPropertyListVector.cpp
    ${VEK_REVENGE}/RVNGStreamImplementation.cpp ${VEK_REVENGE}/RVNGString.cpp ${VEK_REVENGE}/RVNGStringVector.cpp
    ${VEK_REVENGE}/RVNGSVGDrawingGenerator.cpp
    ${VEK_REVENGE}/RVNGZipStream.cpp)
target_include_directories(vek_revenge
    PUBLIC ${vek_librevenge_SOURCE_DIR}/inc
    PRIVATE ${VEK_REVENGE} ${CDR_SUPPORT}/include)
target_compile_definitions(vek_revenge PRIVATE LIBREVENGE_BUILD BOOST_ALL_NO_LIB)
target_link_libraries(vek_revenge PRIVATE miniz)

# libcdr mit Ersatz für ICU (Zeichensätze über Windows bzw. iconv)
file(GLOB VEK_CDR_SOURCES ${vek_libcdr_SOURCE_DIR}/src/lib/*.cpp)
add_library(vek_cdr STATIC ${VEK_CDR_SOURCES} ${CDR_SUPPORT}/src/icu_shim.cpp)
target_include_directories(vek_cdr
    PUBLIC ${vek_libcdr_SOURCE_DIR}/inc
    PRIVATE ${vek_libcdr_SOURCE_DIR}/src/lib ${CDR_SUPPORT}/include)
target_compile_definitions(vek_cdr PRIVATE LIBCDR_BUILD BOOST_ALL_NO_LIB)
target_link_libraries(vek_cdr PUBLIC vek_revenge PRIVATE vek_lcms2 miniz Qt6::Core)
foreach(t vek_lcms2 vek_revenge vek_cdr)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${t} PRIVATE -w)
    endif()
endforeach()
