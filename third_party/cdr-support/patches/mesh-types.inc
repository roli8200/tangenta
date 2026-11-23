// TANGENTA: Maschenfüllung eines Objekts, gelesen in mesh-read.inc
struct TangentaMesh
{
  unsigned rows = 0;                               // Knoten je Spalte
  unsigned columns = 0;                            // Knoten je Zeile
  std::vector<std::pair<double, double> > nodes;   // zeilenweise, lokal zum Objekt (Zoll)
  std::vector<CDRColor> colors;                    // je Knoten
};

