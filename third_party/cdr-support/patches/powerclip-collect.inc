// TANGENTA: PowerClip. Die Inhalte eines PowerClips stehen nicht auf der Seite, sondern als Gruppe
// im Abschnitt "clpt" des Dokuments, gekennzeichnet mit einer Nummer (spnd). Das Objekt auf der
// Seite (der Rahmen) verweist mit Argument 8005 auf diese Nummer. libcdr behandelte "clpt" wie ein
// Vektormuster und gab die Inhalte nie aus. Hier werden sie gesammelt (in Seitenkoordinaten der
// ersten Seite) und beim Rahmen als Gruppe "tangenta:powerclip" ausgegeben: zuerst der Rahmen,
// danach die Inhalte. Der Import macht daraus einen SuperClip.
void libcdr::CDRContentCollector::collectTangentaClipList(unsigned level)
{
  m_currentVectLevel = level;
  m_tangentaClipLevel = level;
  m_spnd = 0; // die Nummer dieser Inhalte folgt (spnd); eine ältere darf sie nicht verdecken
  m_outputElementsStack = &m_tangentaClipStack;
  m_outputElementsQueue = &m_tangentaClipQueue;
  if (!m_ps.m_pages.empty())
    m_page = m_ps.m_pages[0];
}

void libcdr::CDRContentCollector::collectTangentaClip(unsigned id)
{
  if (m_currentObjectLevel)
    m_tangentaClipId = id;
}

void libcdr::CDRContentCollector::_tangentaStoreClip()
{
  std::vector<CDROutputElementList> &contents = m_tangentaClips[m_spnd];
  contents.clear();
  while (!m_tangentaClipStack.empty())
  {
    contents.push_back(m_tangentaClipStack.top());
    m_tangentaClipStack.pop();
  }
  while (!m_tangentaClipQueue.empty())
  {
    contents.push_back(m_tangentaClipQueue.front());
    m_tangentaClipQueue.pop();
  }
  m_spnd = 0;
}

void libcdr::CDRContentCollector::_tangentaPushObject(const CDROutputElementList &outputElement)
{
  const auto clip = m_tangentaClipId ? m_tangentaClips.find(m_tangentaClipId) : m_tangentaClips.end();
  if (clip == m_tangentaClips.end() || clip->second.empty())
  {
    if (m_reverseOrder)
      m_outputElementsStack->push(outputElement);
    else
      m_outputElementsQueue->push(outputElement);
    return;
  }
  // Zeichenreihenfolge: Gruppe auf, Rahmen, Inhalte, Gruppe zu. Der Stapel wird von oben gelesen,
  // darum dort umgekehrt ablegen.
  librevenge::RVNGPropertyList groupProps;
  groupProps.insert("tangenta:powerclip", true);
  CDROutputElementList open, close;
  open.addStartGroup(groupProps);
  close.addEndGroup();
  if (m_reverseOrder)
  {
    m_outputElementsStack->push(close);
    for (auto it = clip->second.rbegin(); it != clip->second.rend(); ++it)
      m_outputElementsStack->push(*it);
    m_outputElementsStack->push(outputElement);
    m_outputElementsStack->push(open);
  }
  else
  {
    m_outputElementsQueue->push(open);
    m_outputElementsQueue->push(outputElement);
    for (const CDROutputElementList &content : clip->second)
      m_outputElementsQueue->push(content);
    m_outputElementsQueue->push(close);
  }
}

