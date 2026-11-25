#include "vault.h"
#include "../Pistryak/transaction_amount.h"
/* The storage module has a private Transaction typedef. Give it a local
   name here so both representations can coexist without changing its API. */
#define Transaction StoredTransaction
#include "../Prannychuk/include/backup.h"
#undef Transaction
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int vault_lock_session(const char *path)
{
    char lock_path[4096];
    if (!path || !*path) return VAULT_LOCK_ERROR;
    int length = snprintf(lock_path, sizeof lock_path, "%s.pft-lock", path);
    if (length < 0 || (size_t)length >= sizeof lock_path) return VAULT_LOCK_ERROR;

    // Lock a stable file because saving the wallet replaces its inode.
    int fd = open(lock_path, O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0) return VAULT_LOCK_ERROR;
    struct stat file;
    if (fstat(fd, &file) == 0 && S_ISREG(file.st_mode)) {
        struct flock lock = {.l_type = F_WRLCK, .l_whence = SEEK_SET};
        if (fcntl(fd, F_SETLK, &lock) == 0) return fd;
        if (errno == EACCES || errno == EAGAIN) {
            close(fd);
            return VAULT_LOCK_BUSY;
        }
    }
    close(fd);
    return VAULT_LOCK_ERROR;
}

static int convert(const Transaction *t, StoredTransaction *r)
{
    int64_t cents;
    if (!t || t->id <= 0 || t->id >= INT_MAX || t->id > INT32_MAX ||
        (t->is_income != 0 && t->is_income != 1) ||
        !amount_to_cents(t->amount, &cents) || cents <= 0 || cents > VAULT_MAX_CENTS ||
        !memchr(t->category, 0, sizeof t->category) || !t->category[0] ||
        strlen(t->category) >= sizeof r->category ||
        !memchr(t->description, 0, sizeof t->description)) return 0;
    *r = (StoredTransaction){0};
    r->id = t->id;
    r->amount_cents = cents;
    r->type = t->is_income ? INCOME : EXPENSE;
    memcpy(r->date, t->date, sizeof r->date);
    strcpy(r->category, t->category);
    strcpy(r->description, t->description);
    return 1;
}
int vault_valid_transaction(const Transaction *t)
{
    StoredTransaction r;
    TransactionList list;
    pft_list_init(&list);
    int ok = convert(t, &r) && pft_list_add(&list, &r) == PFT_OK;
    pft_list_free(&list);
    return ok;
}
PFTStatus vault_save(const char *path, const TransactionManager *m, const char *password)
{
    if (!m || m->count < 0 || m->count > MAX_TRANSACTIONS) return PFT_ERR_ARGUMENT;
    TransactionList list;
    pft_list_init(&list);
    PFTStatus s = PFT_OK;
    for (int i = 0; i < m->count && s == PFT_OK; ++i) {
        StoredTransaction r;
        if (!convert(&m->transactions[i], &r)) { s = PFT_ERR_ARGUMENT; break; }
        for (int j = 0; j < i; ++j) if (m->transactions[j].id == r.id) s = PFT_ERR_FORMAT;
        if (s == PFT_OK) s = pft_list_add(&list, &r);
    }
    if (s == PFT_OK) s = pft_storage_save(path, &list, password);
    pft_list_free(&list);
    return s;
}
static PFTStatus load_manager(const char *path, TransactionManager *m, const char *password)
{
    if (!m) return PFT_ERR_ARGUMENT;
    TransactionList list;
    pft_list_init(&list);
    PFTStatus s = pft_storage_load(path, &list, password);
    TransactionManager temp = {0};
    init_transaction_manager(&temp);
    if (s == PFT_OK && list.count > MAX_TRANSACTIONS) s = PFT_ERR_FORMAT;
    for (size_t i = 0; i < list.count && s == PFT_OK; ++i) {
        const StoredTransaction *r = &list.items[i];
        if (r->id >= INT_MAX || r->amount_cents > VAULT_MAX_CENTS) { s = PFT_ERR_FORMAT; break; }
        for (size_t j = 0; j < i; ++j) if (list.items[j].id == r->id) s = PFT_ERR_FORMAT;
        if (s != PFT_OK) break;
        Transaction t = {0};
        t.id = r->id;
        t.amount = (double)r->amount_cents / 100.0;
        t.is_income = r->type == INCOME;
        strcpy(t.date, r->date);
        strcpy(t.category, r->category);
        strcpy(t.description, r->description);
        temp.transactions[temp.count++] = t;
        if (t.id >= temp.next_id) temp.next_id = t.id + 1;
    }
    if (s == PFT_OK) *m = temp;
    pft_list_free(&list);
    return s;
}
PFTStatus vault_load(const char *path, TransactionManager *m, const char *password) { return load_manager(path, m, password); }
PFTStatus vault_backup(const char *path, const char *backup, const char *password) { return pft_backup_create(path, backup, password); }
PFTStatus vault_restore(const char *backup, const char *path, TransactionManager *m,
                        const char *backup_password, const char *wallet_password)
{
    if (!backup || !path || !m || !strcmp(backup, path)) return PFT_ERR_ARGUMENT;
    struct stat source, destination;
    if (stat(backup, &source) == 0 && stat(path, &destination) == 0 &&
        source.st_dev == destination.st_dev && source.st_ino == destination.st_ino) return PFT_ERR_ARGUMENT;

    // Validate the full backup before replacing either the saved wallet or its open records.
    TransactionManager restored;
    PFTStatus s = load_manager(backup, &restored, backup_password);
    if (s == PFT_OK) s = vault_save(path, &restored, wallet_password);
    if (s == PFT_OK) *m = restored;
    return s;
}
PFTStatus vault_change_password(const char *path, const char *old, const char *new_password) { return pft_password_change(path, old, new_password); }
PFTStatus vault_verify(const char *path, const char *password, size_t *count) { return pft_storage_verify(path, password, count); }
