#include "heap.h"
#include <errno.h>
#include <stdint.h>

static inline int is_aligned(void* ptr, size_t alignment)
{
    return ((uintptr_t)ptr & (alignment - 1)) == 0;
}

static inline int is_allocated(void* ptr) { return ((uintptr_t)ptr) & 0x1; }

static inline void* set_allocated(void* ptr) { return (void*)(((uintptr_t)ptr) | 0x1); }

static inline void* clear_allocated(void* ptr) { return (void*)(((uintptr_t)ptr) & ~0x1); }

int init_heap(heap_t* heap, void* pool, size_t pool_size, size_t alignment)
{
    if (heap == NULL || pool == NULL || alignment < 2 || !(is_aligned(pool, alignment)))
    {
        errno = EINVAL;
        return -1;
    }

    heap->pool_start = pool;
    heap->pool_size = pool_size;
    heap->alignment = alignment;
    heap->blocks = pool;

    heap_node_t* block = heap->blocks;
    block->size = pool_size - sizeof(heap_node_t);
    block->next = NULL;

    return 0;
}

void* heap_alloc(heap_t* heap, size_t size)
{
    if (heap == NULL || size == 0)
    {
        errno = EINVAL;
        return NULL;
    }

    size_t aligned_size = (is_aligned((void*)size, heap->alignment))
        ? size
        : size + (heap->alignment - (size % heap->alignment));

    heap_node_t* cur = heap->blocks;

    while (cur != NULL)
    {
        // Merge with the next block if it is free
        while (!is_allocated(cur->next) && cur->next != NULL && !is_allocated(cur->next->next))
        {
            cur->size += cur->next->size + sizeof(heap_node_t);
            cur->next = cur->next->next;
        }

        // Check if this block is large enough and not already allocated
        if (cur->size >= aligned_size && !is_allocated(cur->next))
        {
            // Check if we can split the block
            if (cur->size - aligned_size > sizeof(heap_node_t))
            {
                // Create a new block
                heap_node_t* new_block
                    = (heap_node_t*)((uintptr_t)cur + aligned_size + sizeof(heap_node_t));
                // Set the new block size
                new_block->size = cur->size - aligned_size - sizeof(heap_node_t);
                // Set the new block next pointer
                new_block->next = cur->next;

                cur->size = aligned_size;
                cur->next = new_block;
            }

            cur->next = set_allocated(cur->next);

            return (void*)((uintptr_t)cur + sizeof(heap_node_t));
        }

        cur = clear_allocated(cur->next);
    }

    errno = ENOMEM;
    return NULL;
}

void heap_free(heap_t* heap, void* obj)
{
    if (heap == NULL || obj == NULL)
    {
        errno = EINVAL;
        return;
    }

    // Get the block to free
    heap_node_t* cur = obj - sizeof(heap_node_t);
    cur->next = clear_allocated(cur->next);

    // Merge with the next block if it is free
    if (cur->next != NULL && !is_allocated(cur->next->next))
    {
        cur->size += cur->next->size + sizeof(heap_node_t);
        cur->next = cur->next->next;
    }
}