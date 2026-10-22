#include <buddy_pmm.h>
#include <stdio.h>
#include <string.h>

// lab2 function2-3 tests 2410250
// Boot-time tests own all allocations. This bitmap records real Page indices,
// so the temporary initialization test can restore exactly the original pool.
static uint8_t original_pages[BUDDY_MAX_PAGES];

static void
check_count(size_t expected) {
    assert(nr_free_pages() == expected);
    buddy_validate();
}

static void
check_allocation(struct Page *base, size_t actual_pages) {
    assert(base != NULL);
    assert((page2ppn(base) & (actual_pages - 1)) == 0);
    assert(page2ppn(base) + actual_pages <= npage);
    for (size_t i = 0; i < actual_pages; i++) {
        assert(!PageReserved(base + i) && !PageProperty(base + i));
        assert(page_ref(base + i) == 0);
    }
}

static void
check_disjoint(struct Page *a, size_t na, struct Page *b, size_t nb) {
    assert(a + na <= b || b + nb <= a);
}

static void
test_sizes_and_failure(void) {
    size_t before = nr_free_pages();
    struct Page *one = alloc_pages(1);
    check_allocation(one, 1);
    check_count(before - 1);
    struct Page *four = alloc_pages(4);
    check_allocation(four, 4);
    check_count(before - 5);
    struct Page *three = alloc_pages(3);
    check_allocation(three, 4);
    check_count(before - 9);
    check_disjoint(one, 1, four, 4);
    check_disjoint(one, 1, three, 4);
    check_disjoint(four, 4, three, 4);

    assert(alloc_pages((size_t)-1) == NULL);
    assert(alloc_pages((size_t)1 << (sizeof(size_t) * 8 - 1)) == NULL);
    assert(alloc_pages(BUDDY_MAX_PAGES + 1) == NULL);
    check_count(before - 9);
    check_allocation(one, 1);
    check_allocation(four, 4);
    check_allocation(three, 4);
    cprintf("[Buddy] PASS: huge requests fail without changing live allocations\n");

    // An order different from allocation order must restore the same capacity.
    free_pages(four, 4);
    check_count(before - 5);
    free_pages(one, 1);
    check_count(before - 4);
    free_pages(three, 3);  // Use the original request, not the rounded size.
    check_count(before);
    cprintf("[Buddy] PASS: 1/4/3->4 pages, alignment, no overlap, original-size free\n");
}

static struct Page *
find_original_run(size_t count, size_t alignment, size_t remainder) {
    size_t limit = npage - nbase;
    for (size_t index = 0; index + count <= limit; index++) {
        if ((nbase + index) % alignment != remainder) {
            continue;
        }
        size_t i = 0;
        while (i < count && original_pages[index + i]) {
            i++;
        }
        if (i == count) {
            return pages + index;
        }
    }
    panic("buddy tests: no suitable real page interval");
}

// All free pages are allocated one at a time. Exhaustion makes later tests
// independent of which address an allocator normally prefers.
static void
exhaust_pool(size_t expected) {
    memset(original_pages, 0, sizeof(original_pages));
    size_t count = 0;
    struct Page *page;
    while ((page = alloc_page()) != NULL) {
        check_allocation(page, 1);
        size_t index = page - pages;
        assert(index < npage - nbase && index < BUDDY_MAX_PAGES);
        assert(!original_pages[index]);
        original_pages[index] = 1;
        count++;
        assert(count <= expected && nr_free_pages() == expected - count);
    }
    assert(count == expected);
    assert(alloc_pages(1) == NULL && alloc_pages(3) == NULL);
    check_count(0);
}

static void
test_non_buddies(void) {
    // PPNs 4k+1 and 4k+2 are adjacent, but belong to different order-1 blocks.
    struct Page *base = find_original_run(2, 4, 1);
    free_page(base);
    free_page(base + 1);
    check_count(2);
    assert(alloc_pages(2) == NULL);
    check_count(2);
    struct Page *a = alloc_page();
    struct Page *b = alloc_page();
    assert(a != b);
    assert((a == base && b == base + 1) ||
           (a == base + 1 && b == base));
    check_count(0);
    cprintf("[Buddy] PASS: exhaustion and adjacent non-buddies stay separate\n");
}

