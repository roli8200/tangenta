    else if (argTypes[i] == 0x4ace) // TANGENTA: Maschenfüllung
    {
      // Aufbau: Länge, 0x96, Knoten je Spalte und je Zeile; danach eine Linie je Zeile und je Spalte
      // (0x96, Anzahl Punkte 3·(Knoten−1)+1, Punkte, Punkttypen, Zusatzdaten) und zuletzt
      // 0x96, Zeilen, Spalten mit einem Datensatz je Knoten (Länge, Daten). Die Farbe des Knotens ist
      // der letzte Farbeintrag (01 0c 00 00 00, Farbe) im Datensatz; gemischte Farben führen ihre
      // Bestandteile davor auf.
      const unsigned len = readU32(input);
      input->seek(-4, librevenge::RVNG_SEEK_CUR);
      unsigned long have = 0;
      const unsigned char *raw = len >= 16 && len <= length ? input->read(len, have) : nullptr;
      if (raw && have == len)
      {
        const std::vector<unsigned char> b(raw, raw + have);
        const size_t none = size_t(-1);
        auto u32 = [&b](size_t o) -> unsigned
        {
          return o + 4 <= b.size() ? unsigned(b[o]) | unsigned(b[o + 1]) << 8 | unsigned(b[o + 2]) << 16 | unsigned(b[o + 3]) << 24 : 0u;
        };
        auto find = [&](size_t from, unsigned first, unsigned second) -> size_t
        {
          for (size_t o = from; o + 8 <= b.size(); ++o)
            if (u32(o) == first && u32(o + 4) == second)
              return o;
          return none;
        };
        TangentaMesh mesh;
        mesh.rows = u32(8);
        mesh.columns = u32(12);
        bool ok = mesh.rows >= 2 && mesh.columns >= 2 && mesh.rows <= 100 && mesh.columns <= 100;
        // Knoten auf den Linien (jeder dritte Punkt); Randlinien folgen dem Umriss und tragen dessen
        // zusätzliche Knoten. Ein Knoten der Masche ist der Punkt einer Zeile, der auch auf der
        // Spalte liegt.
        std::vector<std::vector<std::pair<int, int> > > lines;
        size_t o = 16;
        while (ok && lines.size() < size_t(mesh.rows) + mesh.columns)
        {
          while (o + 8 <= b.size() && !(u32(o) == 0x96 && u32(o + 4) >= 4 && u32(o + 4) <= 3000 && (u32(o + 4) - 1) % 3 == 0))
            ++o;
          const unsigned count = o + 8 <= b.size() ? u32(o + 4) : 0;
          if (!count || o + 8 + 9 * size_t(count) > b.size())
          {
            ok = false;
            break;
          }
          std::vector<std::pair<int, int> > line;
          for (unsigned n = 0; n < count; n += 3)
            line.push_back(std::make_pair(int(u32(o + 8 + 8 * size_t(n))), int(u32(o + 12 + 8 * size_t(n)))));
          lines.push_back(line);
          o += 8 + 9 * size_t(count);
        }
        for (unsigned r = 0; ok && r < mesh.rows; ++r)
          for (unsigned c = 0; c < mesh.columns; ++c)
          {
            std::pair<int, int> best(0, 0);
            double bestDistance = -1.0;
            for (const auto &p : lines[r])
              for (const auto &q : lines[mesh.rows + c])
              {
                const double dx = double(p.first) - q.first, dy = double(p.second) - q.second;
                if (bestDistance < 0.0 || dx * dx + dy * dy < bestDistance)
                {
                  bestDistance = dx * dx + dy * dy;
                  best = p;
                }
              }
            mesh.nodes.push_back(std::make_pair(best.first / 254000.0, best.second / 254000.0));
          }
        // Farben: 0x96, Spalten, Zeilen, dann ein Datensatz je Knoten (zeilenweise)
        while (ok)
        {
          o = find(o, 0x96, mesh.columns);
          if (o == none)
            ok = false;
          else if (u32(o + 8) == mesh.rows)
            break;
          else
            ++o;
        }
        size_t p = ok ? o + 12 : 0;
        for (size_t n = 0; ok && n < mesh.nodes.size(); ++n)
        {
          const size_t end = p + 4 + u32(p);
          if (end > b.size() || end <= p + 4)
          {
            ok = false;
            break;
          }
          size_t c = none;
          for (size_t q = p + 4; q + 21 <= end; ++q)
            if (b[q] == 1 && u32(q + 1) == 12)
              c = q + 5;
          if (c == none)
          {
            ok = false;
            break;
          }
          CDRColor color;
          color.m_colorModel = (unsigned short)(b[c] | b[c + 1] << 8);
          if (color.m_colorModel == 0x01 && m_version >= 1300)
            color.m_colorModel = 0x19;
          if (color.m_colorModel == 0x1e)
          {
            color.m_colorModel = 0x19;
            color.m_colorPalette = 0x1e;
            color.m_colorValue = u32(c + 2);
          }
          else
          {
            color.m_colorPalette = (unsigned short)(b[c + 2] | b[c + 3] << 8);
            color.m_colorValue = u32(c + 8);
          }
          mesh.colors.push_back(color);
          p = end;
        }
        if (ok)
          m_collector->collectTangentaMesh(mesh);
      }
    }
