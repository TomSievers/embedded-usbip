#include "mem_pool.h"
#include <check.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

START_TEST(test_mem_pool_create_no_pool_obj)
{
    uint64_t some;

    ck_assert_int_eq(init_mem_pool(sizeof(void*) / 2, &some, 0, NULL), -1);
    ck_assert_int_eq(errno, ENOMEM);
}
END_TEST

START_TEST(test_mem_pool_create_no_pool)
{
    mem_pool_t some;

    ck_assert_int_eq(init_mem_pool(sizeof(void*) / 2, NULL, 0, &some), -1);
    ck_assert_int_eq(errno, ENOMEM);
}
END_TEST

START_TEST(test_mem_pool_create_too_small_pool)
{
    uint64_t some;

    ck_assert_int_eq(init_mem_pool(sizeof(void*) / 2, &some, sizeof(void*) / 4, (mem_pool_t*)&some), -1);
    ck_assert_int_eq(errno, ENOMEM);
}
END_TEST

START_TEST(test_mem_pool_create_obj_lt_ptr)
{
    mem_pool_t pool;
    uint64_t mem[2];

    ck_assert_int_eq(init_mem_pool(sizeof(void*) / 2, &mem, 16, &pool), 0);
    ck_assert_int_eq(pool.obj_size, sizeof(void*));
}
END_TEST

START_TEST(test_mem_pool_create_unaligned_size)
{
    mem_pool_t pool;
    uint64_t mem[2];

    ck_assert_int_eq(init_mem_pool(sizeof(void*) / 2, &mem, 12, &pool), 0);
    ck_assert_int_eq(pool.obj_size, sizeof(void*));
    ck_assert_int_eq(pool.pool_size, sizeof(void*));
}
END_TEST

START_TEST(test_mem_alloc_free)
{
    mem_pool_t pool;
    uint64_t data[4];

    void* mem = data;

    ck_assert_int_eq(init_mem_pool(sizeof(void*), mem, 32, &pool), 0);
    ck_assert_int_eq(pool.obj_size, sizeof(void*));
    ck_assert_int_eq(pool.pool_size, sizeof(void*) * 4);

    void* alloc = mem_pool_alloc(&pool);

    ck_assert_ptr_eq(alloc, mem);

    mem_pool_free(&pool, alloc);

    alloc = mem_pool_alloc(&pool);

    ck_assert_ptr_eq(alloc, mem);
}
END_TEST

START_TEST(test_mem_alloc_free_unordered)
{
    mem_pool_t pool;
    uint64_t data[4];

    void* mem = data;

    ck_assert_int_eq(init_mem_pool(sizeof(void*), mem, 32, &pool), 0);
    ck_assert_int_eq(pool.obj_size, sizeof(void*));
    ck_assert_int_eq(pool.pool_size, sizeof(void*) * 4);

    void* one = mem_pool_alloc(&pool);
    void* two = mem_pool_alloc(&pool);

    ck_assert_ptr_eq(one, mem);
    ck_assert_ptr_eq(two, mem + sizeof(void*));

    mem_pool_free(&pool, one);

    one         = mem_pool_alloc(&pool);
    void* three = mem_pool_alloc(&pool);

    ck_assert_ptr_eq(one, mem);
    ck_assert_ptr_eq(three, mem + sizeof(void*) * 2);

    mem_pool_free(&pool, one);
    mem_pool_free(&pool, two);
    mem_pool_free(&pool, three);

    one   = mem_pool_alloc(&pool);
    two   = mem_pool_alloc(&pool);
    three = mem_pool_alloc(&pool);

    ck_assert_ptr_eq(one, mem + sizeof(void*) * 2);
    ck_assert_ptr_eq(two, mem + sizeof(void*));
    ck_assert_ptr_eq(three, mem);
}
END_TEST

START_TEST(test_mem_alloc_oob)
{
    mem_pool_t pool;
    uint64_t data[4];

    void* mem = data;

    ck_assert_int_eq(init_mem_pool(sizeof(void*), mem, 32, &pool), 0);
    ck_assert_int_eq(pool.obj_size, sizeof(void*));
    ck_assert_int_eq(pool.pool_size, sizeof(void*) * 4);

    void* one   = mem_pool_alloc(&pool);
    void* two   = mem_pool_alloc(&pool);
    void* three = mem_pool_alloc(&pool);
    void* four  = mem_pool_alloc(&pool);
    void* five  = mem_pool_alloc(&pool);

    ck_assert_ptr_eq(one, mem);
    ck_assert_ptr_eq(two, mem + sizeof(void*));
    ck_assert_ptr_eq(three, mem + sizeof(void*) * 2);
    ck_assert_ptr_eq(four, mem + sizeof(void*) * 3);
    ck_assert_ptr_eq(five, NULL);
}
END_TEST

Suite* mem_pool_suite(void)
{
    Suite* s;
    TCase* tc_core;
    TCase* tc_limits;

    s       = suite_create("mem pool");

    /* Core test case */
    tc_core = tcase_create("core");

    tcase_add_test(tc_core, test_mem_pool_create_no_pool_obj);
    tcase_add_test(tc_core, test_mem_pool_create_no_pool);
    tcase_add_test(tc_core, test_mem_pool_create_too_small_pool);
    tcase_add_test(tc_core, test_mem_pool_create_obj_lt_ptr);
    tcase_add_test(tc_core, test_mem_pool_create_unaligned_size);
    tcase_add_test(tc_core, test_mem_alloc_free);
    tcase_add_test(tc_core, test_mem_alloc_free_unordered);
    tcase_add_test(tc_core, test_mem_alloc_oob);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s;
    SRunner* sr;

    s  = mem_pool_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}