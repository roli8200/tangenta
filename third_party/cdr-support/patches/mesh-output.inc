  // TANGENTA: Maschenfüllung für den Import weitergeben (siehe mesh-read.inc): Knoten zeilenweise in
  // Seitenkoordinaten (Zoll, y nach unten) mit ihrer Farbe
  if (!m_tangentaMesh.nodes.empty() && m_tangentaMesh.nodes.size() == m_tangentaMesh.colors.size())
  {
    propList.insert("tangenta:mesh-rows", int(m_tangentaMesh.rows));
    propList.insert("tangenta:mesh-columns", int(m_tangentaMesh.columns));
    librevenge::RVNGPropertyListVector nodes;
    for (size_t n = 0; n < m_tangentaMesh.nodes.size(); ++n)
    {
      double x = m_tangentaMesh.nodes[n].first, y = m_tangentaMesh.nodes[n].second;
      m_currentTransforms.applyToPoint(x, y);
      if (!m_groupTransforms.empty())
        m_groupTransforms.top().applyToPoint(x, y);
      x -= m_page.offsetX;
      y = m_page.height - (y - m_page.offsetY);
      librevenge::RVNGPropertyList node;
      node.insert("svg:x", x);
      node.insert("svg:y", y);
      node.insert("tangenta:color", m_ps.getRGBColorString(m_tangentaMesh.colors[n]));
      nodes.append(node);
    }
    propList.insert("tangenta:mesh-nodes", nodes);
  }
