// TANGENTA: Zeichensatz-Umsetzung für libcdr ohne ICU. Unter Windows mit MultiByteToWideChar,
// sonst mit iconv. Gelesen wird Zeichen für Zeichen, wie ucnv_getNextUChar es verlangt.
#include "unicode/ucnv.h"
#include "unicode/ucsdet.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <iconv.h>
#endif

struct UConverter {
    bool utf16le = false;
#ifdef _WIN32
    unsigned codepage = 1252;
#else
    iconv_t cd = reinterpret_cast<iconv_t>(-1);
#endif
};

namespace {

// "windows-1252" -> 1252
unsigned codepageOf(const char *name)
{
    const char *digits = std::strchr(name, '-');
    return digits ? unsigned(std::strtoul(digits + 1, nullptr, 10)) : 0;
}

#ifndef _WIN32
iconv_t openIconv(unsigned codepage)
{
    std::vector<std::string> names{"WINDOWS-" + std::to_string(codepage), "CP" + std::to_string(codepage)};
    if (codepage == 874)
        names.push_back("TIS-620");
    if (codepage == 936)
        names.push_back("GBK");
    if (codepage == 949)
        names.push_back("UHC");
    if (codepage == 950)
        names.push_back("BIG5");
    names.push_back("ISO-8859-1");
    for (const std::string &n : names) {
        iconv_t cd = iconv_open("UTF-32LE", n.c_str());
        if (cd != reinterpret_cast<iconv_t>(-1))
            return cd;
    }
    return reinterpret_cast<iconv_t>(-1);
}

// count Bytes umsetzen; true und Zeichen, wenn genau ein Zeichen herauskommt
bool decode(iconv_t cd, const char *src, size_t count, UChar32 &out)
{
    iconv(cd, nullptr, nullptr, nullptr, nullptr);
    char in[4];
    std::memcpy(in, src, count);
    char *inp = in;
    size_t inLeft = count;
    uint32_t buf[2] = {0, 0};
    char *outp = reinterpret_cast<char *>(buf);
    size_t outLeft = sizeof(buf);
    if (iconv(cd, &inp, &inLeft, &outp, &outLeft) == static_cast<size_t>(-1) || inLeft != 0)
        return false;
    if (sizeof(buf) - outLeft != 4)
        return false;
    out = UChar32(buf[0]);
    return true;
}
#else
bool decode(unsigned codepage, const char *src, int count, UChar32 &out)
{
    wchar_t w[2];
    const int n = MultiByteToWideChar(codepage, MB_ERR_INVALID_CHARS, src, count, w, 2);
    if (n == 1) {
        out = UChar32(w[0]);
        return true;
    }
    if (n == 2 && w[0] >= 0xd800 && w[0] <= 0xdbff) {
        out = 0x10000 + ((UChar32(w[0]) - 0xd800) << 10) + (UChar32(w[1]) - 0xdc00);
        return true;
    }
    return false;
}
#endif

} // namespace

UConverter *ucnv_open(const char *converterName, UErrorCode *err)
{
    auto *c = new UConverter;
    if (converterName && std::strcmp(converterName, "UTF-16LE") == 0) {
        c->utf16le = true;
        return c;
    }
    const unsigned codepage = converterName ? codepageOf(converterName) : 0;
#ifdef _WIN32
    c->codepage = codepage ? codepage : 1252;
#else
    c->cd = openIconv(codepage ? codepage : 1252);
    if (c->cd == reinterpret_cast<iconv_t>(-1)) {
        delete c;
        if (err)
            *err = U_UNSUPPORTED_ERROR;
        return nullptr;
    }
#endif
    return c;
}

void ucnv_close(UConverter *converter)
{
    if (!converter)
        return;
#ifndef _WIN32
    if (converter->cd != reinterpret_cast<iconv_t>(-1))
        iconv_close(converter->cd);
#endif
    delete converter;
}

UChar32 ucnv_getNextUChar(UConverter *converter, const char **source, const char *sourceLimit, UErrorCode *err)
{
    const char *s = *source;
    const long left = long(sourceLimit - s);
    if (!converter || left <= 0) {
        if (err)
            *err = U_INDEX_OUTOFBOUNDS_ERROR;
        return 0xffff;
    }
    if (converter->utf16le) {
        if (left < 2) {
            *source = sourceLimit;
            return 0xfffd;
        }
        UChar32 c = UChar32(uint8_t(s[0]) | (uint8_t(s[1]) << 8));
        *source = s + 2;
        if (c >= 0xd800 && c <= 0xdbff && left >= 4) {
            const UChar32 low = UChar32(uint8_t(s[2]) | (uint8_t(s[3]) << 8));
            if (low >= 0xdc00 && low <= 0xdfff) {
                *source = s + 4;
                return 0x10000 + ((c - 0xd800) << 10) + (low - 0xdc00);
            }
        }
        return c;
    }
    UChar32 c = 0;
    for (int count = 1; count <= 2 && count <= left; ++count) {
#ifdef _WIN32
        if (decode(converter->codepage, s, count, c)) {
#else
        if (decode(converter->cd, s, size_t(count), c)) {
#endif
            *source = s + count;
            return c;
        }
    }
    *source = s + 1; // ungültiges Byte überspringen
    return 0xfffd;
}

UCharsetDetector *ucsdet_open(UErrorCode *status)
{
    if (status)
        *status = U_UNSUPPORTED_ERROR;
    return nullptr;
}

void ucsdet_close(UCharsetDetector *) {}

UBool ucsdet_enableInputFilter(UCharsetDetector *, UBool) { return 0; }

void ucsdet_setText(UCharsetDetector *, const char *, int32_t, UErrorCode *status)
{
    if (status)
        *status = U_UNSUPPORTED_ERROR;
}

const UCharsetMatch *ucsdet_detect(UCharsetDetector *, UErrorCode *status)
{
    if (status)
        *status = U_UNSUPPORTED_ERROR;
    return nullptr;
}

const char *ucsdet_getName(const UCharsetMatch *, UErrorCode *status)
{
    if (status)
        *status = U_UNSUPPORTED_ERROR;
    return nullptr;
}

int32_t ucsdet_getConfidence(const UCharsetMatch *, UErrorCode *status)
{
    if (status)
        *status = U_UNSUPPORTED_ERROR;
    return 0;
}
