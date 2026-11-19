/* Standalone module 3 checks; linked only into a temporary test copy. */
#include <assert.h>
#include <defs.h>
#include <intr.h>
#include <list.h>
#include <mmu.h>
#include <pmm.h>
#include <proc.h>
#include <riscv.h>
#include <sbi.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>

void __real_proc_run(struct proc_struct *proc);
void __real_switch_to(struct context *from, struct context *to);
int module3_check_context(uintptr_t seed, uintptr_t *saved_sp, void (*fn)(void));

static bool capture_selection;
static struct proc_struct *selected;
static unsigned selection_calls, real_switches;
static struct proc_struct *worker;
static volatile unsigned visits;
static volatile bool worker_complete;
static uintptr_t saved_sp[2];

static uintptr_t expected_satp(struct proc_struct *proc) {
    return ((uintptr_t)SATP_MODE_SV39 << 60) | (proc->pgdir >> RISCV_PGSHIFT);
}

void __wrap_proc_run(struct proc_struct *proc) {
    if (capture_selection) {
        assert((read_csr(sstatus) & SSTATUS_SIE) == 0);
        selected = proc;
        selection_calls++;
        return;
    }
    __real_proc_run(proc);
}

void __wrap_switch_to(struct context *from, struct context *to) {
    assert(from != to && to == &current->context);
    assert((read_csr(sstatus) & SSTATUS_SIE) == 0);
    assert(read_csr(satp) == expected_satp(current));
    real_switches++;
    __real_switch_to(from, to);
    /* The old proc_run resumes inside its critical section. */
    assert((read_csr(sstatus) & SSTATUS_SIE) == 0);
    assert(read_csr(satp) == expected_satp(current));
}

static void set_interrupts(bool enabled) {
    if (enabled) intr_enable();
    else intr_disable();
}

static void fixture_proc(struct proc_struct *proc, enum proc_state state) {
    memset(proc, 0, sizeof(*proc));
    proc->state = state;
    list_init(&proc->list_link);
}

static void expect_selection(struct proc_struct *source,
                             struct proc_struct *target, bool enabled) {
    current = source;
    source->need_resched = 1;
    selected = NULL;
    selection_calls = 0;
    int previous_runs = target->runs;
    set_interrupts(enabled);
    schedule();
    assert(current == source); /* Selection is captured before any real switch. */
    assert(!source->need_resched);
    assert(target->runs == previous_runs + 1);
    assert(selection_calls == (source != target ? 1 : 0));
    assert(selected == (source != target ? target : NULL));
    assert(!!(read_csr(sstatus) & SSTATUS_SIE) == enabled);
}

static void test_selection(void) {
    struct proc_struct *old_current = current, *old_idle = idleproc;
    list_entry_t old_list = proc_list;
    struct proc_struct idle, a, b, c, d;
    fixture_proc(&idle, PROC_RUNNABLE);
    fixture_proc(&a, PROC_SLEEPING);
    fixture_proc(&b, PROC_ZOMBIE);
    fixture_proc(&c, PROC_ZOMBIE);
    fixture_proc(&d, PROC_RUNNABLE);
    capture_selection = 1;
    idleproc = &idle;
    list_init(&proc_list);
    expect_selection(&idle, &idle, 0);
    list_add_before(&proc_list, &a.list_link);
    list_add_before(&proc_list, &b.list_link);
    expect_selection(&a, &idle, 1);
    cprintf("[Module 3] PASS: empty/all-blocked lists fall back to idle\n");

    list_del_init(&b.list_link);
    a.state = PROC_RUNNABLE;
    expect_selection(&a, &a, 0);
    expect_selection(&a, &a, 1);
    cprintf("[Module 3] PASS: sole runnable current is selected without switching\n");

    b.state = PROC_SLEEPING;
    list_add_before(&proc_list, &b.list_link);
    list_add_before(&proc_list, &c.list_link);
    list_add_before(&proc_list, &d.list_link);
    expect_selection(&a, &d, 1);
    expect_selection(&d, &a, 0);
    expect_selection(&idle, &a, 1);
    cprintf("[Module 3] PASS: circular selection skips sleeping/zombie entries\n");
    intr_disable();
    proc_list = old_list;
    current = old_current;
    idleproc = old_idle;
    capture_selection = 0;
}

