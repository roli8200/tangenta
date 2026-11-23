"""Symbole für die Menüeinträge (eigene Zeichnungen, GPL wie das Projekt).

Gleicher Stil wie make-icons.py: 24er-Raster, dunkler Umriss, helle Flächen mit leichtem Verlauf;
beim Schreiben wird alles in Graustufen umgerechnet. Die Symbole sind aus wenigen Bausteinen
zusammengesetzt (Blatt, Foto, Tabelle, Fenster, Abzeichen), damit verwandte Befehle verwandt aussehen.
Bitmap-Effekte heissen fx-<id mit - statt .> und werden über ihre Effekt-ID zugeordnet.
"""

INK = '#3a3a3a'
S = 'stroke="%s" stroke-width="1" stroke-linejoin="round" stroke-linecap="round"' % INK
BLUE = '#1f5fae'
ORANGE = '#e2740c'
GREEN = '#3aa04a'
RED = '#d32f2f'


# --- Bausteine ---------------------------------------------------------------------------------

def page(x=4.5, y=2.5, w=13, h=17, fold=4):
    """Blatt mit Eselsohr oben rechts"""
    r, b = x + w, y + h
    return (f'<path d="M{x} {y}h{w - fold}l{fold} {fold}v{h - fold}H{x}z" fill="url(#paper)" {S}/>'
            f'<path d="M{r - fold} {y}v{fold}h{fold}" fill="#ffffff" {S}/>')


def lines(x, y, w, n=3, gap=2.5, color=INK, width=1.0):
    return ''.join(f'<path d="M{x} {y + i * gap}h{w if i < n - 1 else w * 0.6:.1f}" stroke="{color}" stroke-width="{width}" stroke-linecap="round"/>'
                   for i in range(n))


def photo(x=3, y=5, w=16, h=12, sun=True):
    """Foto: Rahmen, Berge, Sonne"""
    sx, sy = x + w * 0.72, y + h * 0.3
    return (f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="#ffffff" {S}/>'
            f'<path d="M{x + 1} {y + h - 1}l{w * 0.3:.1f} {-h * 0.45:.1f} {w * 0.22:.1f} {h * 0.27:.1f} {w * 0.13:.1f} {-h * 0.14:.1f} {w * 0.29:.1f} {h * 0.32:.1f}z" fill="#1f5fae"/>'
            + (f'<circle cx="{sx:.1f}" cy="{sy:.1f}" r="{min(w, h) * 0.11:.1f}" fill="{ORANGE}"/>' if sun else ''))


def window(x=2.5, y=3.5, w=19, h=15):
    return (f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="1" fill="#ffffff" {S}/>'
            f'<path d="M{x} {y + 3}h{w}" {S}/><rect x="{x + 0.5}" y="{y + 0.5}" width="{w - 1}" height="2.5" fill="url(#blue)"/>')


def table(x=3, y=4, w=18, h=15, cols=3, rows=3, fill='#ffffff'):
    out = f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="{fill}" {S}/>'
    for c in range(1, cols):
        out += f'<path d="M{x + w * c / cols:.2f} {y}v{h}" {S}/>'
    for r in range(1, rows):
        out += f'<path d="M{x} {y + h * r / rows:.2f}h{w}" {S}/>'
    return out


def cell(x, y, w, h, color=BLUE, opacity=0.55):
    return f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="{color}" fill-opacity="{opacity}"/>'


def plus(cx=18, cy=18, r=4.2, color=GREEN):
    return (f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="{color}" stroke="#ffffff" stroke-width="0.8"/>'
            f'<path d="M{cx - r * 0.55:.2f} {cy}h{r * 1.1:.2f}M{cx} {cy - r * 0.55:.2f}v{r * 1.1:.2f}" stroke="#ffffff" stroke-width="1.5" stroke-linecap="round"/>')


def minus(cx=18, cy=18, r=4.2, color=RED):
    return (f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="{color}" stroke="#ffffff" stroke-width="0.8"/>'
            f'<path d="M{cx - r * 0.55:.2f} {cy}h{r * 1.1:.2f}" stroke="#ffffff" stroke-width="1.5" stroke-linecap="round"/>')


def cross(cx=18, cy=18, r=4.2, color=RED):
    d = r * 0.45
    return (f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="{color}" stroke="#ffffff" stroke-width="0.8"/>'
            f'<path d="M{cx - d:.2f} {cy - d:.2f}l{2 * d:.2f} {2 * d:.2f}M{cx + d:.2f} {cy - d:.2f}l{-2 * d:.2f} {2 * d:.2f}" stroke="#ffffff" stroke-width="1.4" stroke-linecap="round"/>')


def check(cx=18, cy=18, r=4.2, color=GREEN):
    return (f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="{color}" stroke="#ffffff" stroke-width="0.8"/>'
            f'<path d="M{cx - 2} {cy}l1.4 1.5 2.8-3" fill="none" stroke="#ffffff" stroke-width="1.4" stroke-linecap="round" stroke-linejoin="round"/>')


def arrow(x1, y1, x2, y2, color=BLUE, width=1.6, head=2.6):
    """Gerader Pfeil mit Spitze"""
    import math
    a = math.atan2(y2 - y1, x2 - x1)
    l1 = (x2 - head * math.cos(a - 0.5), y2 - head * math.sin(a - 0.5))
    l2 = (x2 - head * math.cos(a + 0.5), y2 - head * math.sin(a + 0.5))
    return (f'<path d="M{x1} {y1}L{x2} {y2}" stroke="{color}" stroke-width="{width}" stroke-linecap="round"/>'
            f'<path d="M{l1[0]:.2f} {l1[1]:.2f}L{x2} {y2}L{l2[0]:.2f} {l2[1]:.2f}" fill="none" stroke="{color}" stroke-width="{width}" stroke-linecap="round" stroke-linejoin="round"/>')


def gear(cx, cy, r=4.5):
    import math
    pts = []
    for i in range(16):
        a = math.pi * 2 * i / 16
        rr = r if i % 2 == 0 else r * 0.72
        pts.append(f'{cx + rr * math.cos(a):.2f} {cy + rr * math.sin(a):.2f}')
    return (f'<path d="M{"L".join(pts)}z" fill="url(#steel)" {S}/>'
            f'<circle cx="{cx}" cy="{cy}" r="{r * 0.32:.2f}" fill="#ffffff" {S}/>')


def sparkle(cx, cy, r=3.0, color=ORANGE):
    return (f'<path d="M{cx} {cy - r}l{r * 0.3:.2f} {r * 0.7:.2f} {r * 0.7:.2f} {r * 0.3:.2f}-{r * 0.7:.2f} {r * 0.3:.2f}'
            f'-{r * 0.3:.2f} {r * 0.7:.2f}-{r * 0.3:.2f}-{r * 0.7:.2f}-{r * 0.7:.2f}-{r * 0.3:.2f} {r * 0.7:.2f}-{r * 0.3:.2f}z" fill="{color}" stroke="#ffffff" stroke-width="0.5"/>')


def magnifier(cx=15, cy=15, r=4):
    return (f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="url(#glass)" stroke="{INK}" stroke-width="1.4"/>'
            f'<path d="M{cx + r * 0.72:.2f} {cy + r * 0.72:.2f}l3.2 3.2" stroke="#5b4636" stroke-width="2.4" stroke-linecap="round"/>')


def folder(x=2.5, y=6, w=17, h=12):
    return (f'<path d="M{x} {y}h5l1.5 1.5h{w - 6.5}v{h - 1.5}H{x}z" fill="url(#folder)" {S}/>'
            f'<path d="M{x} {y + 3}h{w}" stroke="#ffffff" stroke-width="0.8" stroke-opacity="0.6"/>')


def node_square(x, y, s=2.4, fill='#ffffff'):
    return f'<rect x="{x - s / 2}" y="{y - s / 2}" width="{s}" height="{s}" fill="{fill}" stroke="{INK}" stroke-width="0.8"/>'


def obj_shapes(dx=0, dy=0):
    """Zwei Objekte (Kreis und Quadrat)"""
    return (f'<rect x="{3 + dx}" y="{9 + dy}" width="9" height="9" fill="url(#steel)" {S}/>'
            f'<circle cx="{15 + dx}" cy="{9 + dy}" r="5" fill="url(#blue)" {S}/>')


def curl(color=BLUE):
    return f'<path d="M5 17c0-6 3-11 7-11s5 4 2 6-6 0-4-3" fill="none" stroke="{color}" stroke-width="1.6" stroke-linecap="round"/>'


# Mini-Symbol einer Unterkategorie, darunter ein Foto (für Bitmap-Untermenüs)
def photo_with(overlay, sun=True):
    return photo(2.5, 4, 15, 11.5, sun) + overlay


