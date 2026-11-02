#pragma once

#include "Node.h"
#include "Property.h"
#include "Effects.h"
#include "Ramp.h"
#include "Style.h"

#include <QPainterPath>
#include <QRectF>
#include <QSizeF>
#include <QTransform>
#include <QVariant>

#include <algorithm>
#include <cmath>

namespace vek {

// Basisklasse aller Objekte, die man im Zeichenbereich erstellen kann.
//
// Ein Objekt besteht aus einer Grundform in lokalen Koordinaten (localPath) und einer
// affinen Transformation in die Seitenkoordinaten. Skalieren, Drehen und Neigen verändern
// nur die Transformation; ein gedrehtes Rechteck bleibt ein Rechteck mit Breite und Höhe.
// Alle Grundfunktionen arbeiten in Seitenkoordinaten (mm, Ursprung oben links, y nach unten).
class DrawObject : public Node {
public:
    // Grundform ohne Transformation
    virtual QPainterPath localPath() const = 0;

    // Umriss in Seitenkoordinaten (samt Hülle)
    virtual QPainterPath path() const { return m_transform.map(m_envelopes.isEmpty() ? localPath() : enveloped(localPath())); }
    // Umriss zum Einrasten und für die Ziehvorschau (Seite). Objekte aus sehr vielen Knoten
    // (Text) liefern hier ihren Rahmen, sonst müsste jede Mausbewegung tausende Glyphenknoten
    // durchsuchen.
    virtual QPainterPath snapPath() const { return path(); }
    // Achsparalleles Begrenzungsrechteck in Seitenkoordinaten, ohne Konturbreite
    virtual QRectF boundingRect() const { return path().boundingRect(); }
    // tolerance in mm; treatAsFilled: auch ungefüllte Objekte im Inneren treffen
    virtual bool hitTest(const QPointF& point, double tolerance, bool treatAsFilled) const;

    bool canContain(const Node&) const override { return false; }

    const QTransform& transform() const { return m_transform; }
    void setTransform(const QTransform& t) { m_transform = t; }
    const Style& style() const { return m_style; }
    virtual void setStyle(const Style& s) { m_style = s; }
    // Unschärfe wie der Gausssche Weichzeichner in Inkscape: Standardabweichung in mm,
    // 0 = scharf. Gilt für jedes Objekt samt Füllung und Kontur, bei Gruppen für den Inhalt.
    // Nur am Bildschirm/beim Ausgeben; die Pfade bleiben unverändert. Kann verlaufen (blurRamp).
    // blur() = stärkste Stelle, setBlur = gleichmässig.
    double blur() const { return m_blur.maximum(); }
    void setBlur(double mm) { m_blur = Ramp::uniform(std::max(0.0, mm)); }
    const Ramp& blurRamp() const { return m_blur; }
    // Transparenz in Prozent, gleichmässig oder verlaufend (interaktive Transparenz)
    const Ramp& transparency() const { return m_transparency; }
    const Ramp& effect(Effect e) const { return e == Effect::Blur ? m_blur : m_transparency; }
    void setEffect(Effect e, const Ramp& ramp) { (e == Effect::Blur ? m_blur : m_transparency) = ramp; }
    // Unschärfe und Transparenz von other übernehmen (Umwandeln, Kombinieren)
    void copyEffects(const DrawObject& other)
    {
        m_blur = other.m_blur;
        m_transparency = other.m_transparency;
        m_lens = other.m_lens;
        m_bevel = other.m_bevel;
        m_shadow = other.m_shadow;
        m_contour = other.m_contour;
        m_extrusion = other.m_extrusion;
        m_envelopes = other.m_envelopes;
    }
    // Hüllen (Effekte > Hülle), nacheinander auf die lokale Geometrie angewendet; die letzte ist
    // die, die man bearbeitet ("Neue Hülle" legt eine weitere darüber)
    const QList<Envelope>& envelopes() const { return m_envelopes; }
    void setEnvelopes(const QList<Envelope>& envelopes) { m_envelopes = envelopes; }
    bool hasEnvelope() const { return !m_envelopes.isEmpty(); }
    // Lokale Geometrie (Grundform, bei Text ein Glyphenstück) durch alle Hüllen; zwischengespeichert
    QPainterPath enveloped(const QPainterPath& local) const;
    const ContourEffect& contour() const { return m_contour; }
    void setContour(const ContourEffect& c) { m_contour = c; }
    const Extrusion& extrusion() const { return m_extrusion; }
    void setExtrusion(const Extrusion& e) { m_extrusion = e; }
    const DropShadow& shadow() const { return m_shadow; }
    void setShadow(const DropShadow& s) { m_shadow = s; }
    // Effekte > Linse und Effekte > Abschrägung
    const Lens& lens() const { return m_lens; }
    void setLens(const Lens& l) { m_lens = l; }
    const Bevel& bevel() const { return m_bevel; }
    void setBevel(const Bevel& b) { m_bevel = b; }
    // Effekt löschen: Unschärfe, Transparenz, Linse, Abschrägung weg
    void clearEffects()
    {
        m_blur = Ramp();
        m_transparency = Ramp();
        m_lens = Lens();
        m_bevel = Bevel();
        m_shadow = DropShadow();
        m_contour = ContourEffect();
        m_extrusion = Extrusion();
        m_envelopes.clear();
    }
    bool hasEffects() const { return m_blur.isActive() || m_transparency.type != Ramp::Type::None || m_lens.isActive() || m_bevel.isActive() || m_shadow.active || m_contour.active || m_extrusion.active || !m_envelopes.isEmpty(); }
    // Gesperrt: lässt sich auswählen, aber weder verschieben noch ändern
    bool locked() const { return m_locked; }
    void setLocked(bool locked) { m_locked = locked; }

