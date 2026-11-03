# Scripting (JavaScript)

TANGENTA runs JavaScript (ECMAScript 7, Qt QJSEngine), either in the **Scripts** docker
(Ctrl+Enter) or via **Tools › Scripts**. Scripts are stored as `.js` files in the scripts folder
(Options › Workspace › Scripts; empty = `%APPDATA%/Tangenta/Tangenta/Skripte`).

- Measurements in **mm**, page coordinates, (0,0) = top left, y pointing down.
- Colors as `"#rrggbb"`, `"#rrggbbaa"` or names (`"orange"`, `"royalblue"`); `null` = none.
- A run is **one** undo step. If an error occurs, everything is reverted.
- If a script runs longer than the time limit (default 30 s), it is aborted.
- No access to files, the network or other programs.

## The `tg` object

| Property / function | Meaning |
|---|---|
| `tg.pageCount`, `tg.currentPage` (1-based, settable) | pages |
| `tg.pageWidth`, `tg.pageHeight` | size of the current page |
| `tg.rectangle(x, y, w, h, radius?)` | rectangle |
| `tg.ellipse(x, y, w, h)` | ellipse |
| `tg.polygon(x, y, w, h, corners?, star?)` | polygon or star |
| `tg.line(x1, y1, x2, y2)` | line |
| `tg.polyline([[x, y], …], closed?)` | polyline |
| `tg.text(x, y, text, pt?, font?)` | artistic text |
| `tg.selection()` / `tg.select(obj \| [obj…])` | read / set the selection |
| `tg.objects()` | objects on the current page |
| `tg.find(id)` | object by its number |
| `console.log(…)`, `tg.print(…)` | output in the docker |

Newly created objects get the default fill and outline; after the run, the last object created is
selected (or whatever `tg.select` set).

## Objects

| | |
|---|---|
| `id`, `type` | number, kind (`rectangle`, `ellipse`, `polygon`, `curve`, `text`, `table`, `image`, …) |
| `x`, `y`, `width`, `height` | bounding box, settable (moves or scales) |
| `fill`, `stroke`, `strokeWidth` | fill, outline color, outline width in mm |
| `text` | content of a text object |
| `name` | name in the Object Manager |
| `move(dx, dy)`, `moveTo(x, y)` | move |
| `rotate(degrees, cx?, cy?)` | rotate counterclockwise, by default around the center |
| `scale(sx, sy?)` | scale around the center |
| `duplicate(dx?, dy?)` | copy, returns the new object |
| `remove()` | delete |
| `properties()`, `get(name)`, `set(name, value)` | property bar values, e.g. `points` for a polygon, `rows`/`columns` for a table |

## Example

```js
// Grid of circles, color by column
const colors = ["crimson", "orange", "gold", "seagreen", "royalblue"];
for (let r = 0; r < 4; r++)
    for (let c = 0; c < colors.length; c++) {
        const k = tg.ellipse(20 + c * 30, 20 + r * 30, 24, 24);
        k.fill = colors[c];
        k.stroke = null;
    }

// Move all selected objects 5 mm to the right and give them a red outline
for (const o of tg.selection()) {
    o.move(5, 0);
    o.stroke = "red";
}
```
