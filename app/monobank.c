#define _POSIX_C_SOURCE 200809L
#include "monobank.h"
#include <curl/curl.h>
#include <json-c/json.h>
#include <openssl/crypto.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BASE_URL "https://api.monobank.ua"
#define MAX_RESPONSE (4u * 1024u * 1024u)
#define REF_PREFIX "[mono:"

typedef struct { char *data; size_t size; } Response;

const char *mono_status_string(MonoStatus s)
{
    switch (s) {
    case MONO_OK: return "OK";
    case MONO_INVALID: return "Invalid argument";
    case MONO_NETWORK: return "Network or TLS error (check internet connection)";
    case MONO_AUTH: return "Access denied by the public API";
    case MONO_RATE_LIMIT: return "API rate limit reached; wait at least 60 seconds";
    case MONO_HTTP: return "Bank API returned an HTTP error";
    case MONO_FORMAT: return "Invalid or unsupported rate response";
    case MONO_MEMORY: return "Not enough memory";
    }
    return "Unknown error";
}
MonoStatus mono_http_status(long code)
{
    if (code == 200) return MONO_OK;
    if (code == 401 || code == 403) return MONO_AUTH;
    if (code == 429) return MONO_RATE_LIMIT;
    return MONO_HTTP;
}
static size_t receive(char *data, size_t size, size_t n, void *userdata)
{
    Response *r = userdata;
    if (size && n > SIZE_MAX / size) return 0;
    size_t bytes = size * n;
    if (bytes > MAX_RESPONSE - r->size) return 0;
    char *p = realloc(r->data, r->size + bytes + 1);
    if (!p) return 0;
    r->data = p;
    memcpy(p + r->size, data, bytes);
    r->size += bytes; p[r->size] = 0;
    return bytes;
}
static void response_free(Response *r)
{
    if (r->data) { OPENSSL_cleanse(r->data, r->size); free(r->data); }
    *r = (Response){0};
}
/* Only fixed Monobank HTTPS endpoints call this function; never follow redirects. */
static MonoStatus fetch(Response *r)
{
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) return MONO_NETWORK;
    CURL *c = curl_easy_init();
    MonoStatus s = MONO_NETWORK;
    if (!c) goto done;
#define OPT(name, value) do { if (curl_easy_setopt(c, name, value) != CURLE_OK) goto done; } while (0)
    OPT(CURLOPT_URL, BASE_URL "/bank/currency");
    OPT(CURLOPT_FOLLOWLOCATION, 0L);
    OPT(CURLOPT_SSL_VERIFYPEER, 1L);
    OPT(CURLOPT_SSL_VERIFYHOST, 2L);
    OPT(CURLOPT_CONNECTTIMEOUT, 8L);
    OPT(CURLOPT_TIMEOUT, 20L);
    OPT(CURLOPT_NOSIGNAL, 1L);
    OPT(CURLOPT_USERAGENT, "PersonalFinanceTracker/1.0");
    OPT(CURLOPT_WRITEFUNCTION, receive);
    OPT(CURLOPT_WRITEDATA, r);
#undef OPT
    if (curl_easy_perform(c) == CURLE_OK) {
        long code = 0;
        if (curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &code) == CURLE_OK)
            s = mono_http_status(code);
        if (s == MONO_OK && (!r->data || strlen(r->data) != r->size)) s = MONO_FORMAT;
    }
