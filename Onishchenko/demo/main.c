#include <stdio.h>
#include "../include/transaction_manager.h"

void print_menu(void) {
    printf("\n===== PERSONAL FINANCE TRACKER =====\n");
    printf("1. Add transaction\n");
    printf("2. Show all transactions\n");
    printf("3. Edit transaction\n");
    printf("4. Delete transaction\n");
    printf("0. Exit\n");
    printf("Choose an option: ");
}

int main(void) {
    TransactionManager manager;
    init_transaction_manager(&manager);

    int choice;

    do {
        print_menu();
        scanf("%d", &choice);

        switch (choice) {
            case 1: {
                Transaction transaction;

                printf("\n--- Add New Transaction ---\n");

                printf("Enter type (1 - Income, 2 - Expense): ");
                scanf("%d", &transaction.is_income);

                if (transaction.is_income != 1 && transaction.is_income != 2) {
                    printf("Invalid transaction type.\n");
                    break;
                }

                transaction.is_income = (transaction.is_income == 1);

                printf("Enter amount: ");
                scanf("%lf", &transaction.amount);

                if (transaction.amount <= 0) {
                    printf("Invalid amount.\n");
                    break;
                }

                printf("Enter date (YYYY-MM-DD): ");
                scanf("%10s", transaction.date);

                printf("Enter category: ");
                scanf("%49s", transaction.category);

                if (add_transaction(&manager, transaction)) {
                    printf("Transaction added successfully!\n");
                } else {
                    printf("Failed to add transaction.\n");
                }

                break;
            }

            case 2:
                printf("\n--- All Transactions ---\n");

                 if (manager.count == 0) {
               printf("No transactions found.\n");
                } else {
                 for (int i = 0; i < manager.count; i++) {
                 Transaction t = manager.transactions[i];

                 printf("\nID: %d\n", t.id);
                printf("Type: %s\n", t.is_income ? "Income" : "Expense");
                printf("Amount: %.2f\n", t.amount);
                printf("Date: %s\n", t.date);
                printf("Category: %s\n", t.category);
                }
            }

                break;

            case 3: {
                int id;
                Transaction updated;

                printf("\n--- Edit Transaction ---\n");
                printf("Enter transaction ID: ");
                scanf("%d", &id);

                printf("Enter new type (1 - Income, 2 - Expense): ");
                scanf("%d", &updated.is_income);

                if (updated.is_income != 1 && updated.is_income != 2) {
                    printf("Invalid transaction type.\n");
                    break;
                }

                updated.is_income = (updated.is_income == 1);

                printf("Enter new amount: ");
                scanf("%lf", &updated.amount);

                if (updated.amount <= 0) {
                    printf("Invalid amount.\n");
                    break;
                }

                printf("Enter new date (YYYY-MM-DD): ");
                scanf("%10s", updated.date);

                printf("Enter new category: ");
                scanf("%49s", updated.category);

                if (edit_transaction(&manager, id, updated)) {
                    printf("Transaction updated successfully!\n");
                } else {
                    printf("Transaction not found.\n");
                }

                break;
            }
            case 4: {
                int id;

                printf("\n--- Delete Transaction ---\n");
                printf("Enter transaction ID: ");
                scanf("%d", &id);

                if (delete_transaction(&manager, id)) {
                    printf("Transaction deleted successfully!\n");
                } else {
                    printf("Transaction not found.\n");
                }

                break;
            }

            case 0:
                printf("Goodbye!\n");
                break;

            default:
                printf("Invalid option. Try again.\n");
        }

    } while (choice != 0);

    return 0;
}