// TANGENTA: Ersatz für boost::optional auf Basis von std::optional (mit get() wie bei Boost)
#pragma once
#include <optional>
#include <utility>

namespace boost {

template <typename T>
class optional : public std::optional<T>
{
public:
    using std::optional<T>::optional;
    using std::optional<T>::operator=;
    T &get() { return this->value(); }
    const T &get() const { return this->value(); }
};

template <typename T>
T &get(optional<T> &o) { return o.get(); }
template <typename T>
const T &get(const optional<T> &o) { return o.get(); }

} // namespace boost
