// Host adapter: execute the real kernel allocator and tests with sanitizers.
#include <buddy_pmm.h>
#ifdef TEST_SLUB
#include <slub.h>
#endif
extern int printf(const char *, ...);
extern int vprintf(const char *, __builtin_va_list);
extern void exit(int) __attribute__((noreturn));
struct Page *pages;
size_t npage;
const size_t nbase = 0x80000;
uint64_t va_pa_offset;
static int expected_panic;
const struct pmm_manager *pmm_manager = &buddy_pmm_manager;
struct Page *alloc_pages(size_t n) { return pmm_manager->alloc_pages(n); }
void free_pages(struct Page *p, size_t n) { pmm_manager->free_pages(p, n); }
size_t nr_free_pages(void) { return pmm_manager->nr_free_pages(); }
int cprintf(const char *fmt, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    int result = vprintf(fmt, ap);
    __builtin_va_end(ap);
    return result;
}
void __panic(const char *file, int line, const char *fmt, ...) {
    printf("PANIC %s:%d: ", file, line);
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    vprintf(fmt, ap);
    __builtin_va_end(ap);
    printf("\n");
    exit(expected_panic ? 77 : 1);
}
int main(int argc, char **argv) {
    buddy_test();
    cprintf("Buddy host tests: PASS\n");
#ifdef TEST_SLUB
    static unsigned char memory[512 * PGSIZE] __attribute__((aligned(4096)));
    static struct Page descriptors[512];
    pages = descriptors;
    npage = nbase + 512;
    va_pa_offset = (uintptr_t)memory - (nbase << PGSHIFT);
    for (size_t i = 0; i < 512; i++) SetPageReserved(pages + i);
    pmm_manager->init();
    pmm_manager->init_memmap(pages, 512);
    if (argc > 1) {
        expected_panic = 1;
        char mode = argv[1][0];
        if (mode == 'd' || mode == 's' || mode == 'i' || mode == 'r') {
            struct Page *p = alloc_pages(3);
            if (mode == 'd') { free_pages(p, 3); free_pages(p, 3); }
            if (mode == 's') free_pages(p, 2);
            if (mode == 'i') free_pages(p + 1, 1);
            if (mode == 'r') { set_page_ref(p, 1); free_pages(p, 3); }
        } else {
            void *p = kmalloc(mode == 'l' ? 8193 : 16);
            void *keep_alive = kmalloc(16);
            (void)keep_alive;
            if (mode == 'j' || mode == 'l') kfree((char *)p + 1);
            else { kfree(p); kfree(p); }
        }
        cprintf("ERROR: invalid free was accepted\n");
        return 2;
    }
    slub_check();
    struct Page *held[512];
    for (size_t i = 0; i < 512; i++) {
        held[i] = alloc_pages(1);
        assert(held[i] != NULL);
    }
    assert(nr_free_pages() == 0);
    assert(kmalloc(16) == NULL && kmalloc(8193) == NULL);
    assert(nr_free_pages() == 0);
    for (size_t i = 0; i < 512; i++) free_pages(held[i], 1);
    assert(nr_free_pages() == 512);
    cprintf("[SLUB] backing page exhaustion and recovery: PASS\n");
    static struct Page maximum[1UL << BUDDY_MAX_ORDER];
    pages = maximum;
    npage = nbase + (1UL << BUDDY_MAX_ORDER);
    for (size_t i = 0; i < (1UL << BUDDY_MAX_ORDER); i++)
        SetPageReserved(pages + i);
    pmm_manager->init();
    pmm_manager->init_memmap(pages, 1UL << BUDDY_MAX_ORDER);
    struct Page *max_block = alloc_pages(1UL << BUDDY_MAX_ORDER);
    assert(max_block == pages && nr_free_pages() == 0);
    free_pages(max_block, 1UL << BUDDY_MAX_ORDER);
    assert(nr_free_pages() == (1UL << BUDDY_MAX_ORDER));
    assert(alloc_pages((1UL << BUDDY_MAX_ORDER) + 1) == NULL);
    cprintf("[Buddy] maximum order and request overflow: PASS\n");
    cprintf("SLUB host tests: PASS\n");
#else
    (void)argc;
    (void)argv;
#endif
    return 0;
}
