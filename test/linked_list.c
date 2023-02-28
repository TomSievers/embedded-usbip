#include "linked_list.h"
#include <check.h>
#include <errno.h>
#include <memory.h>
#include <stdint.h>
#include <stdlib.h>

START_TEST(test_linked_list_create_no_alloc)
{
    linked_list_t list;

    ck_assert_int_eq(linked_list_init(NULL, free, &list), -1);
    ck_assert_int_eq(errno, ENOMEM);
}
END_TEST

START_TEST(test_linked_list_create_no_free)
{
    linked_list_t list;

    ck_assert_int_eq(linked_list_init(malloc, NULL, &list), -1);
    ck_assert_int_eq(errno, ENOMEM);
}
END_TEST

START_TEST(test_linked_list_create_no_fn)
{
    linked_list_t list;

    ck_assert_int_eq(linked_list_init(NULL, NULL, &list), -1);
    ck_assert_int_eq(errno, ENOMEM);
}
END_TEST

START_TEST(test_linked_list_create_no_list)
{
    linked_list_t list;

    ck_assert_int_eq(linked_list_init(malloc, free, NULL), -1);
    ck_assert_int_eq(errno, ENOMEM);
}
END_TEST

START_TEST(test_linked_list_create_multiple)
{
    linked_list_t list;

    ck_assert_int_eq(linked_list_init(malloc, free, &list), 0);

    int a = 0;
    int b = 1;
    int c = 2;

    ck_assert_int_eq(linked_list_push(&list, &a), 0);
    ck_assert_int_eq(linked_list_push(&list, &b), 0);
    ck_assert_int_eq(linked_list_push(&list, &c), 0);

    ck_assert_ptr_eq(linked_list_get(&list, 0), &a);
    ck_assert_ptr_eq(linked_list_get(&list, 1), &b);
    ck_assert_ptr_eq(linked_list_get(&list, 2), &c);

    ck_assert_ptr_eq(linked_list_rem(&list, 1), &b);

    ck_assert_ptr_eq(linked_list_get(&list, 0), &a);
    ck_assert_ptr_eq(linked_list_get(&list, 1), &c);

    ck_assert_ptr_eq(linked_list_rem(&list, 0), &a);

    ck_assert_ptr_eq(linked_list_get(&list, 0), &c);

    ck_assert_ptr_eq(linked_list_rem(&list, 0), &c);

    ck_assert_ptr_eq(linked_list_get(&list, 0), NULL);
}
END_TEST

int iter_test(void* data, size_t i, void* ctx)
{
    int* value = data;
    switch (i)
    {
    case 0:
        ck_assert_int_eq(*value, 0);
        break;
    case 1:
        ck_assert_int_eq(*value, 1);
        break;
    case 2:
        ck_assert_int_eq(*value, 2);
        break;

    default:
        break;
    }

    return 0;
}

START_TEST(test_linked_list_iterate)
{
    linked_list_t list;

    ck_assert_int_eq(linked_list_init(malloc, free, &list), 0);

    int a = 0;
    int b = 1;
    int c = 2;

    ck_assert_int_eq(linked_list_push(&list, &a), 0);
    ck_assert_int_eq(linked_list_push(&list, &b), 0);
    ck_assert_int_eq(linked_list_push(&list, &c), 0);

    linked_list_iter(&list, iter_test, NULL);

    ck_assert_ptr_eq(linked_list_rem(&list, 0), &a);
    ck_assert_ptr_eq(linked_list_rem(&list, 0), &b);
    ck_assert_ptr_eq(linked_list_rem(&list, 0), &c);
}
END_TEST

int iter_test_invalidate(void* data, size_t i, void* ctx)
{
    linked_list_t* list = ctx;
    int* value          = data;
    switch (i)
    {
    case 0:
        ck_assert_int_eq(*value, 0);
        break;
    case 1:
        ck_assert_int_eq(*value, 1);
        linked_list_rem(list, i);
        return 1;
    case 2:
        ck_assert_int_eq(*value, 2);
        break;

    default:
        break;
    }

    return 0;
}

START_TEST(test_linked_list_iterate_invalidate)
{
    linked_list_t list;

    ck_assert_int_eq(linked_list_init(malloc, free, &list), 0);

    int a = 0;
    int b = 1;
    int c = 2;

    ck_assert_int_eq(linked_list_push(&list, &a), 0);
    ck_assert_int_eq(linked_list_push(&list, &b), 0);
    ck_assert_int_eq(linked_list_push(&list, &c), 0);

    linked_list_iter(&list, iter_test_invalidate, &list);

    ck_assert_ptr_eq(linked_list_rem(&list, 0), &a);
    ck_assert_ptr_eq(linked_list_rem(&list, 0), &c);
}
END_TEST

Suite* linked_list_suite(void)
{
    Suite* s;
    TCase* tc_core;
    TCase* tc_limits;

    s       = suite_create("linked list");

    /* Core test case */
    tc_core = tcase_create("core");

    tcase_add_test(tc_core, test_linked_list_create_no_alloc);
    tcase_add_test(tc_core, test_linked_list_create_no_free);
    tcase_add_test(tc_core, test_linked_list_create_no_fn);
    tcase_add_test(tc_core, test_linked_list_create_no_list);
    tcase_add_test(tc_core, test_linked_list_create_multiple);
    tcase_add_test(tc_core, test_linked_list_iterate);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s;
    SRunner* sr;

    s  = linked_list_suite();
    sr = srunner_create(s);

    srunner_set_fork_status(sr, CK_NOFORK);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}