#include "search.h"
#include <string.h>

int search_by_category(const Transaction* arr, int size, const char* category, Transaction* result) {
    int count = 0;
    
    for (int i = 0; i < size; i++) {
        // Порівнюємо рядки. Якщо повернуло 0 - рядки однакові
        if (strcmp(arr[i].category, category) == 0) {
            result[count] = arr[i]; // Копіюємо знайдену транзакцію в масив результатів
            count++;
        }
    }
    
    return count; // Повертаємо кількість знайдених транзакцій
}