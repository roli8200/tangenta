#pragma once

#include <QComboBox>
#include <QStringList>

class QStringListModel;

namespace vek {

class Document;
class Editor;

// Schriftliste der Eigenschaftsleiste nach Optionen > Text > Schriften: zuletzt benutzte
// Schriften oben, Symbolschriften und Dokumentschriften nach Wahl, Namen in der eigenen Schrift.
// Ins Feld tippen sucht (Teil des Namens genügt).
class FontBox final : public QComboBox {
    Q_OBJECT

public:
    FontBox(const Editor& editor, QWidget* parent = nullptr);

    QString currentFamily() const;
    // Wählt die Schrift; fehlt sie in der Liste (z.B. nicht installiert), wird sie oben eingefügt
    void setCurrentFamily(const QString& family);
    void rebuild();

    // Schrift als benutzt merken (Liste "kürzlich verwendet")
    static void noteUsed(const QString& family);
    static QStringList recentFamilies();
    // Gefilterte Schriftfamilien ohne die kürzlich verwendeten
    static QStringList families(const Document& document);
    static QStringList documentFamilies(const Document& document);

signals:
    void familyActivated(const QString& family);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void choose(const QString& family);
    void takeTyped();
    QString exactMatch(const QString& text) const;
    QString familyIgnoringCase(const QString& name) const;

    const Editor& m_editor;
    QStringListModel* m_names = nullptr; // alle wählbaren Namen, einmal, für die Suche
    QString m_current;
};

} // namespace vek
