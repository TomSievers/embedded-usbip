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

#define INIT_LINKED_LIST(name, alloc, free_fn)                                                     \
    static linked_list_t name                                                                      \
        = { .size = 0, .first = NULL, .last = NULL, .allocator = alloc, .free = free_fn };

/**
 * @brief Iterator callback
 * @param obj, Pointer to the current object of iteration.
 * @param i, Index of the current object.
 * @param ctx, The context passed to the iter function.
 * @return int, 0 if the current object is still valid, -1 if the object was invalidated during this
 * callback function
 */
typedef int (*iter_cb_t)(void* obj, size_t i, void* ctx);

/**
 * @brief Initialize a linked list using an allocator.
 * @param allocator, Pointer to function to use for allocating nodes in the linked list.
 * @param free, Pointer to function to free allocated memory with.
 * @param list, Pointer to the linked list object that should be initialized
 * @return int, -1 on failure and sets errno, otherwise 0
 */
int linked_list_init(alloc_fn allocator, free_fn free, linked_list_t* list);

/**
 * @brief Push an element to the end of the linked list.
 * @param list, The list to push an element onto.
 * @param obj, The object to be pushed onto the list.
 * @return int, -1 on failure and sets errno, otherwise 0
 */
int linked_list_push(linked_list_t* list, void* obj);

/**
 * @brief Get an element at the specified position from the list.
 * @param list, The list to search the index for.
 * @param i, The index to get from the list.
 * @return void*, Pointer to the retrieved object, if it was not found a NULL pointer is returned.
 */
void* linked_list_get(linked_list_t* list, size_t i);

/**
 * @brief Remove an element at the specified position.
 * @param list, The list to remove the index from.
 * @param i, The index of the item to be removed.
 * @return void*, The pointer to the underlying object, if the index does not exist a NULL pointer
 * is returned.
 */
void* linked_list_rem(linked_list_t* list, size_t i);

/**
 * @brief Iterate through a linked list using a callback function with a given context.
 * @param list, The list to iterate through.
 * @param iter_cb, The function to call for every iterated object.
 * @param ctx, A context passed into the function which may point to some relevant object.
 */
void linked_list_iter(linked_list_t* list, iter_cb_t iter_cb, void* ctx);