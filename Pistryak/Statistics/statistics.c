#include "statistics.h"
#include "../transaction_amount.h"
#include "../transaction_month.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Рахуємо загальну суму транзакцій одного типу за весь період.
static double calculate_total_by_type(const Transaction *arr, int size, int is_income) {
    double total = 0.0;
    for (int i = 0; i < size; i++) {
        if (arr[i].is_income == is_income) {
            total += arr[i].amount;
        }
    }
    return total;
}

double calculate_total_income(const Transaction* arr, int size) {
    return calculate_total_by_type(arr, size, 1);
}

double calculate_total_expense(const Transaction* arr, int size) {
    return calculate_total_by_type(arr, size, 0);
}

FinancialSummary calculate_financial_summary(const Transaction *arr, int size) {
    FinancialSummary summary;
    summary.income = calculate_total_income(arr, size);
    summary.expense = calculate_total_expense(arr, size);
    summary.balance = summary.income - summary.expense;
    return summary;
}

typedef struct {
    int64_t income_cents;
    int64_t expense_cents;
} MonthlyTotals;

// Зберігаємо цілі копійки до формування звіту й порівняння місяців.
static int calculate_monthly_totals(const Transaction *arr, int size, int year, int month,
                                    MonthlyTotals *totals) {
    char month_prefix[TRANSACTION_MONTH_PREFIX_SIZE];
    if (size < 0 || (size > 0 && arr == NULL) ||
        !format_transaction_month(year, month, month_prefix)) {
        return 0;
    }

    int64_t income_cents = 0;
    int64_t expense_cents = 0;
    for (int i = 0; i < size; i++) {
        if ((arr[i].is_income != 0 && arr[i].is_income != 1) ||
            !transaction_date_matches_month(arr[i].date, month_prefix)) {
            continue;
        }

        // Доходи й витрати мають додатні суми. Тип визначає, до якого підсумку їх додати.
        int64_t *total = arr[i].is_income == 1 ? &income_cents : &expense_cents;
        int64_t amount_cents;
        if (!amount_to_cents(arr[i].amount, &amount_cents) || amount_cents > INT64_MAX - *total) {
            return 0;
        }
        *total += amount_cents;
    }

    totals->income_cents = income_cents;
    totals->expense_cents = expense_cents;
    return 1;
}

int calculate_monthly_summary(const Transaction *arr, int size, int year, int month,
                              FinancialSummary *summary) {
    MonthlyTotals totals;
    if (summary == NULL || !calculate_monthly_totals(arr, size, year, month, &totals)) {
        return 0;
    }

    // Чинний API повертає гривні. Спочатку рахуємо баланс у копійках.
    summary->income = (double)totals.income_cents / 100.0;
    summary->expense = (double)totals.expense_cents / 100.0;
    summary->balance = (double)(totals.income_cents - totals.expense_cents) / 100.0;
    return 1;
}

static int find_category_expense(const CategoryExpense *expenses, int count, const char *category) {
    for (int i = 0; i < count; i++) {
        if (strcmp(expenses[i].category, category) == 0) {
            return i;
        }
    }
    return -1;
}

