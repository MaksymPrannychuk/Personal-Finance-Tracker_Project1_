#ifndef TRANSACTION_H
#define TRANSACTION_H

typedef struct {
    int id;
    char date[11];      // Формат "YYYY-MM-DD"
    double amount;      // Сума
    char category[50];  // "Їжа", "Транспорт" тощо
    int is_income;      // 1 - дохід, 0 - витрата
} Transaction;

#endif