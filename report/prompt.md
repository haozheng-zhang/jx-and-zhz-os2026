# Lab3 实验提示词

模块一负责人：2413790 姜禧；模块二负责人：2410250 张浩正。以下按功能模块顺序汇总提示词。

## 功能模块一：Trap 上下文恢复与 Trap 入口功能

负责人：2413790 姜禧。

### [PROMPT]

实现Trap 上下文恢复与 Trap 入口功能，在kern\trap\trapentry.S中自行设计并实现 RESTORE_ALL和__alltraps。

必须修改实际文件，不能仅展示代码。不修改代码中已有的接口和函数，不得修改 SAVE_ALL、struct trapframe 和 trap() 的函数签名。以工程代码为准，不引入无关代码。

### [RELY]

使用工程已有的trap（）函数，struct trapframe结构体，已经提供的 SAVE_ALL、__alltraps、__trapret、RESTORE_ALL 以及 RISC-V 函数调用约定和特权级 Trap 返回机制，以实际工程定义为准：

```c
struct trapframe {
    struct pushregs gpr;
    uintptr_t status;
    uintptr_t epc;
    uintptr_t badvaddr;
    uintptr_t cause;
};

void trap(struct trapframe *tf) {
    // dispatch based on what type of trap occurred
    trap_dispatch(tf);
}

static inline void trap_dispatch(struct trapframe *tf) {
    if ((intptr_t)tf->cause < 0) {
        // interrupts
        interrupt_handler(tf);
    } else {
        // exceptions
        exception_handler(tf);
    }
}

```

pushregs定义在kern/trap/trap.h

SAVE_ALL定义在 kern/trap/trapentry.S

### [GUARANTEE] 

实现：

RESTORE_ALL

__alltraps

整体实现链应该为：

__alltraps → SAVE_ALL → trap（tf） → __trapret → RESTORE_ALL → sret

仅可根据需要在当前模块内增加内部辅助函数，不新建跨文件接口。

### [SPECIFICATION]

#### RESTORE_ALL：

Pre-Condition：C语言处理已经结束，当前sp 指向一个有效的 struct trapframe

Post-Condition：从当前 Trap frame 恢复返回所需的 sstatus、`sepc` 和通用寄存器，包括原来的 sp。恢复时采用 Trap frame 中处理后的值，包括可能被处理函数修改的 epc。恢复过程不得破坏尚未读取的现场，完成后由 __trapret 中已有的 sret 返回。

Requirements：RESTORE_ALL 必须与已经提供的 SAVE_ALL 保持对应关系，不能自行假设一个与实际 Trap frame 不一致的数据布局。不能简单地把所有寄存器按照完全相同的方式恢复。

#### __alltraps：

Pre-Condition：	CPU 已因 Trap 跳到此入口，寄存器现场尚待软件保存

Post-Condition：通过已有的 SAVE_ALL 保存现场，按照 RISC-V 调用约定，将当前 Trap frame 地址作为参数传给 trap()。

Requirements：需要保证当前 Trap frame 的地址能够按照 RISC-V 调用约定作为 trap() 的参数传递，同时不能破坏后续恢复上下文所需要的信息。

## 功能模块二：中断与异常处理

负责人：2410250 张浩正。

[PROMPT]
请完成 Lab3 功能模块二，直接修改 kern/trap/trap.c，
实现中断和异常处理。保留同学的模块一、已有函数和评分测试，
将本模块 YOUR CODE 改为 2410250。
在 Ubuntu、QEMU 4.1.1 下编译验证，保存关键截图和原始日志。

[RELY]
以工程中的 trapframe、riscv.h、clock.h、sbi.h 为准。
tf->cause 表示原因，tf->epc 是返回位置。
使用 clock_set_next_event、ticks、TICK_NUM、
print_ticks、print_trapframe 和 sbi_shutdown。
当前缺少 SHUTDOWN_AFTER，先设为 10，表示打印十次后关机。

[GUARANTEE]
实现：
```c
void interrupt_handler(struct trapframe *tf);
void exception_handler(struct trapframe *tf);
```
可在本模块增加必要的内部定义，不修改其他函数的行为。

[SPECIFICATION]

interrupt_handler：
前置：tf 是有效的 Trap frame。
处理时钟中断时，设置下次事件，ticks 增加一次；
每满 TICK_NUM 次调用 print_ticks，累计打印十次后关机。
其他已知中断输出类型，未知中断输出原因和完整 Trap frame。
中断处理保留 tf->epc。

exception_handler：
前置：tf 有效；待处理的非法指令或断点位于可读取的内核代码中。
输出 Illegal instruction at 或 Breakpoint at 及对应地址；
根据实际指令编码区分长度，更新 tf->epc，保证返回后继续执行。
其他异常调用 print_trapframe，不擅自跳过指令。

验证原有评分，并补查 16/32 位异常返回、连续时钟中断和自动关机。
失败时保留输出，修复原因，不修改评分断言或伪造通过信息。


## 模块二补充验证

```text
[PROMPT]
补充模块二测试，检查不同指令长度的异常返回和定时关机，保留运行日志和关键截图。

[RELY]
使用 Ubuntu、QEMU 4.1.1 和现有工程，在临时副本中运行测试。

[GUARANTEE]
保留模块一和原有评分脚本，测试结束后不改变正常内核的启动流程。

[SPECIFICATION]
前置条件：模块二已完成并能编译。
分别触发 16/32 位断点和非法指令，检查返回后能继续执行。
检查其他中断和未处理异常保留 Trap frame。
后置条件：打印十次 100 ticks，在 ticks=1000 时调用关机并正常退出；超时或断言失败都算测试失败。
```