int calculate_monthly_expense_breakdown(const Transaction *arr, int size, int year, int month,
                                        CategoryExpense *result, int capacity) {
    char month_prefix[TRANSACTION_MONTH_PREFIX_SIZE];
    if (size < 0 || (size > 0 && arr == NULL) || capacity < 0 ||
        (capacity > 0 && result == NULL) || !format_transaction_month(year, month, month_prefix)) {
        return -1;
    }

    int count = 0;
    int64_t total_cents = 0;
    for (int i = 0; i < size; i++) {
        const Transaction *transaction = &arr[i];
        if (transaction->is_income != 0 ||
            !transaction_date_matches_month(transaction->date, month_prefix)) {
            continue;
        }

        int64_t amount_cents;
        if (!amount_to_cents(transaction->amount, &amount_cents) || amount_cents > INT64_MAX - total_cents) {
            return -1;
        }
        if (amount_cents == 0) {
            continue;
        }
        if (memchr(transaction->category, '\0', sizeof transaction->category) == NULL) {
            return -1;
        }

        // Порівнюємо назви категорій так само, як пошук і бюджет.
        // Історична категорія потрапляє до звіту за назвою в транзакції.
        int category_index = find_category_expense(result, count, transaction->category);
        if (category_index < 0) {
            if (count == capacity) {
                return -1;
            }
            category_index = count;
            strcpy(result[category_index].category, transaction->category);
            result[category_index].amount_cents = 0;
            result[category_index].percentage = 0.0;
            count++;
        }

        result[category_index].amount_cents += amount_cents;
        total_cents += amount_cents;
    }

    // Частку обчислюємо від усіх витрат місяця. Без витрат цикл не виконується.
    // Вставками розміщуємо більші суми раніше, зберігаючи порядок однакових сум.
    for (int i = 0; i < count; i++) {
        CategoryExpense expense = result[i];
        expense.percentage = 100.0 * (double)expense.amount_cents / (double)total_cents;
        int position = i;
        while (position > 0 && result[position - 1].amount_cents < expense.amount_cents) {
            result[position] = result[position - 1];
            position--;
        }
        result[position] = expense;
    }
    return count;
}

void print_financial_summary(const Transaction *arr, int size) {
    FinancialSummary summary = calculate_financial_summary(arr, size);
    printf("\n--- Фінансове зведення ---\n");
    printf("Доходи: %.2f\n", summary.income);
    printf("Витрати: %.2f\n", summary.expense);
    printf("Баланс: %.2f\n", summary.balance);
}

static int load_monthly_breakdown(const Transaction *arr, int size, int year, int month,
                                  int64_t expense_cents, CategoryExpense **breakdown) {
    if (expense_cents == 0) {
        return 0;
    }

    // Категорій щонайбільше стільки, скільки є транзакцій.
    *breakdown = calloc((size_t)size, sizeof **breakdown);
    if (*breakdown == NULL) {
        return -1;
    }
    return calculate_monthly_expense_breakdown(arr, size, year, month, *breakdown, size);
}

static const BudgetLimit *find_category_budget(const BudgetLimit *budgets, int count, const char *category) {
    for (int i = 0; i < count; i++) {
        if (strcmp(budgets[i].category, category) == 0) {
            return &budgets[i];
        }
    }
    return NULL;
}

static int valid_report_budgets(const BudgetLimit *budgets, int count) {
    if (count < 0 || (count > 0 && budgets == NULL)) {
        return 0;
    }
    for (int i = 0; i < count; i++) {
        BudgetProgress progress;
        // В одному місяці кожна категорія може мати лише один ліміт.
        if (!calculate_budget_progress(&budgets[i], 0, &progress) ||
            find_category_budget(budgets, i, budgets[i].category) != NULL) {
            return 0;
        }
    }
    return 1;
}

static void print_category_expense(const char *category, int64_t amount_cents, double percentage,
                                   const BudgetLimit *budget) {
    printf("%s: ", category);
    print_amount_cents(amount_cents);
    printf(" (%.2f%% усіх витрат)", percentage);
    BudgetProgress progress;
    if (budget != NULL && calculate_budget_progress(budget, amount_cents, &progress)) {
        print_budget_progress(&progress);
    }
    putchar('\n');
}

static void print_expense_change(int64_t change_cents) {
    if (change_cents == 0) {
        printf("без змін");
    } else {
        printf("на ");
        print_amount_cents(change_cents < 0 ? -change_cents : change_cents);
        printf(change_cents > 0 ? " більше" : " менше");
    }
}

