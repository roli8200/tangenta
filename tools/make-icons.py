#!/usr/bin/env python3
"""Erzeugt den Symbolsatz unter resources/icons/ (eigene Zeichnungen, GPL wie das Projekt).

Klassischer Stil: kleine, klare Glyphen auf 24er-Raster mit dunklem
Umriss und hellen Flächen mit leichtem Verlauf. Beim Schreiben werden alle Farben in
Graustufen gleicher Helligkeit umgerechnet, damit die Leisten ruhig wirken.

    python tools/make-icons.py
"""

import os
import re

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'resources', 'icons')

INK = '#3a3a3a'

DEFS = '''<defs>
<linearGradient id="paper" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#ffffff"/><stop offset="1" stop-color="#dfe6ee"/></linearGradient>
<linearGradient id="steel" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#f4f6f8"/><stop offset="1" stop-color="#b9c3cd"/></linearGradient>
<linearGradient id="blue" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#5aa0ea"/><stop offset="1" stop-color="#1f5fae"/></linearGradient>
<linearGradient id="glass" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#ffffff"/><stop offset="1" stop-color="#9ec9ef"/></linearGradient>
<linearGradient id="folder" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#ffd967"/><stop offset="1" stop-color="#e3a51c"/></linearGradient>
<linearGradient id="orange" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#ffb44a"/><stop offset="1" stop-color="#e2740c"/></linearGradient>
<linearGradient id="gold" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#ffe08a"/><stop offset="1" stop-color="#c99312"/></linearGradient>
<linearGradient id="bw" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#1a1a1a"/><stop offset="1" stop-color="#ffffff"/></linearGradient>
<linearGradient id="hue" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#ffffff"/><stop offset="1" stop-color="#e53935"/></linearGradient>
<linearGradient id="shade" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#000000" stop-opacity="0"/><stop offset="1" stop-color="#000000"/></linearGradient>
<radialGradient id="radial" cx="0.5" cy="0.5" r="0.6"><stop offset="0" stop-color="#ffffff"/><stop offset="1" stop-color="#1f5fae"/></radialGradient>
</defs>'''


def dedupe_attributes(markup):
    """Doppelte Attribute in einem Tag (z.B. stroke-width aus S und danach überschrieben)
    sind ungültiges XML; das spätere gewinnt."""
    def fix(match):
        tag = match.group(0)
        attrs = re.findall(r'([\w:-]+)="([^"]*)"', tag)
        name = re.match(r'<\s*([\w:-]+)', tag).group(1)
        merged = {}
        for key, value in attrs:
            merged.pop(key, None)
            merged[key] = value
        closing = '/>' if tag.rstrip().endswith('/>') else '>'
        return '<' + name + ''.join(' %s="%s"' % kv for kv in merged.items()) + closing
    return re.sub(r'<[a-zA-Z][^<>]*>', fix, markup)


def to_gray(markup):
    """Jede Farbe #rgb/#rrggbb durch das Grau gleicher Helligkeit (Rec. 709) ersetzen."""
    def gray(match):
        h = match.group(1)
        if len(h) == 3:
            h = ''.join(c * 2 for c in h)
        r, g, b = (int(h[i:i + 2], 16) for i in (0, 2, 4))
        y = round(0.2126 * r + 0.7152 * g + 0.0722 * b)
        return '#%02x%02x%02x' % (y, y, y)
    return re.sub(r'#([0-9a-fA-F]{6}|[0-9a-fA-F]{3})\b', gray, markup)


def svg(body):
    return to_gray(dedupe_attributes('<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24">'
                                     + DEFS + body + '</svg>\n'))


S = 'stroke="%s" stroke-width="1" stroke-linejoin="round" stroke-linecap="round"' % INK

