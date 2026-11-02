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
    endif()
else()
    message(FATAL_ERROR "patch.cmake: KIND=cdr oder KIND=revenge angeben")
endif()
