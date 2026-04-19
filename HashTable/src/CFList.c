#include "CFList.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

CFList* CFListCtor(size_t initial_capacity) {
    if ( initial_capacity == 0 ) initial_capacity = 16;

    CFList *list = ( CFList* )malloc( sizeof( CFList ) );
    if ( !list ) return NULL;

    list->nodes = ( CFNode* )malloc( initial_capacity * sizeof( CFNode ) );
    if ( !list->nodes ) {
        free( list );
        return NULL;
    }

    list->capacity = initial_capacity;
    list->size = 0;
    list->free_head = CF_NULL_INDEX;

    return list;
}

void CFListDtor( CFList *list ) {
    if ( list ) {
        free( list->nodes );
        free( list );
    }
}

static int CFListResize( CFList *list ) {
    size_t new_capacity = list->capacity * 2;
    CFNode *new_nodes = ( CFNode* )realloc( list->nodes, new_capacity * sizeof( CFNode ) );
    if (!new_nodes) return 0;
    
    list->nodes = new_nodes;
    list->capacity = new_capacity;
    return 1;
}

int CFListAllocateNode( CFList *list, const char *key_data, int next_idx ) {
    assert( list != NULL );
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

    return new_idx;
}

void CFListFreeNode( CFList *list, int node_idx ) {
    assert( list != NULL );
    if ( node_idx == CF_NULL_INDEX ) return;

    list->nodes[node_idx].next = list->free_head;
    list->free_head = node_idx;
}

CFNode* CFListGetNode(const CFList *list, int node_idx ) {
    assert( list != NULL );
    if ( node_idx == CF_NULL_INDEX ) return NULL;
    return &list->nodes[node_idx];
}
