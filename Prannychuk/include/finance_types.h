#ifndef PFT_FINANCE_TYPES_H
#define PFT_FINANCE_TYPES_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    INCOME = 0,
    EXPENSE = 1
} TransactionType;

typedef struct {
    int32_t id;
    int64_t amount_cents;  /* Amount in kopecks, always > 0 */
    char date[11];         /* YYYY-MM-DD + NUL */
    char category[32];
    char description[128];
    TransactionType type;
} Transaction;

typedef struct {
    Transaction *items;
    size_t count;
    size_t capacity;
} TransactionList;

#endif
