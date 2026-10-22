// lab2 Challenge 2412677: byte-level contents and page-level leak checks.
#include <slub.h>
#include <pmm.h>
#include <stdio.h>

static void fill(void *p, size_t n, unsigned char value) {
    for (size_t i = 0; i < n; i++) ((unsigned char *)p)[i] = value;
}
static void verify(void *p, size_t n, unsigned char value) {
    for (size_t i = 0; i < n; i++) assert(((unsigned char *)p)[i] == value);
}

void slub_check(void) {
    slub_init();
    size_t before = nr_free_pages();
    assert(kmalloc(0) == NULL && kmalloc((size_t)-1) == NULL);
    assert(kmalloc(1UL << 31) == NULL);
    kfree(NULL);
    const size_t sizes[] = {1, 15, 16, 17, 31, 32, 33, 63, 64, 65,
        127, 128, 129, 255, 256, 257, 511, 512, 513, 1023, 1024,
        1025, 2047, 2048, 2049, 4095, 4096, 4097, 8193, 20000};
    void *objects[30];
    const size_t count = sizeof(sizes) / sizeof(sizes[0]);
    for (size_t i = 0; i < count; i++) {
        objects[i] = kmalloc(sizes[i]);
        assert(objects[i] != NULL && ((uintptr_t)objects[i] & 15) == 0);
        for (size_t j = 0; j < i; j++)
            assert((uintptr_t)objects[i] + sizes[i] <= (uintptr_t)objects[j] ||
                (uintptr_t)objects[j] + sizes[j] <= (uintptr_t)objects[i]);
        fill(objects[i], sizes[i], i + 1);
    }
    for (size_t i = count; i > 0; i--) {
        verify(objects[i - 1], sizes[i - 1], i);
        kfree(objects[i - 1]);
    }
    assert(nr_free_pages() == before);
    cprintf("[SLUB] size boundaries, alignment, contents, large objects: PASS\n");

    void *small[300];
    for (size_t i = 0; i < 300; i++) {
        small[i] = kmalloc(16);
        assert(small[i] != NULL);
        fill(small[i], 16, i % 251);
    }
    assert(nr_free_pages() < before - 1);
    void *reused = small[100];
    kfree(reused);
    small[100] = kmalloc(16);
    assert(small[100] == reused);
    fill(small[100], 16, 100);
    for (size_t i = 0; i < 300; i++) {
        verify(small[i], 16, i % 251);
        kfree(small[i]);
    }
    assert(nr_free_pages() == before);
    cprintf("[SLUB] multiple slabs, slot reuse, empty slab reclaim: PASS\n");
    for (size_t round = 0; round < 100; round++) {
        for (size_t i = 0; i < count; i++) {
            void *p = kmalloc(sizes[i]);
            assert(p != NULL);
            fill(p, sizes[i], round);
            verify(p, sizes[i], round);
            kfree(p);
        }
        assert(nr_free_pages() == before);
    }
    cprintf("[SLUB] 3000 mixed-size allocations, no page leaks: PASS\n");
}
