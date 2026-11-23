#include "AiImageDialog.h"

#include "ColorTransfer.h"
#include "Matting.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QHttpMultiPart>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPainter>
#include <QThread>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QSignalBlocker>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <optional>
#include <cmath>

namespace vek {

namespace {
constexpr int kSendSize = 768; // längste Kante des Bildes, das die KI sieht

// Zeile eines Formulars, deren Feld widget ist oder enthält (Regler stehen mit Zahlenfeld in einer Zeile)
int formRow(QFormLayout* form, QWidget* widget)
{
    for (int row = 0; row < form->rowCount(); ++row) {
        QLayoutItem* field = form->itemAt(row, QFormLayout::FieldRole);
        if (!field)
            continue;
        if (field->widget() == widget || (field->layout() && field->layout()->indexOf(widget) >= 0))
            return row;
    }
    return -1;
}

void setRowShown(QFormLayout* form, QWidget* widget, bool visible)
{
    const int row = formRow(form, widget);
    if (row >= 0)
        form->setRowVisible(row, visible);
}
} // namespace

namespace aiopt {

namespace {

const char* goalText(Goal goal)
{
    switch (goal) {
    case Goal::Natural: return "natural and balanced: neutral colours, correct exposure, no visible effect";
    case Goal::Vivid: return "vivid: punchy colours and contrast, still believable";
    case Goal::Warm: return "warm: golden, friendly mood";
    case Goal::Cool: return "cool: fresh, slightly bluish mood";
    case Goal::Bright: return "bright and airy: light, open shadows, soft contrast";
    case Goal::Moody: return "moody: darker, denser, more dramatic";
    case Goal::Print: return "ready for print: slightly brighter midtones and open shadows (paper darkens), clean colours";
    }
    return "";
}

int clampValue(double v)
{
    return std::clamp(int(std::lround(v)), -100, 100);
}

} // namespace

QString systemPrompt()
{
    return QStringLiteral(
        "You are a careful photo retoucher inside the vector graphics program TANGENTA. You look at one photo and "
        "choose values for the program's image adjustment controls. You never describe or invent image content.\n"
        "Controls (integers from -100 to 100, 0 = unchanged):\n"
        "- temperature: + warmer (more red/yellow), - cooler (more blue)\n"
        "- tint: + magenta, - green\n"
        "- saturation: colour intensity\n"
        "- brightness: overall lightness\n"
        "- contrast: overall contrast\n"
        "- highlights: + brighter / - darker bright areas (negative recovers bright skies)\n"
        "- shadows: + opens dark areas, - deepens them\n"
        "- midtones: + lighter, - darker midtones (gamma)\n"
        "- stretch: true = first stretch each colour channel to the full tonal range (removes haze and colour casts "
        "strongly; only when the histogram is clearly compressed)\n"
        "Typical corrections are small: most values between -30 and 30. Leave a control at 0 when the photo is fine "
        "there. Controls the user has locked must be 0.\n"
        "Answer ONLY with one JSON object, no other text: {\"temperature\":0,\"tint\":0,\"saturation\":0,"
        "\"brightness\":0,\"contrast\":0,\"highlights\":0,\"shadows\":0,\"midtones\":0,\"stretch\":false,"
        "\"comment\":\"one or two short sentences for the user: what you changed and why\"}");
}

QString statistics(const QImage& image)
{
    const QImage small = image.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_RGB32);
    std::array<std::array<int, 256>, 4> hist{}; // R, G, B, Helligkeit
    double saturation = 0.0;
    const int n = std::max(1, small.width() * small.height());
    for (int y = 0; y < small.height(); ++y) {
        const auto* line = reinterpret_cast<const QRgb*>(small.constScanLine(y));
        for (int x = 0; x < small.width(); ++x) {
            const int r = qRed(line[x]), g = qGreen(line[x]), b = qBlue(line[x]);
            ++hist[0][r];
            ++hist[1][g];
            ++hist[2][b];
            ++hist[3][(r * 299 + g * 587 + b * 114) / 1000];
            const int hi = std::max({r, g, b}), lo = std::min({r, g, b});
            saturation += hi > 0 ? double(hi - lo) / hi : 0.0;
        }
    }
    auto percentile = [n](const std::array<int, 256>& h, double p) {
        const int target = int(std::ceil(n * p));
        int sum = 0;
        for (int i = 0; i < 256; ++i) {
            sum += h[std::size_t(i)];
            if (sum >= target)
                return i;
        }
        return 255;
    };
    QStringList parts;
    const char* names[] = {"red", "green", "blue", "luminance"};
    for (int c = 0; c < 4; ++c)
        parts << QStringLiteral("%1 p1/p50/p99 = %2/%3/%4")
                     .arg(QLatin1String(names[c]))
                     .arg(percentile(hist[std::size_t(c)], 0.01))
                     .arg(percentile(hist[std::size_t(c)], 0.5))
                     .arg(percentile(hist[std::size_t(c)], 0.99));
    parts << QStringLiteral("mean saturation = %1 %").arg(int(std::lround(100.0 * saturation / n)));
    return parts.join(QStringLiteral("; "));
}

QString question(const Settings& s, const QImage& image, const QString& language)
{
    QStringList locked;
    if (!s.color)
        locked << QStringLiteral("temperature, tint");
    if (!s.tone)
        locked << QStringLiteral("brightness, contrast, highlights, shadows, midtones, stretch");
    if (!s.saturation)
        locked << QStringLiteral("saturation");
    QString q = QStringLiteral("Optimise this photo. Goal: %1.\n").arg(QLatin1String(goalText(s.goal)));
    if (!locked.isEmpty())
        q += QStringLiteral("Locked controls (must stay 0): %1.\n").arg(locked.join(QStringLiteral(", ")));
    if (!s.wish.trimmed().isEmpty())
        q += QStringLiteral("Additional wish of the user: %1\n").arg(s.wish.trimmed());
    q += QStringLiteral("Tonal statistics of the full image (0-255): %1.\n").arg(statistics(image));
    q += QStringLiteral("Write the comment in %1.").arg(language);
    return q;
}

Suggestion parse(const QString& reply, const Settings& s)
{
    Suggestion out;
    const qsizetype from = reply.indexOf(QLatin1Char('{'));
    const qsizetype to = reply.lastIndexOf(QLatin1Char('}'));
    if (from < 0 || to <= from) {
        out.error = QObject::tr("Die KI hat keine Werte geliefert.");
        return out;
    }
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(reply.mid(from, to - from + 1).toUtf8(), &err);
    if (!doc.isObject()) {
        out.error = QObject::tr("Die Antwort der KI war nicht lesbar: %1").arg(err.errorString());
        return out;
    }
    const QJsonObject o = doc.object();
    auto value = [&o](const char* key) { return clampValue(o.value(QLatin1String(key)).toDouble()); };
    if (s.color) {
        out.basic.temperature = value("temperature");
        out.basic.tint = value("tint");
    }
    if (s.saturation)
        out.basic.saturation = value("saturation");
    if (s.tone) {
        out.basic.brightness = value("brightness");
        out.basic.contrast = value("contrast");
        out.basic.highlights = value("highlights");
        out.basic.shadows = value("shadows");
        out.basic.midtones = value("midtones");
        out.stretch = o.value(QLatin1String("stretch")).toBool();
    }
    out.comment = o.value(QLatin1String("comment")).toString().trimmed();
    out.ok = true;
    return out;
}

adjust::Basic scaled(const adjust::Basic& b, int percent)
{
    const double f = percent / 100.0;
    adjust::Basic r;
    r.temperature = clampValue(b.temperature * f);
    r.tint = clampValue(b.tint * f);
    r.saturation = clampValue(b.saturation * f);
    r.brightness = clampValue(b.brightness * f);
    r.contrast = clampValue(b.contrast * f);
    r.highlights = clampValue(b.highlights * f);
    r.shadows = clampValue(b.shadows * f);
    r.midtones = clampValue(b.midtones * f);
    return r;
}

QSize cloudSize(const QSize& image)
{
    // gpt-image: 1024x1024, 1536x1024, 1024x1536
    const double r = image.height() > 0 ? double(image.width()) / image.height() : 1.0;
    const QList<QSize> sizes{{1024, 1024}, {1536, 1024}, {1024, 1536}};
    QSize best = sizes.first();
    for (const QSize& s : sizes)
        if (std::abs(std::log(double(s.width()) / s.height()) - std::log(r)) < std::abs(std::log(double(best.width()) / best.height()) - std::log(r)))
            best = s;
    return best;
}

QImage padForCloud(const QImage& image, QRect* content)
{
    const QSize target = cloudSize(image.size());
    const QSize fit = image.size().scaled(target, Qt::KeepAspectRatio);
    const QRect r(QPoint((target.width() - fit.width()) / 2, (target.height() - fit.height()) / 2), fit);
    QImage out(target, QImage::Format_RGB32);
    out.fill(QColor(128, 128, 128));
    QPainter p(&out);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.drawImage(r, image.convertToFormat(QImage::Format_RGB32));
    p.end();
    if (content)
        *content = r;
    return out;
}

QImage unpadFromCloud(const QImage& result, const QRect& content, const QSize& original)
{
    // Die Antwort kann eine andere Grösse haben als die Anfrage: Rand im gleichen Verhältnis
    const QSize sent = cloudSize(original);
    const double sx = double(result.width()) / sent.width();
    const double sy = double(result.height()) / sent.height();
    const QRect inResult(qRound(content.x() * sx), qRound(content.y() * sy), qRound(content.width() * sx), qRound(content.height() * sy));
    return result.copy(inResult).convertToFormat(QImage::Format_ARGB32).scaled(original, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}


const QList<TaskInfo>& tasks()
{
    static const QList<TaskInfo> list{
        {CastTask, QObject::tr("Farbstich entfernen, Weissabgleich"),
         "remove every colour cast and set a neutral, natural white balance", false},
        {ExposureTask, QObject::tr("Belichtung und Kontrast"),
         "correct the exposure and contrast, recover blown highlights and open up dark shadows", false},
        {ColorsTask, QObject::tr("Natürliche, kräftige Farben"),
         "give natural, pleasant colours with healthy saturation", false},
        {SkyTask, QObject::tr("Himmel betonen"),
         "where there is sky, make it a clear natural blue with defined clouds", false},
        {SharpenTask, QObject::tr("Schärfen, Details herausarbeiten"),
         "sharpen the image and bring out fine detail without halos", true},
        {DenoiseTask, QObject::tr("Rauschen entfernen"),
         "remove image noise and compression artefacts while keeping real texture", true},
        {SkinTask, QObject::tr("Haut glätten (Porträt)"),
         "gently retouch skin: reduce blemishes and shine, keep natural skin texture and the person's identity", true},
        {RestoreTask, QObject::tr("Altes Foto restaurieren"),
         "restore the old photo: remove scratches, dust, stains and fading", true},
        {ColorizeTask, QObject::tr("Schwarzweissfoto kolorieren"),
         "colourise the black-and-white photo with realistic, period-appropriate colours", true},
    };
    return list;
}

bool changesContent(int flags)
{
    for (const TaskInfo& t : tasks())
        if ((flags & t.flag) && t.changesContent)
            return true;
    return false;
}

QString editPrompt(const EditSettings& s)
{
    QStringList jobs;
    for (const TaskInfo& t : tasks())
        if (s.tasks & t.flag)
            jobs << QString::fromLatin1(t.prompt);
    QString p = QStringLiteral("You are a professional photo retoucher. Edit this photo.\n");
    if (!jobs.isEmpty())
        p += QStringLiteral("Tasks: %1.\n").arg(jobs.join(QStringLiteral("; ")));
    p += QStringLiteral("Look: %1.\n").arg(QLatin1String(goalText(s.goal)));
    if (!s.wish.trimmed().isEmpty())
        p += QStringLiteral("Additional wish of the user: %1\n").arg(s.wish.trimmed());
    p += QStringLiteral(
        "Keep the composition exactly as it is: same framing, same size and position of every object, no cropping, "
        "no zoom, nothing added, removed or moved, same people with the same faces. If the image has a plain grey "
        "border, keep it unchanged. The result must look like a natural, high-quality photograph.");
    return p;
}

QStringList imageModels(const QString& kind)
{
    if (kind == QLatin1String("openai"))
        return {QStringLiteral("gpt-image-2"), QStringLiteral("gpt-image-1.5"), QStringLiteral("gpt-image-1-mini")};
    if (kind == QLatin1String("gemini"))
        return {QStringLiteral("gemini-2.5-flash-image"), QStringLiteral("gemini-3-pro-image-preview")};
    return {};
}

QByteArray geminiRequest(const QString& prompt, const QByteArray& png)
{
    const QJsonObject text{{QStringLiteral("text"), prompt}};
    const QJsonObject image{{QStringLiteral("inline_data"),
                             QJsonObject{{QStringLiteral("mime_type"), QStringLiteral("image/png")},
                                         {QStringLiteral("data"), QString::fromLatin1(png.toBase64())}}}};
    const QJsonObject body{
        {QStringLiteral("contents"), QJsonArray{QJsonObject{{QStringLiteral("parts"), QJsonArray{text, image}}}}},
        {QStringLiteral("generationConfig"),
         QJsonObject{{QStringLiteral("responseModalities"), QJsonArray{QStringLiteral("TEXT"), QStringLiteral("IMAGE")}}}}};
    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

QImage geminiImage(const QByteArray& body, QString* text)
{
    const QJsonArray candidates = QJsonDocument::fromJson(body).object().value(QStringLiteral("candidates")).toArray();
    QStringList said;
    for (const QJsonValue& c : candidates) {
        const QJsonArray parts = c.toObject().value(QStringLiteral("content")).toObject().value(QStringLiteral("parts")).toArray();
        for (const QJsonValue& part : parts) {
            const QJsonObject o = part.toObject();
            QJsonObject data = o.value(QStringLiteral("inlineData")).toObject();
            if (data.isEmpty())
                data = o.value(QStringLiteral("inline_data")).toObject();
            if (!data.isEmpty()) {
                QImage image;
                if (image.loadFromData(QByteArray::fromBase64(data.value(QStringLiteral("data")).toString().toLatin1())))
                    return image;
            }
            if (o.contains(QStringLiteral("text")))
                said << o.value(QStringLiteral("text")).toString().trimmed();
        }
        if (said.isEmpty() && c.toObject().contains(QStringLiteral("finishReason")))
            said << QObject::tr("Abbruchgrund: %1").arg(c.toObject().value(QStringLiteral("finishReason")).toString());
    }
    if (text)
        *text = said.join(QLatin1Char(' '));
    return {};
}

QString geminiEndpoint(const QString& baseUrl, const QString& model)
{
    QString root = baseUrl.trimmed().isEmpty() ? QStringLiteral("https://generativelanguage.googleapis.com/v1beta") : baseUrl.trimmed();
    while (root.endsWith(QLatin1Char('/')))
        root.chop(1);
    // Der Zugang zeigt auf die OpenAI-kompatible Schnittstelle; Bilder gibt es nur über die eigene
    if (root.endsWith(QLatin1String("/openai")))
        root.chop(7);
    return QStringLiteral("%1/models/%2:generateContent").arg(root, model);
}

} // namespace aiopt

AiImageDialog::AiImageDialog(const QImage& image, QWidget* parent, bool cutout)
    : ImagePreviewDialog(cutout ? tr("Bild freistellen") : tr("Bild mit KI optimieren"), image, parent)
{
    QSettings settings;
    auto* layout = new QVBoxLayout(settingsArea());
    layout->setContentsMargins(0, 0, 0, 0);
    m_form = new QFormLayout;
    QFormLayout* form = m_form;

    m_method = new QComboBox(this);
    m_method->setObjectName(QStringLiteral("method"));
    m_method->addItem(tr("KI bearbeitet das Bild"));
    m_method->addItem(tr("KI schlägt Reglerwerte vor"));
    m_method->setItemData(0, tr("Ein Bildmodell (OpenAI, Google Gemini) bearbeitet eine Kopie des Bildes nach den gewählten "
                                "Aufgaben. TANGENTA überträgt das Ergebnis aufs Original."), Qt::ToolTipRole);
    m_method->setItemData(1, tr("Die KI sieht eine verkleinerte Kopie und schlägt nur Werte für die Regler vor; gerechnet "
                                "wird in TANGENTA. Schnell und günstig, aber begrenzt auf diese Regler."), Qt::ToolTipRole);
    m_method->setCurrentIndex(std::clamp(settings.value(QStringLiteral("aiOptimize/method"), 0).toInt(), 0, 1));
    form->addRow(tr("Verfahren:"), m_method);

    // Zugänge mit eigener Schnittstelle (Claude Code bekommt hier kein Bild)
    m_account = new QComboBox(this);
    m_account->setObjectName(QStringLiteral("account"));
    auto* setup = new QPushButton(tr("Einrichten …"), this);
    setup->setObjectName(QStringLiteral("accountSetup"));
    setup->setToolTip(tr("Öffnet Extras > Optionen > Global > KI: dort einen Zugang mit API-Schlüssel anlegen"));
    connect(setup, &QPushButton::clicked, this, &AiImageDialog::accountSetupRequested);
    auto* accountRow = new QHBoxLayout;
    accountRow->addWidget(m_account, 1);
    accountRow->addWidget(setup);
    form->addRow(tr("KI-Zugang:"), accountRow);

    m_imageModel = new QComboBox(this);
    m_imageModel->setObjectName(QStringLiteral("imageModel"));
    m_imageModel->setEditable(true);
    m_imageModel->setToolTip(tr("Bildmodell des Anbieters; eigene Modellnamen lassen sich eintippen"));
    m_quality = new QComboBox(this);
    m_quality->setObjectName(QStringLiteral("quality"));
    m_quality->addItem(tr("Hoch"), QStringLiteral("high"));
    m_quality->addItem(tr("Mittel"), QStringLiteral("medium"));
    m_quality->addItem(tr("Niedrig"), QStringLiteral("low"));
    m_quality->setToolTip(tr("Qualität bei OpenAI: höher ist genauer, dauert länger und kostet mehr"));
    m_quality->setCurrentIndex(std::clamp(settings.value(QStringLiteral("aiOptimize/quality"), 0).toInt(), 0, 2));
    auto* modelRow = new QHBoxLayout;
    modelRow->addWidget(m_imageModel, 1);
    modelRow->addWidget(m_quality);
    form->addRow(tr("Bildmodell:"), modelRow);

    m_goal = new QComboBox(this);
    m_goal->addItems({tr("Natürlich und ausgewogen"), tr("Lebendig, kräftige Farben"), tr("Warm"), tr("Kühl"),
                      tr("Hell und freundlich"), tr("Stimmungsvoll, dunkler"), tr("Für den Druck")});
    m_goal->setCurrentIndex(std::clamp(settings.value(QStringLiteral("aiOptimize/goal"), 0).toInt(), 0, m_goal->count() - 1));
    form->addRow(tr("Ziel:"), m_goal);

    // Aufgaben für das Bildmodell
    m_taskBox = new QWidget(this);
    auto* taskGrid = new QGridLayout(m_taskBox);
    taskGrid->setContentsMargins(0, 0, 0, 0);
    taskGrid->setVerticalSpacing(2);
    const int chosen = settings.value(QStringLiteral("aiOptimize/tasks"), aiopt::EditSettings{}.tasks).toInt();
    for (const aiopt::TaskInfo& t : aiopt::tasks()) {
        auto* box = new QCheckBox(t.label, m_taskBox);
        box->setProperty("task", t.flag);
        box->setChecked(chosen & t.flag);
        if (t.changesContent)
            box->setToolTip(tr("Verändert Bildinhalte: wird nur mit „KI-Bild übernehmen“ übertragen"));
        taskGrid->addWidget(box, int(m_tasks.size()) / 2, int(m_tasks.size()) % 2);
        m_tasks << box;
    }
    form->addRow(tr("Verbessern:"), m_taskBox);

    m_color = new QCheckBox(tr("Farbstich (Temperatur, Tönung)"), this);
    m_tone = new QCheckBox(tr("Belichtung und Kontrast"), this);
    m_saturation = new QCheckBox(tr("Sättigung"), this);
    m_color->setChecked(settings.value(QStringLiteral("aiOptimize/color"), true).toBool());
    m_tone->setChecked(settings.value(QStringLiteral("aiOptimize/tone"), true).toBool());
    m_saturation->setChecked(settings.value(QStringLiteral("aiOptimize/saturation"), true).toBool());
    m_allowedBox = new QWidget(this);
    auto* allowed = new QVBoxLayout(m_allowedBox);
    allowed->setContentsMargins(0, 0, 0, 0);
    allowed->setSpacing(2);
    allowed->addWidget(m_color);
    allowed->addWidget(m_tone);
    allowed->addWidget(m_saturation);
    form->addRow(tr("Die KI darf ändern:"), m_allowedBox);

    m_wish = new QLineEdit(this);
    m_wish->setPlaceholderText(tr("z.B. Himmel etwas blauer, Gesichter heller"));
    m_wish->setClearButtonEnabled(true);
    form->addRow(tr("Wunsch:"), m_wish);

    m_apply = new QComboBox(this);
    m_apply->setObjectName(QStringLiteral("apply"));
    m_apply->addItem(tr("Farbe und Licht aufs Original übertragen"));
    m_apply->addItem(tr("KI-Bild übernehmen"));
    m_apply->setItemData(0, tr("Das Original bleibt in voller Auflösung, Farbtiefe und Schärfe; übernommen werden Farben, "
                               "Weissabgleich, Belichtung und grossflächige Korrekturen der KI."), Qt::ToolTipRole);
    m_apply->setItemData(1, tr("Das Bild der KI ersetzt das Original (höchstens 1536 Pixel, wird hochgerechnet). Nötig für "
                               "Schärfen, Entrauschen, Retusche, Restaurieren und Kolorieren; Details können sich ändern."),
                         Qt::ToolTipRole);
    m_apply->setCurrentIndex(std::clamp(settings.value(QStringLiteral("aiOptimize/apply"), 0).toInt(), 0, 1));
    form->addRow(tr("Übernehmen:"), m_apply);
    layout->addLayout(form);

    m_ask = new QPushButton(this);
    m_ask->setObjectName(QStringLiteral("ask"));
    layout->addWidget(m_ask);
    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("status"));
    m_status->setWordWrap(true);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_status);

