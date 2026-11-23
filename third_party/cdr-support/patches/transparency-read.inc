void libcdr::CDRParser::readOpacity(librevenge::RVNGInputStream *input, unsigned /* length */)
{
  // TANGENTA: Transparenz (Argument 8000 eines Objekts). libcdr las hier nur eine Zahl an fester
  // Stelle als Deckkraft; das traf nur die einfache Transparenz älterer Dateien, alle anderen
  // Fassungen ergaben Unsinn (oft 0, das Objekt verschwand). Aufbau, aus Dateien ermittelt:
  //  - bis Version 12: u32 Art; Art 1 = einfache Transparenz (u16 Unterart, u16 Wert in Promille),
  //    Art 5 = interaktive Transparenz (Aufbau wie unten, Kopf 12 Bytes)
  //  - ab Version 13: 8 Bytes Kopf (Kennung, Länge), dann u8 Art 5 und 8 Bytes, also Kopf 17 Bytes
  //  - interaktive Transparenz ab dem Kopf: u32 «Anfasser vorhanden», 4 Koordinaten (Anfang und
  //    Ende in der Zeichnung), u32 1, u32 Nummer, u32 1, 4 Koordinaten (lokal), u32 -1,
  //    double Anfangswert und double Endwert in Prozent, u32 Mischmodus, u32 Wirkung, 20 Bytes,
  //    u32 Nummer der Füllung, deren Grauwerte den Verlauf der Deckkraft beschreiben (255 = deckend)
  const long start = input->tell();
  TangentaTransparency t;
  unsigned head = 0;
  if (m_version < 1300)
  {
    const unsigned kind = readU32(input);
    if (kind == 1)
    {
      input->seek(4, librevenge::RVNG_SEEK_CUR);
      t.subtype = readU16(input);
      t.from = readU16(input) / 10.0;
      t.to = t.from;
      t.kind = 1;
      m_collector->collectTangentaTransparency(t);
      return;
    }
    if (kind != 5)
      return;
    head = 12;
  }
  else
  {
    input->seek(8, librevenge::RVNG_SEEK_CUR);
    if (readU8(input) != 5)
      return;
    head = 17;
  }
  input->seek(start + head, librevenge::RVNG_SEEK_SET);
  t.kind = 5;
  t.handles = readU32(input) != 0;
  for (double &v : t.world)
    v = readCoordinate(input);
  input->seek(4, librevenge::RVNG_SEEK_CUR);
  t.reference = readU32(input);
  input->seek(4, librevenge::RVNG_SEEK_CUR);
  for (double &v : t.local)
    v = readCoordinate(input);
  input->seek(4, librevenge::RVNG_SEEK_CUR);
  t.from = readDouble(input);
  t.to = readDouble(input);
  t.mode = readU32(input);
  t.target = readU32(input);
  input->seek(20, librevenge::RVNG_SEEK_CUR);
  t.fill = readU32(input);
  m_collector->collectTangentaTransparency(t);
}

