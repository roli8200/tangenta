# Usage

| Action | Effect |
|---|---|
| Click | select; Shift+click adds/removes |
| Drag on empty space | selection marquee, only fully enclosed objects |
| Drag a corner handle | scale proportionally; Shift from the center, Ctrl in 100% steps |
| Drag a side handle | wider/narrower or taller/flatter |
| Second click on the selection | rotate/skew mode; corners rotate (Ctrl 15°), sides skew, ⊙ can be moved |
| Drag an object | move; Ctrl constrains to horizontal/vertical |
| While dragging (move, scale, rotate, skew): right-click | the original stays, a copy lands at the new position on release (one undo step) |
| While dragging: Space | stamps a copy at the current position and keeps dragging |
| Numpad + | duplicate in place (Edit > Duplicate in Place) |
| Arrow keys | nudge by the page's nudge distance (default 0.1 mm); Shift ten times, Ctrl one tenth |
| F6 / F7 / Y / F5 / I / F10 / X / Z / Space | Rectangle / Ellipse / Polygon / Freehand / Artistic Media / Shape / Eraser / Zoom / toggle between Pick and the last drawing tool |
| Tools with a triangle | flyout: click the triangle or press and hold; the button shows the most recently chosen tool |
| Rectangle, Ellipse, Polygon: drag | Ctrl = square/circle, Shift = from the center; Ellipse also as pie or arc, stars with points and sharpness |
| Artistic Media: drag | smoothed stroke with width variation (Preset), a fixed nib (Calligraphic), artwork stretched or repeated along the path (Brush) or objects sprayed along it (Sprayer: list, size variation, spacing, count per dab, rotation, offset, order in the property bar); the stroke follows later node edits |
| Window > Dockers > Artistic Media | stroke library with thumbnails (recently used, presets, brushes, sprayer lists, own strokes); a click applies the stroke to the selection, "Save" stores the selected objects as an own brush stroke or sprayer list |
| Ctrl+K on a Brush/Sprayer stroke | separates it into a group of the colored pieces and the plain control path |
| Crop | drag and adjust a frame; double-click or Enter crops the selection (otherwise the page) |
| Knife / Eraser | a line through objects splits them; Eraser (X) removes along the path, width and tip in the property bar |
| Right-click on objects | context menu: Convert to Curves, Break Apart, Undo, Cut, Copy, Delete, Lock, Order |
| Ctrl+Page Up / Page Down | forward one / back one; Shift+Page Up/Page Down within the layer, Ctrl+Home/End within the page |
| Ctrl+L / Ctrl+K | Combine / Break Apart; artistic text splits into lines, a single line into words, a single word into characters (stays text, same position) |
| Freehand: click, click | straight line; dragging draws freely; clicking an end of the selected curve continues it, ending on its start closes it |
| 2-Point Line: drag | straight line; Ctrl snaps in 15° steps (also with the Freehand tool) |
| Bézier: click / drag | click places a cusp node, drag a symmetrical curve node; clicking the first node closes the path, double-click or Enter finishes, Esc discards |
| Ctrl+Q | Convert to Curves (base shapes become curves, Artistic Media strokes are broken apart) |
| Shape: click/drag nodes | select and move; Shift adds, a marquee selects several, Ctrl constrains to horizontal/vertical |
| Shape: right-click on a node | Add, Delete, To Line, To Curve, Cusp/Smooth/Symmetrical, Close, Reverse Direction (also in the property bar) |
| Shape: drag a curve segment | bends it; drag the control points of selected nodes |
| Shape on a rectangle | drag a corner node: rounds all corners (one corner if "equal corners" is off), also rotated/skewed |
| Shape on an ellipse | drag its node: inside makes a pie, outside an arc (Ctrl: 15° steps) |
| Shape on a polygon/star | drag a node: all corresponding nodes follow (points → star, side nodes → curvy polygon; Ctrl radial only) |
| Shape: several nodes selected | property bar: stretch/scale or rotate/skew nodes with a frame (Ctrl 15°, Shift from the center), mirror nodes |
| Window > Dockers > Corners | fillet, scallop or chamfer the sharp corners (or the nodes selected with Shape); basic shapes become curves |
| Shape: double-click | on the line: insert a node; on a node: delete it; Delete removes selected nodes |
| Block Shadow (interactive tools flyout): drag from an object | solid vector shadow along the drag; drag its end handle to change direction and depth; depth, direction, color, expand, simplify, remove holes in the property bar; Ctrl+K separates it as a curve |
| Effects > Photo Mosaic … / Point Raster … | rebuild the selected bitmap (or the selection rendered as one) from the images of a folder, or as a vector raster of circles, squares or a second selected shape |
| Bitmaps menu filters on a vector object or text | added as a live effect: the object stays editable; Window > Dockers > Effects lists them (eye on/off, edit, order, delete, Apply Effects converts to a bitmap). On a bitmap they change the pixels |
| Bitmaps > Creative > Frame | decorative frame (torn paper, brush stroke, soft edge, stamp, ...) as transparent edge or colored border; on a SuperClip frame the contents are clipped to the rendered edge |
| Blur (property bar) | Gaussian blur in mm, for every object and every group |
| Drag from a ruler | guideline; click and drag to move it, drag it back into the ruler or press Delete to remove it |
| Snap To | snap to nodes, center, edge, guidelines, page (View menu/standard toolbar) |
| Property bar | position, size, scale, angle, mirror, arrowheads, line style, outline width; specific fields per object type |
| Palette left/right click | fill / outline; the button at the top expands the color palette |
| Shift+F11 / F11 | Uniform Fill (color dialog with eyedropper) / Fountain Fill |
| G | Interactive Fill: drag a gradient onto the object, select an end point and click a palette color |
| Interactive Fill: click a node | popup next to the node: color, eyedropper, node transparency, position of intermediate nodes |
| Interactive Fill: double-click the gradient line / right-click a node | add a node / remove an intermediate node; drag a palette color onto a node or the line to color it or add a node |
| Interactive Transparency: click a node | popup with the transparency of that node |
| Units (property bar, nothing selected) | document unit for rulers and all length fields; typing a unit (`10 pt`, `2 in`, `3 cm`) converts |
| Click a guideline twice | rotation handles at the ends; the pivot ⊙ can be moved freely, even off the line; angle/anchor in the property bar |
| Number fields | calculate: `297/2`, `(210-20)*0.5` (a decimal comma works too); Enter or leaving the field applies the value |
| Mouse wheel | zoom; Ctrl/Shift scroll, middle button pans |
| Page tabs | drag a tab to reorder the pages; right-click: rename, insert, duplicate, move, delete, change orientation (also in the Layout menu) |
| Window > Dockers > Pages | thumbnails of all pages as a list or grid, size slider, search by name; drag to reorder, double-click shows the page |
| View > Multi-Page View | all pages side by side and editable; click a page to make it current, drag objects from page to page; layout (grid/columns, row, column, spacing) under the gear in the Pages docker or Options > Document > Layout |
| Multi-Page View: click a page name | selects the page (right-click: page commands); drag its handles to resize it, Shift keeps it centered |

More shortcuts are listed in the menus and under Tools > Options > Customization > Commands.
