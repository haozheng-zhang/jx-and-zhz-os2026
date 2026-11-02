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

