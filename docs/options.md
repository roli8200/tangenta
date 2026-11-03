# Options dialog

Dialog "Options": tree on the left, page with a blue title on the right; OK / Cancel / Help at the bottom.
Tree: Workspace (General, Display, Edit, Snap to Objects, Dynamic Guides, Alignment Guides, Warnings,
VBA, Save, SuperTrace, Plugins, Text[Paragraph Text Frame, Fonts, Spell Checking, QuickCorrect], Toolbox[+],
Customization[+]),
Document (General, Page Size, Layout, Label, Background, Guidelines[Horizontal, Vertical,
Guidelines, Presets], Grid, Rulers, Save, Export to HTML), Global[+].

## Workspace
- General: Getting started: "On startup:" combo (Nothing …); [x] Show 'New Document' dialog box. Undo
  levels: Regular 150, Bitmap effects 2. User interface: [x] Center dialog boxes when displayed,
  [ ] Show titles on floating dockers, [ ] Auto-execute single-item pop-up menus, [x] Enable sounds.
- Display: [ ] Proof colors by default. Miscellaneous controls: [x] Interruptible refresh,
  [ ] Manual refresh, [x] Use offscreen image, [x] Auto-panning, [x] Show tooltips;
  [ ] Show node tracking, [x] PostScript fills in enhanced view, [ ] Anti-aliased bitmaps in
  enhanced view, [ ] Hide bounding box for curve tools, [ ] Highlight outline of selected
  objects. Full-screen preview: (o) Draft view / (•) Enhanced view; [x] Show page border.
  Preview fountain steps: 256. Default mouse wheel action: Zoom. Pen tablet: Configure… (grayed out)
- Edit: General: Constrain angle 15.0 degrees; Drawing precision 3 decimal places; Min.
  extrude facet size 1.0 mm; Node size: Small (combo). [ ] Save facet size with document,
  [ ] Double-click to edit bitmaps in a bitmap editor, [x] Show curve direction while editing.
  SuperClip frame: Automatically center new SuperClip frame content: (•) When content lies completely
  outside the frame / ( ) Always / ( ) Never. Info note.
- Snap to Objects: [x] Enable Snap to Objects, Snap radius 10 pixels; [x] Show snap location
  marks, [x] Screen tip; [ ] Snap to page. Modes (list with checkmarks + icon): Node, Intersection,
  Midpoint, Quadrant, Tangent, Perpendicular, Edge, Center, Text baseline; buttons Select All / Deselect All.
- Dynamic Guides: [ ] Enable dynamic guides (Alt+Shift+D); [x] Snap to tick marks;
  [x] Angle screen tip; Spacing 2.5 mm; [x] Spacing screen tip. Guides:
  degree field 10°, list 0/30/45/60/90/120/135… with checkmarks (0, 45, 90, 135 on), Add/Delete/Select All/
  Deselect All, guide preview (rays). [x] Extend along segment. Note text.
- Alignment Guides: [ ] Alignment guides on (Alt+Shift+A); Color (color picker); show for:
  (•) Bounding box of a group / ( ) Individual objects of a group; [x] Align edges to center;
  [ ] Use margins as alignment guides; Margin settings (grayed out): Margin 5.0 mm, show for
  ( ) Margins only / (•) Margins and object.
- Warnings: list with checkmarks "Show warnings when:" (transparency/spot color, fill pattern not RGB,
  duotone, lens effects, distortion too complex, soft mask, effects on text with graphics, pasting
  and importing text, importing facing pages, 3DMF, target file format single pages, modifying
  existing guidelines …).
- Scripts: TANGENTA uses JavaScript ("Scripts (JavaScript)": scripts folder, time limit); there is no VBA.
- Save: Auto-backup: [x] Auto-backup every 20 minutes (combo); Default folder:
  (•) Temporary user folder / ( ) Specific folder [path] Browse…. Save text in previous
  version: (•) Preserve text appearance / ( ) Keep text editable; [x] Ask before saving.
  [x] Make backup on save.
- SuperTrace: Quick Trace: combo (Last used); Performance: slider Better performance … Higher
  quality; Color merging: (•) Average color merging / ( ) Merge with first selected color.
- Plugins: list of plugin folders with checkmarks; Add… / Remove.
- Text > Paragraph Text Frame: [x] Show text frame linking, [x] Show text frames, [ ] Expand and shrink
  paragraph text frames to fit text; Apply paragraph frame formatting when editing to:
  ( ) all linked frames / ( ) selected frames only / (•) selected and subsequent frames;
  [ ] Make all new paragraph text frames web-compatible.

