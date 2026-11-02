    // SPDX-License-Identifier: MPL-2.0
    // TANGENTA: ganze Abbildung lokal -> Seite (Zoll, y nach unten), damit gedrehter, gestauchter
    // und gespiegelter Text stimmt; dazu der Rahmen von Mengentext in lokalen Koordinaten
    {
      double px[3] = { 0.0, 1.0, 0.0 };
      double py[3] = { 0.0, 0.0, 1.0 };
      for (int k = 0; k < 3; ++k)
      {
        m_currentTransforms.applyToPoint(px[k], py[k]);
        if (!m_groupTransforms.empty())
          m_groupTransforms.top().applyToPoint(px[k], py[k]);
        px[k] -= m_page.offsetX;
        py[k] = m_page.height - (py[k] - m_page.offsetY);
      }
      textFrameProps.insert("tangenta:m11", px[1] - px[0], librevenge::RVNG_GENERIC);
      textFrameProps.insert("tangenta:m12", py[1] - py[0], librevenge::RVNG_GENERIC);
      textFrameProps.insert("tangenta:m21", px[2] - px[0], librevenge::RVNG_GENERIC);
      textFrameProps.insert("tangenta:m22", py[2] - py[0], librevenge::RVNG_GENERIC);
      textFrameProps.insert("tangenta:dx", px[0], librevenge::RVNG_GENERIC);
      textFrameProps.insert("tangenta:dy", py[0], librevenge::RVNG_GENERIC);
      if (!CDR_ALMOST_ZERO(m_currentTextBox.m_h) && !CDR_ALMOST_ZERO(m_currentTextBox.m_w))
      {
        textFrameProps.insert("tangenta:box-x", m_currentTextBox.m_x, librevenge::RVNG_GENERIC);
        textFrameProps.insert("tangenta:box-y", m_currentTextBox.m_y, librevenge::RVNG_GENERIC);
        textFrameProps.insert("tangenta:box-w", m_currentTextBox.m_w, librevenge::RVNG_GENERIC);
        textFrameProps.insert("tangenta:box-h", m_currentTextBox.m_h, librevenge::RVNG_GENERIC);
      }
    }
