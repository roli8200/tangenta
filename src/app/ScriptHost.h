#pragma once

#include "Node.h"

#include <QJSValue>
#include <QObject>
#include <QStringList>
#include <QVariant>

#include <memory>

class QJSEngine;

namespace vek {

class Editor;
class Transaction;
class DrawObject;
class ScriptHost;

// Ein Objekt des Dokuments, wie es ein Skript sieht. Masse in mm, Seitenkoordinaten
// (0,0 = links oben), Farben als "#rrggbb" bzw. null für keine.
class ScriptObject final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int id READ id CONSTANT)
    Q_PROPERTY(QString type READ type CONSTANT)
    Q_PROPERTY(double x READ x WRITE setX)
    Q_PROPERTY(double y READ y WRITE setY)
    Q_PROPERTY(double width READ width WRITE setWidth)
    Q_PROPERTY(double height READ height WRITE setHeight)
    Q_PROPERTY(QVariant fill READ fill WRITE setFill)
    Q_PROPERTY(QVariant stroke READ stroke WRITE setStroke)
    Q_PROPERTY(double strokeWidth READ strokeWidth WRITE setStrokeWidth)
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(QString name READ name WRITE setName)

public:
    ScriptObject(ScriptHost& host, NodeId id) : m_host(host), m_id(id) {}

    int id() const { return int(m_id); }
    QString type() const;
    double x() const;
    double y() const;
    double width() const;
    double height() const;
    void setX(double v);
    void setY(double v);
    void setWidth(double v);
    void setHeight(double v);
    QVariant fill() const;
    void setFill(const QVariant& v);
    QVariant stroke() const;
    void setStroke(const QVariant& v);
    double strokeWidth() const;
    void setStrokeWidth(double v);
    QString text() const;
    void setText(const QString& t);
    QString name() const;
    void setName(const QString& n);

    Q_INVOKABLE void move(double dx, double dy);
    Q_INVOKABLE void moveTo(double x, double y);
    // Drehen gegen den Uhrzeigersinn; ohne Mittelpunkt um die Mitte
    Q_INVOKABLE void rotate(double degrees, QJSValue cx = QJSValue(), QJSValue cy = QJSValue());
    Q_INVOKABLE void scale(double sx, QJSValue sy = QJSValue());
    Q_INVOKABLE void remove();
    Q_INVOKABLE QJSValue duplicate(double dx = 0.0, double dy = 0.0);
    // Eigenschaften der Eigenschaftsleiste (z.B. "points" beim Polygon, "rows" bei der Tabelle)
    Q_INVOKABLE QVariant get(const QString& property) const;
    Q_INVOKABLE void set(const QString& property, const QVariant& value);
    Q_INVOKABLE QStringList properties() const;

private:
    const DrawObject* object() const;
    void transform(const QTransform& m);

    ScriptHost& m_host;
    NodeId m_id;
};

// Das globale Objekt "tg" in Skripten
class ScriptApi final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int pageCount READ pageCount)
    Q_PROPERTY(int currentPage READ currentPage WRITE setCurrentPage)
    Q_PROPERTY(double pageWidth READ pageWidth)
    Q_PROPERTY(double pageHeight READ pageHeight)
    Q_PROPERTY(QString version READ version CONSTANT)

public:
    explicit ScriptApi(ScriptHost& host) : m_host(host) {}

    int pageCount() const;
    int currentPage() const;
    void setCurrentPage(int index);
    double pageWidth() const;
    double pageHeight() const;
    QString version() const;

    Q_INVOKABLE QJSValue rectangle(double x, double y, double w, double h, double radius = 0.0);
    Q_INVOKABLE QJSValue ellipse(double x, double y, double w, double h);
    Q_INVOKABLE QJSValue polygon(double x, double y, double w, double h, int corners = 5, bool star = false);
    Q_INVOKABLE QJSValue line(double x1, double y1, double x2, double y2);
    // Punkte als [[x, y], …]
    Q_INVOKABLE QJSValue polyline(const QJSValue& points, bool closed = false);
    Q_INVOKABLE QJSValue text(double x, double y, const QString& text, double sizePt = 24.0, const QString& font = QString());

    Q_INVOKABLE QJSValue selection() const;
    Q_INVOKABLE void select(const QJSValue& objects);
    // Alle Objekte der aktuellen Seite (oberste Ebene, von hinten nach vorn)
    Q_INVOKABLE QJSValue objects() const;
    Q_INVOKABLE QJSValue find(int id) const;
    Q_INVOKABLE void print(const QJSValue& a = QJSValue(), const QJSValue& b = QJSValue(), const QJSValue& c = QJSValue(),
                           const QJSValue& d = QJSValue());

private:
    ScriptHost& m_host;
};

// Führt JavaScript (QJSEngine, ECMAScript 7) gegen das Dokument aus. Ein Lauf ist genau ein
// Rückgängig-Schritt; bricht das Skript mit einem Fehler ab, bleibt das Dokument unverändert.
// Endlosschleifen stoppt eine Zeitgrenze (Optionen > Skripte).
class ScriptHost final : public QObject {
    Q_OBJECT

public:
    explicit ScriptHost(Editor& editor, QObject* parent = nullptr);
    ~ScriptHost() override;

    struct Result {
        bool ok = false;
        QString error; // mit Zeilennummer
        QStringList output;
        QJSValue value;
    };
    Result run(const QString& code, const QString& name = QString(), int timeoutMs = -1);

    // Für ScriptObject/ScriptApi
    Editor& editor() { return m_editor; }
    Transaction& tx();
    QJSEngine& engine() { return *m_engine; }
    QJSValue wrap(NodeId id);
    NodeId insert(std::unique_ptr<DrawObject> object);
    void log(const QString& line) { m_output << line; }
    QList<NodeId>& newSelection() { return m_newSelection; }
    bool selectionChanged() const { return m_selectionSet; }
    void markSelectionSet() { m_selectionSet = true; }

signals:
    void output(const QString& line);

private:
    Editor& m_editor;
    std::unique_ptr<QJSEngine> m_engine;
    std::unique_ptr<Transaction> m_tx;
    QStringList m_output;
    QList<NodeId> m_newSelection;
    bool m_selectionSet = false;
};

} // namespace vek
