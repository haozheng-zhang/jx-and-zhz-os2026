# Lab5 实验提示词

## 功能模块一：用户态系统调用功能

负责人：2413790 姜禧。


### [PROMPT]

在 kern/trap/trap.c 中更新 exception_handler(struct trapframe *tf)，保留其他异常的处理逻辑。在 kern/syscall/syscall.c 中实现 syscall() 和各个 sys_* 函数。必须修改实际文件，不能仅展示代码。不修改代码中已有的接口，以工程代码为准，不引入无关代码。

### [RELY]

使用工程已有的系统调用号：

```c
#define SYS_exit            1
#define SYS_fork            2
#define SYS_wait            3
#define SYS_exec            4
#define SYS_clone           5
#define SYS_yield           10
#define SYS_sleep           11
#define SYS_kill            12
#define SYS_gettime         17
#define SYS_getpid          18
#define SYS_brk             19
#define SYS_mmap            20
#define SYS_munmap          21
#define SYS_shmem           22
#define SYS_putc            30
#define SYS_pgdir           31
```

使用已有的trapframe结构体:

```c
struct trapframe
{
    struct pushregs gpr;
    uintptr_t status;
    uintptr_t epc;
    uintptr_t tval;
    uintptr_t cause;
};
```

可以在kern/process/proc.c中查找do_exit、do_fork等内核进程管理相关的代码
在kern/mm/vmm.c中用user_mem_check 来检查用户参数是否合法

### [GUARANTEE]

实现：

```text
exception_handler()，syscall(），sys_exit()，sys_fork()，sys_wait()，sys_exec()，sys_yield()，sys_kill()，sys_getpid()，sys_putc()，sys_pgdir()
```

整理实现链：
用户程序执行 ecall → 内核保存现场 → exception_handler() 识别系统调用 → syscall() 按调用号分发给对应的 sys_* → 执行内核操作。调用正常返回时，结果写入 a0，再通过 sret 返回用户态。

### [SPECIFICATION]

#### void exception_handler():

Pre-Condition：

写入初始化好的trapframe结构体

Post-Condition：

只需要关注 CAUSE_USER_ECALL 这一类情况，把系统调用异常转交给 syscall

Requirements：

在识别到用户态系统调用后必须把 epc 前移，避免返回后重复执行同一条 ecall；识别到 CAUSE_USER_ECALL 后，将 tf->epc 加 4，再调用 syscall()。

#### void syscall(void)：

Pre-Condition：

exception_handler 成功把系统调用异常转交给 syscall

Post-Condition：

从当前进程的 trapframe 中取出系统调用号和参数并完成分发

Requirements：

从 current->tf->gpr.a0 读取系统调用号，从 a1～`a5` 读取参数。访问 syscalls[] 前检查编号范围和表项是否为空。非法调用将 -E_INVAL 写入 a0，有效调用将处理函数的返回值写回 a0。`syscalls[]` 引用的 sys_* 函数必须在表前声明或定义。

#### static int sys_exit():

Pre-Condition：

有效的current->tf

Post-Condition：

- case1：若 do_exit() 返回负错误码，sys_exit()`原样返回该错误码
- case2：当前进程以 arg[0] 指定的退出码结束，进入僵尸态，等待父进程回收，成功退出后不返回原用户程序。

#### static int sys_fork():

Pre-Condition：

有效的current->tf

Post-Condition：

- case1：调用进程收到 do_fork() 返回的负错误码，没有新的子进程进入可运行状态。
- case2：父进程的系统调用返回值为子进程 PID，子进程的返回值为 0

#### static int sys_wait(uint64_t arg[]):

Pre-Condition：

arg[0] 是目标子进程 PID，0 表示等待任意子进程。arg[1] 是退出码写入地址，可为 NULL。

Post-Condition：

- case1：等待失败，原样返回 do_wait() 的负错误码。
- case2：等待成功，回收子进程并返回 0。若 arg[1] 非空，将子进程的退出码写入该地址。

Requirements：

实际写入前，使用 user_mem_check() 检查非空的退出码地址是否可写。

#### static int sys_exec(uint64_t arg[]):

Pre-Condition：

arg[0]～arg[3] 依次是程序名地址、程序名长度、ELF 映像地址和映像大小。

Post-Condition：

- case1：若 do_execve() 返回负错误码，原样返回该错误码。
- case2：装载成功，当前进程的程序映像被替换，返回用户态后从新程序入口开始执行。

Requirements：

将四个参数传给 do_execve()。用户态传入的缓冲区须在读取前检查可读性，旧地址空间释放前须妥善保存后续仍需读取的数据。

#### static int sys_yield(uint64_t arg[]):

Pre-Condition：

current 有效，无需读取 arg[]。

Post-Condition：

调用 do_yield() 请求重新调度，并原样返回其结果。

#### static int sys_kill(uint64_t arg[]):

Pre-Condition：

arg[0] 提供目标进程 PID。

Post-Condition：

- case1：请求失败，原样返回 do_kill() 的负错误码。
- case2：请求成功，目标进程被标记为待退出，返回 0。

#### static int sys_getpid(uint64_t arg[]):

Pre-Condition：

current 有效，无需读取 arg[]。

Post-Condition：

返回当前进程的 PID，即 current->pid。

#### static int sys_putc(uint64_t arg[]):

Pre-Condition：

arg[0] 提供待输出的字符值。

Post-Condition：

调用 cputchar((int)arg[0]) 输出该字符，返回 0。

#### static int sys_pgdir(uint64_t arg[]):

Pre-Condition：

内核提供可用的 print_pgdir() 实现，无需读取 arg[]。

Post-Condition：

调用 print_pgdir() 输出页表信息，返回 0。
