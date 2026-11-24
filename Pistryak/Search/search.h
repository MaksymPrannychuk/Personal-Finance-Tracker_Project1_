#ifndef SEARCH_H
#define SEARCH_H

#include "transaction.h"

typedef enum {
    SEARCH_ALL,
    SEARCH_INCOME,
    SEARCH_EXPENSE
} SearchType;

typedef struct {
    const char *category;
    const char *date_from;
    const char *date_to;
    SearchType type;
} SearchCriteria;

// NULL для категорії чи дати вимикає відповідний фільтр.
// Дати у форматі YYYY-MM-DD, обидві межі включно.
// Повертає повну кількість збігів і записує не більше capacity транзакцій.
// Якщо збігів більше за capacity, у result зберігаються лише перші capacity.
// За capacity == 0 дозволено передати result == NULL для підрахунку збігів.
// NULL для criteria означає пошук без фільтрів. Неправильні аргументи дають -1.
int search_transactions(const Transaction *arr, int size, const SearchCriteria *criteria,
                        Transaction *result, int capacity);

// Для пошуку лише за категорією result має вміщувати size транзакцій.
int search_by_category(const Transaction* arr, int size, const char* category, Transaction* result);

#endif
