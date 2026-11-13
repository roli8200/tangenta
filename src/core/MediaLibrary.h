#pragma once

#include "CurveObject.h"

#include <QList>
#include <QObject>
#include <QString>

namespace vek {

// Eintrag der Strichbibliothek: Name und vollständige Einstellungen des Strichs
struct MediaStroke {
    QString name;
    ArtisticMedia media;
    bool builtIn = false;

    friend bool operator==(const MediaStroke&, const MediaStroke&) = default;
};

// Strichbibliothek der künstlerischen Medien: eingebaute Voreinstellungen, Pinsel und
// Sprühlisten, eigene Striche (aus Objekten gespeichert) und die zuletzt benutzten. Eigene
// und zuletzt benutzte Striche liegen als JSON im Programmdatenordner.
class MediaLibrary final : public QObject {
    Q_OBJECT

public:
    static MediaLibrary& instance();

    // Datei der Bibliothek; leer = <Programmdaten>/medien.json. Lädt neu.
    void setFilePath(const QString& path);
    QString filePath() const;

    QList<MediaStroke> presets() const;
    QList<MediaStroke> brushes() const;  // eingebaute und eigene Pinsel
    QList<MediaStroke> sprayers() const; // eingebaute und eigene Sprühlisten
    QList<MediaStroke> user() const;     // nur eigene
    QList<MediaStroke> recent() const;

    // Zuletzt benutzt: vorn einreihen, höchstens kRecent Einträge
    void noteUsed(const MediaStroke& stroke);
    // Eigener Pinsel bzw. eigene Sprühliste aus Objekten (Seitenkoordinaten, siehe media::piecesOf)
    MediaStroke addUserBrush(const QString& name, const ArtItem& pieces);
    MediaStroke addUserSprayer(const QString& name, const QList<ArtItem>& objects);
    bool removeUser(const QString& name);
    // Freier Name nach dem Muster "base 1", "base 2", …
    QString uniqueName(const QString& base) const;

    // Eintrag, der zu media gehört (Name aus der Bibliothek), sonst ein namenloser
    MediaStroke strokeFor(const ArtisticMedia& media) const;
    // Zählt bei jeder Änderung hoch (Listen neu aufbauen)
    int revision() const { return m_revision; }

    static constexpr int kRecent = 10;

signals:
    void changed();

private:
    MediaLibrary() = default;
    void ensureLoaded() const;
    void load();
    bool save() const;
    void touch();

    QString m_path;
    mutable bool m_loaded = false;
    QList<MediaStroke> m_user;
    QList<MediaStroke> m_recent;
    int m_revision = 0;
};

} // namespace vek
