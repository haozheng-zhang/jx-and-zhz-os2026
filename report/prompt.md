# Lab4 实验提示词

## 功能模块一：虚拟内存页操作功能

负责人：2413790 姜禧。


### [PROMPT]

实现虚拟内存页操作功能，
在kern/mm/pmm.c自行设计并完成page_insert（）和page_remove（），必须修改实际文件，不能仅展示代码。不修改代码中已有的接口和函数，不得修改get_pte()、pte_create()等函数签名，以工程代码为准，不引入无关代码。

### [RELY]

复用工程已有的 get_pte() 定位目标页表项，用 PTE_V 判断旧项是否有效。
用 pte2page() 从有效旧项取得旧物理页，用
pte_create(page2ppn(page), perm) 构造新页表项。

使用 page_ref_inc()、page_ref_dec() 和 free_page() 管理物理页引用，
在写入或清零页表项后调用 tlb_invalidate(pgdir, la)。

struct Page、pde_t、pte_t 和上述接口均以工程定义为准。

```c
struct Page {
    int ref;                        // page frame's reference counter
    uint_t flags;                 // array of flags that describe the status of the page frame
    unsigned int property;          // the num of free block, used in first fit pm manager
    list_entry_t page_link;         // free list link
    list_entry_t pra_page_link;     // used for pra (page replace algorithm)
    uintptr_t pra_vaddr;            // used for pra (page replace algorithm)
};

pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create)
{
    pde_t *pdep1 = &pgdir[PDX1(la)];
    if (!(*pdep1 & PTE_V))
    {
        struct Page *page;
        if (!create || (page = alloc_page()) == NULL)
        {
            return NULL;
        }
        set_page_ref(page, 1);
        uintptr_t pa = page2pa(page);
        memset(KADDR(pa), 0, PGSIZE);
        *pdep1 = pte_create(page2ppn(page), PTE_U | PTE_V);
    }
    pde_t *pdep0 = &((pte_t *)KADDR(PDE_ADDR(*pdep1)))[PDX0(la)];
    if (!(*pdep0 & PTE_V))
    {
        struct Page *page;
        if (!create || (page = alloc_page()) == NULL)
        {
            return NULL;
        }
        set_page_ref(page, 1);
        uintptr_t pa = page2pa(page);
        memset(KADDR(pa), 0, PGSIZE);
        *pdep0 = pte_create(page2ppn(page), PTE_U | PTE_V);
    }
    return &((pte_t *)KADDR(PDE_ADDR(*pdep0)))[PTX(la)];
}

static inline pte_t pte_create(uintptr_t ppn, int type)
{
    return (ppn << PTE_PPN_SHIFT) | PTE_V | type;
}
```

可以在kern/mm/ 下的pmm.c,pmm.h,memlayout.h,mmu.h等文件中搜索可能用到但未列出的函数等

### [GUARANTEE]

实现：

```text
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm);(kern/mm/pmm.c)
void page_remove(pde_t *pgdir, uintptr_t la);(kern/mm/pmm.c)
```

整体实现链如下：
page_insert()
get_pte(pgdir, la, 1) → 失败则返回 -E_NO_MEM → 判断是新建映射、换页还是同页更新 → 更新引用计数（同页不变）→ 用 pte_create() 写入新 PTE → tlb_invalidate() → 旧页引用归零时释放 → 返回 0

page_remove()
get_pte(pgdir, la, 0) → 无有效映射则直接返回 → 用 pte2page() 取得旧页 → 清零 PTE → tlb_invalidate() → 减少旧页引用计数 → 归零时 free_page()

仅可根据需要在当前模块内增加内部辅助函数，不允许新建跨文件的新接口


### [SPECIFICATION]


#### page_insert()：

Pre-Condition：
pgdir 指向一个有效、可写的根页表并且page 指向物理页管理器中一个已分配的页描述符。

Post-Condition：

- case1：如果失败则返回 -E_NO_MEM且不能留下半完成映射。
- case2：成功时应让指定线性地址稳定映射到目标物理页，更新引用计数，并检查TLB的状态