static void print_largest_category_change(const CategoryExpense *current, int count,
                                           const CategoryExpense *previous, int previous_count) {
    const char *category = NULL;
    int64_t largest_change = 0;
    int64_t largest_magnitude = 0;
    for (int i = 0; i < count; i++) {
        int previous_index = find_category_expense(previous, previous_count, current[i].category);
        int64_t previous_amount = previous_index < 0 ? 0 : previous[previous_index].amount_cents;
        int64_t change = current[i].amount_cents - previous_amount;
        int64_t magnitude = change < 0 ? -change : change;
        if (magnitude > largest_magnitude) {
            category = current[i].category;
            largest_magnitude = magnitude;
            largest_change = change;
        }
    }

    // Витрати могли зникнути зовсім. Такі категорії є тільки в попередньому місяці.
    for (int i = 0; i < previous_count; i++) {
        if (previous[i].amount_cents > largest_magnitude &&
            find_category_expense(current, count, previous[i].category) < 0) {
            category = previous[i].category;
            largest_magnitude = previous[i].amount_cents;
            largest_change = -previous[i].amount_cents;
        }
    }
    if (category != NULL) {
        printf("Найбільша зміна за категорією «%s» — ", category);
        print_expense_change(largest_change);
        putchar('\n');
    }
}

void print_monthly_financial_summary_with_budgets(const Transaction *arr, int size, int year, int month,
                                                 const BudgetLimit *budgets, int budget_count) {
    MonthlyTotals current;
    if (!calculate_monthly_totals(arr, size, year, month, &current) ||
        !valid_report_budgets(budgets, budget_count)) {
        puts("Не вдалося сформувати місячне зведення. Перевірте період, транзакції й ліміти.");
        return;
    }

    // Для січня попередній місяць — грудень попереднього року.
    int previous_year = month == 1 ? year - 1 : year;
    int previous_month = month == 1 ? 12 : month - 1;
    MonthlyTotals previous = {0};
    int compare_months = previous_year > 0 &&
                         calculate_monthly_totals(arr, size, previous_year, previous_month, &previous);

    CategoryExpense *breakdown = NULL;
    CategoryExpense *previous_breakdown = NULL;
    int count = load_monthly_breakdown(arr, size, year, month, current.expense_cents, &breakdown);
    int previous_count = 0;
    if (count >= 0 && compare_months) {
        previous_count = load_monthly_breakdown(arr, size, previous_year, previous_month,
                                                previous.expense_cents, &previous_breakdown);
    }
    if (count < 0) {
        puts("Не вдалося сформувати розподіл витрат.");
        free(breakdown);
        return;
    }

    printf("\n--- Фінансове зведення за %04d-%02d ---\n", year, month);
    printf("Доходи: ");
    print_amount_cents(current.income_cents);
    printf("\nВитрати: ");
    print_amount_cents(current.expense_cents);
    printf("\nБаланс: ");
    print_amount_cents(current.income_cents - current.expense_cents);
    putchar('\n');
    if (compare_months && previous_count >= 0) {
        printf("Витрати порівняно з %04d-%02d — ", previous_year, previous_month);
        print_expense_change(current.expense_cents - previous.expense_cents);
        putchar('\n');
        print_largest_category_change(breakdown, count, previous_breakdown, previous_count);
    } else if (previous_year > 0) {
        puts("Порівняння недоступне. Перевірте дані попереднього місяця.");
    }

    if (count == 0) {
        puts("За цей місяць витрат немає.");
    }
    if (count > 0 || budget_count > 0) {
        puts("Витрати за категоріями:");
    }
    for (int i = 0; i < count; i++) {
        const BudgetLimit *budget = find_category_budget(budgets, budget_count, breakdown[i].category);
        print_category_expense(breakdown[i].category, breakdown[i].amount_cents, breakdown[i].percentage, budget);
    }
    for (int i = 0; i < budget_count; i++) {
        if (find_category_expense(breakdown, count, budgets[i].category) < 0) {
            print_category_expense(budgets[i].category, 0, 0.0, &budgets[i]);
        }
    }
    free(previous_breakdown);
    free(breakdown);
}

void print_monthly_financial_summary(const Transaction *arr, int size, int year, int month) {
    print_monthly_financial_summary_with_budgets(arr, size, year, month, NULL, 0);
}