ICONS = {
    # --- Datei ---------------------------------------------------------------------------------
    'doc-close': page() + cross(17, 17),
    'doc-close-all': page(7, 1.5, 12, 15) + page(3.5, 4.5, 12, 15) + cross(17, 18),
    'doc-save-as': ('<rect x="2.5" y="2.5" width="15" height="15" rx="1" fill="url(#steel)" ' + S + '/>'
                    '<rect x="5" y="2.5" width="9" height="5" fill="#ffffff" ' + S + '/><rect x="5" y="11" width="10" height="6.5" fill="#ffffff" ' + S + '/>'
                    '<path d="M13 21.5l1-3.4 6.3-6.3 2.4 2.4-6.3 6.3z" fill="url(#orange)" ' + S + '/>'),
    'doc-html': page() + f'<path d="M7 10.5l-2 2.5 2 2.5M12 10.5l2 2.5-2 2.5M10.6 9.8l-1.4 6.4" fill="none" stroke="{BLUE}" stroke-width="1.3" stroke-linecap="round" stroke-linejoin="round"/>'
                + f'<circle cx="18" cy="18" r="4" fill="url(#glass)" {S}/><path d="M14 18h8M18 14c-2 2.5-2 5.5 0 8M18 14c2 2.5 2 5.5 0 8" fill="none" stroke="{INK}" stroke-width="0.7"/>',
    'doc-pdf': page() + f'<rect x="2.5" y="11" width="12" height="6" rx="1" fill="{RED}" stroke="#ffffff" stroke-width="0.6"/>'
               '<path d="M4.3 15.6v-3.2h1.2a.9.9 0 0 1 0 1.8H4.3M7.6 15.6v-3.2h.9c1 0 1.5.7 1.5 1.6s-.5 1.6-1.5 1.6zM11.4 15.6v-3.2h1.9M11.4 14h1.5" fill="none" stroke="#ffffff" stroke-width="0.8"/>'
               + arrow(16, 18, 21.5, 12.5, GREEN, 1.6, 2.4),
    'doc-print-preview': ('<rect x="5.5" y="2.5" width="11" height="5.5" fill="#ffffff" ' + S + '/><rect x="2.5" y="7.5" width="17" height="7" rx="1" fill="#d7dde6" ' + S + '/>'
                          '<rect x="5.5" y="12.5" width="11" height="6" fill="#ffffff" ' + S + '/>' + magnifier(16, 16, 3.6)),
    'doc-imposition': ('<rect x="2.5" y="3.5" width="19" height="17" fill="url(#paper)" ' + S + '/>'
                       + ''.join(f'<rect x="{4 + c * 8.5}" y="{5 + r * 7.5}" width="7" height="6" fill="#ffffff" {S}/>' for c in range(2) for r in range(2))
                       + '<path d="M12 2v2M12 20v2.2M1.5 12h2M20.5 12h2.2" stroke="' + INK + '" stroke-width="0.8"/>'),
    'app-exit': ('<path d="M3.5 3.5h10v17h-10z" fill="url(#steel)" ' + S + '/><path d="M13.5 3.5l-5 2v17l5-2" fill="#c9d1da" ' + S + '/>'
                 + arrow(12.5, 12, 21.5, 12, RED, 1.8, 3)),

    # --- Bearbeiten ----------------------------------------------------------------------------
    'edit-paste-special': ('<rect x="3.5" y="4.5" width="12" height="16" rx="1" fill="#c9a56a" ' + S + '/><rect x="6.5" y="2.5" width="6" height="4" rx="1" fill="url(#steel)" ' + S + '/>'
                           + page(10, 9, 10, 12, 3) + sparkle(19, 6, 3.2)),
    'symbol-new': ('<circle cx="10" cy="11" r="7" fill="url(#steel)" ' + S + '/><path d="M6.8 11.2l2.2 2.3 4.4-4.6" fill="none" stroke="' + INK + '" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"/>' + plus(18, 18)),
    'symbol-revert': ('<circle cx="9" cy="9" r="6" fill="url(#steel)" ' + S + '/><path d="M6.3 9.2l1.9 2 3.8-4" fill="none" stroke="' + INK + '" stroke-width="1.4" stroke-linecap="round" stroke-linejoin="round"/>'
                      + arrow(13, 14, 16.5, 17.5, BLUE, 1.4, 2.2) + '<rect x="14.5" y="17" width="4.5" height="4.5" fill="url(#steel)" ' + S + '/><circle cx="20" cy="16" r="2.4" fill="url(#blue)" ' + S + '/>'),
    'symbol-manager': (window(2.5, 3, 19, 17) + '<circle cx="8" cy="11.5" r="3" fill="url(#steel)" ' + S + '/><path d="M6.7 11.6l.9 1 1.8-1.9" fill="none" stroke="' + INK + '" stroke-width="0.9"/>'
                       + lines(12.5, 10, 7, 3, 3, INK, 1.0)),
    'edit-duplicate': ('<rect x="3" y="3" width="11" height="11" fill="url(#steel)" ' + S + '/><rect x="9.5" y="9.5" width="11" height="11" fill="url(#blue)" ' + S + '/>'),
    'edit-duplicate-inplace': ('<rect x="4.5" y="4.5" width="13" height="13" fill="url(#steel)" ' + S + '/><rect x="6.5" y="6.5" width="13" height="13" fill="url(#blue)" fill-opacity="0.85" ' + S + '/>'
                               + '<path d="M2 2l3 3M22 22l-3-3" stroke="' + ORANGE + '" stroke-width="1.2" stroke-linecap="round"/>'),
    'edit-step-repeat': ''.join(f'<rect x="{2.5 + i * 4}" y="{2.5 + i * 4}" width="8" height="8" fill="{"url(#blue)" if i == 3 else "url(#steel)"}" {S}/>' for i in range(4)),
    'select-all': ('<rect x="2.5" y="2.5" width="19" height="19" fill="none" stroke="' + BLUE + '" stroke-width="1" stroke-dasharray="2 1.5"/>' + obj_shapes(0.5, 1)),
    'select-text': ('<rect x="2.5" y="4.5" width="19" height="15" fill="none" stroke="' + BLUE + '" stroke-width="1" stroke-dasharray="2 1.5"/>'
                    '<path d="M6 17l4-11h1.5l4 11M7.4 13.2h6.6" fill="none" stroke="' + INK + '" stroke-width="1.6" stroke-linejoin="round"/><path d="M17 8v10" stroke="' + INK + '" stroke-width="1"/>'),
    'select-nodes': ('<path d="M4 18C6 8 12 4 20 6" fill="none" stroke="' + INK + '" stroke-width="1.3"/>' + node_square(4, 18, 3, BLUE) + node_square(10.5, 8, 3, BLUE) + node_square(20, 6, 3, BLUE)
                     + '<rect x="1.5" y="3" width="21" height="17.5" fill="none" stroke="' + BLUE + '" stroke-width="0.9" stroke-dasharray="2 1.5"/>'),
    'edit-find': page(3, 2.5, 13, 17) + lines(5.5, 8, 7, 3, 2.6) + magnifier(15.5, 15.5, 4),
    'edit-replace': (page(3, 2.5, 12, 16) + '<path d="M5.5 8h6M5.5 11h4" stroke="' + INK + '" stroke-width="1" stroke-linecap="round"/>'
                     + '<path d="M13 15a5 5 0 0 1 8.5-1.5" fill="none" stroke="' + BLUE + '" stroke-width="1.5" stroke-linecap="round"/><path d="M21.8 10.8v3.2h-3.2" fill="none" stroke="' + BLUE + '" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>'
                     + '<path d="M21.5 18.5a5 5 0 0 1-8.5 1.5" fill="none" stroke="' + ORANGE + '" stroke-width="1.5" stroke-linecap="round"/><path d="M12.7 23v-3.2h3.2" fill="none" stroke="' + ORANGE + '" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>'),

    # --- Ansicht -------------------------------------------------------------------------------
    'view-wireframe-simple': '<rect x="3" y="9" width="10" height="10" fill="none" stroke="' + INK + '" stroke-width="1"/><circle cx="15" cy="9" r="6" fill="none" stroke="' + INK + '" stroke-width="1"/>',
    'view-wireframe': ('<rect x="3" y="9" width="10" height="10" fill="none" stroke="' + INK + '" stroke-width="1"/><circle cx="15" cy="9" r="6" fill="none" stroke="' + INK + '" stroke-width="1"/>'
                       '<rect x="4" y="2" width="5" height="5" fill="none" stroke="' + INK + '" stroke-width="0.8"/><path d="M5 5.5l1-2 1.2 1.3 1-.8" fill="none" stroke="' + INK + '" stroke-width="0.6"/>'),
    'view-draft': ('<rect x="3" y="9" width="10" height="10" fill="#9fb0c2" ' + S + '/><circle cx="15" cy="9" r="6" fill="#6f8fb8" ' + S + '/>'
                   '<path d="M3 13l4-4M3 17l8-8M5 19l8-8M9 19l4-4" stroke="#ffffff" stroke-width="0.7" stroke-opacity="0.7"/>'),
    'view-normal': '<rect x="3" y="9" width="10" height="10" fill="url(#steel)" ' + S + '/><circle cx="15" cy="9" r="6" fill="url(#blue)" ' + S + '/>',
    'view-enhanced': ('<rect x="3" y="9" width="10" height="10" fill="url(#steel)" ' + S + '/><circle cx="15" cy="9" r="6" fill="url(#radial)" ' + S + '/>' + sparkle(19.5, 18.5, 3.2)),
    'view-pixel': ''.join(f'<rect x="{3 + c * 3}" y="{3 + r * 3}" width="3" height="3" fill="{"#1f5fae" if (c - 3) ** 2 + (r - 3) ** 2 < 8 else ("#b9c3cd" if (c + r) % 2 else "#ffffff")}" stroke="#ffffff" stroke-width="0.3"/>' for c in range(6) for r in range(6))
                  + '<rect x="3" y="3" width="18" height="18" fill="none" ' + S + '/>',
    'view-preview-selected': ('<rect x="3" y="9" width="10" height="10" fill="#e3e8ee" stroke="#9aa5b1" stroke-width="1" stroke-dasharray="1.5 1"/><circle cx="15" cy="9" r="6" fill="url(#blue)" ' + S + '/>'
                              + node_square(9, 3, 2) + node_square(21, 3, 2) + node_square(9, 15, 2) + node_square(21, 15, 2)),
    'zoom-selection': ('<rect x="2.5" y="2.5" width="11" height="11" fill="none" stroke="' + BLUE + '" stroke-width="1" stroke-dasharray="2 1.3"/><rect x="5" y="5" width="6" height="6" fill="url(#steel)" ' + S + '/>'
                       + magnifier(14.5, 14.5, 4.2)),
    'zoom-all': (obj_shapes(-1, -2) + magnifier(15.5, 15.5, 4.2)),
    'view-rulers': ('<path d="M2.5 2.5h19v5h-14v14h-5z" fill="#f4e2b0" ' + S + '/>'
                    + ''.join(f'<path d="M{8 + i * 2.7:.1f} 2.5v{2.6 if i % 2 == 0 else 1.6}" stroke="{INK}" stroke-width="0.8"/>' for i in range(5))
                    + ''.join(f'<path d="M2.5 {8 + i * 2.7:.1f}h{2.6 if i % 2 == 0 else 1.6}" stroke="{INK}" stroke-width="0.8"/>' for i in range(5))),
    'view-guides': ('<rect x="3.5" y="3.5" width="17" height="17" fill="url(#paper)" ' + S + '/><path d="M2 9.5h20M15.5 2v20" stroke="' + BLUE + '" stroke-width="1.1" stroke-dasharray="2.4 1.4"/>'),
    'view-grid': ('<rect x="3.5" y="3.5" width="17" height="17" fill="url(#paper)" ' + S + '/>'
                  + ''.join(f'<path d="M{3.5 + i * 3.4:.1f} 3.5v17M3.5 {3.5 + i * 3.4:.1f}h17" stroke="#8fa6c4" stroke-width="0.6"/>' for i in range(1, 5))),
    'snap-guides': ('<path d="M2 12h20" stroke="' + BLUE + '" stroke-width="1.1" stroke-dasharray="2.4 1.4"/>' + '<rect x="6" y="5" width="9" height="7" fill="url(#steel)" ' + S + '/>'
                    + '<path d="M15.5 21v-3.5a3 3 0 0 1 6 0V21" fill="none" stroke="' + RED + '" stroke-width="2"/><path d="M15.5 21h2M19.5 21h2" stroke="#b9c3cd" stroke-width="2"/>'),
    'snap-objects': ('<rect x="2.5" y="6" width="9" height="9" fill="url(#steel)" ' + S + '/><circle cx="16" cy="10.5" r="4.5" fill="url(#blue)" ' + S + '/>' + node_square(11.5, 10.5, 2.4, ORANGE)
                     + '<path d="M13.5 22v-3.5a3 3 0 0 1 6 0V22" fill="none" stroke="' + RED + '" stroke-width="2"/>'),
    'snap-page': ('<rect x="3.5" y="2.5" width="14" height="18" fill="url(#paper)" ' + S + '/><rect x="3.5" y="2.5" width="6" height="5" fill="url(#blue)" ' + S + '/>'
                  + '<path d="M15.5 22v-3.5a3 3 0 0 1 6 0V22" fill="none" stroke="' + RED + '" stroke-width="2"/>'),
    'snap-grid': (''.join(f'<path d="M{3 + i * 4} 2.5v16M2.5 {3 + i * 4}h16" stroke="#8fa6c4" stroke-width="0.6"/>' for i in range(5))
                  + '<rect x="7" y="7" width="8" height="8" fill="url(#blue)" ' + S + '/>' + '<path d="M15.5 22v-3.5a3 3 0 0 1 6 0V22" fill="none" stroke="' + RED + '" stroke-width="2"/>'),
    'snap-dynamic': ('<rect x="3" y="12" width="7" height="7" fill="url(#steel)" ' + S + '/><path d="M6.5 12L18 3.5" stroke="' + GREEN + '" stroke-width="1" stroke-dasharray="2 1.2"/>'
                     + '<circle cx="14" cy="6.5" r="1.4" fill="' + GREEN + '"/>' + '<path d="M15.5 22v-3.5a3 3 0 0 1 6 0V22" fill="none" stroke="' + RED + '" stroke-width="2"/>'),
    'snap-alignment': ('<rect x="3" y="4" width="7" height="6" fill="url(#steel)" ' + S + '/><rect x="12" y="12" width="7" height="6" fill="url(#blue)" ' + S + '/>'
                       + '<path d="M3 4V22M12 18V2" stroke="' + ORANGE + '" stroke-width="0.9" stroke-dasharray="1.6 1.1"/>'),
    'snap-baseline': ('<path d="M2 8.5h20M2 14.5h20M2 20.5h20" stroke="' + BLUE + '" stroke-width="0.8" stroke-dasharray="1.5 1"/>'
                      '<path d="M4 14.5l3-8h1.2l3 8M5 12h5.4" fill="none" stroke="' + INK + '" stroke-width="1.4"/>' + '<path d="M14.5 13v-3.5a3 3 0 0 1 6 0V13" fill="none" stroke="' + RED + '" stroke-width="2"/>'),
    'view-color-depth': (''.join(f'<rect x="{3 + i * 3}" y="4" width="3" height="12" fill="{c}"/>' for i, c in enumerate(['#000000', '#333333', '#666666', '#999999', '#cccccc', '#ffffff']))
                         + '<rect x="3" y="4" width="18" height="12" fill="none" ' + S + '/><path d="M9 16v3h6v-3M7 19.5h10" fill="none" ' + S + '/>'),
    'view-ui-size': (window(2, 4, 13, 11) + window(9, 8, 13, 12) + arrow(4, 20, 8, 16, ORANGE, 1.3, 2)),
    'view-ui-fit': ('<rect x="2.5" y="3.5" width="19" height="13" rx="1" fill="url(#glass)" ' + S + '/><path d="M9 16.5v3h6v-3M7 20h10" fill="none" ' + S + '/>'
                    + arrow(8, 10, 4.5, 6.5, BLUE, 1.2, 1.8) + arrow(16, 10, 19.5, 13.5, BLUE, 1.2, 1.8)),
    'view-language': ('<circle cx="11" cy="11" r="8" fill="url(#glass)" ' + S + '/><path d="M3 11h16M11 3c-3.5 4-3.5 12 0 16M11 3c3.5 4 3.5 12 0 16M4.5 7h13M4.5 15h13" fill="none" stroke="' + INK + '" stroke-width="0.7"/>'
                      + '<rect x="13.5" y="14" width="9" height="7.5" rx="1.5" fill="url(#orange)" ' + S + '/><path d="M16 19.5l2-4 2 4M16.7 18.2h2.6" fill="none" stroke="#ffffff" stroke-width="0.9"/>'),

    # --- Layout --------------------------------------------------------------------------------
    'page-rename': page(3, 2.5, 12, 16) + '<rect x="9" y="13" width="13" height="7" rx="1" fill="#ffffff" ' + S + '/><path d="M11 15.5v2M12.5 15h1.5v3h-1.5M16 15v3M19.5 15v3" stroke="' + BLUE + '" stroke-width="0.9"/><path d="M20.8 14v5" stroke="' + INK + '" stroke-width="0.8"/>',
    'page-move': page(2.5, 4, 10, 14, 3) + page(12, 4, 10, 14, 3) + arrow(5.5, 21.5, 17.5, 21.5, BLUE, 1.4, 2.2),
    'page-orientation': ('<rect x="3" y="2.5" width="10" height="14" fill="url(#paper)" ' + S + '/><rect x="8" y="11" width="14" height="10" fill="url(#paper)" ' + S + '/>'
                         + '<path d="M15.5 4a5 5 0 0 1 4.5 5" fill="none" stroke="' + BLUE + '" stroke-width="1.4" stroke-linecap="round"/><path d="M18 8.6l2 1.2 1.2-2" fill="none" stroke="' + BLUE + '" stroke-width="1.4" stroke-linecap="round" stroke-linejoin="round"/>'),

    # --- Anordnen ------------------------------------------------------------------------------
    'arrange-group': ('<rect x="2.5" y="2.5" width="19" height="19" fill="none" stroke="' + INK + '" stroke-width="1"/>' + node_square(2.5, 2.5, 2.2) + node_square(21.5, 2.5, 2.2) + node_square(2.5, 21.5, 2.2) + node_square(21.5, 21.5, 2.2)
                      + '<rect x="5" y="10" width="8" height="8" fill="url(#steel)" ' + S + '/><circle cx="15" cy="9.5" r="4.5" fill="url(#blue)" ' + S + '/>'),
    'arrange-ungroup': ('<rect x="2.5" y="2.5" width="19" height="19" fill="none" stroke="' + INK + '" stroke-width="1" stroke-dasharray="2 1.5"/>'
                        + '<rect x="4" y="11" width="8" height="8" fill="url(#steel)" ' + S + '/><circle cx="16" cy="8.5" r="4.5" fill="url(#blue)" ' + S + '/>' + node_square(8, 11, 2) + node_square(16, 4, 2)),
    'arrange-ungroup-all': ('<rect x="1.5" y="1.5" width="21" height="21" fill="none" stroke="' + INK + '" stroke-width="0.9" stroke-dasharray="2 1.5"/><rect x="4" y="4" width="16" height="16" fill="none" stroke="' + INK + '" stroke-width="0.9" stroke-dasharray="2 1.5"/>'
                            + '<rect x="6" y="11.5" width="6" height="6" fill="url(#steel)" ' + S + '/><circle cx="15" cy="9" r="3.5" fill="url(#blue)" ' + S + '/>'),
    'shaping': ('<circle cx="9" cy="10" r="6" fill="url(#steel)" ' + S + '/><circle cx="15" cy="14" r="6" fill="url(#blue)" fill-opacity="0.85" ' + S + '/>'
                '<path d="M9.8 16a6 6 0 0 0 5.2-8" fill="none" stroke="' + ORANGE + '" stroke-width="1.6"/>'),
    'page-to-master': (page(2.5, 7, 11, 14, 3) + '<rect x="11.5" y="2.5" width="10" height="13" fill="url(#gold)" ' + S + '/><path d="M14 6h5M14 8.5h5" stroke="#ffffff" stroke-width="0.9"/>' + arrow(6, 5, 11, 5, BLUE, 1.3, 2)),
    'page-from-master': ('<rect x="2.5" y="2.5" width="10" height="13" fill="url(#gold)" ' + S + '/><path d="M5 6h5M5 8.5h5" stroke="#ffffff" stroke-width="0.9"/>' + page(10.5, 7, 11, 14, 3) + arrow(5, 18.5, 10, 18.5, BLUE, 1.3, 2)),

    # --- Effekte -------------------------------------------------------------------------------
    'adjust-menu': ('<path d="M4 4v16M10 4v16M16 4v16" stroke="' + INK + '" stroke-width="1.2"/>' + ''.join(f'<rect x="{x - 2.2}" y="{y - 1.5}" width="4.4" height="3" rx="0.8" fill="url(#orange)" {S}/>' for x, y in ((4, 14), (10, 8), (16, 16)))
                    + '<circle cx="20" cy="6" r="2.6" fill="url(#radial)" ' + S + '/>'),
    'adjust-tone-curve': ('<rect x="3" y="3" width="18" height="18" fill="#ffffff" ' + S + '/><path d="M3 9h18M3 15h18M9 3v18M15 3v18" stroke="#c9d1da" stroke-width="0.6"/>'
                          '<path d="M3 21C9 19 9 8 21 3" fill="none" stroke="' + BLUE + '" stroke-width="1.6"/>' + node_square(10, 13, 2.4)),
    'correction-menu': (photo(2.5, 3, 15, 12) + '<path d="M13 21.5l1-3.4 6.3-6.3 2.4 2.4-6.3 6.3z" fill="url(#steel)" ' + S + '/>' + sparkle(10, 18.5, 2.8)),
    'clip-empty': '<rect x="3" y="3" width="18" height="18" rx="1" fill="#ffffff" stroke="' + INK + '" stroke-width="1.2" stroke-dasharray="2.5 1.5"/><path d="M8 8l8 8M16 8l-8 8" stroke="#b9c3cd" stroke-width="1.2"/>',
    'clip-center': ('<rect x="2.5" y="2.5" width="19" height="19" fill="#ffffff" ' + S + '/><rect x="8" y="8" width="8" height="8" fill="url(#blue)" ' + S + '/>'
                    '<path d="M12 3v3.5M12 17.5V21M3 12h3.5M17.5 12H21" stroke="' + ORANGE + '" stroke-width="1.1"/>'),
    'clip-fill': ('<rect x="2.5" y="5.5" width="19" height="13" fill="#ffffff" ' + S + '/><rect x="2.5" y="2" width="19" height="20" fill="url(#blue)" fill-opacity="0.55" stroke="' + BLUE + '" stroke-width="0.8" stroke-dasharray="1.5 1"/>'
                  '<rect x="2.5" y="5.5" width="19" height="13" fill="none" stroke="' + INK + '" stroke-width="1.2"/>'),
    'clip-stretch': ('<rect x="2.5" y="4.5" width="19" height="15" fill="url(#blue)" fill-opacity="0.6" ' + S + '/>' + arrow(9, 12, 3.5, 12, INK, 1.2, 2) + arrow(15, 12, 20.5, 12, INK, 1.2, 2)
                     + arrow(12, 9, 12, 5.5, INK, 1.2, 2) + arrow(12, 15, 12, 18.5, INK, 1.2, 2)),
    'fx-copy': ('<rect x="2.5" y="3" width="9" height="9" fill="url(#blue)" ' + S + '/>' + '<rect x="12.5" y="12" width="9" height="9" fill="#ffffff" ' + S + '/>'
                + '<rect x="2.5" y="3" width="9" height="9" fill="none" stroke="#000" stroke-opacity="0.25" stroke-width="2.5" transform="translate(0.8 0.8)"/>'
                + arrow(9, 14.5, 12, 17, ORANGE, 1.3, 2) + sparkle(17, 16.5, 2.4)),
    'fx-clone': ('<rect x="2.5" y="3" width="9" height="9" fill="url(#blue)" ' + S + '/>' + '<rect x="12.5" y="12" width="9" height="9" fill="url(#blue)" fill-opacity="0.6" ' + S + '/>'
                 + '<path d="M7 12v4.5h5.5" fill="none" stroke="' + ORANGE + '" stroke-width="1.2" stroke-dasharray="1.6 1"/>' + sparkle(17, 16.5, 2.4)),

    # --- Bitmaps: Befehle ----------------------------------------------------------------------
    'bitmap-straighten': (f'<g transform="rotate(-8 11 11)">{photo(3, 5, 15, 11)}</g>' + '<path d="M2 20.5h20" stroke="' + BLUE + '" stroke-width="1.1" stroke-dasharray="2 1.2"/>'
                          + '<path d="M19 4a6 6 0 0 1 2 5" fill="none" stroke="' + ORANGE + '" stroke-width="1.3" stroke-linecap="round"/>'),
    'bitmap-crop': (photo(4, 4, 15, 12) + '<path d="M7 1.5V17h15.5M1.5 7H17v15.5" fill="none" stroke="' + INK + '" stroke-width="1.8"/>'),
    'bitmap-color-mask': (photo(2.5, 3, 15, 12) + '<rect x="11" y="11" width="11" height="11" rx="1.5" fill="#ffffff" ' + S + '/>'
                          + ''.join(f'<rect x="{12.5 + (i % 2) * 4.3}" y="{12.5 + (i // 2) * 4.3}" width="3.6" height="3.6" fill="{c}" stroke="{INK}" stroke-width="0.5"/>' for i, c in enumerate([RED, GREEN, BLUE, ORANGE]))
                          + '<path d="M12 21l9-9" stroke="' + RED + '" stroke-width="1.3"/>'),
    'bitmap-mode': (photo(2.5, 3, 15, 12) + '<rect x="11" y="12" width="11" height="10" fill="url(#bw)" ' + S + '/>'),
    'bitmap-resample': (''.join(f'<rect x="{2.5 + c * 3.5}" y="{2.5 + r * 3.5}" width="3.5" height="3.5" fill="{"#1f5fae" if (c + r) % 3 == 0 else "#b9c3cd"}" stroke="#ffffff" stroke-width="0.3"/>' for c in range(3) for r in range(3))
                        + '<rect x="2.5" y="2.5" width="10.5" height="10.5" fill="none" ' + S + '/>' + photo(10, 10, 12, 10) + arrow(8, 17, 8, 21, ORANGE, 1.2, 1.8)),
    'trace-lineart': (photo(2.5, 3, 12, 10) + '<path d="M12 21c2-6 4-9 9-11M14 15l2.5 2.5M17 12.5l2 2" fill="none" stroke="' + INK + '" stroke-width="1.4" stroke-linecap="round"/>' + node_square(12, 21, 2.2) + node_square(21, 10, 2.2)),
    'trace-logo': (photo(2.5, 3, 12, 10) + '<circle cx="17" cy="16" r="5" fill="url(#blue)" ' + S + '/><path d="M14.5 16h5M17 13.5v5" stroke="#ffffff" stroke-width="1.3"/>' + node_square(17, 11, 2) + node_square(22, 16, 2)),
    'trace-logo-detail': (photo(2.5, 3, 12, 10) + '<circle cx="17" cy="16" r="5" fill="url(#blue)" ' + S + '/><circle cx="17" cy="16" r="2.8" fill="url(#orange)" ' + S + '/><circle cx="17" cy="16" r="1" fill="#ffffff"/>' + node_square(17, 11, 2) + node_square(22, 16, 2)),
    'trace-clipart': (photo(2.5, 3, 12, 10) + '<path d="M12.5 21l3-7 2.5 3 2-2 2.5 6z" fill="url(#orange)" ' + S + '/><circle cx="19.5" cy="11.5" r="2" fill="url(#gold)" ' + S + '/>'),
    'trace-photo-low': (photo(2.5, 3, 12, 10) + ''.join(f'<rect x="{12 + c * 3.4}" y="{12 + r * 3.4}" width="3.4" height="3.4" fill="{["#5aa0ea", "#1f5fae", "#e2740c"][(c * 2 + r) % 3]}" stroke="{INK}" stroke-width="0.4"/>' for c in range(3) for r in range(3))),
    'trace-photo-high': (photo(2.5, 3, 12, 10) + ''.join(f'<rect x="{12 + c * 2}" y="{12 + r * 2}" width="2" height="2" fill="{["#5aa0ea", "#3c7fd0", "#1f5fae", "#ffb44a", "#e2740c"][(c * 3 + r * 2) % 5]}"/>' for c in range(5) for r in range(5))
                         + '<rect x="12" y="12" width="10" height="10" fill="none" ' + S + '/>'),

    'bitmap-cutout': ('<rect x="2.5" y="3" width="19" height="17" fill="url(#paper)" stroke="#9aa5b1" stroke-width="0.8" stroke-dasharray="1.6 1"/>'
                      + ''.join(f'<rect x="{3 + c * 3}" y="{3.5 + r * 3}" width="1.5" height="1.5" fill="#c9d1da"/>' for c in range(6) for r in range(6) if (c + r) % 2 == 0)
                      + '<circle cx="12" cy="8.5" r="3.2" fill="url(#orange)" ' + S + '/><path d="M5.5 20c0-4 3-6.5 6.5-6.5s6.5 2.5 6.5 6.5z" fill="url(#blue)" ' + S + '/>'
                      + '<path d="M18 2.5l1.5 3 3 .4-2.2 2 .6 3-2.9-1.5-2.8 1.5.5-3-2.2-2 3-.4z" fill="url(#gold)" ' + S + ' stroke-width="0.6"/>'),

    # --- Text ----------------------------------------------------------------------------------
    'text-straighten': ('<path d="M3 9c3-4 6 2 9-2s6 2 9-1" fill="none" stroke="#b9c3cd" stroke-width="1" stroke-dasharray="1.6 1"/>'
                        '<path d="M4 20l3.2-8.5h1.2l3.2 8.5M5.1 17.2h5.4M13.5 20v-8.5h2.8a2 2 0 0 1 0 4h-2.8 3.2a2.2 2.2 0 0 1 0 4.5z" fill="none" stroke="' + INK + '" stroke-width="1.3" stroke-linejoin="round"/>'
                        '<path d="M2 21.5h20" stroke="' + BLUE + '" stroke-width="1.1"/>'),
    'text-frame': '<rect x="3" y="3" width="18" height="18" fill="#ffffff" stroke="' + INK + '" stroke-width="1" stroke-dasharray="2 1.3"/>' + lines(5.5, 7, 13, 4, 3.3, INK, 1.2),
    'text-link': ('<rect x="2" y="2.5" width="9" height="11" fill="#ffffff" stroke="' + INK + '" stroke-width="0.9" stroke-dasharray="1.8 1.1"/>' + lines(3.5, 5, 6, 3, 2.6)
                  + '<rect x="13" y="10.5" width="9" height="11" fill="#ffffff" stroke="' + INK + '" stroke-width="0.9" stroke-dasharray="1.8 1.1"/>' + lines(14.5, 13, 6, 2, 2.6)
                  + arrow(10, 14.5, 13.5, 16.5, BLUE, 1.3, 2)),
    'text-unlink': ('<rect x="2" y="2.5" width="9" height="11" fill="#ffffff" stroke="' + INK + '" stroke-width="0.9" stroke-dasharray="1.8 1.1"/>' + lines(3.5, 5, 6, 3, 2.6)
                    + '<rect x="13" y="10.5" width="9" height="11" fill="#ffffff" stroke="' + INK + '" stroke-width="0.9" stroke-dasharray="1.8 1.1"/>' + lines(14.5, 13, 6, 2, 2.6)
                    + '<path d="M9.5 18.5l4-4M9.5 14.5l4 4" stroke="' + RED + '" stroke-width="1.5" stroke-linecap="round"/>'),
    'text-spell': ('<path d="M3 13l3.5-9h1.4l3.5 9M4.3 10h5.6M13 13V4h3a2.2 2.2 0 0 1 0 4.4h-3 3.4a2.3 2.3 0 0 1 0 4.6z" fill="none" stroke="' + INK + '" stroke-width="1.3" stroke-linejoin="round"/>'
                   '<path d="M5 19l3.5 3 9-9" fill="none" stroke="' + GREEN + '" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>'),

    # --- Tabelle -------------------------------------------------------------------------------
    'table-from-text': (lines(3, 4, 8, 4, 2.8, INK, 1.1) + arrow(12, 8, 15, 8, BLUE, 1.2, 1.8) + table(13, 11, 9, 10, 2, 3)),
    'table-to-text': (table(2.5, 3, 9, 10, 2, 3) + arrow(12, 8, 15, 8, BLUE, 1.2, 1.8) + lines(14, 12, 8, 4, 2.8, INK, 1.1)),
    'table-insert': table() + plus(18.5, 18.5, 4),
    'table-insert-row-above': table(3, 7, 18, 14) + cell(3, 7, 18, 4.67, GREEN, 0.7) + arrow(12, 1.5, 12, 6, GREEN, 1.4, 2),
    'table-insert-row-below': table(3, 3, 18, 14) + cell(3, 12.33, 18, 4.67, GREEN, 0.7) + arrow(12, 22.5, 12, 18, GREEN, 1.4, 2),
    'table-insert-col-left': table(7, 3, 14, 18) + cell(7, 3, 4.67, 18, GREEN, 0.7) + arrow(1.5, 12, 6, 12, GREEN, 1.4, 2),
    'table-insert-col-right': table(3, 3, 14, 18) + cell(12.33, 3, 4.67, 18, GREEN, 0.7) + arrow(22.5, 12, 18, 12, GREEN, 1.4, 2),
    'table-insert-rows': table(3, 3, 18, 18, 3, 4) + cell(3, 7.5, 18, 9, GREEN, 0.6) + plus(19, 19, 3.6),
    'table-insert-cols': table(3, 3, 18, 18, 4, 3) + cell(7.5, 3, 9, 18, GREEN, 0.6) + plus(19, 19, 3.6),
    'table-select': table() + cell(3, 4, 18, 15, BLUE, 0.35) + '<rect x="2" y="3" width="20" height="17" fill="none" stroke="' + BLUE + '" stroke-width="0.9" stroke-dasharray="2 1.2"/>',
    'table-select-table': table() + cell(3, 4, 18, 15, BLUE, 0.5),
    'table-select-row': table() + cell(3, 9, 18, 5, BLUE, 0.6),
    'table-select-col': table() + cell(9, 4, 6, 15, BLUE, 0.6),
    'table-delete': table() + minus(18.5, 18.5, 4),
    'table-delete-row': table() + cell(3, 9, 18, 5, RED, 0.5) + '<path d="M5 11.5h14" stroke="' + RED + '" stroke-width="1.5"/>',
    'table-delete-col': table() + cell(9, 4, 6, 15, RED, 0.5) + '<path d="M12 6v11" stroke="' + RED + '" stroke-width="1.5"/>',
    'table-delete-table': table() + cross(18.5, 18.5, 4),
    'table-distribute': table(3, 4, 18, 15, 3, 3) + arrow(9, 21.5, 4, 21.5, ORANGE, 1.1, 1.6) + arrow(15, 21.5, 20, 21.5, ORANGE, 1.1, 1.6),
    'table-distribute-rows': table(5, 3, 14, 18, 1, 4) + arrow(2.5, 8, 2.5, 3.5, ORANGE, 1.1, 1.6) + arrow(2.5, 16, 2.5, 20.5, ORANGE, 1.1, 1.6),
    'table-distribute-cols': table(3, 5, 18, 14, 4, 1) + arrow(8, 2.5, 3.5, 2.5, ORANGE, 1.1, 1.6) + arrow(16, 2.5, 20.5, 2.5, ORANGE, 1.1, 1.6),
    'table-split-rows': table(3, 3, 18, 18, 2, 2) + cell(3, 3, 9, 9, BLUE, 0.35) + '<path d="M3 7.5h9" stroke="' + ORANGE + '" stroke-width="1.4" stroke-dasharray="1.6 1"/>',
    'table-split-cols': table(3, 3, 18, 18, 2, 2) + cell(3, 3, 9, 9, BLUE, 0.35) + '<path d="M7.5 3v9" stroke="' + ORANGE + '" stroke-width="1.4" stroke-dasharray="1.6 1"/>',
    'table-split-cells': table(3, 3, 18, 18, 1, 1) + '<path d="M12 3v18M3 12h18" stroke="' + ORANGE + '" stroke-width="1.3" stroke-dasharray="1.6 1"/>',

    # --- Extras --------------------------------------------------------------------------------
    'options': gear(9, 9, 6.5) + gear(17.5, 17, 4.5),
    'ai-assistant': ('<path d="M3 5.5a2.5 2.5 0 0 1 2.5-2.5h11a2.5 2.5 0 0 1 2.5 2.5v7a2.5 2.5 0 0 1-2.5 2.5H10l-4 4v-4h-.5A2.5 2.5 0 0 1 3 12.5z" fill="url(#glass)" ' + S + '/>'
                     + sparkle(9, 9, 3.2, BLUE) + sparkle(14.5, 7, 1.8, ORANGE) + sparkle(19.5, 18.5, 2.6, ORANGE)),
    'script-editor': (window(2, 3, 20, 17) + '<path d="M6 10l-2 2 2 2M10 10l2 2-2 2" fill="none" stroke="' + BLUE + '" stroke-width="1.2" stroke-linecap="round" stroke-linejoin="round"/>'
                      + lines(13.5, 10, 6, 3, 2.6) + '<path d="M5 17h12" stroke="#c9d1da" stroke-width="1"/>'),
    'script-folder': folder(2.5, 6, 19, 14) + '<path d="M10 11.5l-2 2 2 2M14 11.5l2 2-2 2" fill="none" stroke="' + BLUE + '" stroke-width="1.2" stroke-linecap="round" stroke-linejoin="round"/>',

    # --- Fenster -------------------------------------------------------------------------------
    'win-cascade': window(2, 2, 13, 10) + window(5.5, 6, 13, 10) + window(9, 10, 13, 10),
    'win-tile': window(2, 4, 9.5, 16) + window(12.5, 4, 9.5, 16),
    'win-next': window(2, 3, 14, 12) + arrow(14, 19, 21.5, 19, BLUE, 1.6, 2.6),
    'win-prev': window(8, 3, 14, 12) + arrow(10, 19, 2.5, 19, BLUE, 1.6, 2.6),
    'docker': window(2, 3, 20, 18) + '<rect x="14" y="6.5" width="7.5" height="14" fill="url(#steel)" ' + S + '/>' + lines(15.5, 9.5, 4.5, 3, 2.8, INK, 0.9),
    'docker-objects': (window(2.5, 2.5, 19, 19) + '<path d="M6 9.5h3M8 12.5h3M8 15.5h3M8 18.5h3M6.8 10v8.5" fill="none" stroke="' + INK + '" stroke-width="0.9"/>'
                       + '<rect x="12" y="11" width="3" height="3" fill="url(#blue)"/><circle cx="13.5" cy="16.5" r="1.5" fill="' + ORANGE + '"/><path d="M17 12.5h3M17 16.5h3" stroke="' + INK + '" stroke-width="0.9"/>'),
    'docker-history': (window(2.5, 2.5, 19, 19) + '<circle cx="12" cy="14" r="5" fill="#ffffff" ' + S + '/><path d="M12 11v3l2 1.5" fill="none" stroke="' + INK + '" stroke-width="1.1" stroke-linecap="round"/>'
                       + '<path d="M6 12.5a6.5 6.5 0 0 1 2-3.5" fill="none" stroke="' + BLUE + '" stroke-width="1.2"/>'),
    'docker-effects': (window(2.5, 2.5, 19, 19) + '<rect x="5.5" y="9" width="6" height="6" fill="url(#blue)" ' + S + '/><rect x="5.5" y="9" width="6" height="6" fill="none" stroke="#000" stroke-opacity="0.25" stroke-width="2" transform="translate(1 1)"/>'
                       + sparkle(16, 12, 3) + lines(5.5, 18.5, 13, 1, 2)),
}


