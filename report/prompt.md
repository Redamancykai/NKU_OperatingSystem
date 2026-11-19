# 功能模块一：虚拟内存页操作功能
### 负责人：黄泽恺 2411268

[PROMPT]

**任务**  
在 WSL Ubuntu-22.04 的 `/home/zekai/projects/OS_labs/code` 中完成 Lab4 功能模块一。实现 Sv39 页表中单个 4 KiB 页的映射建立、替换和撤销，使末级页表项、物理页引用计数和 TLB 保持一致。`pmm_init()` 会在进程初始化前调用 `check_pgdir()` 和 `check_boot_pgdir()` 验证这些操作。

**操作要求**  
直接编辑实际的 `kern/mm/pmm.c`，在 `get_page()` 与 `tlb_invalidate()` 之间的 `//LAB4 function1 2411268` 标记下补全代码。保留标记和用户已有改动，修改范围限于该处。沿用现有页表遍历和物理页分配接口，保留框架自检、评分脚本和公开函数声明。

**输出要求**  
完成后说明修改位置，并报告实际执行的编译或测试及其结果。优先用项目的 Makefile 单独编译 `pmm.c`。完整内核可构建时，在 QEMU 中检查两项启动自检，并用评分用例核对 `[Test PMM] PASS`。遇到 `kmalloc.c` 或后续模块导致的构建阻塞时，记录具体报错和未执行的检查，保持本模块的修改范围。

[RELY]

- `kern/mm/mmu.h` 提供 Sv39 页表索引 `PDX1(la)`、`PDX0(la)`、`PTX(la)`，以及 `PGSIZE`（4096 字节）和 `PTE_V`、`PTE_R`、`PTE_W`、`PTE_X`、`PTE_U` 等标志。`pde_t` 和 `pte_t` 在 `kern/mm/memlayout.h` 中定义为 `uintptr_t`。
- `kern/mm/pmm.c` 已有 `get_pte(pde_t *pgdir, uintptr_t la, bool create)`。它返回末级 PTE 的地址，返回的 PTE 内容可能无效。`create = false` 只查找，`create = true` 可分配中间级页表页。当前实现可能在挂接第一张中间级页表后，因下一张分配失败而返回 `NULL`。
- `kern/mm/pmm.h` 提供 `page2ppn(page)`、`pte_create(ppn, type)`、`pte2page(pte)` 和 `pde2page(pde)`。`pte_create()` 会设置 `PTE_V`，`pte2page()` 要求输入是有效 PTE。
- 同一头文件提供 `page_ref_inc(page)`、`page_ref_dec(page)` 和 `free_page(page)`。前两个函数返回更新后的引用计数。`pmm.c` 提供 `tlb_invalidate(pgdir, la)`，`pmm.h` 提供 `flush_tlb()`。内存不足的错误码使用 `libs/error.h` 中的 `E_NO_MEM`。
- `tools/test_lab4.c` 的 `test_lab4_pmm_basic()` 检查建映射、换页、同页重插和重复删除。框架用 `perm = 0`、`PTE_W`、`PTE_U | PTE_W` 检查软件层的 PTE 行为，实际读写验证使用 `PTE_R | PTE_W`。

[GUARANTEE]

实现以下两个已在 `kern/mm/pmm.h` 声明的接口：

```c
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm);
void page_remove(pde_t *pgdir, uintptr_t la);
```

如需复用清理逻辑，可在同一标记处添加 `static` 辅助函数。

[SPECIFICATION]

### `page_insert()`

**Pre-Condition**  
物理页分配器已初始化。`pgdir` 指向可访问的 Sv39 根页表，`page` 是已分配且由物理页管理器管理的页描述符。`la` 可带页内偏移，操作目标是它所在的 4 KiB 页。已有映射属于由引用计数管理的末级映射。

**Post-Condition**  
成功返回 `0`。目标 PTE 指向 `page`，设置 `PTE_V`，权限按本次 `perm` 设置。旧映射独有的权限随更新清除。同一物理页通过多个虚拟地址映射时，每个映射各占一次引用。

- **Case 1 · 目标无有效映射**  
  建立映射，目标物理页的引用计数增加 `1`。
- **Case 2 · 已映射到同一物理页**  
  更新权限，引用计数和空闲页数保持原值，页始终处于已分配状态。
