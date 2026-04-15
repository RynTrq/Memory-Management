/*
 * MeMS - a small custom memory-management system.
 *
 * The public API returns simulated MeMS virtual addresses starting at 1000.
 * These addresses are not process pointers. Call mems_get() to translate a
 * MeMS address into the real mmap-backed address before reading or writing.
 */

#include "mems.h"

#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

#define MEMS_VIRTUAL_BASE ((uintptr_t)1000U)
#define MEMS_METADATA_ALIGNMENT (_Alignof(max_align_t))

typedef enum SegmentType {
    SEGMENT_HOLE = 0,
    SEGMENT_PROCESS = 1
} SegmentType;

typedef struct Segment {
    size_t offset;
    size_t size;
    SegmentType type;
    struct Segment *prev;
    struct Segment *next;
} Segment;

typedef struct MainNode {
    void *physical_base;
    uintptr_t virtual_base;
    size_t logical_size;
    size_t mapped_size;
    Segment *segments;
    struct MainNode *prev;
    struct MainNode *next;
} MainNode;

typedef struct MetadataChunk {
    struct MetadataChunk *next;
    size_t mapped_size;
    size_t used;
} MetadataChunk;

static MainNode *g_main_head = NULL;
static MainNode *g_main_tail = NULL;
static MetadataChunk *g_metadata_chunks = NULL;
static uintptr_t g_next_virtual_base = MEMS_VIRTUAL_BASE;
static size_t g_os_page_size = PAGE_SIZE;
static int g_initialized = 0;

static int add_overflows_size(size_t a, size_t b)
{
    return a > SIZE_MAX - b;
}

static int add_overflows_uintptr(uintptr_t a, size_t b)
{
#if SIZE_MAX > UINTPTR_MAX
    if (b > (size_t)UINTPTR_MAX) {
        return 1;
    }
#endif

    return a > UINTPTR_MAX - (uintptr_t)b;
}

static size_t round_up_size(size_t value, size_t alignment)
{
    size_t remainder;

    if (alignment == 0) {
        return 0;
    }

    remainder = value % alignment;
    if (remainder == 0) {
        return value;
    }

    if (add_overflows_size(value, alignment - remainder)) {
        return 0;
    }

    return value + alignment - remainder;
}

static size_t detect_os_page_size(void)
{
    long detected = sysconf(_SC_PAGESIZE);

    if (detected <= 0) {
        return PAGE_SIZE;
    }

    return (size_t)detected;
}

static void reset_globals(void)
{
    g_main_head = NULL;
    g_main_tail = NULL;
    g_metadata_chunks = NULL;
    g_next_virtual_base = MEMS_VIRTUAL_BASE;
    g_os_page_size = PAGE_SIZE;
    g_initialized = 0;
}

void mems_finish(void);

void mems_init(void)
{
    if (g_initialized) {
        mems_finish();
    }

    g_main_head = NULL;
    g_main_tail = NULL;
    g_metadata_chunks = NULL;
    g_next_virtual_base = MEMS_VIRTUAL_BASE;
    g_os_page_size = detect_os_page_size();
    g_initialized = 1;
}

static int ensure_initialized(void)
{
    if (!g_initialized) {
        mems_init();
    }

    return g_initialized;
}

static void *map_bytes(size_t size)
{
    void *mapping;

    if (size == 0) {
        return NULL;
    }

    mapping = mmap(NULL, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapping == MAP_FAILED) {
        return NULL;
    }

    return mapping;
}

static void *metadata_alloc(size_t size)
{
    MetadataChunk *chunk;
    size_t aligned_size;
    size_t header_size;

    aligned_size = round_up_size(size, MEMS_METADATA_ALIGNMENT);
    if (aligned_size == 0) {
        return NULL;
    }

    header_size = round_up_size(sizeof(MetadataChunk), MEMS_METADATA_ALIGNMENT);
    if (header_size == 0) {
        return NULL;
    }

    for (chunk = g_metadata_chunks; chunk != NULL; chunk = chunk->next) {
        if (chunk->used <= chunk->mapped_size &&
            chunk->mapped_size - chunk->used >= aligned_size) {
            void *result = (char *)chunk + chunk->used;
            chunk->used += aligned_size;
            return result;
        }
    }

    {
        size_t requested;
        size_t mapped_size;
        MetadataChunk *new_chunk;

        if (add_overflows_size(header_size, aligned_size)) {
            return NULL;
        }

        requested = header_size + aligned_size;
        if (requested < PAGE_SIZE) {
            requested = PAGE_SIZE;
        }

        mapped_size = round_up_size(requested, g_os_page_size);
        if (mapped_size == 0) {
            return NULL;
        }

        new_chunk = (MetadataChunk *)map_bytes(mapped_size);
        if (new_chunk == NULL) {
            return NULL;
        }

        new_chunk->next = g_metadata_chunks;
        new_chunk->mapped_size = mapped_size;
        new_chunk->used = header_size + aligned_size;
        g_metadata_chunks = new_chunk;

        return (char *)new_chunk + header_size;
    }
}

