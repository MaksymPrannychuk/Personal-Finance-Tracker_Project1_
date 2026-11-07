#include "statistics.h"

double calculate_total_income(const Transaction* arr, int size) {
    double total = 0.0;
    
    for (int i = 0; i < size; i++) {
        if (arr[i].is_income == 1) {
            total += arr[i].amount;
        }
    }
    
    return total;
}

double calculate_total_expense(const Transaction* arr, int size) {
    double total = 0.0;
    
    for (int i = 0; i < size; i++) {
        if (arr[i].is_income == 0) {
            total += arr[i].amount;
        }
    }
    
    return total;
}