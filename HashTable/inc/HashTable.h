#ifndef HASH_TABLES_H
#define HASH_TABLES_H

#include <stddef.h>
#include "CFList.h"

typedef struct {
    int *buckets;
    CFList *list;
    size_t capacity;
    double max_load_factor;
} HashTable;

HashTable* HashTableCtor( size_t initial_capacity, double max_load_factor );
void       HashTableDtor( HashTable *table );
int        HashTableInsert( HashTable *table, const char* key_data );
int        HashTableContains( const HashTable *table, const char* key_data );

#endif /* HASH_TABLES_H */