static Segment *new_segment(size_t offset, size_t size, SegmentType type)
{
    Segment *segment = (Segment *)metadata_alloc(sizeof(Segment));

    if (segment == NULL || size == 0) {
        return NULL;
    }

    segment->offset = offset;
    segment->size = size;
    segment->type = type;
    segment->prev = NULL;
    segment->next = NULL;
    return segment;
}

static MainNode *new_main_node(void *physical_base,
                               uintptr_t virtual_base,
                               size_t logical_size,
                               size_t mapped_size,
                               Segment *segments)
{
    MainNode *node = (MainNode *)metadata_alloc(sizeof(MainNode));

    if (node == NULL) {
        return NULL;
    }

    node->physical_base = physical_base;
    node->virtual_base = virtual_base;
    node->logical_size = logical_size;
    node->mapped_size = mapped_size;
    node->segments = segments;
    node->prev = NULL;
    node->next = NULL;
    return node;
}

static void append_main_node(MainNode *node)
{
    if (g_main_tail == NULL) {
        g_main_head = node;
        g_main_tail = node;
        return;
    }

    node->prev = g_main_tail;
    g_main_tail->next = node;
    g_main_tail = node;
}

static uintptr_t segment_virtual_start(const MainNode *node,
                                       const Segment *segment)
{
    return node->virtual_base + segment->offset;
}

static uintptr_t segment_virtual_end(const MainNode *node,
                                     const Segment *segment)
{
    return segment_virtual_start(node, segment) + segment->size - 1U;
}

static Segment *find_first_fit(size_t size, MainNode **owner)
{
    MainNode *node;

    for (node = g_main_head; node != NULL; node = node->next) {
        Segment *segment;

        for (segment = node->segments; segment != NULL; segment = segment->next) {
            if (segment->type == SEGMENT_HOLE && segment->size >= size) {
                if (owner != NULL) {
                    *owner = node;
                }
                return segment;
            }
        }
    }

    return NULL;
}

static void *allocate_from_segment(MainNode *node, Segment *segment, size_t size)
{
    uintptr_t virtual_address;

    if (add_overflows_size(segment->offset, size)) {
        return NULL;
    }

    if (segment->size == size) {
        segment->type = SEGMENT_PROCESS;
    } else {
        Segment *hole = new_segment(segment->offset + size,
                                    segment->size - size,
                                    SEGMENT_HOLE);
        if (hole == NULL) {
            return NULL;
        }

        hole->next = segment->next;
        hole->prev = segment;
        if (hole->next != NULL) {
            hole->next->prev = hole;
        }

        segment->next = hole;
        segment->size = size;
        segment->type = SEGMENT_PROCESS;
    }

    virtual_address = segment_virtual_start(node, segment);
    return (void *)virtual_address;
}

static void *allocate_new_main_node(size_t size)
{
    size_t logical_size;
    size_t mapped_size;
    void *physical_base;
    Segment *process;
    Segment *hole = NULL;
    MainNode *node;
    uintptr_t virtual_base;

    logical_size = round_up_size(size, PAGE_SIZE);
    if (logical_size == 0) {
        return NULL;
    }

    mapped_size = round_up_size(logical_size, g_os_page_size);
    if (mapped_size == 0) {
        return NULL;
    }

    if (add_overflows_uintptr(g_next_virtual_base, logical_size)) {
        return NULL;
    }

    process = new_segment(0, size, SEGMENT_PROCESS);
    if (process == NULL) {
        return NULL;
    }

    if (logical_size > size) {
        hole = new_segment(size, logical_size - size, SEGMENT_HOLE);
        if (hole == NULL) {
            return NULL;
        }
        process->next = hole;
        hole->prev = process;
    }

    physical_base = map_bytes(mapped_size);
    if (physical_base == NULL) {
        return NULL;
    }

    virtual_base = g_next_virtual_base;
    node = new_main_node(physical_base, virtual_base, logical_size, mapped_size,
                         process);
    if (node == NULL) {
        munmap(physical_base, mapped_size);
        return NULL;
    }

    append_main_node(node);
    g_next_virtual_base += logical_size;
    return (void *)virtual_base;
}

void *mems_malloc(size_t size)
{
    Segment *segment;
    MainNode *node = NULL;

    if (size == 0 || !ensure_initialized()) {
        return NULL;
    }

    segment = find_first_fit(size, &node);
    if (segment != NULL && node != NULL) {
        return allocate_from_segment(node, segment, size);
    }

    return allocate_new_main_node(size);
}

static MainNode *find_node_containing(uintptr_t virtual_address)
{
    MainNode *node;

    for (node = g_main_head; node != NULL; node = node->next) {
        uintptr_t end;

        if (add_overflows_uintptr(node->virtual_base, node->logical_size)) {
            continue;
        }

        end = node->virtual_base + node->logical_size;
        if (virtual_address >= node->virtual_base && virtual_address < end) {
            return node;
        }
    }

    return NULL;
}

