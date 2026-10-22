#include <pmm.h>
#include <list.h>
#include <best_fit_pmm.h>
#include <stdio.h>

//lab2 function2-2 2412414

static free_area_t free_area;
#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free)

static void best_init(void) {
    list_init(&free_list);
    nr_free = 0;
}

/* Insert once in address order, then merge physical neighbours. */
static void best_insert_block(struct Page *base) {
    list_entry_t *le = list_next(&free_list);
    while (le != &free_list && le2page(le, page_link) < base) {
        le = list_next(le);
    }
    list_add_before(le, &base->page_link);
    le = list_prev(&base->page_link);
    if (le != &free_list) {
        struct Page *prev = le2page(le, page_link);
        if (prev + prev->property == base) {
            prev->property += base->property;
            ClearPageProperty(base);
            base->property = 0;
            list_del(&base->page_link);
            base = prev;
        }
    }
    le = list_next(&base->page_link);
    if (le != &free_list) {
        struct Page *next = le2page(le, page_link);
        if (base + base->property == next) {
            base->property += next->property;
            ClearPageProperty(next);
            next->property = 0;
            list_del(&next->page_link);
        }
    }
}

static void best_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    for (struct Page *p = base; p < base + n; p++) {
        assert(PageReserved(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    nr_free += n;
    best_insert_block(base);
}

static struct Page *best_alloc_pages(size_t n) {
    assert(n > 0);
    if (n > nr_free) {
        return NULL;
    }
    struct Page *best = NULL;
    for (list_entry_t *le = list_next(&free_list); le != &free_list;
         le = list_next(le)) {
        struct Page *p = le2page(le, page_link);
        if (p->property >= n && (best == NULL || p->property < best->property)) {
            best = p;
            if (p->property == n) {
                break;
            }
        }
    }
    if (best != NULL) {
        size_t remaining = best->property - n;
        list_entry_t *prev = list_prev(&best->page_link);
        list_del(&best->page_link);
        if (remaining > 0) {
            struct Page *tail = best + n;
            tail->property = remaining;
            SetPageProperty(tail);
            list_add_after(prev, &tail->page_link);
        }
        best->property = 0;
        ClearPageProperty(best);
        nr_free -= n;
    }
    return best;
}

static void best_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    for (struct Page *p = base; p < base + n; p++) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    nr_free += n;
    best_insert_block(base);
}

static size_t best_nr_free_pages(void) {
    return nr_free;
}

// Framework tests: students must not modify basic_check or best_fit_check.
static void
basic_check(void) {
    struct Page *p0, *p1, *p2;
    p0 = p1 = p2 = NULL;
    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(p0 != p1 && p0 != p2 && p1 != p2);
    assert(page_ref(p0) == 0 && page_ref(p1) == 0 && page_ref(p2) == 0);

    assert(page2pa(p0) < npage * PGSIZE);
    assert(page2pa(p1) < npage * PGSIZE);
    assert(page2pa(p2) < npage * PGSIZE);

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    assert(alloc_page() == NULL);

    free_page(p0);
    free_page(p1);
    free_page(p2);
    assert(nr_free == 3);

    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(alloc_page() == NULL);

    free_page(p0);
    assert(!list_empty(&free_list));

    struct Page *p;
    assert((p = alloc_page()) == p0);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    free_list = free_list_store;
    nr_free = nr_free_store;

    free_page(p);
    free_page(p1);
    free_page(p2);
}

static void
best_fit_check(void) {
    int score = 0, sumscore = 6;
    int count = 0, total = 0;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        assert(PageProperty(p));
        count++, total += p->property;
    }
    assert(total == nr_free_pages());

    basic_check();

    #ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n", score, sumscore);
    #endif
    struct Page *p0 = alloc_pages(5), *p1, *p2;
    assert(p0 != NULL);
    assert(!PageProperty(p0));

    #ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n", score, sumscore);
    #endif
    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));
    assert(alloc_page() == NULL);

    #ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n", score, sumscore);
    #endif
    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    // * - - * -
    free_pages(p0 + 1, 2);
    free_pages(p0 + 4, 1);
    assert(alloc_pages(4) == NULL);
    assert(PageProperty(p0 + 1) && p0[1].property == 2);
    // * - - * *
    assert((p1 = alloc_pages(1)) != NULL);
    assert(alloc_pages(2) != NULL);
    assert(p0 + 4 == p1);

    #ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n", score, sumscore);
    #endif
    p2 = p0 + 1;
    free_pages(p0, 5);
    assert((p0 = alloc_pages(5)) != NULL);
    assert(alloc_page() == NULL);

    #ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n", score, sumscore);
    #endif
    assert(nr_free == 0);
    nr_free = nr_free_store;

    free_list = free_list_store;
    free_pages(p0, 5);

    le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        count--, total -= p->property;
    }
    assert(count == 0);
    assert(total == 0);
    #ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n", score, sumscore);
    #endif
}


/* Supplementary tests use only pages allocated from the live manager. */
static void best_test_state(size_t blocks, size_t total) {
    size_t count = 0, sum = 0;
    struct Page *prev = NULL;
    for (list_entry_t *le = list_next(&free_list); le != &free_list;
         le = list_next(le)) {
        assert(list_next(list_prev(le)) == le);
        assert(list_prev(list_next(le)) == le);
        struct Page *p = le2page(le, page_link);
        assert(PageProperty(p) && p->property > 0);
        if (prev != NULL) {
            assert(prev + prev->property < p);
        }
        for (size_t i = 0; i < p->property; i++) {
            assert(!PageReserved(p + i) && page_ref(p + i) == 0);
            assert(i == 0 || !PageProperty(p + i));
        }
        prev = p;
        sum += p->property;
        assert(++count <= blocks);
    }
    assert(list_next(list_prev(&free_list)) == &free_list);
    assert(list_prev(list_next(&free_list)) == &free_list);
    assert(count == blocks && sum == total && nr_free_pages() == total);
}

