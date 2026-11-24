#ifndef TRANSACTION_MANAGER_H
#define TRANSACTION_MANAGER_H

#include "../../Pistryak/transaction.h"

#define MAX_TRANSACTIONS 1000

typedef struct {
    Transaction transactions[MAX_TRANSACTIONS];
    int count;
    int next_id;
} TransactionManager;

void init_transaction_manager(TransactionManager *manager);

int add_transaction(TransactionManager *manager, Transaction transaction);

int edit_transaction(TransactionManager *manager, int id, Transaction updated);

int delete_transaction(TransactionManager *manager, int id);

#endif