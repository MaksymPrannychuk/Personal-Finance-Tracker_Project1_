#ifndef STATISTICS_H
#define STATISTICS_H

#include "transaction.h"

// Функції для підрахунку загального доходу та витрат
double calculate_total_income(const Transaction* arr, int size);
double calculate_total_expense(const Transaction* arr, int size);

#endif