    m_values = new QFormLayout;
    QFormLayout* values = m_values;
    m_variant = new QComboBox(this);
    m_variant->setObjectName(QStringLiteral("variant"));
    values->addRow(tr("Variante:"), m_variant);
    m_strength = addEffectSlider(values, SliderSpec{tr("Stärke (%):"), 0, 150, 100}, this);
    m_strength->setValue(std::clamp(settings.value(QStringLiteral("aiOptimize/strength"), 100).toInt(), 0, 150));
    m_local = addEffectSlider(values, SliderSpec{tr("Lokal (%):"), 0, 100, 80}, this);
    m_local->setObjectName(QStringLiteral("local"));
    m_local->setToolTip(tr("Wie fein grossflächige Korrekturen der KI (Himmel, aufgehellte Bereiche) übernommen werden; "
                           "0 = nur eine Farbabbildung fürs ganze Bild"));
    m_local->setValue(std::clamp(settings.value(QStringLiteral("aiOptimize/local"), 80).toInt(), 0, 100));
    for (const QString& name : {tr("Temperatur:"), tr("Tönung:"), tr("Sättigung:"), tr("Helligkeit:"), tr("Kontrast:"),
                                tr("Glanzlichter:"), tr("Schatten:"), tr("Mitteltöne:")})
        m_sliders << addEffectSlider(values, SliderSpec{name}, this);
    layout->addLayout(values);
    m_stretch = new QCheckBox(tr("Tonwerte vorher strecken (wie Automatische Anpassung)"), this);
    layout->addWidget(m_stretch);

