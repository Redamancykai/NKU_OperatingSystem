// lab2 Challenge 2412677: simplified, single-CPU SLUB-style object allocator.
#include <slub.h>
#include <pmm.h>
#include <list.h>

#define SLAB_MAGIC 0x534C55424C414232ULL
#define LARGE_CLASS 8
struct slab {
    uint64_t magic;
    list_entry_t link;
    void *free;
    size_t request_pages;
    unsigned int class_id, inuse, capacity;
    uint64_t allocated[4];
};
#define HEADER_SIZE ((sizeof(struct slab) + 15) & ~15UL)
static list_entry_t caches[8];
static bool initialized;

void slub_init(void) {
    // Idempotent: a second init must not forget live slab pages.
    if (initialized) return;
    for (unsigned int i = 0; i < 8; i++) list_init(caches + i);
    initialized = 1;
}

static struct slab *new_slab(size_t request_pages, unsigned int class_id) {
    struct Page *page = alloc_pages(request_pages);
    if (!page) return NULL;
    // The framework's KADDR is commented out. Use its established direct map.
    struct slab *slab = (struct slab *)(page2pa(page) + va_pa_offset);
    slab->magic = SLAB_MAGIC;
    slab->request_pages = request_pages;
    slab->class_id = class_id;
    slab->inuse = 0;
    slab->capacity = 0;
    slab->free = NULL;
    for (unsigned int i = 0; i < 4; i++) slab->allocated[i] = 0;
    list_init(&slab->link);
    return slab;
}

void *kmalloc(size_t size) {
    if (!size || size > (size_t)PHYSICAL_MEMORY_END - DRAM_BASE - HEADER_SIZE)
        return NULL;
    slub_init();
    if (size > 2048) {
        size_t npages = (size + HEADER_SIZE + PGSIZE - 1) / PGSIZE;
        struct slab *slab = new_slab(npages, LARGE_CLASS);
        if (!slab) return NULL;
        slab->inuse = 1;
        return (char *)slab + HEADER_SIZE;
    }
    unsigned int class_id = 0;
    size_t object_size = 16;
    while (object_size < size) { object_size <<= 1; class_id++; }
    struct slab *slab = NULL;
    list_entry_t *head = caches + class_id;
    for (list_entry_t *le = list_next(head); le != head; le = list_next(le)) {
        struct slab *candidate = to_struct(le, struct slab, link);
        if (candidate->free) { slab = candidate; break; }
    }
    if (!slab) {
        slab = new_slab(1, class_id);
        if (!slab) return NULL;
        slab->capacity = (PGSIZE - HEADER_SIZE) / object_size;
        assert(slab->capacity <= 256);
        // Reverse insertion makes the first object the lowest-address slot.
        for (size_t i = slab->capacity; i > 0; i--) {
            void *slot = (char *)slab + HEADER_SIZE + (i - 1) * object_size;
            *(void **)slot = slab->free;
            slab->free = slot;
        }
        list_add(head, &slab->link);
    }
    void *result = slab->free;
    slab->free = *(void **)result;
    size_t index = ((char *)result - (char *)slab - HEADER_SIZE) / object_size;
    assert(!(slab->allocated[index / 64] & (1ULL << (index % 64))));
    slab->allocated[index / 64] |= 1ULL << (index % 64);
    slab->inuse++;
    return result;
}

void kfree(void *ptr) {
    if (!ptr) return;
    struct slab *slab = (struct slab *)((uintptr_t)ptr & ~(PGSIZE - 1UL));
    assert(slab->magic == SLAB_MAGIC && slab->inuse > 0);
    if (slab->class_id == LARGE_CLASS) {
        assert(ptr == (char *)slab + HEADER_SIZE && slab->inuse == 1);
        slab->inuse = 0;
    } else {
        assert(slab->class_id < 8);
        size_t object_size = 16UL << slab->class_id;
        uintptr_t offset = (uintptr_t)ptr - (uintptr_t)slab;
        assert(offset >= HEADER_SIZE);
        offset -= HEADER_SIZE;
        assert(offset % object_size == 0);
        size_t index = offset / object_size;
        assert(index < slab->capacity);
        uint64_t bit = 1ULL << (index % 64);
        assert(slab->allocated[index / 64] & bit);
        slab->allocated[index / 64] &= ~bit;
        *(void **)ptr = slab->free;
        slab->free = ptr;
        slab->inuse--;
        if (slab->inuse != 0) {
            // Prefer the slab just freed into, reusing its slot immediately.
            list_del(&slab->link);
            list_add(caches + slab->class_id, &slab->link);
            return;
        }
        list_del(&slab->link);
    }
    size_t request_pages = slab->request_pages;
    uintptr_t pa = (uintptr_t)slab - va_pa_offset;
    slab->magic = 0;
    free_pages(pa2page(pa), request_pages);
}
