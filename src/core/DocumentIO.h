#pragma once

#include "Document.h"

#include <QString>

#include <memory>

namespace vek {

// Natives Dateiformat (.vek): ZIP-Container
//
//   mimetype        application/x-vektoreditor, unkomprimiert, erster Eintrag
//   manifest.json   {"format": "vektoreditor", "version": 1, "generator": "..."}
//   document.json   {"unit": "mm", "root": <Knotenbaum, siehe Serialization.h>}
//
// Bilder und eingebettete Schriften kommen später als eigene Einträge (media/…) dazu.
namespace io {

inline constexpr int FormatVersion = 1;
inline constexpr char MimeType[] = "application/x-vektoreditor";
inline constexpr char FileSuffix[] = "vek";

bool save(const Document& doc, const QString& path, QString* error = nullptr);
std::unique_ptr<Document> load(const QString& path, QString* error = nullptr);

// Ohne Datei, für Tests und Zwischenablage
QByteArray saveToBytes(const Document& doc, QString* error = nullptr);
std::unique_ptr<Document> loadFromBytes(const QByteArray& data, QString* error = nullptr);

} // namespace io

} // namespace vek