## Document
- Guidelines > Vertical (Horizontal likewise): input field + unit (millimeters), list; Add,
  Move, Delete, Clear All.
- Text > Fonts: Font list contents: [x] TrueType fonts, [x] TrueType symbols, [x] Type 1 fonts,
  [x] Type 1 symbols; [ ] Show document fonts; [ ] Flyout with font sample next to font list;
  [x] Flyout with available font styles; [x] Use font to display font name; Number of
  recently used fonts 5. Font matching when
  opening; Panose font matching…. Insert character list contents: 4 checkmarks as above.
- Text > Spell Checking: [x] Perform automatic spell checking; Visibility: ( ) Errors in all
  text frames / (•) only in the selected one; Show 10 suggestions; [ ] Add corrections to QuickCorrect;
  [x] Show errors which have been ignored.
- Text > QuickCorrect: [ ] Capitalize first letter of sentences; [x] Correct two initial capitals;
  [x] Capitalize names of days; [x] Automatically hyperlink; Language: German (Switzerland); [x] Change
  straight quotes to typographic quotes; [x] Straight quotes after numbers; Single: open ‹ close ›; Double « »;
  [x] Replace text while typing; replacement table Replace/With, Add/Delete/Replace.
- Toolbox (tools): Dimension (style Decimal, precision 0.00, units mm, prefix, suffix,
  note), Shape Recognition (delay slider 1.00 seconds), Freehand/Bézier (freehand smoothing 50,
  corner threshold 5 px, straight line threshold 5 px, auto-join 5 px), Pick tool ([ ] Cross hair cursor,
  [x] Treat all objects as filled; Moving and transforming: [x] Redraw complex objects,
  delay 0.5 s; Ctrl/Shift: (•) Classic (Ctrl=constrain, Shift=from center) /
  ( ) Windows standard (Ctrl=duplicate, Shift=constrain)), Ellipse tool ((•) Ellipse ( ) Pie
  ( ) Arc; start angle 90, end angle 90; Clockwise/Counterclockwise grayed out), Eraser (thickness 1.0 mm,
  [x] Automatically reduce nodes of resulting objects), Rectangle (corner type 3 buttons, rectangle
  corners 4 radii + lock, scale corners), Table (rows 3, columns 4, Tab: ( ) Insert tab /
  (•) Move to next cell, order), Connector ([x] Geometric anchor points, spacing 8.0 mm),
  Mesh Fill (columns 2, rows 2), Knife, Graph Paper, Spiral, Angular Dimension, Zoom/Pan.
- Knife: [ ] Leave as one object; [x] Automatically close object.
- Graph Paper: number of cells width 4, height 3.
- Spiral: (•) Symmetrical / ( ) Logarithmic + preview image; number of revolutions 4; expansion 100 (slider,
  logarithmic only).
- Angular Dimension: precision 0.00, units °, prefix, suffix, note.
- Zoom/Pan: Right mouse button for Zoom: (•) Zoom out / ( ) Context menu; for the tool (Pan):
  (•) Zoom out / ( ) Context menu; [x] Zoom relative to 1:1; [ ] Center mouse when zooming with the mouse wheel;
  Calibrate rulers….
- Customization: Command Bars, Commands, Color Palette, Application.
- Customization > Command Bars: list with checkmarks (Menu Bar, Context Menu Bar, Status Bar, Standard,
  Property Bar, Toolbox, Text, Zoom, Internet, Print Merge, Transform, Macros, Layout); New,
  Reset. Size: Button (1 - Large…), Border 2; Default button appearance (Default);
  Other: [x] Title on floating toolbars, [x] Lock toolbars; Menu bar mode (Normal).
- Customization > Commands: category combo (File, …) + search; command list with icons; tabs General
  (tooltip help, Currently available for), Shortcut Keys (table Main, New shortcut key + Assign,
  Currently assigned to, [ ] Go to conflict on assign, Current shortcut keys + Delete, Reset
  All, View All…), Appearance (caption, image editor, Import, Restore defaults).
- Customization > Color Palette: [x] Automatically update document palette; Maximum number of palette rows
  when docked 1; Swatches: [ ] Wide borders, [ ] Large swatches, [x] Show 'No color' swatch; Right mouse button:
  ( ) Context menu / (•) Set outline color; tip text (hold button for 1 s = context menu).