    // Freistellen
    auto* cutForm = new QFormLayout;
    m_cutout = new QCheckBox(tr("Motiv freistellen (Hintergrund entfernen)"), this);
    cutForm->addRow(m_cutout);
    m_cutoutMethod = new QComboBox(this);
    m_cutoutMethod->addItem(tr("Lokal, pixelgenau (offline)"));
    m_cutoutMethod->addItem(tr("OpenAI (zeichnet das Motiv neu)"));
    m_cutoutMethod->setItemData(0, tr("Ein Freistellmodell rechnet auf diesem Rechner eine Maske; das Motiv bleibt Pixel für "
                                      "Pixel das Original. Kein Bild verlässt den Rechner."), Qt::ToolTipRole);
    m_cutoutMethod->setItemData(1, tr("Schickt das Bild an OpenAI (gpt-image-1.5). Die KI liefert das Motiv neu gezeichnet "
                                      "auf transparentem Grund: Details, Schrift und Gesichter können sich verändern."), Qt::ToolTipRole);
    m_cutoutMethod->setCurrentIndex(std::clamp(settings.value(QStringLiteral("aiOptimize/cutoutMethod"), 0).toInt(), 0, 1));
    cutForm->addRow(tr("Verfahren:"), m_cutoutMethod);
    m_hardness = addEffectSlider(cutForm, SliderSpec{tr("Kantenhärte:"), 0, 100, 0}, this);
    m_hardness->setValue(std::clamp(settings.value(QStringLiteral("aiOptimize/hardness"), 0).toInt(), 0, 100));
    layout->addLayout(cutForm);
    m_cutoutStatus = new QLabel(this);
    m_cutoutStatus->setWordWrap(true);
    layout->addWidget(m_cutoutStatus);
    layout->addStretch(1);
    connect(m_cutout, &QCheckBox::toggled, this, [this](bool on) {
        if (on)
            startCutout();
        refreshPreview();
    });
    connect(m_cutoutMethod, &QComboBox::currentIndexChanged, this, [this] {
        if (m_cutout->isChecked())
            startCutout();
        refreshPreview();
    });
    connect(m_hardness, &QSlider::valueChanged, this, &AiImageDialog::refreshPreview);
    if (cutout)
        QMetaObject::invokeMethod(this, [this] { setCutout(true); }, Qt::QueuedConnection);