- **Case 3 · 已映射到另一物理页**  
  建立新映射，新页引用计数增加 `1`，旧页引用计数减少 `1`。旧页的最后一个引用消失时，将它归还给物理页分配器。
- **Case 4 · 无法分配所需页表页**  
  返回 `-E_NO_MEM`。目标页仍由调用者持有，引用计数和已有映射保持原值。如果这次调用只建成了部分页表路径，撤销本次新增的目录项并回收对应页表页，保留调用前就存在的目录项。

**Requirements**  
保留框架对 `perm` 的原有约定，按传入的标志位构造 PTE。更新叶项后同步对应的 TLB，涉及中间级目录项的变动时使用全量刷新。旧页回到空闲链表前，应先让旧映射失效并完成 TLB 同步。

### `page_remove()`

**Pre-Condition**  
物理页分配器已初始化，`pgdir` 指向可访问的 Sv39 根页表。`la` 所在页可以有映射，也可以从未建立映射。

**Post-Condition**  
调用后该地址无有效映射。对同一地址重复调用时，物理页引用计数和空闲页数保持稳定。

- **Case 1 · 页表路径不存在，或末级 PTE 无效**  
  直接返回。查找过程不分配页表页，也不修改引用计数。
- **Case 2 · 末级 PTE 有效**  
  取得所映射的原物理页，清零末级 PTE，并使对应 TLB 失效。原页引用计数减少 `1`，归零时释放；还有其他映射时继续保留该页。

**Requirements**  
普通删除只撤销末级映射，中间级页表页继续由调用方管理。现有 `check_pgdir()` 会在删除映射后检查页表页引用计数，再自行回收这些页表页。

# 功能模块三：进程调度与上下文切换
### 负责人：郑奕杰 2412677

[PROMPT]

**任务**

在 RISC-V 64 位 uCore Lab 4 中实现内核线程调度。系统启动后，空闲线程响应调度请求，将 CPU 交给就绪的 `initproc`，使它进入 `init_main()`。普通线程主动让出 CPU 后可以从原调用点继续执行。普通线程均不可运行时，CPU 回到空闲线程。

**操作要求**

直接编辑项目中的实际文件。在 `code/kern/process/proc.c` 补全 `cpu_idle()` 和 `proc_run()`，在 `code/kern/schedule/sched.c` 补全 `schedule()`，在 `code/kern/process/switch.S` 补全 `switch_to()`。沿用框架已有的进程链表、中断接口和现场恢复路径。在各文件的 `LAB4 function3` 标记处补全实现，将三个 `YOUR CODE` 替换为学号 `2412677`。

**输出要求**

依据下列接口与行为规格交付源码。使用项目 Makefile 分别编译三个目标文件，反汇编核对 `switch_to()` 的寄存器与偏移。整核验收检查 QEMU 中的 `init_main()` 输出、线程轮转、空闲回退和中断状态，并用 `make grade` 核对 `[Test Schedule] PASS`。交付时列出修改位置、实际运行的命令和观察结果。

[RELY]

- `code/kern/process/proc.h` 定义 `struct proc_struct`。调度使用 `state`、`runs`、`need_resched` 和 `list_link`，切换使用 `context`、`pgdir` 和 `tf`。其中 `pgdir` 保存根页表物理地址。候选线程的就绪状态为 `PROC_RUNNABLE`。
- `proc_list` 是带哨兵的循环双向链表，保存普通线程。使用 `list_next()` 遍历，用 `le2proc(entry, list_link)` 从普通链表节点取得进程控制块。`current` 指向当前线程，`idleproc` 指向空闲线程。
- `struct context` 的字段依次为 `ra`、`sp`、`s0` 到 `s11`，类型均为 `uintptr_t`。在 RV64 上每项占 8 字节。`code/libs/riscv.h` 提供汇编使用的 `STORE`、`LOAD` 和 `REGBYTES`。
- `code/kern/sync/sync.h` 提供 `local_intr_save(flag)` 和 `local_intr_restore(flag)`，通过 `sstatus.SIE` 保存进入临界区时的中断状态。`code/libs/riscv.h` 提供 `write_csr()`、`SATP_MODE_SV39` 和 `RISCV_PGSHIFT`。Sv39 的模式字段位于 `satp[63:60]`，根页表物理地址转换为页号后写入 PPN 字段。`code/kern/mm/pmm.c` 中的 `enable_paging()` 给出了现有编码方式，`pmm.h` 提供 `flush_tlb()`。
- 进程初始化完成后，`current == idleproc`，`idleproc->need_resched == 1`，`initproc` 已登记且处于就绪状态。新线程的 `context.ra` 指向 `forkret`，`context.sp` 指向内核栈上的 `trapframe`。框架已有 `forkret → forkrets → __trapret → sret` 路径。中断帧中的 `epc` 指向 `kernel_thread_entry`，`s0`、`s1` 分别保存线程函数与参数，`status` 提供首次返回时的特权级和中断状态。