Requirements：
保持“页表项内容、物理页引用计数、TLB 状态”三者同步


#### page_remove()：

Pre-Condition：pgdir 指向一个有效、可写的根页表， la是一个正确的虚拟地址

Post-Condition：

- case1：如果目标地址无映射就返回
- case2：成功删除映射，更新引用计数，并检查TLB的状态

Requirements：不应把“未映射地址”当作异常情况，不能假定目标地址一定存在有效映射。

## 功能模块二：进程创建及初始化功能

负责人：2413790 姜禧。


### [PROMPT]

实现进程创建及初始化功能，在kern/process/proc.c文件中实现alloc_proc，proc_init，kernel_thread，do_fork，copy_thread，copy_mm等函数，必须修改实际文件，不能仅展示代码。不修改代码中已有的接口和函数，以工程代码为准，不引入无关代码。

### [RELY]

- 以 proc.h 中的 struct proc_struct、struct context、PROC_UNINIT、
  PROC_RUNNABLE、MAX_PROCESS 为准。复用 proc.c 已有的 proc_list、
  hash_list、nr_process、idleproc、initproc 和 current。

- 用 kmalloc()、kfree() 管理 PCB。用已有的 setup_kstack() 和
  put_kstack() 分配和回收内核栈，栈大小使用 KSTACKSIZE。

- 用 list_init()、list_add() 管理链表，用 get_pid()、hash_proc()
  分配和登记 PID，用 wakeup_proc() 将新线程设为可运行。
  用 local_intr_save()、local_intr_restore() 保护 PID 分配和进程登记。

- idleproc 复用 bootstack。内核线程使用 boot_pgdir_pa，
  proc->mm 保持为 NULL。复用 set_proc_name() 和 init_main()。

- 按 trap.h 中的 struct trapframe 和 proc.h 中的 struct context
  准备首次运行现场。使用 read_csr(sstatus) 和 SSTATUS_SPP、
  SSTATUS_SPIE、SSTATUS_SIE 设置初始状态。

- 按 kernel_thread_entry 的汇编约定，将线程函数地址放入
  tf.gpr.s0，参数放入 tf.gpr.s1，入口地址写入 tf.epc。
  copy_thread() 将 trapframe 放在新线程的内核栈上，并令
  context.ra 指向 forkret，context.sp 指向该 trapframe。
  首次运行沿 forkret → forkrets → __trapret → kernel_thread_entry 执行。

- 使用已有的 CLONE_VM、E_NO_MEM 和 E_NO_FREE_PROC 定义。


### [GUARANTEE]

实现：

```c
static struct proc_struct *alloc_proc(void);
void proc_init(void);
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
static void copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf);
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc)；
```

整体流程如下：

```text
proc_init()
├→ idleproc = alloc_proc()
├→ 初始化 idleproc，设置 current = idleproc
├→ pid = kernel_thread()
│  └→ do_fork()
│     ├→ alloc_proc() → setup_kstack() → copy_mm() → copy_thread()
│     └→ get_pid() → hash_proc() → wakeup_proc()
└→ initproc = find_proc(pid)
```



### [SPECIFICATION]


#### *alloc_proc(void)

Pre-Condition：内核堆分配器已初始化。

Post-Condition：返回一个已完成基础字段初始化的 PCB

Requirements：alloc_proc 必须把 PCB 置于安全空白状态，避免后续代码误用未初始化字段；


#### proc_init(void)：

Pre-Condition：内存管理成功初始化

Post-Condition：

- case1：失败时调用panic（）
- case2：成功时保证进程管理全局结构、idleproc、current 和 initproc 都处于可用状态

Requirements：proc_init 中的 idleproc 并不是普通 fork 出来的线程，而是直接复用当前执行现场

#### kernel_thread（）：

Pre-Condition：成功初始化 idleproc

Post-Condition：把普通函数包装成可创建线程的上下文并返回新线程 PID；


#### do_fork():

