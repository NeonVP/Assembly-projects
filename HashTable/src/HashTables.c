#include <assert.h>
#include <stdlib.h>

#include "HashTables.h"

typedef enum BucketState {
    BUCKET_EMPTY = 0,
    BUCKET_OCCUPIED = 1,
    BUCKET_DELETED = 2
} BucketState;

typedef struct Bucket {
    int key;
    BucketState state;
} Bucket;

static size_t HashPrimary( int key, size_t capacity ) {
    const double constant = 0.6180339887498949;
    double fraction = constant * ( double )key;
    fraction -= ( double )( ( long long )fraction );
    if ( fraction < 0.0 ) {
        fraction += 1.0;
    }
    return ( size_t )( fraction * ( double )capacity );
}

static size_t HashSecondary( int key, size_t capacity ) {
    const double constant = 0.7071067811865476;
    double fraction = constant * ( double )key;
    fraction -= ( double )( ( long long )fraction );
    if ( fraction < 0.0 ) {
        fraction += 1.0;
    }
    size_t step = ( size_t )( fraction * ( double )( capacity - 1 ) );
    return step + 1;
}

typedef struct ChainNode {
    int key;
    struct ChainNode *next;
} ChainNode;

struct HashTableChaining {
    ChainNode **buckets;
    size_t capacity;
    size_t size;
    double max_load_factor;
};

static double ChainingLoadFactor( const HashTableChaining *table ) {
    if ( table->capacity == 0 ) {
        return 0.0;
    }
    return ( double )table->size / ( double )table->capacity;
}

static ChainNode *ChainNodeCreate( int key, ChainNode *next ) {
    ChainNode *node = ( ChainNode * )malloc( sizeof( *node ) );
    if ( !node ) {
        return NULL;
    }
    node->key = key;
    node->next = next;
    return node;
}

static void ChainFree( ChainNode *head ) {
    while ( head ) {
        ChainNode *next_node = head->next;
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
            size_t new_index = HashPrimary( node->key, new_capacity );
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

int HashTableChainingInsert( HashTableChaining *table, int key ) {
    assert( table && "Null pointer to table" );

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

    size_t bucket_index = HashPrimary( key, table->capacity );
    ChainNode *node = ChainNodeCreate( key, table->buckets[bucket_index] );
    if ( !node ) {
        return 0;
    }

    table->buckets[bucket_index] = node;
    table->size++;
    return 1;
}

int HashTableChainingContains( const HashTableChaining *table, int key ) {
    assert( table && "Null pointer to table" );

    size_t bucket_index = HashPrimary( key, table->capacity );
    ChainNode *node = table->buckets[bucket_index];
    while ( node ) {
        if ( node->key == key ) {
            return 1;
        }
        node = node->next;
    }

    return 0;
}

int HashTableChainingErase( HashTableChaining *table, int key ) {
    assert( table && "Null pointer to table" );

    size_t bucket_index = HashPrimary( key, table->capacity );
    ChainNode *node = table->buckets[bucket_index];
    ChainNode *prev = NULL;

    while ( node ) {
        if ( node->key == key ) {
            if ( prev ) {
                prev->next = node->next;
            } else {
                table->buckets[bucket_index] = node->next;
            }
            free( node );
            table->size--;
            return 1;
        }

        prev = node;
        node = node->next;
    }

    return 0;
}
