#ifndef CFLIST_H
#define CFLIST_H

#include <stddef.h>

#define MAX_LINE_LENGTH 32
#define CF_NULL_INDEX -1

typedef struct CFNode {
    char key_data[MAX_LINE_LENGTH];
    int next;
} CFNode;

typedef struct CFList {
    CFNode *nodes;
    size_t capacity;
    size_t size;
    int free_head;
} CFList;

CFList* CFListCtor( size_t initial_capacity );
void    CFListDtor( CFList *list );

int     CFListAllocateNode( CFList* list, const char* key_data, int next_idx );
void    CFListFreeNode    ( CFList* list, int node_idx );
CFNode* CFListGetNode     ( const CFList* list, int node_idx );


#endif