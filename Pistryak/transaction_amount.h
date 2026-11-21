#ifndef TRANSACTION_AMOUNT_H
#define TRANSACTION_AMOUNT_H

#include <inttypes.h>
#include <math.h>
#include <stdio.h>

// Наявні поля містять гривні як double. Округлюємо кожну суму один раз,
// перед додаванням, щоб статистика й бюджет працювали з цілими копійками.
static inline int amount_to_cents(double amount, int64_t *cents) {
    long double rounded = roundl((long double)amount * 100.0L);
    // Перевіряємо діапазон до перетворення на int64_t. 2^63 задається точно.
    if (amount < 0.0 || !(rounded >= 0.0L && rounded < 0x1p63L)) {
        return 0;
    }

    *cents = (int64_t)rounded;
    return 1;
}

// Ділимо цілу суму на гривні й копійки, щоб вивести її без округлення.
static inline void print_amount_cents(int64_t cents) {
    int64_t hryvnias = cents / 100;
    int64_t kopecks = cents % 100;
    if (cents < 0) {
        putchar('-');
        hryvnias = -hryvnias;
        kopecks = -kopecks;
    }
    printf("%" PRId64 ".%02" PRId64 " грн", hryvnias, kopecks);
}

#endif
