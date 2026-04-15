#ifndef MEMS_H
#define MEMS_H

#include <stddef.h>

/*
 * The assignment convention uses a 4096-byte logical page so output is stable
 * across machines. mmap itself is still called with the host OS page size when
 * the host requires a larger mapping granularity.
 */
#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#if PAGE_SIZE == 0
#error "PAGE_SIZE must be greater than zero"
#endif

void mems_init(void);
void mems_finish(void);
void *mems_malloc(size_t size);
void mems_print_stats(void);
void *mems_get(void *v_ptr);
void mems_free(void *v_ptr);

#endif
