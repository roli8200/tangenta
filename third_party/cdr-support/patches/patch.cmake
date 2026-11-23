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
        # Ab Version 2022 steht hinter der Füllungsnummer ein Block "Wert, 16, Kennung (16 Bytes)";
        # ohne ihn zu überspringen ist jede Füllung leer
        replace_line(${SOURCE}/src/lib/CDRParser.cpp "  unsigned fillId = readU32(input);
  if (m_version >= 1300)
    input->seek(8, librevenge::RVNG_SEEK_CUR);"
            "  unsigned fillId = readU32(input);
  if (m_version >= 1300)
  {
    readU32(input);
    if (readU32(input) == 16) // TANGENTA: Kennungsblock neuerer Fassungen
      input->seek(24, librevenge::RVNG_SEEK_CUR);
  }")
        # CDR5 speichert ein Byte je Zeichen; Bit 0 der Zeichenflags bedeutet dort nicht "zwei Bytes"
        replace_line(${SOURCE}/src/lib/CDRParser.cpp "charDescriptions.push_back((unsigned char)((readU16(input) >> 3) & 0xff));"
            "charDescriptions.push_back((unsigned char)((readU16(input) >> 3) & 0xfe)); // TANGENTA")
        # Bitmap-Masken (durchsichtige Bereiche, z. B. ausgestellte Fotos)
        file(READ ${PATCHES}/bitmap-mask.inc maskdecl)
        file(READ ${PATCHES}/bitmap-mask-read.inc maskread)
        file(READ ${PATCHES}/bitmap-mask-apply.inc maskapply)
        replace_line(${SOURCE}/src/lib/CDRParser.cpp "#include \"CDRParser.h\"\n" "#include \"CDRParser.h\"\n${maskdecl}")
        replace_line(${SOURCE}/src/lib/CDRParser.cpp "  readRImage(colorModel, width, height, bpp, palette, bitmap, input);\n"
            "  readRImage(colorModel, width, height, bpp, palette, bitmap, input);\n${maskread}")
        replace_line(${SOURCE}/src/lib/CDRStylesCollector.cpp "#include \"libcdr_utils.h\"\n" "#include \"libcdr_utils.h\"\n${maskdecl}")
        replace_line(${SOURCE}/src/lib/CDRStylesCollector.cpp "  if (storeBMP)\n" "${maskapply}  if (storeBMP)\n")
        file(READ ${PATCHES}/text-transform.inc trafo)
        replace_line(${SOURCE}/src/lib/CDRContentCollector.cpp "    librevenge::RVNGPropertyList textFrameProps;\n"
            "    librevenge::RVNGPropertyList textFrameProps;\n${trafo}")
        replace_line(${SOURCE}/src/lib/CDRContentCollector.cpp "            spanProps.insert(\"style:font-name\", j.m_style.m_fontName);"
            "            spanProps.insert(\"style:font-name\", j.m_style.m_fontName);\n          if (j.m_style.m_fontWeight >= 0x1000) // TANGENTA\n            spanProps.insert(\"fo:font-weight\", \"bold\");\n          if (j.m_style.m_fontItalic > 0)\n            spanProps.insert(\"fo:font-style\", \"italic\");")
        # Text: Kontur und fehlende Füllung (weisser Text mit farbigem Umriss)
        file(READ ${PATCHES}/text-outline.inc outline)
        replace_line(${SOURCE}/src/lib/CDRContentCollector.cpp "            spanProps.insert(\"fo:color\", m_ps.getRGBColorString(j.m_style.m_fillStyle.color1));
"
            "            spanProps.insert(\"fo:color\", m_ps.getRGBColorString(j.m_style.m_fillStyle.color1));
${outline}")
    endif()

    # Effekte, eigene Stufe mit eigener Marke: ältere Build-Ordner haben die Grundanpassungen schon
    # (PATCH_LEVEL in cdr.cmake erzwingt den erneuten Aufruf, dann kommen nur die neuen Stufen dazu)
    file(READ ${f} text)
    string(FIND "${text}" "TANGENTA-FX" done)
    if(done EQUAL -1)
        # Effekte. Transparenz (0x1f40): libcdr las nur eine Zahl, ab X6 meist 0 = Objekt unsichtbar.
        # Der ganze Datensatz mit Verlauf, Anfassern und Mischmodus geht als tangenta:transparency-*
        # an den Import.
        set(p ${SOURCE}/src/lib/CDRParser.cpp)
        set(c ${SOURCE}/src/lib/CDRContentCollector.cpp)
        set(ch ${SOURCE}/src/lib/CDRContentCollector.h)
        replace_region(${p} "void libcdr::CDRParser::readOpacity(" "void libcdr::CDRParser::readBmpf(" transparency-read.inc)
        file(READ ${PATCHES}/transparency-types.inc trtypes)
        file(READ ${PATCHES}/mesh-types.inc meshtypes)
        replace_line(${SOURCE}/src/lib/CDRCollector.h "class CDRCollector\n" "${trtypes}${meshtypes}class CDRCollector\n")
        replace_line(${SOURCE}/src/lib/CDRCollector.h "  virtual void collectFillOpacity(double opacity) = 0;\n"
            "  virtual void collectFillOpacity(double opacity) = 0;\n  virtual void collectTangentaTransparency(const TangentaTransparency &) {}\n  virtual void collectTangentaMesh(const TangentaMesh &) {}\n  virtual void collectTangentaClipList(unsigned level) { collectVect(level); }\n  virtual void collectTangentaClip(unsigned) {}\n")
        replace_line(${ch} "  void collectFillOpacity(double opacity) override;\n"
            "  void collectFillOpacity(double opacity) override;\n  void collectTangentaTransparency(const TangentaTransparency &t) override { m_tangentaTransparency = t; }\n  void collectTangentaMesh(const TangentaMesh &m) override { m_tangentaMesh = m; }\n  void collectTangentaClipList(unsigned level) override;\n  void collectTangentaClip(unsigned id) override;\n  void _tangentaStoreClip();\n  void _tangentaPushObject(const CDROutputElementList &outputElement);\n")
        replace_line(${ch} "  double m_fillOpacity;\n"
            "  double m_fillOpacity;\n  TangentaTransparency m_tangentaTransparency;\n  TangentaMesh m_tangentaMesh;\n  unsigned m_tangentaClipLevel = 0;\n  unsigned m_tangentaClipId = 0;\n  std::stack<CDROutputElementList> m_tangentaClipStack;\n  std::queue<CDROutputElementList> m_tangentaClipQueue;\n  std::map<unsigned, std::vector<CDROutputElementList>> m_tangentaClips;\n")
        replace_line(${c} "#include \"CDRContentCollector.h\"\n" "#include \"CDRContentCollector.h\"\n\n#include <cstdlib>\n")
        replace_line(${c} "  m_fillOpacity = 1.0;\n" "  m_fillOpacity = 1.0;\n  m_tangentaTransparency = TangentaTransparency();\n  m_tangentaMesh = TangentaMesh();\n")
        file(READ ${PATCHES}/mesh-output.inc meshout)
        file(READ ${PATCHES}/transparency-output.inc trout)
        replace_line(${c} "  if (m_fillOpacity < 1.0)\n    propList.insert(\"draw:opacity\", m_fillOpacity, librevenge::RVNG_PERCENT);\n"
            "  if (m_fillOpacity < 1.0)\n    propList.insert(\"draw:opacity\", m_fillOpacity, librevenge::RVNG_PERCENT);\n${meshout}${trout}")
        # Bilder bekamen keinen eigenen Stil; ohne ihn fehlen Transparenz und Mischmodus
        replace_line(${c} "    outputElement.addGraphicObject(propList);\n"
            "    // TANGENTA: eigener Stil für das Bild (Transparenz, Mischmodus); sonst gälte der Stil des\n    // vorher gezeichneten Objekts\n    librevenge::RVNGPropertyList imageStyle;\n    _fillProperties(imageStyle);\n    outputElement.addStyle(imageStyle);\n    outputElement.addGraphicObject(propList);\n")
        # Farbverläufe: konisch kam linear heraus, radial mit cy = cx, mehrfarbig radial als linear.
        # Die Geometrie geht als tangenta:gradient-* an den Import.
        file(READ ${PATCHES}/gradient-output.inc gradout)
        replace_line(${c} "      case 2: // Gradient\n" "${gradout}")
        # PowerClip: Die Inhalte stehen in einer clpt-Gruppe, die libcdr als Vektormuster verwarf;
        # der Rahmen verweist mit 0x1f45 auf sie
        replace_line(${p} "      else if (listType == CDR_FOURCC_vect || listType == CDR_FOURCC_clpt)\n        m_collector->collectVect(level);"
            "      else if (listType == CDR_FOURCC_vect)\n        m_collector->collectVect(level);\n      else if (listType == CDR_FOURCC_clpt) // TANGENTA: Inhalte von PowerClips\n        m_collector->collectTangentaClipList(level);")
        file(READ ${PATCHES}/mesh-read.inc meshread)
        replace_line(${p} "    else if (argTypes[i] == 0x1f40)\n      readOpacity(input, length);"
            "${meshread}    else if (argTypes[i] == 0x1f40)\n      readOpacity(input, length);\n    else if (argTypes[i] == 0x1f45) // TANGENTA: PowerClip-Rahmen, Nummer der Inhalte\n      m_collector->collectTangentaClip(readU32(input));")
        replace_line(${p} "      else if (chunkType == 0x14) // Polygon\n        readPolygonCoords(input);"
            "      else if (chunkType == 0x14) // Polygon\n        readPolygonCoords(input);\n      else if (chunkType == 0x20) // TANGENTA-FX: Umriss eines Objekts mit Maschenfüllung\n        readLineAndCurve(input);")
        file(READ ${PATCHES}/powerclip-collect.inc clip)
        replace_line(${c} "void libcdr::CDRContentCollector::collectVect(unsigned level)\n" "${clip}void libcdr::CDRContentCollector::collectVect(unsigned level)\n")
        replace_line(${c} "  if (!outputElement.empty())\n  {\n    if (m_reverseOrder)\n      m_outputElementsStack->push(outputElement);\n    else\n      m_outputElementsQueue->push(outputElement);\n  }\n  m_currentTransforms.clear();"
            "  if (!outputElement.empty())\n    _tangentaPushObject(outputElement); // TANGENTA: PowerClip\n  m_tangentaClipId = 0;\n  m_currentTransforms.clear();")
        replace_line(${c} "  if (level <= m_currentVectLevel)\n  {\n    m_currentVectLevel = 0;"
            "  if (m_tangentaClipLevel && m_spnd && m_groupLevels.empty()) // TANGENTA: PowerClip-Inhalte ablegen\n    _tangentaStoreClip();\n  if (level <= m_tangentaClipLevel)\n    m_tangentaClipLevel = 0;\n  if (level <= m_currentVectLevel)\n  {\n    m_currentVectLevel = 0;")
    endif()

    # Zeichen horizontal verschieben (JSON character.h_shift, Hundertstel der Schriftgrösse); damit
    # stehen z. B. die Tageszahlen eines Kalenders als ein Text in ihren Spalten
    file(READ ${f} text)
    string(FIND "${text}" "TANGENTA-SHIFT" done)
    if(done EQUAL -1)
        replace_line(${SOURCE}/src/lib/CDRTypes.h "  int m_fontItalic = -1;\n  CDRStyle()"
            "  int m_fontItalic = -1;\n  double m_hShift = 0.0;\n  bool m_hasHShift = false;\n  CDRStyle()")
        replace_line(${SOURCE}/src/lib/CDRTypes.h "      m_fontItalic = override.m_fontItalic;"
            "      m_fontItalic = override.m_fontItalic;\n    if (override.m_hasHShift)\n    {\n      m_hShift = override.m_hShift;\n      m_hasHShift = true;\n    }")
        replace_line(${f} "      style.m_fontItalic = italic ? 1 : 0;\n"
            "      style.m_fontItalic = italic ? 1 : 0;\n    const QJsonValue shift = value(\"character.h_shift\"); // TANGENTA-SHIFT\n    if (shift.isString() || shift.isDouble())\n    {\n      style.m_hShift = shift.isString() ? shift.toString().toDouble() : shift.toDouble();\n      style.m_hasHShift = true;\n    }\n")
        replace_line(${SOURCE}/src/lib/CDRContentCollector.cpp "            spanProps.insert(\"fo:font-style\", \"italic\");\n"
            "            spanProps.insert(\"fo:font-style\", \"italic\");\n          if (j.m_style.m_hShift != 0.0)\n            spanProps.insert(\"tangenta:h-shift\", j.m_style.m_hShift / 100.0);\n")
    endif()

    # Dasselbe in den Versionen 7 bis X5: Zeichenstil-Feld "Offset X", gleiche Einheit wie h_shift
    file(READ ${f} text)
    string(FIND "${text}" "TANGENTA-SHIFT2" done)
    if(done EQUAL -1)
        replace_line(${f} "        if (fl2&0x10) // Offset X\n          input->seek(4, librevenge::RVNG_SEEK_CUR);"
            "        if (fl2&0x10) // Offset X (TANGENTA-SHIFT2: Hundertstel der Schriftgrösse)\n        {\n          style.m_hShift = (double)(int)readU32(input);\n          style.m_hasHShift = true;\n        }")
    endif()

    # Der letzte Lauf eines Textes bekam den Zeichenstil des vorletzten (Schrift, Farbe, Verschiebung)
    set(s ${SOURCE}/src/lib/CDRStylesCollector.cpp)
    file(READ ${s} text)
    string(FIND "${text}" "TANGENTA-LASTRUN" done)
    if(done EQUAL -1)
        replace_line(${s} "  }\n  librevenge::RVNGString text;\n  if (!tmpTextData.empty())\n"
            "  }\n  // TANGENTA-LASTRUN: Stil des letzten Laufs, nicht den des vorletzten\n  tmpCharStyle = defaultCharStyle;\n  {\n    auto last = styleOverrides.find(tmpCharDescription & 0xfe);\n    if (last != styleOverrides.end())\n      tmpCharStyle.overrideStyle(last->second);\n  }\n  librevenge::RVNGString text;\n  if (!tmpTextData.empty())\n")
    endif()
elseif(KIND STREQUAL "revenge")
    set(f ${SOURCE}/src/lib/RVNGBinaryData.cpp)
    file(READ ${f} text)
    string(FIND "${text}" "TANGENTA:" done)
    if(done EQUAL -1)
        replace_region(${f} "void convertFromBase64(" "} // anonymous namespace" rvng-base64.inc)
        set(f ${SOURCE}/src/lib/RVNGPropertyList.cpp)
        replace_line(${f} "#include <boost/spirit/include/qi.hpp>"
            "#include <cctype>\n#include <charconv>\n#include <cstring>\n#include <locale>\n#include <sstream>\n#include <system_error>")
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
