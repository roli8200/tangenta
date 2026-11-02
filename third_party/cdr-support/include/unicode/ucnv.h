// TANGENTA: Ersatz für ICU ucnv.h (Codepages von Windows und UTF-16LE, Zeichen für Zeichen)
#pragma once
#include "utypes.h"

typedef struct UConverter UConverter;

UConverter *ucnv_open(const char *converterName, UErrorCode *err);
void ucnv_close(UConverter *converter);
// Nächstes Zeichen aus [*source, sourceLimit); ungültige Bytes werden zu U+FFFD (wie ICU mit Ersatzzeichen)
UChar32 ucnv_getNextUChar(UConverter *converter, const char **source, const char *sourceLimit, UErrorCode *err);
