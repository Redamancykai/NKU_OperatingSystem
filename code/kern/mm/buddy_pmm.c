// lab2 function2-3 2412677
#include <buddy_pmm.h>
#include <stdio.h>

#define ALLOC_HEAD (1UL << 2)
#define ALLOC_USED (1UL << 3)

static struct {
    list_entry_t free[BUDDY_MAX_ORDER + 1];
    size_t nr_free;
} area;

static void buddy_init(void) {
    for (unsigned int order = 0; order <= BUDDY_MAX_ORDER; order++)
        list_init(&area.free[order]);
    area.nr_free = 0;
}

// Return -1 before shifting can overflow, including for SIZE_MAX requests.
static int request_order(size_t n) {
    if (n == 0 || n > (1UL << BUDDY_MAX_ORDER)) return -1;
    int order = 0;
    while ((1UL << order) < n) order++;
    return order;
}

static void insert_block(struct Page *base, unsigned int order) {
    while (order < BUDDY_MAX_ORDER) {
        size_t buddy_ppn = page2ppn(base) ^ (1UL << order);
        if (buddy_ppn < nbase || buddy_ppn >= npage) break;
        struct Page *other = pages + (buddy_ppn - nbase);
        if (!PageProperty(other) || other->property != order) break;
        list_del(&other->page_link);
        ClearPageProperty(other);
        other->property = 0;
        if (other < base) base = other;
        order++;
    }
    base->property = order;
    SetPageProperty(base);
    list_add(&area.free[order], &base->page_link);
}

static void buddy_init_memmap(struct Page *base, size_t n) {
    assert(n > 0 && base >= pages && base < pages + (npage - nbase));
    assert(n <= (size_t)(pages + (npage - nbase) - base));
    for (size_t i = 0; i < n; i++) assert(PageReserved(base + i));
    for (size_t i = 0; i < n; i++) {
        base[i].flags = base[i].property = 0;
        set_page_ref(base + i, 0);
        list_init(&base[i].page_link);
    }
    area.nr_free += n;
    while (n > 0) {
        unsigned int order = BUDDY_MAX_ORDER;
        while ((1UL << order) > n || (page2ppn(base) & ((1UL << order) - 1)))
            order--;
        insert_block(base, order);
        base += 1UL << order;
        n -= 1UL << order;
    }
}

static struct Page *buddy_alloc_pages(size_t n) {
    int wanted = request_order(n);
    if (wanted < 0 || (1UL << wanted) > area.nr_free) return NULL;
    unsigned int order = wanted;
    while (order <= BUDDY_MAX_ORDER && list_empty(&area.free[order])) order++;
    if (order > BUDDY_MAX_ORDER) return NULL;
    struct Page *base = le2page(list_next(&area.free[order]), page_link);
    list_del(&base->page_link);
    ClearPageProperty(base);
    while (order > (unsigned int)wanted) {
        order--;
        struct Page *right = base + (1UL << order);
        right->property = order;
        SetPageProperty(right);
        list_add(&area.free[order], &right->page_link);
    }
    size_t count = 1UL << wanted;
    for (size_t i = 0; i < count; i++) {
        base[i].flags = ALLOC_USED;
        base[i].property = 0;
        set_page_ref(base + i, 0);
    }
    base->flags |= ALLOC_HEAD;
    base->property = n;
    area.nr_free -= count;
    return base;
}

static void buddy_free_pages(struct Page *base, size_t n) {
    int order = request_order(n);
    assert(order >= 0);
    assert(base >= pages && base < pages + (npage - nbase));
    assert((base->flags & ALLOC_HEAD) && base->property == n);
    size_t count = 1UL << order;
    assert(count <= (size_t)(pages + (npage - nbase) - base));
    assert((page2ppn(base) & (count - 1)) == 0);
    // Validate the whole allocation before mutating any state.
    for (size_t i = 0; i < count; i++) {
        assert(base[i].flags & ALLOC_USED);
        assert(!PageReserved(base + i) && page_ref(base + i) == 0);
    }
    for (size_t i = 0; i < count; i++) {
        base[i].flags = base[i].property = 0;
        list_init(&base[i].page_link);
    }
    area.nr_free += count;
    insert_block(base, order);
}

static size_t buddy_nr_free_pages(void) { return area.nr_free; }

static void buddy_check(void) {
    // Nodes in real memory remain untouched. Their sentinel addresses do not
    // change when area is restored, so the saved links remain valid.
    typeof(area) saved_area = area;
    buddy_test();
    area = saved_area;
    size_t before = area.nr_free;
    struct Page *p = buddy_alloc_pages(3);
    assert(p != NULL && area.nr_free == before - 4);
    buddy_free_pages(p, 3);
    assert(area.nr_free == before);
    cprintf("[Buddy] real memory accounting: PASS\n");
}

const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};
