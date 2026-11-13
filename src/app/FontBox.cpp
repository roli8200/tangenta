#include "FontBox.h"

#include "Document.h"
#include "Editor.h"
#include "Options.h"
#include "TableObject.h"
#include "TextObject.h"

#include <QEvent>
#include <QTimer>
#include <QAbstractItemView>
#include <QCompleter>
#include <QFontDatabase>
#include <QLineEdit>
#include <QSettings>
#include <QSignalBlocker>
#include <QStringListModel>

#include <functional>

namespace vek {

namespace {

const QString kRecentKey = QStringLiteral("fonts/recent");

bool isSymbolFont(const QString& family)
{
    const QList<QFontDatabase::WritingSystem> systems = QFontDatabase::writingSystems(family);
    return systems.contains(QFontDatabase::Symbol) && !systems.contains(QFontDatabase::Latin);
}

} // namespace

FontBox::FontBox(const Editor& editor, QWidget* parent) : QComboBox(parent), m_editor(editor)
{
    // Eingabefeld zum Suchen: Treffer irgendwo im Namen, ohne Gross-/Kleinschreibung
    setEditable(true);
    setInsertPolicy(QComboBox::NoInsert);
    setMaxVisibleItems(20);
    m_names = new QStringListModel(this);
    auto* completer = new QCompleter(m_names, this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->setMaxVisibleItems(20);
    setCompleter(completer);
    rebuild();
    connect(this, &QComboBox::activated, this, [this](int index) { choose(itemData(index).toString()); });
    connect(completer, qOverload<const QString&>(&QCompleter::activated), this, [this](const QString& family) { choose(family); });
    connect(lineEdit(), &QLineEdit::editingFinished, this, [this] { takeTyped(); });
    // Passen mehrere Schriften ("Poppins" -> Poppins, Poppins Black …), bleibt die Trefferliste offen.
    // Ein Klick daneben schliesst sie nur und kommt nicht beim Ziel an; editingFinished folgte dann
    // nie und die nächsten Eingaben landeten im Schriftfeld. Darum gilt beim Schliessen der Liste
    // ein genau getippter Name, und der Fokus geht zurück.
    completer->popup()->installEventFilter(this);
}

bool FontBox::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == completer()->popup() && event->type() == QEvent::Hide)
        QTimer::singleShot(0, this, [this] {
            if (!completer()->popup()->isVisible() && !exactMatch(lineEdit()->text()).isEmpty()) {
                takeTyped();
                lineEdit()->clearFocus();
            }
        });
    return QComboBox::eventFilter(watched, event);
}

QString FontBox::exactMatch(const QString& text) const
{
    const QString typed = text.trimmed();
    return m_names->stringList().contains(typed) ? typed : familyIgnoringCase(typed);
}

void FontBox::takeTyped()
{
    // Getippter Name einer vorhandenen Schrift wählt sie, alles andere zeigt wieder die aktuelle
    const QString match = exactMatch(lineEdit()->text());
    if (!match.isEmpty() && match != m_current)
        choose(match);
    else
        setCurrentFamily(m_current);
}

void FontBox::choose(const QString& family)
{
    if (family.isEmpty())
        return;
    noteUsed(family);
    m_current = family;
    emit familyActivated(family);
    rebuild();
    setCurrentFamily(family);
}

QString FontBox::familyIgnoringCase(const QString& name) const
{
    for (const QString& f : m_names->stringList())
        if (f.compare(name, Qt::CaseInsensitive) == 0)
            return f;
    return {};
}

QString FontBox::currentFamily() const
{
    return m_current;
}

void FontBox::setCurrentFamily(const QString& family)
{
    const QSignalBlocker block(this);
    // Den Eintrag in der alphabetischen Liste wählen (den letzten Treffer), nicht den in
    // "zuletzt benutzt": So öffnet sich die Liste an der Stelle der Schrift. Gross-/Kleinschreibung
    // zählt nicht, Namen aus Dateien weichen darin oft ab.
    int index = -1;
    for (int i = count() - 1; i >= 0 && index < 0; --i)
        if (itemData(i).toString() == family)
            index = i;
    for (int i = count() - 1; i >= 0 && index < 0; --i)
        if (itemData(i).toString().compare(family, Qt::CaseInsensitive) == 0)
            index = i;
    if (index < 0 && !family.isEmpty()) {
        insertItem(0, family, family);
        index = 0;
    }
    m_current = family;
    setCurrentIndex(index);
    if (index >= 0 && view())
        view()->scrollTo(model()->index(index, 0));
}

void FontBox::rebuild()
{
    const QString current = currentFamily();
    const QSignalBlocker block(this);
    clear();
    const bool preview = Options::instance().flag(QStringLiteral("ws/text/fonts/previewName"));
    auto add = [this, preview](const QString& family) {
        addItem(family, family);
        if (preview)
            setItemData(count() - 1, QFont(family, font().pointSize()), Qt::FontRole);
    };
    const QStringList recent = recentFamilies();
    for (const QString& f : recent)
        add(f);
    if (!recent.isEmpty())
        insertSeparator(count());
    QStringList all = families(m_editor.document());
    for (const QString& f : all)
        add(f);
    for (const QString& f : recent)
        if (!all.contains(f))
            all << f;
    m_names->setStringList(all);
    if (!current.isEmpty())
        setCurrentFamily(current);
}

void FontBox::noteUsed(const QString& family)
{
    QSettings settings;
    QStringList list = settings.value(kRecentKey).toStringList();
    list.removeAll(family);
    list.prepend(family);
    while (list.size() > 20)
        list.removeLast();
    settings.setValue(kRecentKey, list);
}

QStringList FontBox::recentFamilies()
{
    const int n = std::clamp(Options::instance().integer(QStringLiteral("ws/text/fonts/recent")), 0, 20);
    return QSettings().value(kRecentKey).toStringList().mid(0, n);
}

QStringList FontBox::documentFamilies(const Document& document)
{
    QStringList result;
    std::function<void(const Node&)> visit = [&](const Node& n) {
        if (const auto* t = node_cast<TextObject>(&n)) {
            for (const TextParagraph& p : t->paragraphs())
                for (const TextRun& r : p.runs)
                    if (!result.contains(r.format.family))
                        result << r.format.family;
        } else if (const auto* table = node_cast<TableObject>(&n)) {
            if (!result.contains(table->textFormat().family))
                result << table->textFormat().family;
        }
        for (int i = 0; i < n.childCount(); ++i)
            visit(*n.child(i));
    };
    visit(document.root());
    result.sort(Qt::CaseInsensitive);
    return result;
}

QStringList FontBox::families(const Document& document)
{
    const Options& o = Options::instance();
    if (o.flag(QStringLiteral("ws/text/fonts/docFonts")))
        return documentFamilies(document);
    const bool scalable = o.flag(QStringLiteral("ws/text/fonts/truetype"));
    const bool symbols = o.flag(QStringLiteral("ws/text/fonts/ttSymbols"));
    QStringList result;
    for (const QString& family : QFontDatabase::families()) {
        if (QFontDatabase::isPrivateFamily(family))
            continue;
        if (!symbols && isSymbolFont(family))
            continue;
        // "TrueType-Schriften": skalierbare Umrissschriften (TrueType/OpenType)
        if (!scalable && QFontDatabase::isSmoothlyScalable(family))
            continue;
        result << family;
    }
    return result;
}

} // namespace vek
