# Skripte (JavaScript)

TANGENTA führt JavaScript (ECMAScript 7, Qt QJSEngine) aus – im Andockfenster **Skripte**
(Strg+Eingabe) oder über **Extras › Skripte**. Skripte liegen als `.js` im Skriptordner
(Optionen › Arbeitsbereich › Skripte; leer = `%APPDATA%/Tangenta/Tangenta/Skripte`).

- Masse in **mm**, Seitenkoordinaten, (0,0) = links oben, y nach unten.
- Farben als `"#rrggbb"`, `"#rrggbbaa"` oder Namen (`"orange"`, `"royalblue"`); `null` = keine.
- Ein Lauf ist **ein** Rückgängig-Schritt. Bei einem Fehler wird alles zurückgenommen.
- Läuft ein Skript länger als die Zeitgrenze (Vorgabe 30 s), wird es abgebrochen.
- Kein Zugriff auf Dateien, Netz oder Programme.

## Das Objekt `tg`

| Eigenschaft / Funktion | Bedeutung |
|---|---|
| `tg.pageCount`, `tg.currentPage` (1-basiert, setzbar) | Seiten |
| `tg.pageWidth`, `tg.pageHeight` | Grösse der aktuellen Seite |
| `tg.rectangle(x, y, b, h, radius?)` | Rechteck |
| `tg.ellipse(x, y, b, h)` | Ellipse |
| `tg.polygon(x, y, b, h, ecken?, stern?)` | Polygon bzw. Stern |
| `tg.line(x1, y1, x2, y2)` | Linie |
| `tg.polyline([[x, y], …], geschlossen?)` | Linienzug |
| `tg.text(x, y, text, pt?, schrift?)` | Grafiktext |
| `tg.selection()` / `tg.select(obj \| [obj…])` | Auswahl lesen / setzen |
| `tg.objects()` | Objekte der aktuellen Seite |
| `tg.find(id)` | Objekt über seine Nummer |
| `console.log(…)`, `tg.print(…)` | Ausgabe im Andockfenster |

Neu erzeugte Objekte bekommen die Standardfüllung und -kontur; nach dem Lauf ist das zuletzt
erzeugte Objekt ausgewählt (oder was `tg.select` gesetzt hat).

## Objekte

| | |
|---|---|
| `id`, `type` | Nummer, Art (`rectangle`, `ellipse`, `polygon`, `curve`, `text`, `table`, `image`, …) |
| `x`, `y`, `width`, `height` | Begrenzungsrahmen, setzbar (verschiebt bzw. skaliert) |
| `fill`, `stroke`, `strokeWidth` | Füllung, Konturfarbe, Konturbreite in mm |
| `text` | Inhalt eines Textes |
| `name` | Name im Objekt-Manager |
| `move(dx, dy)`, `moveTo(x, y)` | verschieben |
| `rotate(grad, cx?, cy?)` | drehen gegen den Uhrzeigersinn, Vorgabe um die Mitte |
| `scale(sx, sy?)` | skalieren um die Mitte |
| `duplicate(dx?, dy?)` | Kopie, gibt das neue Objekt zurück |
| `remove()` | löschen |
| `properties()`, `get(name)`, `set(name, wert)` | Werte der Eigenschaftsleiste, z. B. `points` beim Polygon, `rows`/`columns` bei der Tabelle |

## Beispiel

```js
// Raster aus Kreisen, Farbe nach Spalte
const colors = ["crimson", "orange", "gold", "seagreen", "royalblue"];
for (let r = 0; r < 4; r++)
    for (let c = 0; c < colors.length; c++) {
        const k = tg.ellipse(20 + c * 30, 20 + r * 30, 24, 24);
        k.fill = colors[c];
        k.stroke = null;
    }

// Alle ausgewählten Objekte 5 mm nach rechts und rot umranden
for (const o of tg.selection()) {
    o.move(5, 0);
    o.stroke = "red";
}
```
