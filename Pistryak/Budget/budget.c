#include "budget.h"
#include <stdio.h>
#include <string.h>

void check_budget_limit(const Transaction* arr, int size, BudgetLimit budget) {
    double spent = 0.0;

    // Рахуємо суму витрат для заданої категорії
    for (int i = 0; i < size; i++) {
        if (arr[i].is_income == 0 && strcmp(arr[i].category, budget.category) == 0) {
            spent += arr[i].amount;
        }
    }

    // Виводимо звіт та попередження у терміналі
    printf("\n--- Бюджетний звіт для категорії: %s ---\n", budget.category);
    printf("Витрачено: %.2f / Ліміт: %.2f\n", spent, budget.max_limit);

    if (spent > budget.max_limit) {
        // Використовуємо червоний колір шрифту для попередження в консолі (ANSI-код)
        printf("\033[1;31m[УВАГА] Ліміт перевищено на %.2f!\033[0m\n", spent - budget.max_limit);
    } else {
        printf("\033[1;32m[ОК] Бюджет у нормі. Залишок: %.2f\033[0m\n", budget.max_limit - spent);
    }
}