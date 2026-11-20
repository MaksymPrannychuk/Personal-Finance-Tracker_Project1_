#define _POSIX_C_SOURCE 200809L
#include "backup.h"

#include <string.h>
#include <sys/stat.h>

/* Detect direct path aliases and hard links. For simplicity, users are not
   allowed to choose the same file as both source and destination. */
static int paths_alias(const char *a, const char *b)
{
    if (!a || !b || !*a || !*b) return 1;
    if (strcmp(a, b) == 0) return 1;
    struct stat sa, sb;
    return stat(a, &sa) == 0 && stat(b, &sb) == 0 &&
           sa.st_dev == sb.st_dev && sa.st_ino == sb.st_ino;
}

static PFTStatus authenticated_copy(const char *source, const char *destination,
                                    const char *password)
{
    if (!source || !*source || !destination || !*destination ||
        !password || paths_alias(source, destination)) return PFT_ERR_ARGUMENT;
    TransactionList list;
    pft_list_init(&list);
    PFTStatus status = pft_storage_load(source, &list, password);
    if (status == PFT_OK) status = pft_storage_save(destination, &list, password);
    pft_list_free(&list);
    return status;
}

PFTStatus pft_backup_create(const char *vault_path, const char *backup_path,
                            const char *password)
{
    return authenticated_copy(vault_path, backup_path, password);
}

PFTStatus pft_backup_restore(const char *backup_path, const char *vault_path,
                             const char *password)
{
    return authenticated_copy(backup_path, vault_path, password);
}

PFTStatus pft_password_change(const char *vault_path,
                              const char *old_password, const char *new_password)
{
    if (!vault_path || !*vault_path || !old_password || !new_password)
        return PFT_ERR_ARGUMENT;
    /* No plaintext file is ever written. The existing encrypted vault is
       atomically replaced only after a valid new encrypted blob is ready. */
    TransactionList list;
    pft_list_init(&list);
    PFTStatus status = pft_storage_load(vault_path, &list, old_password);
    if (status == PFT_OK) status = pft_storage_save(vault_path, &list, new_password);
    pft_list_free(&list);
    return status;
}

PFTStatus pft_storage_verify(const char *vault_path, const char *password,
                             size_t *record_count)
{
    if (record_count) *record_count = 0;
    if (!vault_path || !*vault_path || !password) return PFT_ERR_ARGUMENT;
    TransactionList list;
    pft_list_init(&list);
    PFTStatus status = pft_storage_load(vault_path, &list, password);
    if (status == PFT_OK && record_count) *record_count = list.count;
    pft_list_free(&list);
    return status;
}
