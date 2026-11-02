#pragma once

#include "Node.h"

#include <QJsonObject>

#include <functional>
#include <memory>

namespace vek {

// Knoten <-> JSON. Ein Knoten wird zu {"type", "id", <Eigenschaften>, "children": [...]}.
// Dieselbe Darstellung dient dem Dateiformat und dem Undo-Journal.
namespace serial {

enum class Ids { Keep, Drop };

// Ganzer Teilbaum
QJsonObject toJson(const Node& node, Ids ids = Ids::Keep);
// Nur eigene Eigenschaften, ohne Kinder und ohne Typ
QJsonObject properties(const Node& node);

// Liefert nullptr bei unbekanntem Typ oder verletzter Strukturregel (error gesetzt).
// Mit Ids::Drop bekommen alle Knoten die Id 0 und erhalten beim Einfügen neue.
std::unique_ptr<Node> fromJson(const QJsonObject& json, Ids ids = Ids::Keep, QString* error = nullptr);

using Factory = std::function<std::unique_ptr<Node>()>;
// Neue Objekttypen melden sich hier an; die eingebauten sind bereits registriert.
void registerType(const QString& typeName, Factory factory);

} // namespace serial

} // namespace vek
