"""Vorlage für den CDR-Export (src/core/CdrExport.cpp) erzeugen: resources/cdr/x3-template.riff

Aufruf (im Quellordner): python -I tools/cdr-template.py <zeichnung-x3.cdr>

Die Quelle ist eine beliebige Zeichnung im RIFF-Format der Version 13 ('CDRD'). Übernommen werden nur
die Voreinstellungen, die jede Datei dieser Version mitbringt: Dokumentkonfiguration (mcfg),
Schriftliste, Standardfüllungen und -konturen samt der Stilliste, die auf sie verweist, und die
Hauptseite mit den Ebenen Hilfslinien, Desktop, Ebene 1 und Raster. Zeichnungsinhalte, Vorschaubild
und Seiten der Quelle bleiben draussen. Markennamen in Bezeichnern werden durch neutrale gleicher
Länge ersetzt, damit die Satzlängen stimmen.

Ausgabe: RIFF 'TPL ' mit LIST 'doc ', LIST 'page' (Hauptseite) und 'sumi', ohne Kompression.

Eine Zeichnung der Version 16 (ZIP mit content/root.dat) ergibt resources/cdr/x6-template.riff:
RIFF 'TPL6' mit LIST 'doc ' (Blöcke aufgelöst) und 'cdss' (styles/document.cdss, Stilvorlagen).
"""
import struct, sys, zipfile, zlib


def parse(d, off, end, table=None):
    out = []
    while off + 8 <= end:
        if table is not None:
            while off < end and d[off] == 0:
                off += 1
            if off + 8 > end:
                break
        cc = d[off:off + 4]
        ln = struct.unpack_from('<I', d, off + 4)[0]
        if table is not None and ln < len(table):
            ln = table[ln]
        body = off + 8
        if cc in (b'RIFF', b'LIST'):
            kind = d[body:body + 4]
            if kind == b'cmpr':
                csize, usize, bcsize, busize = struct.unpack_from('<IIII', d, body + 4)
                start = body + 20
                data = zlib.decompress(d[start + 8:start + csize]) if usize else b''
                blocks = zlib.decompress(d[start + csize + 8:start + csize + bcsize]) if busize else b''
                tbl = [struct.unpack_from('<I', blocks, i)[0] for i in range(0, len(blocks), 4)]
                out.append((cc, kind, parse(data, 0, len(data), tbl)))
            elif kind == b'stlt':
                out.append((cc, kind, d[body + 4:body + ln]))
            else:
                out.append((cc, kind, parse(d, body + 4, body + ln, table)))
        else:
            out.append((cc, None, d[body:body + ln]))
        off = body + ln + ((ln & 1) if table is None else 0)
    return out


def serialize(c):
    cc, kind, payload = c
    if kind:
        body = kind + (payload if isinstance(payload, bytes) else b''.join(serialize(x) for x in payload))
    else:
        body = payload
    return cc + struct.pack('<I', len(body)) + body + (b'\0' if len(body) & 1 else b'')


def u16le(s):
    return s.encode('utf-16le')


def neutral(c):
    cc, kind, payload = c
    if isinstance(payload, list):
        return (cc, kind, [neutral(x) for x in payload])
    # Bezeichner der Standardstile und -einstellungen; Länge bleibt gleich
    brand = bytes([0x43, 0, 0x6f, 0, 0x72, 0, 0x65, 0, 0x6c, 0])
    payload = payload.replace(brand + u16le('DefaultID'), u16le('DefaultStyleID'))
    payload = payload.replace(brand + u16le('\\WebSite\\'), u16le('Tangenta\\Site\\'))
    return (cc, kind, payload)


def find(items, cc, kind=None):
    return [x for x in items if x[0] == cc and (kind is None or x[1] == kind)]


def resolve(items, files):
    """Version 16: Blöcke sind Verweise (Datei, Länge, Versatz) in die Datendateien; ffffffff = direkt"""
    out = []
    for cc, kind, payload in items:
        if isinstance(payload, list):
            out.append((cc, kind, resolve(payload, files)))
            continue
        fi, length, offset = struct.unpack_from('<III', payload)
        out.append((cc, None, payload[8:8 + length] if fi == 0xffffffff else files[fi][offset:offset + length]))
    return out


if zipfile.is_zipfile(sys.argv[1]):
    # Version 16 (ZIP): Dokumentteil und Stilvorlagen; Hauptseite und Ebenen erzeugt der Export selbst
    z = zipfile.ZipFile(sys.argv[1])
    names = [n for n in z.read('content/dataFileList.dat').decode('latin1').split('\n') if n]
    files = [z.read('content/data/' + n) for n in names]
    src = z.read('content/root.dat')
    root = parse(src, 0, len(src))[0]
    assert root[1] == b'CDRG', 'Quelle muss eine Zeichnung der Version 16 sein'
    vrsn = resolve(find(root[2], b'vrsn'), files)[0][2]
    assert struct.unpack_from('<H', vrsn)[0] == 1600, 'Quelle muss eine Zeichnung der Version 16 sein'
    doc = neutral(resolve(find(root[2], b'LIST', b'doc '), files)[0])
    styles = (b'cdss', None, z.read('styles/document.cdss'))
    out, target = serialize((b'RIFF', b'TPL6', [doc, styles])), 'resources/cdr/x6-template.riff'
else:
    src = open(sys.argv[1], 'rb').read()
    root = parse(src, 0, len(src))[0]
    assert root[1] == b'CDRD', 'Quelle muss eine Zeichnung der Version 13 sein'
    top = root[2]
    body = [x for x in find(top, b'LIST', b'cmpr') if find(x[2], b'LIST', b'doc ')][0][2]
    doc = neutral(find(body, b'LIST', b'doc ')[0])
    master = neutral(find(body, b'LIST', b'page')[0])
    sumi = find(top, b'sumi')[0]
    out, target = serialize((b'RIFF', b'TPL ', [doc, master, sumi])), 'resources/cdr/x3-template.riff'
# Markenname klein, als Bytes; darf weder als ASCII noch als UTF-16 übrig bleiben
word = bytes([0x63, 0x6f, 0x72, 0x65, 0x6c])
for w in (word, word.decode('ascii').encode('utf-16le')):
    assert w not in out.lower(), 'Markenname in der Vorlage'
open(target, 'wb').write(out)
print(target, len(out), 'Bytes')
