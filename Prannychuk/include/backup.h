#ifndef PFT_BACKUP_H
#define PFT_BACKUP_H

#include <stddef.h>
#include "storage.h"

/* Password is verified before creating an independently encrypted backup.
   Backup uses a fresh random salt and nonce. If an operation fails before
   replacement, an existing destination remains unchanged. */
PFTStatus pft_backup_create(const char *vault_path, const char *backup_path,
                            const char *password);

/* Authenticate the backup before replacing the vault; do not overwrite a
   working vault when the backup or the password is wrong. */
PFTStatus pft_backup_restore(const char *backup_path, const char *vault_path,
                             const char *password);

/* Verify/decrypt and change the password without writing plaintext to disk.
   Old password must be valid; a new salt+nonce are generated. */
PFTStatus pft_password_change(const char *vault_path,
                              const char *old_password, const char *new_password);

/* Validate encrypted vault and serialized records without modifying it. */
PFTStatus pft_storage_verify(const char *vault_path, const char *password,
                             size_t *record_count);

#endif