- Customization > Application: [ ] Make user interface transparent; Make transparent (grayed out): command bars,
  dockers, user interface with color information; Transparency level 0..100% (slider).

## Document
- General: Basic settings: [ ] Fill open curves; [x] Automatically convert all bitmaps for bitmap
  effects; View mode: Enhanced. Duplicate placement: Horizontal 5.0 mm, Vertical 5.0 mm. Colors:
  Default color mode RGB + info.
- Page Size: Size and orientation: Size A4 (+ save/delete preset), Width 210.0, Height 297.0,
  unit millimeters, portrait/landscape; [ ] Apply size to current page only; [x] Show page border;
  Add page frame. Resolution: Rendering resolution 300 DPI. Bleed 0.0; [ ] Show bleed
  area.
- Layout: layout combo (Full Page, Book, Booklet, Tent Card, Side-Fold, Top-Fold,
  Parallel Fold), width/height, "One page per sheet", preview images; [ ] Facing pages; Start on
  (Right side, grayed out); info text.
- Label: (•) Normal paper / ( ) Labels; tree of label types (Ace Laser, AVERY …), preview,
  Customize label….
- Background: (•) No background / ( ) Solid [color] / ( ) Bitmap [Browse…]; Source: Linked /
  Embedded; Bitmap size: Default size / Custom size H/V + [x] Maintain aspect ratio;
  [x] Print and export background.
- Guidelines: [x] Show guidelines; [ ] Snap to guidelines; Default guideline color (blue);
  Default preset guideline color (green).
- Guidelines > Horizontal / Vertical: value field + unit, list of positions; Add, Move, Delete,
  Clear All; [x] Show guidelines, [ ] Snap to guidelines.
- Guidelines > Guidelines (slanted): list "(x,y) angle"; Specify: angle and 1 point (or 2 points); X, Y
  (millimeters), angle (degrees); Add/Move/Delete/Clear All.
- Guidelines > Presets: (•) Default presets / ( ) User defined; checkmarks: One-centimeter
  margins, Bleed area, Page borders, Printable area, Three-column newsletter, Basic grid,
  Upper left grid origin; Apply preset.
- Grid: Document grid: Horizontal 0.2 / Vertical 0.2 (grid lines per millimeter | spacing); [ ] Snap to
  grid; [ ] Show grid (•) As lines ( ) As dots. Baseline grid: spacing 14.0 (pt), color,
  start from top 15.0 mm, [ ] snap, [ ] show. Pixel grid: opacity slider 60.0, color,
  [x] Show grid (from 800%), [x] Snap to pixels; info (View > Pixels).
- Rulers: Nudging: nudge 0.1 (units mm), super nudge 2 x, micro
  nudge / 2; [x] Same units for duplicate distance, nudge and rulers. Units:
  Horizontal millimeters, Vertical (grayed out); [x] Same units for horizontal and vertical rulers.
  Origin: Horizontal 0.0, Vertical 0.0 mm. Tick divisions: 10 per unit; [x] Show fractions.
  [x] Show rulers. Edit scale… (drawing scale).
- Save: File optimization: [x] Bitmap compression, [x] Graphic object
  compression; Fill patterns: (•) Save with file / ( ) Rebuild on open; Blends and extrusions:
  ( ) Save with file / (•) Rebuild on open.
- Export to HTML: Position tolerance 0, Image white space 100, Position white space 4; Image map type:
  [x] Client, [ ] Server (NCSA/CERN).

## Global
- Printing: Default print style (Default); [ ] Print only current page by default; table
  Special settings (option/setting: spot color separations warning, preview colors, overprint black threshold
  95, Print large bitmaps in segments No, bitmap output threshold 4096, overlap pixels 32, font limit 8 …).
- Printing > Driver Compatibility: printer combo (installed printers); checkmarks: All text as graphics, software
  clipping for fills, bitmaps in 64 KB chunks, Bézier curves to driver, fountains to driver, printer can
  adjust document page sizes; note.
- Bitmap Effects: Preview method on open: Full screen / Before/After / Result only / (•) Last used;
  [x] Show last used values in the input fields.
- Filters: Available file types (tree raster/vector/text/animation), Add >> / << Remove, list of active
  filters, Up/Down, description, Reset.
- Filters > Associate: Associate file extension with (checkmark list AI, ARW, BMP, CAL, CDR, …), description,
  Reset.
- Color: Default spot color palette (PANTONE solid coated, HKS …); color palette mapping, PANTONE colors (Matching
  System Library); [x] Show "Select Color Palette" dialog box.
