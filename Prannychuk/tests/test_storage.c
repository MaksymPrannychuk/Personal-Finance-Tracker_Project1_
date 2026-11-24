#define _POSIX_C_SOURCE 200809L
#include "storage.h"
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void temporary_filename(char path[64])
{
    strcpy(path, "/tmp/pft-storage-XXXXXX");
    int fd = mkstemp(path);
    assert(fd >= 0);
    assert(close(fd) == 0);
    assert(unlink(path) == 0);
}

static Transaction example(void)
{
    Transaction t = {0};
    t.id = 42;
    t.amount_cents = 12345;
    t.type = EXPENSE;
    strcpy(t.date, "2026-11-19");
    strcpy(t.category, "Food");
    strcpy(t.description, "Lunch");
    return t;
}

static void test_roundtrip(void)
{
    char path[64]; temporary_filename(path);
    TransactionList a, b;
    pft_list_init(&a); pft_list_init(&b);
    Transaction t = example();
    assert(pft_list_add(&a, &t) == PFT_OK);
    assert(pft_storage_save(path, &a, "good-password") == PFT_OK);
    struct stat sb;
    assert(stat(path, &sb) == 0 && (sb.st_mode & 077) == 0);
    assert(pft_storage_load(path, &b, "good-password") == PFT_OK);
    assert(b.count == 1 && b.items[0].id == 42 &&
           b.items[0].amount_cents == 12345 && b.items[0].type == EXPENSE &&
           strcmp(b.items[0].category, "Food") == 0);
    pft_list_free(&a); pft_list_free(&b);
    unlink(path);
    puts("[PASS] Storage: save/load and private file permissions");
}

static void test_wrong_password_preserves_list(void)
{
    char path[64]; temporary_filename(path);
    TransactionList a;
    pft_list_init(&a);
    Transaction t = example();
    assert(pft_list_add(&a, &t) == PFT_OK);
    assert(pft_storage_save(path, &a, "correct") == PFT_OK);
    assert(pft_storage_load(path, &a, "incorrect") == PFT_ERR_AUTH);
    assert(a.count == 1 && a.items[0].id == 42);
    pft_list_free(&a); unlink(path);
    puts("[PASS] Storage: wrong password does not erase current data");
}

static void test_damage_detected(void)
{
    char path[64]; temporary_filename(path);
    TransactionList a; pft_list_init(&a);
    Transaction t = example();
    assert(pft_list_add(&a, &t) == PFT_OK);
    assert(pft_storage_save(path, &a, "correct") == PFT_OK);
    int fd = open(path, O_RDWR);
    assert(fd >= 0);
    assert(lseek(fd, -1, SEEK_END) >= 0);
    unsigned char last;
    assert(read(fd, &last, 1) == 1);
    assert(lseek(fd, -1, SEEK_END) >= 0);
    last ^= 0x20;
    assert(write(fd, &last, 1) == 1);
    close(fd);
    assert(pft_storage_load(path, &a, "correct") == PFT_ERR_AUTH);
    assert(a.count == 1);
    pft_list_free(&a); unlink(path);
    puts("[PASS] Storage: altered encrypted file rejected");
}

static void test_empty_list(void)
{
    char path[64]; temporary_filename(path);
    TransactionList a, b; pft_list_init(&a); pft_list_init(&b);
    assert(pft_storage_save(path, &a, "secret") == PFT_OK);
    assert(pft_storage_load(path, &b, "secret") == PFT_OK && b.count == 0);
    pft_list_free(&a); pft_list_free(&b); unlink(path);
    puts("[PASS] Storage: empty list");
}

static void test_invalid_record_and_missing_file(void)
{
    char path[64]; temporary_filename(path);
    TransactionList a; pft_list_init(&a);
    Transaction t = example();
    strcpy(t.date, "2026-02-30");
    assert(pft_list_add(&a, &t) == PFT_ERR_ARGUMENT);
    assert(pft_storage_load(path, &a, "test") == PFT_ERR_IO);
    assert(a.count == 0);
    pft_list_free(&a);
    puts("[PASS] Storage: invalid date and missing file");
}

static void test_file_is_not_plaintext(void)
{
    char path[64]; temporary_filename(path);
    TransactionList a; pft_list_init(&a);
    Transaction t = example();
    assert(pft_list_add(&a, &t) == PFT_OK);
    assert(pft_storage_save(path, &a, "password") == PFT_OK);
    FILE *file = fopen(path, "rb");
    assert(file);
    uint8_t bytes[4096];
    size_t used = fread(bytes, 1, sizeof bytes, file);
    assert(fclose(file) == 0);
    assert(used > 16);
    assert(memcmp(bytes, "PFTENC01", 8) == 0);
    for (size_t i = 0; i + 4 <= used; ++i)
        assert(memcmp(bytes + i, "Food", 4) != 0);
    pft_list_free(&a); unlink(path);
    puts("[PASS] Storage: data is encrypted, not stored as plain text");
}

static void test_replace_existing_file(void)
{
    char path[64]; temporary_filename(path);
    TransactionList a, b; pft_list_init(&a); pft_list_init(&b);
    Transaction t = example();
    assert(pft_list_add(&a, &t) == PFT_OK);
    assert(pft_storage_save(path, &a, "password1") == PFT_OK);
    t.id = 43;
    assert(pft_list_add(&a, &t) == PFT_OK);
    assert(pft_storage_save(path, &a, "password2") == PFT_OK);
    assert(pft_storage_load(path, &b, "password2") == PFT_OK && b.count == 2);
    assert(pft_storage_load(path, &b, "password1") == PFT_ERR_AUTH && b.count == 2);
    pft_list_free(&a); pft_list_free(&b); unlink(path);
    puts("[PASS] Storage: overwrite and password change");
}

int main(void)
{
    test_roundtrip();
    test_wrong_password_preserves_list();
    test_damage_detected();
    test_empty_list();
    test_invalid_record_and_missing_file();
    test_file_is_not_plaintext();
    test_replace_existing_file();
    puts("All 7 storage tests passed.");
    return 0;
}
