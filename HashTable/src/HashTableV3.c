#include "HashTable.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <immintrin.h>
#include <stdint.h>

size_t HashCRC32( const char *data, size_t capacity ) {
    uint64_t crc = 0xFFFFFFFF;
    const uint64_t *buf = ( const uint64_t * )data;
    
    crc = _mm_crc32_u64( crc, buf[0] );
    crc = _mm_crc32_u64( crc, buf[1] );
    crc = _mm_crc32_u64( crc, buf[2] );
    crc = _mm_crc32_u64( crc, buf[3] );
    
    return ( size_t )( crc % capacity );
}

HashTable* HashTableCtor( size_t initial_capacity, double max_load_factor ) {
    HashTable *table = ( HashTable* )malloc( sizeof( HashTable ) );
    if ( !table ) return NULL;

    table->capacity = initial_capacity;
    table->max_load_factor = max_load_factor;

    table->buckets = ( int* )malloc( initial_capacity * sizeof( int ) );
    for ( size_t i = 0; i < initial_capacity; i++ ) {
        table->buckets[i] = CF_NULL_INDEX;
    }

    size_t pool_size = ( size_t )( initial_capacity * max_load_factor ) + 16;
    table->list = CFListCtor( pool_size );

    return table;
}

void HashTableDtor( HashTable *table ) {
    if ( !table ) return;
    CFListDtor( table->list );
    free( table->buckets );
    free( table );
}

int HashTableInsert( HashTable *table, const char* key_data ) {    
    size_t idx = HashCRC32( key_data, table->capacity );

    int new_node_idx = CFListAllocateNode( table->list, key_data, table->buckets[idx] );
    if ( new_node_idx == CF_NULL_INDEX ) return 0;

    table->buckets[idx] = new_node_idx;
    return 1;
}

extern int HashTableContains( const HashTable *table, const char* key_data );
