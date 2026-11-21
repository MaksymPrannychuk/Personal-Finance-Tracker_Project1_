#ifndef BUDGET_H
#define BUDGET_H

#include <stdint.h>

#include "transaction.h"

// Структура для ліміту витрат на конкретну категорію
typedef struct {
    char category[50];
    double max_limit;
} BudgetLimit;

typedef enum {
    BUDGET_OK,
    BUDGET_APPROACHING_LIMIT,
    BUDGET_AT_LIMIT,
    BUDGET_OVER_LIMIT
} BudgetStatus;

typedef struct {
    int64_t spent_cents;
    int64_t limit_cents;
    int64_t remaining_cents; // Від’ємне значення означає перевищення.
    double percentage;
    BudgetStatus status;
} BudgetProgress;

// Повертає 1 після успішного встановлення ліміту, 0 при неправильних даних.
// Категорія має займати від 1 до 49 байтів, а ліміт після округлення містити принаймні одну копійку.
int set_budget_limit(BudgetLimit *budget, const char *category, double max_limit);

// Рахує статус за вже підсумованими витратами в копійках.
// Повертає 1 при успіху, 0 при помилці. При помилці progress лишається без змін.
int calculate_budget_progress(const BudgetLimit *budget, int64_t spent_cents, BudgetProgress *progress);

// Підсумовує витрати за категорією та місяцем і повертає статус ліміту.
int calculate_monthly_budget_progress(const Transaction *arr, int size, BudgetLimit budget,
                                      int year, int month, BudgetProgress *progress);

// Дописує до рядка ліміт, відсоток використання, залишок або суму перевищення.
void print_budget_progress(const BudgetProgress *progress);

// Рахує витрати за категорією для переданих транзакцій.
// Попереджає від 80% ліміту, окремо повідомляє про досягнення та перевищення.
void check_budget_limit(const Transaction* arr, int size, BudgetLimit budget);

// Виводить звіт для витрат цієї категорії лише за вибраний рік і місяць.
void check_monthly_budget_limit(const Transaction *arr, int size, BudgetLimit budget,
                                int year, int month);

#endif
