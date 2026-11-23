// TANGENTA: Transparenz eines Objekts, gelesen in transparency-read.inc
struct TangentaTransparency
{
  unsigned kind = 0;               // 1 einfache Transparenz (ältere Dateien), 5 interaktive Transparenz
  unsigned subtype = 0;            // nur Art 1
  bool handles = false;            // Verlauf mit Anfang und Ende
  double world[4] = {0, 0, 0, 0};  // Anfang und Ende in der Zeichnung (Zoll)
  double local[4] = {0, 0, 0, 0};  // dieselben Punkte lokal zum Objekt
  unsigned reference = 0;
  double from = 0.0;               // Prozent am Anfang bzw. gleichmässig
  double to = 0.0;                 // Prozent am Ende
  unsigned mode = 0;               // Mischmodus
  unsigned target = 0;             // Wirkung (Füllung, Kontur, beides)
  unsigned fill = 0;               // Füllung mit dem Verlauf der Deckkraft (Grauwerte)
};

