# Lab2 实验提示词

模块一负责人：2413790 姜禧；模块二负责人：2410250 张浩正。以下按功能模块顺序汇总提示词。

## 功能模块一：物理内存初始化

负责人：2413790 姜禧。

[PROMPT]

实现物理内存探测与页描述符初始化功能，在 `kern/mm/pmm.c` 中自行设计并实现 `static void page_init(void)`。

必须修改实际文件，不能仅展示代码。不修改代码中已有的接口和函数，不修改 `pmm.h` 等头文件已有的接口和结构体。以工程代码为准，不引入无关代码。

明确区分虚拟地址、物理地址、物理页号和 Page 描述符地址；正确处理可管理上界和页面对齐，确保内核及描述符数组不会进入空闲区。设备树信息无效时终止初始化，没有可用完整页面时不得调用 `init_memmap()`。

[RELY]

使用工程中已有的物理内存管理器接口、设备树内存查询接口、管理器初始化逻辑和页描述符结构。原提示词提供的依赖如下，以实际工程定义为准：

```c
struct pmm_manager {
    const char *name;  // XXX_pmm_manager's name
    void (*init)(
        void);  // initialize internal description&management data structure
                // (free block list, number of free block) of XXX_pmm_manager
    void (*init_memmap)(
        struct Page *base,
        size_t n);  // setup description&management data structcure according to
                    // the initial free physical memory space
    struct Page *(*alloc_pages)(
        size_t n);  // allocate >=n pages, depend on the allocation algorithm
    void (*free_pages)(struct Page *base, size_t n);  // free >=n pages with
                                                      // "base" addr of Page
                                                      // descriptor
                                                      // structures(memlayout.h)
    size_t (*nr_free_pages)(void);  // return the number of free pages
    void (*check)(void);            // check the correctness of XXX_pmm_manager
};
// kern/driver/dtb.h 中已有的声明
uint64_t get_memory_base(void);
uint64_t get_memory_size(void);

static void init_pmm_manager(void) {
    pmm_manager = &default_pmm_manager;
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}
struct Page {
    int ref;                        // page frame's reference counter
    uint64_t flags;                 // array of flags that describe the status of the page frame
    unsigned int property;          // the num of free block, used in first fit pm manager
    list_entry_t page_link;         // free list link
};
```

[GUARANTEE]

实现：

```c
static void page_init(void);
```

使内核能够识别、组织并将可用物理内存交给当前管理器。整体实现链为：

```text
pmm_init()
  → init_pmm_manager()
  → page_init()
  → init_memmap()
  → 后续 alloc_pages() / free_pages()
```

仅可根据需要在当前模块内增加内部辅助函数，不新建跨文件接口。

[SPECIFICATION]

Pre-Condition：`init_pmm_manager()` 已选择并初始化具体的物理内存管理器。

Post-Condition：

- 成功时，正确设置 `va_pa_offset`、`npage` 和 `pages`，输出探测到的物理内存范围，并完成当前管理器初始空闲区的注册。
- 设备树信息无效时，终止初始化。

Requirements：没有可用完整页面时，不得调用 `init_memmap()`。

## 功能模块二：物理内存分配

负责人：2410250 张浩正。依次列出 Best-Fit、Buddy 实现及 Buddy 独立测试提示词；First-Fit 的分析要求包含在 Best-Fit 提示词中。

### 1. Best-Fit

[PROMPT]

请完成 Lab2 的 Best-Fit 页分配器，直接修改 `kern/mm/best_fit_pmm.c`，并在 `pmm.c` 中启用它。参考 `default_pmm.c`，保留两个已有测试函数。保留 lab2 注释，把本人代码处的 YOUR CODE 改为 2410250。不要改同学的 `page_init()`。

完成后在 Ubuntu 编译运行，检查原有测试和 `make grade`，保存关键截图及原始日志。说明 First-Fit 的初始化、分配、回收与合并过程，以及两种算法的改进空间。

[RELY]

