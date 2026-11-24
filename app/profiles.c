#include "profiles.h"
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define PROFILE_ROOT "profiles"

int profile_name_valid(const char *name)
{
    if (!name || !*name) return 0;
    size_t n = strlen(name);
    if (n > PROFILE_NAME_MAX) return 0;
    for (size_t i = 0; i < n; ++i) {
        char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return 0;
    }
    return 1;
}
static int directory(const char *path)
{
    struct stat st;
    return lstat(path, &st) == 0 && S_ISDIR(st.st_mode);
}
static int regular_file(const char *path)
{
    struct stat st;
    return lstat(path, &st) == 0 && S_ISREG(st.st_mode);
}
static int profile_dir(const char *name, char *out, size_t capacity)
{
    if (!profile_name_valid(name) || !out || !capacity) return 0;
    int n = snprintf(out, capacity, PROFILE_ROOT "/p-%s", name);
    return n >= 0 && (size_t)n < capacity;
}
int profile_path(const char *name, char *path, size_t capacity)
{
    if (!profile_name_valid(name) || !path || !capacity) return 0;
    int n = snprintf(path, capacity, PROFILE_ROOT "/p-%s/wallet.enc", name);
    return n >= 0 && (size_t)n < capacity;
}
int profile_create(const char *name, char *path, size_t capacity, const char *password)
{
    char dir[PROFILE_PATH_CAP];
    if (!profile_path(name, path, capacity) || !profile_dir(name, dir, sizeof dir) || !password || strlen(password) < 8) return 0;
    if (mkdir(PROFILE_ROOT, 0700) != 0 && errno != EEXIST) return 0;
    if (!directory(PROFILE_ROOT)) return 0;
    /* An exclusive directory reservation prevents accidental overwrite. */
    if (mkdir(dir, 0700) != 0) return 0;
    TransactionManager empty = {0};
    init_transaction_manager(&empty);
    if (vault_save(path, &empty, password) != PFT_OK) {
        (void)rmdir(dir);
        return 0;
    }
    return 1;
}
int profile_open(const char *name, char *path, size_t capacity)
{
    char dir[PROFILE_PATH_CAP];
    return profile_path(name, path, capacity) && profile_dir(name, dir, sizeof dir) &&
           directory(PROFILE_ROOT) && directory(dir) && regular_file(path);
}
void profile_list(void)
{
    if (!directory(PROFILE_ROOT)) { puts("No local profiles yet."); return; }
    DIR *d = opendir(PROFILE_ROOT);
    if (!d) { puts("Cannot list profiles."); return; }
    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(d)) != NULL) {
        char path[PROFILE_PATH_CAP];
        if (!strncmp(entry->d_name, "p-", 2) && profile_open(entry->d_name + 2, path, sizeof path)) {
            printf("  %s\n", entry->d_name + 2);
            ++count;
        }
    }
    closedir(d);
    if (!count) puts("No local profiles yet.");
}
int profile_demo(TransactionManager *manager)
{
    if (!manager) return 0;
    Transaction sample[] = {
        {.id=1,.date="2026-11-01",.amount=18000,.category="Salary",.is_income=1,.description="Sample monthly salary"},
        {.id=2,.date="2026-11-03",.amount=4200,.category="Rent",.is_income=0,.description="Sample housing"},
        {.id=3,.date="2026-11-06",.amount=850.50,.category="Food",.is_income=0,.description="Sample groceries"},
        {.id=4,.date="2026-11-09",.amount=320,.category="Transport",.is_income=0,.description="Sample transport"},
        {.id=5,.date="2026-11-15",.amount=3000,.category="Freelance",.is_income=1,.description="Sample side job"},
        {.id=6,.date="2026-11-20",.amount=450,.category="Food",.is_income=0,.description="Sample groceries"}
    };
    *manager = (TransactionManager){0};
    init_transaction_manager(manager);
    for (size_t i = 0; i < sizeof sample / sizeof sample[0]; ++i) {
        if (!vault_valid_transaction(&sample[i]) || !add_transaction(manager, sample[i])) return 0;
    }
    return 1;
}
