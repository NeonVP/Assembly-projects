#ifndef HASH_TABLES_H
#define HASH_TABLES_H

#include <stddef.h>

typedef struct HashTableChaining HashTableChaining;
typedef struct HashTableLinear HashTableLinear;
typedef struct HashTableQuadratic HashTableQuadratic;
typedef struct HashTableDouble HashTableDouble;
typedef struct HashTableCuckoo HashTableCuckoo;

HashTableChaining *HashTableChainingCtor( size_t initial_capacity,
                                          double max_load_factor );
void HashTableChainingDtor( HashTableChaining *table );
int HashTableChainingInsert( HashTableChaining *table, int key );
int HashTableChainingContains( const HashTableChaining *table, int key );
int HashTableChainingErase( HashTableChaining *table, int key );

#endif /* HASH_TABLES_H */