以工程中的 `pmm.h`、`memlayout.h`、`list.h` 为准。`struct Page` 含 `ref`、`flags`、`property`、`page_link`；`free_area_t` 含 `free_list` 和 `nr_free`。空闲块首页设置 `PG_property`，`property` 记录块的页数，`nr_free` 记录空闲页总数。可使用现有页状态宏、引用计数和链表函数。

已有测试会直接保存、清空和恢复 `free_list`、`nr_free`，也会释放分配区的一部分或多个相邻已分配区。实现需要兼容这些操作。参考代码的链表尾插分支需要检查退出条件。

[GUARANTEE]

实现以下函数，可添加 static 辅助函数：

```c
static void best_fit_init(void);
static void best_fit_init_memmap(struct Page *base, size_t n);
static struct Page *best_fit_alloc_pages(size_t n);
static void best_fit_free_pages(struct Page *base, size_t n);
static size_t best_fit_nr_free_pages(void);
static void init_pmm_manager(void);
```

定义 `best_fit_pmm_manager`，将 `.check` 连接原有 `best_fit_check()`。修改 `init_pmm_manager()` 的选择。`basic_check()` 和 `best_fit_check()` 保持原样。

[SPECIFICATION]

- `best_fit_init`：Pre-Condition：管理器首次初始化。Post-Condition：空闲链表为空，空闲页数为 0。
- `best_fit_init_memmap`：Pre-Condition：n>0，连续描述符有效，页面均 Reserved 且未注册。Post-Condition：将这 n 页加入空闲区，清零引用计数，正确设置块首页和长度，保持地址有序并合并相邻区域，不包含其他保留页。
- `best_fit_alloc_pages`：Pre-Condition：n>0，管理器已初始化。Post-Condition：选择能够容纳 n 页的最小空闲块，同大小取低地址；返回连续 n 页，必要时保留余块，计数减少 n。找不到连续块时返回 NULL，状态不变。空链表必须分配失败。
- `best_fit_free_pages`：Pre-Condition：n>0，区间有效且全部已分配、非保留页，不重复释放。Post-Condition：回收 n 页，计数增加 n，合并前后相邻空闲块，不重复计数或破坏链表。支持上述测试中的部分释放和合并释放。
- `best_fit_nr_free_pages`：Pre-Condition：管理器已初始化。Post-Condition：返回空闲页总数，不改变状态。
- `init_pmm_manager`：Pre-Condition：开始初始化物理内存管理。Post-Condition：默认选中 Best-Fit 并调用其初始化函数。

通过真实断言检查最小适配、拆分、合并、资源不足和计数恢复。测试失败时保留输出，修复原因后重跑，不修改评分标准。

### 2. Buddy System

[PROMPT]

请新增 `kern/mm/buddy_pmm.c` 和 `buddy_pmm.h`，实现并接入 Buddy 页分配器。沿用现有管理器接口，不修改 `struct Page` 和同学的 `page_init()`。默认保留 Best-Fit，通过 `PMM=buddy` 选择 Buddy。切换配置时干净构建。

本次先完成算法和基本状态检查，完整测试在下一份提示词中补充。保存实际构建、启动结果，代码注明学号 2410250。

[RELY]

参考真实的 `pmm.h`、`memlayout.h`、`list.h`。页面大小为 PGSIZE，页地址转换使用工程提供的函数。初始化交付的可用区不一定按最大块对齐，长度也不一定是 2 的幂。

本模块约定：申请 n 页时，实际分配向上取整到 2 的幂的 m 页；释放时传入原始 base 和原始 n，整块归还 m 页。空闲页计数按实际占用的 m 页增减。保留必要的分配元信息，检查原始地址和请求大小。本接口不支持分配子区间的单独释放。

[GUARANTEE]

实现：

```c
static void buddy_init(void);
static void buddy_init_memmap(struct Page *base, size_t n);
static struct Page *buddy_alloc_pages(size_t n);
static void buddy_free_pages(struct Page *base, size_t n);
static size_t buddy_nr_free_pages(void);
static void buddy_check(void);
void buddy_validate(void);
```

定义 `buddy_pmm_manager`，头文件提供声明。允许 static 辅助函数和私有状态，另提供 `buddy_validate()` 供独立测试核对内部状态。接入构建配置和 `init_pmm_manager()`，保持默认评分使用 Best-Fit。

