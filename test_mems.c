#include "mems.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void assert_virtual(void *actual, uintptr_t expected)
{
    assert((uintptr_t)actual == expected);
}

static void test_implicit_init_and_zero_alloc(void)
{
    void *allocation;

    mems_finish();
    assert(mems_malloc(0) == NULL);

    allocation = mems_malloc(16);
    assert_virtual(allocation, 1000U);
    assert(mems_get(allocation) != NULL);

    mems_finish();
}

static void test_translation_and_write_access(void)
{
    void *allocation;
    unsigned char *physical;

    mems_init();

    allocation = mems_malloc(1000);
    assert_virtual(allocation, 1000U);

    physical = (unsigned char *)mems_get(allocation);
    assert(physical != NULL);
    memset(physical, 0xAB, 1000);

    assert(physical[0] == 0xAB);
    assert(physical[999] == 0xAB);
    assert(mems_get((void *)((uintptr_t)allocation + 999U)) == physical + 999);
    assert(mems_get((void *)((uintptr_t)allocation + 1000U)) == NULL);
    assert(mems_get((void *)999U) == NULL);

    mems_finish();
}

static void test_first_fit_split_and_exact_reuse(void)
{
    void *a;
    void *b;
    void *c;
    void *d;

    mems_init();

    a = mems_malloc(1000);
    b = mems_malloc(2000);
    assert_virtual(a, 1000U);
    assert_virtual(b, 2000U);

    mems_free(a);
    c = mems_malloc(500);
    d = mems_malloc(500);

    assert_virtual(c, 1000U);
    assert_virtual(d, 1500U);

    mems_free(b);
    mems_free(c);
    mems_free(d);

    c = mems_malloc(PAGE_SIZE);
    assert_virtual(c, 1000U);
    assert(mems_get((void *)((uintptr_t)c + PAGE_SIZE - 1U)) != NULL);
    assert(mems_get((void *)((uintptr_t)c + PAGE_SIZE)) == NULL);

    mems_finish();
}

static void test_coalescing_ignores_invalid_and_interior_frees(void)
{
    void *a;
    void *b;
    void *c;
    void *large;

    mems_init();

    a = mems_malloc(700);
    b = mems_malloc(800);
    c = mems_malloc(900);
    assert_virtual(a, 1000U);
    assert_virtual(b, 1700U);
    assert_virtual(c, 2500U);

    mems_free((void *)((uintptr_t)b + 1U));
    assert(mems_get(b) != NULL);

    mems_free((void *)1234567U);
    assert(mems_get(a) != NULL);
    assert(mems_get(b) != NULL);
    assert(mems_get(c) != NULL);

    mems_free(a);
    mems_free(b);
    mems_free(c);

    large = mems_malloc(3000);
    assert_virtual(large, 1000U);

    mems_finish();
}

static void test_large_allocation_and_lifecycle_reset(void)
{
    void *large;
    void *again;
    unsigned char *physical;

    mems_init();

    large = mems_malloc(9000);
    assert(large != NULL);
    assert_virtual(large, 1000U);
    assert(mems_get((void *)((uintptr_t)large + 8999U)) != NULL);
    assert(mems_get((void *)((uintptr_t)large + 9000U)) == NULL);

    physical = (unsigned char *)mems_get(large);
    assert(physical != NULL);
    physical[0] = 0x11;
    physical[8999] = 0x22;
    assert(physical[0] == 0x11);
    assert(physical[8999] == 0x22);

    mems_finish();
    mems_finish();

    mems_init();
    again = mems_malloc(PAGE_SIZE);
    assert_virtual(again, 1000U);
    mems_finish();
}

static void test_stats_smoke(void)
{
    void *a;
    void *b;

    mems_init();
    a = mems_malloc(256);
    b = mems_malloc(512);
    assert(a != NULL);
    assert(b != NULL);

    mems_free(a);
    mems_print_stats();

    mems_finish();
}

int main(void)
{
    test_implicit_init_and_zero_alloc();
    test_translation_and_write_access();
    test_first_fit_split_and_exact_reuse();
    test_coalescing_ignores_invalid_and_interior_frees();
    test_large_allocation_and_lifecycle_reset();
    test_stats_smoke();

    puts("All MeMS tests passed.");
    return 0;
}