    connect(m_method, &QComboBox::currentIndexChanged, this, [this] {
        reloadAccounts();
        updateMode();
        refreshPreview();
    });
    connect(m_account, &QComboBox::currentIndexChanged, this, [this] {
        fillImageModels();
        updateMode();
    });
    connect(m_apply, &QComboBox::currentIndexChanged, this, [this] {
        updateMode();
        refreshPreview();
    });
    for (QCheckBox* box : std::as_const(m_tasks))
        connect(box, &QCheckBox::toggled, this, [this, box](bool on) {
            // Aufgaben, die Bildinhalte verändern, lassen sich nicht als Farbe und Licht übertragen
            if (on && aiopt::changesContent(box->property("task").toInt()) && applyMode() == Apply::Transfer) {
                setApplyMode(Apply::Replace);
                m_status->setText(tr("„%1“ verändert Bildinhalte, deshalb wird das KI-Bild selbst übernommen.").arg(box->text()));
            }
        });
    connect(m_variant, &QComboBox::currentIndexChanged, this, &AiImageDialog::refreshPreview);
    connect(m_local, &QSlider::valueChanged, this, &AiImageDialog::refreshPreview);
    connect(m_ask, &QPushButton::clicked, this, [this] {
        if (method() == Method::Edit)
            editImage();
        else
            ask();
    });
    connect(m_wish, &QLineEdit::returnPressed, m_ask, &QPushButton::click);
    connect(m_strength, &QSlider::valueChanged, this, &AiImageDialog::refreshPreview);
    for (QSlider* s : std::as_const(m_sliders))
        connect(s, &QSlider::valueChanged, this, &AiImageDialog::refreshPreview);
    connect(m_stretch, &QCheckBox::toggled, this, &AiImageDialog::refreshPreview);
    connect(this, &QDialog::accepted, this, &AiImageDialog::saveSettings);
    reloadAccounts();
    fillImageModels();
    updateMode();
    finishLayout();
}

AiImageDialog::~AiImageDialog()
{
    if (m_provider)
        m_provider->abort();
}

aiopt::Settings AiImageDialog::settings() const
{
    aiopt::Settings s;
    s.goal = aiopt::Goal(m_goal->currentIndex());
    s.color = m_color->isChecked();
    s.tone = m_tone->isChecked();
    s.saturation = m_saturation->isChecked();
    s.wish = m_wish->text();
    return s;
}

adjust::Basic AiImageDialog::basic() const
{
    adjust::Basic b;
    int* fields[] = {&b.temperature, &b.tint, &b.saturation, &b.brightness, &b.contrast, &b.highlights, &b.shadows, &b.midtones};
    for (int i = 0; i < m_sliders.size(); ++i)
        *fields[i] = m_sliders[i]->value();
    return b;
}

