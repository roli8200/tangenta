#include "Matting.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLibrary>
#include <QMutex>
#include <QMutexLocker>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QThread>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include "onnxruntime_c_api.h"

namespace vek::matting {

namespace {

// Ältere Laufzeiten (Linux-Pakete) reichen: verwendet werden nur Funktionen aus ONNX Runtime 1.16
constexpr uint32_t kApiVersion = 16;

struct Runtime {
    QLibrary library;
    const OrtApiBase* base = nullptr;
    const OrtApi* api = nullptr;
    OrtEnv* env = nullptr;
    QString error;
    QString version;
};

QMutex& mutex()
{
    static QMutex m;
    return m;
}

Runtime& runtime()
{
    static Runtime r;
    static bool tried = false;
    if (tried)
        return r;
    tried = true;
    QStringList candidates;
    if (const QString env = qEnvironmentVariable("VEK_ONNXRUNTIME"); !env.isEmpty())
        candidates << env;
    const QString appDir = QCoreApplication::applicationDirPath();
#if defined(Q_OS_WIN)
    candidates << appDir + QStringLiteral("/onnxruntime.dll") << QStringLiteral("onnxruntime");
#elif defined(Q_OS_MACOS)
    candidates << appDir + QStringLiteral("/../Frameworks/libonnxruntime.dylib") << appDir + QStringLiteral("/libonnxruntime.dylib")
               << QStringLiteral("libonnxruntime");
#else
    candidates << appDir + QStringLiteral("/libonnxruntime.so") << appDir + QStringLiteral("/../lib/tangenta/libonnxruntime.so")
               << QStringLiteral("libonnxruntime.so.1") << QStringLiteral("onnxruntime");
#endif
    using GetApiBase = const OrtApiBase* (*)();
    for (const QString& c : candidates) {
        r.library.setFileName(c);
        if (!r.library.load())
            continue;
        auto getBase = reinterpret_cast<GetApiBase>(r.library.resolve("OrtGetApiBase"));
        if (!getBase) {
            r.library.unload();
            continue;
        }
        r.base = getBase();
        r.api = r.base ? r.base->GetApi(kApiVersion) : nullptr;
        if (!r.api) {
            r.error = QCoreApplication::translate("vek::matting", "ONNX Runtime %1 ist zu alt (mindestens 1.16 nötig).")
                          .arg(QString::fromLatin1(r.base ? r.base->GetVersionString() : "?"));
            r.library.unload();
            continue;
        }
        r.version = QString::fromLatin1(r.base->GetVersionString());
        if (OrtStatus* s = r.api->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "tangenta", &r.env)) {
            r.error = QString::fromUtf8(r.api->GetErrorMessage(s));
            r.api->ReleaseStatus(s);
            r.api = nullptr;
            continue;
        }
        r.error.clear();
        return r;
    }
    if (r.error.isEmpty())
        r.error = QCoreApplication::translate("vek::matting",
                                              "ONNX Runtime wurde nicht gefunden. Sie gehört neben das Programm "
                                              "(onnxruntime.dll bzw. libonnxruntime) oder wird als Paket installiert.");
    return r;
}

// Fehler einer ONNX-Funktion in Text; true = Fehler
bool failed(OrtStatus* status, QString* error)
{
    if (!status)
        return false;
    const OrtApi* api = runtime().api;
    if (error)
        *error = QString::fromUtf8(api->GetErrorMessage(status));
    api->ReleaseStatus(status);
    return true;
}

// Sitzung je Modelldatei (das Laden dauert), bleibt bis zum Programmende
OrtSession* session(const QString& path, QString* error)
{
    static QString loadedPath;
    static OrtSession* loaded = nullptr;
    if (loaded && loadedPath == path)
        return loaded;
    Runtime& r = runtime();
    OrtSessionOptions* options = nullptr;
    if (failed(r.api->CreateSessionOptions(&options), error))
        return nullptr;
    r.api->SetIntraOpNumThreads(options, std::max(1, QThread::idealThreadCount()));
    r.api->SetSessionGraphOptimizationLevel(options, ORT_ENABLE_ALL);
    OrtSession* s = nullptr;
#ifdef _WIN32
    const std::wstring file = QDir::toNativeSeparators(path).toStdWString();
#else
    const std::string file = QFile::encodeName(path).toStdString();
#endif
    const bool bad = failed(r.api->CreateSession(r.env, file.c_str(), options, &s), error);
    r.api->ReleaseSessionOptions(options);
    if (bad)
        return nullptr;
    if (loaded)
        r.api->ReleaseSession(loaded);
    loaded = s;
    loadedPath = path;
    return s;
}

} // namespace

