          // TANGENTA: Text ohne Füllung und Kontur der Zeichen weitergeben (Breite in Zoll)
          if (j.m_style.m_fillStyle.fillType == 0)
            spanProps.insert("tangenta:fill-none", true);
          if (j.m_style.m_lineStyle.lineType != (unsigned short)-1 && !(j.m_style.m_lineStyle.lineType & 0x1))
          {
            spanProps.insert("tangenta:stroke-color", m_ps.getRGBColorString(j.m_style.m_lineStyle.color));
            spanProps.insert("tangenta:stroke-width", j.m_style.m_lineStyle.lineWidth);
          }
