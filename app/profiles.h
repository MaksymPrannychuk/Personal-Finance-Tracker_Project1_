#ifndef APP_PROFILES_H
#define APP_PROFILES_H
#include "vault.h"
#define PROFILE_NAME_MAX 32
#define PROFILE_PATH_CAP 128
/* Names are lowercase ASCII, digits, hyphens and underscores. */
int profile_name_valid(const char *name);
/* Returns 1 on success. Never opens or overwrites an existing profile on create. */
int profile_path(const char *name, char *path, size_t capacity);
int profile_create(const char *name, char *path, size_t capacity, const char *password);
int profile_open(const char *name, char *path, size_t capacity);
void profile_list(void);
int profile_demo(TransactionManager *manager);
#endif
