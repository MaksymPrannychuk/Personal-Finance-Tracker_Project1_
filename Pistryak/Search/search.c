#include "search.h"
#include "../transaction_month.h"

#include <string.h>

static int matches(const Transaction *transaction, const SearchCriteria *criteria) {
    if (criteria->category != NULL && strcmp(transaction->category, criteria->category) != 0) {
        return 0;
    }
    if (criteria->date_from != NULL && strcmp(transaction->date, criteria->date_from) < 0) {
        return 0;
    }
    if (criteria->date_to != NULL && strcmp(transaction->date, criteria->date_to) > 0) {
        return 0;
    }
    if (criteria->type == SEARCH_INCOME) {
        return transaction->is_income == 1;
    }
    if (criteria->type == SEARCH_EXPENSE) {
        return transaction->is_income == 0;
    }
    return 1;
}

int search_transactions(const Transaction *arr, int size, const SearchCriteria *criteria,
                        Transaction *result, int capacity) {
    if (size < 0 || capacity < 0 || (size > 0 && arr == NULL) ||
        (capacity > 0 && result == NULL)) {
        return -1;
    }

    const SearchCriteria all = {0};
    if (criteria == NULL) {
        criteria = &all;
    }
    if (criteria->type != SEARCH_ALL && criteria->type != SEARCH_INCOME &&
        criteria->type != SEARCH_EXPENSE) {
        return -1;
    }
    if ((criteria->date_from != NULL && !transaction_date_is_valid(criteria->date_from)) ||
        (criteria->date_to != NULL && !transaction_date_is_valid(criteria->date_to))) {
        return -1;
    }
    if (criteria->date_from != NULL && criteria->date_to != NULL &&
        strcmp(criteria->date_from, criteria->date_to) > 0) {
        return -1;
    }

    int count = 0;
    for (int i = 0; i < size; i++) {
        if (matches(&arr[i], criteria)) {
            if (count < capacity) {
                result[count] = arr[i];
            }
            count++;
        }
    }
    return count;
}

int search_by_category(const Transaction* arr, int size, const char* category, Transaction* result) {
    if (size <= 0) {
        return 0;
    }
    if (category == NULL) {
        return -1;
    }

    const SearchCriteria criteria = {.category = category};
    return search_transactions(arr, size, &criteria, result, size);
}
