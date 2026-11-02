"""Erzeugt das TANGENTA-Zeichen: SVG-Master, PNG je Grösse und das Windows-Symbol (.ico).

Das Zeichen ist eine Hüllkurve: Zwei Scharen gerader Linien sind die Tangenten zweier
quadratischer Bézierkurven, deren Kontrollpolygone gegenläufig in den Ecken liegen. Die Kurven
selbst werden nie gezeichnet; zwischen den Scharen bleibt ein Blatt als Negativform frei.

Je Grösse gibt es einen eigenen Schnitt (Linienzahl und -stärke), unter 20 px werden die
Scharen zu Flächen. Aufruf: python tools/make-logo.py  (braucht Pillow für PNG/ICO)
"""

import io
import struct
from pathlib import Path

from PIL import Image, ImageDraw

OUT = Path(__file__).resolve().parent.parent / "resources" / "brand"

VIEW = 120.0
PLATE_RADIUS = 27.0
LO, HI = 22.0, 98.0              # Eckpunkte der Kontrollpolygone
INK = (21, 25, 30)
PAPER = (246, 247, 249)
WHITE = (242, 244, 246)
RED_FROM = (214, 38, 36)         # Schar 1 läuft von Rot nach Orange
RED_TO = (255, 140, 66)
LIGHT_OPACITY = 0.88             # Schar 2 (weiss bzw. Tinte) tritt zurück

# Schnitte: (max. Pixelgrösse, Linien je Schar, Strichstärke in Einheiten von VIEW)
CUTS = [(26, 0, 0.0), (40, 6, 2.4), (72, 9, 1.6), (160, 14, 1.1), (10_000, 20, 0.75)]
PNG_SIZES = [16, 24, 32, 48, 64, 128, 256, 512]
ICO_SIZES = [16, 24, 32, 48, 64, 128, 256]

FAN_A = ((LO, LO), (LO, HI), (HI, HI))   # wölbt sich in die Ecke unten links
FAN_B = ((HI, HI), (HI, LO), (LO, LO))   # wölbt sich in die Ecke oben rechts


def cut_for(size):
    for limit, lines, width in CUTS:
        if size <= limit:
            return lines, width
    raise ValueError(size)


def lerp(a, b, t):
    return a + (b - a) * t


def mix(c1, c2, t):
    return tuple(round(lerp(a, b, t)) for a, b in zip(c1, c2))


def tangents(fan, lines):
    """Innere Tangenten t = 1/n … (n-1)/n; die Polygonseiten selbst (t=0, t=1) entfallen."""
    p0, c, p2 = fan
    for i in range(1, lines):
        t = i / lines
        a = (lerp(p0[0], c[0], t), lerp(p0[1], c[1], t))
        b = (lerp(c[0], p2[0], t), lerp(c[1], p2[1], t))
        yield t, a, b


def wedge(fan, steps=48):
    """Fläche zwischen Ecke und Kurve – der Ersatz für die Schar in kleinen Grössen."""
    p0, c, p2 = fan
    pts = []
    for i in range(steps + 1):
        t = i / steps
        u = 1 - t
        pts.append((u * u * p0[0] + 2 * u * t * c[0] + t * t * p2[0],
                    u * u * p0[1] + 2 * u * t * c[1] + t * t * p2[1]))
    return pts + [c]


# --- SVG ----------------------------------------------------------------------------------

def f(v):
    return f"{v:.2f}".rstrip("0").rstrip(".")


def hexc(c):
    return "#%02x%02x%02x" % c


def svg_mark(lines, width, dark=True):
    second = WHITE if dark else INK
    out = []
    if lines == 0:
        pts = " ".join(f"{f(x)},{f(y)}" for x, y in wedge(FAN_A))
        out.append(f'<polygon points="{pts}" fill="url(#tg-red)"/>')
        pts = " ".join(f"{f(x)},{f(y)}" for x, y in wedge(FAN_B))
        out.append(f'<polygon points="{pts}" fill="{hexc(second)}" opacity="{LIGHT_OPACITY}"/>')
        return "".join(out)
    out.append(f'<g stroke-width="{f(width)}" stroke-linecap="round">')
    for t, a, b in tangents(FAN_A, lines):
        out.append(f'<path d="M{f(a[0])} {f(a[1])}L{f(b[0])} {f(b[1])}" stroke="{hexc(mix(RED_FROM, RED_TO, t))}"/>')
    out.append(f'</g><g stroke="{hexc(second)}" stroke-width="{f(width)}" stroke-linecap="round" opacity="{LIGHT_OPACITY}">')
    for _, a, b in tangents(FAN_B, lines):
        out.append(f'<path d="M{f(a[0])} {f(a[1])}L{f(b[0])} {f(b[1])}"/>')
    out.append("</g>")
    return "".join(out)