void AiImageDialog::setSuggestion(const aiopt::Suggestion& s)
{
    const int values[] = {s.basic.temperature, s.basic.tint, s.basic.saturation, s.basic.brightness,
                          s.basic.contrast, s.basic.highlights, s.basic.shadows, s.basic.midtones};
    for (int i = 0; i < m_sliders.size(); ++i)
        m_sliders[i]->setValue(values[i]);
    m_stretch->setChecked(s.stretch);
    refreshPreview();
}

void AiImageDialog::reloadAccounts()
{
    QSettings settings;
    const QString current = m_account->currentIndex() >= 0
        ? m_account->currentData().toString()
        : settings.value(QStringLiteral("aiOptimize/account"), ai::currentAccount().id).toString();
    const bool edit = method() == Method::Edit;
    const QSignalBlocker block(m_account);
    m_accounts.clear();
    m_account->clear();
    for (const ai::Account& a : ai::accounts()) {
        // Claude Code bekommt hier kein Bild; bearbeiten können nur Anbieter mit Bildmodell
        if (a.kind == QLatin1String("claude_code") || (edit && aiopt::imageModels(a.kind).isEmpty()))
            continue;
        m_accounts << a;
        m_account->addItem(a.displayName(), a.id);
        if (a.id == current)
            m_account->setCurrentIndex(m_account->count() - 1);
    }
    const bool none = m_accounts.isEmpty();
    m_account->setEnabled(!none);
    if (!asking())
        m_ask->setEnabled(!none);
    const QString missing = tr("Noch kein passender KI-Zugang.");
    if (none)
        m_status->setText(edit ? missing + QLatin1Char(' ')
                                     + tr("Zum Bearbeiten braucht es einen Zugang zu OpenAI oder Google Gemini mit "
                                          "API-Schlüssel: „Einrichten …“. Der Zugang über Claude Code kann keine Bilder "
                                          "übergeben.")
                               : missing + QLatin1Char(' ')
                                     + tr("Mit „Einrichten …“ einen Zugang mit API-Schlüssel anlegen (Claude, OpenAI, Gemini, "
                                          "Ollama …). Der Zugang über Claude Code kann hier keine Bilder übergeben."));
    else if (m_status->text().startsWith(missing))
        m_status->clear();
    fillImageModels();
}

const ai::Account* AiImageDialog::selectedAccount() const
{
    const int index = m_account->currentIndex();
    return index >= 0 && index < m_accounts.size() ? &m_accounts[index] : nullptr;
}

void AiImageDialog::fillImageModels()
{
    if (!m_imageModel)
        return;
    const ai::Account* account = selectedAccount();
    const QString kind = account ? account->kind : QString();
    const QStringList models = aiopt::imageModels(kind);
    const QSignalBlocker block(m_imageModel);
    m_imageModel->clear();
    m_imageModel->addItems(models);
    const QString saved = QSettings().value(QStringLiteral("aiOptimize/imageModel/") + kind).toString();
    m_imageModel->setCurrentText(saved.isEmpty() && !models.isEmpty() ? models.first() : saved);
    m_quality->setVisible(kind == QLatin1String("openai"));
}

AiImageDialog::Method AiImageDialog::method() const
{
    return m_method->currentIndex() == 1 ? Method::Values : Method::Edit;
}

void AiImageDialog::setMethod(Method m)
{
    m_method->setCurrentIndex(m == Method::Values ? 1 : 0);
}

AiImageDialog::Apply AiImageDialog::applyMode() const
{
    return m_apply->currentIndex() == 1 ? Apply::Replace : Apply::Transfer;
}

void AiImageDialog::setApplyMode(Apply apply)
{
    m_apply->setCurrentIndex(apply == Apply::Replace ? 1 : 0);
}

aiopt::EditSettings AiImageDialog::editSettings() const
{
    aiopt::EditSettings s;
    s.goal = aiopt::Goal(m_goal->currentIndex());
    s.tasks = 0;
    for (QCheckBox* box : m_tasks)
        if (box->isChecked())
            s.tasks |= box->property("task").toInt();
    s.wish = m_wish->text();
    return s;
}

void AiImageDialog::updateMode()
{
    const bool edit = method() == Method::Edit;
    setRowShown(m_form, m_taskBox, edit);
    setRowShown(m_form, m_allowedBox, !edit);
    setRowShown(m_form, m_apply, edit);
    setRowShown(m_form, m_imageModel, edit);
    setRowShown(m_values, m_variant, edit && m_edits.size() > 1);
    setRowShown(m_values, m_local, edit && applyMode() == Apply::Transfer);
    if (!asking())
        m_ask->setText(edit ? (m_edits.isEmpty() ? tr("KI bearbeiten lassen") : tr("Weitere Variante"))
                            : tr("KI fragen"));
    m_ask->setToolTip(edit ? tr("Schickt das Bild (höchstens 1536 Pixel) an den gewählten Anbieter. Abgerechnet wird beim "
                                "Anbieter nach Verbrauch.")
                           : tr("Schickt eine verkleinerte Kopie des Bildes an den gewählten KI-Zugang. Die KI schlägt "
                                "nur Reglerwerte vor; gerechnet wird in TANGENTA auf dem Original."));
}

bool AiImageDialog::asking() const
{
    return (m_provider && m_provider->busy()) || m_editReply;
}

void AiImageDialog::setBusy(bool busy, const QString& status)
{
    m_ask->setEnabled(!busy && !m_accounts.isEmpty());
    if (method() == Method::Edit)
        m_ask->setText(busy ? tr("KI bearbeitet das Bild …") : tr("Weitere Variante"));
    else
        m_ask->setText(busy ? tr("KI denkt nach …") : tr("KI erneut fragen"));
    if (!status.isNull())
        m_status->setText(status);
}

void AiImageDialog::saveSettings() const
{
    QSettings settings;
    if (m_account->currentIndex() >= 0)
        settings.setValue(QStringLiteral("aiOptimize/account"), m_account->currentData());
    settings.setValue(QStringLiteral("aiOptimize/goal"), m_goal->currentIndex());
    settings.setValue(QStringLiteral("aiOptimize/method"), m_method->currentIndex());
    settings.setValue(QStringLiteral("aiOptimize/tasks"), editSettings().tasks);
    settings.setValue(QStringLiteral("aiOptimize/apply"), m_apply->currentIndex());
    settings.setValue(QStringLiteral("aiOptimize/local"), m_local->value());
    settings.setValue(QStringLiteral("aiOptimize/quality"), m_quality->currentIndex());
    if (const ai::Account* a = selectedAccount(); a && !m_imageModel->currentText().trimmed().isEmpty())
        settings.setValue(QStringLiteral("aiOptimize/imageModel/") + a->kind, m_imageModel->currentText().trimmed());
    settings.setValue(QStringLiteral("aiOptimize/color"), m_color->isChecked());
    settings.setValue(QStringLiteral("aiOptimize/tone"), m_tone->isChecked());
    settings.setValue(QStringLiteral("aiOptimize/saturation"), m_saturation->isChecked());
    settings.setValue(QStringLiteral("aiOptimize/strength"), m_strength->value());
    settings.setValue(QStringLiteral("aiOptimize/cutoutMethod"), m_cutoutMethod->currentIndex());
    settings.setValue(QStringLiteral("aiOptimize/hardness"), m_hardness->value());
}

void AiImageDialog::setCutout(bool on)
{
    m_cutout->setChecked(on);
}

