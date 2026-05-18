#include "CFList.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#if CFLIST_ENABLE_VERIFY
enum {
    CF_CANARY_REGION_SIZE = sizeof( uint64_t )
};

static const uint64_t CF_LIST_CANARY = 0xC0FFEE12DEADBEEFULL;
static const uint64_t CF_NODES_CANARY = 0xABCDEF0198765432ULL;

static inline uint8_t* CFNodesRawBegin( const CFNode *nodes ) {
    return ( uint8_t* )nodes - CF_CANARY_REGION_SIZE;
}

static inline const uint64_t* CFNodesLeftCanary( const CFNode *nodes ) {
    return ( const uint64_t* )CFNodesRawBegin( nodes );
}

static inline const uint64_t* CFNodesRightCanary( const CFList *list ) {
    return ( const uint64_t* )( ( const uint8_t* )list->nodes + list->capacity * sizeof( CFNode ) );
}

static CFNode* CFNodesAllocWithCanaries( size_t capacity ) {
    const size_t raw_size = CF_CANARY_REGION_SIZE + capacity * sizeof( CFNode ) + CF_CANARY_REGION_SIZE;
    uint8_t *raw = ( uint8_t* )malloc( raw_size );
    if ( !raw ) return NULL;

    *( ( uint64_t* )raw ) = CF_NODES_CANARY;
    *( ( uint64_t* )( raw + CF_CANARY_REGION_SIZE + capacity * sizeof( CFNode ) ) ) = CF_NODES_CANARY;

    return ( CFNode* )( raw + CF_CANARY_REGION_SIZE );
}

static void CFNodesFreeWithCanaries( CFNode *nodes ) {
    if ( !nodes ) return;
    free( CFNodesRawBegin( nodes ) );
}

static CFNode* CFNodesReallocWithCanaries( CFNode *nodes, size_t old_capacity, size_t new_capacity ) {
    const size_t old_size = CF_CANARY_REGION_SIZE + old_capacity * sizeof( CFNode ) + CF_CANARY_REGION_SIZE;
    const size_t new_size = CF_CANARY_REGION_SIZE + new_capacity * sizeof( CFNode ) + CF_CANARY_REGION_SIZE;

    uint8_t *old_raw = CFNodesRawBegin( nodes );
    uint8_t *new_raw = ( uint8_t* )realloc( old_raw, new_size );
    if ( !new_raw ) return NULL;

    *( ( uint64_t* )new_raw ) = CF_NODES_CANARY;
    *( ( uint64_t* )( new_raw + CF_CANARY_REGION_SIZE + new_capacity * sizeof( CFNode ) ) ) = CF_NODES_CANARY;

    if ( new_size > old_size ) {
        memset( new_raw + old_size, 0, new_size - old_size );
    }

    return ( CFNode* )( new_raw + CF_CANARY_REGION_SIZE );
}
#endif

CFList* CFListCtor(size_t initial_capacity) {
    if ( initial_capacity == 0 ) initial_capacity = 16;

    CFList *list = ( CFList* )malloc( sizeof( CFList ) );
    if ( !list ) return NULL;

#if CFLIST_ENABLE_VERIFY
    list->nodes = CFNodesAllocWithCanaries( initial_capacity );
#else
    list->nodes = ( CFNode* )malloc( initial_capacity * sizeof( CFNode ) );
#endif
    if ( !list->nodes ) {
        free( list );
        return NULL;
    }

#if CFLIST_ENABLE_VERIFY
    list->left_canary = CF_LIST_CANARY;
    list->right_canary = CF_LIST_CANARY;
#endif
    list->capacity = initial_capacity;
    list->size = 0;
    list->free_head = CF_NULL_INDEX;

#if CFLIST_ENABLE_VERIFY
    assert( CFListVerify( list ) );
#endif
    return list;
}

void CFListDtor( CFList *list ) {
    if ( list ) {
#if CFLIST_ENABLE_VERIFY
        assert( CFListVerify( list ) );
        CFNodesFreeWithCanaries( list->nodes );
#else
        free( list->nodes );
#endif
        free( list );
    }
}

static int CFListResize( CFList *list ) {
#if CFLIST_ENABLE_VERIFY
    assert( CFListVerify( list ) );
#endif
    size_t new_capacity = list->capacity * 2;

#if CFLIST_ENABLE_VERIFY
    CFNode *new_nodes = CFNodesReallocWithCanaries( list->nodes, list->capacity, new_capacity );
#else
    CFNode *new_nodes = ( CFNode* )realloc( list->nodes, new_capacity * sizeof( CFNode ) );
#endif
    if (!new_nodes) return 0;
    
    list->nodes = new_nodes;
    list->capacity = new_capacity;
#if CFLIST_ENABLE_VERIFY
    assert( CFListVerify( list ) );
#endif
    return 1;
}

int CFListAllocateNode( CFList *list, const char *key_data, int next_idx ) {
    assert( list != NULL );
#if CFLIST_ENABLE_VERIFY
    assert( CFListVerify( list ) );
#endif
    int new_idx = CF_NULL_INDEX;

    if ( list->free_head != CF_NULL_INDEX ) {
        new_idx = list->free_head;
        list->free_head = list->nodes[new_idx].next;
    } else {
        if ( list->size >= list->capacity ) {
            if ( !CFListResize( list ) ) return CF_NULL_INDEX;
        }
        new_idx = list->size++;
    }

    CFNode* node = &list->nodes[new_idx];

    memcpy( node->key_data, key_data, MAX_LINE_LENGTH );
    node->next = next_idx;

#if CFLIST_ENABLE_VERIFY
    assert( CFListVerify( list ) );
#endif
    return new_idx;
}

void CFListFreeNode( CFList *list, int node_idx ) {
    assert( list != NULL );
#if CFLIST_ENABLE_VERIFY
    assert( CFListVerify( list ) );
#endif
    if ( node_idx == CF_NULL_INDEX ) return;

    list->nodes[node_idx].next = list->free_head;
    list->free_head = node_idx;
#if CFLIST_ENABLE_VERIFY
    assert( CFListVerify( list ) );
#endif
}

CFNode* CFListGetNode(const CFList *list, int node_idx ) {
    assert( list != NULL );
#if CFLIST_ENABLE_VERIFY
    assert( CFListVerify( list ) );
#endif
    if ( node_idx == CF_NULL_INDEX ) return NULL;
    return &list->nodes[node_idx];
}

#if CFLIST_ENABLE_VERIFY
int CFListVerify( const CFList *list ) {
    if ( !list ) return 0;
    if ( list->left_canary != CF_LIST_CANARY || list->right_canary != CF_LIST_CANARY ) return 0;
    if ( !list->nodes ) return 0;
    if ( list->capacity == 0 || list->size > list->capacity ) return 0;
    if ( list->free_head != CF_NULL_INDEX && ( list->free_head < 0 || ( size_t )list->free_head >= list->capacity ) ) return 0;

    if ( *CFNodesLeftCanary( list->nodes ) != CF_NODES_CANARY ) return 0;
    if ( *CFNodesRightCanary( list ) != CF_NODES_CANARY ) return 0;

    int current = list->free_head;
    size_t steps = 0;
    while ( current != CF_NULL_INDEX ) {
        if ( current < 0 || ( size_t )current >= list->capacity ) return 0;
        current = list->nodes[current].next;
        if ( ++steps > list->capacity ) return 0;
    }

    return 1;
}
#endif
