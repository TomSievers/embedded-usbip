#include "mem_pool.h"
#include "test.h"
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

test(test_mem_pool_create_no_pool_obj)
{
    uint64_t some;

    assert_int_eq(init_mem_pool(sizeof(void*) / 2, &some, 0, NULL), -1);
    assert_int_eq(errno, ENOMEM);
}

test(test_mem_pool_create_no_pool)
{
    mem_pool_t some;

    assert_int_eq(init_mem_pool(sizeof(void*) / 2, NULL, 0, &some), -1);
    assert_int_eq(errno, ENOMEM);
}

test(test_mem_pool_create_too_small_pool)
{
    uint64_t some;

    assert_int_eq(init_mem_pool(sizeof(void*) / 2, &some, sizeof(void*) / 4, (mem_pool_t*)&some), -1);
    assert_int_eq(errno, ENOMEM);
}

test(test_mem_pool_create_obj_lt_ptr)
{
    mem_pool_t pool;
    uint64_t mem[2];

    assert_int_eq(init_mem_pool(sizeof(void*) / 2, &mem, 16, &pool), 0);
    assert_int_eq(pool.obj_size, sizeof(void*));
}

test(test_mem_pool_create_unaligned_size)
{
    mem_pool_t pool;
    uint64_t mem[2];

    assert_int_eq(init_mem_pool(sizeof(void*) / 2, &mem, 12, &pool), 0);
    assert_int_eq(pool.obj_size, sizeof(void*));
    assert_int_eq(pool.pool_size, sizeof(void*));
}

test(test_mem_alloc_free)
{
    mem_pool_t pool;
    uint64_t data[4];

    void* mem = data;

    assert_int_eq(init_mem_pool(sizeof(void*), mem, 32, &pool), 0);
    assert_int_eq(pool.obj_size, sizeof(void*));
    assert_int_eq(pool.pool_size, sizeof(void*) * 4);

    void* alloc = mem_pool_alloc(&pool);

    assert_ptr_eq(alloc, mem);

    mem_pool_free(&pool, alloc);

    alloc = mem_pool_alloc(&pool);

    assert_ptr_eq(alloc, mem);
}

test(test_mem_alloc_free_unordered)
{
    mem_pool_t pool;
    uint64_t data[4];

    void* mem = data;

    assert_int_eq(init_mem_pool(sizeof(void*), mem, 32, &pool), 0);
    assert_int_eq(pool.obj_size, sizeof(void*));
    assert_int_eq(pool.pool_size, sizeof(void*) * 4);

    void* one = mem_pool_alloc(&pool);
    void* two = mem_pool_alloc(&pool);

    assert_ptr_eq(one, mem);
    assert_ptr_eq(two, mem + sizeof(void*));

    mem_pool_free(&pool, one);

    one         = mem_pool_alloc(&pool);
    void* three = mem_pool_alloc(&pool);

    assert_ptr_eq(one, mem);
    assert_ptr_eq(three, mem + sizeof(void*) * 2);

    mem_pool_free(&pool, one);
    mem_pool_free(&pool, two);
    mem_pool_free(&pool, three);

    one   = mem_pool_alloc(&pool);
    two   = mem_pool_alloc(&pool);
    three = mem_pool_alloc(&pool);

    assert_ptr_eq(one, mem + sizeof(void*) * 2);
    assert_ptr_eq(two, mem + sizeof(void*));
    assert_ptr_eq(three, mem);
}

test(test_mem_alloc_oob)
{
    mem_pool_t pool;
    uint64_t data[4];

    void* mem = data;

    assert_int_eq(init_mem_pool(sizeof(void*), mem, 32, &pool), 0);
    assert_int_eq(pool.obj_size, sizeof(void*));
    assert_int_eq(pool.pool_size, sizeof(void*) * 4);

    void* one   = mem_pool_alloc(&pool);
    void* two   = mem_pool_alloc(&pool);
    void* three = mem_pool_alloc(&pool);
    void* four  = mem_pool_alloc(&pool);
    void* five  = mem_pool_alloc(&pool);

    assert_ptr_eq(one, mem);
    assert_ptr_eq(two, mem + sizeof(void*));
    assert_ptr_eq(three, mem + sizeof(void*) * 2);
    assert_ptr_eq(four, mem + sizeof(void*) * 3);
    assert_ptr_eq(five, NULL);
}

int main(void)
{
    run_test(test_mem_pool_create_no_pool_obj);
    run_test(test_mem_pool_create_no_pool);
    run_test(test_mem_pool_create_too_small_pool);
    run_test(test_mem_pool_create_obj_lt_ptr);
    run_test(test_mem_pool_create_unaligned_size);
    run_test(test_mem_alloc_free);
    run_test(test_mem_alloc_free_unordered);
    run_test(test_mem_alloc_oob);

    printf("Tests finished\n");

    return EXIT_SUCCESS;
}