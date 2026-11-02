#pragma once

#include "Node.h"

#include <QList>
#include <QPointF>
#include <QSizeF>

namespace vek {

// Wurzel des Dokumentbaums. Erstes Kind ist immer die Masterseite, danach folgen die Seiten.
class DocumentRoot final : public Node {
public:
    QString typeName() const override { return QStringLiteral("document"); }
    bool canContain(const Node& child) const override;
};

// Hilfslinie: unendliche Gerade durch den Ankerpunkt, Winkel in Grad gegen den Uhrzeigersinn
// (0 = waagrecht, 90 = senkrecht). Um den Ankerpunkt wird gedreht.
struct Guide {
    QPointF anchor;
    double angle = 0.0;
    bool preset = false; // aus Optionen > Hilfslinien > Voreinstellungen (eigene Farbe)

    static Guide horizontal(double y, double anchorX, bool preset = false) { return {QPointF(anchorX, y), 0.0, preset}; }
    static Guide vertical(double x, double anchorY, bool preset = false) { return {QPointF(x, anchorY), 90.0, preset}; }

    // Einheitsvektor entlang der Linie (y nach unten) und senkrecht dazu
    QPointF direction() const;
    QPointF normal() const { const QPointF d = direction(); return QPointF(-d.y(), d.x()); }
    bool isHorizontal() const;
    bool isVertical() const;
    // Vorzeichenbehafteter Abstand eines Punkts von der Linie
    double distance(const QPointF& p) const { return QPointF::dotProduct(p - anchor, normal()); }
    QPointF project(const QPointF& p) const { return p - normal() * distance(p); }

    friend bool operator==(const Guide&, const Guide&) = default;
};

// Eine Seite. Die Masterseite ist eine Seite mit master() == true; ihre Objekte erscheinen
// auf allen Seiten (globaler Objektbaum). Alle Masse in mm.
// Hilfslinien der Masterseite gelten für alle Seiten (global), die einer
// Seite nur dort (seitenspezifisch).
class Page final : public Node {
public:
    static constexpr QSizeF A4 = QSizeF(210.0, 297.0);

    explicit Page(bool master = false) : m_master(master) {}

    QString typeName() const override { return m_master ? QStringLiteral("masterPage") : QStringLiteral("page"); }
    bool canContain(const Node& child) const override;

    bool master() const { return m_master; }

    QSizeF size() const { return m_size; }
    void setSize(const QSizeF& size) { m_size = size; }

    const QList<Guide>& guides() const { return m_guides; }
    QList<Guide>& guides() { return m_guides; }

    // Dokumenteinstellungen, gespeichert auf der Masterseite:
    // Pfeiltasten verschieben um nudge, Duplikate liegen um duplicateOffset versetzt (mm)
    double nudge() const { return m_nudge; }
    void setNudge(double mm) { m_nudge = mm; }
    QPointF duplicateOffset() const { return m_duplicateOffset; }
    void setDuplicateOffset(const QPointF& offset) { m_duplicateOffset = offset; }
    // Übrige Dokument-Einstellungen des Dialogs "Optionen" (Gitter, Hintergrund, …) als
    // Schlüssel/Wert; nur abweichende Werte, die Vorgaben kennt die Anwendung
    const QJsonObject& options() const { return m_options; }
    void setOptions(const QJsonObject& options) { m_options = options; }

    void writeProperties(QJsonObject& out) const override;
    void readProperties(const QJsonObject& in) override;

private:
    bool m_master = false;
    QSizeF m_size = A4;
    QList<Guide> m_guides;
    double m_nudge = 0.1;
    QPointF m_duplicateOffset = QPointF(5.0, 5.0);
    QJsonObject m_options;
};

// Ebene innerhalb einer Seite; enthält die Zeichenobjekte.
class Layer final : public Node {
public:
    QString typeName() const override { return QStringLiteral("layer"); }
    bool canContain(const Node& child) const override;

    bool visible() const { return m_visible; }
    void setVisible(bool v) { m_visible = v; }
    bool locked() const { return m_locked; }
    void setLocked(bool v) { m_locked = v; }
    bool printable() const { return m_printable; }
    void setPrintable(bool v) { m_printable = v; }

    void writeProperties(QJsonObject& out) const override;
    void readProperties(const QJsonObject& in) override;

private:
    bool m_visible = true;
    bool m_locked = false;
    bool m_printable = true;
};

// Neue Seite mit einer leeren Ebene, bereit zum Einfügen
std::unique_ptr<Page> createPage(const QSizeF& size, const QString& layerName, bool master = false);

} // namespace vek
