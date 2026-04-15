#include "mems.h"

#include <stdint.h>
#include <stdio.h>

int main(void)
{
    void *addresses[10];
    int *value;
    size_t i;

    mems_init();

    for (i = 0; i < 10; i++) {
        addresses[i] = mems_malloc(1000);
        printf("Virtual address: %zu\n", (size_t)(uintptr_t)addresses[i]);
    }

    value = (int *)mems_get(addresses[0]);
    if (value != NULL) {
        *value = 200;
        printf("Physical address: %p\n", (void *)value);
        printf("Value written: %d\n", *value);
    }

    mems_print_stats();

    mems_free(addresses[2]);
    mems_free(addresses[4]);
    printf("\nAfter freeing two allocations:\n");
    mems_print_stats();

    mems_finish();
    return 0;
}