[SPECIFICATION]

- `buddy_init`：Pre-Condition：首次建立，或独占启动测试中已释放所有活动分配后的受控重置。Post-Condition：各阶空闲结构为空，计数和私有状态完成初始化。
- `buddy_init_memmap`：Pre-Condition：n>0，区间有效、连续、Reserved 且未注册。Post-Condition：把全部 n 页划分为对齐的幂次块，不遗漏零头，不越界，不包含保留区。伙伴计算使用一致的物理页号或区域偏移。
- `buddy_alloc_pages`：Pre-Condition：n>0，管理器已初始化。Post-Condition：成功时返回对齐、无重叠的 m 页块，必要时逐级拆分，记录原始请求，空闲数减少 m；请求过大或没有合适块时返回 NULL，状态不变。大小计算避免溢出。
- `buddy_free_pages`：Pre-Condition：base 和 n 对应尚未释放的原始分配。Post-Condition：归还 m 页，只合并同阶、空闲且真正互为伙伴的块；相邻非伙伴不合并。错误地址、重复释放和请求不匹配应在修改状态前被拒绝或触发断言。
- `buddy_nr_free_pages`：Pre-Condition：已初始化。Post-Condition：只读返回实际空闲页数。
- `buddy_check`：Pre-Condition：可用区已注册。Post-Condition：检查块大小、对齐、范围、空闲状态和总数一致性，保持有效状态；随后接入独立测试。
- `buddy_validate`：Pre-Condition：管理器已初始化，当前状态稳定。Post-Condition：只读检查块大小、对齐、范围、覆盖、伙伴合并和计数一致性；不改变分配状态。

拆分和合并不能额外改变空闲页总数。代码和注释说明非幂次请求的内部碎片及释放约定。

### 3. Buddy 独立测试

[PROMPT]

请阅读当前 Buddy 实现，在 `kern/mm/buddy_pmm_test.c` 补充独立测试，通过 `buddy_pmm.c` 和 `buddy_pmm.h` 接入。保留 Best-Fit 的原有测试和评分标准。测试发现问题时，保存失败输出，修复后重跑。

完成后分别运行 Buddy 专项测试和默认 Best-Fit 的回归评分。保存关键截图和日志，给出测试覆盖与结果。

[RELY]

使用实际实现的 Buddy 接口。申请 n 页实际占用向上取整后的 m 页，释放使用原始 base 和 n。`nr_free_pages()` 统计实际空闲页数。默认管理器是 Best-Fit，`PMM=buddy` 选择 Buddy；`make grade` 不包含 Buddy 测试。

优先通过现有管理器接口测试，可以使用 assert、cprintf 和页地址转换。初始化边界测试使用真实 Page 描述符及受控空闲区域，测试后恢复状态。

[GUARANTEE]

实现 `void buddy_run_tests(void)`，由 `buddy_check()` 调用；需要时添加 static 测试函数。各场景有明确断言，通过后输出简短结果。

[SPECIFICATION]

Pre-Condition：当前启用 Buddy，物理内存初始化完成，测试独占启动阶段，无其他调用者的活动分配，并存在连续 16 页对齐空闲区间。记录初始空闲页数。

Post-Condition：以下行为经过实际断言验证，测试分配全部回收，最终空闲页数和内部状态恢复。

1. 单页、幂次大小以及 3 页请求的分配和释放正确；3 页请求实际扣除 4 页。
2. 非幂次长度、非最大阶对齐起点的可用区无丢页、越界或重复注册。
3. 大块可逐级拆分，活动分配范围不重叠且满足对齐。
4. 伙伴释放后可逐级合并并重新分配大块；相邻非伙伴不能误合并。
5. 耗尽或过大请求失败，计数与已有分配保持不变。
6. 改变释放顺序后仍能恢复容量，恢复不能仅靠重置计数。

Requirements：先干净构建并运行 Buddy，再切回默认 Best-Fit 完成评分。每个 PASS 对应真实断言，不用固定输出代替测试。截图保留命令、管理器名称和关键结果；如实记录实际发生的问题。

