#pragma once

class QMenu;
class QMenuBar;

namespace vek {

// Symbole der Menüeinträge, die beim Anlegen keins bekommen haben: Tabelle aus Elternmenü und
// Eintrag (Quelltexte, über die aktive Übersetzung verglichen, also in jeder Sprache gleich).
// Bitmap-Effekte bekommen ihr Symbol über die Effekt-ID (MainWindow::addBitmapEffectAction).
// Auswahllisten (Sprachen, Oberflächengrösse in Prozent, offene Fenster) tragen die Eigenschaft
// "dynamicEntry" und bleiben ohne Symbol.
void assignMenuIcons(QMenuBar* bar);
void assignMenuIcons(QMenu* menu);

} // namespace vek
