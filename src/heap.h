#pragma once

#include <stddef.h>

typedef struct heap
{
    size_t pool_size;
    size_t alignment;
    void* pool_start;
    void* blocks;
} heap_t;

typedef struct heap_node
{
    size_t size;
    struct heap_node* next;
} heap_node_t;

/**
 * @brief Initialize a heap.
 * @param name, Name of the heap.
 * @param pool, Memory to use for this heap.
 * @param pool_size, Size of the given memory in bytes.
 * @param alignment, The alignment of the memory blocks, minimum of 2.
 * @return int, -1 on failure and sets errno, otherwise 0.
 * @note The alignment must be a power of 2.
 */
int init_heap(heap_t* heap, void* pool, size_t pool_size, size_t alignment);

/**
 * @brief Allocate some memory from the heap.
 * @param heap, The heap from which memory will be allocated.
 * @param size, The size of the memory to allocate.
 * @return void*, pointer to the newly allocated memory, or NULL if no memory was free.
 */
void* heap_alloc(heap_t* heap, size_t size);

/**
 * @brief Free some memory associated with this heap.
 * @param heap, The heap to which memory will be returned.
 * @param obj, Pointer that will be freed.
 */
void heap_free(heap_t* heap, void* obj);
