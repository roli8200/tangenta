# Architektur

Grundlage ist die Architekturreferenz in `K:\Claude\vektoreditor-architektur\index.html`.
Hier steht, was davon umgesetzt ist und wo bewusst abgewichen wurde.

## Dokumentmodell

Ein Baum aus `Node`-Objekten, den nur das `Document` besitzt:

    document
    ├── masterPage      globaler Objektbaum, auf jeder Seite sichtbar
    │   └── layer
    │       └── DrawObject …
    └── page …          seitenspezifischer Objektbaum
        └── layer
            └── DrawObject …

- `canContain()` legt fest, was wohin darf (Seiten nur unter der Wurzel, Ebenen nur in
  Seiten, Zeichenobjekte in Ebenen und Gruppen).
- Jeder Knoten hat eine dokumentweit eindeutige, stabile `NodeId`. Sie bleibt über Undo,
  Redo, Speichern und Laden gleich. Werkzeuge und Oberfläche merken sich Ids, nie Zeiger.
- Alle Masse in mm, Ursprung oben links auf der Seite, y nach unten.

## Zeichenobjekte

Objektorientiert: alles, was man zeichnen kann, erbt von `DrawObject`. Ein Objekt ist eine
Grundform in lokalen Koordinaten (`localPath()`) plus eine affine Transformation. Skalieren,
Strecken, Drehen und Neigen ändern nur die Transformation; ein gedrehtes Rechteck bleibt ein
Rechteck mit Breite und Höhe. Die Griffe sitzen nach dem
Drehen auf dem achsparallelen Begrenzungsrechteck, und Strecken eines gedrehten Rechtecks
ergibt ein Parallelogramm.

Eine Gruppe hat keine eigene Transformation; Transformationen wirken direkt auf die Kinder.

Ein neuer Objekttyp braucht: Unterklasse von `DrawObject`, `typeName()`, `localPath()`,
`writeProperties()`/`readProperties()` und einen Eintrag in `Serialization.cpp`.

## Undo/Redo

Kein Befehlsmuster mit selbst geschriebenen Inversen. Jede Änderung läuft durch eine
`Transaction` und wird dabei als Daten festgehalten:

- `insert` merkt sich den eingefügten Teilbaum,
- `remove` den entfernten Teilbaum samt Position,
- `modify` die Eigenschaften vorher und nachher.

Undo spielt diese Daten rückwärts ab, Redo vorwärts. Grundlage ist dieselbe Serialisierung
wie im Dateiformat: Was ein Objekt nicht speichert, kann es auch nicht rückgängig machen,
und die Tests decken beides gemeinsam ab. Eine Transaktion ohne `commit()` nimmt ihre
Änderungen im Destruktor zurück.

Werkzeuge ändern das Dokument während des Ziehens nicht. Sie zeigen eine Vorschau und
schreiben beim Loslassen genau einen Undo-Schritt.

## Dateiformat (.vek)

ZIP-Container (miniz, im Repo unter `third_party/miniz`):

    mimetype        application/x-vektoreditor, unkomprimiert, erster Eintrag
    manifest.json   Format und Formatversion
    document.json   Knotenbaum als JSON

Dateien aus neueren Formatversionen werden abgelehnt statt halb gelesen. Gespeichert wird
über `QSaveFile`, also nie ein halbes Dokument auf der Platte.

## Farbe

Nur RGB (sRGB, 8 Bit je Kanal, mit Deckkraft). Abweichung von der Architekturreferenz:
CMYK, Schmuckfarben und Separationen sind nicht vorgesehen. Damit entfällt auch die
Trennung in Bildschirm- und Export-Renderer vorerst.

## Rendering

QPainter statt Skia. Für RGB reicht das, und es spart den aufwendigen Skia-Build auf beiden
Plattformen. Die Zeichenfläche ruft nur `path()` und `style()` ab; ein Wechsel auf Skia bleibt
auf `Canvas::paintObject` beschränkt.

## Lizenz

GPL-3.0-or-later. miniz steht unter MIT und ist damit verträglich.

## Eigenschaftsleiste und objekteigene Bearbeitung

Jeder Objekttyp beschreibt selbst, wie man ihn bearbeitet: `DrawObject::propertyDescriptors()`
liefert eine Liste von `PropertyDescriptor` (Länge, Winkel, Ein/Aus, Auswahl, Befehl) mit Symbol
und Tooltip, `propertyValue()`/`setPropertyValue()`/`invokeAction()` setzen sie um. Die
Eigenschaftsleiste baut daraus ihre Bedienelemente; jede Änderung läuft als Transaktion.
Ein neuer Objekttyp bekommt so seine eigene Leiste, ohne dass die Oberfläche ihn kennt.

Das Rechteck ist das erste Beispiel: Eckentyp (rund, ausgebuchtet, abgeschrägt), vier
Eckenradien, gekoppelt oder einzeln. Wie gewohnt wandert beim Skalieren eines nicht
geneigten Rechtecks die Skalierung in Breite und Höhe, damit die Radien in mm erhalten bleiben.

## Hilfslinien und Einrasten

Hilfslinien liegen auf der Masterseite (gelten für alle Seiten), werden mitgespeichert und
sind rückgängig machbar. Die `SnapEngine` rastet an Objektknoten, Objektmitten, Objekträndern,
Hilfslinien und Seitenrändern/-mitte ein; der Fangradius ist in Bildschirmpixeln festgelegt.

## Füllungen und Farbverläufe

`Fill` kennt keine, gleichmässige und Verlaufsfüllungen (linear, radial, konisch) mit beliebig
vielen Farbstopps. Die Verlaufsachse liegt relativ zum lokalen Begrenzungsrechteck der Grundform
(0..1), dadurch dreht, skaliert und neigt sich der Verlauf mit dem Objekt. `GradientGeometry`
übersetzt diese Achse in die Begriffe des klassischen Dialogs (Winkel, Randauffüllung,
Mittelpunktverschiebung) und zurück. Die Füllung ist Teil von `Style` und gilt damit für jedes
Zeichenobjekt; das interaktive Füllwerkzeug (G) zieht die Achse direkt auf dem Objekt.

## Symbole

Eigene Zeichnungen, erzeugt von `tools/make-icons.py` nach `resources/icons/` und in die
Anwendung eingebettet. Neue Symbole dort ergänzen und das Skript erneut ausführen; der Test
`allIconsRender` prüft, dass jedes Symbol gültig ist.