const Model& defaultModel()
{
    static const Model m = [] {
        Model x;
        x.file = QStringLiteral("BiRefNet-general-bb_swin_v1_tiny-epoch_232.onnx");
        x.name = QStringLiteral("BiRefNet (general, Swin-T)");
        x.url = QStringLiteral("https://github.com/danielgatis/rembg/releases/download/v0.0.0/BiRefNet-general-bb_swin_v1_tiny-epoch_232.onnx");
        x.sha256 = QByteArrayLiteral("5600024376f572a557870a5eb0afb1e5961636bef4e1e22132025467d0f03333");
        x.bytes = 224005088;
        x.license = QStringLiteral("MIT");
        return x;
    }();
    return m;
}

QString modelDirectory()
{
    if (const QString env = qEnvironmentVariable("VEK_MATTING_MODEL_DIR"); !env.isEmpty())
        return env;
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/models");
}

QString modelPath(const Model& model)
{
    return modelDirectory() + QLatin1Char('/') + model.file;
}

bool modelPresent(const Model& model)
{
    const QFileInfo info(modelPath(model));
    return info.isFile() && info.size() == model.bytes;
}

bool runtimeAvailable(QString* error)
{
    QMutexLocker lock(&mutex());
    Runtime& r = runtime();
    if (error)
        *error = r.error;
    return r.api != nullptr;
}

QString runtimeVersion()
{
    QMutexLocker lock(&mutex());
    return runtime().version;
}

