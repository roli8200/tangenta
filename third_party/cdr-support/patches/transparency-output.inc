  // TANGENTA: Transparenz für den Import weitergeben (siehe transparency-read.inc): Werte in Prozent,
  // Anfasser in Seitenkoordinaten (Zoll, y nach unten) – einmal aus den Punkten in der Zeichnung,
  // einmal aus den lokalen Punkten mit der Abbildung des Objekts
  {
    const TangentaTransparency &t = m_tangentaTransparency;
    if (t.kind)
    {
      propList.insert("tangenta:transparency-kind", int(t.kind));
      propList.insert("tangenta:transparency-subtype", int(t.subtype));
      propList.insert("tangenta:transparency-from", t.from);
      propList.insert("tangenta:transparency-to", t.to);
      propList.insert("tangenta:transparency-mode", int(t.mode));
      propList.insert("tangenta:transparency-reference", int(t.reference));
      // Verlauf der Deckkraft: Füllung mit Grauwerten (Farbe 0 bei radial aussen); Werte hier als
      // Transparenz in Prozent
      const auto fill = t.fill ? m_ps.m_fillStyles.find(t.fill) : m_ps.m_fillStyles.end();
      if (fill != m_ps.m_fillStyles.end() && fill->second.fillType == 2 && fill->second.gradient.m_stops.size() >= 2)
      {
        const CDRGradient &g = fill->second.gradient;
        double x0 = 0.0, y0 = 0.0, x1 = 1.0, y1 = 0.0;
        m_currentTransforms.applyToPoint(x0, y0);
        m_currentTransforms.applyToPoint(x1, y1);
        propList.insert("tangenta:transparency-gradient-type", int(g.m_type));
        propList.insert("tangenta:transparency-gradient-angle", g.m_angle * 180.0 / M_PI + atan2(y1 - y0, x1 - x0) * 180.0 / M_PI);
        propList.insert("tangenta:transparency-gradient-edge", double(g.m_edgeOffset));
        propList.insert("tangenta:transparency-gradient-cx", double(g.m_centerXOffset));
        propList.insert("tangenta:transparency-gradient-cy", double(g.m_centerYOffset));
        librevenge::RVNGPropertyListVector stops;
        for (const CDRGradientStop &stop : g.m_stops)
        {
          const unsigned rgb = std::strtoul(m_ps.getRGBColorString(stop.m_color).cstr() + 1, nullptr, 16);
          librevenge::RVNGPropertyList s;
          s.insert("svg:offset", stop.m_offset);
          s.insert("tangenta:transparency", 100.0 - ((rgb >> 8) & 0xff) / 2.55);
          stops.append(s);
        }
        propList.insert("tangenta:transparency-stops", stops);
      }
      if (t.handles)
      {
        for (int i = 0; i < 2; ++i)
        {
          double x = t.world[2 * i], y = t.world[2 * i + 1];
          x -= m_page.offsetX;
          y = m_page.height - (y - m_page.offsetY);
          propList.insert(i ? "tangenta:transparency-x2" : "tangenta:transparency-x1", x);
          propList.insert(i ? "tangenta:transparency-y2" : "tangenta:transparency-y1", y);
          double lx = t.local[2 * i], ly = t.local[2 * i + 1];
          m_currentTransforms.applyToPoint(lx, ly);
          if (!m_groupTransforms.empty())
            m_groupTransforms.top().applyToPoint(lx, ly);
          lx -= m_page.offsetX;
          ly = m_page.height - (ly - m_page.offsetY);
          propList.insert(i ? "tangenta:transparency-lx2" : "tangenta:transparency-lx1", lx);
          propList.insert(i ? "tangenta:transparency-ly2" : "tangenta:transparency-ly1", ly);
        }
      }
    }
  }
