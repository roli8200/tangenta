// TANGENTA: Ersatz für ICU utf8.h (nur U8_APPEND_UNSAFE)
#pragma once
#include "utypes.h"

#define U8_MAX_LENGTH 4

#define U8_APPEND_UNSAFE(s, i, c) do { \
    uint32_t __uc = (uint32_t)(c); \
    if (__uc <= 0x7f) { \
        (s)[(i)++] = (uint8_t)__uc; \
    } else if (__uc <= 0x7ff) { \
        (s)[(i)++] = (uint8_t)((__uc >> 6) | 0xc0); \
        (s)[(i)++] = (uint8_t)((__uc & 0x3f) | 0x80); \
    } else if (__uc <= 0xffff) { \
        (s)[(i)++] = (uint8_t)((__uc >> 12) | 0xe0); \
        (s)[(i)++] = (uint8_t)(((__uc >> 6) & 0x3f) | 0x80); \
        (s)[(i)++] = (uint8_t)((__uc & 0x3f) | 0x80); \
    } else { \
        (s)[(i)++] = (uint8_t)((__uc >> 18) | 0xf0); \
        (s)[(i)++] = (uint8_t)(((__uc >> 12) & 0x3f) | 0x80); \
        (s)[(i)++] = (uint8_t)(((__uc >> 6) & 0x3f) | 0x80); \
        (s)[(i)++] = (uint8_t)((__uc & 0x3f) | 0x80); \
    } \
} while (0)
