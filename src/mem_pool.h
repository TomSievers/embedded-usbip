#pragma once

#include <stddef.h>

typedef struct mem_pool
{
    size_t pool_size;
    size_t obj_size;
    void* pool_start;
    void* free_block;
} mem_pool_t;

/**
 * @brief Initialize a memory pool.
 * @param obj_size, The size of the objects being allocated in this pool, the real size will be at
 * minimum the size of pointers.
 * @param pool, Memory to use for this pool.
 * @param pool_size, Size of the given memory in bytes.
 * @param out_pool, Object into which pool will be initialized.
 * @return int, -1 on failure and sets errno, otherwise 0.
 */
int init_mem_pool(size_t obj_size, void* pool, size_t pool_size, mem_pool_t* out_pool);

/**
 * @brief Allocate some memory from the memory pool
 * @param pool, The pool from which memory will be allocated.
 * @return void*, pointer to the newly allocated memory, or NULL if no memory was free.
 */
void* mem_pool_alloc(mem_pool_t* pool);

/**
 * @brief Free some memory associated with this memory pool.
 * @param pool, The pool to which memory will be returned.
 * @param obj, Pointer that will be freed.
 */
void mem_pool_free(mem_pool_t* pool, void* obj);