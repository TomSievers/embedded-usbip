#pragma once
#include <stdio.h>
#include <stdlib.h>

#define test(testname) void testname##_fn()
#define run_test(testname)                     \
    printf("\tRunning test: " #testname "\n"); \
    testname##_fn()
#define assert_int_eq(a, b) _assert_int_eq(a, b, #a, #b, __FILE__, __LINE__)
#define assert_ptr_eq(a, b) _assert_ptr_eq(a, b, #a, #b, __FILE__, __LINE__)

void _assert_int_eq(int a, int b, const char* a_name, const char* b_name, const char* FILE, int LINE)
{
    if (a != b)
    {
        printf("Assert failed '%s == %s' %d != %d at %s:%d", a_name, b_name, a, b, FILE, LINE);
        exit(1);
    }
}

void _assert_ptr_eq(void* a, void* b, const char* a_name, const char* b_name, const char* FILE, int LINE)
{
    if (a != b)
    {
        printf("Assert failed '%s == %s' %p != %p at %s:%d", a_name, b_name, a, b, FILE, LINE);
        exit(1);
    }
}