/* Reclaim every currently free test block as an allocation, keeping the arena. */
static void best_test_drain(void) {
    while (!list_empty(&free_list)) {
        struct Page *p = le2page(list_next(&free_list), page_link);
        assert(alloc_pages(p->property) == p);
    }
    best_test_state(0, 0);
}

static void best_test_selection(struct Page *arena) {
    /* Address order: 8, 5, 7 pages. Request 4: choose 5, not 8 or 7. */
    free_pages(arena, 8);
    free_pages(arena + 9, 5);
    free_pages(arena + 15, 7);
    best_test_state(3, 20);
    assert(alloc_pages(4) == arena + 9);
    assert(!PageProperty(arena + 9));
    assert(PageProperty(arena + 13) && arena[13].property == 1);
    best_test_state(3, 16);
    /* Enough total pages, but largest block is 8; both failures preserve state. */
    assert(alloc_pages(9) == NULL);
    assert(alloc_pages(17) == NULL);
    best_test_state(3, 16);
    assert(alloc_page() == arena + 13);
    assert(!PageProperty(arena + 13));
    best_test_state(2, 15);
    best_test_drain();

    /* Equal non-exact minima: lower address wins, leaving a 2-page suffix. */
    free_pages(arena, 5);
    free_pages(arena + 8, 5);
    free_pages(arena + 16, 8);
    assert(alloc_pages(3) == arena);
    assert(PageProperty(arena + 3) && arena[3].property == 2);
    /* Exact 5-page match must beat the later 8-page candidate. */
    assert(alloc_pages(5) == arena + 8);
    best_test_state(2, 10);
    best_test_drain();

    /* Equal exact minima: return the lower address, then the other whole block. */
    free_pages(arena + 8, 5);
    free_pages(arena, 5);
    assert(alloc_pages(5) == arena);
    assert(alloc_pages(5) == arena + 8);
    best_test_state(0, 0);
    assert(alloc_page() == NULL);
    cprintf("[Best-Fit extra] selection, split, ties and failures: PASS\n");
}

static void best_test_merging(struct Page *arena) {
    /* Empty, tail and middle insertions; gaps prevent merging. */
    free_pages(arena + 2, 2);
    free_pages(arena + 10, 2);
    free_pages(arena + 6, 2);
    free_pages(arena + 14, 2);
    best_test_state(4, 8);
    best_test_drain();

    free_pages(arena, 2);
    free_pages(arena + 2, 2);  /* predecessor only */
    assert(arena->property == 4 && !PageProperty(arena + 2));
    best_test_state(1, 4);
    best_test_drain();

    free_pages(arena + 4, 2);
    free_pages(arena + 2, 2);  /* successor only; insert at head */
    assert(arena[2].property == 4 && !PageProperty(arena + 4));
    best_test_state(1, 4);
    best_test_drain();

    free_pages(arena, 2);
    free_pages(arena + 4, 2);
    free_pages(arena + 2, 2);  /* both neighbours */
    assert(arena->property == 6);
    assert(!PageProperty(arena + 2) && !PageProperty(arena + 4));
    best_test_state(1, 6);
    best_test_drain();

    /* Register reserved pages out of order, merging on both sides. */
    for (size_t i = 0; i < 6; i++) {
        SetPageReserved(arena + i);
    }
    best_init_memmap(arena + 4, 2);
    best_init_memmap(arena, 2);
    best_init_memmap(arena + 2, 2);
    assert(arena->property == 6);
    best_test_state(1, 6);
    best_test_drain();

    /* Repeated splits and releases must restore the complete arena block. */
    for (size_t i = 0; i < 16; i++) {
        free_pages(arena, 32);
        struct Page *a = alloc_pages(3), *b = alloc_pages(5);
        assert(a == arena && b == arena + 3);
        free_pages(a, 3);
        free_pages(b, 5);
        assert(arena->property == 32);
        best_test_state(1, 32);
        assert(alloc_pages(32) == arena);
        best_test_state(0, 0);
    }
    cprintf("[Best-Fit extra] insertion, merging, registration and cycles: PASS\n");
}

/* Internal test entry called by pmm_init after the unchanged framework check. */
void best_fit_run_extra_checks(void) {
    size_t initial = nr_free_pages();
    struct Page *arena = alloc_pages(32);
    assert(arena != NULL);
    list_entry_t saved_list = free_list;
    unsigned int saved_count = nr_free;
    list_init(&free_list);
    nr_free = 0;

    best_test_selection(arena);
    best_test_merging(arena);
    best_test_state(0, 0);

    free_list = saved_list;
    nr_free = saved_count;
    free_pages(arena, 32);
    assert(nr_free_pages() == initial);
    cprintf("[Best-Fit extra] live free-page count restored: PASS\n");
}

const struct pmm_manager best_fit_pmm_manager = {
    .name = "best_fit_pmm_manager",
    .init = best_init,
    .init_memmap = best_init_memmap,
    .alloc_pages = best_alloc_pages,
    .free_pages = best_free_pages,
    .nr_free_pages = best_nr_free_pages,
    .check = best_fit_check,
};