def _fx(name, body):
    ICONS['fx-' + name] = body


# --- Bitmap-Effekte (ID mit - statt .) ---------------------------------------------------------
_fx('adjust-auto', photo(2.5, 3, 16, 12) + '<path d="M14 21.5l1.2-3.6 5.2-5.2 2.4 2.4-5.2 5.2z" fill="url(#gold)" ' + S + '/>' + sparkle(19, 5, 3) + sparkle(13, 18.5, 2))
_fx('adjust-lab', photo(2.5, 3, 15, 12) + '<path d="M12 22.5v-5M17 22.5v-5M22 22.5v-5" stroke="' + INK + '" stroke-width="1"/>' + ''.join(f'<rect x="{x - 1.5}" y="{y}" width="3" height="2" fill="url(#orange)" {S}/>' for x, y in ((12, 18.5), (17, 20), (22, 18))))
_fx('adjust-contrast', '<circle cx="12" cy="12" r="8.5" fill="#ffffff" ' + S + '/><path d="M12 3.5a8.5 8.5 0 0 1 0 17z" fill="' + INK + '"/>' + sparkle(19.5, 19.5, 2.6))
_fx('adjust-localequalize', photo(2.5, 3, 19, 15) + ''.join(f'<rect x="{4 + i * 4.5}" y="{19.5}" width="3.5" height="{2.5}" fill="url(#bw)" {S}/>' for i in range(4)))
_fx('adjust-sampletarget', '<circle cx="9" cy="9" r="6" fill="url(#radial)" ' + S + '/><circle cx="9" cy="9" r="2" fill="none" stroke="#ffffff" stroke-width="1"/>'
    + '<path d="M13.5 20.5l-1-1 5-5 1 1z" fill="url(#steel)" ' + S + '/><path d="M18 14l2.5-2.5 1.5 1.5-2.5 2.5z" fill="' + INK + '"/>' + '<circle cx="18" cy="18" r="2.6" fill="' + ORANGE + '" ' + S + '/>')
