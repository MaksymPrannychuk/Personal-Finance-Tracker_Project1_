#define _POSIX_C_SOURCE 200809L
#include "../app/vault.h"
#include "../Pistryak/Search/search.h"
#include "../Pistryak/Statistics/statistics.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static Transaction record(double amount, int income, const char *category)
{
    Transaction t = {0}; t.id = 1; t.amount = amount; t.is_income = income;
    strcpy(t.date,"2026-11-20"); strcpy(t.category,category); strcpy(t.description,"Test description");
    return t;
}
int main(void)
{
    TransactionManager m = {0}, loaded = {0}; init_transaction_manager(&m); init_transaction_manager(&loaded);
    Transaction income = record(3000.25,1,"Salary"), expense = record(245.50,0,"Food");
    assert(vault_valid_transaction(&income));
    assert(add_transaction(&m,income)); assert(add_transaction(&m,expense));
    assert(m.count == 2 && m.transactions[1].id == 2);
    expense.amount = 250.25; assert(edit_transaction(&m,2,expense));
    assert(!edit_transaction(&m,999,expense));
    assert(!delete_transaction(&m,999));
    puts("[PASS] Core: add, edit, unique IDs and missing ID");
    FinancialSummary summary;
    assert(calculate_monthly_summary(m.transactions,m.count,2026,11,&summary));
    assert(fabs(summary.balance - 2750.0) < 0.001);
    CategoryExpense breakdown[2];
    assert(calculate_monthly_expense_breakdown(m.transactions,m.count,2026,11,breakdown,2)==1);
    assert(breakdown[0].amount_cents == 25025);
    SearchCriteria filter = {"Food","2026-11-01","2026-11-30",SEARCH_EXPENSE};
    Transaction found[2]; assert(search_transactions(m.transactions,m.count,&filter,found,2)==1 && found[0].id == 2);
    puts("[PASS] Analytics: monthly summary, category breakdown and combined search");
    filter.date_from = "2026-02-30";
    assert(search_transactions(m.transactions,m.count,&filter,found,2) == -1);
    filter.date_from = "2024-02-29";
    assert(search_transactions(m.transactions,m.count,&filter,found,2) == 1);
    filter.date_to = "1900-02-29";
    assert(search_transactions(m.transactions,m.count,&filter,found,2) == -1);
    filter.date_from = NULL;
    assert(search_transactions(m.transactions,m.count,&filter,found,2) == -1);
    puts("[PASS] Search: calendar dates and leap days");
    BudgetLimit budget; BudgetProgress progress;
    assert(set_budget_limit(&budget,"Food",300));
    assert(calculate_monthly_budget_progress(m.transactions,m.count,budget,2026,11,&progress));
    assert(progress.status == BUDGET_APPROACHING_LIMIT && progress.spent_cents == 25025);
    assert(calculate_budget_progress(&budget,30000,&progress) && progress.status == BUDGET_AT_LIMIT);
    assert(calculate_budget_progress(&budget,30001,&progress) && progress.status == BUDGET_OVER_LIMIT);
    puts("[PASS] Budget: approaching, at and over limit");
    char dir[] = "/tmp/pft-integration-XXXXXX"; assert(mkdtemp(dir));
    char path[256], backup[256]; snprintf(path,sizeof path,"%s/wallet.enc",dir); snprintf(backup,sizeof backup,"%s/wallet.enc.bak",dir);
    int lock = vault_lock_session(path);
    assert(lock >= 0);
    assert(vault_save(path,&m,"test-password") == PFT_OK);
    pid_t second = fork();
    assert(second >= 0);
    if (second == 0) _exit(vault_lock_session(path) == VAULT_LOCK_BUSY ? 0 : 1);
    int child_status;
    assert(waitpid(second, &child_status, 0) == second && WIFEXITED(child_status) && WEXITSTATUS(child_status) == 0);
    assert(close(lock) == 0);
    lock = vault_lock_session(path);
    assert(lock >= 0 && close(lock) == 0);
    puts("[PASS] Storage: second session blocked even after an atomic save");
    assert(vault_load(path,&loaded,"test-password") == PFT_OK);
    assert(loaded.count==2 && loaded.next_id==3 && loaded.transactions[1].amount==250.25);
    assert(!strcmp(loaded.transactions[0].description,"Test description"));
    assert(vault_load(path,&loaded,"wrong") != PFT_OK && loaded.count==2);
    size_t count; assert(vault_verify(path,"test-password",&count)==PFT_OK && count==2);
    puts("[PASS] Adapter: encrypted roundtrip, exact cents, description, wrong password and IDs");
    assert(vault_backup(path,backup,"test-password")==PFT_OK);
    assert(vault_change_password(path,"test-password","new-password")==PFT_OK);
    assert(delete_transaction(&loaded,2)); assert(vault_save(path,&loaded,"new-password")==PFT_OK);
    assert(vault_restore(backup,path,&loaded,"wrong","new-password")!=PFT_OK && loaded.count==1);
    assert(vault_verify(path,"new-password",&count)==PFT_OK && count==1);
    assert(vault_restore(path,path,&loaded,"new-password","new-password")!=PFT_OK && loaded.count==1);
    assert(vault_restore(backup,path,&loaded,"test-password","new-password")==PFT_OK && loaded.count==2);
    assert(vault_load(path,&loaded,"test-password")!=PFT_OK);
    assert(vault_load(path,&loaded,"new-password")==PFT_OK && loaded.count==2);
    puts("[PASS] Integration: restore an old backup and keep the current password");
    Transaction bad = income; strcpy(bad.date,"2026-02-30"); assert(!vault_valid_transaction(&bad));
    bad = income; bad.amount = NAN; assert(!vault_valid_transaction(&bad));
    bad = income; memset(bad.category,'x',32); bad.category[32]=0; assert(!vault_valid_transaction(&bad));
    loaded.transactions[1].id = loaded.transactions[0].id;
    assert(vault_save(path,&loaded,"new-password")!=PFT_OK);
    assert(vault_load(path,&loaded,"new-password")==PFT_OK && loaded.transactions[1].id==2);
    char lock_path[sizeof path + sizeof ".pft-lock"];
    snprintf(lock_path,sizeof lock_path,"%s.pft-lock",path);
    assert(unlink(path)==0 && unlink(backup)==0 && unlink(lock_path)==0 && rmdir(dir)==0);
    puts("[PASS] Validation: invalid date/amount/category, duplicate ID, unchanged vault");
    puts("All 8 integration tests passed.");
    return 0;
}
