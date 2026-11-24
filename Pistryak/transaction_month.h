#ifndef TRANSACTION_MONTH_H
#define TRANSACTION_MONTH_H

#include <stdio.h>
#include <string.h>

enum { TRANSACTION_MONTH_PREFIX_SIZE = sizeof "YYYY-MM-" };

// Будує префікс "YYYY-MM-" для дат транзакцій. Нуль означає помилку вводу.
static inline int format_transaction_month(int year, int month, char prefix[TRANSACTION_MONTH_PREFIX_SIZE]) {
    if (year < 1 || year > 9999 || month < 1 || month > 12) {
        return 0;
    }

    return snprintf(prefix, TRANSACTION_MONTH_PREFIX_SIZE, "%04d-%02d-", year, month) == TRANSACTION_MONTH_PREFIX_SIZE - 1;
}

// Дати вже зберігаються у форматі YYYY-MM-DD, тому для відбору
// достатньо порівняти рік, місяць і роздільник перед днем.
static inline int transaction_date_matches_month(const char *date, const char *prefix) {
    return strncmp(date, prefix, TRANSACTION_MONTH_PREFIX_SIZE - 1) == 0;
}

#endif
