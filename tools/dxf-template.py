# Erzeugt resources/dxf/acad2004.dxf: eine leere AutoCAD-2004-Zeichnung (mm) als Vorlage für den
# DXF-Export. Kopf, Tabellen, Blöcke und Objekte stammen aus ezdxf (deren Ausgabe öffnet AutoCAD
# zuverlässig); TANGENTA setzt an den @MARKEN@ Ebenen, Objekte, Bilddefinitionen und Handles ein.
#
#   python -m pip install --user ezdxf
#   python tools/dxf-template.py

import os
import ezdxf

doc = ezdxf.new('R2004', setup=False, units=4)
msp = doc.modelspace()
# Ein Bild anlegen, damit ACAD_IMAGE_DICT und ACAD_IMAGE_VARS samt Klassen entstehen
image_def = doc.add_image_def(filename='x.png', size_in_pixel=(1, 1))
msp.add_image(image_def, insert=(0, 0), size_in_units=(1, 1))
tmp = os.path.join(os.path.dirname(__file__), '_tmp.dxf')
doc.saveas(tmp)
lines = open(tmp, encoding='utf-8').read().split('\n')
os.remove(tmp)

pairs = [(lines[i].strip(), lines[i + 1].rstrip()) for i in range(0, len(lines) - 1, 2)]

# In Objekte zerlegen (Gruppencode 0 beginnt ein Objekt)
records = []
for code, value in pairs:
    if code == '0':
        records.append([(code, value)])
    else:
        records[-1].append((code, value))


def handle(rec):
    for c, v in rec:
        if c == '5':
            return v
    return None


def owner(rec):
    for c, v in rec:
        if c == '330':
            return v
    return None


modelspace = next(handle(r) for r in records if r[0][1] == 'BLOCK_RECORD' and ('2', '*Model_Space') in r)
layer_table = next(handle(r) for r in records if r[0][1] == 'TABLE' and ('2', 'LAYER') in r)
image_dict = None
meta = None
for r in records:
    if r[0][1] == 'DICTIONARY':
        for i, (c, v) in enumerate(r):
            if c == '3' and v == 'ACAD_IMAGE_DICT':
                image_dict = r[i + 1][1]
            if c == '3' and v == 'EZDXF_META':
                meta = r[i + 1][1]

out = []
seed = None
section = None
for r in records:
    kind = r[0][1]
    if kind == 'SECTION':
        section = r[1][1]
    if section == 'ENTITIES' and kind not in ('SECTION', 'ENDSEC'):
        continue  # Beispielbild entfernen
    if kind in ('IMAGEDEF', 'IMAGEDEF_REACTOR'):
        continue
    if meta and (handle(r) == meta or owner(r) == meta):
        continue  # ezdxf-Metadaten
    if kind == 'ENDSEC' and section == 'ENTITIES':
        out.append([('@', 'ENTITIES')])
    if kind == 'ENDSEC' and section == 'OBJECTS':
        out.append([('@', 'OBJECTS')])
    if kind == 'ENDTAB' and out and out[-1][0][1] == 'LAYER':
        out.append([('@', 'LAYERS')])
    rec = []
    skip = 0
    for i, (c, v) in enumerate(r):
        if skip:
            skip -= 1
            continue
        if c == '3' and v in ('x.png', 'EZDXF_META'):
            skip = 1
            continue
        if c == '2' and v == 'EZDXF':
            v = 'TANGENTA'  # eigene Anwendungskennung statt der von ezdxf
        if c == '9' and v == '$HANDSEED':
            seed = r[i + 1][1]
            rec.append((c, v))
            rec.append((r[i + 1][0], '@HANDSEED@'))
            skip = 1
            continue
        if c == '9' and v in ('$EXTMIN', '$EXTMAX'):
            rec.append((c, v))
            tag = v[1:]
            rec += [('10', '@%s_X@' % tag), ('20', '@%s_Y@' % tag), ('30', '0.0')]
            skip = 3
            continue
        rec.append((c, v))
    if handle(r) == image_dict:
        rec.append(('@', 'IMAGEDICT'))
    out.append(rec)

text = ['999', 'TANGENTA DXF-Vorlage (aus ezdxf %s), Modellbereich=%s Ebenentabelle=%s Bildverzeichnis=%s Handles ab=%s'
        % (ezdxf.__version__, modelspace, layer_table, image_dict, seed)]
for rec in out:
    for c, v in rec:
        if c == '@':
            text.append('@%s@' % v)
        else:
            text += [c.rjust(3), v]
target = os.path.join(os.path.dirname(__file__), '..', 'resources', 'dxf', 'acad2004.dxf')
os.makedirs(os.path.dirname(target), exist_ok=True)
open(target, 'w', encoding='utf-8', newline='\n').write('\n'.join(text) + '\n')
print('Modellbereich', modelspace, 'Ebenentabelle', layer_table, 'Bildverzeichnis', image_dict, '->', os.path.abspath(target))
