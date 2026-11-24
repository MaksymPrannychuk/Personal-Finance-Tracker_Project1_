#include "../include/transaction_manager.h"
#include <stddef.h>

void init_transaction_manager(TransactionManager *manager) {
    if (manager == NULL) {
        return;
    }

    manager->count = 0;
    manager->next_id = 1;
}

int add_transaction(TransactionManager *manager, Transaction transaction) {
    if (manager == NULL || manager->count >= MAX_TRANSACTIONS) {
        return 0;
    }

    transaction.id = manager->next_id;
    manager->transactions[manager->count] = transaction;

    manager->count++;
    manager->next_id++;

    return 1;
}

int edit_transaction(TransactionManager *manager, int id, Transaction updated) {
    if (manager == NULL || id <= 0) {
        return 0;
    }

    for (int i = 0; i < manager->count; i++) {
        if (manager->transactions[i].id == id) {
            updated.id = id;
            manager->transactions[i] = updated;
            return 1;
        }
    }

    return 0;
}

int delete_transaction(TransactionManager *manager, int id) {
    if (manager == NULL || id <= 0) {
        return 0;
    }

    for (int i = 0; i < manager->count; i++) {
        if (manager->transactions[i].id == id) {
            for (int j = i; j < manager->count - 1; j++) {
                manager->transactions[j] = manager->transactions[j + 1];
            }

            manager->count--;
            return 1;
        }
    }

    return 0;
}