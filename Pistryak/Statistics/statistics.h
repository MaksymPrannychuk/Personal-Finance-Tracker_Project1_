#ifndef STATISTICS_H
#define STATISTICS_H

#include <stdint.h>

#include "../Budget/budget.h"
#include "transaction.h"

typedef struct {
    double income;
    double expense;
    double balance;
} FinancialSummary;

typedef struct {
    char category[sizeof ((Transaction *)0)->category];
    int64_t amount_cents;
    double percentage;
} CategoryExpense;

// Функції для підрахунку загального доходу та витрат
double calculate_total_income(const Transaction* arr, int size);
double calculate_total_expense(const Transaction* arr, int size);

// Зведення для переданих транзакцій. Баланс дорівнює доходам мінус витрати.
FinancialSummary calculate_financial_summary(const Transaction *arr, int size);
void print_financial_summary(const Transaction *arr, int size);

// Записує зведення за вибраний календарний місяць і повертає 1.
// За неправильних аргументів повертає 0 і не змінює результат.
// Порожній місяць дає нульові суми.
int calculate_monthly_summary(const Transaction *arr, int size, int year, int month,
                              FinancialSummary *summary);

// Записує розподіл додатних витрат за місяць у порядку спадання суми.
// Категорії з однаковою сумою лишаються в порядку першої появи.
// Повертає кількість категорій або -1 за помилки чи браку місця в result.
// Результат можна читати лише за успіху. NULL дозволено при capacity == 0.
int calculate_monthly_expense_breakdown(const Transaction *arr, int size, int year, int month,
                                        CategoryExpense *result, int capacity);

// Виводить місячні підсумки, порівняння з попереднім місяцем і розподіл витрат.
void print_monthly_financial_summary(const Transaction *arr, int size, int year, int month);

// Додає до звіту ліміти за категоріями. Передаємо ліміти саме вибраного місяця,
// по одному для кожної категорії. NULL дозволено при budget_count == 0.
void print_monthly_financial_summary_with_budgets(const Transaction *arr, int size, int year, int month,
                                                 const BudgetLimit *budgets, int budget_count);

#endif
