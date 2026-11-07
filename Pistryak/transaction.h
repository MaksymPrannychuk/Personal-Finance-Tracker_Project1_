#ifndef TRANSACTION_H
#define TRANSACTION_H

// Структура, що описує одну фінансову операцію
typedef struct {
    int id;
    char date[11];      // Формат "YYYY-MM-DD"
    double amount;      // Сума транзакції
    char category[50];  // Категорія ("Їжа", "Транспорт" тощо)
    int is_income;      // 1 - дохід, 0 - витрата
} Transaction;

#endif