_fx('adjust-bci', '<circle cx="8" cy="8" r="3.4" fill="url(#gold)" ' + S + '/><path d="M8 1.5v1.8M8 12.7v1.8M1.5 8h1.8M12.7 8h1.8M3.4 3.4l1.3 1.3M11.3 11.3l1.3 1.3M3.4 12.6l1.3-1.3M11.3 4.7l1.3-1.3" stroke="' + INK + '" stroke-width="1" stroke-linecap="round"/>'
    + '<circle cx="16" cy="16" r="5.5" fill="#ffffff" ' + S + '/><path d="M16 10.5a5.5 5.5 0 0 1 0 11z" fill="' + INK + '"/>')
_fx('adjust-colorbalance', '<path d="M12 3v15M5 18h14" stroke="' + INK + '" stroke-width="1.2"/><path d="M3 8h18" stroke="' + INK + '" stroke-width="1.2" transform="rotate(-10 12 8)"/>'
    + f'<circle cx="5" cy="12" r="3" fill="{RED}" {S}/><circle cx="19" cy="9.5" r="3" fill="{BLUE}" {S}/><path d="M9 21.5h6" stroke="{INK}" stroke-width="1.6"/>')
_fx('adjust-gamma', '<rect x="3" y="3" width="18" height="18" fill="#ffffff" ' + S + '/><path d="M3 21C6 10 12 5 21 3" fill="none" stroke="' + BLUE + '" stroke-width="1.6"/>'
    + '<path d="M11 13.5c1 0 1.8 1 2.5 3.5l1 4M15.5 13.5l-3 8" fill="none" stroke="' + ORANGE + '" stroke-width="1.2" stroke-linecap="round"/>')