bool AiImageDialog::cutout() const
{
    return m_cutout->isChecked();
}

AiImageDialog::CutoutMethod AiImageDialog::cutoutMethod() const
{
    return m_cutoutMethod->currentIndex() == 1 ? CutoutMethod::Cloud : CutoutMethod::Local;
}

void AiImageDialog::setCutoutMask(const QImage& mask)
{
    m_mask = mask;
    refreshPreview();
}

void AiImageDialog::setCloudCutout(const QImage& rgba)
{
    m_cloudCutout = rgba;
    refreshPreview();
}

void AiImageDialog::cutoutStatus(const QString& text)
{
    m_cutoutStatus->setText(text);
}

void AiImageDialog::startCutout()
{
    if (m_cutoutBusy)
        return;
    if (cutoutMethod() == CutoutMethod::Local) {
        if (!m_mask.isNull())
            return;
        QString error;
        if (!matting::runtimeAvailable(&error)) {
            cutoutStatus(error);
            return;
        }
        if (!matting::modelPresent(matting::defaultModel())) {
            downloadModel();
            return;
        }
        runLocalCutout();
    } else {
        if (m_cloudCutout.isNull())
            runCloudCutout();
    }
}

void AiImageDialog::runLocalCutout()
{
    m_cutoutBusy = true;
    cutoutStatus(tr("Motiv wird erkannt … (einige Sekunden)"));
    QImage* result = new QImage;
    QString* error = new QString;
    const QImage source = m_original;
    QThread* thread = QThread::create([source, result, error] {
        *result = matting::subjectMask(source, matting::defaultModel(), error);
    });
    connect(thread, &QThread::finished, this, [this, thread, result, error] {
        m_cutoutBusy = false;
        if (result->isNull()) {
            cutoutStatus(tr("Freistellen ging nicht: %1").arg(*error));
        } else {
            m_mask = *result;
            cutoutStatus(tr("Motiv freigestellt (%1). Kantenhärte nach Wunsch.").arg(matting::defaultModel().name));
            refreshPreview();
        }
        delete result;
        delete error;
        thread->deleteLater();
    });
    thread->start();
}

void AiImageDialog::downloadModel()
{
    const matting::Model& model = matting::defaultModel();
    const auto answer = QMessageBox::question(
        this, tr("Freistellmodell herunterladen"),
        tr("Zum Freistellen braucht TANGENTA einmalig das Modell %1 (%2 MB, Lizenz %3). Es wird von\n%4\n"
           "heruntergeladen, geprüft und unter\n%5\ngespeichert. Jetzt herunterladen?")
            .arg(model.name)
            .arg(model.bytes / 1000000)
            .arg(model.license, QUrl(model.url).host(), QDir::toNativeSeparators(matting::modelDirectory())));
    if (answer != QMessageBox::Yes) {
        m_cutout->setChecked(false);
        return;
    }
    QDir().mkpath(matting::modelDirectory());
    const QString target = matting::modelPath(model);
    auto* file = new QFile(target + QStringLiteral(".part"), this);
    if (!file->open(QIODevice::WriteOnly)) {
        cutoutStatus(tr("Kann %1 nicht schreiben.").arg(QDir::toNativeSeparators(file->fileName())));
        delete file;
        return;
    }
    if (!m_net)
        m_net = new QNetworkAccessManager(this);
    QNetworkRequest request{QUrl(model.url)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_net->get(request);
    auto* hash = new QCryptographicHash(QCryptographicHash::Sha256);
    m_cutoutBusy = true;
    connect(reply, &QNetworkReply::readyRead, this, [reply, file, hash] {
        const QByteArray chunk = reply->readAll();
        hash->addData(chunk);
        file->write(chunk);
    });
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 got, qint64 total) {
        cutoutStatus(tr("Modell wird heruntergeladen: %1 von %2 MB").arg(got / 1000000).arg(std::max<qint64>(total, 0) / 1000000));
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, file, hash, target, model] {
        const QByteArray rest = reply->readAll();
        hash->addData(rest);
        file->write(rest);
        file->close();
        const bool ok = reply->error() == QNetworkReply::NoError && hash->result().toHex() == model.sha256;
        const QString error = reply->error() != QNetworkReply::NoError ? reply->errorString() : tr("Prüfsumme stimmt nicht");
        delete hash;
        reply->deleteLater();
        m_cutoutBusy = false;
        if (!ok) {
            file->remove();
            file->deleteLater();
            cutoutStatus(tr("Download fehlgeschlagen: %1").arg(error));
            m_cutout->setChecked(false);
            return;
        }
        QFile::remove(target);
        file->rename(target);
        file->deleteLater();
        runLocalCutout();
    });
}

void AiImageDialog::runCloudCutout()
{
    // Zugang mit OpenAI-Schlüssel: der gewählte, sonst der erste
    std::optional<ai::Account> account;
    const int index = m_account->currentIndex();
    if (index >= 0 && index < m_accounts.size() && m_accounts[index].kind == QLatin1String("openai"))
        account = m_accounts[index];
    for (const ai::Account& a : std::as_const(m_accounts))
        if (!account && a.kind == QLatin1String("openai") && !a.key.isEmpty())
            account = a;
    if (!account) {
        cutoutStatus(tr("Dafür braucht es einen OpenAI-Zugang mit API-Schlüssel (Optionen > Global > KI)."));
        return;
    }
    QRect content;
    const QImage padded = aiopt::padForCloud(m_original, &content);
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    padded.save(&buffer, "PNG");
    auto* multi = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    auto field = [multi](const QString& name, const QByteArray& value) {
        QHttpPart part;
        part.setHeader(QNetworkRequest::ContentDispositionHeader, QStringLiteral("form-data; name=\"%1\"").arg(name));
        part.setBody(value);
        multi->append(part);
    };
    const QSize size = aiopt::cloudSize(m_original.size());
    field(QStringLiteral("model"), "gpt-image-1.5");
    field(QStringLiteral("prompt"), "Remove the background completely. Keep the main subject exactly as it is: same position, size, "
                                    "shape, colours and details. Output only the subject on a transparent background.");
    field(QStringLiteral("background"), "transparent");
    field(QStringLiteral("output_format"), "png");
    field(QStringLiteral("input_fidelity"), "high");
    field(QStringLiteral("quality"), "high");
    field(QStringLiteral("size"), QStringLiteral("%1x%2").arg(size.width()).arg(size.height()).toLatin1());
    QHttpPart image;
    image.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("image/png"));
    image.setHeader(QNetworkRequest::ContentDispositionHeader, QStringLiteral("form-data; name=\"image[]\"; filename=\"bild.png\""));
    image.setBody(png);
    multi->append(image);
    QString base = account->baseUrl.isEmpty() ? QStringLiteral("https://api.openai.com/v1") : account->baseUrl;
    while (base.endsWith(QLatin1Char('/')))
        base.chop(1);
    QNetworkRequest request{QUrl(base + QStringLiteral("/images/edits"))};
    request.setRawHeader("Authorization", "Bearer " + account->key.toUtf8());
    request.setTransferTimeout(300000);
    if (!m_net)
        m_net = new QNetworkAccessManager(this);
    QNetworkReply* reply = m_net->post(request, multi);
    multi->setParent(reply);
    m_cutoutBusy = true;
    cutoutStatus(tr("Bild wird an OpenAI geschickt … (bis zu einer Minute)"));
    const QSize original = m_original.size();
    connect(reply, &QNetworkReply::finished, this, [this, reply, content, original] {
        reply->deleteLater();
        m_cutoutBusy = false;
        const QByteArray body = reply->readAll();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || status != 200) {
            cutoutStatus(ai::explainFailure(QStringLiteral("OpenAI"), status, body, reply->errorString()));
            return;
        }
        const QJsonArray data = QJsonDocument::fromJson(body).object().value(QStringLiteral("data")).toArray();
        QImage result;
        if (!data.isEmpty())
            result.loadFromData(QByteArray::fromBase64(data.first().toObject().value(QStringLiteral("b64_json")).toString().toLatin1()));
        if (result.isNull()) {
            cutoutStatus(tr("OpenAI hat kein Bild geliefert."));
            return;
        }
        m_cloudCutout = aiopt::unpadFromCloud(result, content, original);
        cutoutStatus(tr("Von OpenAI freigestellt. Achtung: Das Motiv ist neu gezeichnet und kann vom Original abweichen."));
        refreshPreview();
    });
}

