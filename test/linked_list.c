#include "test.h"
#include <errno.h>
#include <linked_list.h>
#include <memory.h>
#include <stdint.h>
#include <stdlib.h>

test(test_linked_list_create_no_alloc)
{
    linked_list_t list;

    assert_int_eq(linked_list_init(NULL, free, &list), -1);
    assert_int_eq(errno, ENOMEM);
}

test(test_linked_list_create_no_free)
{
    linked_list_t list;

    assert_int_eq(linked_list_init(malloc, NULL, &list), -1);
    assert_int_eq(errno, ENOMEM);
}

test(test_linked_list_create_no_fn)
{
    linked_list_t list;

    assert_int_eq(linked_list_init(NULL, NULL, &list), -1);
    assert_int_eq(errno, ENOMEM);
}

test(test_linked_list_create_no_list)
{
    linked_list_t list;

    assert_int_eq(linked_list_init(malloc, free, NULL), -1);
    assert_int_eq(errno, ENOMEM);
}

test(test_linked_list_create_multiple)
{
    linked_list_t list;

    assert_int_eq(linked_list_init(malloc, free, &list), 0);

    int a = 0;
    int b = 1;
    int c = 2;

    assert_int_eq(linked_list_push(&list, &a), 0);
    assert_int_eq(linked_list_push(&list, &b), 0);
    assert_int_eq(linked_list_push(&list, &c), 0);

    assert_ptr_eq(linked_list_get(&list, 0), &a);
    assert_ptr_eq(linked_list_get(&list, 1), &b);
    assert_ptr_eq(linked_list_get(&list, 2), &c);

    assert_ptr_eq(linked_list_rem(&list, 1), &b);

    assert_ptr_eq(linked_list_get(&list, 0), &a);
    assert_ptr_eq(linked_list_get(&list, 1), &c);

    assert_ptr_eq(linked_list_rem(&list, 0), &a);

    assert_ptr_eq(linked_list_get(&list, 0), &c);

    assert_ptr_eq(linked_list_rem(&list, 0), &c);

    assert_ptr_eq(linked_list_get(&list, 0), NULL);
}

int iter_test(void* data, size_t i, void* ctx)
{
    int* value = data;
    switch (i)
    {
    case 0:
        assert_int_eq(*value, 0);
        break;
    case 1:
        assert_int_eq(*value, 1);
        break;
    case 2:
        assert_int_eq(*value, 2);
        break;

    default:
        break;
    }

    return 0;
}

test(test_linked_list_iterate)
{
    linked_list_t list;

    assert_int_eq(linked_list_init(malloc, free, &list), 0);

    int a = 0;
    int b = 1;
    int c = 2;

    assert_int_eq(linked_list_push(&list, &a), 0);
    assert_int_eq(linked_list_push(&list, &b), 0);
    assert_int_eq(linked_list_push(&list, &c), 0);

    linked_list_iter(&list, iter_test, NULL);

    assert_ptr_eq(linked_list_rem(&list, 0), &a);
    assert_ptr_eq(linked_list_rem(&list, 0), &b);
    assert_ptr_eq(linked_list_rem(&list, 0), &c);
}

int iter_test_invalidate(void* data, size_t i, void* ctx)
{
    linked_list_t* list = ctx;
    int* value          = data;
    switch (i)
    {
    case 0:
        assert_int_eq(*value, 0);
        break;
    case 1:
        assert_int_eq(*value, 1);
        linked_list_rem(list, i);
        return 1;
    case 2:
        assert_int_eq(*value, 2);
        break;

    default:
        break;
    }

    return 0;
}

test(test_linked_list_iterate_invalidate)
{
    linked_list_t list;

    assert_int_eq(linked_list_init(malloc, free, &list), 0);

    int a = 0;
    int b = 1;
    int c = 2;

    assert_int_eq(linked_list_push(&list, &a), 0);
    assert_int_eq(linked_list_push(&list, &b), 0);
    assert_int_eq(linked_list_push(&list, &c), 0);

    linked_list_iter(&list, iter_test_invalidate, &list);

    assert_ptr_eq(linked_list_rem(&list, 0), &a);
    assert_ptr_eq(linked_list_rem(&list, 0), &c);
}

int main(void)
{
    run_test(test_linked_list_create_no_alloc);
    run_test(test_linked_list_create_no_free);
    run_test(test_linked_list_create_no_fn);
    run_test(test_linked_list_create_no_list);
    run_test(test_linked_list_create_multiple);
    run_test(test_linked_list_iterate);
    run_test(test_linked_list_iterate_invalidate);

    printf("Tests finished\n");

    return EXIT_SUCCESS;
}