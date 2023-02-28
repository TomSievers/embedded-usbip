#pragma once

#include "types.h"

typedef struct node
{
    void* data;
    struct node* next;
    struct node* prev;
} node_t;

typedef struct linked_list
{
    size_t size;
    node_t* first;
    node_t* last;
    alloc_fn allocator;
    free_fn free;
} linked_list_t;

typedef int (*iter_cb_t)(void*, size_t, void*);

int linked_list_init(alloc_fn allocator, free_fn free, linked_list_t* list);

int linked_list_push(linked_list_t* list, void* obj);

void* linked_list_get(linked_list_t* list, size_t i);

void* linked_list_rem(linked_list_t* list, size_t i);

void linked_list_iter(linked_list_t* list, iter_cb_t iter_cb, void* ctx);