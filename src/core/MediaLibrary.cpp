#include "MediaLibrary.h"

#include "PathOps.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>

namespace vek {

namespace {

QJsonObject strokeToJson(const MediaStroke& s)
{
    return {{QStringLiteral("name"), s.name}, {QStringLiteral("media"), media::toJson(s.media)}};
}

MediaStroke strokeFromJson(const QJsonObject& o)
{
    MediaStroke s;
    s.name = o.value(QStringLiteral("name")).toString();
    s.media = media::fromJson(o.value(QStringLiteral("media")).toObject());
    return s;
}

// Gleicher Strich für die Bibliothek: Zufallszahl, Druckverlauf und Breite zählen nicht
bool sameStroke(const ArtisticMedia& a, const ArtisticMedia& b)
{
    if (a.kind != b.kind)
        return false;
    switch (a.kind) {
    case ArtisticMedia::Kind::Preset:
        return a.preset == b.preset;
    case ArtisticMedia::Kind::Brush:
        return a.artName == b.artName && a.art == b.art;
    case ArtisticMedia::Kind::Sprayer:
        return a.art.isEmpty() && b.art.isEmpty() ? a.sprayList == b.sprayList : a.artName == b.artName && a.art == b.art;
    default:
        return true;
    }
}

} // namespace

MediaLibrary& MediaLibrary::instance()
{
    static MediaLibrary library;
    return library;
}

void MediaLibrary::setFilePath(const QString& path)
{
    m_path = path;
    load();
    touch();
}

QString MediaLibrary::filePath() const
{
    if (!m_path.isEmpty())
        return m_path;
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/medien.json");
}

void MediaLibrary::ensureLoaded() const
{
    if (!m_loaded)
        const_cast<MediaLibrary*>(this)->load();
}

void MediaLibrary::load()
{
    m_loaded = true;
    m_user.clear();
    m_recent.clear();
    QFile file(filePath());
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    for (const QJsonValue& v : root.value(QStringLiteral("user")).toArray())
        m_user << strokeFromJson(v.toObject());
    for (const QJsonValue& v : root.value(QStringLiteral("recent")).toArray())
        m_recent << strokeFromJson(v.toObject());
    while (m_recent.size() > kRecent)
        m_recent.removeLast();
}

bool MediaLibrary::save() const
{
    QJsonArray user, recent;
    for (const MediaStroke& s : m_user)
        user << strokeToJson(s);
    for (const MediaStroke& s : m_recent)
        recent << strokeToJson(s);
    const QJsonObject root{{QStringLiteral("version"), 1}, {QStringLiteral("user"), user}, {QStringLiteral("recent"), recent}};
    const QString path = filePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    return file.commit();
}

void MediaLibrary::touch()
{
    ++m_revision;
    emit changed();
}

QList<MediaStroke> MediaLibrary::presets() const
{
    QList<MediaStroke> out;
    for (int i = 0; i < pathops::mediaPresetCount(); ++i) {
        MediaStroke s;
        s.name = pathops::mediaPresetName(i);
        s.media.kind = ArtisticMedia::Kind::Preset;
        s.media.preset = i;
        s.media.width = 5.0;
        s.builtIn = true;
        out << s;
    }
    return out;
}

QList<MediaStroke> MediaLibrary::brushes() const
{
    ensureLoaded();
    QList<MediaStroke> out;
    for (int i = 0; i < media::builtInBrushCount(); ++i) {
        MediaStroke s;
        s.name = media::builtInBrushName(i);
        s.media.kind = ArtisticMedia::Kind::Brush;
        s.media.art = {media::builtInBrush(i, &s.media.artAspect, &s.media.brushRepeat)};
        s.media.artName = s.name;
        s.media.width = 8.0;
        s.builtIn = true;
        out << s;
    }
    for (const MediaStroke& s : m_user)
        if (s.media.kind == ArtisticMedia::Kind::Brush)
            out << s;
    return out;
}

QList<MediaStroke> MediaLibrary::sprayers() const
{
    ensureLoaded();
    QList<MediaStroke> out;
    for (int i = 0; i < pathops::sprayListCount(); ++i) {
        MediaStroke s;
        s.name = pathops::sprayListName(i);
        s.media.kind = ArtisticMedia::Kind::Sprayer;
        s.media.sprayList = i;
        s.media.width = 6.0;
        s.builtIn = true;
        out << s;
    }
    for (const MediaStroke& s : m_user)
        if (s.media.kind == ArtisticMedia::Kind::Sprayer)
            out << s;
    return out;
}

QList<MediaStroke> MediaLibrary::user() const
{
    ensureLoaded();
    return m_user;
}

QList<MediaStroke> MediaLibrary::recent() const
{
    ensureLoaded();
    return m_recent;
}

void MediaLibrary::noteUsed(const MediaStroke& stroke)
{
    ensureLoaded();
    if (!stroke.media.isActive())
        return;
    MediaStroke s = stroke;
    s.builtIn = false;
    s.media.pressure.clear();
    if (s.name.isEmpty())
        s.name = strokeFor(s.media).name;
    if (!m_recent.isEmpty() && sameStroke(m_recent.first().media, s.media) && m_recent.first().media == s.media)
        return; // schon vorn: nichts zu schreiben
    m_recent.erase(std::remove_if(m_recent.begin(), m_recent.end(), [&s](const MediaStroke& r) { return sameStroke(r.media, s.media); }),
                   m_recent.end());
    m_recent.prepend(s);
    while (m_recent.size() > kRecent)
        m_recent.removeLast();
    save();
    touch();
}

MediaStroke MediaLibrary::addUserBrush(const QString& name, const ArtItem& pieces)
{
    ensureLoaded();
    MediaStroke s;
    s.name = name.isEmpty() ? uniqueName(tr("Eigener Pinsel")) : name;
    s.media.kind = ArtisticMedia::Kind::Brush;
    double height = 1.0;
    s.media.art = {media::normalizedBrush(pieces, &s.media.artAspect, &height)};
    s.media.width = std::clamp(height, 0.5, 200.0);
    s.media.artName = s.name;
    removeUser(s.name);
    m_user << s;
    save();
    touch();
    return s;
}

MediaStroke MediaLibrary::addUserSprayer(const QString& name, const QList<ArtItem>& objects)
{
    ensureLoaded();
    MediaStroke s;
    s.name = name.isEmpty() ? uniqueName(tr("Eigene Sprühliste")) : name;
    s.media.kind = ArtisticMedia::Kind::Sprayer;
    double size = 0.0;
    for (const ArtItem& item : objects) {
        if (item.isEmpty())
            continue;
        QRectF b;
        for (const ArtPiece& p : item)
            b = b.isNull() ? p.path.boundingRect() : b.united(p.path.boundingRect());
        size += std::max(b.width(), b.height());
        s.media.art << media::normalizedSprayItem(item);
    }
    if (!s.media.art.isEmpty())
        s.media.width = std::clamp(size / s.media.art.size(), 0.5, 200.0);
    s.media.spacing = std::max(0.2, s.media.width * 1.2);
    s.media.artName = s.name;
    removeUser(s.name);
    m_user << s;
    save();
    touch();
    return s;
}

bool MediaLibrary::removeUser(const QString& name)
{
    ensureLoaded();
    const auto it = std::find_if(m_user.begin(), m_user.end(), [&name](const MediaStroke& s) { return s.name == name; });
    if (it == m_user.end())
        return false;
    m_user.erase(it);
    save();
    touch();
    return true;
}

QString MediaLibrary::uniqueName(const QString& base) const
{
    ensureLoaded();
    for (int i = 1;; ++i) {
        const QString name = QStringLiteral("%1 %2").arg(base).arg(i);
        if (std::none_of(m_user.begin(), m_user.end(), [&name](const MediaStroke& s) { return s.name == name; }))
            return name;
    }
}

MediaStroke MediaLibrary::strokeFor(const ArtisticMedia& m) const
{
    QList<MediaStroke> candidates;
    if (m.kind == ArtisticMedia::Kind::Preset)
        candidates = presets();
    else if (m.kind == ArtisticMedia::Kind::Brush)
        candidates = brushes();
    else if (m.kind == ArtisticMedia::Kind::Sprayer)
        candidates = sprayers();
    for (const MediaStroke& s : std::as_const(candidates)) {
        if (sameStroke(s.media, m)) {
            MediaStroke found = s;
            found.media = m;
            return found;
        }
    }
    return MediaStroke{QString(), m, false};
}

} // namespace vek
