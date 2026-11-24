#define _POSIX_C_SOURCE 200809L
#include "storage.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <openssl/crypto.h>
#include <openssl/rand.h>

#define RECORD_LIMIT 50000u
#define SERIAL_HEADER_LEN 12u
#define RECORD_LEN (4u + 8u + 1u + 11u + 32u + 128u)
static const uint8_t SERIAL_MAGIC[8] = {'P','F','T','D','A','T','0','1'};

static void put32(uint8_t *p, uint32_t v)
{
    for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8u * i));
}
static uint32_t get32(const uint8_t *p)
{
    uint32_t v = 0;
    for (unsigned i = 0; i < 4; ++i) v |= (uint32_t)p[i] << (8u * i);
    return v;
}
static void put64(uint8_t *p, uint64_t v)
{
    for (unsigned i = 0; i < 8; ++i) p[i] = (uint8_t)(v >> (8u * i));
}
static uint64_t get64(const uint8_t *p)
{
    uint64_t v = 0;
    for (unsigned i = 0; i < 8; ++i) v |= (uint64_t)p[i] << (8u * i);
    return v;
}

static int valid_date(const char d[11])
{
    if (d[4] != '-' || d[7] != '-' || d[10] != '\0') return 0;
    for (int i = 0; i < 10; ++i)
        if (i != 4 && i != 7 && (d[i] < '0' || d[i] > '9')) return 0;
    int y = (d[0]-'0')*1000 + (d[1]-'0')*100 + (d[2]-'0')*10 + d[3]-'0';
    int m = (d[5]-'0')*10 + d[6]-'0';
    int day = (d[8]-'0')*10 + d[9]-'0';
    if (y < 1 || m < 1 || m > 12) return 0;
    static const int days[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    int max_day = days[m];
    if (m == 2 && (y%400 == 0 || (y%4 == 0 && y%100 != 0))) ++max_day;
    return day >= 1 && day <= max_day;
}

static int valid_text(const char *text, size_t max, int must_be_nonempty)
{
    const void *found = memchr(text, '\0', max);
    return found && (!must_be_nonempty || text[0] != '\0');
}
static int valid_record(const Transaction *r)
{
    return r && r->id > 0 && r->amount_cents > 0 &&
           (r->type == INCOME || r->type == EXPENSE) &&
           valid_date(r->date) && valid_text(r->category, sizeof r->category, 1) &&
           valid_text(r->description, sizeof r->description, 0);
}

void pft_list_init(TransactionList *list)
{
    if (list) { list->items = NULL; list->count = 0; list->capacity = 0; }
}
void pft_list_free(TransactionList *list)
{
    if (!list) return;
    if (list->items) {
        OPENSSL_cleanse(list->items, list->capacity * sizeof *list->items);
        free(list->items);
    }
    pft_list_init(list);
}

PFTStatus pft_list_add(TransactionList *list, const Transaction *record)
{
    if (!list || !valid_record(record) || list->count > list->capacity ||
        (list->count && !list->items) || list->count >= RECORD_LIMIT)
        return PFT_ERR_ARGUMENT;
    /* Copy BEFORE realloc: record might point into list->items. */
    Transaction clean = {0};
    clean.id = record->id;
    clean.amount_cents = record->amount_cents;
    clean.type = record->type;
    memcpy(clean.date, record->date, sizeof clean.date);
    /* Copy only the actual strings, zero-pad the remaining bytes. */
    size_t c = strlen(record->category), d = strlen(record->description);
    memcpy(clean.category, record->category, c);
    memcpy(clean.description, record->description, d);
    if (list->count == list->capacity) {
        size_t cap = list->capacity ? list->capacity * 2u : 4u;
        if (cap > RECORD_LIMIT) cap = RECORD_LIMIT;
        Transaction *p = realloc(list->items, cap * sizeof *p);
        if (!p) return PFT_ERR_MEMORY;
        list->items = p;
        list->capacity = cap;
    }
    list->items[list->count++] = clean;
    return PFT_OK;
}

static PFTStatus serialize(const TransactionList *list, uint8_t **out, size_t *size)
{
    if (!list || list->count > RECORD_LIMIT || list->count > list->capacity ||
        (list->count && !list->items)) return PFT_ERR_ARGUMENT;
    size_t n = SERIAL_HEADER_LEN + list->count * RECORD_LEN;
    uint8_t *data = calloc(n, 1);
    if (!data) return PFT_ERR_MEMORY;
    memcpy(data, SERIAL_MAGIC, sizeof SERIAL_MAGIC);
    put32(data + 8, (uint32_t)list->count);
    for (size_t i = 0; i < list->count; ++i) {
        const Transaction *r = &list->items[i];
        if (!valid_record(r)) {
            OPENSSL_cleanse(data, n);
            free(data);
            return PFT_ERR_ARGUMENT;
        }
        uint8_t *p = data + SERIAL_HEADER_LEN + i * RECORD_LEN;
        put32(p, (uint32_t)r->id); p += 4;
        put64(p, (uint64_t)r->amount_cents); p += 8;
        *p++ = (uint8_t)r->type;
        memcpy(p, r->date, 11); p += 11;
        memcpy(p, r->category, 32); p += 32;
        memcpy(p, r->description, 128);
    }
    *out = data;
    *size = n;
    return PFT_OK;
}

static PFTStatus deserialize(const uint8_t *data, size_t len, TransactionList *result)
{
    if (len < SERIAL_HEADER_LEN ||
        memcmp(data, SERIAL_MAGIC, sizeof SERIAL_MAGIC) != 0)
        return PFT_ERR_FORMAT;
    uint32_t count = get32(data + 8);
    if (count > RECORD_LIMIT ||
        len != SERIAL_HEADER_LEN + (size_t)count * RECORD_LEN)
        return PFT_ERR_FORMAT;
    for (uint32_t i = 0; i < count; ++i) {
        const uint8_t *p = data + SERIAL_HEADER_LEN + (size_t)i * RECORD_LEN;
        uint32_t raw_id = get32(p); p += 4;
        uint64_t raw_amount = get64(p); p += 8;
        uint8_t raw_type = *p++;
        if (raw_id > INT32_MAX || raw_amount > INT64_MAX || raw_type > EXPENSE)
            return PFT_ERR_FORMAT;
        Transaction r = {0};
        r.id = (int32_t)raw_id;
        r.amount_cents = (int64_t)raw_amount;
        r.type = (TransactionType)raw_type;
        memcpy(r.date, p, 11); p += 11;
        memcpy(r.category, p, 32); p += 32;
        memcpy(r.description, p, 128);
        if (!valid_record(&r)) return PFT_ERR_FORMAT;
        PFTStatus s = pft_list_add(result, &r);
        if (s != PFT_OK) return s;
    }
    return PFT_OK;
}

static int write_full(int fd, const uint8_t *data, size_t n)
{
    while (n) {
        ssize_t k = write(fd, data, n);
        if (k < 0 && errno == EINTR) continue;
        if (k <= 0) return 0;
        data += (size_t)k;
        n -= (size_t)k;
    }
    return 1;
}

static PFTStatus write_atomic(const char *path, const uint8_t *data, size_t len)
{
    size_t base_len = strlen(path);
    if (base_len > 4096 || base_len == 0) return PFT_ERR_ARGUMENT;
    char *temp = malloc(base_len + 6u + 16u + 1u);
    if (!temp) return PFT_ERR_MEMORY;
    memcpy(temp, path, base_len);
    memcpy(temp + base_len, ".tmp.", 5);
    temp[base_len + 5u + 16u] = '\0';
    static const char hex[] = "0123456789abcdef";
    int fd = -1;
    PFTStatus status = PFT_ERR_IO;
    for (int attempt = 0; attempt < 4; ++attempt) {
        uint8_t rnd[8];
        if (RAND_bytes(rnd, sizeof rnd) != 1) { status = PFT_ERR_CRYPTO; goto finish; }
        for (size_t i = 0; i < sizeof rnd; ++i) {
            temp[base_len + 5u + 2u*i] = hex[rnd[i] >> 4];
            temp[base_len + 5u + 2u*i + 1u] = hex[rnd[i] & 0xf];
        }
        fd = open(temp, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
        if (fd >= 0) break;
        if (errno != EEXIST) goto finish;
    }
    if (fd < 0) goto finish;
    if (!write_full(fd, data, len) || fsync(fd) != 0) goto cleanup;
    if (close(fd) != 0) { fd = -1; goto cleanup; }
    fd = -1;
    if (rename(temp, path) != 0) goto cleanup;
    status = PFT_OK;
    goto finish;
cleanup:
    if (fd >= 0) close(fd);
    unlink(temp);
finish:
    free(temp);
    return status;
}

static PFTStatus read_file(const char *path, uint8_t **out, size_t *len)
{
    int fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) return PFT_ERR_IO;
    struct stat sb;
    if (fstat(fd, &sb) != 0 || !S_ISREG(sb.st_mode) || sb.st_size < 0 ||
        (uintmax_t)sb.st_size > PFT_MAX_PLAINTEXT + 64u) {
        close(fd);
        return PFT_ERR_FORMAT;
    }
    size_t n = (size_t)sb.st_size;
    uint8_t *data = malloc(n ? n : 1u);
    if (!data) { close(fd); return PFT_ERR_MEMORY; }
    size_t used = 0;
    while (used < n) {
        ssize_t k = read(fd, data + used, n - used);
        if (k < 0 && errno == EINTR) continue;
        if (k <= 0) { free(data); close(fd); return PFT_ERR_IO; }
        used += (size_t)k;
    }
    uint8_t byte;
    ssize_t extra = read(fd, &byte, 1);
    if (close(fd) != 0 || extra != 0) {
        OPENSSL_cleanse(data, n);
        free(data);
        return PFT_ERR_IO;
    }
    *out = data;
    *len = n;
    return PFT_OK;
}

PFTStatus pft_storage_save(const char *path, const TransactionList *list,
                           const char *password)
{
    if (!path || !*path || !list || !password) return PFT_ERR_ARGUMENT;
    uint8_t *plain = NULL, *cipher = NULL;
    size_t plain_len = 0, cipher_len = 0;
    PFTStatus status = serialize(list, &plain, &plain_len);
    if (status != PFT_OK) return status;
    status = pft_encrypt_bytes(plain, plain_len, password, &cipher, &cipher_len);
    OPENSSL_cleanse(plain, plain_len);
    free(plain);
    if (status != PFT_OK) return status;
    status = write_atomic(path, cipher, cipher_len);
    OPENSSL_cleanse(cipher, cipher_len);
    free(cipher);
    return status;
}

PFTStatus pft_storage_load(const char *path, TransactionList *list,
                           const char *password)
{
    if (!path || !*path || !list || !password) return PFT_ERR_ARGUMENT;
    uint8_t *cipher = NULL, *plain = NULL;
    size_t cipher_len = 0, plain_len = 0;
    PFTStatus status = read_file(path, &cipher, &cipher_len);
    if (status != PFT_OK) return status;
    status = pft_decrypt_bytes(cipher, cipher_len, password, &plain, &plain_len);
    OPENSSL_cleanse(cipher, cipher_len);
    free(cipher);
    if (status != PFT_OK) return status;
    TransactionList loaded;
    pft_list_init(&loaded);
    status = deserialize(plain, plain_len, &loaded);
    OPENSSL_cleanse(plain, plain_len);
    free(plain);
    if (status == PFT_OK) {
        pft_list_free(list);
        *list = loaded;
    } else {
        pft_list_free(&loaded);
    }
    return status;
}
