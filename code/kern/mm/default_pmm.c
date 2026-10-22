#include <pmm.h>
#include <list.h>
#include <string.h>
#include <default_pmm.h>

static free_area_t free_area;

#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free)

static void
default_init(void) {  //准备管理结构
    list_init(&free_list); //空闲链表为空
    nr_free = 0; //空闲总页数为0
}

static void
default_init_memmap(struct Page *base, size_t n) { //接受并登记空闲页
    assert(n > 0); //n是连续页面数量
    struct Page *p = base; //base是起始页的描述符指针
    for (; p != base + n; p ++) { //不到最后一个起始页不停止
        assert(PageReserved(p)); //A交来的页面原来处于保留状态
        p->flags = p->property = 0; //清除保留标志、旧属性、引用计数
        set_page_ref(p, 0);
    }
    base->property = n;  //然后把整个区间描述为一个空闲块
    SetPageProperty(base);
    nr_free += n;
    //最后，将首页的page_link按地址递增的顺序插入空闲链表
    if (list_empty(&free_list)) {
        list_add(&free_list, &(base->page_link));
    } else {
        list_entry_t* le = &free_list;
        while ((le = list_next(le)) != &free_list) {
            struct Page* page = le2page(le, page_link);
            if (base < page) {
                list_add_before(le, &(base->page_link));
                break;
            } else if (list_next(le) == &free_list) {
                list_add(le, &(base->page_link));
            }
        }
    }
}

static struct Page *
default_alloc_pages(size_t n) { //寻找、摘除和拆分
    assert(n > 0);
    if (n > nr_free) { //如果所有寻找的连续空闲页数太大，不成功
        return NULL;   //其实，n <= nr_free不保证能成功,3+4不保证连续5页申请
    }
    struct Page *page = NULL;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        if (p->property >= n) { //遍历链表
            page = p;
            break; //找到第一个合适的块就结束，体现了First-Fit
        }
    }
    if (page != NULL) {
        list_entry_t* prev = list_prev(&(page->page_link));
        list_del(&(page->page_link));
        if (page->property > n) { //假设A不够，B足够，我们选择的是B
            struct Page *p = page + n; //选中后，将B的首页从空闲链表中摘除
            p->property = page->property - n; //如果B比申请量大，就拆分
            SetPageProperty(p); //把拆分后新的空闲块的首页放回原来链表的位置
            list_add(prev, &(p->page_link)); //把拆分后新的空闲块的首页放回原来链表的位置
        }
        nr_free -= n; //空闲总页数-n
        ClearPageProperty(page); //清除PG_property ？
    }
    return page;
}

static void
default_free_pages(struct Page *base, size_t n) { //回收并合并
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(!PageReserved(p) && !PageProperty(p)); //先检查释放区间不是保留页
        p->flags = 0; //清理状态                      //也不是已有的空闲块的首页
        set_page_ref(p, 0);
    }
    base->property = n;  //接着建立一个新的空闲块
    SetPageProperty(base);
    nr_free += n;

    if (list_empty(&free_list)) {
        list_add(&free_list, &(base->page_link));
    } else {
        list_entry_t* le = &free_list;
        while ((le = list_next(le)) != &free_list) {
            struct Page* page = le2page(le, page_link);
            if (base < page) {
                list_add_before(le, &(base->page_link));
                break;
            } else if (list_next(le) == &free_list) {
                list_add(le, &(base->page_link));
            }
        }
    }

    list_entry_t* le = list_prev(&(base->page_link));
    if (le != &free_list) {
        p = le2page(le, page_link);
        if (p + p->property == base) { //与前块合并的条件，前块的结束位置恰好等于当前块起点
            p->property += base->property;
            ClearPageProperty(base);
            list_del(&(base->page_link));
            base = p; //后续需要用合并后的新首页继续检查后块
        }
    }

    le = list_next(&(base->page_link));
    if (le != &free_list) {
        p = le2page(le, page_link);
        if (base + base->property == p) { //与后块合并的条件
            base->property += p->property;
            ClearPageProperty(p);
            list_del(&(p->page_link));
        }
    }
}

static size_t
default_nr_free_pages(void) {
    return nr_free;
}
// basic_check() 主要检查单页分配、地址互异、空闲耗尽返回 NULL、释放后重新分配及页数统计。
// default_check() 进一步检查：
// 连续多页分配与拆分。
// 连续块不足时分配失败。
// 释放后块大小和首页标志是否正确。
// 相邻块能否重新合并成完整的大块。
// 测试结束后空闲块数量和空闲页总数是否恢复。
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

// LAB2: below code is used to check the first fit allocation algorithm (your EXERCISE 1) 
// NOTICE: You SHOULD NOT CHANGE basic_check, default_check functions!
static void
default_check(void) {
    int count = 0, total = 0;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        assert(PageProperty(p));
        count ++, total += p->property;
    }
    assert(total == nr_free_pages());

    basic_check();

    struct Page *p0 = alloc_pages(5), *p1, *p2;
    assert(p0 != NULL);
    assert(!PageProperty(p0));

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));
    assert(alloc_page() == NULL);

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    free_pages(p0 + 2, 3);
    assert(alloc_pages(4) == NULL);
    assert(PageProperty(p0 + 2) && p0[2].property == 3);
    assert((p1 = alloc_pages(3)) != NULL);
    assert(alloc_page() == NULL);
    assert(p0 + 2 == p1);

    p2 = p0 + 1;
    free_page(p0);
    free_pages(p1, 3);
    assert(PageProperty(p0) && p0->property == 1);
    assert(PageProperty(p1) && p1->property == 3);

    assert((p0 = alloc_page()) == p2 - 1);
    free_page(p0);
    assert((p0 = alloc_pages(2)) == p2 + 1);

    free_pages(p0, 2);
    free_page(p2);

    assert((p0 = alloc_pages(5)) != NULL);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    nr_free = nr_free_store;

    free_list = free_list_store;
    free_pages(p0, 5);

    le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        count --, total -= p->property;
    }
    assert(count == 0);
    assert(total == 0);
}

const struct pmm_manager default_pmm_manager = {
    .name = "default_pmm_manager",
    .init = default_init,
    .init_memmap = default_init_memmap,
    .alloc_pages = default_alloc_pages,
    .free_pages = default_free_pages,
    .nr_free_pages = default_nr_free_pages,
    .check = default_check,
};
