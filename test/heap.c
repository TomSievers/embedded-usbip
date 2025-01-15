#include "heap.h"
#include "test.h"
#include <errno.h>
#include <stdint.h>

test(test_heap_create_no_pool_obj)
{
    uint64_t some;

    assert_int_eq(init_heap(NULL, &some, 0, 0), -1);
    assert_int_eq(errno, EINVAL);
}

test(test_heap_create_no_pool)
{
    heap_t some;

    assert_int_eq(init_heap(&some, NULL, 0, 0), -1);
    assert_int_eq(errno, EINVAL);
}

test(test_heap_create_unaligned_pool)
{
    heap_t some;
    uint8_t mem[16];

    assert_int_eq(init_heap(&some, mem + 1, 16, 2), -1);
    assert_int_eq(errno, EINVAL);
}

test(test_heap_create_obj_lt_ptr)
{
    heap_t pool;
    uint64_t mem[2];

    assert_int_eq(init_heap(&pool, &mem, 16, 4), 0);
    assert_int_eq(pool.alignment, 4);
    assert_int_eq(pool.pool_size, 16);
    assert_ptr_eq(pool.pool_start, &mem);
    assert_ptr_eq(pool.blocks, &mem);
}

test(test_heap_alloc_simple)
{
    heap_t pool;
    uint64_t mem[64];

    assert_int_eq(init_heap(&pool, mem, sizeof(uint64_t) * 64, 4), 0);
    assert_int_eq(pool.alignment, 4);
    assert_int_eq(pool.pool_size, sizeof(uint64_t) * 64);
    assert_ptr_eq(pool.pool_start, &mem);
    assert_ptr_eq(pool.blocks, &mem);

    void* one = heap_alloc(&pool, 4);
    void* two = heap_alloc(&pool, 4);

    assert_ptr_eq(one, (uint8_t*)mem + sizeof(heap_node_t));
    assert_ptr_eq(two, (uint8_t*)mem + 4 + sizeof(heap_node_t) * 2);
}

test(test_heap_alloc_free_realloc)
{
    heap_t pool;
    uint64_t mem[64];

    assert_int_eq(init_heap(&pool, mem, sizeof(uint64_t) * 64, 4), 0);
    assert_int_eq(pool.alignment, 4);
    assert_int_eq(pool.pool_size, sizeof(uint64_t) * 64);
    assert_ptr_eq(pool.pool_start, &mem);
    assert_ptr_eq(pool.blocks, &mem);

    void* one = heap_alloc(&pool, 4);
    void* two = heap_alloc(&pool, 4);
    void* three = heap_alloc(&pool, 4);

    assert_ptr_eq(one, (uint8_t*)mem + sizeof(heap_node_t));
    assert_ptr_eq(two, (uint8_t*)mem + 4 + sizeof(heap_node_t) * 2);
    assert_ptr_eq(three, (uint8_t*)mem + 8 + sizeof(heap_node_t) * 3);

    heap_free(&pool, two);

    void* four = heap_alloc(&pool, 4);

    assert_ptr_eq(four, (uint8_t*)mem + 4 + sizeof(heap_node_t) * 2);
}

test(test_heap_free_combine_free_blocks)
{
    heap_t pool;
    uint64_t mem[64];

    assert_int_eq(init_heap(&pool, mem, sizeof(uint64_t) * 64, 4), 0);
    assert_int_eq(pool.alignment, 4);
    assert_int_eq(pool.pool_size, sizeof(uint64_t) * 64);
    assert_ptr_eq(pool.pool_start, &mem);
    assert_ptr_eq(pool.blocks, &mem);

    void* one = heap_alloc(&pool, 4);
    void* two = heap_alloc(&pool, 4);
    void* three = heap_alloc(&pool, 4);

    assert_ptr_eq(one, (uint8_t*)mem + sizeof(heap_node_t));
    assert_ptr_eq(two, (uint8_t*)mem + 4 + sizeof(heap_node_t) * 2);
    assert_ptr_eq(three, (uint8_t*)mem + 8 + sizeof(heap_node_t) * 3);

    heap_free(&pool, two);
    heap_free(&pool, one);

    void* four = heap_alloc(&pool, 8);

    assert_ptr_eq(four, one);
}

test(test_heap_free_memory_fragmentation)
{
    heap_t pool;
    uint64_t mem[64];

    assert_int_eq(init_heap(&pool, mem, sizeof(uint64_t) * 64, 4), 0);
    assert_int_eq(pool.alignment, 4);
    assert_int_eq(pool.pool_size, sizeof(uint64_t) * 64);
    assert_ptr_eq(pool.pool_start, &mem);
    assert_ptr_eq(pool.blocks, &mem);

    void* one = heap_alloc(&pool, 4);
    void* two = heap_alloc(&pool, 4);
    void* three = heap_alloc(&pool, 4);

    assert_ptr_eq(one, (uint8_t*)mem + sizeof(heap_node_t));
    assert_ptr_eq(two, (uint8_t*)mem + 4 + sizeof(heap_node_t) * 2);
    assert_ptr_eq(three, (uint8_t*)mem + 8 + sizeof(heap_node_t) * 3);

    heap_free(&pool, one);
    heap_free(&pool, two);

    void* four = heap_alloc(&pool, 8);

    assert_ptr_eq(four, (uint8_t*)mem + sizeof(heap_node_t));
}

int main(void)
{
    run_test(test_heap_create_no_pool_obj);
    run_test(test_heap_create_no_pool);
    run_test(test_heap_create_unaligned_pool);
    run_test(test_heap_create_obj_lt_ptr);
    run_test(test_heap_alloc_simple);
    run_test(test_heap_alloc_free_realloc);
    run_test(test_heap_free_combine_free_blocks);
    run_test(test_heap_free_memory_fragmentation);

    printf("Tests finished\n");

    return EXIT_SUCCESS;
}