_fx('adjust-hsl', ''.join(f'<path d="M12 12L{12 + 9 * __import__("math").cos(a / 6 * 6.2832):.2f} {12 + 9 * __import__("math").sin(a / 6 * 6.2832):.2f}A9 9 0 0 1 {12 + 9 * __import__("math").cos((a + 1) / 6 * 6.2832):.2f} {12 + 9 * __import__("math").sin((a + 1) / 6 * 6.2832):.2f}z" fill="{c}"/>'
                          for a, c in enumerate(['#e53935', '#fdd835', '#43a047', '#00acc1', '#1e88e5', '#8e24aa']))
    + '<circle cx="12" cy="12" r="9" fill="none" ' + S + '/><circle cx="12" cy="12" r="3.5" fill="#ffffff" ' + S + '/>')
_fx('adjust-selectivecolor', ''.join(f'<rect x="{3 + i * 6}" y="4" width="5" height="9" fill="{c}" {S}/>' for i, c in enumerate([RED, GREEN, BLUE]))
    + '<rect x="7.5" y="2.5" width="8" height="12" fill="none" stroke="' + ORANGE + '" stroke-width="1.4"/><path d="M4 18.5h16" stroke="' + INK + '" stroke-width="1"/><rect x="10" y="16.5" width="4" height="4" rx="1" fill="url(#orange)" ' + S + '/>')
