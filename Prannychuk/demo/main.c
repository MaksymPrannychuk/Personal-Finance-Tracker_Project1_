#define _POSIX_C_SOURCE 200809L
#include "backup.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <openssl/crypto.h>

#define DEFAULT_VAULT "finance.enc"
#define MAX_PATH 4096
#define PASSWORD_CAP 256

static int use_color;
static const char *green(void) { return use_color ? "\033[1;32m" : ""; }
static const char *red(void)   { return use_color ? "\033[1;31m" : ""; }
static const char *blue(void)  { return use_color ? "\033[1;36m" : ""; }
static const char *dim(void)   { return use_color ? "\033[2m" : ""; }
static const char *reset(void) { return use_color ? "\033[0m" : ""; }

/* Every input has a fixed size and cannot spill into the next prompt. */
static int read_line(const char *prompt, char *out, size_t cap, int secret)
{
    if (!out || cap < 2 || cap > INT_MAX) return 0;
    printf("%s", prompt);
    fflush(stdout);
    struct termios old_settings;
    int hidden = 0;
    if (secret && isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO, &old_settings) == 0) {
        struct termios new_settings = old_settings;
        new_settings.c_lflag &= (tcflag_t)~ECHO;
        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_settings) == 0) hidden = 1;
    }
    char *result = fgets(out, (int)cap, stdin);
    if (hidden) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_settings);
        putchar('\n');
    }
    if (!result) return 0;
    size_t n = strcspn(out, "\r\n");
    if (out[n] == '\0' && n == cap - 1 && !feof(stdin)) {
        /* fgets may fill the buffer with exactly cap-1 characters and
           leave only a newline behind. That is a valid full-length input. */
        int next = getchar();
        if (next != '\n' && next != '\r' && next != EOF) {
            while (next != '\n' && next != EOF) next = getchar();
            out[0] = '\0';
            return 0;
        }
        if (next == '\r') {
            int after = getchar();
            if (after != '\n' && after != EOF) ungetc(after, stdin);
        }
    }
    out[n] = '\0';
    return 1;
}
static int ask_password(const char *label, char pwd[PASSWORD_CAP])
{
    memset(pwd, 0, PASSWORD_CAP);
    return read_line(label, pwd, PASSWORD_CAP, 1) && pwd[0] != '\0';
}
static int confirm(const char *prompt)
{
    char line[32];
    return read_line(prompt, line, sizeof line, 0) &&
           (strcmp(line, "yes") == 0 || strcmp(line, "YES") == 0);
}
static void print_status(const char *label, PFTStatus status)
{
    printf("  %s%s%s: %s%s%s\n", blue(), label, reset(),
           status == PFT_OK ? green() : red(), pft_status_string(status), reset());
}
static void show_header(const char *path, const TransactionList *list, int unlocked, int dirty)
{
    printf("\n%s", blue());
    puts("+----------------------------------------------------------+");
    puts("|              PERSONAL FINANCE TRACKER                  |");
    puts("|                 SECURE DATA VAULT                      |");
    puts("+----------------------------------------------------------+");
    printf("%s", reset());
    printf("  Vault: %s\n", path);
    printf("  Status: %s%s%s", unlocked ? green() : dim(), unlocked ? "UNLOCKED" : "LOCKED", reset());
    if (dirty) printf("   %s* unsaved changes%s", red(), reset());
    if (unlocked) printf("   Records: %zu", list->count);
    putchar('\n');
}
static void menu(void)
{
    puts("\n  1  Create a new encrypted wallet");
    puts("  2  Unlock an existing wallet");
    puts("  3  Add income / expense");
    puts("  4  View operations and balance");
    puts("  5  Delete a transaction");
    puts("  6  Save encrypted changes");
    puts("  7  Create encrypted backup (.bak)");
    puts("  8  Restore from encrypted backup");
    puts("  9  Change wallet password");
    puts(" 10  Verify integrity and password");
    puts("  0  Exit");
}
static int parse_positive_int(const char *s, int *value)
{
    if (!s || !*s) return 0;
    errno = 0;
    char *end;
    long v = strtol(s, &end, 10);
    if (errno || *end || v < 1 || v > INT_MAX) return 0;
    *value = (int)v;
    return 1;
}
static int parse_amount(const char *s, int64_t *out)
{
    if (!s || !*s) return 0;
    uint64_t whole = 0, fractional = 0;
    const unsigned char *p = (const unsigned char *)s;
    int before = 0, after = 0;
    while (*p >= '0' && *p <= '9') {
        unsigned digit = (unsigned)(*p++ - '0');
        if (whole > ((uint64_t)INT64_MAX / 100u - digit) / 10u) return 0;
        whole = whole * 10u + digit;
        ++before;
    }
    if (!before) return 0;
    if (*p == '.') {
        ++p;
        while (*p >= '0' && *p <= '9' && after < 2) {
            fractional = fractional * 10u + (unsigned)(*p++ - '0');
            ++after;
        }
        if (!after) return 0;
    }
    if (*p) return 0;
    if (after == 1) fractional *= 10u;
    if (whole > ((uint64_t)INT64_MAX - fractional) / 100u) return 0;
    uint64_t amount = whole * 100u + fractional;
    if (!amount) return 0;
    *out = (int64_t)amount;
    return 1;
}
static void format_amount(int64_t value, char *out, size_t n)
{
    snprintf(out, n, "%" PRId64 ".%02" PRId64, value / 100, value % 100);
}
static void view_list(const TransactionList *list)
{
    if (!list->count) { puts("  No transactions in this wallet."); return; }
    puts("+-----+------------+---------+-----------------+----------------------+----------------+");
    puts("| ID  | Date       | Type    | Category        | Description          | Amount (UAH)   |");
    puts("+-----+------------+---------+-----------------+----------------------+----------------+");
    int64_t income = 0, expenses = 0;
    int overflow = 0;
    for (size_t i = 0; i < list->count; ++i) {
        const Transaction *t = &list->items[i];
        char amount[64];
        format_amount(t->amount_cents, amount, sizeof amount);
        printf("| %-3d | %-10s | %-7s | %-15.15s | %-20.20s | %14s |\n",
               t->id, t->date, t->type == INCOME ? "INCOME" : "EXPENSE",
               t->category, t->description, amount);
        int64_t *sum = t->type == INCOME ? &income : &expenses;
        if (INT64_MAX - *sum < t->amount_cents) overflow = 1;
        else *sum += t->amount_cents;
    }
    puts("+-----+------------+---------+-----------------+----------------------+----------------+");
    if (overflow) { puts("  Balance too large to display safely."); return; }
    char in[64], ex[64], bal[64];
    format_amount(income, in, sizeof in);
    format_amount(expenses, ex, sizeof ex);
    /* This subtraction is safe since both operands are nonnegative. */
    int64_t net = income - expenses;
    int negative = net < 0;
    /* Avoid absolute(INT64_MIN) overflow; these values cannot reach it. */
    format_amount(negative ? -net : net, bal, sizeof bal);
    printf("  Income: %s UAH   Expenses: %s UAH\n", in, ex);
    printf("  Balance: %s%s%s%s UAH\n", negative ? red() : green(),
           negative ? "-" : "", bal, reset());
}
static void add_operation(TransactionList *list, int *dirty)
{
    char input[256];
    Transaction t = {0};
    int max_id = 0;
    for (size_t i = 0; i < list->count; ++i)
        if (list->items[i].id > max_id) max_id = list->items[i].id;
    if (max_id == INT32_MAX) { puts("  Transaction ID limit reached."); return; }
    t.id = max_id + 1;
    if (!read_line("  Type [1 income / 2 expense]: ", input, sizeof input, 0)) return;
    if (strcmp(input, "1") == 0) t.type = INCOME;
    else if (strcmp(input, "2") == 0) t.type = EXPENSE;
    else { puts("  Invalid transaction type."); return; }
    if (!read_line("  Amount (example 245.50): ", input, sizeof input, 0) ||
        !parse_amount(input, &t.amount_cents)) {
        puts("  Invalid amount (positive, up to two decimal places)."); return;
    }
    if (!read_line("  Date (YYYY-MM-DD): ", t.date, sizeof t.date, 0)) {
        puts("  Date is too long."); return;
    }
    if (!read_line("  Category: ", t.category, sizeof t.category, 0)) {
        puts("  Category is too long."); return;
    }
    if (!read_line("  Description: ", t.description, sizeof t.description, 0)) {
        puts("  Description is too long."); return;
    }
    PFTStatus s = pft_list_add(list, &t);
    print_status("Add transaction", s);
    if (s == PFT_OK) *dirty = 1;
}
static void delete_operation(TransactionList *list, int *dirty)
{
    char input[64];
    int id;
    if (!read_line("  ID to delete: ", input, sizeof input, 0) ||
        !parse_positive_int(input, &id)) { puts("  Invalid ID."); return; }
    for (size_t i = 0; i < list->count; ++i) {
        if (list->items[i].id == id) {
            if (i + 1 < list->count)
                memmove(&list->items[i], &list->items[i + 1],
                        (list->count - i - 1) * sizeof *list->items);
            --list->count;
            OPENSSL_cleanse(&list->items[list->count], sizeof *list->items);
            *dirty = 1;
            printf("  %sTransaction deleted.%s\n", green(), reset());
            return;
        }
    }
    puts("  Transaction not found.");
}
static void make_backup_path(const char *path, char *out, size_t n)
{
    snprintf(out, n, "%s.bak", path);
}
int main(int argc, char **argv)
{
    const char *vault_path = argc > 1 ? argv[1] : DEFAULT_VAULT;
    if (!*vault_path || strlen(vault_path) > MAX_PATH - 5u) {
        fputs("Invalid vault path.\n", stderr);
        return 1;
    }
    char backup_path[MAX_PATH + 1u];
    make_backup_path(vault_path, backup_path, sizeof backup_path);
    use_color = isatty(STDOUT_FILENO) && getenv("NO_COLOR") == NULL;
    TransactionList list;
    pft_list_init(&list);
    int unlocked = 0, dirty = 0;
    char line[64];
    char password[PASSWORD_CAP] = {0}, second[PASSWORD_CAP] = {0};
    for (;;) {
        show_header(vault_path, &list, unlocked, dirty);
        menu();
        if (!read_line("\n  Choose: ", line, sizeof line, 0)) break;
        int cmd = -1;
        if (strcmp(line, "0") == 0) cmd = 0;
        else if (parse_positive_int(line, &cmd) && cmd <= 10) { }
        else { puts("  Unknown command."); continue; }
        if (cmd == 0) {
            if (dirty && !confirm("  Unsaved changes! Exit anyway? Type yes: ")) continue;
            break;
        }
        if (cmd == 1) {
            if (dirty && !confirm("  Unsaved changes will be lost. Type yes: ")) continue;
            if (access(vault_path, F_OK) == 0 &&
                !confirm("  Existing vault will be overwritten. Type yes: ")) continue;
            if (!ask_password("  New password: ", password) ||
                !ask_password("  Confirm password: ", second) ||
                strcmp(password, second) != 0) {
                puts("  Passwords must be non-empty and match.");
            } else {
                TransactionList empty; pft_list_init(&empty);
                PFTStatus status = pft_storage_save(vault_path, &empty, password);
                print_status("Create wallet", status);
                if (status == PFT_OK) {
                    pft_list_free(&list);
                    unlocked = 1;
                    dirty = 0;
                }
            }
        } else if (cmd == 2) {
            if (dirty && !confirm("  Unsaved changes will be lost. Type yes: ")) continue;
            if (ask_password("  Password: ", password)) {
                PFTStatus status = pft_storage_load(vault_path, &list, password);
                print_status("Unlock wallet", status);
                if (status == PFT_OK) { unlocked = 1; dirty = 0; }
            }
        } else if (cmd == 3 || cmd == 4 || cmd == 5) {
            if (!unlocked) { puts("  Unlock or create a wallet first."); continue; }
            if (cmd == 3) add_operation(&list, &dirty);
            if (cmd == 4) view_list(&list);
            if (cmd == 5) delete_operation(&list, &dirty);
        } else if (cmd == 6) {
            if (!unlocked) { puts("  Unlock wallet first."); continue; }
            if (ask_password("  Current wallet password: ", password)) {
                /* Verify before saving: never accidentally reset the password
                   merely because the user mistyped it. */
                PFTStatus status = pft_storage_verify(vault_path, password, NULL);
                if (status == PFT_OK) status = pft_storage_save(vault_path, &list, password);
                print_status("Save changes", status);
                if (status == PFT_OK) dirty = 0;
            }
        } else if (cmd == 7) {
            if (dirty) { puts("  Save changes first, then create a backup."); continue; }
            if (ask_password("  Current wallet password: ", password)) {
                PFTStatus status = pft_backup_create(vault_path, backup_path, password);
                print_status("Create backup", status);
                if (status == PFT_OK) printf("  Backup: %s\n", backup_path);
            }
        } else if (cmd == 8) {
            if (dirty && !confirm("  Unsaved changes will be lost. Type yes: ")) continue;
            if (!confirm("  Replace wallet from backup? Type yes: ")) continue;
            if (ask_password("  Backup password: ", password)) {
                PFTStatus status = pft_backup_restore(backup_path, vault_path, password);
                if (status == PFT_OK) status = pft_storage_load(vault_path, &list, password);
                print_status("Restore backup", status);
                if (status == PFT_OK) { unlocked = 1; dirty = 0; }
            }
        } else if (cmd == 9) {
            if (dirty) { puts("  Save current changes before changing password."); continue; }
            if (!ask_password("  Old password: ", password) ||
                !ask_password("  New password: ", second)) continue;
            char confirmation[PASSWORD_CAP] = {0};
            if (!ask_password("  Confirm new password: ", confirmation) ||
                strcmp(second, confirmation) != 0) {
                puts("  Passwords do not match.");
            } else {
                print_status("Change password",
                             pft_password_change(vault_path, password, second));
                puts("  Note: existing .bak backups still use their own old password.");
            }
            OPENSSL_cleanse(confirmation, sizeof confirmation);
        } else if (cmd == 10) {
            if (ask_password("  Wallet password: ", password)) {
                size_t count = 0;
                PFTStatus status = pft_storage_verify(vault_path, password, &count);
                print_status("Integrity verification", status);
                if (status == PFT_OK) printf("  Valid encrypted records: %zu\n", count);
            }
        }
        OPENSSL_cleanse(password, sizeof password);
        OPENSSL_cleanse(second, sizeof second);
    }
    OPENSSL_cleanse(password, sizeof password);
    OPENSSL_cleanse(second, sizeof second);
    pft_list_free(&list);
    puts("  Goodbye!\n");
    return 0;
}
