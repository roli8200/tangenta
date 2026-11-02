# Erzeugt src/core/FontTables.inc aus fontTools: Standard-, WinAnsi- und MacRoman-Kodierung
# (Glyphennamen je Code), Adobe Glyph List (Name -> Unicode) und die 391 CFF-Standardzeichenketten.
#
#   python -m pip install --user fonttools
#   python tools/font-tables.py

import os
from fontTools import agl
from fontTools.cffLib import cffStandardStrings
from fontTools.encodings.MacRoman import MacRoman
from fontTools.encodings.StandardEncoding import StandardEncoding
from fontTools.ttLib.standardGlyphOrder import standardGlyphOrder


def win_ansi():
    names = []
    for code in range(256):
        try:
            ch = bytes([code]).decode('cp1252')
        except UnicodeDecodeError:
            names.append('.notdef')
            continue
        uv = ord(ch)
        if code < 32 or code == 127:
            names.append('.notdef')
        elif uv == 0xA0:
            names.append('space')
        elif uv == 0xAD:
            names.append('hyphen')
        else:
            names.append(agl.UV2AGL.get(uv, 'uni%04X' % uv))
    return names


def array(name, items):
    body = ',\n'.join('    "%s"' % s for s in items)
    return 'static const char* const %s[%d] = {\n%s};\n' % (name, len(items), body)


out = ['// Erzeugt von tools/font-tables.py aus fontTools – nicht von Hand ändern.\n']
out.append(array('kStandardEncoding', StandardEncoding))
out.append(array('kWinAnsiEncoding', win_ansi()))
out.append(array('kMacRomanEncoding', MacRoman))
out.append(array('kCffStandardStrings', cffStandardStrings))
out.append(array('kMacGlyphOrder', standardGlyphOrder))
# Vollständige (Legacy-)Liste, je Name nur einzelne Zeichen aus der BMP
pairs = sorted((n, uvs[0]) for n, uvs in agl.LEGACY_AGL2UV.items() if len(uvs) == 1 and uvs[0] <= 0xFFFF)
out.append('struct AglEntry {\n    const char* name;\n    char16_t unicode;\n};\n')
out.append('static const AglEntry kGlyphList[%d] = {\n%s};\n' % (
    len(pairs), ',\n'.join('    {"%s", 0x%04X}' % (n, uv) for n, uv in pairs)))
target = os.path.join(os.path.dirname(__file__), '..', 'src', 'core', 'FontTables.inc')
open(target, 'w', encoding='utf-8', newline='\n').write('\n'.join(out))
print(len(pairs), 'Glyphennamen ->', os.path.abspath(target))