ICONS = {
    # --- Hilfsmittel ---------------------------------------------------------------------
    'tool-pick': f'<path d="M6.5 2.8v16.4l4.3-4 2.9 6.1 2.7-1.3-2.9-6h5.9z" fill="#1d1d1d" stroke="#ffffff" stroke-width="0.9" stroke-linejoin="round"/>',
    'tool-shape': f'<path d="M5 2.8v14l3.6-3.4 2.5 5.2 2.3-1.1-2.5-5.1h5z" fill="#ffffff" {S}/>'
                  f'<path d="M12 21.5c3-0.5 5.5-2.6 7.5-6" fill="none" stroke="#1f5fae" stroke-width="1.2"/>'
                  f'<rect x="17.5" y="12.5" width="4" height="4" fill="#1f5fae" stroke="#ffffff" stroke-width="0.6"/>',
    'tool-zoom': f'<circle cx="10" cy="10" r="6.2" fill="url(#glass)" stroke="{INK}" stroke-width="1.6"/>'
                 f'<path d="M14.6 14.6l5.8 5.8" stroke="#5b4636" stroke-width="3.2" stroke-linecap="round"/>'
                 f'<path d="M7 8.2a3.5 3.5 0 0 1 3-2.4" fill="none" stroke="#ffffff" stroke-width="1.2" stroke-linecap="round"/>',
    'tool-rectangle': f'<rect x="3.5" y="6" width="17" height="12" fill="url(#steel)" {S} stroke-width="1.3"/>'
                      f'<rect x="2.3" y="4.8" width="2.4" height="2.4" fill="#ffffff" stroke="{INK}" stroke-width="0.8"/>'
                      f'<rect x="19.3" y="16.8" width="2.4" height="2.4" fill="#ffffff" stroke="{INK}" stroke-width="0.8"/>',
    'tool-ellipse': f'<ellipse cx="12" cy="12" rx="8.7" ry="6.3" fill="url(#steel)" {S} stroke-width="1.3"/>',
    'tool-polygon': f'<path d="M12 3.3l8.3 6-3.2 9.7H6.9L3.7 9.3z" fill="url(#steel)" {S} stroke-width="1.3"/>',
    'tool-text': f'<path d="M4.5 20.5L10.6 3.5h2.8l6.1 17h-3.2l-1.5-4.4H9.2l-1.5 4.4zM10.1 13.4h3.8L12 7.6z" fill="#1d1d1d"/>',
    'tool-interactive-fill': f'<rect x="3" y="3" width="18" height="18" rx="1" fill="url(#bw)" {S}/>'
                             f'<path d="M6.5 17.5L17.5 6.5" stroke="#ffffff" stroke-width="2.6"/>'
                             f'<path d="M6.5 17.5L17.5 6.5" stroke="{INK}" stroke-width="1" stroke-dasharray="2 1.5"/>'
                             f'<rect x="4.5" y="15.5" width="4" height="4" fill="#1d1d1d" stroke="#ffffff" stroke-width="0.8"/>'
                             f'<rect x="15.5" y="4.5" width="4" height="4" fill="#ffffff" stroke="{INK}" stroke-width="0.8"/>',
    'tool-fill': f'<path d="M4 11.5l6.5-6.5 7 7-6.5 6.5c-.8.8-2 .8-2.8 0l-4.2-4.2c-.8-.8-.8-2 0-2.8z" fill="url(#steel)" {S} stroke-width="1.2"/>'
                 f'<path d="M4.4 12l13-0.2" stroke="{INK}" stroke-width="0.8"/>'
                 f'<path d="M4.6 12.2l12.7-0.2-6.3 6.3c-.8.8-2 .8-2.8 0z" fill="url(#orange)"/>'
                 f'<path d="M10.5 5L8 2.5" stroke="{INK}" stroke-width="1.2"/>'
                 f'<path d="M19.6 14.2c1 1.6 1.9 2.8 1.9 3.9a1.9 1.9 0 0 1-3.8 0c0-1.1.9-2.3 1.9-3.9z" fill="url(#orange)" {S} stroke-width="0.8"/>',
    'tool-outline': f'<path d="M12 2.5l5 9.5-5 9-5-9z" fill="url(#steel)" {S} stroke-width="1.2"/>'
                    f'<path d="M12 2.5v11" stroke="{INK}" stroke-width="1"/>'
                    f'<circle cx="12" cy="13.6" r="1.4" fill="#1f5fae" stroke="{INK}" stroke-width="0.6"/>'
                    f'<path d="M7.6 17.6h8.8" stroke="#1f5fae" stroke-width="1.6"/>',
    # --- Standardleiste --------------------------------------------------------------------
    'doc-new': f'<path d="M5 2.5h9l5 5v14H5z" fill="url(#paper)" {S}/><path d="M14 2.5v5h5" fill="#e9eef3" {S}/>'
               f'<path d="M18.5 1l.9 2.2 2.3.2-1.8 1.5.6 2.3-2-1.3-2 1.3.6-2.3-1.8-1.5 2.3-.2z" fill="#ffd23f" stroke="#c48a00" stroke-width="0.5"/>',
    'doc-open': f'<path d="M2.5 6h6l2 2h9v3H6.8l-3.3 9H2.5z" fill="url(#folder)" stroke="#9c6d0a" stroke-width="1" stroke-linejoin="round"/>'
                f'<path d="M6.8 11h15.2l-3.6 9.5H3.2z" fill="#ffe08a" stroke="#9c6d0a" stroke-width="1" stroke-linejoin="round"/>',
    'doc-save': f'<path d="M3.5 3.5h14l3 3v14h-17z" fill="url(#blue)" stroke="#173f73" stroke-width="1" stroke-linejoin="round"/>'
                f'<rect x="7" y="3.5" width="9" height="6" fill="#e8eef5" stroke="#173f73" stroke-width="0.8"/>'
                f'<rect x="13" y="4.5" width="2" height="4" fill="#173f73"/>'
                f'<rect x="6" y="13" width="12" height="7.5" rx="0.5" fill="#ffffff" stroke="#173f73" stroke-width="0.8"/>'
                f'<path d="M8 15.5h8M8 18h8" stroke="#9fb3c8" stroke-width="0.8"/>',
    'edit-delete': f'<path d="M5 6.5h14M9.5 6.5V4.5h5v2" fill="none" stroke="{INK}" stroke-width="1.3"/>'
                   f'<path d="M6.5 6.5l1 14h9l1-14z" fill="url(#steel)" {S}/>'
                   f'<path d="M10 9.5v8M14 9.5v8" stroke="{INK}" stroke-width="1"/>',
    'edit-cut':f'<path d="M8.5 3l6.5 11M15.5 3L9 14" stroke="#8a96a3" stroke-width="2" stroke-linecap="round"/>'
                f'<circle cx="7.5" cy="17.5" r="3.2" fill="none" stroke="#d9342b" stroke-width="1.8"/>'
                f'<circle cx="16.5" cy="17.5" r="3.2" fill="none" stroke="#d9342b" stroke-width="1.8"/>',
    'edit-copy': f'<path d="M3.5 2.5h8l3 3v11h-11z" fill="url(#paper)" {S}/>'
                 f'<path d="M9.5 7.5h8l3 3v11h-11z" fill="url(#paper)" {S}/>'
                 f'<path d="M12 13h6M12 15.5h6M12 18h4" stroke="#9fb3c8" stroke-width="0.8"/>',
    'edit-paste': f'<rect x="4" y="4" width="15" height="17" rx="1.5" fill="#c8915a" stroke="#6b4423" stroke-width="1"/>'
                  f'<rect x="8.5" y="2.5" width="6" height="3.5" rx="1" fill="url(#steel)" {S}/>'
                  f'<path d="M9.5 9h8l3 3v10h-11z" fill="url(#paper)" {S}/>',
    'edit-undo': f'<path d="M9 5L3.5 10 9 15v-3.2c4.3-.3 7.6 1 9.6 4.7.3-5.7-3.4-9.4-9.6-9.4z" fill="url(#blue)" stroke="#173f73" stroke-width="1" stroke-linejoin="round"/>',
    'edit-redo': f'<path d="M15 5l5.5 5-5.5 5v-3.2c-4.3-.3-7.6 1-9.6 4.7-.3-5.7 3.4-9.4 9.6-9.4z" fill="url(#blue)" stroke="#173f73" stroke-width="1" stroke-linejoin="round"/>',
    'snap': f'<path d="M5 4h4v8a3 3 0 0 0 6 0V4h4v8a7 7 0 0 1-14 0z" fill="#d9342b" stroke="#7a1712" stroke-width="1" stroke-linejoin="round"/>'
            f'<path d="M5 4h4v3H5zM15 4h4v3h-4z" fill="#d7dde3" stroke="#7a1712" stroke-width="1"/>',
    'zoom-in': f'<circle cx="10" cy="10" r="6.2" fill="url(#glass)" stroke="{INK}" stroke-width="1.6"/>'
               f'<path d="M14.6 14.6l5.8 5.8" stroke="#5b4636" stroke-width="3.2" stroke-linecap="round"/>'
               f'<path d="M7 10h6M10 7v6" stroke="#1f5fae" stroke-width="1.6"/>',
    'zoom-out': f'<circle cx="10" cy="10" r="6.2" fill="url(#glass)" stroke="{INK}" stroke-width="1.6"/>'
                f'<path d="M14.6 14.6l5.8 5.8" stroke="#5b4636" stroke-width="3.2" stroke-linecap="round"/>'
                f'<path d="M7 10h6" stroke="#1f5fae" stroke-width="1.6"/>',
    'zoom-page': f'<path d="M3.5 2.5h10v14h-10z" fill="url(#paper)" {S}/>'
                 f'<circle cx="14" cy="13" r="4.5" fill="url(#glass)" stroke="{INK}" stroke-width="1.4"/>'
                 f'<path d="M17.3 16.3l4 4" stroke="#5b4636" stroke-width="2.6" stroke-linecap="round"/>',
    # --- Eigenschaftsleiste -----------------------------------------------------------------
    'position': f'<rect x="4.5" y="4.5" width="15" height="15" fill="#ffffff" {S}/>'
                f'<path d="M12 4.5v15M4.5 12h15" stroke="#9fb3c8" stroke-width="0.8"/>'
                f'<circle cx="12" cy="12" r="2.2" fill="#d9342b" stroke="#7a1712" stroke-width="0.6"/>',
    'width': f'<path d="M3.5 6v12M20.5 6v12" stroke="{INK}" stroke-width="1.2"/>'
             f'<path d="M5 12h14M8 9l-3 3 3 3M16 9l3 3-3 3" fill="none" stroke="#1f5fae" stroke-width="1.4" stroke-linejoin="round"/>',
    'height': f'<path d="M6 3.5h12M6 20.5h12" stroke="{INK}" stroke-width="1.2"/>'
              f'<path d="M12 5v14M9 8l3-3 3 3M9 16l3 3 3-3" fill="none" stroke="#1f5fae" stroke-width="1.4" stroke-linejoin="round"/>',
    'lock': f'<path d="M8 11V8a4 4 0 0 1 8 0v3" fill="none" stroke="{INK}" stroke-width="1.8"/>'
            f'<rect x="5.5" y="10.5" width="13" height="10" rx="1.5" fill="url(#gold)" stroke="#7a5a08" stroke-width="1"/>'
            f'<circle cx="12" cy="15" r="1.4" fill="#5a4206"/>',
    'lock-open': f'<path d="M8 11V8a4 4 0 0 1 7.8-1.2" fill="none" stroke="{INK}" stroke-width="1.8"/>'
                 f'<rect x="5.5" y="10.5" width="13" height="10" rx="1.5" fill="url(#gold)" stroke="#7a5a08" stroke-width="1"/>'
                 f'<circle cx="12" cy="15" r="1.4" fill="#5a4206"/>',
    'rotate': f'<path d="M18.5 12a6.5 6.5 0 1 1-2-4.7" fill="none" stroke="#1f5fae" stroke-width="1.9"/>'
              f'<path d="M14.5 3.5l3.6 3.3-4.6 1.3z" fill="#1f5fae" stroke="#1f5fae" stroke-width="1" stroke-linejoin="round"/>',
    # Eine Winkelform und ihr Spiegelbild an der gepunkteten Achse
    'mirror-h': f'<path d="M12 3v18" stroke="{INK}" stroke-width="1.2" stroke-dasharray="1.6 1.4"/>'
                f'<path d="M4.5 6.5h3v7h2.5v4.5H4.5z" fill="url(#paper)" stroke="{INK}" stroke-width="1.1" stroke-linejoin="miter"/>'
                f'<path d="M19.5 6.5h-3v7h-2.5v4.5h5.5z" fill="url(#paper)" stroke="#8c8c8c" stroke-width="1.1" stroke-linejoin="miter"/>',
    'mirror-v': f'<path d="M3 12h18" stroke="{INK}" stroke-width="1.2" stroke-dasharray="1.6 1.4"/>'
                f'<path d="M6.5 4.5h11v3h-6v2.5h-5z" fill="url(#paper)" stroke="{INK}" stroke-width="1.1" stroke-linejoin="miter"/>'
                f'<path d="M6.5 19.5h11v-3h-6v-2.5h-5z" fill="url(#paper)" stroke="#8c8c8c" stroke-width="1.1" stroke-linejoin="miter"/>',
    'outline-width': f'<path d="M3.5 6h17" stroke="{INK}" stroke-width="0.8"/><path d="M3.5 11h17" stroke="{INK}" stroke-width="1.8"/>'
                     f'<path d="M3.5 17h17" stroke="{INK}" stroke-width="3.4"/>',
    'corner-round': f'<path d="M4.5 20V12a7.5 7.5 0 0 1 7.5-7.5h8" fill="none" stroke="{INK}" stroke-width="1.8"/>',
    'corner-scallop': f'<path d="M4.5 20v-7.5a7.5 7.5 0 0 0 7.5-8h8" fill="none" stroke="{INK}" stroke-width="1.8"/>',
    'corner-chamfer': f'<path d="M4.5 20v-7.5l8-8h7.5" fill="none" stroke="{INK}" stroke-width="1.8"/>',
    'radius-tl': f'<path d="M5 19v-7a7 7 0 0 1 7-7h7" fill="none" stroke="{INK}" stroke-width="1.6"/><circle cx="5" cy="5" r="1.6" fill="#1f5fae"/>',
    'radius-tr': f'<path d="M19 19v-7a7 7 0 0 0-7-7H5" fill="none" stroke="{INK}" stroke-width="1.6"/><circle cx="19" cy="5" r="1.6" fill="#1f5fae"/>',
    'radius-bl': f'<path d="M5 5v7a7 7 0 0 0 7 7h7" fill="none" stroke="{INK}" stroke-width="1.6"/><circle cx="5" cy="19" r="1.6" fill="#1f5fae"/>',
    'radius-br': f'<path d="M19 5v7a7 7 0 0 1-7 7H5" fill="none" stroke="{INK}" stroke-width="1.6"/><circle cx="19" cy="19" r="1.6" fill="#1f5fae"/>',
    'page-portrait': f'<path d="M6.5 2.5h8l3 3v16h-11z" fill="url(#paper)" {S}/>',
    'page-landscape': f'<path d="M2.5 6.5h16l3 3v8h-19z" fill="url(#paper)" {S}/>',
    'tool-transparency': f'<rect x="4.5" y="4.5" width="15" height="15" fill="#ffffff" {S}/>'
                         f'<path d="M4.5 4.5h5v5h-5zM14.5 4.5h5v5h-5zM9.5 9.5h5v5h-5zM4.5 14.5h5v5h-5zM14.5 14.5h5v5h-5z" fill="#b9b9b9"/>'
                         f'<rect x="4.5" y="4.5" width="15" height="15" fill="url(#bw)" opacity="0.65" {S}/>',
    'tool-blur': '<circle cx="12" cy="12" r="9" fill="#1f5fae" opacity="0.15"/>'
                 '<circle cx="12" cy="12" r="7" fill="#1f5fae" opacity="0.3"/>'
                 '<circle cx="12" cy="12" r="5" fill="#1f5fae" opacity="0.55"/>'
                 '<circle cx="12" cy="12" r="3" fill="#1f5fae"/>',
    'shape-weld': '<path d="M3 8h6V4h12v10h-9.75L8.5 20z" fill="#1f5fae" stroke="#0d3a73" stroke-width="1"/>',
    'shape-trim': '<path d="M9 4h12v10h-9.75L14 8H9z" fill="#1f5fae" stroke="#0d3a73" stroke-width="1"/>'
                  '<path d="M3 8h11l-5.5 12z" fill="none" stroke="#333" stroke-width="1"/>',
    'shape-intersect': '<path d="M3 8h11l-5.5 12z" fill="none" stroke="#999" stroke-width="1"/><path d="M9 4h12v10H9z" fill="none" stroke="#999" stroke-width="1"/>'
                       '<path d="M9 8h5l-2.75 6H9z" fill="#1f5fae" stroke="#0d3a73" stroke-width="1"/>',
    'shape-simplify': '<path d="M9 4h12v10h-9.75L14 8H9z" fill="#9fb8dc" stroke="#0d3a73" stroke-width="1"/>'
                      '<path d="M3 8h11l-5.5 12z" fill="#1f5fae" stroke="#0d3a73" stroke-width="1"/>',
    'shape-front-minus-back': '<path d="M9 4h12v10H9z" fill="none" stroke="#999" stroke-width="1" stroke-dasharray="2 1.5"/>'
                              '<path d="M3 8h6v6h2.25L8.5 20z" fill="#1f5fae" stroke="#0d3a73" stroke-width="1"/>',
    'shape-back-minus-front': '<path d="M3 8h11l-5.5 12z" fill="none" stroke="#999" stroke-width="1" stroke-dasharray="2 1.5"/>'
                              '<path d="M9 4h12v10h-9.75L14 8H9z" fill="#1f5fae" stroke="#0d3a73" stroke-width="1"/>',
    'shape-boundary': '<path d="M3 8h11l-5.5 12z" fill="#d6dde8"/><path d="M9 4h12v10H9z" fill="#d6dde8"/>'
                      '<path d="M3 8h6V4h12v10h-9.75L8.5 20z" fill="none" stroke="#1a1a1a" stroke-width="1.4"/>',
    'tool-pan': f'<path d="M8 11V5.5a1.5 1.5 0 0 1 3 0V10V4a1.5 1.5 0 0 1 3 0v6V5.5a1.5 1.5 0 0 1 3 0V12V8.5a1.5 1.5 0 0 1 3 0V14c0 4-2.5 7-6.5 7c-3 0-4.5-1.5-6-4l-2.5-4a1.5 1.5 0 0 1 2.5-1.6L8 13z" fill="#ffffff" {S}/>',
    'graph-columns': f'<rect x="3.5" y="5.5" width="17" height="13" fill="#ffffff" {S}/><path d="M9.5 5.5v13M14.5 5.5v13" {S}/>',
    'graph-rows': f'<rect x="3.5" y="5.5" width="17" height="13" fill="#ffffff" {S}/><path d="M3.5 9.8h17M3.5 14.2h17" {S}/>',
    'spiral-revolutions': f'<path d="M12 12a1.5 1.5 0 0 1 3 0a3 3 0 0 1-6 0a4.5 4.5 0 0 1 9 0a6 6 0 0 1-12 0a7.5 7.5 0 0 1 15 0" fill="none" {S}/>',
    'spiral-symmetric': f'<path d="M12 12a1.5 1.5 0 0 1 3 0a3 3 0 0 1-6 0a4.5 4.5 0 0 1 9 0a6 6 0 0 1-12 0a7.5 7.5 0 0 1 15 0" fill="none" stroke="#1f5fae" stroke-width="1.3"/>',
    'spiral-log': f'<path d="M12 12a0.6 0.6 0 0 1 1.2 0a1.5 1.5 0 0 1-3 0a3.2 3.2 0 0 1 6.4 0a6 6 0 0 1-12 0a8.5 8.5 0 0 1 17 0" fill="none" stroke="#1f5fae" stroke-width="1.3"/>',
    'spiral-expansion': f'<path d="M4 12h16M16 8l4 4l-4 4M8 8l-4 4l4 4" fill="none" {S}/>',
    'tool-dimension': f'<path d="M4 18L18 4" {S}/><path d="M7 21L21 7" stroke="#1f5fae" stroke-width="1.2"/><path d="M21 7l-1 3.5l-2.5-2.5zM7 21l1-3.5l2.5 2.5z" fill="#1f5fae"/><path d="M4 18l3 3M18 4l3 3" {S}/>',
    'tool-dimension-hv': f'<path d="M4 9v10M20 9v10" {S}/><path d="M4 14h16" stroke="#1f5fae" stroke-width="1.2"/><path d="M4 14l3-1.6v3.2zM20 14l-3-1.6v3.2z" fill="#1f5fae"/><path d="M8 5h8" {S}/>',
    'tool-dimension-angle': f'<path d="M4 20L20 20M4 20L16 6" {S}/><path d="M13 20a9 9 0 0 0-2.8-6.6" fill="none" stroke="#1f5fae" stroke-width="1.2"/>',
    'tool-connector': f'<rect x="2.5" y="3.5" width="7" height="6" fill="#ffffff" {S}/><rect x="14.5" y="14.5" width="7" height="6" fill="#ffffff" {S}/><path d="M9.5 9.5L14.5 14.5" stroke="#1f5fae" stroke-width="1.4"/>',
    'tool-connector-angle': f'<rect x="2.5" y="3.5" width="7" height="6" fill="#ffffff" {S}/><rect x="14.5" y="14.5" width="7" height="6" fill="#ffffff" {S}/><path d="M9.5 6.5H18V14.5" fill="none" stroke="#1f5fae" stroke-width="1.4"/>',
    'tool-connector-round': f'<rect x="2.5" y="3.5" width="7" height="6" fill="#ffffff" {S}/><rect x="14.5" y="14.5" width="7" height="6" fill="#ffffff" {S}/><path d="M9.5 6.5H15a3 3 0 0 1 3 3V14.5" fill="none" stroke="#1f5fae" stroke-width="1.4"/>',
    'connector-clearance': f'<rect x="3.5" y="8.5" width="6" height="7" fill="#ffffff" {S}/><path d="M9.5 12H20" stroke="#1f5fae" stroke-width="1.4"/><path d="M11 9v6M15 9v6" {S}/>',
    'tool-table': f'<rect x="3.5" y="5.5" width="17" height="13" fill="#fff" {S}/><path d="M3.5 9.8h17M3.5 14.2h17M9.2 5.5v13M14.8 5.5v13" fill="none" {S}/><rect x="3.5" y="5.5" width="17" height="4.3" fill="#9cc3ef" opacity="0.6"/>',
    'tool-mesh': f'<path d="M4 5c5-1 11 1 16 0v14c-5 1-11-1-16 0z" fill="#e8f0fb" {S}/><path d="M4 12c5-1 11 1 16 0M9.5 5.2c-1 4.5 1 9 0 13.6M14.5 5.2c1 4.5-1 9 0 13.6" fill="none" {S}/><rect x="8" y="10.4" width="3" height="3" fill="#1f5fae"/>',
    'script-run': f'<path d="M7 4.5l12 7.5-12 7.5z" fill="#3aa04a" {S}/>',
    'obj-align-left': '<path d="M4 3v18" stroke="#d22" stroke-width="1.4"/><rect x="5" y="5" width="14" height="5" fill="#9cc3ef" stroke="#1f5fae" stroke-width="1"/><rect x="5" y="13" width="8" height="5" fill="#d9d9d9" stroke="#555" stroke-width="1"/>',
    'obj-align-right': '<path d="M20 3v18" stroke="#d22" stroke-width="1.4"/><rect x="5" y="5" width="14" height="5" fill="#9cc3ef" stroke="#1f5fae" stroke-width="1"/><rect x="11" y="13" width="8" height="5" fill="#d9d9d9" stroke="#555" stroke-width="1"/>',
    'obj-align-hcenter': '<path d="M12 3v18" stroke="#d22" stroke-width="1.4"/><rect x="4.5" y="5" width="15" height="5" fill="#9cc3ef" stroke="#1f5fae" stroke-width="1"/><rect x="8" y="13" width="8" height="5" fill="#d9d9d9" stroke="#555" stroke-width="1"/>',
    'obj-align-top': '<path d="M3 4h18" stroke="#d22" stroke-width="1.4"/><rect x="5" y="5" width="5" height="14" fill="#9cc3ef" stroke="#1f5fae" stroke-width="1"/><rect x="13" y="5" width="5" height="8" fill="#d9d9d9" stroke="#555" stroke-width="1"/>',
    'obj-align-bottom': '<path d="M3 20h18" stroke="#d22" stroke-width="1.4"/><rect x="5" y="5" width="5" height="14" fill="#9cc3ef" stroke="#1f5fae" stroke-width="1"/><rect x="13" y="11" width="5" height="8" fill="#d9d9d9" stroke="#555" stroke-width="1"/>',
    'obj-align-vcenter': '<path d="M3 12h18" stroke="#d22" stroke-width="1.4"/><rect x="5" y="4.5" width="5" height="15" fill="#9cc3ef" stroke="#1f5fae" stroke-width="1"/><rect x="13" y="8" width="5" height="8" fill="#d9d9d9" stroke="#555" stroke-width="1"/>',
    'obj-align-page': '<rect x="3.5" y="3.5" width="17" height="17" fill="#fff" stroke="#555" stroke-width="1"/><rect x="8" y="9" width="8" height="6" fill="#9cc3ef" stroke="#1f5fae" stroke-width="1"/><path d="M12 4v16M4 12h16" stroke="#d22" stroke-width="0.8" stroke-dasharray="1.5 1"/>',
    'obj-align-page-h': '<rect x="3.5" y="3.5" width="17" height="17" fill="#fff" stroke="#555" stroke-width="1"/><rect x="8" y="6" width="8" height="6" fill="#9cc3ef" stroke="#1f5fae" stroke-width="1"/><path d="M12 4v16" stroke="#d22" stroke-width="0.8" stroke-dasharray="1.5 1"/>',
    'obj-align-page-v': '<rect x="3.5" y="3.5" width="17" height="17" fill="#fff" stroke="#555" stroke-width="1"/><rect x="5" y="9" width="8" height="6" fill="#9cc3ef" stroke="#1f5fae" stroke-width="1"/><path d="M4 12h16" stroke="#d22" stroke-width="0.8" stroke-dasharray="1.5 1"/>',
    'align-dialog': '<path d="M4 3v18" stroke="#d22" stroke-width="1.4"/><rect x="5" y="5" width="12" height="5" fill="#9cc3ef" stroke="#1f5fae" stroke-width="1"/><rect x="5" y="13" width="7" height="5" fill="#d9d9d9" stroke="#555" stroke-width="1"/><path d="M15 15h6M18 12v6" stroke="#1f5fae" stroke-width="1.4"/>',
    'obj-dist-left': '<path d="M4 3v18M13 3v18" stroke="#d22" stroke-width="1.4"/><rect x="4" y="6" width="6" height="5" fill="#9cc3ef" stroke="#1f5fae" stroke-width="1"/><rect x="13" y="13" width="7" height="5" fill="#d9d9d9" stroke="#555" stroke-width="1"/>',
    'obj-dist-hcenter': '<path d="M7 3v18M16.5 3v18" stroke="#d22" stroke-width="1.4"/><rect x="4" y="6" width="6" height="5" fill="#9cc3ef" stroke="#1f5fae" stroke-width="1"/><rect x="13" y="13" width="7" height="5" fill="#d9d9d9" stroke="#555" stroke-width="1"/>',
    'obj-dist-hspace': '<rect x="3" y="7" width="5" height="10" fill="#9cc3ef" stroke="#1f5fae" stroke-width="1"/><rect x="16" y="7" width="5" height="10" fill="#d9d9d9" stroke="#555" stroke-width="1"/><path d="M9 12h6M9 10v4M15 10v4" stroke="#d22" stroke-width="1.2"/>',
    'obj-dist-right': '<path d="M10 3v18M20 3v18" stroke="#d22" stroke-width="1.4"/><rect x="4" y="6" width="6" height="5" fill="#9cc3ef" stroke="#1f5fae" stroke-width="1"/><rect x="13" y="13" width="7" height="5" fill="#d9d9d9" stroke="#555" stroke-width="1"/>',
    'obj-dist-top': '<path d="M3 4h18M3 13h18" stroke="#d22" stroke-width="1.4"/><rect x="6" y="4" width="5" height="6" fill="#9cc3ef" stroke="#1f5fae" stroke-width="1"/><rect x="13" y="13" width="5" height="7" fill="#d9d9d9" stroke="#555" stroke-width="1"/>',
    'obj-dist-vcenter': '<path d="M3 7h18M3 16.5h18" stroke="#d22" stroke-width="1.4"/><rect x="6" y="4" width="5" height="6" fill="#9cc3ef" stroke="#1f5fae" stroke-width="1"/><rect x="13" y="13" width="5" height="7" fill="#d9d9d9" stroke="#555" stroke-width="1"/>',
    'obj-dist-vspace': '<rect x="7" y="3" width="10" height="5" fill="#9cc3ef" stroke="#1f5fae" stroke-width="1"/><rect x="7" y="16" width="10" height="5" fill="#d9d9d9" stroke="#555" stroke-width="1"/><path d="M12 9v6M10 9h4M10 15h4" stroke="#d22" stroke-width="1.2"/>',
    'obj-dist-bottom': '<path d="M3 10h18M3 20h18" stroke="#d22" stroke-width="1.4"/><rect x="6" y="4" width="5" height="6" fill="#9cc3ef" stroke="#1f5fae" stroke-width="1"/><rect x="13" y="13" width="5" height="7" fill="#d9d9d9" stroke="#555" stroke-width="1"/>',
    'xf-position': '<rect x="3" y="11" width="8" height="8" fill="none" stroke="#888" stroke-dasharray="2 1"/><rect x="12" y="4" width="8" height="8" fill="#9cc3ef" stroke="#1f5fae" stroke-width="1"/><path d="M8 14l7-6M15 8h-4M15 8v4" fill="none" stroke="#d22" stroke-width="1.3"/>',
    'xf-rotate': '<rect x="7" y="7" width="10" height="10" fill="#9cc3ef" stroke="#1f5fae" stroke-width="1" transform="rotate(20 12 12)"/><path d="M4 9a9 9 0 0 1 9-6M13 3l-2.5-1.5M13 3l-1.5 2.5" fill="none" stroke="#d22" stroke-width="1.3"/>',
    'xf-scale': '<rect x="3" y="12" width="6" height="6" fill="none" stroke="#888"/><rect x="9" y="4" width="11" height="11" fill="#9cc3ef" stroke="#1f5fae" stroke-width="1"/><path d="M7 14l5-5M12 9h-3M12 9v3" fill="none" stroke="#d22" stroke-width="1.3"/>',
    'xf-size': '<rect x="5" y="7" width="14" height="10" fill="#9cc3ef" stroke="#1f5fae" stroke-width="1"/><path d="M3 4h18M3 2.5v3M21 2.5v3M2 20h0" fill="none" stroke="#d22" stroke-width="1.2"/>',
    'xf-skew': '<path d="M8 6h12l-4 12H4z" fill="#9cc3ef" stroke="#1f5fae" stroke-width="1"/><path d="M3 21h18" stroke="#d22" stroke-width="1.2"/>',
    'xf-clear': '<rect x="6" y="6" width="12" height="12" fill="#9cc3ef" stroke="#1f5fae" stroke-width="1" transform="rotate(25 12 12)"/><path d="M5 19L19 5" stroke="#d22" stroke-width="1.6"/>',
    'outline-to-object': '<rect x="4" y="4" width="16" height="16" fill="none" stroke="#1f5fae" stroke-width="3"/><rect x="5.5" y="5.5" width="13" height="13" fill="none" stroke="#fff" stroke-width="0.8"/>',
    'join-curves': '<path d="M3 17c3-8 6-8 9-5" fill="none" stroke="#1f5fae" stroke-width="1.5"/><path d="M12 12c3 3 6 3 9-5" fill="none" stroke="#1f5fae" stroke-width="1.5"/><circle cx="12" cy="12" r="2" fill="#d22"/>',
    'unlock-all': '<rect x="4" y="11" width="10" height="8" rx="1" fill="#e2c35b" stroke="#8a6d1a"/><path d="M6 11V8a3 3 0 0 1 6 0" fill="none" stroke="#8a6d1a" stroke-width="1.4"/><rect x="12" y="6" width="8" height="6" rx="1" fill="#e2c35b" stroke="#8a6d1a"/>',
    'edit-repeat': '<path d="M5 12a7 7 0 1 0 2-5" fill="none" stroke="#1f5fae" stroke-width="1.8"/><path d="M4 3v5h5" fill="none" stroke="#1f5fae" stroke-width="1.8"/>',
    'edit-clone': '<rect x="3" y="3" width="11" height="11" fill="#9cc3ef" stroke="#1f5fae" stroke-width="1"/><rect x="10" y="10" width="11" height="11" fill="#fff" stroke="#1f5fae" stroke-dasharray="2 1.2"/><path d="M8 8l7 7" stroke="#d22" stroke-width="1.2"/>',
    'symbol': '<circle cx="12" cy="12" r="8" fill="#9cc3ef" stroke="#1f5fae" stroke-width="1"/><path d="M8 12l3 3 5-6" fill="none" stroke="#fff" stroke-width="2"/>',
    'object-properties': '<rect x="4" y="3" width="16" height="18" rx="1" fill="#fff" stroke="#555"/><path d="M7 8h10M7 12h10M7 16h6" stroke="#1f5fae" stroke-width="1.4"/>',
    'tool-pipette': '<path d="M15 4l5 5-2 2-1-1-8 8H6v-3l8-8-1-1z" fill="#cfe0f5" stroke="#1f5fae" stroke-width="1.1"/><path d="M4 20l2-2" stroke="#1f5fae" stroke-width="1.6"/><circle cx="4" cy="20" r="1.2" fill="#d22"/>',
    'tool-attr-pipette': '<path d="M15 4l5 5-2 2-1-1-8 8H6v-3l8-8-1-1z" fill="#cfe0f5" stroke="#1f5fae" stroke-width="1.1"/><rect x="2" y="16" width="6" height="6" fill="#e2740c" stroke="#7a3d00" stroke-width="0.8"/>',
    'fill-bucket': '<path d="M5 11l7-7 7 7-7 7z" fill="#cfe0f5" stroke="#1f5fae" stroke-width="1.2"/><path d="M19 13c0 2 2 3 2 5a2 2 0 0 1-4 0c0-2 2-3 2-5z" fill="#1f5fae"/>',
    'attr-properties': '<rect x="4" y="4" width="16" height="16" fill="#9cc3ef" stroke="#1f5fae" stroke-width="2"/>',
    'attr-transform': '<rect x="7" y="7" width="10" height="10" fill="none" stroke="#1f5fae" stroke-width="1.4" transform="rotate(25 12 12)"/><path d="M3 21l5-5M3 21h4M3 21v-4" stroke="#d22" stroke-width="1.3" fill="none"/>',
    'attr-effects': '<circle cx="12" cy="12" r="7" fill="#9cc3ef" opacity="0.5"/><circle cx="12" cy="12" r="4" fill="#1f5fae" opacity="0.8"/>',
    'fx-bevel': '<path d="M4 4h16v16H4z" fill="#9cc3ef" stroke="#1f5fae"/><path d="M4 4l4 4h8l4-4z" fill="#ffffff" opacity="0.8"/><path d="M4 20l4-4h8l4 4z" fill="#1f5fae" opacity="0.6"/><rect x="8" y="8" width="8" height="8" fill="#9cc3ef"/>',
    'fx-lens': '<circle cx="10" cy="10" r="6" fill="#cfe7ff" stroke="#1f5fae" stroke-width="1.5"/><path d="M14.5 14.5l6 6" stroke="#555" stroke-width="2.2"/>',
    'fx-perspective': '<path d="M6 5h12l3 14H3z" fill="#9cc3ef" stroke="#1f5fae"/><path d="M12 5v14M4.5 12h15" stroke="#1f5fae" stroke-width="0.7" stroke-dasharray="1.5 1"/>',
    'fx-powerclip': '<circle cx="12" cy="12" r="8" fill="none" stroke="#1f5fae" stroke-width="1.5"/><path d="M5 15l4-5 3 3 3-4 4 6" fill="none" stroke="#e2740c" stroke-width="1.4"/>',
    'fx-clear': '<circle cx="12" cy="12" r="7" fill="#9cc3ef" opacity="0.6"/><path d="M6 6l12 12M18 6L6 18" stroke="#d22" stroke-width="1.8"/>',
    'view-fullscreen': '<rect x="3" y="5" width="18" height="14" fill="#333" stroke="#111"/><rect x="7" y="7" width="10" height="10" fill="#fff"/>',
    'table-merge': '<rect x="3.5" y="5.5" width="17" height="13" fill="#fff" stroke="#1f5fae"/><path d="M3.5 12h17M12 12v6.5" stroke="#1f5fae"/><rect x="4" y="6" width="16" height="5.5" fill="#9cc3ef"/>',
    'to-bitmap': '<path d="M3 17l5-9 4 6 3-3 6 6z" fill="#9cc3ef" stroke="#1f5fae"/><path d="M14 4h7v7h-7z" fill="none" stroke="#555"/><path d="M14 4h3.5v3.5H14zM17.5 7.5H21V11h-3.5z" fill="#555"/>',
    'tool-shadow': '<rect x="7" y="7" width="12" height="12" fill="#555" opacity="0.45"/><rect x="4" y="4" width="12" height="12" fill="#9cc3ef" stroke="#1f5fae"/>',
    'shadow-angle': '<path d="M4 20h16M4 20L15 6" fill="none" stroke="#1f5fae" stroke-width="1.5"/><path d="M10 20a6 6 0 0 0-2-4" fill="none" stroke="#d22"/>',
    'shadow-stretch': '<rect x="6" y="12" width="12" height="8" fill="#9cc3ef" stroke="#1f5fae"/><path d="M12 11V3M9 6l3-3 3 3" fill="none" stroke="#d22" stroke-width="1.3"/>',
    'shadow-fade': '<defs><linearGradient id="fd" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#333"/><stop offset="1" stop-color="#333" stop-opacity="0"/></linearGradient></defs><rect x="3" y="8" width="18" height="8" fill="url(#fd)"/>',
    'shadow-opacity': '<rect x="3" y="3" width="18" height="18" fill="#fff" stroke="#888"/><path d="M3 21L21 3v18z" fill="#555" opacity="0.6"/>',
    'shadow-feather': '<circle cx="12" cy="12" r="8" fill="#555" opacity="0.25"/><circle cx="12" cy="12" r="5" fill="#555" opacity="0.5"/>',
    'shadow-color': '<rect x="4" y="4" width="16" height="16" fill="#333" stroke="#000"/><path d="M4 4l16 16" stroke="#fff" stroke-width="0.8"/>',
    'tool-blend': '<circle cx="6" cy="17" r="3.5" fill="#1f5fae"/><circle cx="10" cy="13.5" r="3.5" fill="#4a7fc0"/><circle cx="14" cy="10" r="3.5" fill="#7aa5d8"/><circle cx="18" cy="6.5" r="3.5" fill="#bcd5f0" stroke="#1f5fae"/>',
    'tool-contour': '<rect x="3" y="3" width="18" height="18" rx="3" fill="#dfeaf7" stroke="#1f5fae"/><rect x="6" y="6" width="12" height="12" rx="2" fill="#9cc3ef" stroke="#1f5fae"/><rect x="9" y="9" width="6" height="6" fill="#1f5fae"/>',
    'tool-distort': '<path d="M4 12c2-6 4 6 6 0s4 6 6 0 4 6 4 0" fill="none" stroke="#1f5fae" stroke-width="1.6"/>',
    'tool-envelope': '<path d="M4 6c5 3 11 3 16 0v12c-5-3-11-3-16 0z" fill="#9cc3ef" stroke="#1f5fae"/><rect x="2.5" y="4.5" width="3" height="3" fill="#fff" stroke="#000" stroke-width="0.8"/><rect x="18.5" y="16.5" width="3" height="3" fill="#fff" stroke="#000" stroke-width="0.8"/>',
    'tool-extrude': '<path d="M4 9h11v11H4z" fill="#9cc3ef" stroke="#1f5fae"/><path d="M4 9l5-5h11l-5 5zM15 9l5-5v11l-5 5z" fill="#5f8fca" stroke="#1f5fae"/>',
    'contour-center': '<rect x="3" y="3" width="18" height="18" fill="none" stroke="#1f5fae"/><rect x="7" y="7" width="10" height="10" fill="none" stroke="#1f5fae"/><circle cx="12" cy="12" r="1.5" fill="#d22"/>',
    'contour-inside': '<rect x="3" y="3" width="18" height="18" fill="#9cc3ef" stroke="#1f5fae"/><rect x="7" y="7" width="10" height="10" fill="none" stroke="#1f5fae"/>',
    'contour-outside': '<rect x="3" y="3" width="18" height="18" fill="none" stroke="#1f5fae" stroke-dasharray="2 1"/><rect x="7" y="7" width="10" height="10" fill="#9cc3ef" stroke="#1f5fae"/>',
    'contour-offset': '<path d="M4 4v16M12 4v16M8 12H20M17 9l3 3-3 3" fill="none" stroke="#1f5fae" stroke-width="1.3"/>',
    'blend-steps': '<rect x="3" y="14" width="4" height="6" fill="#1f5fae"/><rect x="10" y="10" width="4" height="10" fill="#4a7fc0"/><rect x="17" y="6" width="4" height="14" fill="#7aa5d8"/>',
    'extrude-depth': '<path d="M5 15h9v5H5z" fill="#9cc3ef" stroke="#1f5fae"/><path d="M5 15l5-5h9l-5 5zM14 15l5-5v5l-5 5z" fill="#5f8fca"/>',
    'distort-pushpull': '<path d="M12 3l3 6 6 3-6 3-3 6-3-6-6-3 6-3z" fill="#9cc3ef" stroke="#1f5fae"/>',
    'distort-zipper': '<path d="M3 12l2-4 2 8 2-8 2 8 2-8 2 8 2-8 2 8 2-4" fill="none" stroke="#1f5fae" stroke-width="1.3"/>',
    'distort-twister': '<path d="M12 12m-1 0a1 1 0 1 1 2 0a3 3 0 1 1-6 0a5 5 0 1 1 10 0a7 7 0 1 1-14 0" fill="none" stroke="#1f5fae" stroke-width="1.3"/>',
    'outline-pen': '<path d="M4 20L15 9l3 3L7 23z" fill="#ffd967" stroke="#3a3a3a"/><path d="M15 9l3-3 3 3-3 3z" fill="#3a3a3a"/><path d="M3 4h12" stroke="#1f5fae" stroke-width="2.5" stroke-linecap="round"/>',
    'tool-smear': '<path d="M4 18c3-1 5-6 9-7s6 2 8-1" fill="none" stroke="#1f5fae" stroke-width="3" stroke-linecap="round"/><circle cx="8" cy="8" r="4" fill="none" stroke="#3a3a3a" stroke-dasharray="1.5 1"/>',
    'tool-smudge': '<path d="M3 6h9c3 0 3 4 6 4s3 4 3 9H3z" fill="#9cc3ef" stroke="#1f5fae"/><path d="M16 4l5 5" stroke="#3a3a3a" stroke-width="1.5"/>',
    'tool-roughen': '<path d="M3 16l2-3 2 3 2-3 2 3 2-3 2 3 2-3 2 3 2-3v6H3z" fill="#9cc3ef" stroke="#1f5fae"/>',
    'tool-twirl': '<path d="M12 12m-1 0a1 1 0 1 1 2 0a3 3 0 1 1-6 0a5 5 0 1 1 10 0a7 7 0 1 1-14 0" fill="none" stroke="#e2740c" stroke-width="1.5"/>',
    'tool-attract': '<circle cx="12" cy="12" r="8" fill="none" stroke="#3a3a3a" stroke-dasharray="1.5 1"/><path d="M4 12h5M15 12h5M12 4v5M12 15v5" stroke="#1f5fae" stroke-width="1.5"/><path d="M9 12l-2-2M9 12l-2 2M15 12l2-2M15 12l2 2" stroke="#1f5fae"/>',
    'tool-repel': '<circle cx="12" cy="12" r="8" fill="none" stroke="#3a3a3a" stroke-dasharray="1.5 1"/><path d="M8 12H3M16 12h5M12 8V3M12 16v5" stroke="#d22" stroke-width="1.5"/><path d="M3 12l2-2M3 12l2 2M21 12l-2-2M21 12l-2 2" stroke="#d22"/>',
    'tool-free-transform': '<rect x="5" y="5" width="10" height="10" fill="#9cc3ef" stroke="#1f5fae"/><rect x="9" y="9" width="10" height="10" fill="none" stroke="#3a3a3a" stroke-dasharray="2 1" transform="rotate(20 14 14)"/>',
    'ft-rotate': '<path d="M18 12a6 6 0 1 1-2-4.5" fill="none" stroke="#1f5fae" stroke-width="1.6"/><path d="M17 3v5h-5" fill="none" stroke="#1f5fae" stroke-width="1.6"/>',
    'ft-mirror': '<path d="M12 3v18" stroke="#3a3a3a" stroke-dasharray="2 1"/><path d="M10 6L3 18h7z" fill="#9cc3ef" stroke="#1f5fae"/><path d="M14 6l7 12h-7z" fill="none" stroke="#1f5fae"/>',
    'ft-scale': '<rect x="3.5" y="11.5" width="9" height="9" fill="#9cc3ef" stroke="#1f5fae"/><rect x="3.5" y="3.5" width="17" height="17" fill="none" stroke="#3a3a3a" stroke-dasharray="2 1"/>',
    'ft-skew': '<path d="M7 5h13l-3 14H4z" fill="#9cc3ef" stroke="#1f5fae"/>',
    'tool-smart-fill': '<circle cx="9" cy="12" r="6" fill="none" stroke="#1f5fae"/><circle cx="15" cy="12" r="6" fill="none" stroke="#1f5fae"/><path d="M12 7.2a6 6 0 0 1 0 9.6a6 6 0 0 1 0-9.6z" fill="#e2740c"/>',
    'text-on-path': '<path d="M3 19C6 9 18 9 21 19" fill="none" stroke="#1f5fae" stroke-dasharray="2 1"/><text x="5" y="14" font-family="Arial" font-weight="bold" font-size="8" fill="#3a3a3a" transform="rotate(-20 8 12)">A</text><text x="11" y="10" font-family="Arial" font-weight="bold" font-size="8" fill="#3a3a3a">B</text><text x="16" y="12" font-family="Arial" font-weight="bold" font-size="8" fill="#3a3a3a" transform="rotate(20 18 10)">C</text>',
    'text-path-flip': '<path d="M3 12C8 6 16 6 21 12" fill="none" stroke="#1f5fae"/><path d="M12 13v8M9 18l3 3 3-3" fill="none" stroke="#d22" stroke-width="1.4"/>',
    'text-bullets': '<circle cx="5" cy="7" r="1.8" fill="#3a3a3a"/><circle cx="5" cy="13" r="1.8" fill="#3a3a3a"/><circle cx="5" cy="19" r="1.8" fill="#3a3a3a"/><path d="M9 7h12M9 13h12M9 19h12" stroke="#1f5fae" stroke-width="1.6"/>',
    'text-dropcap': '<text x="2" y="15" font-family="Times New Roman" font-weight="bold" font-size="15" fill="#3a3a3a">A</text><path d="M13 5h9M13 9h9M13 13h9M3 19h19" stroke="#1f5fae" stroke-width="1.5"/>',
    'perfect-basic': '<path d="M12 3c-3-4-10 0-7 6l7 9 7-9c3-6-4-10-7-6z" fill="#9cc3ef" stroke="#1f5fae"/>',
    'perfect-arrow': '<path d="M3 9h10V4l8 8-8 8v-5H3z" fill="#9cc3ef" stroke="#1f5fae"/>',
    'perfect-flowchart': '<path d="M12 2l6 5-6 5-6-5z" fill="#9cc3ef" stroke="#1f5fae"/><rect x="6.5" y="15.5" width="11" height="6" fill="#fff" stroke="#1f5fae"/><path d="M12 12v3.5" stroke="#3a3a3a"/>',
    'perfect-banner': '<path d="M2 9h4v9H2l2-4.5zM22 9h-4v9h4l-2-4.5z" fill="#5f8fca" stroke="#1f5fae"/><rect x="5.5" y="6.5" width="13" height="9" fill="#9cc3ef" stroke="#1f5fae"/>',
    'perfect-callout': '<path d="M3.5 4.5h17v11h-9l-5 5v-5h-3z" fill="#fff" stroke="#1f5fae"/>',
    'fill-pattern': '<rect x="3.5" y="3.5" width="17" height="17" fill="#fff" stroke="#3a3a3a"/><path d="M4 4h4v4H4zM12 4h4v4h-4zM8 8h4v4H8zM16 8h4v4h-4zM4 12h4v4H4zM12 12h4v4h-4zM8 16h4v4H8zM16 16h4v4h-4z" fill="#1f5fae"/>',
    'fill-texture': '<rect x="3.5" y="3.5" width="17" height="17" fill="#c9a26b" stroke="#3a3a3a"/><path d="M4 8c4-2 8 2 16-1M4 13c5-2 9 2 16-1M4 18c4-2 9 1 16-1" fill="none" stroke="#7a5527" stroke-width="1.2"/>',
    'env-straight': '<path d="M3 6L12 9 21 6v12l-9-3-9 3z" fill="#9cc3ef" stroke="#1f5fae"/>',
    'env-single': '<path d="M3 6Q12 12 21 6v12Q12 12 3 18z" fill="#9cc3ef" stroke="#1f5fae"/>',
    'env-double': '<path d="M3 7C7 2 9 12 12 7S17 2 21 7v11C17 13 15 23 12 18S7 13 3 18z" fill="#9cc3ef" stroke="#1f5fae"/>',
    'env-free': '<path d="M3 6C8 3 10 10 21 5c-2 5 2 9 0 13C14 15 9 21 3 18c2-4-2-8 0-12z" fill="#9cc3ef" stroke="#1f5fae"/><circle cx="21" cy="5" r="1.6" fill="#fff" stroke="#000" stroke-width="0.8"/>',
    'env-new': '<rect x="3.5" y="6.5" width="13" height="11" fill="#9cc3ef" stroke="#1f5fae" stroke-dasharray="2 1"/><path d="M18 3v6M15 6h6" stroke="#2e7d32" stroke-width="1.8"/>',
    'env-from': '<path d="M3 9Q10 4 17 9v9Q10 13 3 18z" fill="#9cc3ef" stroke="#1f5fae"/><path d="M16 15l5 5M21 15v5h-5" fill="none" stroke="#d22" stroke-width="1.4"/>',
    'clip-edit': '<rect x="3.5" y="3.5" width="17" height="17" rx="2" fill="none" stroke="#1f5fae" stroke-dasharray="2 1"/><path d="M8 17l1-4 7-7 3 3-7 7z" fill="#ffd967" stroke="#3a3a3a"/>',
    'clip-select': '<rect x="3.5" y="3.5" width="17" height="17" rx="2" fill="none" stroke="#1f5fae"/><path d="M8 7l9 5-4 1 2 4-2 1-2-4-3 3z" fill="#fff" stroke="#3a3a3a"/>',
    'clip-extract': '<rect x="3.5" y="9.5" width="11" height="11" fill="none" stroke="#1f5fae"/><path d="M10 14L20 4M14 4h6v6" fill="none" stroke="#e2740c" stroke-width="1.6"/>',
    'clip-lock': '<rect x="6" y="11" width="12" height="9" rx="1" fill="#ffd967" stroke="#3a3a3a"/><path d="M8.5 11V8a3.5 3.5 0 0 1 7 0v3" fill="none" stroke="#3a3a3a" stroke-width="1.5"/>',
    'clip-fit': '<rect x="3.5" y="3.5" width="17" height="17" fill="none" stroke="#1f5fae"/><rect x="7" y="7" width="10" height="10" fill="#9cc3ef"/><path d="M4 4l3 3M20 4l-3 3M4 20l3-3M20 20l-3-3" stroke="#3a3a3a"/>',
    'clip-finish': '<circle cx="12" cy="12" r="8.5" fill="#43a047" stroke="#2e7d32"/><path d="M7.5 12.5l3 3 6-7" fill="none" stroke="#fff" stroke-width="2"/>',
    'tool-smart': f'<path d="M3 18c3-8 6-11 9-9s2 7 5 6s3-6 4-9" fill="none" {S}/><rect x="13.5" y="3.5" width="7" height="5" fill="none" stroke="#1f5fae" stroke-width="1.2"/><circle cx="7" cy="7" r="3.2" fill="none" stroke="#1f5fae" stroke-width="1.2"/>',
    'guide-horizontal': f'<path d="M2.5 12h19" stroke="#2b5cd6" stroke-width="1.6" stroke-dasharray="3 2"/>'
                        f'<circle cx="12" cy="12" r="2.4" fill="#ffffff" stroke="{INK}" stroke-width="1"/>',
    'guide-vertical': f'<path d="M12 2.5v19" stroke="#2b5cd6" stroke-width="1.6" stroke-dasharray="3 2"/>'
                      f'<circle cx="12" cy="12" r="2.4" fill="#ffffff" stroke="{INK}" stroke-width="1"/>',
    'edge-pad': f'<rect x="3.5" y="5.5" width="17" height="13" fill="url(#bw)" {S}/>'
                f'<path d="M3.5 5.5h4v13h-4zM16.5 5.5h4v13h-4z" fill="none" stroke="#d9342b" stroke-width="1"/>',
    'fill-none': f'<rect x="4.5" y="4.5" width="15" height="15" fill="#ffffff" {S}/><path d="M6 6l12 12M18 6L6 18" stroke="#d9342b" stroke-width="1.6"/>',
    'fill-uniform': f'<rect x="4.5" y="4.5" width="15" height="15" fill="url(#blue)" {S}/>',
    'fill-linear': f'<rect x="4.5" y="4.5" width="15" height="15" fill="url(#bw)" {S}/>',
    'fill-radial': f'<rect x="4.5" y="4.5" width="15" height="15" fill="url(#radial)" {S}/>',
    'fill-conical': f'<rect x="4.5" y="4.5" width="15" height="15" fill="#1f5fae" {S}/>'
                    f'<path d="M12 12L19.5 12V4.5H12z" fill="#ffffff"/><path d="M12 12V4.5H4.5z" fill="#c9daf0"/>'
                    f'<path d="M12 12L4.5 4.5V12z" fill="#8fb3e0"/><path d="M12 12H4.5v7.5z" fill="#5a8fd1"/>'
                    f'<rect x="4.5" y="4.5" width="15" height="15" fill="none" {S}/>',
    # --- Seitennavigation -------------------------------------------------------------------
    'page-insert': f'<path d="M4.5 2.5h8l3 3v15h-11z" fill="url(#paper)" {S}/>'
                   f'<circle cx="17" cy="17" r="4.8" fill="#3fae49" stroke="#1e6e26" stroke-width="0.8"/>'
                   f'<path d="M17 14.3v5.4M14.3 17h5.4" stroke="#ffffff" stroke-width="1.6"/>',
    # --- Kurven und Knoten (Freihand- und Formwerkzeug) -------------------------------------
    'tool-freehand': f'<path d="M3.5 18.5c2.5-6 5-7.5 7-4.5s4 2.5 6-3" fill="none" stroke="{INK}" stroke-width="1.4"/>'
                     f'<path d="M15.6 9.2l3.6-5.6 1.9 1.2-3.6 5.6-2.5 1.1z" fill="url(#steel)" {S}/>',
    'tool-line': f'<path d="M4.5 19.5L19.5 4.5" stroke="{INK}" stroke-width="1.5"/>'
                 f'<rect x="2.6" y="17.6" width="3.8" height="3.8" fill="#ffffff" stroke="{INK}" stroke-width="1"/>'
                 f'<rect x="17.6" y="2.6" width="3.8" height="3.8" fill="#ffffff" stroke="{INK}" stroke-width="1"/>',
    'tool-bezier': f'<path d="M3.5 20C4 12 8 10.5 12 10.5S20 9 20.5 3.5" fill="none" stroke="{INK}" stroke-width="1.5"/>'
                   f'<path d="M5 10.5h14" stroke="{INK}" stroke-width="0.8" stroke-dasharray="1.6 1.2"/>'
                   f'<circle cx="4.5" cy="10.5" r="1.6" fill="#1d1d1d"/>'
                   f'<circle cx="19.5" cy="10.5" r="1.6" fill="#1d1d1d"/>'
                   f'<rect x="10" y="8.5" width="4" height="4" fill="#ffffff" stroke="{INK}" stroke-width="1"/>',
    'node-add':f'<path d="M3 17c4-9 10-9 14 0" fill="none" stroke="{INK}" stroke-width="1.2"/>'
                f'<rect x="8" y="8.6" width="4" height="4" fill="#ffffff" stroke="{INK}" stroke-width="1"/>'
                f'<path d="M17.5 3.5v6M14.5 6.5h6" stroke="{INK}" stroke-width="1.6"/>',
    'node-delete': f'<path d="M3 17c4-9 10-9 14 0" fill="none" stroke="{INK}" stroke-width="1.2"/>'
                   f'<rect x="8" y="8.6" width="4" height="4" fill="#ffffff" stroke="{INK}" stroke-width="1"/>'
                   f'<path d="M14.5 6.5h6" stroke="{INK}" stroke-width="1.6"/>',
    'node-to-line': f'<path d="M4 18L20 6" stroke="{INK}" stroke-width="1.4"/>'
                    f'<rect x="2.5" y="16.5" width="3" height="3" fill="#ffffff" stroke="{INK}" stroke-width="0.9"/>'
                    f'<rect x="18.5" y="4.5" width="3" height="3" fill="#ffffff" stroke="{INK}" stroke-width="0.9"/>',
    'node-to-curve': f'<path d="M4 18C6 8 14 16 20 6" fill="none" stroke="{INK}" stroke-width="1.4"/>'
                     f'<rect x="2.5" y="16.5" width="3" height="3" fill="#ffffff" stroke="{INK}" stroke-width="0.9"/>'
                     f'<rect x="18.5" y="4.5" width="3" height="3" fill="#ffffff" stroke="{INK}" stroke-width="0.9"/>',
    'node-cusp': f'<path d="M3 19L12 7l9 12" fill="none" stroke="{INK}" stroke-width="1.4"/>'
                 f'<path d="M12 7L6 3.5M12 7l6-3.5" stroke="{INK}" stroke-width="0.9" stroke-dasharray="1.5 1"/>'
                 f'<rect x="10.3" y="5.3" width="3.4" height="3.4" fill="{INK}"/>',
    'node-smooth': f'<path d="M3 19c1-8 5-11 9-11s6 4 9 11" fill="none" stroke="{INK}" stroke-width="1.4"/>'
                   f'<path d="M5 8h14" stroke="{INK}" stroke-width="0.9" stroke-dasharray="1.5 1"/>'
                   f'<rect x="10.3" y="6.3" width="3.4" height="3.4" fill="{INK}"/>',
    'node-symmetric': f'<path d="M3 19c1-8 5-11 9-11s8 3 9 11" fill="none" stroke="{INK}" stroke-width="1.4"/>'
                      f'<path d="M4 8h16" stroke="{INK}" stroke-width="0.9" stroke-dasharray="1.5 1"/>'
                      f'<path d="M4 6v4M20 6v4" stroke="{INK}" stroke-width="1"/>'
                      f'<rect x="10.3" y="6.3" width="3.4" height="3.4" fill="{INK}"/>',
    'path-close': f'<path d="M7 19C2 14 4 5 12 5s10 9 5 14" fill="none" stroke="{INK}" stroke-width="1.4"/>'
                  f'<path d="M8 19h8" stroke="{INK}" stroke-width="1.4" stroke-dasharray="2 1.4"/>',
    'path-reverse': f'<path d="M4 9h13M4 15h13" stroke="{INK}" stroke-width="1.3"/>'
                    f'<path d="M15 6l4 3-4 3z" fill="{INK}"/><path d="M8 12l-4 3 4 3z" fill="{INK}"/>',
    'to-curves': f'<rect x="3.5" y="5.5" width="10" height="10" fill="none" {S}/>'
                 f'<path d="M10 20c2-7 6-9 11-9" fill="none" stroke="{INK}" stroke-width="1.4"/>'
                 f'<rect x="8.5" y="18.5" width="3" height="3" fill="#ffffff" stroke="{INK}" stroke-width="0.9"/>'
                 f'<rect x="19" y="9.5" width="3" height="3" fill="#ffffff" stroke="{INK}" stroke-width="0.9"/>',
    'blur': f'<circle cx="12" cy="12" r="8" fill="#5a5a5a" opacity="0.15"/><circle cx="12" cy="12" r="6.5" fill="#5a5a5a" opacity="0.25"/>'
            f'<circle cx="12" cy="12" r="5" fill="#5a5a5a" opacity="0.4"/><circle cx="12" cy="12" r="3.4" fill="#3a3a3a"/>',
    # --- Beschneiden-Gruppe ------------------------------------------------------------------
    'tool-crop': f'<path d="M7 2.5v14.5h14.5" fill="none" stroke="{INK}" stroke-width="2"/>'
                 f'<path d="M2.5 7h14.5v14.5" fill="none" stroke="{INK}" stroke-width="2"/>'
                 f'<path d="M19.5 4.5l-12 12" stroke="{INK}" stroke-width="1"/>',
    'tool-knife': f'<path d="M3.5 20.5L14.5 9.5l2.2 2.2-8 8.4c-1.6 1.2-3.6 1.3-5.2.4z" fill="url(#steel)" {S}/>'
                  f'<path d="M14.5 9.5l3.4-3.4c.9-.9 2.3-.9 3.2 0 .9.9.9 2.3 0 3.2l-3.4 3.4z" fill="#3a3a3a"/>',
    'tool-eraser': f'<path d="M3.8 15.6l9.4-9.4 6.3 6.3-7.2 7.2H8z" fill="url(#steel)" {S}/>'
                   f'<path d="M3.8 15.6l3.5-3.5 6.3 6.3-1.3 1.3H8z" fill="#ffffff" {S}/>'
                   f'<path d="M12.3 19.7h8.2" stroke="{INK}" stroke-width="1"/>',
    'tool-virtual-delete': f'<path d="M3 17L21 7M3 7l18 10" stroke="{INK}" stroke-width="1.2"/>'
                           f'<path d="M9.5 10.6l5-2.8" stroke="#ffffff" stroke-width="2.6"/>'
                           f'<path d="M9.5 10.6l5-2.8" stroke="{INK}" stroke-width="0.9" stroke-dasharray="1.2 1"/>',
    # --- Kurvengruppe: weitere Werkzeuge ------------------------------------------------------
    'tool-media': f'<path d="M3 16c2-5 5-6 8-3s6 2 9.5-4c-2 7-5.5 9-9.5 6.5S5 13 3 16z" fill="#1d1d1d"/>',
    'tool-pen': f'<path d="M12 2.5l5.5 9-3 7.5h-5l-3-7.5z" fill="url(#steel)" {S}/>'
                f'<path d="M12 2.5v9" stroke="{INK}" stroke-width="0.9"/><circle cx="12" cy="12.5" r="1.3" fill="{INK}"/>'
                f'<path d="M8.5 21.5h7" stroke="{INK}" stroke-width="1.6"/>',
    'tool-bspline': f'<path d="M3 19C7 4 17 4 21 19" fill="none" stroke="{INK}" stroke-width="1.4"/>'
                    f'<path d="M3 19L7.5 6.5 16.5 6.5 21 19" fill="none" stroke="{INK}" stroke-width="0.8" stroke-dasharray="1.5 1.2"/>'
                    f'<circle cx="7.5" cy="6.5" r="1.6" fill="#ffffff" stroke="{INK}" stroke-width="0.9"/>'
                    f'<circle cx="16.5" cy="6.5" r="1.6" fill="#ffffff" stroke="{INK}" stroke-width="0.9"/>',
    'tool-polyline': f'<path d="M3 19l5-11 5 7 4-10 4 6" fill="none" stroke="{INK}" stroke-width="1.4"/>',
    'tool-3point': f'<path d="M3 18C6 6 18 6 21 18" fill="none" stroke="{INK}" stroke-width="1.4"/>'
                   f'<rect x="1.5" y="16.5" width="3" height="3" fill="#ffffff" stroke="{INK}" stroke-width="0.9"/>'
                   f'<rect x="19.5" y="16.5" width="3" height="3" fill="#ffffff" stroke="{INK}" stroke-width="0.9"/>'
                   f'<rect x="10.5" y="7" width="3" height="3" fill="#1d1d1d"/>',
    # --- Künstlerische Medien: Arten und Einstellungen ----------------------------------------
    'media-preset': f'<path d="M3 15c3-5 6-6 9-3s6 2 9-4c-1 6-5 9-9 6.5S5 12 3 15z" fill="#1d1d1d"/>',
    'media-brush': f'<path d="M14 3.5l6.5 6.5-7 7-6.5-6.5z" fill="url(#steel)" {S}/>'
                   f'<path d="M7 10.5l6.5 6.5c-1 3-4 4.5-9.5 4.5 0-5.5 1.5-8.5 3-11z" fill="#3a3a3a"/>',
    'media-sprayer': f'<rect x="4.5" y="9" width="8" height="12" rx="1.5" fill="url(#steel)" {S}/>'
                     f'<rect x="6.5" y="5.5" width="4" height="3.5" fill="#3a3a3a"/>'
                     f'<circle cx="16" cy="5" r="0.9" fill="{INK}"/><circle cx="18.5" cy="7.5" r="0.9" fill="{INK}"/>'
                     f'<circle cx="16.5" cy="9.5" r="0.9" fill="{INK}"/><circle cx="20" cy="4" r="0.9" fill="{INK}"/>'
                     f'<circle cx="20.5" cy="10.5" r="0.9" fill="{INK}"/>',
    'media-calligraphy': f'<path d="M16 2.5l5.5 5.5-9 9-4-1.5-1.5-4z" fill="url(#steel)" {S}/>'
                         f'<path d="M7 11.5l5.5 5.5-8.5 4.5z" fill="#3a3a3a"/>',
    'media-pressure': f'<path d="M4 17c3-1 4-8 8-8s5 7 8 8" fill="none" stroke="#1d1d1d" stroke-width="3" stroke-linecap="round"/>'
                      f'<path d="M12 2.5v5M9.8 5.4 12 7.6l2.2-2.2" fill="none" stroke="{INK}" stroke-width="1.2"/>',
    'smoothing': f'<path d="M3 15l2.5-4 2 3 2.5-5 2 4" fill="none" stroke="{INK}" stroke-width="1.1"/>'
                 f'<path d="M12 12c3-6 6-6 9-2" fill="none" stroke="{INK}" stroke-width="1.6"/>',
    'media-width': f'<path d="M3 12c3-4 6-4 9 0s6 4 9 0" fill="none" stroke="#1d1d1d" stroke-width="4" stroke-linecap="round"/>'
                   f'<path d="M3 4.5h18M3 3v3M21 3v3" stroke="{INK}" stroke-width="1"/>',
    'media-angle': f'<path d="M4 19.5h16M4 19.5L16 7.5" stroke="{INK}" stroke-width="1.3"/>'
                   f'<path d="M11 19.5a7 7 0 0 0-2-5" fill="none" stroke="{INK}" stroke-width="1"/>',
    # --- Radierer ------------------------------------------------------------------------------
    'eraser-width': f'<circle cx="12" cy="13" r="5" fill="#ffffff" stroke="{INK}" stroke-width="1.2"/>'
                    f'<path d="M5 4.5h14M5 3v3M19 3v3" stroke="{INK}" stroke-width="1"/>',
    'eraser-round': f'<circle cx="12" cy="12" r="7" fill="#ffffff" stroke="{INK}" stroke-width="1.4"/>',
    'eraser-square': f'<rect x="5" y="5" width="14" height="14" fill="#ffffff" stroke="{INK}" stroke-width="1.4"/>',
    # --- Rechteck-, Ellipsen- und Polygongruppe -----------------------------------------------
    'tool-rectangle-3point': f'<path d="M4 15l9-9 7 7-9 9z" fill="url(#steel)" {S} stroke-width="1.2"/>'
                             f'<circle cx="4" cy="15" r="1.5" fill="#1d1d1d"/><circle cx="13" cy="6" r="1.5" fill="#1d1d1d"/>'
                             f'<circle cx="20" cy="13" r="1.5" fill="#1d1d1d"/>',
    'tool-ellipse-3point': f'<ellipse cx="12" cy="12" rx="8.5" ry="5" transform="rotate(-35 12 12)" fill="url(#steel)" {S} stroke-width="1.2"/>'
                           f'<circle cx="5" cy="17" r="1.5" fill="#1d1d1d"/><circle cx="19" cy="7" r="1.5" fill="#1d1d1d"/>'
                           f'<circle cx="9.5" cy="8.5" r="1.5" fill="#1d1d1d"/>',
    'tool-star': f'<path d="M12 2.8l2.7 6 6.5.6-4.9 4.3 1.5 6.4L12 16.8l-5.8 3.3 1.5-6.4-4.9-4.3 6.5-.6z" fill="url(#steel)" {S} stroke-width="1.2"/>',
    'tool-complex-star': f'<path d="M12 2.5L17.6 19.7 3 9.1h18L6.4 19.7z" fill="none" stroke="{INK}" stroke-width="1.2" stroke-linejoin="round"/>',
    'tool-graph-paper': f'<rect x="3.5" y="3.5" width="17" height="17" fill="#ffffff" {S}/>'
                        f'<path d="M9.2 3.5v17M14.8 3.5v17M3.5 9.2h17M3.5 14.8h17" stroke="{INK}" stroke-width="0.8"/>',
    'tool-spiral': f'<path d="M12 12a1.5 1.5 0 0 1 3 0 3 3 0 0 1-6 0 4.5 4.5 0 0 1 9 0 6 6 0 0 1-12 0 7.5 7.5 0 0 1 15 0" fill="none" stroke="{INK}" stroke-width="1.2"/>',
    'ellipse-full': f'<ellipse cx="12" cy="12" rx="8.5" ry="6.5" fill="url(#steel)" {S} stroke-width="1.2"/>',
    'ellipse-pie': f'<path d="M12 12h8.5A8.5 6.5 0 1 1 12 5.5z" fill="url(#steel)" {S} stroke-width="1.2"/>',
    'ellipse-arc': f'<path d="M20.5 12A8.5 6.5 0 1 1 12 5.5" fill="none" stroke="{INK}" stroke-width="1.5"/>',
    'angle-start': f'<path d="M4 18h16M4 18L15 7" stroke="{INK}" stroke-width="1.2"/>'
                   f'<path d="M10 18a6 6 0 0 0-1.8-4.2" fill="none" stroke="{INK}" stroke-width="1"/>'
                   f'<circle cx="4" cy="18" r="1.6" fill="#1d1d1d"/>',
    'angle-end': f'<path d="M4 18h16M4 18L15 7" stroke="{INK}" stroke-width="1.2"/>'
                 f'<path d="M10 18a6 6 0 0 0-1.8-4.2" fill="none" stroke="{INK}" stroke-width="1"/>'
                 f'<circle cx="15" cy="7" r="1.6" fill="#1d1d1d"/>',
    'arc-direction': f'<path d="M18.5 12A6.5 6.5 0 1 1 12 5.5" fill="none" stroke="{INK}" stroke-width="1.4"/>'
                     f'<path d="M11 2.5l3.5 3-3.5 3z" fill="{INK}"/>',
    'polygon-points': f'<path d="M12 3.5l8 6-3 9.5H7l-3-9.5z" fill="url(#steel)" {S} stroke-width="1.2"/>'
                      f'<circle cx="12" cy="3.5" r="1.6" fill="#1d1d1d"/><circle cx="20" cy="9.5" r="1.6" fill="#1d1d1d"/>'
                      f'<circle cx="17" cy="19" r="1.6" fill="#1d1d1d"/><circle cx="7" cy="19" r="1.6" fill="#1d1d1d"/>'
                      f'<circle cx="4" cy="9.5" r="1.6" fill="#1d1d1d"/>',
    'star-points': f'<path d="M12 2.8l2.7 6 6.5.6-4.9 4.3 1.5 6.4L12 16.8l-5.8 3.3 1.5-6.4-4.9-4.3 6.5-.6z" fill="url(#steel)" {S}/>'
                   f'<circle cx="12" cy="2.8" r="1.5" fill="#1d1d1d"/><circle cx="21.2" cy="9.4" r="1.5" fill="#1d1d1d"/>'
                   f'<circle cx="2.8" cy="9.4" r="1.5" fill="#1d1d1d"/>',
    'complex-star-points': f'<path d="M12 3L17.3 19.3 3.4 9.2h17.2L6.7 19.3z" fill="none" stroke="{INK}" stroke-width="1.1" stroke-linejoin="round"/>'
                           f'<circle cx="12" cy="3" r="1.5" fill="#1d1d1d"/><circle cx="20.6" cy="9.2" r="1.5" fill="#1d1d1d"/>'
                           f'<circle cx="3.4" cy="9.2" r="1.5" fill="#1d1d1d"/>',
    'star-sharpness': f'<path d="M12 2.5l2 9.5 7.5 9.5H2.5L10 12z" fill="url(#steel)" {S}/>'
                      f'<path d="M7.5 21.5l4.5-5 4.5 5" fill="none" stroke="{INK}" stroke-width="0.9"/>',
    # --- Objekt: Kombinieren, Sperren, Anordnung -------------------------------------------------
    'combine': f'<rect x="3.5" y="3.5" width="10" height="10" fill="#1d1d1d" {S}/>'
               f'<rect x="10.5" y="10.5" width="10" height="10" fill="#1d1d1d" {S}/>'
               f'<rect x="10.5" y="10.5" width="3" height="3" fill="#ffffff"/>',
    'break-apart': f'<rect x="2.5" y="2.5" width="9" height="9" fill="url(#steel)" {S}/>'
                   f'<rect x="12.5" y="12.5" width="9" height="9" fill="#1d1d1d" {S}/>'
                   f'<path d="M14 5.5l4.5 0M18.5 5.5l-2-2M18.5 5.5l-2 2" fill="none" stroke="{INK}" stroke-width="1"/>',
    # Textleiste: Buchstaben als Umriss, Ausrichtung als Zeilen
    'text-bold': f'<path d="M7 4.5h6c2.6 0 4 1.3 4 3.3 0 1.4-.8 2.4-2 2.9 1.6.4 2.6 1.6 2.6 3.3 0 2.4-1.7 3.8-4.5 3.8H7zm3 2.4v3.3h2.6c1.1 0 1.8-.6 1.8-1.7s-.7-1.6-1.8-1.6zm0 5.6v3.6h3c1.2 0 2-.7 2-1.8s-.8-1.8-2-1.8z" fill="#ffffff" stroke="{INK}" stroke-width="1" stroke-linejoin="round"/>',
    'text-italic': f'<path d="M10 4.5h7.5l-.5 2.2h-2.4l-2.4 10.6H14.6l-.5 2.2H6.6l.5-2.2h2.4L11.9 6.7H9.5z" fill="#ffffff" stroke="{INK}" stroke-width="1" stroke-linejoin="round"/>',
    'text-underline': f'<path d="M7 3.5h3v8c0 1.6.8 2.5 2 2.5s2-.9 2-2.5v-8h3v8.2c0 3.2-2 5-5 5s-5-1.8-5-5z" fill="#ffffff" stroke="{INK}" stroke-width="1" stroke-linejoin="round"/>'
                      f'<path d="M5.5 20h13" stroke="{INK}" stroke-width="1.6"/>',
    'align-left': f'<path d="M4 5.5h16M4 9.5h10M4 13.5h16M4 17.5h10" stroke="{INK}" stroke-width="1.6"/>',
    'align-center': f'<path d="M4 5.5h16M7 9.5h10M4 13.5h16M7 17.5h10" stroke="{INK}" stroke-width="1.6"/>',
    'align-right': f'<path d="M4 5.5h16M10 9.5h10M4 13.5h16M10 17.5h10" stroke="{INK}" stroke-width="1.6"/>',
    'align-justify': f'<path d="M4 5.5h16M4 9.5h16M4 13.5h16M4 17.5h16" stroke="{INK}" stroke-width="1.6"/>',
    'hyphenation': f'<path d="M3 15.5c0-2.6 1.4-4 3.4-4 1.9 0 3.1 1.3 3.1 3.6v4.4M9.5 15.6c-.5-.5-1.6-.7-2.7-.7-1.8 0-3 .9-3 2.3 0 1.2.9 2.1 2.4 2.1 1.4 0 2.6-.9 3.3-2" fill="none" stroke="{INK}" stroke-width="1.3"/>'
                   f'<path d="M11 14.5h3" stroke="#1f5fae" stroke-width="1.8"/>'
                   f'<path d="M15.5 8.5v11M15.5 15c.4-2 1.6-3.2 3.1-3.2 1.7 0 2.9 1.5 2.9 3.8s-1.2 3.9-2.9 3.9c-1.5 0-2.7-1.2-3.1-3.2" fill="none" stroke="{INK}" stroke-width="1.3"/>',
    'doc-import': f'<path d="M5 3.5h9l4.5 4.5v12.5H5z" fill="url(#paper)" {S}/>'
                  f'<path d="M7.5 17l3-4 2 2.5 1.5-1.5 2.5 3z" fill="#1f5fae"/><circle cx="9.5" cy="10" r="1.3" fill="#e2740c"/>'
                  f'<path d="M15 12.5l5.5 0M18 10l2.5 2.5-2.5 2.5" fill="none" stroke="#1f5fae" stroke-width="1.6"/>',
    'doc-export': f'<path d="M5 3.5h9l4.5 4.5v12.5H5z" fill="url(#paper)" {S}/>'
                  f'<path d="M7.5 17l3-4 2 2.5 1.5-1.5 2.5 3z" fill="#1f5fae"/>'
                  f'<path d="M13 9.5h7.5M18 7l2.5 2.5-2.5 2.5" fill="none" stroke="#e2740c" stroke-width="1.6"/>',
    'doc-print': f'<rect x="6.5" y="3.5" width="11" height="6" fill="#fff" {S}/><rect x="3.5" y="9.5" width="17" height="7" rx="1" fill="#d7dde6" {S}/>'
                 f'<rect x="6.5" y="14" width="11" height="6.5" fill="#fff" {S}/><path d="M8.5 16.5h7M8.5 18.5h5" fill="none" stroke="#1f5fae" stroke-width="1"/>'
                 f'<circle cx="17.8" cy="11.8" r="0.9" fill="#3aa04a"/>',
    'bitmap-edit': f'<rect x="3" y="5" width="15" height="12" fill="#ffffff" {S}/>'
                   f'<path d="M4.5 15.5l4-5 3 3.5 2-2 3 3.5z" fill="#1f5fae"/><circle cx="14" cy="8.5" r="1.4" fill="#e2740c"/>'
                   f'<path d="M14.5 21l1-3.5 6-6 2.5 2.5-6 6z" fill="url(#gold)" stroke="{INK}" stroke-width="0.9" stroke-linejoin="round"/>',
    'bitmap-trace': f'<rect x="3" y="3" width="11" height="11" fill="#ffffff" {S}/>'
                    f'<path d="M4.5 6.5h2v2h2v2h2v2h-6z" fill="#7a7a7a"/>'
                    f'<path d="M10 21c0-5 3-9 11-9" fill="none" stroke="#1f5fae" stroke-width="1.6"/>'
                    f'<rect x="8.6" y="19.6" width="2.8" height="2.8" fill="#ffffff" stroke="{INK}" stroke-width="0.9"/>'
                    f'<rect x="19.6" y="10.6" width="2.8" height="2.8" fill="#ffffff" stroke="{INK}" stroke-width="0.9"/>',
    'frame-reset': f'<rect x="4" y="4" width="16" height="16" fill="none" stroke="{INK}" stroke-width="1.2" stroke-dasharray="2 1.5"/>'
                   f'<path d="M4 4l9 0-9 9z" fill="#1f5fae" opacity="0.6"/><path d="M15 9.5a4 4 0 1 1-2.5-1.8" fill="none" stroke="{INK}" stroke-width="1.4"/>'
                   f'<path d="M11.2 5.6l2.4 2.1-2.8 1.2z" fill="{INK}"/>',
    'font': f'<path d="M14.5 4c-3.5 0-7 4-7 9 0 3.5 1.7 6 4.5 6 3.5 0 7-4 7-9 0-3.5-1.7-6-4.5-6zm-.3 1.6c1.3 0 1.9 1.5 1.9 3.6 0 3.8-1.9 8.2-4.2 8.2-1.3 0-1.9-1.5-1.9-3.6 0-3.8 1.9-8.2 4.2-8.2z" fill="#1d1d1d"/>',
    'wrap-text': f'<rect x="8.5" y="8" width="7" height="8" fill="url(#steel)" {S}/>'
                 f'<path d="M3 5h18M3 8.5h4M3 12h4M3 15.5h4M17 8.5h4M17 12h4M17 15.5h4M3 19h18" stroke="{INK}" stroke-width="1"/>',
    'break-connector': f'<rect x="2.5" y="4.5" width="6" height="6" fill="url(#steel)" {S}/>'
                       f'<rect x="15.5" y="13.5" width="6" height="6" fill="url(#steel)" {S}/>'
                       f'<path d="M8.5 7.5h3v3M12.5 13.5v3h3" fill="none" stroke="{INK}" stroke-width="1.2"/>',
    'lock': f'<path d="M8 11V7.5a4 4 0 0 1 8 0V11" fill="none" stroke="{INK}" stroke-width="1.8"/>'
            f'<rect x="5.5" y="10.5" width="13" height="10" rx="1.2" fill="#3a3a3a"/>'
            f'<circle cx="12" cy="15" r="1.4" fill="#ffffff"/>',
    'unlock': f'<path d="M8 11V7.5a4 4 0 0 1 7.8-1.2" fill="none" stroke="{INK}" stroke-width="1.8"/>'
              f'<rect x="5.5" y="10.5" width="13" height="10" rx="1.2" fill="url(#steel)" {S}/>'
              f'<circle cx="12" cy="15" r="1.4" fill="{INK}"/>',
    'arrange-page-front': f'<rect x="3.5" y="3.5" width="12" height="12" fill="url(#steel)" {S}/>'
                          f'<rect x="8.5" y="8.5" width="12" height="12" fill="#1d1d1d" {S}/>'
                          f'<path d="M3 21h4" stroke="{INK}" stroke-width="1.5"/>',
    'arrange-page-back': f'<rect x="8.5" y="8.5" width="12" height="12" fill="url(#steel)" {S}/>'
                         f'<rect x="3.5" y="3.5" width="12" height="12" fill="#1d1d1d" {S}/>'
                         f'<rect x="8.5" y="8.5" width="12" height="12" fill="none" {S}/>',
    'arrange-front': f'<rect x="3.5" y="3.5" width="11" height="11" fill="url(#steel)" {S}/>'
                     f'<rect x="9.5" y="9.5" width="11" height="11" fill="#1d1d1d" {S}/>',
    'arrange-back': f'<rect x="9.5" y="9.5" width="11" height="11" fill="url(#steel)" {S}/>'
                    f'<rect x="3.5" y="3.5" width="11" height="11" fill="#1d1d1d" {S}/>'
                    f'<path d="M9.5 9.5h5v5h-5z" fill="url(#steel)"/>',
    'arrange-forward': f'<rect x="3.5" y="8.5" width="10" height="10" fill="url(#steel)" {S}/>'
                       f'<rect x="8.5" y="11.5" width="10" height="10" fill="#1d1d1d" {S}/>'
                       f'<path d="M18 9V3M15.5 5.5 18 3l2.5 2.5" fill="none" stroke="{INK}" stroke-width="1.3"/>',
    'arrange-backward': f'<rect x="8.5" y="11.5" width="10" height="10" fill="url(#steel)" {S}/>'
                        f'<rect x="3.5" y="8.5" width="10" height="10" fill="#1d1d1d" {S}/>'
                        f'<path d="M18 3v6M15.5 6.5 18 9l2.5-2.5" fill="none" stroke="{INK}" stroke-width="1.3"/>',
    'arrange-before': f'<rect x="3.5" y="9.5" width="11" height="11" fill="url(#steel)" {S}/>'
                      f'<path d="M14 3l7 4.5-3 .6 2.2 3.4-1.6 1-2.2-3.4-2 2.2z" fill="#1d1d1d"/>',
    'arrange-behind': f'<rect x="3.5" y="9.5" width="11" height="11" fill="#1d1d1d" {S}/>'
                      f'<path d="M14 3l7 4.5-3 .6 2.2 3.4-1.6 1-2.2-3.4-2 2.2z" fill="url(#steel)" {S}/>',
    'arrange-reverse': f'<rect x="3.5" y="3.5" width="9" height="9" fill="#1d1d1d" {S}/>'
                       f'<rect x="11.5" y="11.5" width="9" height="9" fill="url(#steel)" {S}/>'
                       f'<path d="M15 4.5a4 4 0 0 1 4 4M19 8.5l1.5-2M19 8.5l-2-1.2M9 19.5a4 4 0 0 1-4-4M5 15.5l-1.5 2M5 15.5l2 1.2" fill="none" stroke="{INK}" stroke-width="1"/>',
    # --- Seiten-Eigenschaftsleiste -------------------------------------------------------------
    'pages-all': f'<rect x="7.5" y="3.5" width="11" height="14" fill="#ffffff" {S}/>'
                 f'<rect x="4.5" y="6.5" width="11" height="14" fill="url(#paper)" {S}/>',
    'page-current': f'<rect x="6.5" y="3.5" width="11" height="17" fill="url(#paper)" {S}/>'
                    f'<path d="M9 9h6M9 12h6M9 15h4" stroke="{INK}" stroke-width="0.9"/>',
    'nudge': f'<path d="M12 3l3 3.5h-2v4h4v-2l3.5 3.5-3.5 3.5v-2h-4v4h2L12 21l-3-3.5h2v-4H7v2L3.5 12 7 8.5v2h4v-4H9z" fill="{INK}"/>',
    'duplicate-x': f'<rect x="3.5" y="7.5" width="8" height="8" fill="url(#paper)" {S}/>'
                   f'<rect x="12.5" y="7.5" width="8" height="8" fill="#ffffff" {S} stroke-dasharray="1.5 1"/>'
                   f'<path d="M5 19.5h14" stroke="{INK}" stroke-width="1"/><path d="M17 17.8l2.2 1.7-2.2 1.7z" fill="{INK}"/>',
    'duplicate-y': f'<rect x="7.5" y="3.5" width="8" height="8" fill="url(#paper)" {S}/>'
                   f'<rect x="7.5" y="12.5" width="8" height="8" fill="#ffffff" {S} stroke-dasharray="1.5 1"/>'
                   f'<path d="M19.5 5v14" stroke="{INK}" stroke-width="1"/><path d="M17.8 17l1.7 2.2 1.7-2.2z" fill="{INK}"/>',
    # Pfeile für Klapplisten und Zahlenfelder der Leisten (Stylesheet)
    'arrow-down': f'<path d="M7 9.5h10l-5 5.5z" fill="{INK}"/>',
    'arrow-up': f'<path d="M7 14.5h10l-5-5.5z" fill="{INK}"/>',
    'nav-first': f'<path d="M6 6v12" stroke="{INK}" stroke-width="1.8"/><path d="M18 6v12l-9-6z" fill="{INK}"/>',
    'nav-prev': f'<path d="M16 6v12l-9-6z" fill="{INK}"/>',
    'nav-next': f'<path d="M8 6v12l9-6z" fill="{INK}"/>',
    'nav-last': f'<path d="M18 6v12" stroke="{INK}" stroke-width="1.8"/><path d="M6 6v12l9-6z" fill="{INK}"/>',
    # --- Farbe ------------------------------------------------------------------------------
    'eyedropper': f'<path d="M17.5 3.2a2.5 2.5 0 0 1 3.3 3.3l-2.6 2.6 1 1-1.7 1.7-1-1-7.4 7.4-3.3.8.8-3.3 7.4-7.4-1-1 1.7-1.7 1 1z" fill="url(#steel)" {S}/>'
                  f'<path d="M6.6 17.2l-.6 2.6 2.6-.6 5.6-5.6-2-2z" fill="#1f5fae"/>'
                  f'<path d="M4 21.5c-.9-1.2-.9-2 .1-3.1.9 1.1 1 1.9-.1 3.1z" fill="#1f5fae"/>',
    'color-models': f'<rect x="3.5" y="3.5" width="17" height="17" fill="url(#hue)" {S}/>'
                    f'<rect x="3.5" y="3.5" width="17" height="17" fill="url(#shade)"/>'
                    f'<rect x="3.5" y="3.5" width="17" height="17" fill="none" {S}/>',
    'color-palettes': f'<rect x="3.5" y="3.5" width="8" height="8" fill="#e53935" {S}/><rect x="12.5" y="3.5" width="8" height="8" fill="#fdd835" {S}/>'
                      f'<rect x="3.5" y="12.5" width="8" height="8" fill="#43a047" {S}/><rect x="12.5" y="12.5" width="8" height="8" fill="#1e88e5" {S}/>',
}


def main():
    os.makedirs(OUT, exist_ok=True)
    keep = set(name + '.svg' for name in ICONS)
    for f in os.listdir(OUT):
        if f.endswith('.svg') and f not in keep:
            os.remove(os.path.join(OUT, f))
    for name, body in ICONS.items():
        with open(os.path.join(OUT, name + '.svg'), 'w', encoding='utf-8', newline='\n') as fh:
            fh.write(svg(body))
    print('%d Symbole nach %s' % (len(ICONS), os.path.normpath(OUT)))


if __name__ == '__main__':
    main()
