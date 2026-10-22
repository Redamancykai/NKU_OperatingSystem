// lab2 function2-3 2412677: tests use only the public manager interface.
#include <buddy_pmm.h>
#include <stdio.h>

static struct Page fixture[128];
#define bm buddy_pmm_manager

static void reset_fixture(void) {
    bm.init();
    for (size_t i = 0; i < 128; i++) {
        fixture[i].flags = 0;
        SetPageReserved(&fixture[i]);
        fixture[i].property = 0;
        fixture[i].ref = 0;
    }
}

static size_t capacity(size_t n) {
    size_t k = 1;
    while (k < n) k <<= 1;
    return k;
}

void buddy_test(void) {
    struct Page *saved_pages = pages;
    size_t saved_npage = npage;
    pages = fixture;
    npage = nbase + 128;
    reset_fixture();
    assert(bm.nr_free_pages() == 0 && bm.alloc_pages(1) == NULL);
    bm.init_memmap(fixture + 3, 93);
    assert(bm.nr_free_pages() == 93);
    assert(bm.alloc_pages(0) == NULL);
    assert(bm.alloc_pages((size_t)-1) == NULL);
    assert(bm.alloc_pages(129) == NULL);
    struct Page *held[128];
    size_t count = 0;
    while ((held[count] = bm.alloc_pages(1)) != NULL) {
        assert(held[count] >= fixture + 3 && held[count] < fixture + 96);
        for (size_t i = 0; i < count; i++) assert(held[count] != held[i]);
        count++;
        assert(count <= 93);
    }
    assert(count == 93 && bm.nr_free_pages() == 0);
    for (size_t i = 0; i < count; i++) bm.free_pages(held[i], 1);
    assert(bm.nr_free_pages() == 93);
    for (size_t i = 0; i < 3; i++) assert(PageReserved(fixture + i));
    for (size_t i = 96; i < 128; i++) assert(PageReserved(fixture + i));
    cprintf("[Buddy] unaligned range, tail pages, exhaustion: PASS\n");

    reset_fixture();
    bm.init_memmap(fixture, 128);
    const size_t requests[] = {1, 2, 3, 5, 7, 8, 9, 31, 32, 33, 64, 65, 128};
    for (size_t i = 0; i < sizeof(requests) / sizeof(requests[0]); i++) {
        size_t n = requests[i], k = capacity(n);
        struct Page *p = bm.alloc_pages(n);
        assert(p != NULL && page2ppn(p) % k == 0);
        assert(bm.nr_free_pages() == 128 - k);
        assert(!PageProperty(p) && page_ref(p) == 0);
        bm.free_pages(p, n);
        assert(bm.nr_free_pages() == 128);
        p = bm.alloc_pages(128);
        assert(p == fixture && bm.nr_free_pages() == 0);
        bm.free_pages(p, 128);
    }
    cprintf("[Buddy] power-of-two rounding, split, recursive merge: PASS\n");

    // Keep alternating physical pages occupied: free count alone is insufficient.
    for (size_t i = 0; i < 128; i++) {
        struct Page *p = bm.alloc_pages(1);
        assert(p != NULL);
        held[p - fixture] = p;
    }
    for (size_t i = 0; i < 128; i += 2) bm.free_pages(held[i], 1);
    assert(bm.nr_free_pages() == 64 && bm.alloc_pages(2) == NULL);
    for (size_t i = 1; i < 128; i += 2) bm.free_pages(held[i], 1);
    struct Page *whole = bm.alloc_pages(128);
    assert(whole == fixture);
    bm.free_pages(whole, 128);
    cprintf("[Buddy] non-buddy fragmentation and recovery: PASS\n");

    reset_fixture();
    bm.init_memmap(fixture, 16);
    bm.init_memmap(fixture + 32, 16);
    assert(bm.nr_free_pages() == 32 && bm.alloc_pages(32) == NULL);
    struct Page *a = bm.alloc_pages(16), *b = bm.alloc_pages(16);
    assert(a != NULL && b != NULL && a != b);
    assert(bm.alloc_pages(1) == NULL);
    bm.free_pages(b, 16);
    bm.free_pages(a, 16);
    assert(bm.nr_free_pages() == 32);
    assert(PageReserved(fixture + 16) && PageReserved(fixture + 31));
    reset_fixture();
    bm.init_memmap(fixture + 16, 16);
    bm.init_memmap(fixture, 16);
    whole = bm.alloc_pages(32);
    assert(whole == fixture);
    bm.free_pages(whole, 32);
    cprintf("[Buddy] holes and adjacent range registration: PASS\n");

    reset_fixture();
    bm.init_memmap(fixture, 128);
    size_t sizes[32] = {0};
    for (size_t i = 0; i < 32; i++) held[i] = NULL;
    unsigned int rng = 2412677;
    size_t used = 0;
    for (size_t step = 0; step < 4000; step++) {
        rng = rng * 1664525U + 1013904223U;
        size_t slot = (rng >> 16) % 32;
        if (held[slot]) {
            bm.free_pages(held[slot], sizes[slot]);
            used -= capacity(sizes[slot]);
            held[slot] = NULL;
        } else {
            sizes[slot] = 1 + ((rng >> 8) % 17);
            struct Page *p = bm.alloc_pages(sizes[slot]);
            if (p) {
                size_t k = capacity(sizes[slot]);
                assert(p >= fixture && p + k <= fixture + 128);
                assert(page2ppn(p) % k == 0);
                for (size_t i = 0; i < 32; i++) {
                    if (held[i]) assert(p + k <= held[i] ||
                        held[i] + capacity(sizes[i]) <= p);
                }
                held[slot] = p;
                used += k;
            }
        }
        assert(bm.nr_free_pages() + used == 128);
    }
    for (size_t i = 0; i < 32; i++) if (held[i]) bm.free_pages(held[i], sizes[i]);
    whole = bm.alloc_pages(128);
    assert(whole == fixture);
    bm.free_pages(whole, 128);
    cprintf("[Buddy] 4000 deterministic mixed operations: PASS\n");
    pages = saved_pages;
    npage = saved_npage;
}