[GUARANTEE]

实现下列四个接口，沿用框架中的签名及链接符号。

```c
void cpu_idle(void);
void schedule(void);
void proc_run(struct proc_struct *proc);
void switch_to(struct context *from, struct context *to);
```

[SPECIFICATION]

### `cpu_idle()`

**Pre-Condition**

内核初始化已进入空闲线程，`current == idleproc`，调度所需的进程状态和链表已就绪。

**Post-Condition**

函数持续承担空闲线程的执行循环。每次恢复到空闲线程时，继续检查当前的重调度请求。

- **Case 1 · 有调度请求**。`need_resched` 置位时调用 `schedule()`，由调度器选择下一执行线程。
- **Case 2 · 暂无调度请求**。保持空闲循环，等待下一次请求。

### `schedule()`

**Pre-Condition**

当前线程有效。普通线程位于已初始化的 `proc_list` 中，空闲线程由 `idleproc` 单独持有。

**Post-Condition**

本次调度清除发起线程的 `need_resched`，为选中线程的 `runs` 增加 1，并在目标发生变化时切换执行。发起线程从本次调用返回时，其本地中断使能状态恢复到调用前的值。

- **Case 1 · 存在可切换的普通就绪线程**。当前为空闲线程时从表头找起。当前为普通线程时从其后继找起，越过表尾后继续从表头查找。选择遇到的第一个就绪线程。
- **Case 2 · 当前普通线程是唯一就绪线程**。完成一轮查找后继续运行当前线程，本次调度仍计入其 `runs`。
- **Case 3 · 普通线程均不可运行**。选中 `idleproc`。如果当前已在 idle，则继续执行空闲循环。

**Requirements**

在本地中断保护下读取进程状态、确定调度目标和切换。链表哨兵只用于界定遍历，调度候选来自就绪线程和空闲线程。

### `proc_run(proc)`

**Pre-Condition**

`proc` 指向调度器选中的线程。当前线程与目标线程均有有效的控制块，目标线程具有可用的 Sv39 根页表和可恢复的上下文。

**Post-Condition**

目标线程接管 CPU 时，`current`、活动页表和 CPU 执行现场都对应目标线程。原线程的执行现场可在以后恢复。

- **Case 1 · 目标就是当前线程**。直接返回，保留当前页表、上下文和中断状态。
- **Case 2 · 目标为另一线程**。切换到目标的 Sv39 页表并同步 TLB。目标线程在其保存的现场开始或继续执行，原线程在以后恢复时接续本次调用。

**Requirements**

更新 `current`、激活页表和交接上下文的过程受本地中断保护。原线程恢复后，按它在本次调用进入临界区前保存的状态恢复中断。首次运行线程的中断状态由其 `trapframe` 在 `sret` 时恢复。

### `switch_to(from, to)`

**Pre-Condition**

`from` 指向可写的旧线程上下文，`to` 指向已准备好的目标上下文。两个参数按 RISC-V 调用约定由 `a0`、`a1` 传入，调用方已保护切换临界区。

**Post-Condition**

旧线程的 `ra`、`sp` 和 `s0–s11` 保存在 `from` 中，CPU 中对应寄存器来自 `to`。执行流从恢复后的 `ra` 继续，已运行过的线程回到原调用点，新线程进入预设入口。

**Requirements**

保存和恢复的槽位与 `struct context` 布局一致。在 RV64 上，14 个寄存器槽位的偏移依次为 0、8、16 字节，直至 104 字节。

**说明**

本节依据开发方案和实际实现整理。正式改动限于三处 `function3`，进程创建、现有 Lab 3 中断路径和评分脚本沿用团队代码，远端提交由本人审查。当前独立 QEMU 验证使用临时线程和启动条件，整核联调与正式评分以成员 B 接入并确认启动前置条件后的结果为准。