static void
test_split_and_merge(void) {
    struct Page *base = find_original_run(8, 8, 0);
    // Free even offsets, then odd offsets, to exercise several merge levels.
    for (size_t parity = 0; parity < 2; parity++) {
        for (size_t i = parity; i < 8; i += 2) {
            free_page(base + i);
        }
    }
    check_count(8);
    struct Page *whole = alloc_pages(8);
    assert(whole == base);
    check_count(0);
    free_pages(whole, 8);
    check_count(8);

    // Two 3-page requests must split the restored 8-page block into two halves.
    struct Page *a = alloc_pages(3);
    struct Page *b = alloc_pages(3);
    check_allocation(a, 4);
    check_allocation(b, 4);
    assert((a == base && b == base + 4) ||
           (a == base + 4 && b == base));
    check_count(0);
    free_pages(b, 3);
    check_count(4);
    assert(alloc_pages(8) == NULL);  // The other half is still allocated.
    check_count(4);
    free_pages(a, 3);
    check_count(8);
    whole = alloc_pages(8);
    assert(whole == base);
    check_count(0);
    free_pages(whole, 8);

    // Return to the all-single-page allocation state used by pool restoration.
    unsigned int seen = 0;
    for (size_t i = 0; i < 8; i++) {
        struct Page *page = alloc_page();
        assert(page != NULL);
        assert(page >= base && page < base + 8);
        unsigned int bit = 1U << (page - base);
        assert((seen & bit) == 0);
        seen |= bit;
    }
    assert(seen == 0xff);
    check_count(0);
    cprintf("[Buddy] PASS: splitting, multi-level merging and a busy buddy\n");
}

static void
reserve_original_pages(void) {
    for (size_t i = 0; i < npage - nbase; i++) {
        if (original_pages[i]) {
            SetPageReserved(pages + i);
            ClearPageProperty(pages + i);
            pages[i].property = 0;
        }
    }
}

static void
test_unaligned_init_and_restore(size_t before) {
    struct Page *arena = find_original_run(16, 16, 0);
    // No live allocation may survive init(): genuinely free every test page.
    for (size_t i = npage - nbase; i-- > 0;) {
        if (original_pages[i]) {
            free_page(pages + i);
        }
    }
    check_count(before);
    cprintf("[Buddy] PASS: reverse-order release restores the entire pool\n");

    // This controlled reset is only valid in the exclusive boot-time check.
    // Every temporary page belongs to the original allocator's real free pool.
    pmm_manager->init();
    reserve_original_pages();
    pmm_manager->init_memmap(arena + 1, 13);
    check_count(13);
    assert(PageReserved(arena) && PageReserved(arena + 14));
    assert(PageReserved(arena + 15));
    struct Page *allocated[13];
    unsigned int seen = 0;
    for (size_t i = 0; i < 13; i++) {
        allocated[i] = alloc_page();
        assert(allocated[i] != NULL);
        assert(allocated[i] >= arena + 1 && allocated[i] < arena + 14);
        unsigned int bit = 1U << (allocated[i] - arena - 1);
        assert((seen & bit) == 0);
        seen |= bit;
    }
    assert(seen == 0x1fff);
    assert(alloc_page() == NULL);
    check_count(0);
    for (size_t i = 0; i < 13; i++) {
        free_page(allocated[(i * 5) % 13]);
    }
    check_count(13);
    assert(PageReserved(arena) && PageReserved(arena + 14));
    assert(PageReserved(arena + 15));

    // Re-register every original physical run, preserving holes and reserved
    // pages. Counts recover through actual initialization, never assignment.
    pmm_manager->init();
    reserve_original_pages();
    size_t index = 0, limit = npage - nbase;
    while (index < limit) {
        if (!original_pages[index]) {
            index++;
            continue;
        }
        size_t start = index;
        while (index < limit && original_pages[index]) {
            index++;
        }
        pmm_manager->init_memmap(pages + start, index - start);
    }
    check_count(before);
    struct Page *probe = alloc_pages(16);
    check_allocation(probe, 16);
    check_count(before - 16);
    free_pages(probe, 16);
    check_count(before);
    cprintf("[Buddy] PASS: unaligned 13-page init and exact pool restoration\n");
}

void
buddy_run_tests(void) {
    assert(pmm_manager == &buddy_pmm_manager);
    size_t before = nr_free_pages();
    assert(before >= 32 && before <= BUDDY_MAX_PAGES);
    test_sizes_and_failure();
    exhaust_pool(before);
    test_non_buddies();
    test_split_and_merge();
    test_unaligned_init_and_restore(before);
    check_count(before);
}
