#define _POSIX_C_SOURCE 200809L
#include "backup.h"
#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void path_temp(char out[64])
{
    strcpy(out, "/tmp/pft-backup-XXXXXX");
    int fd = mkstemp(out);
    assert(fd >= 0);
    assert(close(fd) == 0);
    assert(unlink(out) == 0);
}
static void add_one(TransactionList *list, int id)
{
    Transaction t = {0};
    t.id = id;
    t.amount_cents = 999 + id;
    t.type = EXPENSE;
    strcpy(t.date, "2026-11-19");
    strcpy(t.category, "Food");
    strcpy(t.description, "Backup unit test");
    assert(pft_list_add(list, &t) == PFT_OK);
}
static void test_backup_restores_prior_version(void)
{
    char primary[64], backup[64]; path_temp(primary); path_temp(backup);
    TransactionList a, b;
    pft_list_init(&a); pft_list_init(&b);
    add_one(&a, 1);
    assert(pft_storage_save(primary, &a, "pw") == PFT_OK);
    assert(pft_backup_create(primary, backup, "pw") == PFT_OK);
    add_one(&a, 2);
    assert(pft_storage_save(primary, &a, "pw") == PFT_OK);
    assert(pft_backup_restore(backup, primary, "pw") == PFT_OK);
    assert(pft_storage_load(primary, &b, "pw") == PFT_OK);
    assert(b.count == 1 && b.items[0].id == 1);
    struct stat sb;
    assert(stat(backup, &sb) == 0 && (sb.st_mode & 077) == 0);
    pft_list_free(&a); pft_list_free(&b);
    unlink(primary); unlink(backup);
    puts("[PASS] Backup: recovery restores earlier state");
}
static void test_wrong_password_and_identity_guard(void)
{
    char primary[64], backup[64]; path_temp(primary); path_temp(backup);
    TransactionList a, b; pft_list_init(&a); pft_list_init(&b);
    add_one(&a, 7);
    assert(pft_storage_save(primary, &a, "correct") == PFT_OK);
    assert(pft_backup_create(primary, backup, "incorrect") == PFT_ERR_AUTH);
    assert(access(backup, F_OK) != 0);
    assert(pft_backup_create(primary, primary, "correct") == PFT_ERR_ARGUMENT);
    assert(pft_backup_create(primary, backup, "correct") == PFT_OK);
    assert(pft_backup_restore(backup, primary, "wrong") == PFT_ERR_AUTH);
    assert(pft_storage_load(primary, &b, "correct") == PFT_OK && b.count == 1);
    pft_list_free(&a); pft_list_free(&b);
    unlink(primary); unlink(backup);
    puts("[PASS] Backup: wrong password and identical paths are rejected");
}
static void test_corrupt_backup_keeps_vault(void)
{
    char primary[64], backup[64]; path_temp(primary); path_temp(backup);
    TransactionList a, b; pft_list_init(&a); pft_list_init(&b);
    add_one(&a, 9);
    assert(pft_storage_save(primary, &a, "pw") == PFT_OK);
    assert(pft_backup_create(primary, backup, "pw") == PFT_OK);
    int fd = open(backup, O_WRONLY | O_TRUNC); assert(fd >= 0);
    assert(write(fd, "bad", 3) == 3);
    assert(close(fd) == 0);
    PFTStatus st = pft_backup_restore(backup, primary, "pw");
    assert(st == PFT_ERR_FORMAT || st == PFT_ERR_AUTH);
    assert(pft_storage_load(primary, &b, "pw") == PFT_OK && b.count == 1);
    pft_list_free(&a); pft_list_free(&b);
    unlink(primary); unlink(backup);
    puts("[PASS] Backup: corrupt backup never destroys working vault");
}
static void test_password_rotation(void)
{
    char primary[64]; path_temp(primary);
    TransactionList a, b; pft_list_init(&a); pft_list_init(&b);
    add_one(&a, 10);
    assert(pft_storage_save(primary, &a, "old") == PFT_OK);
    assert(pft_password_change(primary, "wrong", "new") == PFT_ERR_AUTH);
    assert(pft_storage_load(primary, &b, "old") == PFT_OK);
    assert(pft_password_change(primary, "old", "new") == PFT_OK);
    assert(pft_storage_load(primary, &b, "old") == PFT_ERR_AUTH);
    assert(b.count == 1);
    assert(pft_storage_load(primary, &b, "new") == PFT_OK);
    assert(b.count == 1);
    pft_list_free(&a); pft_list_free(&b); unlink(primary);
    puts("[PASS] Security: changing password revokes old password");
}
static void test_integrity_verification(void)
{
    char primary[64]; path_temp(primary);
    TransactionList a; pft_list_init(&a);
    add_one(&a, 5); add_one(&a, 6);
    assert(pft_storage_save(primary, &a, "pass") == PFT_OK);
    size_t n = 123;
    assert(pft_storage_verify(primary, "pass", &n) == PFT_OK && n == 2);
    assert(pft_storage_verify(primary, "wrong", &n) == PFT_ERR_AUTH && n == 0);
    int fd = open(primary, O_RDWR); assert(fd >= 0);
    assert(lseek(fd, -1, SEEK_END) >= 0);
    unsigned char c; assert(read(fd, &c, 1) == 1);
    assert(lseek(fd, -1, SEEK_END) >= 0);
    c ^= 0x40;
    assert(write(fd, &c, 1) == 1);
    assert(close(fd) == 0);
    assert(pft_storage_verify(primary, "pass", &n) == PFT_ERR_AUTH && n == 0);
    pft_list_free(&a); unlink(primary);
    puts("[PASS] Security: integrity verification detects modified data");
}
static void test_large_vault_roundtrip(void)
{
    char primary[64]; path_temp(primary);
    TransactionList a, b; pft_list_init(&a); pft_list_init(&b);
    for (int i = 1; i <= 3000; ++i) add_one(&a, i);
    assert(pft_storage_save(primary, &a, "stress") == PFT_OK);
    assert(pft_storage_load(primary, &b, "stress") == PFT_OK);
    assert(b.count == 3000);
    assert(b.items[0].id == 1 && b.items[2999].id == 3000);
    pft_list_free(&a); pft_list_free(&b); unlink(primary);
    puts("[PASS] Storage: 3000-record encrypted stress test");
}
int main(void)
{
    test_backup_restores_prior_version();
    test_wrong_password_and_identity_guard();
    test_corrupt_backup_keeps_vault();
    test_password_rotation();
    test_integrity_verification();
    test_large_vault_roundtrip();
    puts("All 6 extended tests passed.");
    return 0;
}