QImage subjectMask(const QImage& image, const Model& model, QString* error)
{
    QMutexLocker lock(&mutex()); // eine Rechnung zur Zeit; ONNX nutzt dafür alle Kerne
    Runtime& r = runtime();
    if (!r.api) {
        if (error)
            *error = r.error;
        return {};
    }
    if (image.isNull())
        return {};
    OrtSession* s = session(modelPath(model), error);
    if (!s)
        return {};

    // Eingabe: size x size, RGB, normalisiert, NCHW
    const int n = model.size;
    const QImage rgb = image.convertToFormat(QImage::Format_RGB888).scaled(n, n, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    std::vector<float> input(std::size_t(3) * n * n);
    for (int y = 0; y < n; ++y) {
        const uchar* line = rgb.constScanLine(y);
        for (int x = 0; x < n; ++x)
            for (int c = 0; c < 3; ++c)
                input[std::size_t(c) * n * n + std::size_t(y) * n + x] = (line[x * 3 + c] / 255.0f - model.mean[c]) / model.std[c];
    }

    OrtAllocator* allocator = nullptr;
    if (failed(r.api->GetAllocatorWithDefaultOptions(&allocator), error))
        return {};
    char* inName = nullptr;
    char* outName = nullptr;
    if (failed(r.api->SessionGetInputName(s, 0, allocator, &inName), error))
        return {};
    if (failed(r.api->SessionGetOutputName(s, 0, allocator, &outName), error)) {
        r.api->AllocatorFree(allocator, inName);
        return {};
    }
    auto freeNames = qScopeGuard([&] {
        r.api->AllocatorFree(allocator, inName);
        r.api->AllocatorFree(allocator, outName);
    });
    OrtMemoryInfo* memory = nullptr;
    if (failed(r.api->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &memory), error))
        return {};
    const int64_t shape[4] = {1, 3, n, n};
    OrtValue* in = nullptr;
    const bool badTensor = failed(r.api->CreateTensorWithDataAsOrtValue(memory, input.data(), input.size() * sizeof(float), shape, 4,
                                                                        ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &in), error);
    r.api->ReleaseMemoryInfo(memory);
    if (badTensor)
        return {};
    OrtValue* out = nullptr;
    const char* inNames[] = {inName};
    const char* outNames[] = {outName};
    const bool badRun = failed(r.api->Run(s, nullptr, inNames, &in, 1, outNames, 1, &out), error);
    r.api->ReleaseValue(in);
    if (badRun)
        return {};
    auto releaseOut = qScopeGuard([&] { r.api->ReleaseValue(out); });

    // Ausgabe 1x1xHxW
    OrtTensorTypeAndShapeInfo* info = nullptr;
    if (failed(r.api->GetTensorTypeAndShape(out, &info), error))
        return {};
    size_t dims = 0;
    r.api->GetDimensionsCount(info, &dims);
    std::vector<int64_t> d(dims);
    r.api->GetDimensions(info, d.data(), dims);
    r.api->ReleaseTensorTypeAndShapeInfo(info);
    if (dims < 2) {
        if (error)
            *error = QStringLiteral("Unerwartete Ausgabe des Modells");
        return {};
    }
    const int oh = int(d[dims - 2]);
    const int ow = int(d[dims - 1]);
    float* data = nullptr;
    if (failed(r.api->GetTensorMutableData(out, reinterpret_cast<void**>(&data)), error))
        return {};
    std::vector<float> v(data, data + std::size_t(ow) * oh);
    if (model.sigmoid)
        for (float& f : v)
            f = 1.0f / (1.0f + std::exp(-f));
    const auto [lo, hi] = std::minmax_element(v.begin(), v.end());
    const float minV = *lo;
    const float range = std::max(1e-6f, *hi - minV);
    QImage mask(ow, oh, QImage::Format_Grayscale8);
    for (int y = 0; y < oh; ++y) {
        uchar* line = mask.scanLine(y);
        for (int x = 0; x < ow; ++x)
            line[x] = uchar(std::clamp(int(std::lround((v[std::size_t(y) * ow + x] - minV) / range * 255.0f)), 0, 255));
    }
    return mask.scaled(image.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

QImage applyMask(const QImage& image, const QImage& mask, int hardness)
{
    if (image.isNull() || mask.isNull())
        return image;
    const QImage m = (mask.size() == image.size() ? mask : mask.scaled(image.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation))
                         .convertToFormat(QImage::Format_Grayscale8);
    // Kantenhärte: Steigung um die Mitte, 1 (weich wie berechnet) bis 10 (fast hart)
    const double k = 1.0 + std::clamp(hardness, 0, 100) * 0.09;
    uchar table[256];
    for (int i = 0; i < 256; ++i)
        table[i] = uchar(std::clamp(int(std::lround(((i / 255.0 - 0.5) * k + 0.5) * 255.0)), 0, 255));
    if (image.depth() == 64) {
        QImage out = image.convertToFormat(QImage::Format_RGBA64);
        for (int y = 0; y < out.height(); ++y) {
            auto* px = reinterpret_cast<QRgba64*>(out.scanLine(y));
            const uchar* a = m.constScanLine(y);
            for (int x = 0; x < out.width(); ++x)
                px[x].setAlpha(quint16(px[x].alpha() * table[a[x]] / 255));
        }
        return out;
    }
    QImage out = image.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < out.height(); ++y) {
        auto* px = reinterpret_cast<QRgb*>(out.scanLine(y));
        const uchar* a = m.constScanLine(y);
        for (int x = 0; x < out.width(); ++x)
            px[x] = qRgba(qRed(px[x]), qGreen(px[x]), qBlue(px[x]), qAlpha(px[x]) * table[a[x]] / 255);
    }
    return out;
}

} // namespace vek::matting