static Segment *find_segment_containing(const MainNode *node, size_t offset)
{
    Segment *segment;

    for (segment = node->segments; segment != NULL; segment = segment->next) {
        size_t end;

        if (add_overflows_size(segment->offset, segment->size)) {
            continue;
        }

        end = segment->offset + segment->size;
        if (offset >= segment->offset && offset < end) {
            return segment;
        }
    }

    return NULL;
}

static Segment *find_process_start(MainNode **owner, uintptr_t virtual_address)
{
    MainNode *node;
    size_t offset;
    Segment *segment;

    node = find_node_containing(virtual_address);
    if (node == NULL) {
        return NULL;
    }

    offset = (size_t)(virtual_address - node->virtual_base);
    segment = find_segment_containing(node, offset);
    if (segment == NULL || segment->type != SEGMENT_PROCESS) {
        return NULL;
    }

    if (segment->offset != offset) {
        return NULL;
    }

    if (owner != NULL) {
        *owner = node;
    }

    return segment;
}

void *mems_get(void *v_ptr)
{
    uintptr_t virtual_address;
    MainNode *node;
    size_t offset;
    Segment *segment;

    if (v_ptr == NULL || !ensure_initialized()) {
        return NULL;
    }

    virtual_address = (uintptr_t)v_ptr;
    node = find_node_containing(virtual_address);
    if (node == NULL) {
        return NULL;
    }

    offset = (size_t)(virtual_address - node->virtual_base);
    segment = find_segment_containing(node, offset);
    if (segment == NULL || segment->type != SEGMENT_PROCESS) {
        return NULL;
    }

    return (char *)node->physical_base + offset;
}

static void coalesce_with_next(Segment *segment)
{
    Segment *next = segment->next;

    if (next == NULL || next->type != SEGMENT_HOLE) {
        return;
    }

    if (add_overflows_size(segment->size, next->size)) {
        return;
    }

    segment->size += next->size;
    segment->next = next->next;
    if (segment->next != NULL) {
        segment->next->prev = segment;
    }
}

void mems_free(void *v_ptr)
{
    uintptr_t virtual_address;
    MainNode *node = NULL;
    Segment *segment;

    if (v_ptr == NULL || !ensure_initialized()) {
        return;
    }

    virtual_address = (uintptr_t)v_ptr;
    segment = find_process_start(&node, virtual_address);
    if (segment == NULL || node == NULL) {
        return;
    }

    segment->type = SEGMENT_HOLE;

    if (segment->prev != NULL && segment->prev->type == SEGMENT_HOLE) {
        segment = segment->prev;
        coalesce_with_next(segment);
    }

    coalesce_with_next(segment);
}

static size_t count_segments(const MainNode *node)
{
    size_t count = 0;
    const Segment *segment;

    for (segment = node->segments; segment != NULL; segment = segment->next) {
        count++;
    }

    return count;
}

void mems_print_stats(void)
{
    MainNode *node;
    size_t pages_used = 0;
    size_t space_unused = 0;
    size_t main_chain_length = 0;

    ensure_initialized();

    printf("MeMS SYSTEM STATS\n");

    for (node = g_main_head; node != NULL; node = node->next) {
        Segment *segment;

        main_chain_length++;
        pages_used += node->logical_size / PAGE_SIZE;

        printf("MAIN[%zu:%zu] -> ",
               (size_t)node->virtual_base,
               (size_t)(node->virtual_base + node->logical_size - 1U));

        for (segment = node->segments; segment != NULL; segment = segment->next) {
            const char *label = segment->type == SEGMENT_PROCESS ? "PROCESS" : "HOLE";

            if (segment->type == SEGMENT_HOLE) {
                space_unused += segment->size;
            }

            printf("%s[%zu:%zu] <-> ",
                   label,
                   (size_t)segment_virtual_start(node, segment),
                   (size_t)segment_virtual_end(node, segment));
        }

        printf("NULL\n");
    }

    printf("Pages used: %zu\n", pages_used);
    printf("Space unused: %zu\n", space_unused);
    printf("Main Chain Length: %zu\n", main_chain_length);
    printf("Sub-chain Length array: [");

    for (node = g_main_head; node != NULL; node = node->next) {
        printf("%zu", count_segments(node));
        if (node->next != NULL) {
            printf(", ");
        }
    }

    printf("]\n");
}

void mems_finish(void)
{
    MainNode *node;
    MetadataChunk *chunk;

    if (!g_initialized) {
        reset_globals();
        return;
    }

    node = g_main_head;
    while (node != NULL) {
        MainNode *next = node->next;
        if (node->physical_base != NULL && node->mapped_size > 0) {
            munmap(node->physical_base, node->mapped_size);
        }
        node = next;
    }

    chunk = g_metadata_chunks;
    while (chunk != NULL) {
        MetadataChunk *next = chunk->next;
        munmap(chunk, chunk->mapped_size);
        chunk = next;
    }

    reset_globals();
}