_fx('adjust-replacecolors', f'<rect x="2.5" y="3" width="9" height="9" fill="{RED}" {S}/><rect x="12.5" y="12" width="9" height="9" fill="{BLUE}" {S}/>'
    + '<path d="M14 5h4a2 2 0 0 1 2 2v2.5" fill="none" stroke="' + INK + '" stroke-width="1.3"/><path d="M18 8l2 2 2-2" fill="none" stroke="' + INK + '" stroke-width="1.3" stroke-linejoin="round"/>'
    + '<path d="M10 19H6a2 2 0 0 1-2-2v-2.5" fill="none" stroke="' + INK + '" stroke-width="1.3"/><path d="M6 16l-2-2-2 2" fill="none" stroke="' + INK + '" stroke-width="1.3" stroke-linejoin="round"/>')
_fx('adjust-desaturate', '<circle cx="9" cy="10" r="6" fill="url(#hue)" ' + S + '/><circle cx="15" cy="14" r="6" fill="url(#bw)" ' + S + '/>' + arrow(4, 20.5, 9, 20.5, INK, 1.1, 1.6))
_fx('adjust-channelmixer', ''.join(f'<path d="M{5 + i * 7} 4v16" stroke="{INK}" stroke-width="1.1"/><rect x="{3 + i * 7}" y="{[13, 7, 15][i]}" width="4" height="3" rx="0.8" fill="{[RED, GREEN, BLUE][i]}" {S}/>' for i in range(3)))
_fx('correction-invert', '<rect x="3" y="3" width="18" height="18" fill="#ffffff" ' + S + '/><path d="M3 21L21 3v18z" fill="' + INK + '"/><circle cx="8.5" cy="8.5" r="2.8" fill="' + INK + '"/><circle cx="15.5" cy="15.5" r="2.8" fill="#ffffff"/>')
_fx('correction-posterize', ''.join(f'<rect x="{3 + i * 3.6:.1f}" y="4" width="3.6" height="16" fill="#{(40 + i * 45):02x}{(40 + i * 45):02x}{(40 + i * 45):02x}"/>' for i in range(5)) + '<rect x="3" y="4" width="18" height="16" fill="none" ' + S + '/>')

# Modus
_fx('mode-bw', photo(2.5, 3, 15, 12) + ''.join(f'<rect x="{11 + (i % 4) * 2.75}" y="{11 + (i // 4) * 2.75}" width="2.75" height="2.75" fill="{"#000" if (i + i // 4) % 2 else "#fff"}"/>' for i in range(16)) + '<rect x="11" y="11" width="11" height="11" fill="none" ' + S + '/>')
_fx('mode-gray8', photo(2.5, 3, 15, 12) + ''.join(f'<rect x="{11 + i * 2.75}" y="11" width="2.75" height="11" fill="#{(30 + i * 65):02x}{(30 + i * 65):02x}{(30 + i * 65):02x}"/>' for i in range(4)) + '<rect x="11" y="11" width="11" height="11" fill="none" ' + S + '/>')
_fx('mode-gray16', photo(2.5, 3, 15, 12) + '<rect x="11" y="11" width="11" height="11" fill="url(#bw)" ' + S + '/><path d="M12.5 13.5h2M13.5 13.5v4M16 17.5h2.5v-2.2h-2.5v-1.8h2.5" fill="none" stroke="' + ORANGE + '" stroke-width="0.9"/>')
_fx('mode-duotone8', photo(2.5, 3, 15, 12) + '<rect x="11" y="11" width="11" height="11" fill="url(#orange)" ' + S + '/><rect x="11" y="16.5" width="11" height="5.5" fill="' + INK + '" fill-opacity="0.75"/>')
_fx('mode-duotone16', photo(2.5, 3, 15, 12) + '<rect x="11" y="11" width="11" height="11" fill="url(#orange)" ' + S + '/><rect x="11" y="16.5" width="11" height="5.5" fill="' + BLUE + '" fill-opacity="0.85"/><path d="M12.5 12.5h1.5v3M16 12.5h2v3h-2v-1.5h2" fill="none" stroke="#ffffff" stroke-width="0.8"/>')

# 3D-Effekte
_fx('3d-rotate', '<path d="M5 6l9-3 6 4-9 3z" fill="#ffffff" ' + S + '/><path d="M5 6v11l6 4V10z" fill="url(#steel)" ' + S + '/><path d="M11 10l9-3v11l-9 3z" fill="url(#blue)" ' + S + '/>'
    + '<path d="M2 14a10 4 0 0 0 7 6" fill="none" stroke="' + ORANGE + '" stroke-width="1.3"/>')
_fx('3d-perspective', '<path d="M7 4h10l4 16H3z" fill="#ffffff" ' + S + '/><path d="M8.5 10l2.5-3 2 2.5 1.3-1.3 3.3 4.3" fill="none" stroke="' + BLUE + '" stroke-width="1.2"/><path d="M5.5 15h13" stroke="' + BLUE + '" stroke-width="0.8"/>')
_fx('3d-sphere', '<rect x="2.5" y="2.5" width="19" height="19" fill="#ffffff" ' + S + '/><circle cx="12" cy="12" r="7.5" fill="url(#radial)" ' + S + '/>'
    + '<path d="M4.5 12a7.5 3 0 0 0 15 0M12 4.5a3 7.5 0 0 0 0 15" fill="none" stroke="#ffffff" stroke-width="0.7"/>')
_fx('3d-cylinder', '<ellipse cx="12" cy="5" rx="7" ry="2.5" fill="#ffffff" ' + S + '/><path d="M5 5v14a7 2.5 0 0 0 14 0V5" fill="url(#steel)" ' + S + '/><path d="M9 9v10M15 9v10" stroke="' + BLUE + '" stroke-width="1"/>')
_fx('3d-pagecurl', photo(2.5, 3, 19, 18, False) + '<path d="M21.5 13.5c-3 0-6.5 2-8 8h8z" fill="#f0f0f0"/><path d="M21.5 13.5c-4 1-6.5 4-8 8 2.5-.5 5.5-2.5 8-8z" fill="url(#steel)" ' + S + '/>')
_fx('3d-emboss', '<rect x="2.5" y="2.5" width="19" height="19" fill="#b9c3cd" ' + S + '/><path d="M6 17l5-10 6 10" fill="none" stroke="#ffffff" stroke-width="2.2" stroke-linejoin="round" transform="translate(-0.7 -0.7)"/>'
    + '<path d="M6 17l5-10 6 10" fill="none" stroke="#4d5966" stroke-width="2.2" stroke-linejoin="round" transform="translate(0.7 0.7)"/><path d="M6 17l5-10 6 10" fill="none" stroke="#b9c3cd" stroke-width="2" stroke-linejoin="round"/>')
_fx('3d-pinch', '<rect x="2.5" y="2.5" width="19" height="19" fill="#ffffff" ' + S + '/>'
    + '<path d="M2.5 8c6 1 13 1 19 0M2.5 16c6-1 13-1 19 0M8 2.5c1 6 1 13 0 19M16 2.5c-1 6-1 13 0 19" fill="none" stroke="' + BLUE + '" stroke-width="0.9"/>'
    + '<path d="M2.5 12h19M12 2.5v19" stroke="' + BLUE + '" stroke-width="0.9"/>')

# Künstlerische Striche
_fx('art-posterize', photo(2.5, 3, 19, 15) + '<path d="M3.5 17l4-5 3 3 3-4 7 6" fill="' + ORANGE + '" fill-opacity="0.7"/>' + ''.join(f'<rect x="{4 + i * 4}" y="19" width="4" height="3" fill="#{(40 + i * 50):02x}{(40 + i * 50):02x}{(40 + i * 50):02x}" stroke="{INK}" stroke-width="0.5"/>' for i in range(4)))
_fx('art-charcoal', '<rect x="2.5" y="2.5" width="19" height="19" fill="#ffffff" ' + S + '/>' + ''.join(f'<path d="M{4 + i * 1.6:.1f} {19 - i * 0.4:.1f}l{5 + i * 0.3:.1f} {-12 + i * 0.5:.1f}" stroke="#333" stroke-width="{0.9 + (i % 3) * 0.3:.1f}" stroke-opacity="0.75" stroke-linecap="round"/>' for i in range(8))
    + '<path d="M15 20l6-6 1.5 1.5-6 6z" fill="#222" ' + S + '/>')
_fx('art-impressionism', '<rect x="2.5" y="2.5" width="19" height="19" fill="#ffffff" ' + S + '/>' + ''.join(f'<ellipse cx="{5 + (i * 7) % 15}" cy="{5 + (i * 5) % 14}" rx="2.2" ry="1" fill="{["#1f5fae", "#e2740c", "#43a047", "#fdd835", "#5aa0ea"][i % 5]}" transform="rotate({(i * 37) % 180} {5 + (i * 7) % 15} {5 + (i * 5) % 14})"/>' for i in range(16)))
_fx('art-watercolor', '<rect x="2.5" y="2.5" width="19" height="19" fill="#ffffff" ' + S + '/><circle cx="9" cy="10" r="5" fill="' + BLUE + '" fill-opacity="0.45"/><circle cx="14.5" cy="13" r="5" fill="' + ORANGE + '" fill-opacity="0.45"/>'
    + '<path d="M18 22l3.5-9.5 1 .4-3 9.6z" fill="#c9a56a" ' + S + '/>')
