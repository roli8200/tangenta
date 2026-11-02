#include "Serialization.h"

#include "CurveObject.h"
#include "EllipseObject.h"
#include "PolygonObject.h"
#include "GroupObject.h"
#include "ImageObject.h"
#include "Page.h"
#include "RectangleObject.h"
#include "PerfectShape.h"
#include "TextObject.h"
#include "DimensionObject.h"
#include "TableObject.h"
#include "CloneObject.h"
#include "SuperClipObject.h"
#include "BlendObject.h"
#include "ConnectorObject.h"

#include <QHash>
#include <QJsonArray>

namespace vek {

namespace serial {

namespace {

const QString kType = QStringLiteral("type");
const QString kId = QStringLiteral("id");
const QString kChildren = QStringLiteral("children");

QHash<QString, Factory>& registry()
{
    static QHash<QString, Factory> types = [] {
        QHash<QString, Factory> t;
        t.insert(QStringLiteral("document"), [] { return std::make_unique<DocumentRoot>(); });
        t.insert(QStringLiteral("masterPage"), [] { return std::make_unique<Page>(true); });
        t.insert(QStringLiteral("page"), [] { return std::make_unique<Page>(false); });
        t.insert(QStringLiteral("layer"), [] { return std::make_unique<Layer>(); });
        t.insert(QStringLiteral("group"), [] { return std::make_unique<GroupObject>(); });
        t.insert(QStringLiteral("rectangle"), [] { return std::make_unique<RectangleObject>(); });
        t.insert(QStringLiteral("perfectshape"), [] { return std::make_unique<PerfectShapeObject>(); });
        t.insert(QStringLiteral("curve"), [] { return std::make_unique<CurveObject>(); });
        t.insert(QStringLiteral("ellipse"), [] { return std::make_unique<EllipseObject>(); });
        t.insert(QStringLiteral("polygon"), [] { return std::make_unique<PolygonObject>(); });
        t.insert(QStringLiteral("text"), [] { return std::make_unique<TextObject>(); });
        t.insert(QStringLiteral("dimension"), [] { return std::make_unique<DimensionObject>(); });
        t.insert(QStringLiteral("table"), [] { return std::make_unique<TableObject>(); });
        t.insert(QStringLiteral("clone"), [] { return std::make_unique<CloneObject>(); });
        t.insert(QStringLiteral("superclip"), [] { return std::make_unique<SuperClipObject>(); });
        // Dateien aus Version 0.85 und älter
        t.insert(QStringLiteral("powerclip"), [] { return std::make_unique<SuperClipObject>(); });
        t.insert(QStringLiteral("blend"), [] { return std::make_unique<BlendObject>(); });
        t.insert(QStringLiteral("connector"), [] { return std::make_unique<ConnectorObject>(); });
        t.insert(QStringLiteral("image"), [] { return std::make_unique<ImageObject>(); });
        return t;
    }();
    return types;
}

} // namespace

void registerType(const QString& typeName, Factory factory)
{
    registry().insert(typeName, std::move(factory));
}

QJsonObject properties(const Node& node)
{
    QJsonObject o;
    node.writeProperties(o);
    return o;
}

QJsonObject toJson(const Node& node, Ids ids)
{
    QJsonObject o = properties(node);
    o[kType] = node.typeName();
    if (ids == Ids::Keep)
        o[kId] = QString::number(node.id()); // als Text: JSON-Zahlen sind nur 53 Bit genau
    if (node.childCount() > 0) {
        QJsonArray children;
        for (int i = 0; i < node.childCount(); ++i)
            children.append(toJson(*node.child(i), ids));
        o[kChildren] = children;
    }
    return o;
}

std::unique_ptr<Node> fromJson(const QJsonObject& json, Ids ids, QString* error)
{
    const QString type = json.value(kType).toString();
    const auto it = registry().constFind(type);
    if (it == registry().constEnd()) {
        if (error)
            *error = QStringLiteral("Unbekannter Objekttyp '%1'").arg(type);
        return nullptr;
    }
    std::unique_ptr<Node> node = (*it)();
    if (ids == Ids::Drop && json.contains(QStringLiteral("next"))) {
        // Verweise auf andere Knoten (Textketten) gelten nur mit den alten Ids
        QJsonObject own = json;
        own.remove(QStringLiteral("next"));
        node->readProperties(own);
    } else {
        node->readProperties(json);
    }
    if (ids == Ids::Keep)
        setDetachedId(*node, json.value(kId).toString().toULongLong());

    const QJsonArray children = json.value(kChildren).toArray();
    for (const QJsonValue& c : children) {
        std::unique_ptr<Node> child = fromJson(c.toObject(), ids, error);
        if (!child)
            return nullptr;
        if (!node->canContain(*child)) {
            if (error)
                *error = QStringLiteral("'%1' darf nicht in '%2' liegen").arg(child->typeName(), node->typeName());
            return nullptr;
        }
        attachDetachedChild(*node, std::move(child));
    }
    return node;
}

} // namespace serial

} // namespace vek
