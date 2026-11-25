#ifndef APP_VAULT_H
#define APP_VAULT_H
#include "../Onishchenko/include/transaction_manager.h"
#include "../Prannychuk/include/crypto.h"
#include <stddef.h>
#define VAULT_MAX_CENTS INT64_C(1000000000000)
enum { VAULT_LOCK_ERROR = -1, VAULT_LOCK_BUSY = -2 };
/* Hold the returned descriptor from open/create until the session exits. */
int vault_lock_session(const char *path);
/* Adapter keeps the original encrypted format and shared team API separate. */
PFTStatus vault_save(const char *path, const TransactionManager *manager, const char *password);
PFTStatus vault_load(const char *path, TransactionManager *manager, const char *password);
PFTStatus vault_backup(const char *path, const char *backup, const char *password);
PFTStatus vault_restore(const char *backup, const char *path, TransactionManager *manager,
                        const char *backup_password, const char *wallet_password);
PFTStatus vault_change_password(const char *path, const char *old, const char *new_password);
PFTStatus vault_verify(const char *path, const char *password, size_t *count);
int vault_valid_transaction(const Transaction *t);
#endif
