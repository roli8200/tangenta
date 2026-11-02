# Passt die heruntergeladenen Quellen von libcdr und librevenge an TANGENTA an: Die wenigen Stellen,
# die Boost.Spirit, Boost.PropertyTree oder Boost.Serialization brauchen, werden durch die Blöcke in
# diesem Ordner ersetzt. Aufruf (aus FetchContent): cmake -DSOURCE=<Quellordner> -DKIND=cdr|revenge -P patch.cmake
set(PATCHES ${CMAKE_CURRENT_LIST_DIR})

function(replace_region file begin end incfile)
    file(READ ${file} text)
    string(FIND "${text}" "${begin}" a)
    string(FIND "${text}" "${end}" b)
    if(a EQUAL -1 OR b EQUAL -1 OR b LESS a)
        message(FATAL_ERROR "patch.cmake: Stelle in ${file} nicht gefunden (${begin})")
    endif()
    file(READ ${PATCHES}/${incfile} replacement)
    string(SUBSTRING "${text}" 0 ${a} head)
    string(SUBSTRING "${text}" ${b} -1 tail)
    file(WRITE ${file} "${head}${replacement}${tail}")
endfunction()

function(replace_line file old new)
    file(READ ${file} text)
    string(FIND "${text}" "${old}" a)
    if(a EQUAL -1)
        message(FATAL_ERROR "patch.cmake: Zeile in ${file} nicht gefunden (${old})")
    endif()
    string(REPLACE "${old}" "${new}" text "${text}")
    file(WRITE ${file} "${text}")
endfunction()

if(KIND STREQUAL "cdr")
    set(f ${SOURCE}/src/lib/CDRParser.cpp)
    file(READ ${f} text)
    string(FIND "${text}" "TANGENTA:" done)
    if(done EQUAL -1)
        replace_line(${f} "#include <boost/spirit/include/qi.hpp>"
            "#include <QByteArray>\n#include <QJsonDocument>\n#include <QJsonObject>\n#include <QJsonValue>\n#include <QString>\n#include <QStringList>\n#include <cctype>\n#include <cstdlib>\n#include <cstring>\n#include <utility>")
        replace_region(${f} "static void processNameForEncoding(" "  if (!bRes)" cdrparser-colour.inc)
        replace_region(${f} "  boost::property_tree::ptree pt;" "void libcdr::CDRParser::_skipX3Optional(" cdrparser-json.inc)
        # unsigned long ist unter Windows 32 Bit: "Position + Länge" läuft bei beschädigten Dateien
        # über und gibt vor, 4 GB lägen bereit. "Länge < Rest" kann nicht überlaufen.
        replace_line(${SOURCE}/src/lib/CDRInternalStream.cpp "if ((m_offset+numBytes) < m_buffer.size())"
            "if (numBytes < m_buffer.size() - m_offset) // TANGENTA: ohne Überlauf")
        replace_line(${SOURCE}/src/lib/CDRInternalStream.cpp "      for (unsigned long i=0; i<have; i++)\n        m_buffer.push_back(out[i]);"
            "      m_buffer.insert(m_buffer.end(), out, out + have); // TANGENTA: blockweise statt Byte für Byte")
        # CMYK-Bilder: Little CMS rechnet mit Gleitkomma-Eingabe jedes Pixel einzeln ohne Tabelle
        # (35 s für ein grosses Bild). 16-Bit-Eingabe und gleiche Farben nur einmal umrechnen.
        file(READ ${PATCHES}/cmyk-cache.inc cache)
        replace_line(${SOURCE}/src/lib/CDRCollector.cpp "#include \"libcdr_utils.h\"" "#include \"libcdr_utils.h\"\n${cache}")
        replace_line(${SOURCE}/src/lib/CDRCollector.cpp "cmsDoTransform(m_colorTransformCMYK2RGB, cmyk, rgb, 1);"
            "cachedCmykToRgb(m_colorTransformCMYK2RGB, cmyk, rgb); // TANGENTA")
        replace_line(${SOURCE}/src/lib/CDRCollector.cpp "TYPE_CMYK_DBL, tmpRGBProfile" "TYPE_CMYK_16, tmpRGBProfile")
        # Schriftschnitt: neuere Dateien führen ihn im JSON-Stil (weight 0x40 normal, 0x1000 fett; italic)
        replace_line(${SOURCE}/src/lib/CDRTypes.h "  unsigned m_parentId;\n  CDRStyle()"
            "  unsigned m_parentId;\n  unsigned m_fontWeight = 0; // TANGENTA\n  int m_fontItalic = -1;\n  CDRStyle()")
        replace_line(${SOURCE}/src/lib/CDRTypes.h "      m_fontName = override.m_fontName;\n    }"
            "      m_fontName = override.m_fontName;\n    }\n    if (override.m_fontWeight)\n      m_fontWeight = override.m_fontWeight;\n    if (override.m_fontItalic >= 0)\n      m_fontItalic = override.m_fontItalic;")
        # CDR5 speichert ein Byte je Zeichen; Bit 0 der Zeichenflags bedeutet dort nicht "zwei Bytes"
        replace_line(${SOURCE}/src/lib/CDRParser.cpp "charDescriptions.push_back((unsigned char)((readU16(input) >> 3) & 0xff));"
            "charDescriptions.push_back((unsigned char)((readU16(input) >> 3) & 0xfe)); // TANGENTA")
        file(READ ${PATCHES}/text-transform.inc trafo)
        replace_line(${SOURCE}/src/lib/CDRContentCollector.cpp "    librevenge::RVNGPropertyList textFrameProps;\n"
            "    librevenge::RVNGPropertyList textFrameProps;\n${trafo}")
        replace_line(${SOURCE}/src/lib/CDRContentCollector.cpp "            spanProps.insert(\"style:font-name\", j.m_style.m_fontName);"
            "            spanProps.insert(\"style:font-name\", j.m_style.m_fontName);\n          if (j.m_style.m_fontWeight >= 0x1000) // TANGENTA\n            spanProps.insert(\"fo:font-weight\", \"bold\");\n          if (j.m_style.m_fontItalic > 0)\n            spanProps.insert(\"fo:font-style\", \"italic\");")
    endif()