Pre-Condition:成功初始化 idleproc,kernel_thread整理完毕上下文

Post-Condition：

- case1：失败是返回负错误码
- case2：成功时返回新线程的PID

Requirements：必须正确处理失败清理、PID 分配、链表挂接和唤醒顺序


#### copy_thread():

Pre-Condition：新线程的 PCB 和内核栈已准备好，tf 指向有效的trapframe。

Post-Condition：将 *tf 复制到新线程的内核栈上

Requirements：kernel_thread 与 copy_thread 必须配合 forkret、forkrets、__trapret 形成正确的首次运行链路；


#### copy_mm():

Pre-Condition：新的PCB已经初始化

Post-Condition：承担地址空间处理接口的占位作用






## 功能模块三：进程调度

负责人：2410250 张浩正。

```text
[PROMPT]
完成 Lab4 模块三，在 proc.c、sched.c 和 switch.S 中实现进程调度。
保留模块一、二和原评分测试，把模块三的 YOUR CODE 改为 2410250。
在 Ubuntu、QEMU 4.1.1 下验证，保存关键截图和原始日志。

[RELY]
以 proc_struct、context、proc_list、current 和 idleproc 的实际定义为准。
使用现有链表、中断保护和 TLB 接口。
pgdir 存放页表物理地址；satp 使用 Sv39 格式，参考 enable_paging，
不要使用遗留的 Sv32 lsatp。
新线程的初始现场已由模块二准备好。

[GUARANTEE]
实现 cpu_idle、schedule、proc_run 和 switch_to，保持函数签名。
只补齐模块三，不扩展抢占调度和进程退出功能。

[SPECIFICATION]
前置：进程结构和链表已初始化，目标线程的页表、内核栈与现场有效。
cpu_idle 持续检查 need_resched，有需要时调用 schedule。
schedule 在关中断的临界区清除重调度标记，从当前位置向后循环查找
PROC_RUNNABLE 线程；idle 从链表头查找，无可运行线程时选择 idle。
更新所选线程的 runs，需要切换时调用 proc_run。
proc_run 保存原中断状态，更新 current，写入 Sv39 satp 并刷新 TLB，
调用 switch_to，当前线程以后恢复执行时还原原中断状态。
switch_to 按 context 布局保存和恢复 ra、sp、s0—s11，
使旧线程以后能从切换调用处继续，新线程能进入既有首次运行链路。
后置：调度选择正确，页表与 current 一致，现场和中断状态能正确恢复。
检查普通启动、原评分，以及空闲回退和多次切换等边界情况。
```

## 模块三补充验证

负责人：2410250 张浩正。

```text
[PROMPT]
给模块三补充边界测试，在临时副本中运行，保留真实日志。

[RELY]
使用现有进程链表、内核线程接口和 QEMU 4.1.1。

[GUARANTEE]
保留原代码和评分测试，新增独立测试脚本。

[SPECIFICATION]
检查空链表、全部阻塞、仅当前线程可运行和循环选择。
用真实线程反复切换，检查上下文往返、寄存器哨兵、中断状态和页表。
超时、断言失败或缺少结果都判失败，成功后正常关机。
```

## 验证过程中的排查提示词

```text
[PROMPT]
普通启动通过，但 make grade 得到 0/30。检查 QEMU 与 GDB 的连接过程。

[RELY]
查看原评分脚本、GDB 错误和串口日志，继续使用 QEMU 4.1.1。

[GUARANTEE]
保留评分脚本和断言，只修复本地工具配置。

[SPECIFICATION]
确认评分需要的 GDB 命令存在，连接前选择正确的 RV64 架构。
重新运行原评分，记录退出码和实际分数。
```

```text
[PROMPT]
补测在 satp 断言处失败，先核对测试期望。

[RELY]
SATP_MODE_SV39 的值是 8，需要左移 60 位；pgdir 是页表物理地址。

[GUARANTEE]
按实际定义修正测试，保留首次失败日志。

[SPECIFICATION]
核对模式位和根页表页号，重跑完整补测，不能放宽断言。
```