_fx('art-scraperboard', '<rect x="2.5" y="2.5" width="19" height="19" fill="#222" ' + S + '/>' + ''.join(f'<path d="M{5 + i * 2.2:.1f} 19c1-4 1.5-9 0.5-13" fill="none" stroke="#ffffff" stroke-width="0.8"/>' for i in range(7)))
_fx('art-wavypaper', '<rect x="2.5" y="2.5" width="19" height="19" fill="url(#paper)" ' + S + '/>' + ''.join(f'<path d="M3 {5 + i * 3}c3-1.5 4.5 1.5 7.5 0s4.5 1.5 7.5 0 2.5 0 3 0" fill="none" stroke="#8fa6c4" stroke-width="0.9"/>' for i in range(6)))
_fx('art-sketch', '<rect x="2.5" y="4.5" width="17" height="17" fill="#ffffff" ' + S + '/><path d="M2.5 4.5c2-2 4-2 6 0M8.5 4.5c2-2 4-2 6 0M14.5 4.5c2-2 3-2 5 0" fill="none" stroke="' + INK + '" stroke-width="0.8"/>'
    + '<path d="M5.5 17c2-5 5-8 9-9M6 14l7-6M7 18l8-7" fill="none" stroke="#555" stroke-width="0.8" stroke-linecap="round"/><path d="M15 21l6-6 1.5 1.5-6 6-2 .5z" fill="url(#gold)" ' + S + '/>')
_fx('art-oilpaint', '<rect x="2.5" y="2.5" width="19" height="19" fill="#ffffff" ' + S + '/><path d="M4 8c4-3 8 2 15-2M4 13c5-2 8 3 15-1M4 18c4-2 9 2 15-2" fill="none" stroke-width="2.6" stroke-linecap="round" stroke="' + BLUE + '"/>'
    + '<path d="M4 8c4-3 8 2 15-2" fill="none" stroke="' + ORANGE + '" stroke-width="2.6" stroke-linecap="round"/>')

# Unschärfe
_fx('blur-smooth', '<rect x="2.5" y="2.5" width="19" height="19" fill="#ffffff" ' + S + '/><path d="M4 16c3-6 5 2 8-4s5 2 8-4" fill="none" stroke="' + BLUE + '" stroke-width="1.8" stroke-linecap="round"/>'
    + '<path d="M4 19c3-3 5 1 8-2s5 1 8-2" fill="none" stroke="#b9c3cd" stroke-width="1" stroke-linecap="round"/>')
_fx('blur-soften', '<rect x="2.5" y="2.5" width="19" height="19" fill="#ffffff" ' + S + '/><circle cx="12" cy="12" r="6.5" fill="' + BLUE + '" fill-opacity="0.25"/><circle cx="12" cy="12" r="5" fill="' + BLUE + '" fill-opacity="0.4"/><circle cx="12" cy="12" r="3.5" fill="' + BLUE + '" fill-opacity="0.7"/>')
_fx('blur-gaussian', '<rect x="2.5" y="2.5" width="19" height="19" fill="#ffffff" ' + S + '/><path d="M3.5 19C8 19 9 6 12 6s4 13 8.5 13" fill="url(#blue)" fill-opacity="0.6" stroke="' + BLUE + '" stroke-width="1.2"/>')
_fx('blur-motion', '<rect x="2.5" y="2.5" width="19" height="19" fill="#ffffff" ' + S + '/><circle cx="15" cy="12" r="4.5" fill="url(#blue)" ' + S + '/>'
    + '<path d="M4 9h6M3.5 12h6M4 15h6" stroke="' + BLUE + '" stroke-width="1.3" stroke-linecap="round" stroke-opacity="0.6"/>')
_fx('blur-radial', '<rect x="2.5" y="2.5" width="19" height="19" fill="#ffffff" ' + S + '/>' + ''.join(f'<path d="M12 12m{-7 + i * 1.6:.1f} 0a{7 - i * 1.6:.1f} {7 - i * 1.6:.1f} 0 1 0 0 -0.01" fill="none" stroke="{BLUE}" stroke-width="1" stroke-opacity="{0.25 + i * 0.18:.2f}"/>' for i in range(4))
    + '<path d="M12 5a7 7 0 0 1 7 7" fill="none" stroke="' + ORANGE + '" stroke-width="1.4" stroke-linecap="round"/>')
_fx('blur-zoom', '<rect x="2.5" y="2.5" width="19" height="19" fill="#ffffff" ' + S + '/><circle cx="12" cy="12" r="2.5" fill="url(#blue)" ' + S + '/>'
    + ''.join(f'<path d="M{12 + 4 * dx} {12 + 4 * dy}L{12 + 8.5 * dx} {12 + 8.5 * dy}" stroke="{BLUE}" stroke-width="1.1" stroke-linecap="round" stroke-opacity="0.7"/>' for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1), (.7, .7), (-.7, .7), (.7, -.7), (-.7, -.7))))

# Farbtransformation
_fx('color-solarize', '<rect x="2.5" y="2.5" width="19" height="19" fill="url(#gold)" ' + S + '/><circle cx="12" cy="12" r="5" fill="' + INK + '"/>'
    + ''.join(f'<path d="M{12 + 6.5 * __import__("math").cos(a * 0.7854):.2f} {12 + 6.5 * __import__("math").sin(a * 0.7854):.2f}L{12 + 8.5 * __import__("math").cos(a * 0.7854):.2f} {12 + 8.5 * __import__("math").sin(a * 0.7854):.2f}" stroke="{INK}" stroke-width="1.3" stroke-linecap="round"/>' for a in range(8)))
_fx('color-psychedelic', ''.join(f'<rect x="2.5" y="{2.5 + i * 3.8:.1f}" width="19" height="3.8" fill="{c}"/>' for i, c in enumerate(['#e53935', '#fdd835', '#43a047', '#1e88e5', '#8e24aa']))
    + '<rect x="2.5" y="2.5" width="19" height="19" fill="none" ' + S + '/><circle cx="12" cy="12" r="4" fill="none" stroke="#ffffff" stroke-width="1.4"/>')
_fx('color-halftone', '<rect x="2.5" y="2.5" width="19" height="19" fill="#ffffff" ' + S + '/>' + ''.join(f'<circle cx="{5 + c * 3.5}" cy="{5 + r * 3.5}" r="{0.4 + (c + r) * 0.16:.2f}" fill="{[BLUE, ORANGE, INK][(c + r) % 3]}"/>' for c in range(5) for r in range(5)))

# Kontur
_fx('contour-findedges', '<rect x="2.5" y="2.5" width="19" height="19" fill="#ffffff" ' + S + '/><path d="M4 19l5-9 3 4 3-6 5 11" fill="none" stroke="' + INK + '" stroke-width="1.3" stroke-linejoin="round"/>' + magnifier(16, 8, 3))
_fx('contour-detectedges', '<rect x="2.5" y="2.5" width="19" height="19" fill="#222" ' + S + '/><path d="M4 19l5-9 3 4 3-6 5 11" fill="none" stroke="#ffffff" stroke-width="1.3" stroke-linejoin="round"/>')
_fx('contour-trace', '<rect x="2.5" y="2.5" width="19" height="19" fill="#ffffff" ' + S + '/><circle cx="12" cy="12" r="6" fill="none" stroke="' + BLUE + '" stroke-width="1.3"/><circle cx="12" cy="12" r="3.5" fill="none" stroke="' + BLUE + '" stroke-width="0.8"/>'
    + '<path d="M17 4l3 3-8 8-3.5.5.5-3.5z" fill="url(#gold)" ' + S + '/>')

# Kreativ
_fx('creative-fabric', '<rect x="2.5" y="2.5" width="19" height="19" fill="#ffffff" ' + S + '/>' + ''.join(f'<rect x="{2.5 + c * 3.8:.1f}" y="{2.5 + r * 3.8:.1f}" width="3.8" height="3.8" fill="{BLUE if (c + r) % 2 else ORANGE}" fill-opacity="0.6"/>' for c in range(5) for r in range(5))
    + ''.join(f'<path d="M{2.5 + i * 3.8:.1f} 2.5v19M2.5 {2.5 + i * 3.8:.1f}h19" stroke="#ffffff" stroke-width="0.8"/>' for i in range(1, 5)))
_fx('creative-glassblock', '<rect x="2.5" y="2.5" width="19" height="19" fill="url(#glass)" ' + S + '/>' + ''.join(f'<rect x="{3.5 + c * 6}" y="{3.5 + r * 6}" width="5" height="5" fill="none" stroke="#ffffff" stroke-width="1"/><path d="M{4.2 + c * 6} {7.8 + r * 6}l3.5-3.5" stroke="#ffffff" stroke-width="0.6"/>' for c in range(3) for r in range(3)))
_fx('creative-frame', '<rect x="2" y="2" width="20" height="20" fill="url(#gold)" ' + S + '/><rect x="5.5" y="5.5" width="13" height="13" fill="#ffffff" ' + S + '/>' + photo(6.5, 7, 11, 10))
_fx('creative-smokedglass', photo(2.5, 3, 19, 18) + '<rect x="2.5" y="3" width="19" height="18" fill="#556270" fill-opacity="0.45"/><path d="M5 6l4-2M13 19l6-4" stroke="#ffffff" stroke-width="1" stroke-opacity="0.8"/>')
_fx('creative-crystallize', '<rect x="2.5" y="2.5" width="19" height="19" fill="#ffffff" ' + S + '/><path d="M2.5 9l6-1 3-5.5M8.5 8l2 6-8 2M10.5 14l6-2 5 3M16.5 12l-2-9.5M16.5 12l5-4M10.5 14l1 7.5M14.5 21.5l2-6.5" fill="none" stroke="' + INK + '" stroke-width="0.9"/>'
    + '<path d="M8.5 8l2 6 6-2-2-9.5-3.5 0z" fill="url(#blue)" fill-opacity="0.6"/>')
_fx('creative-mosaic', ''.join(f'<rect x="{2.5 + c * 4.75}" y="{2.5 + r * 4.75}" width="4.4" height="4.4" rx="0.6" fill="{["#5aa0ea", "#1f5fae", "#e2740c", "#ffb44a", "#b9c3cd"][(c * 3 + r * 2) % 5]}" stroke="{INK}" stroke-width="0.5"/>' for c in range(4) for r in range(4)))
_fx('creative-vignette', photo(2.5, 3, 19, 18) + '<rect x="2.5" y="3" width="19" height="18" fill="url(#vign)"/>'
    + '<defs><radialGradient id="vign" cx="0.5" cy="0.5" r="0.7"><stop offset="0.45" stop-color="#000" stop-opacity="0"/><stop offset="1" stop-color="#000" stop-opacity="0.85"/></radialGradient></defs>')

