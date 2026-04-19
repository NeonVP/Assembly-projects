#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "HashTable.h"

#define MAX_LINE_LENGTH 256
#define TARGET_LOAD_FACTOR 10.0

int main() {
    int n = 0;
    if ( scanf( "%d", &n ) != 1 ) {
        fprintf(stderr, "Ошибка: не удалось считать количество строк (n).\n");
        return 1;
    }

    size_t initial_capacity = HashTableCapacityForLoadFactor( ( size_t )n, TARGET_LOAD_FACTOR );

    HashTableChaining *table = HashTableChainingCtor( initial_capacity, TARGET_LOAD_FACTOR );
    if ( table == NULL ) {
        fprintf(stderr, "Ошибка: не удалось выделить память под хэш-таблицу.\n");
        return 1;
    }

    char buffer[MAX_LINE_LENGTH] = {};
    for ( int i = 0; i < n; i++ ) {
        if ( scanf( "%s", buffer ) != 1 ) {
            fprintf(stderr, "Предупреждение: файл закончился раньше, чем ожидалось.\n");
            break;
        }

        size_t len = strlen( buffer );
        if ( len > 0 && buffer[len - 1] == '\n' ) {
            buffer[len - 1] = '\0';
            len--;
        }

        StringKey key = { buffer, len };
        HashTableChainingInsert( table, key );
    }

    printf("--- Статистика хэш-таблицы (V0 - Базовая) ---\n");
    printf("Элементов вставлено: %zu\n", HashTableChainingSize(table));
    printf("Текущая вместимость: %zu\n", HashTableChainingCapacity(table));
    printf("Load factor:         %f\n", HashTableChainingLoadFactor(table));

    HashTableChainingDtor(table);

    return 0;
}
