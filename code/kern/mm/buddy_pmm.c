#include <buddy_pmm.h>
#include <stdio.h>
#include <string.h>

// lab2 function2-3 2410250
// Blocks are aligned by physical PPN; property is a free head's page count.
// alloc(n) owns ceil_pow2(n) pages. free(base, n) requires the original pair.
#define BUDDY_MAX_ORDER 15

enum page_state { UNREGISTERED, FREE, ALLOCATED };

static list_entry_t free_lists[BUDDY_MAX_ORDER + 1];
static size_t nr_free, nr_registered;
static uint8_t state[BUDDY_MAX_PAGES];
static uint32_t request[BUDDY_MAX_PAGES];
static uint8_t checked[BUDDY_MAX_PAGES];

static size_t
page_index(struct Page *page) {
    uintptr_t address = (uintptr_t)page;
    uintptr_t first = (uintptr_t)pages;
    assert(npage >= nbase && npage - nbase <= BUDDY_MAX_PAGES);
    assert(address >= first && (address - first) % sizeof(*page) == 0);
    size_t index = (address - first) / sizeof(*page);
    assert(index < npage - nbase);
    return index;
}

static size_t
order_pages(unsigned int order) {
    return (size_t)1 << order;
}

static bool
request_order(size_t n, unsigned int *order) {
    if (n == 0 || n > BUDDY_MAX_PAGES) {
        return 0;
    }
    *order = 0;
    while (order_pages(*order) < n) {
        (*order)++;
    }
    return 1;
}

static void
insert_block(struct Page *base, unsigned int order) {
    base->property = order_pages(order);
    SetPageProperty(base);
    list_add(&free_lists[order], &base->page_link);
}

static void
remove_block(struct Page *base) {
    list_del_init(&base->page_link);
    ClearPageProperty(base);
    base->property = 0;
}

static struct Page *
find_partner(struct Page *base, unsigned int order) {
    size_t ppn = page2ppn(base) ^ order_pages(order);
    if (ppn < nbase || ppn >= npage) {
        return NULL;
    }
    size_t index = ppn - nbase;
    struct Page *partner = pages + index;
    if (state[index] != FREE || !PageProperty(partner) ||
        partner->property != order_pages(order)) {
        return NULL;
    }
    return partner;
}

// The block being released is free, but has not yet been inserted in a list.
static void
release_block(struct Page *base, unsigned int order) {
    while (order < BUDDY_MAX_ORDER) {
        struct Page *partner = find_partner(base, order);
        if (partner == NULL) {
            break;
        }
        remove_block(partner);
        if (partner < base) {
            base = partner;
        }
        order++;
    }
    insert_block(base, order);
}

static void
buddy_init(void) {
    assert(order_pages(BUDDY_MAX_ORDER) == BUDDY_MAX_PAGES);
    for (unsigned int order = 0; order <= BUDDY_MAX_ORDER; order++) {
        list_init(&free_lists[order]);
    }
    memset(state, 0, sizeof(state));
    memset(request, 0, sizeof(request));
    nr_free = nr_registered = 0;
}

static void
buddy_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    size_t index = page_index(base);
    assert(n <= npage - nbase - index);
    for (size_t i = 0; i < n; i++) {
        assert(PageReserved(base + i) && state[index + i] == UNREGISTERED);
    }
    for (size_t i = 0; i < n; i++) {
        struct Page *page = base + i;
        page->flags = page->property = 0;
        set_page_ref(page, 0);
        list_init(&page->page_link);
        state[index + i] = FREE;
    }
    nr_free += n;
    nr_registered += n;

    // Greedily cover every page, including an unaligned prefix and odd tail.
    while (n != 0) {
        unsigned int order = 0;
        size_t ppn = page2ppn(base);
        while (order < BUDDY_MAX_ORDER && order_pages(order + 1) <= n &&
               (ppn & (order_pages(order + 1) - 1)) == 0) {
            order++;
        }
        size_t count = order_pages(order);
        release_block(base, order);
        base += count;
        n -= count;
    }
}