# Verzerren
_fx('distort-pixelate', ''.join(f'<rect x="{2.5 + c * 4.75}" y="{2.5 + r * 4.75}" width="4.75" height="4.75" fill="{"#1f5fae" if (c - 1.5) ** 2 + (r - 1.5) ** 2 < 2.5 else ("#5aa0ea" if r < 2 else "#b9c3cd")}"/>' for c in range(4) for r in range(4))
    + '<rect x="2.5" y="2.5" width="19" height="19" fill="none" ' + S + '/>')
_fx('distort-ripple', '<rect x="2.5" y="2.5" width="19" height="19" fill="#ffffff" ' + S + '/>' + ''.join(f'<path d="M3 {6 + i * 4}c2.5-2.5 4.5 2.5 7 0s4.5 2.5 7 0 3.5-1 4 0" fill="none" stroke="{BLUE}" stroke-width="1.3"/>' for i in range(4)))
_fx('distort-swirl', '<rect x="2.5" y="2.5" width="19" height="19" fill="#ffffff" ' + S + '/><path d="M12 12c0-1.5 2-1.5 2 0s-1.5 3.5-4 3-3.5-4-1-6 7-1 7 3-4 7-8 6-6-5-4-9" fill="none" stroke="' + BLUE + '" stroke-width="1.4" stroke-linecap="round"/>')
_fx('distort-wind', '<rect x="2.5" y="2.5" width="19" height="19" fill="#ffffff" ' + S + '/><path d="M4 8h11a2.5 2.5 0 1 0-2.5-2.5M4 12h14a2.5 2.5 0 1 1-2.5 2.5M4 16h8" fill="none" stroke="' + BLUE + '" stroke-width="1.4" stroke-linecap="round"/>')
_fx('distort-tiles', ''.join(f'<g transform="rotate({[-4, 3, 5, -3][c * 2 + r]} {7 + c * 10} {7 + r * 10})"><rect x="{3 + c * 9.5}" y="{3 + r * 9.5}" width="8.5" height="8.5" fill="{"url(#blue)" if (c + r) % 2 else "url(#steel)"}" {S}/></g>' for c in range(2) for r in range(2)))
_fx('distort-diffuse', '<rect x="2.5" y="2.5" width="19" height="19" fill="#ffffff" ' + S + '/><circle cx="11" cy="12" r="4.5" fill="url(#blue)"/>'
    + ''.join(f'<rect x="{x}" y="{y}" width="1.4" height="1.4" fill="{BLUE}"/>' for x, y in ((5, 6), (16.5, 7), (17, 15), (6, 17), (14.5, 18.5), (4.5, 12), (18.5, 11), (9, 4.5), (12, 19.5))))

# Rauschen
def _noise(n, opacity=1.0):
    import random
    rnd = random.Random(7)
    return ''.join(f'<rect x="{rnd.uniform(3, 20):.1f}" y="{rnd.uniform(3, 20):.1f}" width="1.1" height="1.1" fill="{"#ffffff" if rnd.random() < 0.5 else INK}" fill-opacity="{opacity}"/>' for _ in range(n))


_fx('noise-add', '<rect x="2.5" y="2.5" width="19" height="19" fill="#b9c3cd" ' + S + '/>' + _noise(60) + plus(18, 18, 3.8))
_fx('noise-median', '<rect x="2.5" y="2.5" width="19" height="19" fill="#b9c3cd" ' + S + '/>' + _noise(18, 0.5) + minus(18, 18, 3.8))

# Schärfe
_fx('sharpen-sharpen', '<path d="M12 2.5l8 17H4z" fill="url(#steel)" ' + S + '/><path d="M12 2.5l8 17h-8z" fill="url(#blue)"/><path d="M12 2.5l8 17H4z" fill="none" ' + S + '/>')
_fx('sharpen-unsharp', '<circle cx="9" cy="12" r="6" fill="' + BLUE + '" fill-opacity="0.35"/><circle cx="9" cy="12" r="4.5" fill="' + BLUE + '" fill-opacity="0.45"/><path d="M15 4.5l6.5 15H8.5z" fill="url(#blue)" ' + S + '/>')

# Untermenüs der Bitmaps (Kategoriesymbole)
ICONS['bitmap-3d'] = photo(2.5, 3, 12, 10) + '<path d="M12 13l5-2.5 5 2.5v6l-5 2.5-5-2.5z" fill="url(#steel)" ' + S + '/><path d="M12 13l5 2.5 5-2.5M17 15.5v6" fill="none" ' + S + '/>'
ICONS['bitmap-art'] = photo(2.5, 3, 12, 10) + '<path d="M12.5 21.5l1-3 7.3-7.3 2 2-7.3 7.3z" fill="url(#gold)" ' + S + '/><path d="M12.5 21.5l1-3 2 2z" fill="' + INK + '"/>'
ICONS['bitmap-blur'] = photo(2.5, 3, 12, 10) + '<circle cx="17.5" cy="17" r="5" fill="' + BLUE + '" fill-opacity="0.3"/><circle cx="17.5" cy="17" r="3.4" fill="' + BLUE + '" fill-opacity="0.5"/><circle cx="17.5" cy="17" r="1.8" fill="' + BLUE + '"/>'
ICONS['bitmap-color'] = photo(2.5, 3, 12, 10) + ''.join(f'<rect x="12" y="{12 + i * 2.2:.1f}" width="10" height="2.2" fill="{c}"/>' for i, c in enumerate(['#e53935', '#fdd835', '#43a047', '#1e88e5', '#8e24aa'])) + '<rect x="12" y="12" width="10" height="11" fill="none" ' + S + '/>'
ICONS['bitmap-contour'] = photo(2.5, 3, 12, 10) + '<rect x="12" y="12" width="10" height="10" fill="#ffffff" ' + S + '/><path d="M13 21l3-5 2 2.5 3-5" fill="none" stroke="' + INK + '" stroke-width="1.2" stroke-linejoin="round"/>'
ICONS['bitmap-creative'] = photo(2.5, 3, 12, 10) + ''.join(f'<rect x="{12 + c * 3.4}" y="{12 + r * 3.4}" width="3" height="3" rx="0.5" fill="{["#5aa0ea", "#e2740c", "#ffb44a"][(c + r) % 3]}" stroke="{INK}" stroke-width="0.4"/>' for c in range(3) for r in range(3))
ICONS['bitmap-distort'] = photo(2.5, 3, 12, 10) + '<rect x="12" y="12" width="10" height="10" fill="#ffffff" ' + S + '/><path d="M17 17c0-1 1.2-1 1.2 0s-1 2-2.5 1.8-2-2.5-.5-3.6 4-.5 4 1.8-2.5 4-4.6 3.4" fill="none" stroke="' + BLUE + '" stroke-width="1.1" stroke-linecap="round"/>'
ICONS['bitmap-noise'] = photo(2.5, 3, 12, 10) + '<rect x="12" y="12" width="10" height="10" fill="#b9c3cd" ' + S + '/>' + ''.join(f'<rect x="{12.5 + (i * 5) % 9}" y="{12.5 + (i * 7) % 9}" width="1" height="1" fill="{"#fff" if i % 2 else INK}"/>' for i in range(18))
ICONS['bitmap-sharpen'] = photo(2.5, 3, 12, 10) + '<path d="M17 11l5 11H12z" fill="url(#blue)" ' + S + '/>'


if __name__ == '__main__':
    print(len(ICONS), 'Menüsymbole')

# --- Eigene Symbolleisten und Freistellungs-Assistent ------------------------------------------
_BAR = ('<rect x="2" y="5" width="16" height="7" rx="1" fill="url(#steel)" ' + S + '/>'
        '<rect x="4" y="6.8" width="3.4" height="3.4" fill="url(#blue)"/><rect x="8.5" y="6.8" width="3.4" height="3.4" fill="url(#orange)"/>'
        '<rect x="13" y="6.8" width="3.4" height="3.4" fill="url(#gold)"/>')
ICONS['toolbar-new'] = _BAR + plus(17.5, 17.5)
ICONS['toolbar-customize'] = _BAR + gear(16.5, 17.5, 4.2)
_MARKER = ('<path d="M6 19.5l1.2-4.3 9.6-9.6a1.8 1.8 0 0 1 2.6 0l.5.5a1.8 1.8 0 0 1 0 2.6l-9.6 9.6z" fill="url(#steel)" ' + S + '/>'
           '<path d="M6 19.5l1.2-4.3 3.1 3.1z" fill="' + GREEN + '" ' + S + '/>')
ICONS['cutout-marker'] = '<path d="M2.5 21.5c3-1.5 6-1.5 9 0s6 1.5 9 0" fill="none" stroke="' + GREEN + '" stroke-width="2.6" stroke-linecap="round" opacity="0.8"/>' + _MARKER
_BRUSH = ('<path d="M9.5 13.5l8.3-8.3a1.6 1.6 0 0 1 2.3 2.3l-8.3 8.3z" fill="url(#gold)" ' + S + '/>'
          '<path d="M9.5 13.5c-2.5 0-4 1.6-4 3.6 0 1.5-.8 2.6-2.3 3.2 3.6.9 7.8-.3 8.6-4.5z" fill="url(#blue)" ' + S + '/>')
ICONS['cutout-keep'] = _BRUSH + plus(18, 18, 3.8)
ICONS['cutout-remove'] = _BRUSH + minus(18, 18, 3.8)
ICONS['zoom-100'] = (magnifier(10.5, 10.5, 7) +
                     '<text x="10.5" y="13" font-family="Arial" font-size="6.5" font-weight="bold" text-anchor="middle" fill="' + INK + '">1:1</text>')
ICONS['bitmap-cutout-lab'] = (ICONS['bitmap-cutout'].split('<path d="M18 2.5')[0]
                              + '<path d="M5 21c0-5 3.2-8 7-8s7 3 7 8" fill="none" stroke="' + GREEN + '" stroke-width="2.4" opacity="0.85"/>'
                              + '<circle cx="12" cy="8.5" r="4.4" fill="none" stroke="' + GREEN + '" stroke-width="2.4" opacity="0.85"/>')

# --- Hilfe --------------------------------------------------------------
ICONS['help-about'] = ('<circle cx="12" cy="12" r="9.5" fill="url(#blue)" ' + S + '/>'
                       '<circle cx="12" cy="7.3" r="1.5" fill="#ffffff"/><path d="M10 10.5h3v7h1.6M10 17.5h4.6" fill="none" '
                       'stroke="#ffffff" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"/>')
ICONS['help-web'] = ('<circle cx="12" cy="12" r="9.5" fill="url(#paper)" ' + S + '/>'
                     '<ellipse cx="12" cy="12" rx="4.2" ry="9.5" fill="none" ' + S + '/>'
                     '<path d="M2.5 12h19M4 7h16M4 17h16" fill="none" ' + S + '/>')