def svg_doc(size_hint, plate=True, dark=True):
    lines, width = cut_for(size_hint)
    defs = (f'<defs><linearGradient id="tg-red" x1="{f(LO)}" y1="{f(LO)}" x2="{f(HI)}" y2="{f(HI)}" '
            f'gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="{hexc(RED_FROM)}"/>'
            f'<stop offset="1" stop-color="{hexc(RED_TO)}"/></linearGradient></defs>')
    bg = ""
    if plate:
        bg = (f'<rect width="{f(VIEW)}" height="{f(VIEW)}" rx="{f(PLATE_RADIUS)}" '
              f'fill="{hexc(INK if dark else PAPER)}"/>')
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {f(VIEW)} {f(VIEW)}">'
            f"{defs}{bg}{svg_mark(lines, width, dark)}</svg>\n")


# --- Raster -------------------------------------------------------------------------------

def render(size, plate=True, dark=True):
    ss = 8                                   # Überabtastung für saubere Kanten
    px = size * ss
    k = px / VIEW
    second = WHITE if dark else INK
    img = Image.new("RGBA", (px, px), (0, 0, 0, 0))
    if plate:
        ImageDraw.Draw(img).rounded_rectangle((0, 0, px - 1, px - 1), radius=PLATE_RADIUS * k,
                                              fill=(INK if dark else PAPER) + (255,))
    lines, width = cut_for(size)
    layer_a = Image.new("RGBA", img.size, (0, 0, 0, 0))
    layer_b = Image.new("RGBA", img.size, (0, 0, 0, 0))
    da, db = ImageDraw.Draw(layer_a), ImageDraw.Draw(layer_b)
    if lines == 0:
        # Fläche A mit Verlauf: Maske zeichnen, Verlauf diagonal darunterlegen
        mask = Image.new("L", img.size, 0)
        ImageDraw.Draw(mask).polygon([(x * k, y * k) for x, y in wedge(FAN_A)], fill=255)
        grad = Image.new("RGBA", img.size)
        gd = ImageDraw.Draw(grad)
        for i in range(2 * px):
            t = min(max((i / k - 2 * LO) / (2 * (HI - LO)), 0), 1)
            gd.line([(i, 0), (0, i)], fill=mix(RED_FROM, RED_TO, t) + (255,), width=2)
        layer_a.paste(grad, (0, 0), mask)
        db.polygon([(x * k, y * k) for x, y in wedge(FAN_B)], fill=second + (255,))
    else:
        w = width * k
        for draw, fan, colour in ((da, FAN_A, None), (db, FAN_B, second)):
            for t, a, b in tangents(fan, lines):
                c = (colour or mix(RED_FROM, RED_TO, t)) + (255,)
                a2, b2 = (a[0] * k, a[1] * k), (b[0] * k, b[1] * k)
                draw.line([a2, b2], fill=c, width=max(1, round(w)))
                for x, y in (a2, b2):        # runde Enden
                    draw.ellipse((x - w / 2, y - w / 2, x + w / 2, y + w / 2), fill=c)
    alpha = layer_b.getchannel("A").point(lambda v: round(v * LIGHT_OPACITY))
    layer_b.putalpha(alpha)
    img = Image.alpha_composite(img, layer_a)
    img = Image.alpha_composite(img, layer_b)
    return img.resize((size, size), Image.LANCZOS)


def write_ico(path, images):
    """ICO mit PNG-Einträgen; jede Grösse bringt ihren eigenen Schnitt mit."""
    blobs = []
    for img in images:
        buf = io.BytesIO()
        img.save(buf, "PNG")
        blobs.append((img.width, buf.getvalue()))
    header = struct.pack("<HHH", 0, 1, len(blobs))
    offset = 6 + 16 * len(blobs)
    entries, data = b"", b""
    for size, blob in blobs:
        dim = 0 if size >= 256 else size
        entries += struct.pack("<BBBBHHII", dim, dim, 0, 0, 1, 32, len(blob), offset + len(data))
        data += blob
    path.write_bytes(header + entries + data)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    svgs = {"tangenta.svg": svg_doc(10_000), "tangenta-small.svg": svg_doc(16),
            "tangenta-mark.svg": svg_doc(10_000, plate=False),
            "tangenta-mark-light.svg": svg_doc(10_000, plate=False, dark=False)}
    for name, text in svgs.items():
        (OUT / name).write_bytes(text.encode("utf-8"))   # LF auch unter Windows
    images = {}
    for size in PNG_SIZES:
        images[size] = render(size)
        images[size].save(OUT / f"tangenta-{size}.png")
    write_ico(OUT / "tangenta.ico", [images[s] for s in ICO_SIZES])
    print(f"{len(PNG_SIZES)} PNG, 4 SVG, 1 ICO nach {OUT}")


if __name__ == "__main__":
    main()
