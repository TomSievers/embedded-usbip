#include "mem_pool.h"
#include <errno.h>
#include <stdint.h>

int init_mem_pool(size_t obj_size, void* pool, size_t pool_size, mem_pool_t* out_pool)
{
    if (out_pool == NULL)
    {
        errno = ENOMEM;
        return -1;
    }

    if (pool == NULL)
    {
        errno = ENOMEM;
        return -1;
    }

    if (sizeof(void*) > obj_size)
    {
        obj_size = sizeof(void*);
    }

    pool_size -= pool_size % obj_size;

    if (pool_size == 0)
    {
        errno = ENOMEM;
        return -1;
    }

    *(uintptr_t*)pool    = 0x1;

    out_pool->free_block = pool;
    out_pool->pool_start = pool;
    out_pool->obj_size   = obj_size;
    out_pool->pool_size  = pool_size;

    return 0;
}

void* mem_pool_alloc(mem_pool_t* pool)
{
    void* end = pool->pool_start + pool->pool_size;

    if (pool->free_block == end)
    {
        return NULL;
    }

    uintptr_t* res = pool->free_block;

    if (*res & 0x1)
    {
        pool->free_block += pool->obj_size;
        if (pool->free_block != end)
        {
            *((uintptr_t*)pool->free_block) = 0x1;
        }
    }
    else
    {
        pool->free_block = *((void**)pool->free_block);
    }

    return (void*)res;
}

void mem_pool_free(mem_pool_t* pool, void* obj)
{
    *((void**)obj)   = pool->free_block;
    pool->free_block = obj;
}