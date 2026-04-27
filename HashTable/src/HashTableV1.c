#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <nmmintrin.h>

#include "HashTable.h"

typedef enum BucketState {
    BUCKET_EMPTY = 0,
    BUCKET_OCCUPIED = 1,
    BUCKET_DELETED = 2
} BucketState;

typedef struct Bucket {
    StringKey key;
    BucketState state;
} Bucket;

static size_t StringHashCRC32( StringKey key, size_t capacity ) {
    uint64_t crc = 0xFFFFFFFF;
    const uint8_t *buf = ( const uint8_t * )key.data;
    size_t len = strlen( key.data );

    while ( len >= 8 ) {
        crc = _mm_crc32_u64( crc, *( const uint64_t* )buf );
        buf += 8;
        len -= 8;
    }

    uint32_t crc32 = ( uint32_t )crc;
    while ( len > 0 ) {
        crc32 = _mm_crc32_u8( crc32, *buf );
        buf++;
        len--;
    }

    return ( size_t )( crc32 % capacity );
}

static int StringKeyEquals( StringKey lhs, StringKey rhs ) {
    if ( strlen( lhs.data ) != strlen( rhs.data ) ) {
        return 0;
    }

    if ( strlen (lhs.data ) == 0 ) {
        return 1;
    }

    return memcmp( lhs.data, rhs.data, strlen( lhs.data )  ) == 0;
}

static char *StringKeyDuplicate( StringKey key ) {
    char *copy = ( char * )malloc( MAX_LINE_LENGTH );
    if ( !copy ) {
        return NULL;
    }

    int len = strlen( key.data );

    if ( len > 0 ) {
        memcpy( copy, key.data, len );
    }
    copy[len] = '\0';
    return copy;
}

typedef struct ChainNode {
    StringKey key;
    struct ChainNode *next;
} ChainNode;

struct HashTableChaining {
    ChainNode **buckets;
    size_t capacity;
    size_t size;
    double max_load_factor;
};

size_t HashTableCapacityForLoadFactor( size_t element_count,
                                       double target_load_factor ) {
    assert( target_load_factor > 0.0 && "Invalid target_load_factor" );

    if ( element_count == 0 ) {
        return 1;
    }

    size_t capacity =
        ( size_t )( ( double )element_count / target_load_factor );
    if ( ( double )capacity * target_load_factor < ( double )element_count ) {
        ++capacity;
    }

    return capacity > 0 ? capacity : 1;
}

static double ChainingLoadFactor( const HashTableChaining *table ) {
    if ( table->capacity == 0 ) {
        return 0.0;
    }
    return ( double )table->size / ( double )table->capacity;
}

static ChainNode *ChainNodeCreate( StringKey key, ChainNode *next ) {
    ChainNode *node = ( ChainNode * )malloc( sizeof( *node ) );
    if ( !node ) {
        return NULL;
    }

    char *key_copy = StringKeyDuplicate( key );
    if ( !key_copy ) {
        free( node );
        return NULL;
    }

    node->key.data = key_copy;
    node->next = next;
    return node;
}

static void ChainFree( ChainNode *head ) {
    while ( head ) {
        ChainNode *next_node = head->next;
        free( ( void * )head->key.data );
        free( head );
        head = next_node;
    }
}

static int ChainingRehash( HashTableChaining *table, size_t new_capacity ) {
    assert( table && "Null pointer to table" );
    assert( new_capacity > 0 && "Invalid new_capacity" );

    ChainNode **new_buckets =
        ( ChainNode ** )calloc( new_capacity, sizeof( *new_buckets ) );
    if ( !new_buckets ) {
        return 0;
    }

    for ( size_t bucket_index = 0; bucket_index < table->capacity;
          ++bucket_index ) {
        ChainNode *node = table->buckets[bucket_index];
        while ( node ) {
            ChainNode *next_node = node->next;
            size_t new_index = StringHashCRC32( node->key, new_capacity );
            node->next = new_buckets[new_index];
            new_buckets[new_index] = node;
            node = next_node;
        }
    }

    free( table->buckets );
    table->buckets = new_buckets;
    table->capacity = new_capacity;
    return 1;
}

HashTableChaining *HashTableChainingCtor( size_t initial_capacity,
                                          double max_load_factor ) {
    assert( initial_capacity > 0 && "Invalid initial_capacity" );
    assert( max_load_factor > 0.0 && "Invalid max_load_factor" );

    HashTableChaining *table =
        ( HashTableChaining * )malloc( sizeof( *table ) );
    if ( !table ) {
        return NULL;
    }

    ChainNode **buckets =
        ( ChainNode ** )calloc( initial_capacity, sizeof( *buckets ) );
    if ( !buckets ) {
        free( table );
        return NULL;
    }

    table->buckets = buckets;
    table->capacity = initial_capacity;
    table->size = 0;
    table->max_load_factor = max_load_factor;
    return table;
}

void HashTableChainingDtor( HashTableChaining *table ) {
    if ( !table ) {
        return;
    }

    for ( size_t bucket_index = 0; bucket_index < table->capacity;
          ++bucket_index ) {
        ChainFree( table->buckets[bucket_index] );
    }

    free( table->buckets );
    free( table );
}

int HashTableChainingInsert( HashTableChaining *table, StringKey key ) {
    assert( table && "Null pointer to table" );
    assert( key.data && "Null pointer to key data" );

    if ( HashTableChainingContains( table, key ) ) {
        return 1;
    }

    double load_factor = ChainingLoadFactor( table );
    if ( load_factor >= table->max_load_factor ) {
        size_t new_capacity = table->capacity * 2 + 1;
        int rehashed = ChainingRehash( table, new_capacity );
        if ( !rehashed ) {
            return 0;
        }
    }

    size_t bucket_index = StringHashCRC32( key, table->capacity );
    ChainNode *node = ChainNodeCreate( key, table->buckets[bucket_index] );
    if ( !node ) {
        return 0;
    }

    table->buckets[bucket_index] = node;
    table->size++;
    return 1;
}

int HashTableChainingContains( const HashTableChaining *table, StringKey key ) {
    assert( table && "Null pointer to table" );
    assert( key.data && "Null pointer to key data" );

    size_t bucket_index = StringHashCRC32( key, table->capacity );
    ChainNode *node = table->buckets[bucket_index];
    while ( node ) {
        if ( StringKeyEquals( node->key, key ) ) {
            return 1;
        }
        node = node->next;
    }

    return 0;
}

int HashTableChainingErase( HashTableChaining *table, StringKey key ) {
    assert( table && "Null pointer to table" );
    assert( key.data && "Null pointer to key data" );

    size_t bucket_index = StringHashCRC32( key, table->capacity );
    ChainNode *node = table->buckets[bucket_index];
    ChainNode *prev = NULL;

    while ( node ) {
        if ( StringKeyEquals( node->key, key ) ) {
            if ( prev ) {
                prev->next = node->next;
            } else {
                table->buckets[bucket_index] = node->next;
            }
            free( ( void * )node->key.data );
            free( node );
            table->size--;
            return 1;
        }

        prev = node;
        node = node->next;
    }

    return 0;
}

size_t HashTableChainingSize( const HashTableChaining *table ) {
    assert( table && "Null pointer to table" );
    return table->size;
}

size_t HashTableChainingCapacity( const HashTableChaining *table ) {
    assert( table && "Null pointer to table" );
    return table->capacity;
}

double HashTableChainingLoadFactor( const HashTableChaining *table ) {
    assert( table && "Null pointer to table" );
    return ChainingLoadFactor( table );
}
