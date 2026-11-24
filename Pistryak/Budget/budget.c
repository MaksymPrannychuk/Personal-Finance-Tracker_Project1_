#include "budget.h"
#include "../transaction_amount.h"
#include "../transaction_month.h"

#include <stdio.h>
#include <string.h>

int set_budget_limit(BudgetLimit *budget, const char *category, double max_limit) {
    int64_t limit_cents;
    if (budget == NULL || category == NULL ||
        !amount_to_cents(max_limit, &limit_cents) || limit_cents == 0) {
        return 0;
    }

    size_t length = strlen(category);
    if (length == 0 || length >= sizeof budget->category) {
        return 0;
    }

    memmove(budget->category, category, length + 1);
    budget->max_limit = max_limit;
    return 1;
}

static int read_budget_limit(const BudgetLimit *budget, int64_t *limit_cents) {
    return budget != NULL && budget->category[0] != '\0' &&
           memchr(budget->category, '\0', sizeof budget->category) != NULL &&
           amount_to_cents(budget->max_limit, limit_cents) && *limit_cents > 0;
}

static BudgetProgress budget_progress_from_cents(int64_t spent_cents, int64_t limit_cents) {
    BudgetProgress progress;
    progress.spent_cents = spent_cents;
    progress.limit_cents = limit_cents;
    progress.remaining_cents = limit_cents - spent_cents;
    progress.percentage = 100.0 * (double)spent_cents / (double)limit_cents;

    // Ліміт мінус ціла п’ята частина дає 80%, округлені вгору до копійки.
    // Завдяки цьому поріг точний і не треба множити великі суми.
    int64_t warning_cents = limit_cents - limit_cents / 5;
    if (spent_cents > limit_cents) {
        progress.status = BUDGET_OVER_LIMIT;
    } else if (spent_cents == limit_cents) {
        progress.status = BUDGET_AT_LIMIT;
    } else if (spent_cents >= warning_cents) {
        progress.status = BUDGET_APPROACHING_LIMIT;
    } else {
        progress.status = BUDGET_OK;
    }
    return progress;
}

int calculate_budget_progress(const BudgetLimit *budget, int64_t spent_cents, BudgetProgress *progress) {
    int64_t limit_cents;
    if (progress == NULL || spent_cents < 0 || !read_budget_limit(budget, &limit_cents)) {
        return 0;
    }
    *progress = budget_progress_from_cents(spent_cents, limit_cents);
    return 1;
}

// NULL у month_prefix означає, що беруться витрати за весь період.
static int calculate_period_budget_progress(const Transaction *arr, int size, const BudgetLimit *budget,
                                             const char *month_prefix, BudgetProgress *progress) {
    int64_t limit_cents;
    if (progress == NULL || size < 0 || (size > 0 && arr == NULL) ||
        !read_budget_limit(budget, &limit_cents)) {
        return 0;
    }

    int64_t spent_cents = 0;
    for (int i = 0; i < size; i++) {
        if (arr[i].is_income != 0 ||
            strncmp(arr[i].category, budget->category, sizeof arr[i].category) != 0 ||
            (month_prefix != NULL && !transaction_date_matches_month(arr[i].date, month_prefix))) {
            continue;
        }

        int64_t amount_cents;
        if (!amount_to_cents(arr[i].amount, &amount_cents) || amount_cents > INT64_MAX - spent_cents) {
            return 0;
        }
        spent_cents += amount_cents;
    }

    *progress = budget_progress_from_cents(spent_cents, limit_cents);
    return 1;
}

int calculate_monthly_budget_progress(const Transaction *arr, int size, BudgetLimit budget,
                                      int year, int month, BudgetProgress *progress) {
    char month_prefix[TRANSACTION_MONTH_PREFIX_SIZE];
    return format_transaction_month(year, month, month_prefix) &&
           calculate_period_budget_progress(arr, size, &budget, month_prefix, progress);
}

void print_budget_progress(const BudgetProgress *progress) {
    printf(" · ліміт ");
    print_amount_cents(progress->limit_cents);

    // Поруч із порогом показуємо знак нерівності, якщо округлення відсотка
    // до двох знаків могло б суперечити точному статусу в копійках.
    if (progress->status == BUDGET_OK && progress->percentage >= 79.995) {
        printf(" · <80%%");
    } else if (progress->status == BUDGET_APPROACHING_LIMIT && progress->percentage >= 99.995) {
        printf(" · <100%%");
    } else if (progress->status == BUDGET_OVER_LIMIT && progress->percentage <= 100.005) {
        printf(" · >100%%");
    } else {
        printf(" · %.2f%%", progress->percentage);
    }

    if (progress->status == BUDGET_OVER_LIMIT) {
        printf(" · ліміт перевищено на ");
        print_amount_cents(-progress->remaining_cents);
        return;
    }
    printf(" · залишок ");
    print_amount_cents(progress->remaining_cents);
    if (progress->status == BUDGET_AT_LIMIT) {
        printf(" · ліміт вичерпано");
    } else if (progress->status == BUDGET_APPROACHING_LIMIT) {
        printf(" · наближення до ліміту");
    } else {
        printf(" · бюджет у нормі");
    }
}

static void print_budget_report(const char *category, const BudgetProgress *progress) {
    printf("\n--- Бюджетний звіт для категорії: %s ---\n", category);
    printf("Витрачено: ");
    print_amount_cents(progress->spent_cents);
    print_budget_progress(progress);
    putchar('\n');
}

void check_budget_limit(const Transaction *arr, int size, BudgetLimit budget) {
    BudgetProgress progress;
    if (!calculate_period_budget_progress(arr, size, &budget, NULL, &progress)) {
        puts("Не вдалося перевірити бюджет. Перевірте категорію, ліміт і транзакції.");
        return;
    }
    print_budget_report(budget.category, &progress);
}

void check_monthly_budget_limit(const Transaction *arr, int size, BudgetLimit budget,
                                int year, int month) {
    BudgetProgress progress;
    if (!calculate_monthly_budget_progress(arr, size, budget, year, month, &progress)) {
        puts("Не вдалося перевірити бюджет. Перевірте період, категорію, ліміт і транзакції.");
        return;
    }
    print_budget_report(budget.category, &progress);
}
