#pragma once

#include <stddef.h>

typedef struct mem_pool
{
    size_t pool_size;
    size_t obj_size;
    void* pool_start;
    void* free_block;
} mem_pool_t;

int init_mem_pool(size_t obj_size, void* pool, size_t pool_size, mem_pool_t* out_pool);

void* mem_pool_alloc(mem_pool_t* pool);

void mem_pool_free(mem_pool_t* pool, void* obj);