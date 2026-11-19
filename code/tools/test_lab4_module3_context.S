/* Assembly sentinel helper for the temporary module 3 test build. */
#include <riscv.h>
.text
.align 2
.globl module3_check_context
.type module3_check_context, @function
module3_check_context:
    addi sp, sp, -144
    STORE ra, 0(sp)
    STORE s0, 8(sp)
    STORE s1, 16(sp)
    STORE s2, 24(sp)
    STORE s3, 32(sp)
    STORE s4, 40(sp)
    STORE s5, 48(sp)
    STORE s6, 56(sp)
    STORE s7, 64(sp)
    STORE s8, 72(sp)
    STORE s9, 80(sp)
    STORE s10, 88(sp)
    STORE s11, 96(sp)
    STORE a0, 104(sp)
    STORE a1, 112(sp)
    STORE sp, 0(a1)
    mv t6, a0
    addi s0, t6, 16
    addi s1, t6, 32
    addi s2, t6, 48
    addi s3, t6, 64
    addi s4, t6, 80
    addi s5, t6, 96
    addi s6, t6, 112
    addi s7, t6, 128
    addi s8, t6, 144
    addi s9, t6, 160
    addi s10, t6, 176
    addi s11, t6, 192
    jalr ra, 0(a2)
.Lmodule3_resumed:
    li a0, 1
    la t0, .Lmodule3_resumed
    bne ra, t0, .Lmodule3_done
    li a0, 2
    LOAD t0, 112(sp)
    LOAD t1, 0(t0)
    bne sp, t1, .Lmodule3_done
    LOAD t6, 104(sp)
    li a0, 3
    addi t0, t6, 16
    bne s0, t0, .Lmodule3_done
    li a0, 4
    addi t0, t6, 32
    bne s1, t0, .Lmodule3_done
    li a0, 5
    addi t0, t6, 48
    bne s2, t0, .Lmodule3_done
    li a0, 6
    addi t0, t6, 64
    bne s3, t0, .Lmodule3_done
    li a0, 7
    addi t0, t6, 80
    bne s4, t0, .Lmodule3_done
    li a0, 8
    addi t0, t6, 96
    bne s5, t0, .Lmodule3_done
    li a0, 9
    addi t0, t6, 112
    bne s6, t0, .Lmodule3_done
    li a0, 10
    addi t0, t6, 128
    bne s7, t0, .Lmodule3_done
    li a0, 11
    addi t0, t6, 144
    bne s8, t0, .Lmodule3_done
    li a0, 12
    addi t0, t6, 160
    bne s9, t0, .Lmodule3_done
    li a0, 13
    addi t0, t6, 176
    bne s10, t0, .Lmodule3_done
    li a0, 14
    addi t0, t6, 192
    bne s11, t0, .Lmodule3_done
    li a0, 0
.Lmodule3_done:
    LOAD ra, 0(sp)
    LOAD s0, 8(sp)
    LOAD s1, 16(sp)
    LOAD s2, 24(sp)
    LOAD s3, 32(sp)
    LOAD s4, 40(sp)
    LOAD s5, 48(sp)
    LOAD s6, 56(sp)
    LOAD s7, 64(sp)
    LOAD s8, 72(sp)
    LOAD s9, 80(sp)
    LOAD s10, 88(sp)
    LOAD s11, 96(sp)
    addi sp, sp, 144
    ret
.size module3_check_context, .-module3_check_context
