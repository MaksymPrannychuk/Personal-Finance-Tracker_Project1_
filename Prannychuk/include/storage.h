#ifndef PFT_STORAGE_H
#define PFT_STORAGE_H

#include "finance_types.h"
#include "crypto.h"

/* Call init before using a list. Call free to release the allocated array. */
void pft_list_init(TransactionList *list);
void pft_list_free(TransactionList *list);
PFTStatus pft_list_add(TransactionList *list, const Transaction *record);

/* Saves only encrypted data. Never writes a plaintext copy to disk.
   Uses an atomic temporary file and rename, and permissions 0600 on Linux.
   All returned failures leave any pre-existing destination file untouched. */
PFTStatus pft_storage_save(const char *path, const TransactionList *list,
                           const char *password);

/* On failure, *list is unchanged. On success, *list is replaced. */
PFTStatus pft_storage_load(const char *path, TransactionList *list,
                           const char *password);

#endif
