#include "mem_pool.h"
#include <errno.h>
#include <stdint.h>

int init_mem_pool(size_t obj_size, void* pool, size_t pool_size, mem_pool_t* out_pool)
{
    if (out_pool == NULL || pool == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    // Check if pointer size is larger than the size of the objects being allocated.
    // Minimum element size is pointer size because free blocks will at some point store pointers.
    if (sizeof(void*) > obj_size)
    {
        obj_size = sizeof(void*);
    }

    // Limit the real pool size to the maximum allowable object.
    pool_size -= pool_size % obj_size;

    if (pool_size == 0)
    {
        errno = EINVAL;
        return -1;
    }

    // Initialze pool memory
    *(uintptr_t*)pool = 0x1;

    out_pool->free_block = pool;
    out_pool->pool_start = pool;
    out_pool->obj_size = obj_size;
    out_pool->pool_size = pool_size;

    return 0;
}

void* mem_pool_alloc(mem_pool_t* pool)
{
    // Get end pointer of memory pool
    void* end = pool->pool_start + pool->pool_size;

    // Check if the first free block is at the end, no memory free.
    if (pool->free_block == end)
    {
        return NULL;
    }

    uintptr_t* res = pool->free_block;

    // If bit 0 is set this this block was not previously allocated and does not point to another
    // free block
    if (*res & 0x1)
    {
        pool->free_block += pool->obj_size;
        if (pool->free_block != end)
        {
            *((uintptr_t*)pool->free_block) = 0x1;
        }
    }
    // The free block points to another free block retrieve this block and set it as the new free
    // block.
    else
    {
        pool->free_block = *((void**)pool->free_block);
    }

    return (void*)res;
}

void mem_pool_free(mem_pool_t* pool, void* obj)
{
    // Store the address of the current free block into the object being freed.
    *((void**)obj) = pool->free_block;

    // Set the freed object as the new free block.
    pool->free_block = obj;
}