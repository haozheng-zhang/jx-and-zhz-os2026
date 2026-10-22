# Lab2 实验提示词

模块二负责人：2410250 张浩正。以下汇总本次提示词及调试后确认的接口约定。

测试图片由实际运行日志节选排版生成。

## 模块一提示词（待姜禧补充）

负责人：2413790 姜禧。请在此补充物理内存初始化模块实际使用的提示词，保留 `[PROMPT]`、`[RELY]`、`[GUARANTEE]`、`[SPECIFICATION]` 四段及必要迭代记录。

## 1. Best-Fit

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

## 2. Buddy System

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

## 3. Buddy 独立测试

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

## 4. 启动兼容修正

[PROMPT]

当前 Ubuntu 中，旧 loader 命令让 OpenSBI 的 Next Address 为 0。改用 -kernel 后能够到达内核入口，但 GDB 显示开启 satp 后无法读取低地址的下一条指令。请修正启动链路，保留现有算法、测试和同学的 page_init()。

[RELY]

本机使用 QEMU 8.2.2、OpenSBI 1.3 和 gdb-multiarch。入口先在 0x80200000 执行，再开启 Sv39 并跳到高地址；原启动页表只设置第 511 项。Makefile 和 tools/grade.sh 各有 QEMU 启动命令。

[GUARANTEE]

调整 Makefile 与评分脚本中的启动/调试命令，并修正 entry.S 的启动页表过渡。评分脚本退出时回收本次启动的 QEMU 进程。保留评分断言和预期输出。

[SPECIFICATION]

Pre-Condition：日志已复现上述启动现象。

Post-Condition：QEMU 向固件传入正确内核入口，分页开启后的低地址指令可以继续执行，随后进入高地址 kern_init；正常启动、调试和评分使用一致配置。

Requirements：仅补上必要的启动映射和工具适配，验证实际输出及检查结果，保存修复前后的日志。不要改变模块一实现或评分通过条件。
