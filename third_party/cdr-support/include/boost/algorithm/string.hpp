// TANGENTA: Ersatz für Boost.StringAlgo (nur trim und starts_with, wie libcdr und librevenge sie nutzen)
#pragma once
#include <cctype>
#include <cstring>
#include <string>

namespace boost {

inline void trim(std::string &s)
{
    std::size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b])))
        ++b;
    std::size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1])))
        --e;
    s = s.substr(b, e - b);
}

inline bool starts_with(const std::string &s, const char *prefix)
{
    const std::size_t n = std::strlen(prefix);
    return s.size() >= n && s.compare(0, n, prefix) == 0;
}

} // namespace boost