void AiImageDialog::ask()
{
    const int index = m_account->currentIndex();
    if (asking() || index < 0 || index >= m_accounts.size())
        return;
    saveSettings();
    if (m_provider)
        m_provider->deleteLater();
    m_provider = ai::Provider::create(m_accounts[index], this);
    if (!m_provider)
        return;
    // Verkleinerte Kopie in 8 Bit; Statistik vom ganzen Bild
    const QImage send = m_original.scaled(kSendSize, kSendSize, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                            .convertToFormat(QImage::Format_RGB888);
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    send.save(&buffer, "PNG");
    const aiopt::Settings s = settings();
    ai::Message message;
    message.role = ai::Message::Role::User;
    message.text = aiopt::question(s, m_original, QLocale().nativeLanguageName());
    message.images = {png};
    connect(m_provider, &ai::Provider::done, this, [this, s](const ai::Reply& reply) {
        const aiopt::Suggestion suggestion = aiopt::parse(reply.text, s);
        if (suggestion.ok) {
            setSuggestion(suggestion);
            setBusy(false, suggestion.comment.isEmpty() ? tr("Vorschlag übernommen.") : suggestion.comment);
        } else {
            setBusy(false, suggestion.error);
        }
    });
    connect(m_provider, &ai::Provider::failed, this, [this](const QString& message) { setBusy(false, message); });
    connect(m_provider, &ai::Provider::retrying, this, [this](int attempt, const QString& reason) {
        setBusy(true, tr("Versuch %1: %2").arg(attempt + 1).arg(reason));
    });
    setBusy(true, tr("Bild wird an %1 geschickt …").arg(m_account->currentText()));
    m_provider->round(aiopt::systemPrompt(), {message}, {});
}

void AiImageDialog::editImage()
{
    const ai::Account* account = selectedAccount();
    if (asking() || !account)
        return;
    saveSettings();
    const QString prompt = aiopt::editPrompt(editSettings());
    if (account->kind == QLatin1String("gemini"))
        requestGeminiEdit(*account, prompt);
    else
        requestOpenAiEdit(*account, prompt);
}

void AiImageDialog::editFailed(const QString& message)
{
    m_editReply = nullptr;
    setBusy(false, message);
    updateMode();
}

void AiImageDialog::requestOpenAiEdit(const ai::Account& account, const QString& prompt)
{
    QRect content;
    const QImage padded = aiopt::padForCloud(m_original, &content);
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    padded.save(&buffer, "PNG");
    auto* multi = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    auto field = [multi](const QString& name, const QByteArray& value) {
        QHttpPart part;
        part.setHeader(QNetworkRequest::ContentDispositionHeader, QStringLiteral("form-data; name=\"%1\"").arg(name));
        part.setBody(value);
        multi->append(part);
    };
    const QString model = m_imageModel->currentText().trimmed().isEmpty() ? aiopt::imageModels(account.kind).first()
                                                                          : m_imageModel->currentText().trimmed();
    const QSize size = aiopt::cloudSize(m_original.size());
    field(QStringLiteral("model"), model.toUtf8());
    field(QStringLiteral("prompt"), prompt.toUtf8());
    field(QStringLiteral("output_format"), "png");
    field(QStringLiteral("quality"), m_quality->currentData().toByteArray());
    field(QStringLiteral("size"), QStringLiteral("%1x%2").arg(size.width()).arg(size.height()).toLatin1());
    if (model.startsWith(QLatin1String("gpt-image-1")))
        field(QStringLiteral("input_fidelity"), "high");
    QHttpPart image;
    image.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("image/png"));
    image.setHeader(QNetworkRequest::ContentDispositionHeader, QStringLiteral("form-data; name=\"image[]\"; filename=\"bild.png\""));
    image.setBody(png);
    multi->append(image);
    QString base = account.baseUrl.isEmpty() ? QStringLiteral("https://api.openai.com/v1") : account.baseUrl;
    while (base.endsWith(QLatin1Char('/')))
        base.chop(1);
    QNetworkRequest request{QUrl(base + QStringLiteral("/images/edits"))};
    request.setRawHeader("Authorization", "Bearer " + account.key.toUtf8());
    request.setTransferTimeout(360000);
    if (!m_net)
        m_net = new QNetworkAccessManager(this);
    QNetworkReply* reply = m_net->post(request, multi);
    multi->setParent(reply);
    m_editReply = reply;
    setBusy(true, tr("Bild wird an OpenAI (%1) geschickt … das dauert meist eine halbe bis zwei Minuten.").arg(model));
    const QSize original = m_original.size();
    connect(reply, &QNetworkReply::finished, this, [this, reply, content, original, model] {
        reply->deleteLater();
        const QByteArray body = reply->readAll();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || status != 200)
            return editFailed(ai::explainFailure(QStringLiteral("OpenAI"), status, body, reply->errorString()));
        const QJsonArray data = QJsonDocument::fromJson(body).object().value(QStringLiteral("data")).toArray();
        QImage result;
        if (!data.isEmpty())
            result.loadFromData(QByteArray::fromBase64(data.first().toObject().value(QStringLiteral("b64_json")).toString().toLatin1()));
        if (result.isNull())
            return editFailed(tr("OpenAI hat kein Bild geliefert."));
        m_editReply = nullptr;
        addEditedImage(aiopt::unpadFromCloud(result, content, original), model);
    });
}

void AiImageDialog::requestGeminiEdit(const ai::Account& account, const QString& prompt)
{
    const QImage send = m_original.width() > 1536 || m_original.height() > 1536
        ? m_original.scaled(1536, 1536, Qt::KeepAspectRatio, Qt::SmoothTransformation)
        : m_original;
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    send.convertToFormat(QImage::Format_RGB888).save(&buffer, "PNG");
    const QString model = m_imageModel->currentText().trimmed().isEmpty() ? aiopt::imageModels(account.kind).first()
                                                                          : m_imageModel->currentText().trimmed();
    QNetworkRequest request{QUrl(aiopt::geminiEndpoint(account.baseUrl, model))};
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("x-goog-api-key", account.key.toUtf8());
    request.setTransferTimeout(360000);
    if (!m_net)
        m_net = new QNetworkAccessManager(this);
    QNetworkReply* reply = m_net->post(request, aiopt::geminiRequest(prompt, png));
    m_editReply = reply;
    setBusy(true, tr("Bild wird an Google Gemini (%1) geschickt … das dauert meist eine halbe Minute.").arg(model));
    const QSize original = m_original.size();
    connect(reply, &QNetworkReply::finished, this, [this, reply, original, model] {
        reply->deleteLater();
        const QByteArray body = reply->readAll();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || status != 200)
            return editFailed(ai::explainFailure(QStringLiteral("Google Gemini"), status, body, reply->errorString()));
        QString said;
        const QImage result = aiopt::geminiImage(body, &said);
        if (result.isNull())
            return editFailed(said.isEmpty() ? tr("Gemini hat kein Bild geliefert.") : tr("Gemini hat kein Bild geliefert: %1").arg(said));
        m_editReply = nullptr;
        addEditedImage(result.scaled(original, Qt::IgnoreAspectRatio, Qt::SmoothTransformation), model);
    });
}

