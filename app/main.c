#define _POSIX_C_SOURCE 200809L
#include "vault.h"
#include "monobank.h"
#include "profiles.h"
#include "../Pistryak/Search/search.h"
#include "../Pistryak/Statistics/statistics.h"
#include "../Pistryak/transaction_amount.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <openssl/crypto.h>

#define PASSWORD_SIZE 256
static int color;
static const char *blue(void) { return color ? "\033[1;36m" : ""; }
static const char *green(void) { return color ? "\033[1;32m" : ""; }
static const char *red(void) { return color ? "\033[1;31m" : ""; }
static const char *reset(void) { return color ? "\033[0m" : ""; }
static int line(const char *prompt, char *out, size_t n, int secret)
{
    struct termios old, hidden;
    int hide = 0;
    fputs(prompt, stdout); fflush(stdout);
    if (secret && isatty(STDIN_FILENO)) {
        if (tcgetattr(STDIN_FILENO, &old) != 0) { puts("Cannot hide input; cancelled."); return 0; }
        hidden = old; hidden.c_lflag &= (tcflag_t)~ECHO;
        hide = tcsetattr(STDIN_FILENO, TCSAFLUSH, &hidden) == 0;
        if (!hide) { puts("Cannot hide input; cancelled."); return 0; }
    }
    char *p = fgets(out, (int)n, stdin);
    if (hide) { tcsetattr(STDIN_FILENO, TCSANOW, &old); putchar('\n'); }
    if (!p) return 0;
    size_t k = strcspn(out, "\r\n");
    if (!out[k] && k == n - 1) {
        int c = getchar();
        if (c != '\n' && c != EOF) {
            while (c != '\n' && c != EOF) c = getchar();
            puts("Input too long."); return 0;
        }
    }
    out[k] = 0; return 1;
}
static int number(const char *prompt, int *value)
{
    char b[64], *end;
    if (!line(prompt, b, sizeof b, 0)) return 0;
    errno = 0; long v = strtol(b, &end, 10);
    if (errno || end == b || *end || v < 0 || v > INT_MAX) return 0;
    *value = (int)v; return 1;
}
/* Accept positive decimal amounts with at most two fractional digits. */
static int money(const char *prompt, double *value)
{
    char b[64]; if (!line(prompt, b, sizeof b, 0)) return 0;
    size_t i = 0; int64_t whole = 0, frac = 0; int digits = 0;
    while (b[i] >= '0' && b[i] <= '9') {
        if (whole > VAULT_MAX_CENTS / 100) return 0;
        whole = whole * 10 + b[i++] - '0'; ++digits;
    }
    if (!digits) return 0;
    if (b[i] == '.' || b[i] == ',') {
        ++i; int n = 0;
        while (b[i] >= '0' && b[i] <= '9' && n < 2) { frac = frac * 10 + b[i++] - '0'; ++n; }
        if (n == 0) return 0;
        if (n == 1) frac *= 10;
    }
    if (b[i] || whole > VAULT_MAX_CENTS / 100 || whole * 100 + frac <= 0 || whole * 100 + frac > VAULT_MAX_CENTS) return 0;
    *value = (double)(whole * 100 + frac) / 100.0; return 1;
}
static int yes(const char *prompt) { char b[16]; return line(prompt, b, sizeof b, 0) && strcmp(b, "yes") == 0; }
static int password(const char *prompt, char *p) { return line(prompt, p, PASSWORD_SIZE, 1) && p[0]; }
static int new_password(char *p)
{
    char again[PASSWORD_SIZE] = {0};
    int ok = password("New password (at least 8 characters): ", p) && strlen(p) >= 8 &&
        password("Confirm password: ", again) && strcmp(p, again) == 0;
    OPENSSL_cleanse(again, sizeof again);
    if (!ok) puts("Passwords must match and contain at least 8 characters.");
    return ok;
}
static int status(const char *label, PFTStatus s)
{
    printf("%s%s: %s%s\n", s == PFT_OK ? green() : red(), label, pft_status_string(s), reset());
    return s == PFT_OK;
}
static int month(int *y, int *m)
{
    return number("Year: ", y) && number("Month (1-12): ", m) && *y >= 1 && *y <= 9999 && *m >= 1 && *m <= 12;
}
static void dashboard(const TransactionManager *m, int dirty, int demo, const char *label)
{
    int64_t income = 0, expenses = 0;
    for (int i = 0; i < m->count; ++i) {
        int64_t cents = 0;
        if (amount_to_cents(m->transactions[i].amount, &cents)) {
            if (m->transactions[i].is_income) income += cents;
            else expenses += cents;
        }
    }
    int64_t balance = income - expenses;
    printf("\n%sPERSONAL FINANCE TRACKER%s\n", blue(), reset());
    puts("============================================================");
    printf("  BALANCE  %s", balance >= 0 ? green() : red());
    if (balance < 0) putchar('-');
    int64_t magnitude = balance < 0 ? -balance : balance;
    printf("%" PRId64 ".%02" PRId64 " UAH%s\n", magnitude / 100, magnitude % 100, reset());
    printf("  Income   %s%" PRId64 ".%02" PRId64 "%s    Expenses %s%" PRId64 ".%02" PRId64 "%s\n",
           green(), income/100, income%100, reset(), red(), expenses/100, expenses%100, reset());
    printf("  Profile: %s\n", label);
    if (demo) printf("  Records: %-4d  |  DEMO MODE: temporary sample data\n", m->count);
    else printf("  Records: %-4d  |  Vault: unlocked  |  %s\n", m->count, dirty ? "UNSAVED" : "Saved");
    puts("------------------------------------------------------------");
    puts("  TRANSACTIONS                   ANALYTICS");
    puts("  1  Add transaction             5  Search records");
    puts("  2  View transactions           6  Monthly report");
    puts("  3  Edit transaction            7  Monthly budget check");
    puts("  4  Delete transaction          8  Sort and view");
    puts("------------------------------------------------------------");
    if (demo) puts("  SECURITY & STORAGE (disabled in demo)");
    else puts("  SECURITY & STORAGE");
    puts("  9  Save encrypted data        12  Change password");
    puts(" 10  Create backup               13  Verify saved vault");
    puts(" 11  Restore backup               0  Exit");
    puts("------------------------------------------------------------");
    puts("  PUBLIC EXCHANGE RATES | MONOBANK");
    puts(" 14  Exchange rates");
    puts("============================================================");
}
static void show(const Transaction *arr, int n)
{
    if (!n) { puts("No transactions found."); return; }
    printf("\n%-5s %-10s %-8s %14s  %-20s  %s\n", "ID", "DATE", "TYPE", "AMOUNT (UAH)", "CATEGORY", "DESCRIPTION");
    puts("--------------------------------------------------------------------------------");
    for (int i = 0; i < n; ++i) {
        const Transaction *t = &arr[i];
        printf("%-5d %-10s %s%-8s%s %14.2f  %-20s  %s\n", t->id, t->date,
               t->is_income ? green() : red(), t->is_income ? "INCOME" : "EXPENSE", reset(), t->amount, t->category, mono_visible_description(t->description));
    }
}
static int get_transaction(Transaction *t)
{
    int type;
    *t = (Transaction){0}; t->id = 1;
    if (!number("Type (1 income, 2 expense): ", &type) || (type != 1 && type != 2) ||
        !money("Amount (UAH): ", &t->amount) ||
        !line("Date (YYYY-MM-DD): ", t->date, sizeof t->date, 0) ||
        !line("Category (1-31 bytes): ", t->category, 32, 0) ||
        !line("Description: ", t->description, sizeof t->description, 0) ||
        !strncmp(t->description,"[mono:",6)) return 0;
    t->is_income = type == 1;
    return vault_valid_transaction(t);
}
static int compare_date(const void *a, const void *b)
{
    const Transaction *x = a, *y = b; return strcmp(x->date, y->date);
}
static int compare_amount(const void *a, const void *b)
{
    const Transaction *x = a, *y = b; return (x->amount > y->amount) - (x->amount < y->amount);
}
static int save(const char *path, const TransactionManager *m, const char *p, int *dirty)
{
    if (!status("Save", vault_save(path, m, p))) return 0;
    *dirty = 0; return 1;
}
/* Public rate cache exists only during this session. */
typedef struct {
    MonoRate rates[MONO_MAX_RATES];
    size_t rate_count;
    time_t rates_fetched;
} RateCache;
static const char *currency(int code)
{
    switch (code) { case 980: return "UAH"; case 840: return "USD"; case 978: return "EUR"; case 985: return "PLN"; default: return NULL; }
}
static int bank_status(MonoStatus s)
{
    printf("%sMonobank: %s%s\n",s == MONO_OK ? green() : red(),mono_status_string(s),reset());
    return s == MONO_OK;
}
static void bank_rates(RateCache *b)
{
    time_t now=time(NULL);
    if (!b->rates_fetched || now < b->rates_fetched || now-b->rates_fetched>=300) {
        MonoStatus s=mono_get_rates(b->rates,&b->rate_count);
        if (!bank_status(s)) { b->rates_fetched=0; return; }
        b->rates_fetched=time(NULL);
    } else puts("Using rates cached for up to 5 minutes.");
    puts("\nMONOBANK EXCHANGE RATES | UAH per currency unit");
    printf("%-7s %12s %12s %12s  %s\n","PAIR","BUY","SELL","CROSS","UPDATED (UTC)");
    for (size_t i=0;i<b->rate_count;++i) {
        const MonoRate *r=&b->rates[i]; char buy[24]="-",sell[24]="-",cross[24]="-",date[32]="Unknown";
        if (r->buy) snprintf(buy,sizeof buy,"%.4f",r->buy);
        if (r->sell) snprintf(sell,sizeof sell,"%.4f",r->sell);
        if (r->cross) snprintf(cross,sizeof cross,"%.4f",r->cross);
        time_t stamp=(time_t)r->timestamp; struct tm utc;
        if ((int64_t)stamp==r->timestamp && gmtime_r(&stamp,&utc)) strftime(date,sizeof date,"%Y-%m-%d %H:%M",&utc);
        printf("%s/UAH %12s %12s %12s  %s\n",currency(r->currency),buy,sell,cross,date);
    }
}
int main(int argc, char **argv)
{
    const char *path = "finance.enc", *label = "Default wallet";
    char profile[PROFILE_NAME_MAX + 1] = {0}, selected[PROFILE_PATH_CAP] = {0};
    char backup[4096];
    char pwd[PASSWORD_SIZE] = {0}, next[PASSWORD_SIZE] = {0};
    TransactionManager manager = {0}; init_transaction_manager(&manager);
    RateCache bank = {0};
    color = isatty(STDOUT_FILENO) && !getenv("NO_COLOR");
    struct stat st; int dirty = 0, rc = 1, demo = 0, created = 0;
    if (argc > 2) { puts("Usage: finance_tracker [wallet.enc | --demo]"); goto done; }
    if (argc == 2) {
        if (!strcmp(argv[1], "--demo")) demo = 1;
        else path = argv[1];
    } else {
        for (;;) {
            printf("\n%sPERSONAL FINANCE TRACKER%s\n", blue(), reset());
            puts("============================================================");
            puts("  1  Create profile");
            puts("  2  Open profile");
            puts("  3  Demo mode");
            puts("  4  Open or create default wallet");
            puts("  0  Exit");
            puts("============================================================");
            int choice;
            if (!number("Choose: ", &choice)) { if (feof(stdin)) { rc = 0; goto done; } puts("Invalid option."); continue; }
            if (choice == 0) { rc = 0; goto done; }
            if (choice == 3) { demo = 1; break; }
            if (choice == 4) break;
            if (choice != 1 && choice != 2) { puts("Invalid option."); continue; }
            if (choice == 2) profile_list();
            if (!line("Profile name (lowercase letters, digits, - or _): ", profile, sizeof profile, 0) || !profile_name_valid(profile)) { puts("Invalid profile name."); continue; }
            if (choice == 1) {
                if (!new_password(pwd) || !profile_create(profile, selected, sizeof selected, pwd)) {
                    OPENSSL_cleanse(pwd, sizeof pwd); puts("Cannot create profile. Check name, password and whether it already exists."); continue;
                }
                created = 1; puts("Profile created. Encrypted wallet saved.");
            } else if (!profile_open(profile, selected, sizeof selected)) { puts("Profile not found."); continue; }
            path = selected; label = profile; break;
        }
    }
    if (demo) {
        label = "Demo";
        if (!profile_demo(&manager)) { puts("Cannot initialize demo."); goto done; }
        puts("Demo data is fictional and temporary. File operations are disabled.");
    } else {
        int len = snprintf(backup, sizeof backup, "%s.bak", path);
        if (len < 0 || (size_t)len >= sizeof backup || !*path) goto done;
        if (!created) {
            if (lstat(path, &st) == 0) {
                if (!S_ISREG(st.st_mode) || !password("Wallet password: ", pwd) || !status("Unlock", vault_load(path, &manager, pwd))) goto done;
            } else if (errno == ENOENT && !profile[0]) {
                puts("No wallet found. Create an encrypted wallet.");
                if (!new_password(pwd) || !save(path, &manager, pwd, &dirty)) goto done;
            } else { perror("Wallet"); goto done; }
        }
    }
    rc = 0;
    for (;;) {
        dashboard(&manager, dirty, demo, label);
        int cmd;
        if (!number("Choose: ", &cmd)) {
            if (feof(stdin)) { if (dirty) puts("Unsaved changes discarded on end of input."); break; }
            puts("Invalid option."); continue;
        }
        if (cmd == 0) {
            if (!demo && dirty && !yes("Discard unsaved changes? (yes/no): ")) continue;
            break;
        }
        if (demo && cmd >= 9 && cmd <= 13) {
            puts("Disabled in demo mode. Open a personal profile to use this feature."); continue;
        }
        if (cmd == 1 || cmd == 3) {
            Transaction t; int id = 0;
            if ((cmd == 3 && !number("Transaction ID: ", &id)) || !get_transaction(&t)) { puts("Invalid transaction."); continue; }
            if (cmd == 3) {
                int found = 0;
                for (int i=0;i<manager.count;++i) if (manager.transactions[i].id==id) {
                    found=1;
                    if (!mono_preserve_ref(&t,&manager.transactions[i])) {
                        puts("Legacy imported descriptions must be at most 55 bytes."); found=0;
                    }
                    break;
                }
                if (!found) { puts("Cannot edit transaction."); continue; }
            }
            int ok = cmd == 1 ? add_transaction(&manager, t) : edit_transaction(&manager, id, t);
            if (ok) { dirty = 1; puts("Transaction saved in memory."); } else puts("Cannot change transaction (ID or capacity).");
        } else if (cmd == 2) show(manager.transactions, manager.count);
        else if (cmd == 4) {
            int id; if (number("Transaction ID: ", &id) && yes("Delete? (yes/no): ") && delete_transaction(&manager, id)) { dirty = 1; puts("Deleted."); }
            else puts("No transaction deleted.");
        } else if (cmd == 5) {
            char cat[50], from[11], to[11]; int type;
            if (!line("Category (blank = all): ", cat, sizeof cat, 0) || !line("From date (blank = all): ", from, sizeof from, 0) ||
                !line("To date (blank = all): ", to, sizeof to, 0) || !number("Type (0 all, 1 income, 2 expense): ", &type) || type > 2) { puts("Invalid filter."); continue; }
            SearchCriteria c = {cat[0] ? cat : NULL, from[0] ? from : NULL, to[0] ? to : NULL, (SearchType)type};
            Transaction matches[MAX_TRANSACTIONS]; int n = search_transactions(manager.transactions, manager.count, &c, matches, MAX_TRANSACTIONS);
            if (n < 0) puts("Invalid filter."); else show(matches, n);
        } else if (cmd == 6) {
            int y,m; if (month(&y,&m)) print_monthly_financial_summary(manager.transactions, manager.count,y,m); else puts("Invalid month.");
        } else if (cmd == 7) {
            int y,m; char cat[32]; double limit; BudgetLimit b;
            if (month(&y,&m) && line("Category: ", cat, sizeof cat, 0) && money("Monthly limit (UAH): ", &limit) && set_budget_limit(&b,cat,limit))
                check_monthly_budget_limit(manager.transactions,manager.count,b,y,m);
            else puts("Invalid budget.");
        } else if (cmd == 8) {
            int by; if (!number("Sort by (1 date, 2 amount): ", &by) || (by != 1 && by != 2)) { puts("Invalid sort."); continue; }
            Transaction copy[MAX_TRANSACTIONS]; memcpy(copy, manager.transactions, (size_t)manager.count * sizeof *copy);
            qsort(copy,(size_t)manager.count,sizeof *copy,by == 1 ? compare_date : compare_amount); show(copy,manager.count);
        } else if (cmd == 9) save(path,&manager,pwd,&dirty);
        else if (cmd == 10) {
            if ((!dirty || save(path,&manager,pwd,&dirty)) && (lstat(backup,&st) != 0 || yes("Replace backup? (yes/no): ")))
                status("Backup",vault_backup(path,backup,pwd));
        } else if (cmd == 11) {
            if (yes("Restore backup and replace current records? (yes/no): ") &&
                password("Backup password: ", next) && status("Restore",vault_restore(backup,path,&manager,next))) {
                OPENSSL_cleanse(pwd,sizeof pwd); strcpy(pwd,next); dirty = 0;
            }
            OPENSSL_cleanse(next,sizeof next);
        } else if (cmd == 12) {
            if (dirty && !save(path,&manager,pwd,&dirty)) continue;
            if (new_password(next) && status("Password change",vault_change_password(path,pwd,next))) {
                OPENSSL_cleanse(pwd,sizeof pwd); strcpy(pwd,next); puts("Older backups keep their original password.");
            }
            OPENSSL_cleanse(next,sizeof next);
        } else if (cmd == 13) {
            size_t count; if (status("Verify",vault_verify(path,pwd,&count))) printf("Authenticated saved records: %zu\n",count);
        } else if (cmd == 14) bank_rates(&bank);
        else puts("Invalid option.");
    }
    puts("Goodbye!");
done:
    OPENSSL_cleanse(pwd,sizeof pwd); OPENSSL_cleanse(next,sizeof next); OPENSSL_cleanse(&manager,sizeof manager);
    OPENSSL_cleanse(&bank,sizeof bank);
    return rc;
}
