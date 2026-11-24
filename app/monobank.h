#ifndef PFT_MONOBANK_H
#define PFT_MONOBANK_H

#include "vault.h"
#include <stdint.h>
#include <stddef.h>
#include <time.h>

#define MONO_MAX_RATES 3
#define MONO_REF_LEN 72 /* legacy encrypted description prefix */

typedef struct {
    int currency;
    double buy, sell, cross; /* zero means not supplied */
    int64_t timestamp;
} MonoRate;

typedef enum {
    MONO_OK, MONO_INVALID, MONO_NETWORK, MONO_AUTH, MONO_RATE_LIMIT,
    MONO_HTTP, MONO_FORMAT, MONO_MEMORY
} MonoStatus;

const char *mono_status_string(MonoStatus status);
MonoStatus mono_http_status(long code);
MonoStatus mono_parse_rates(const char *json, MonoRate rates[MONO_MAX_RATES], size_t *count);
MonoStatus mono_get_rates(MonoRate rates[MONO_MAX_RATES], size_t *count);
/* Preserve readability of records imported by earlier versions. */
int mono_has_ref(const char *description);
const char *mono_visible_description(const char *description);
int mono_preserve_ref(Transaction *updated, const Transaction *original);
#endif