void AiImageDialog::addEditedImage(const QImage& edited, const QString& note)
{
    m_edits << edited.convertToFormat(QImage::Format_RGB32);
    m_baseCache.clear();
    {
        const QSignalBlocker block(m_variant);
        m_variant->addItem(note.isEmpty() ? tr("Variante %1").arg(m_edits.size())
                                          : tr("Variante %1 (%2)").arg(m_edits.size()).arg(note));
        m_variant->setCurrentIndex(m_variant->count() - 1);
    }
    QString text = tr("Variante %1 ist da. Unter „Variante“ lassen sich alle Ergebnisse vergleichen; Stärke und Regler "
                      "stimmen sie fein ab.").arg(m_edits.size());
    if (applyMode() == Apply::Transfer) {
        const double align = colortransfer::alignment(m_original, edited);
        if (align < 0.7)
            text += QLatin1Char(' ') + tr("Die KI hat das Bild verschoben oder neu gezeichnet; übernommen werden deshalb vor "
                                          "allem Farbe und Licht insgesamt.");
    }
    setBusy(false, text);
    updateMode();
    refreshPreview();
}

QImage AiImageDialog::editedBase(const QImage& image) const
{
    const int index = m_variant->currentIndex();
    if (index < 0 || index >= m_edits.size())
        return image;
    const QString key = QStringLiteral("%1/%2/%3/%4").arg(image.cacheKey()).arg(index).arg(int(applyMode())).arg(m_local->value());
    QImage base;
    for (const auto& [k, img] : m_baseCache)
        if (k == key)
            base = img;
    const QImage& edited = m_edits[index];
    if (!base.isNull()) {
        // aus dem Zwischenspeicher
    } else if (applyMode() == Apply::Transfer) {
        base = colortransfer::transfer(image, edited, m_local->value() / 100.0).image;
    } else {
        // KI-Bild in dieser Grösse, Transparenz des Originals behalten
        const bool deep = image.depth() > 32;
        base = edited.scaled(image.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                   .convertToFormat(deep ? QImage::Format_RGBA64 : QImage::Format_ARGB32);
        if (image.hasAlphaChannel()) {
            const QImage src = image.convertToFormat(deep ? QImage::Format_RGBA64 : QImage::Format_ARGB32);
            for (int y = 0; y < base.height(); ++y) {
                if (deep) {
                    auto* d = reinterpret_cast<QRgba64*>(base.scanLine(y));
                    const auto* a = reinterpret_cast<const QRgba64*>(src.constScanLine(y));
                    for (int x = 0; x < base.width(); ++x)
                        d[x].setAlpha(a[x].alpha());
                } else {
                    auto* d = reinterpret_cast<QRgb*>(base.scanLine(y));
                    const auto* a = reinterpret_cast<const QRgb*>(src.constScanLine(y));
                    for (int x = 0; x < base.width(); ++x)
                        d[x] = qRgba(qRed(d[x]), qGreen(d[x]), qBlue(d[x]), qAlpha(a[x]));
                }
            }
        }
    }
    if (m_baseCache.isEmpty() || m_baseCache.last().first != key) {
        m_baseCache.append({key, base});
        while (m_baseCache.size() > 2)
            m_baseCache.removeFirst();
    }
    // Stärke: zwischen Original (0 %) und KI (100 %), darüber verstärkt
    const double t = m_strength->value() / 100.0;
    if (std::abs(t - 1.0) > 1e-6) {
        const bool deep = base.depth() > 32;
        const QImage src = image.convertToFormat(base.format());
        for (int y = 0; y < base.height(); ++y) {
            if (deep) {
                auto* d = reinterpret_cast<QRgba64*>(base.scanLine(y));
                const auto* o = reinterpret_cast<const QRgba64*>(src.constScanLine(y));
                auto mix = [t](int a, int b) { return quint16(std::clamp(int(std::lround(a + t * (b - a))), 0, 65535)); };
                for (int x = 0; x < base.width(); ++x)
                    d[x] = QRgba64::fromRgba64(mix(o[x].red(), d[x].red()), mix(o[x].green(), d[x].green()),
                                               mix(o[x].blue(), d[x].blue()), d[x].alpha());
            } else {
                auto* d = reinterpret_cast<QRgb*>(base.scanLine(y));
                const auto* o = reinterpret_cast<const QRgb*>(src.constScanLine(y));
                auto mix = [t](int a, int b) { return std::clamp(int(std::lround(a + t * (b - a))), 0, 255); };
                for (int x = 0; x < base.width(); ++x)
                    d[x] = qRgba(mix(qRed(o[x]), qRed(d[x])), mix(qGreen(o[x]), qGreen(d[x])), mix(qBlue(o[x]), qBlue(d[x])),
                                 qAlpha(d[x]));
            }
        }
    }
    return base;
}

QImage AiImageDialog::process(const QImage& image) const
{
    // Bearbeiten: Grundlage ist das Ergebnis der KI, die Regler stimmen fein ab (ungeskaliert);
    // Reglervorschlag: die Regler mit Stärke skaliert
    const bool edit = method() == Method::Edit && !m_edits.isEmpty();
    const adjust::Basic b = edit ? basic() : aiopt::scaled(basic(), m_strength->value());
    auto adjusted = [&](const QImage& in) {
        const QImage source = edit ? editedBase(in) : in;
        return adjust::apply(m_stretch->isChecked() ? adjust::autoAdjust(source) : source, b);
    };
    if (!m_cutout->isChecked())
        return adjusted(image);
    if (cutoutMethod() == CutoutMethod::Local) {
        const QImage base = adjusted(image);
        return m_mask.isNull() ? base : matting::applyMask(base, m_mask, m_hardness->value());
    }
    if (m_cloudCutout.isNull())
        return adjusted(image);
    // Cloud: Ergebnis der KI in dieser Grösse, Anpassungen darauf, Transparenz der KI behalten
    const QImage cut = m_cloudCutout.scaled(image.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    QImage alpha(cut.size(), QImage::Format_Grayscale8);
    for (int y = 0; y < cut.height(); ++y) {
        const auto* src = reinterpret_cast<const QRgb*>(cut.constScanLine(y));
        uchar* dst = alpha.scanLine(y);
        for (int x = 0; x < cut.width(); ++x)
            dst[x] = uchar(qAlpha(src[x]));
    }
    return matting::applyMask(adjusted(cut.convertToFormat(QImage::Format_RGB32)), alpha, m_hardness->value());
}

} // namespace vek
