/* Supplementary module 2 tests, linked only in a temporary test build. */
#include <assert.h>
#include <clock.h>
#include <defs.h>
#include <riscv.h>
#include <stdio.h>
#include <string.h>
#include <trap.h>

void interrupt_handler(struct trapframe *tf);
void exception_handler(struct trapframe *tf);
void test_lab3_module2(void);

static void check_unchanged(uintptr_t cause, bool interrupt) {
    struct trapframe tf, before;
    memset(&tf, 0, sizeof(tf));
    tf.epc = (uintptr_t)&test_lab3_module2;
    tf.cause = cause;
    before = tf;
    if (interrupt) {
        interrupt_handler(&tf);
    } else {
        exception_handler(&tf);
    }
    assert(memcmp(&tf, &before, sizeof(tf)) == 0);
}

void test_lab3_module2(void) {
    volatile uintptr_t returned[4] = {0, 0, 0, 0};
    uintptr_t resumed = 0;

    /* Explicit encodings avoid the assembler compressing a 32-bit ebreak.
     * The following 32-bit ADDI proves execution resumes after each trap. */
    asm volatile(".option push\n.option norvc\n"
                 ".2byte 0x9002\naddi %0, %0, 1\n.option pop\n"
                 : "+r"(resumed) : : "memory");
    returned[0] = resumed;
    assert(returned[0] == 1);
    cprintf("[Module 2] PASS: 16-bit breakpoint returns\n");

    resumed = 0;
    asm volatile(".option push\n.option norvc\n"
                 ".word 0x00100073\naddi %0, %0, 1\n.option pop\n"
                 : "+r"(resumed) : : "memory");
    returned[1] = resumed;
    assert(returned[1] == 1);
    cprintf("[Module 2] PASS: 32-bit breakpoint returns\n");

    resumed = 0;
    asm volatile(".option push\n.option norvc\n"
                 ".2byte 0x0000\naddi %0, %0, 1\n.option pop\n"
                 : "+r"(resumed) : : "memory");
    returned[2] = resumed;
    assert(returned[2] == 1);
    cprintf("[Module 2] PASS: 16-bit illegal instruction returns\n");

    resumed = 0;
    asm volatile(".option push\n.option norvc\n"
                 ".word 0x0000007b\naddi %0, %0, 1\n.option pop\n"
                 : "+r"(resumed) : : "memory");
    returned[3] = resumed;
    assert(returned[3] == 1);
    cprintf("[Module 2] PASS: 32-bit illegal instruction returns\n");

    for (size_t i = 0; i < 4; ++i) {
        assert(returned[i] == 1);
    }

    /* These are direct handler calls, not physical interrupt injection. */
    const uintptr_t irqs[] = {
        IRQ_U_SOFT, IRQ_S_SOFT, IRQ_H_SOFT, IRQ_M_SOFT,
        IRQ_U_TIMER, IRQ_H_TIMER, IRQ_M_TIMER,
        IRQ_U_EXT, IRQ_S_EXT, IRQ_H_EXT, IRQ_M_EXT, 15
    };
    uintptr_t interrupt_bit = (uintptr_t)1 << (sizeof(uintptr_t) * 8 - 1);
    size_t old_ticks = ticks;
    for (size_t i = 0; i < sizeof(irqs) / sizeof(irqs[0]); ++i) {
        check_unchanged(interrupt_bit | irqs[i], 1);
    }
    check_unchanged(20, 0);
    assert(ticks == old_ticks);
    cprintf("[Module 2] PASS: non-timer handlers preserve trap frames\n");
}

/* --wrap is added only to the temporary Makefile. The real SBI shutdown
 * still executes; the host script requires a natural QEMU exit as well. */
void __real_sbi_shutdown(void);
void __wrap_sbi_shutdown(void) {
    assert(ticks == 1000);
    cprintf("[Module 2] PASS: shutdown at ticks=1000\n");
    __real_sbi_shutdown();
    panic("SBI shutdown unexpectedly returned");
}