static void return_to_idle(void) {
    proc_run(idleproc);
}

static int worker_main(void *arg) {
    assert(current == worker && !!(read_csr(sstatus) & SSTATUS_SIE));
    for (unsigned i = 0; i < 8; i++) {
        assert(read_csr(satp) == expected_satp(worker));
        bool enabled = (i & 1) != 0;
        set_interrupts(enabled);
        visits++;
        assert(module3_check_context(0x2000 + i * 0x100,
                                     &saved_sp[1], return_to_idle) == 0);
        assert(current == worker);
        assert(!!(read_csr(sstatus) & SSTATUS_SIE) == enabled);
        assert(read_csr(satp) == expected_satp(worker));
    }
    worker_complete = 1;
    worker->state = PROC_SLEEPING;
    proc_run(idleproc);
    panic("completed test worker resumed unexpectedly");
}

static void run_worker(void) {
    proc_run(worker);
}

void test_lab4_module3(void) {
    assert(current == idleproc);
    bool initial_interrupts = !!(read_csr(sstatus) & SSTATUS_SIE);
    intr_disable();
    test_selection();

    for (unsigned mode = 0; mode < 2; mode++) {
        set_interrupts(mode);
        struct context before = current->context;
        uintptr_t satp_before = read_csr(satp);
        unsigned calls_before = real_switches;
        proc_run(current);
        assert(real_switches == calls_before && current == idleproc);
        assert(read_csr(satp) == satp_before);
        assert(memcmp(&before, &current->context, sizeof(before)) == 0);
        assert(!!(read_csr(sstatus) & SSTATUS_SIE) == mode);
    }
    cprintf("[Module 3] PASS: proc_run(current) is a no-op with SIE off/on\n");
    intr_disable();
    initproc->state = PROC_SLEEPING; /* Do not enter its do_exit placeholder. */
    struct Page *root_page = alloc_page();
    assert(root_page != NULL);
    memcpy(page2kva(root_page), boot_pgdir_va, PGSIZE);
    int pid = kernel_thread(worker_main, NULL, 0);
    assert(pid > 1);
    worker = find_proc(pid);
    assert(worker != NULL);
    worker->pgdir = page2pa(root_page);
    assert(worker->pgdir != idleproc->pgdir);
    cprintf("[Module 3] Sv39 roots: idle=0x%016lx worker=0x%016lx\n",
            expected_satp(idleproc), expected_satp(worker));

    for (unsigned i = 0; i < 9; i++) {
        bool enabled = (i & 1) != 0;
        set_interrupts(enabled);
        assert(module3_check_context(0x1000 + i * 0x100,
                                     &saved_sp[0], run_worker) == 0);
        assert(current == idleproc);
        assert(!!(read_csr(sstatus) & SSTATUS_SIE) == enabled);
        assert(read_csr(satp) == expected_satp(idleproc));
        assert(visits == (i < 8 ? i + 1 : 8));
    }
    assert(worker_complete && real_switches == 18);
    cprintf("[Module 3] PASS: 18 real switches pass context round-trip and register sentinel checks\n");
    cprintf("[Module 3] PASS: SIE off/on restored after real round trips\n");
    cprintf("[Module 3] PASS: distinct Sv39 root PPNs selected and restored\n");
    cprintf("[Module 3] PASS: switch entry has consistent current/satp with SIE off\n");
    set_interrupts(initial_interrupts);
    cprintf("[Module 3] ALL CHECKS PASSED; requesting SBI shutdown\n");
    sbi_shutdown();
    panic("SBI shutdown returned unexpectedly");
}