done:
    if (c) curl_easy_cleanup(c);
    curl_global_cleanup();
    if (s != MONO_OK) response_free(r);
    return s;
}
static json_object *parse(const char *text, enum json_type type)
{
    if (!text || !*text || strlen(text) > MAX_RESPONSE) return NULL;
    size_t len = strlen(text);
    json_tokener *tok = json_tokener_new_ex(32);
    if (!tok) return NULL;
    json_tokener_set_flags(tok, JSON_TOKENER_STRICT | JSON_TOKENER_VALIDATE_UTF8);
    json_object *obj = json_tokener_parse_ex(tok, text, (int)len);
    size_t end = json_tokener_get_parse_end(tok);
    while (end < len && isspace((unsigned char)text[end])) ++end;
    if (json_tokener_get_error(tok) != json_tokener_success || end != len ||
        !obj || !json_object_is_type(obj, type)) {
        if (obj) json_object_put(obj);
        obj = NULL;
    }
    json_tokener_free(tok);
    return obj;
}
static json_object *field(json_object *o, const char *key)
{
    json_object *v = NULL;
    if (o && json_object_is_type(o, json_type_object)) json_object_object_get_ex(o, key, &v);
    return v;
}
static int integer(json_object *o, const char *key, int64_t *out)
{
    json_object *v = field(o, key);
    if (!v || !json_object_is_type(v, json_type_int)) return 0;
    *out = json_object_get_int64(v); return 1;
}
static int rate(json_object *o, const char *key, double *out)
{
    json_object *v = field(o,key); *out = 0;
    if (!v) return 1;
    if (!json_object_is_type(v,json_type_double) && !json_object_is_type(v,json_type_int)) return 0;
    double x = json_object_get_double(v);
    if (!isfinite(x) || x < 0 || x > 1000000) return 0;
    *out = x; return 1;
}
MonoStatus mono_parse_rates(const char *text, MonoRate rates[MONO_MAX_RATES], size_t *count)
{
    if (!rates || !count) return MONO_INVALID;
    *count = 0;
    json_object *root = parse(text,json_type_array);
    if (!root) return MONO_FORMAT;
    MonoRate tmp[MONO_MAX_RATES] = {0}; size_t n = 0;
    MonoStatus s = MONO_OK;
    for (size_t i = 0; i < json_object_array_length(root); ++i) {
        json_object *o = json_object_array_get_idx(root,i); int64_t a,b;
        if (!integer(o,"currencyCodeA",&a) || !integer(o,"currencyCodeB",&b)) { s = MONO_FORMAT; break; }
        if (b != 980 || (a != 840 && a != 978 && a != 985)) continue;
        int exists = 0; for (size_t j=0;j<n;++j) if (tmp[j].currency == a) exists = 1;
        if (exists) continue;
        MonoRate r = {0}; r.currency = (int)a;
        if (!integer(o,"date",&r.timestamp) || r.timestamp <= 0 ||
            !rate(o,"rateBuy",&r.buy) || !rate(o,"rateSell",&r.sell) || !rate(o,"rateCross",&r.cross) ||
            (!r.buy && !r.sell && !r.cross)) { s = MONO_FORMAT; break; }
        tmp[n++] = r;
    }
    if (!n) s = MONO_FORMAT;
    if (s == MONO_OK) { memcpy(rates,tmp,n*sizeof *rates); *count = n; }
    json_object_put(root); return s;
}
int mono_has_ref(const char *s)
{
    if (!s || strlen(s) < MONO_REF_LEN || strncmp(s,REF_PREFIX,6)) return 0;
    for (size_t i=6;i<70;++i) if (!((s[i]>='0' && s[i]<='9') || (s[i]>='a' && s[i]<='f'))) return 0;
    return s[70] == ']' && s[71] == ' ';
}
const char *mono_visible_description(const char *s) { return mono_has_ref(s) ? s+MONO_REF_LEN : s; }
int mono_preserve_ref(Transaction *updated, const Transaction *original)
{
    if (!updated || !original) return 0;
    if (!mono_has_ref(original->description)) return !mono_has_ref(updated->description);
    size_t n = strlen(updated->description);
    if (n >= sizeof updated->description - MONO_REF_LEN) return 0;
    memmove(updated->description+MONO_REF_LEN,updated->description,n+1);
    memcpy(updated->description,original->description,MONO_REF_LEN); return 1;
}
MonoStatus mono_get_rates(MonoRate rates[MONO_MAX_RATES], size_t *count)
{
    if (!rates || !count) return MONO_INVALID;
    *count = 0;
    Response r={0}; MonoStatus s=fetch(&r);
    if (s==MONO_OK) s=mono_parse_rates(r.data,rates,count);
    response_free(&r); return s;
}
