// TANGENTA: kleiner Ersatz für ICU (nur was libcdr braucht). Zeichensätze setzt icu_shim.cpp mit
// Windows (MultiByteToWideChar) bzw. iconv um, statt die ganze ICU mitzuliefern.
#pragma once
#include <cstdint>

typedef int32_t UChar32;
typedef uint16_t UChar;
typedef int8_t UBool;

typedef enum UErrorCode {
    U_ZERO_ERROR = 0,
    U_ILLEGAL_ARGUMENT_ERROR = 1,
    U_INDEX_OUTOFBOUNDS_ERROR = 8,
    U_INVALID_CHAR_FOUND = 10,
    U_TRUNCATED_CHAR_FOUND = 11,
    U_ILLEGAL_CHAR_FOUND = 12,
    U_UNSUPPORTED_ERROR = 16
} UErrorCode;

#define U_SUCCESS(x) ((x) <= U_ZERO_ERROR)
#define U_FAILURE(x) ((x) > U_ZERO_ERROR)

#define U_IS_UNICODE_NONCHAR(c) \
    ((c) >= 0xfdd0 && ((c) <= 0xfdef || ((c) & 0xfffe) == 0xfffe) && (c) <= 0x10ffff)
#define U_IS_UNICODE_CHAR(c) \
    ((uint32_t)(c) < 0xd800 || (0xdfff < (c) && (c) <= 0x10ffff && !U_IS_UNICODE_NONCHAR(c)))
