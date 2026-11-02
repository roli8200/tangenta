#include "DisplayDepth.h"

#include <QScreen>

#ifdef Q_OS_WIN
#include <QtGui/qscreen_platform.h>
#include <dxgi1_6.h>
#endif

namespace vek {

namespace {

#ifdef Q_OS_WIN
// Ausgang des Bildschirms (Monitor-Handle; ohne Handle der erste) über alle Grafikkarten suchen
bool queryDxgi(HMONITOR monitor, DisplayDepth* out)
{
    IDXGIFactory1* factory = nullptr;
    if (FAILED(CreateDXGIFactory1(IID_IDXGIFactory1, reinterpret_cast<void**>(&factory))))
        return false;
    bool found = false;
    IDXGIAdapter1* adapter = nullptr;
    for (UINT a = 0; !found && factory->EnumAdapters1(a, &adapter) != DXGI_ERROR_NOT_FOUND; ++a) {
        IDXGIOutput* output = nullptr;
        for (UINT o = 0; !found && adapter->EnumOutputs(o, &output) != DXGI_ERROR_NOT_FOUND; ++o) {
            IDXGIOutput6* output6 = nullptr;
            if (SUCCEEDED(output->QueryInterface(IID_IDXGIOutput6, reinterpret_cast<void**>(&output6)))) {
                DXGI_OUTPUT_DESC1 desc;
                if (SUCCEEDED(output6->GetDesc1(&desc)) && (!monitor || desc.Monitor == monitor)) {
                    out->bitsPerColor = int(desc.BitsPerColor);
                    out->hdr = desc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
                    out->name += QStringLiteral(" (%1)").arg(QString::fromWCharArray(desc.DeviceName));
                    out->source = QStringLiteral("DXGI");
                    found = true;
                }
                output6->Release();
            }
            output->Release();
        }
        adapter->Release();
    }
    factory->Release();
    return found;
}
#endif

} // namespace

DisplayDepth displayDepth(const QScreen* screen)
{
    DisplayDepth d;
    if (screen)
        d.name = screen->name();
#ifdef Q_OS_WIN
    HMONITOR monitor = nullptr;
    if (screen) {
        if (auto* native = screen->nativeInterface<QNativeInterface::QWindowsScreen>())
            monitor = native->handle();
    }
    if (queryDxgi(monitor, &d))
        return d;
#endif
    // QScreen::depth(): Bits je Pixel (X11: 24 = 8 Bit je Kanal, 30 = 10 Bit; 32 enthält Alpha)
    if (screen) {
        switch (screen->depth()) {
        case 30: d.bitsPerColor = 10; break;
        case 36: d.bitsPerColor = 12; break;
        case 48: d.bitsPerColor = 16; break;
        default: d.bitsPerColor = 8; break;
        }
    }
    d.source = QStringLiteral("Qt");
    return d;
}

} // namespace vek