static struct Page *
buddy_alloc_pages(size_t n) {
    assert(n > 0);
    unsigned int wanted;
    if (n > nr_free || !request_order(n, &wanted)) {
        return NULL;
    }
    unsigned int order = wanted;
    while (order <= BUDDY_MAX_ORDER && list_empty(&free_lists[order])) {
        order++;
    }
    if (order > BUDDY_MAX_ORDER) {
        return NULL;
    }
    struct Page *base = le2page(list_next(&free_lists[order]), page_link);
    remove_block(base);
    while (order > wanted) {
        order--;
        insert_block(base + order_pages(order), order);
    }

    size_t index = page_index(base), count = order_pages(wanted);
    for (size_t i = 0; i < count; i++) {
        assert(state[index + i] == FREE && request[index + i] == 0);
        state[index + i] = ALLOCATED;
    }
    request[index] = n;
    nr_free -= count;
    return base;
}

static void
buddy_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    size_t index = page_index(base);
    assert(state[index] == ALLOCATED && request[index] == n);
    unsigned int order;
    assert(request_order(n, &order));
    size_t count = order_pages(order);
    assert(count <= npage - nbase - index);
    assert((page2ppn(base) & (count - 1)) == 0);

    // Validate the whole allocation before changing flags, lists or counters.
    for (size_t i = 0; i < count; i++) {
        assert(state[index + i] == ALLOCATED);
        assert(!PageReserved(base + i) && !PageProperty(base + i));
        assert(i == 0 || request[index + i] == 0);
    }
    for (size_t i = 0; i < count; i++) {
        struct Page *page = base + i;
        state[index + i] = FREE;
        page->flags = page->property = 0;
        set_page_ref(page, 0);
    }
    request[index] = 0;
    release_block(base, order);
    nr_free += count;
}

static size_t
buddy_nr_free_pages(void) {
    return nr_free;
}

void
buddy_validate(void) {
    memset(checked, 0, sizeof(checked));
    size_t total = 0, blocks = 0;
    for (unsigned int order = 0; order <= BUDDY_MAX_ORDER; order++) {
        list_entry_t *head = &free_lists[order];
        for (list_entry_t *le = list_next(head); le != head; le = list_next(le)) {
            assert(++blocks <= BUDDY_MAX_PAGES);
            assert(list_prev(list_next(le)) == le);
            assert(list_next(list_prev(le)) == le);
            struct Page *base = le2page(le, page_link);
            size_t index = page_index(base), count = order_pages(order);
            assert(count <= npage - nbase - index);
            assert((page2ppn(base) & (count - 1)) == 0);
            assert(PageProperty(base) && base->property == count);
            for (size_t i = 0; i < count; i++) {
                assert(state[index + i] == FREE && !checked[index + i]);
                assert(!PageReserved(base + i) && request[index + i] == 0);
                assert(page_ref(base + i) == 0);
                assert(i == 0 || !PageProperty(base + i));
                checked[index + i] = 1;
            }
            if (order < BUDDY_MAX_ORDER) {
                assert(find_partner(base, order) == NULL);
            }
            total += count;
        }
    }
    assert(total == nr_free);

    size_t registered = 0;
    for (size_t index = 0; index < npage - nbase; index++) {
        struct Page *base = pages + index;
        if (state[index] == UNREGISTERED) {
            assert(!checked[index] && request[index] == 0);
            continue;
        }
        registered++;
        if (state[index] == FREE) {
            assert(checked[index] == 1);
            continue;
        }
        assert(state[index] == ALLOCATED);
        assert(!PageReserved(base) && !PageProperty(base));
        if (request[index] != 0) {
            unsigned int order;
            assert(request_order(request[index], &order));
            size_t count = order_pages(order);
            assert(count <= npage - nbase - index);
            assert((page2ppn(base) & (count - 1)) == 0);
            for (size_t i = 0; i < count; i++) {
                assert(state[index + i] == ALLOCATED && !checked[index + i]);
                assert(i == 0 || request[index + i] == 0);
                checked[index + i] = 2;
            }
        }
        assert(checked[index] == 2);
    }
    assert(registered == nr_registered);
}

static void
buddy_check(void) {
    buddy_validate();
    size_t before = nr_free;
    buddy_run_tests();
    buddy_validate();
    assert(nr_free == before);
    cprintf("buddy_check() succeeded! free pages restored: %lu\n", nr_free);
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
