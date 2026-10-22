#ifndef __KERN_MM_BUDDY_PMM_H__
#define __KERN_MM_BUDDY_PMM_H__

#include <pmm.h>

#define BUDDY_MAX_PAGES ((PHYSICAL_MEMORY_END - DRAM_BASE) / PGSIZE)

extern const struct pmm_manager buddy_pmm_manager;

/* Read-only invariant check, also used by the separate boot-time tests. */
void buddy_validate(void);
void buddy_run_tests(void);

#endif /* !__KERN_MM_BUDDY_PMM_H__ */
