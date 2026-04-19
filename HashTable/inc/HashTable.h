#ifndef HASH_TABLES_H
#define HASH_TABLES_H

#include <stddef.h>

typedef struct StringKey {
    const char *data;
    size_t len;
} StringKey;

typedef struct HashTableChaining HashTableChaining;
typedef struct HashTableLinear HashTableLinear;
typedef struct HashTableQuadratic HashTableQuadratic;
typedef struct HashTableDouble HashTableDouble;
typedef struct HashTableCuckoo HashTableCuckoo;

size_t HashTableCapacityForLoadFactor( size_t element_count,
                                       double target_load_factor );

HashTableChaining *HashTableChainingCtor( size_t initial_capacity,
                                          double max_load_factor );
void HashTableChainingDtor( HashTableChaining *table );
int HashTableChainingInsert( HashTableChaining *table, StringKey key );
int HashTableChainingContains( const HashTableChaining *table, StringKey key );
int HashTableChainingErase( HashTableChaining *table, StringKey key );
size_t HashTableChainingSize( const HashTableChaining *table );
size_t HashTableChainingCapacity( const HashTableChaining *table );
double HashTableChainingLoadFactor( const HashTableChaining *table );

#endif /* HASH_TABLES_H */