    // --- Grundfunktionen ---------------------------------------------------------------
    // m wird nach der bestehenden Transformation angewendet (also in Seitenkoordinaten).
    virtual void applyTransform(const QTransform& m)
    {
        m_transform = m_transform * m;
        // "Mit Objekt skalieren": Konturbreite mit der mittleren Skalierung
        if (m_style.scaleWithObject)
            m_style.strokeWidth *= std::sqrt(std::abs(m.m11() * m.m22() - m.m12() * m.m21()));
    }

    void move(const QPointF& delta);
    // Breiter/schmaler (sx), höher/flacher (sy); anchor bleibt stehen
    void scale(double sx, double sy, const QPointF& anchor);
    // Linear vergrössern/verkleinern, Seitenverhältnis bleibt
    void scaleUniform(double factor, const QPointF& anchor) { scale(factor, factor, anchor); }
    // Begrenzungsrechteck auf target ziehen
    void resizeTo(const QRectF& target);
    // Grad im Uhrzeigersinn auf dem Bildschirm (y zeigt nach unten)
    void rotate(double degrees, const QPointF& center);
    // Neigen um anchor; Winkel in Grad
    void skew(double degreesX, double degreesY, const QPointF& anchor);

    // --- typeigene Bearbeitungsmethoden für die Eigenschaftsleiste ----------------------
    virtual QList<PropertyDescriptor> propertyDescriptors() const { return {}; }
    virtual QVariant propertyValue(const QString& /*id*/) const { return {}; }
    // Liefert false, wenn die Eigenschaft unbekannt ist
    virtual bool setPropertyValue(const QString& /*id*/, const QVariant& /*value*/) { return false; }
    virtual bool invokeAction(const QString& /*id*/) { return false; }

    // Drehwinkel der Transformation in Grad (im Uhrzeigersinn, y nach unten)
    double rotationAngle() const;

    void writeProperties(QJsonObject& out) const override;
    void readProperties(const QJsonObject& in) override;

protected:
    // Für Grundformen mit eigener Breite und Höhe (Rechteck, Ellipse, Polygon): solange die
    // Transformation nicht neigt, wandert ihre Skalierung in size, damit die Form ihre echten
    // Masse behält (wie)
    void absorbScale(QSizeF& size);

private:
    QTransform m_transform;
    Style m_style;
    Ramp m_blur;
    Ramp m_transparency;
    Lens m_lens;
    Bevel m_bevel;
    DropShadow m_shadow;
    ContourEffect m_contour;
    Extrusion m_extrusion;
    QList<Envelope> m_envelopes;
    struct EnvelopeCache {
        QList<Envelope> envelopes;
        QList<std::pair<QPainterPath, QPainterPath>> paths; // lokal -> verformt
    };
    mutable EnvelopeCache m_envelopeCache;
    bool m_locked = false;
};

// Matrizen für die Grundfunktionen, jeweils in Seitenkoordinaten
namespace xform {
QTransform translation(const QPointF& delta);
QTransform scaling(double sx, double sy, const QPointF& anchor);
QTransform rotation(double degrees, const QPointF& center);
QTransform skewing(double degreesX, double degreesY, const QPointF& anchor);
// Bildet Rechteck from auf to ab (Skalierung + Verschiebung)
QTransform rectToRect(const QRectF& from, const QRectF& to);
} // namespace xform

} // namespace vek