elseif(KIND STREQUAL "revenge")
    set(f ${SOURCE}/src/lib/RVNGBinaryData.cpp)
    file(READ ${f} text)
    string(FIND "${text}" "TANGENTA:" done)
    if(done EQUAL -1)
        replace_region(${f} "void convertFromBase64(" "} // anonymous namespace" rvng-base64.inc)
        set(f ${SOURCE}/src/lib/RVNGPropertyList.cpp)
        replace_line(${f} "#include <boost/spirit/include/qi.hpp>"
            "#include <cctype>\n#include <charconv>\n#include <cstring>\n#include <system_error>")
        replace_region(${f} "bool findDouble(" "} // anonymous namespace" rvng-numbers.inc)
        replace_line(${SOURCE}/src/lib/RVNGStreamImplementation.cpp "if ((unsigned long)d->offset+numBytes < d->buffer.size())"
            "if (numBytes < d->buffer.size() - (unsigned long)d->offset) // TANGENTA: ohne Überlauf")
        replace_line(${SOURCE}/src/lib/RVNGMemoryStream.cpp "if (m_offset+long(numBytes) < long(m_size))"
            "if ((unsigned long long)numBytes < (unsigned long long)(long(m_size) - m_offset)) // TANGENTA: ohne Überlauf")
        # Bilder liegen als RVNGBinaryData vor; getStr() kodiert sie nach Base64 (bei 36 MB mehrere
        # Sekunden). TANGENTA liest sie über diese Funktion direkt.
        replace_line(${SOURCE}/src/lib/RVNGProperty.cpp "\tRVNGBinaryData m_data;\n};"
            "\tRVNGBinaryData m_data;\n\tfriend const RVNGBinaryData *tangentaBinaryData(const RVNGProperty *);\n};\n\n// TANGENTA: Binärdaten ohne Umweg über Base64\nconst RVNGBinaryData *tangentaBinaryData(const RVNGProperty *p)\n{\n\tconst auto *b = dynamic_cast<const RVNGBinaryDataProperty *>(p);\n\treturn b ? &b->m_data : nullptr;\n}")
    endif()
else()
    message(FATAL_ERROR "patch.cmake: KIND=cdr oder KIND=revenge angeben")
endif()
