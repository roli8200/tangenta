      case 2: // Gradient
        // TANGENTA: Verlauf vollständig weitergeben; libcdr gab konische Verläufe als lineare aus,
        // setzte beim radialen Verlauf die Mitte y = Mitte x und mehrfarbige radiale Verläufe linear.
        // Winkel in Grad gegen den Uhrzeigersinn, um die Drehung des Objekts ergänzt
        {
          const CDRGradient &g = m_currentFillStyle.gradient;
          double x0 = 0.0, y0 = 0.0, x1 = 1.0, y1 = 0.0;
          m_currentTransforms.applyToPoint(x0, y0);
          m_currentTransforms.applyToPoint(x1, y1);
          if (!m_groupTransforms.empty())
          {
            m_groupTransforms.top().applyToPoint(x0, y0);
            m_groupTransforms.top().applyToPoint(x1, y1);
          }
          propList.insert("tangenta:gradient-type", int(g.m_type));
          propList.insert("tangenta:gradient-angle", g.m_angle * 180.0 / M_PI + atan2(y1 - y0, x1 - x0) * 180.0 / M_PI);
          propList.insert("tangenta:gradient-edge", double(g.m_edgeOffset));
          propList.insert("tangenta:gradient-cx", double(g.m_centerXOffset));
          propList.insert("tangenta:gradient-cy", double(g.m_centerYOffset));
          propList.insert("tangenta:gradient-midpoint", g.m_midPoint);
          librevenge::RVNGPropertyListVector stops;
          for (const CDRGradientStop &stop : g.m_stops)
          {
            librevenge::RVNGPropertyList s;
            s.insert("svg:offset", stop.m_offset);
            s.insert("svg:stop-color", m_ps.getRGBColorString(stop.m_color));
            stops.append(s);
          }
          propList.insert("tangenta:gradient-stops", stops);
        }
