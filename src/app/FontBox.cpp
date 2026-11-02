#include "FontBox.h"

#include "Document.h"
#include "Editor.h"
#include "Options.h"
#include "TableObject.h"
#include "TextObject.h"

#include <QFontDatabase>
#include <QSettings>
#include <QSignalBlocker>

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
    setEditable(false);
    setMaxVisibleItems(20);
    rebuild();
    connect(this, &QComboBox::activated, this, [this](int index) {
        const QString family = itemData(index).toString();
        if (family.isEmpty())
            return;
        noteUsed(family);
        emit familyActivated(family);
        rebuild();
        setCurrentFamily(family);
    });
}

QString FontBox::currentFamily() const
{
    return currentData().toString();
}

void FontBox::setCurrentFamily(const QString& family)
{
    const QSignalBlocker block(this);
    int index = -1;
    for (int i = 0; i < count() && index < 0; ++i)
        if (itemData(i).toString() == family)
            index = i;
    if (index < 0 && !family.isEmpty()) {
        insertItem(0, family, family);
        index = 0;
    }
    setCurrentIndex(index);
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
    for (const QString& f : families(m_editor.document()))
        add(f);
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
