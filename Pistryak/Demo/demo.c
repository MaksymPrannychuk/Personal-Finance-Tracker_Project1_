#include "../Budget/budget.h"
#include "../Search/search.h"
#include "../Statistics/statistics.h"

#include <stdio.h>

int main(void) {
    // Перші шість записів формують початковий звіт. Дві останні витрати
    // додаються по черзі, щоб показати досягнення та перевищення ліміту.
    const Transaction transactions[] = {
        {1, "2025-12-28", 3200.00, "Подорожі", 0, ""},
        {2, "2025-12-30", 1500.00, "Їжа", 0, ""},
        {3, "2026-01-01", 20000.00, "Зарплата", 1, ""},
        {4, "2026-01-09", 1250.50, "Їжа", 0, ""},
        {5, "2026-01-13", 749.50, "Їжа", 0, ""},
        {6, "2026-01-21", 200.00, "Транспорт", 0, ""},
        {7, "2026-01-22", 500.00, "Їжа", 0, ""},
        {8, "2026-01-23", 100.00, "Їжа", 0, ""}
    };
    const int transaction_count = (int)(sizeof transactions / sizeof transactions[0]);
    const int initial_count = transaction_count - 2;
    const int year = 2026;
    const int month = 1;

    BudgetLimit budgets[2];
    if (!set_budget_limit(&budgets[0], "Їжа", 2500.00) ||
        !set_budget_limit(&budgets[1], "Здоров’я", 1000.00)) {
        fputs("Не вдалося створити демонстраційні ліміти.\n", stderr);
        return 1;
    }
    const int budget_count = (int)(sizeof budgets / sizeof budgets[0]);

    // Пошук поєднує категорію, період і тип, як у майбутньому меню.
    const SearchCriteria criteria = {
        .category = "Їжа", .date_from = "2026-01-01", .date_to = "2026-01-31", .type = SEARCH_EXPENSE
    };
    Transaction matches[sizeof transactions / sizeof transactions[0]];
    int count = search_transactions(transactions, initial_count, &criteria, matches, transaction_count);
    if (count < 0) {
        fputs("Не вдалося виконати демонстраційний пошук.\n", stderr);
        return 1;
    }
    printf("Пошук витрат на їжу за січень. Знайдено: %d\n", count);
    for (int i = 0; i < count; i++) {
        printf("%s · %.2f грн\n", matches[i].date, matches[i].amount);
    }

    print_monthly_financial_summary_with_budgets(transactions, initial_count, year, month, budgets, budget_count);

    // Передаємо актуальну кількість записів після кожного додавання.
    for (int i = initial_count; i < transaction_count; i++) {
        printf("\nДодаємо витрату на їжу: %.2f грн\n", transactions[i].amount);
        check_monthly_budget_limit(transactions, i + 1, budgets[0], year, month);
    }
    return 0;
}
