#ifndef BUDGET_H
#define BUDGET_H

#include "transaction.h"

// Структура для ліміту витрат на конкретну категорію
typedef struct {
    char category[50];
    double max_limit;
} BudgetLimit;

// Функція перевірки бюджету (рахує витрати за категорією і порівнює з лімітом)
void check_budget_limit(const Transaction* arr, int size, BudgetLimit budget);

#endif