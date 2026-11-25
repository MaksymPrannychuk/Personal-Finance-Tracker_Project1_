#ifndef TRANSACTION_MONTH_H
#define TRANSACTION_MONTH_H

#include <stdio.h>
#include <string.h>

enum { TRANSACTION_MONTH_PREFIX_SIZE = sizeof "YYYY-MM-" };

// Перевіряє точний формат YYYY-MM-DD і календарну дату, включно з 29 лютого.
static inline int transaction_date_is_valid(const char *date) {
    if (date == NULL || strlen(date) != sizeof "YYYY-MM-DD" - 1 ||
        date[4] != '-' || date[7] != '-') {
        return 0;
    }
    for (size_t i = 0; i < sizeof "YYYY-MM-DD" - 1; i++) {
        if (i != 4 && i != 7 && (date[i] < '0' || date[i] > '9')) {
            return 0;
        }
    }

    int year = (date[0] - '0') * 1000 + (date[1] - '0') * 100 + (date[2] - '0') * 10 + date[3] - '0';
    int month = (date[5] - '0') * 10 + date[6] - '0';
    int day = (date[8] - '0') * 10 + date[9] - '0';
    if (year == 0 || month < 1 || month > 12 || day < 1) {
        return 0;
    }
    const int days_per_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int leap_year = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    return day <= days_per_month[month - 1] + (month == 2 && leap_year);
}

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
