// SPDX-License-Identifier: MPL-2.0
// TANGENTA: ersetzt beim Bauen die Boost.Spirit-Grammatiken in CDRParser.cpp durch Code von Hand
// (gleiche Bedeutung: Schriftnamen mit Codepage-Endung, Farbangaben "Modell,Palette,Werte,GUID[,~,…]").
static void processNameForEncoding(librevenge::RVNGString &name, unsigned short &encoding)
{
  static const std::pair<const char *, unsigned short> suffixes[] =
  {
    {" Cyrillic", 0xcc}, {" Hebrew", 0xb1}, {" Arabic", 0xb2}, {" Baltic", 0xba}, {" Greek", 0xa1},
    {" Thai", 0xde}, {" Cyr", 0xcc}, {" CYR", 0xcc}, {" Tur", 0xa2}, {" TUR", 0xa2}, {" CE", 0xee}
  };
  const std::string fontName(name.cstr());
  for (const auto &suffix : suffixes)
  {
    const size_t n = std::strlen(suffix.first);
    if (fontName.size() > n && fontName.compare(fontName.size() - n, n, suffix.first) == 0)
    {
      encoding = suffix.second;
      name = fontName.substr(0, fontName.size() - n).c_str();
      return;
    }
  }
  if (fontName.compare(0, 4, "GOST") == 0)
    encoding = 0xcc;
}

static bool cdrAllOf(const std::string &s, int (*pred)(int))
{
  if (s.empty())
    return false;
  for (char c : s)
    if (!pred((unsigned char)c))
      return false;
  return true;
}

static bool cdrIsGuid(const std::string &s)
{
  if (s.size() != 36)
    return false;
  for (size_t i = 0; i < s.size(); ++i)
  {
    const bool dash = i == 8 || i == 13 || i == 18 || i == 23;
    if (dash ? s[i] != '-' : !std::isxdigit((unsigned char)s[i]))
      return false;
  }
  return true;
}

static int parseColourString(const char *colourString, libcdr::CDRColor &colour, double &opacity)
{
  bool bRes = false;

  boost::optional<unsigned> colourModel, colourPalette;
  std::vector<std::string> fallbackColours;
  std::vector<unsigned> val;

  if (colourString)
  {
    static const std::pair<const char *, unsigned> cmodel[] =
    {
      {"CMYK", 2}, {"CMYK255", 3}, {"RGB255", 5}, {"HSB", 6}, {"HLS", 7}, {"GRAY255", 9}, {"YIQ255", 11},
      {"LAB", 12}, {"PANTONEHX", 14}, {"LAB255", 18}, {"REGCOLOR", 20}, {"SPOT", 25}
    };
    static const std::pair<const char *, unsigned> cpalette[] =
    {
      {"USER", 5}, {"FOCTONE", 8}, {"DUPONT", 9}, {"TOYO", 10}, {"DIC", 11}, {"PAN_HEX", 12},
      {"HKS", 0x17}, {"HKS_K", 0x1a}, {"HKS_N", 0x1b}, {"HKS_Z", 0x1c}, {"HKS_E", 0x1d}
    };
    std::string s;
    for (const char *c = colourString; *c; ++c)
      if (!std::isspace((unsigned char)*c))
        s += *c;
    size_t p = 0;
    // Modell und Palette: bekannter Name oder irgendein Wort aus Buchstaben und Ziffern, dann ein Komma
    auto word = [&](const std::pair<const char *, unsigned> *table, size_t count, boost::optional<unsigned> &out)
    {
      const size_t q = s.find(',', p);
      if (q == std::string::npos)
        return false;
      const std::string w = s.substr(p, q - p);
      p = q + 1;
      for (size_t i = 0; i < count; ++i)
        if (w == table[i].first)
        {
          out.emplace(table[i].second);
          return true;
        }
      return cdrAllOf(w, ::isalnum);
    };
    bool ok = word(cmodel, sizeof(cmodel) / sizeof(cmodel[0]), colourModel)
              && word(cpalette, sizeof(cpalette) / sizeof(cpalette[0]), colourPalette);
    // Zahlen mit Komma, dann die GUID
    while (ok)
    {
      const size_t q = s.find(',', p);
      const std::string t = s.substr(p, q == std::string::npos ? std::string::npos : q - p);
      if (q != std::string::npos && cdrAllOf(t, ::isdigit))
      {
        val.push_back((unsigned)std::strtoul(t.c_str(), nullptr, 10));
        p = q + 1;
        continue;
      }
      ok = cdrIsGuid(t);
      p = q == std::string::npos ? s.size() : q;
      break;
    }
    // Optional: ",~,Name,Zahl," und Ersatzfarben, jeweils mit ",~," abgeschlossen; der Rest zählt nicht
    if (ok && p < s.size())
    {
      ++p;
      if (s.compare(p, 2, "~,") == 0)
      {
        p += 2;
        const size_t nameEnd = s.find(',', p);
        ok = nameEnd != std::string::npos && nameEnd > p;
        if (ok)
        {
          p = nameEnd + 1;
          const size_t numberEnd = s.find(',', p);
          ok = numberEnd != std::string::npos && cdrAllOf(s.substr(p, numberEnd - p), ::isdigit);
          if (ok)
          {
            p = numberEnd + 1;
            for (;;)
            {
              const size_t end = s.find(",~,", p);
              if (end == std::string::npos || end == p)
                break;
              fallbackColours.push_back(s.substr(p, end - p));
              p = end + 3;
            }
          }
        }
      }
    }
    bRes = ok;
  }

