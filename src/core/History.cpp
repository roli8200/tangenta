#include "History.h"

#include <algorithm>

namespace vek {

void History::push(ChangeSet set)
{
    if (m_cleanIndex > static_cast<std::ptrdiff_t>(m_cursor))
        m_cleanIndex = -1; // der gespeicherte Stand lag im verworfenen Redo-Zweig
    m_sets.erase(m_sets.begin() + static_cast<std::ptrdiff_t>(m_cursor), m_sets.end());
    m_sets.push_back(std::move(set));
    m_cursor = m_sets.size();
    ++m_pushed;
    trim();
}

bool History::merge(std::size_t mark, const QString& label)
{
    if (mark >= m_pushed || m_cursor != m_sets.size())
        return false;
    const std::size_t count = std::min(m_pushed - mark, m_cursor);
    if (count < 2) {
        if (count == 1 && !label.isEmpty())
            m_sets.back().label = label;
        return count == 1;
    }
    const std::size_t from = m_cursor - count;
    ChangeSet merged;
    merged.label = label.isEmpty() ? m_sets[from].label : label;
    for (std::size_t i = from; i < m_cursor; ++i)
        for (Change& c : m_sets[i].changes)
            merged.changes.push_back(std::move(c));
    m_sets.erase(m_sets.begin() + static_cast<std::ptrdiff_t>(from), m_sets.end());
    m_sets.push_back(std::move(merged));
    if (m_cleanIndex > static_cast<std::ptrdiff_t>(from))
        m_cleanIndex = -1; // gespeicherter Stand lag mitten in den zusammengefassten Schritten
    m_cursor = m_sets.size();
    return true;
}

void History::setLimit(std::size_t limit)
{
    m_limit = limit;
    trim();
}

void History::trim()
{
    if (m_limit == 0 || m_sets.size() <= m_limit)
        return;
    const std::size_t drop = m_sets.size() - m_limit;
    m_sets.erase(m_sets.begin(), m_sets.begin() + static_cast<std::ptrdiff_t>(drop));
    m_cursor = m_cursor > drop ? m_cursor - drop : 0;
    if (m_cleanIndex >= 0)
        m_cleanIndex = m_cleanIndex >= static_cast<std::ptrdiff_t>(drop) ? m_cleanIndex - static_cast<std::ptrdiff_t>(drop) : -1;
}

void History::clear()
{
    m_sets.clear();
    m_cursor = 0;
    m_cleanIndex = 0;
}

} // namespace vek
