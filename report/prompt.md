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

# 功能模块二：进程创建与初始化
### 负责人：黄子恒 2412414

### `alloc_proc()`

[PROMPT]

**任务**

为 RISC-V uCore Lab4 实现 `alloc_proc()`。它为 `proc_init()` 和 `do_fork()` 提供一个已完成基础初始化、尚未登记的进程控制块（PCB）。

**操作要求**

直接编辑 `code/kern/process/proc.c`，在 `LAB4 function2` 标记下实现 `alloc_proc()`。保留该标记，将 `YOUR CODE` 替换为 `2412414`。本轮修改限于该函数和对应标记。

**输出要求**

交付源码，并说明修改位置、实际执行的验证命令和结果。

- **编译检查**。在 `code/` 下运行 `make obj/kern/process/proc.o`，核对函数签名、类型和依赖。
- **分配成功**。按规格逐项核对 PCB 初值和未登记状态。有独立测试条件时，使用预先填充过非零数据的分配内存，验证初值来自本函数。
- **分配失败**。有独立测试条件时，让 `kmalloc()` 返回 `NULL`，检查 `alloc_proc()` 是否也返回 `NULL`。如果当前只能读代码核对这条路径，在结果中标明验证方式。
- **整核联调**。模块二接齐后运行 `make grade`，由 `test_lab4_process_create()` 检查启动后的线程状态。

[RELY]

- `code/kern/process/proc.h` 定义 `struct proc_struct`、`struct context`、`PROC_UNINIT` 和 `PROC_NAME_LEN`。其中 `pgdir` 的类型为 `uintptr_t`，`name` 的长度为 `PROC_NAME_LEN + 1`。
- `code/kern/mm/kmalloc.h` 提供 `void *kmalloc(size_t n)`。参数单位为字节，分配失败时返回 `NULL`。如需清零结构体或字段，可使用 `code/libs/string.h` 中的 `memset()`。
- `code/kern/mm/pmm.h` 声明 `boot_pgdir_pa`。它保存启动根页表的物理地址，由 `pmm_init()` 在进程初始化前设置。`pmm_init()` 同期初始化内核内存分配器。
- PCB 中的 `list_link` 和 `hash_link` 分别用于后续登记到 `proc_list` 和 PID 哈希表。`code/libs/list.h` 提供后续登记使用的链表接口。

[GUARANTEE]

在 `proc.c` 内实现以下内部函数，供同文件中的进程初始化与创建逻辑调用。

```c
static struct proc_struct *alloc_proc(void);
```

[SPECIFICATION]

**Pre-Condition**

内核内存分配器和 `boot_pgdir_pa` 已初始化。

**Post-Condition**

- **Case 1 · 分配成功。** 返回指向新分配的一个 PCB 的指针，并满足以下初始状态。
  - `state == PROC_UNINIT`，`pid == -1`。
  - `runs`、`kstack`、`need_resched` 和 `flags` 均为零。
  - `parent`、`mm` 和 `tf` 均为 `NULL`。
  - `context` 中所有寄存器字段均为零，`name` 数组全部清零。
  - `pgdir == boot_pgdir_pa`，两个链表节点均处于未登记状态。
- **Case 2 · 分配失败。** 直接返回 `NULL`，由调用方处理失败。初始化操作只作用于成功分配的 PCB。

**Requirements**

本函数的职责限于分配和初始化一个 PCB。实际 PID 的分配、内核栈设置、进程链表与 PID 哈希表登记，以及 `nr_process` 更新，都由调用方在后续阶段处理。

### `copy_mm()`

[PROMPT]

**任务**

为 RISC-V uCore Lab4 实现 `copy_mm()`，供后续的 `do_fork()` 准备新建线程的地址空间。本实验的父子内核线程共享启动内核页表。

**操作要求**

在 `code/kern/process/proc.c` 的 `LAB4 function2 2412414` 区块补写 `copy_mm()`。本轮的源码改动限于这个函数。

**输出要求**

在 `code/` 下运行 `make obj/kern/process/proc.o`。检查 `clone_flags` 分别取 `0` 和 `CLONE_VM` 时的返回值，以及父、子线程的 `mm` 和 `pgdir`。报告实际执行的编译和检查结果。

[RELY]

- `code/kern/process/proc.h` 定义的 `mm` 指向用户地址空间的管理结构，`pgdir` 是根页表的物理地址，类型为 `uintptr_t`。
- `code/kern/mm/pmm.h` 声明启动根页表物理地址 `boot_pgdir_pa`，并定义共享地址空间标志 `CLONE_VM`。
- 现有 `alloc_proc()` 为内核线程初始化 `mm = NULL` 和 `pgdir = boot_pgdir_pa`。

[GUARANTEE]

```c
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc);
```

[SPECIFICATION]

**Pre-Condition**

`current` 指向有效的父内核线程，`proc` 指向已完成基础初始化的新内核线程 PCB。两者的 `mm` 均为 `NULL`，`pgdir` 均为 `boot_pgdir_pa`。

**Post-Condition**

返回 `0`。父、子线程的 `mm` 均为 `NULL`，且继续使用由 `boot_pgdir_pa` 指定的内核页表。这个结果适用于本实验中带或不带 `CLONE_VM` 的内核线程创建请求。

**Requirements**

复用已建立的内核页表，额外分配的地址空间资源为零。保留接口参数，供后续实验扩展用户地址空间处理。

### `copy_thread()`

[PROMPT]

**任务**

为 RISC-V uCore Lab4 实现 `copy_thread()`。它根据调用方提供的中断帧，为新线程建立独立的帧副本和首次切换使用的上下文。

**操作要求**

在 `code/kern/process/proc.c` 的 `LAB4 function2 2412414` 区块补写 `copy_thread()`。本轮的源码改动限于这个函数，沿用现有的上下文切换和中断恢复接口。

**输出要求**

在 `code/` 下运行 `make obj/kern/process/proc.o`。用包含非零寄存器和状态值的输入帧分别检查 `esp == 0` 和 `esp != 0` 两个分支。验证帧地址、子线程返回值、两个栈指针、寄存器复制结果和输入帧内容。报告实际执行的检查。首次切换的 QEMU 验证在模块二接齐后执行。

[RELY]

- `code/kern/mm/memlayout.h` 定义 `KSTACKSIZE`。`setup_kstack()` 成功后，`proc->kstack` 是新线程内核栈的低地址。
- `code/kern/trap/trap.h` 定义 `struct trapframe` 及其中的 `gpr.a0`、`gpr.sp`、`epc` 和 `status`。`code/kern/process/proc.h` 定义 `proc->tf` 和包含 `ra`、`sp` 的 `struct context`。
- `code/kern/process/switch.S` 从 `context` 恢复 `ra` 和 `sp`。`forkret()` 通过 `forkrets(current->tf)` 进入 `code/kern/trap/trapentry.S` 的恢复路径，该路径在 `sret` 前从中断帧恢复 `gpr.sp`。

[GUARANTEE]

```c
static void copy_thread(struct proc_struct *proc, uintptr_t esp,
                        struct trapframe *tf);
```

[SPECIFICATION]

**Pre-Condition**

`proc` 指向已取得内核栈的新线程 PCB。`tf` 指向独立于新内核栈的可读中断帧。`esp` 为零时使用新内核栈，非零时由调用方提供有效且对齐的恢复栈指针。

**Post-Condition**

`proc->tf` 指向内核栈高端的完整帧副本。副本的 `gpr.a0` 为 `0`，表示子线程的返回值。除 `gpr.a0` 和 `gpr.sp` 外，帧内容与输入一致。`context.ra` 指向 `forkret`，`context.sp` 指向帧副本。

- **Case 1 · `esp == 0`。** `gpr.sp` 使用帧副本起始地址，恢复后线程可以在其下方使用向低地址增长的内核栈。
- **Case 2 · `esp != 0`。** `gpr.sp` 使用 `esp`。用于首次进入 `forkret` 的 `context.sp` 仍指向内核栈上的帧副本。

**Requirements**

帧副本应完整位于新内核栈内，并使首次进入 C 函数时的栈指针满足 RV64 的 16 字节对齐要求。寄存器字段的调整仅作用于副本，调用方提供的帧保持原值。

### `do_fork()`

[PROMPT]

**任务**

为 RISC-V uCore Lab4 实现 `do_fork()`。它根据调用方提供的中断帧创建内核线程，为其准备 PCB、内核栈和首次运行现场，再登记并唤醒线程。调用方可以用返回的 PID 查找创建成功的线程。

**操作要求**

在 `code/kern/process/proc.c` 的 `LAB4 function2 2412414` 区块补写 `do_fork()`，复用已完成的 `alloc_proc()`、`copy_mm()`、`copy_thread()` 和框架的进程管理接口。本轮的源码改动限于 `do_fork()`。

**输出要求**

- 在 `code/` 下运行 `make obj/kern/process/proc.o`，交付源码并记录实际执行的命令和结果。
- 成功时核对正 PID、`find_proc(pid)`、普通进程链表中的成员身份、`nr_process` 的增量、父线程和就绪状态，并确认内核栈和首次运行现场已准备好。
- 有独立测试条件时，分别模拟达到进程上限、PCB 分配失败、内核栈分配失败，以及 `copy_mm()` 返回错误。核对返回值、各项资源实际释放次数和失败前后的登记状态。当前 `copy_mm()` 对内核线程返回 `0`，地址空间失败路径可用临时替身触发。报告每项是实际运行还是通过读代码核对。
- 在模块二接齐后用 `make grade` 验证完整的创建与调度流程。

[RELY]

- `code/kern/process/proc.h` 声明 `do_fork()`，定义 `MAX_PROCESS`、`MAX_PID`、进程状态和 PCB 字段。模块内的 `nr_process` 记录已有进程数，`current` 指向当前线程。
- `alloc_proc()` 返回尚未登记的 PCB，失败时返回 `NULL`。`setup_kstack(proc)` 成功时设置 `proc->kstack` 并返回 `0`，失败时返回 `-E_NO_MEM`。已成功分配的栈由 `put_kstack(proc)` 回收，PCB 由 `code/kern/mm/kmalloc.h` 声明的 `kfree()` 回收。
- `copy_mm(clone_flags, proc)` 处理当前实验的共享地址空间，成功返回 `0`。`copy_thread(proc, stack, tf)` 使用已有内核栈准备中断帧副本和首次上下文，`stack == 0` 时使用子线程的内核栈。
- `get_pid()` 分配正 PID，`hash_proc(proc)` 登记 PID 哈希表，`list_add()` 将 `list_link` 加入 `proc_list`，`find_proc(pid)` 可查到已登记的线程。
- `code/kern/sync/sync.h` 提供 `local_intr_save()` 和 `local_intr_restore()`。`code/kern/schedule/sched.h` 声明 `wakeup_proc()`，用于将尚未就绪的新线程设为 `PROC_RUNNABLE`。
- `code/libs/error.h` 定义 `E_NO_FREE_PROC` 和 `E_NO_MEM`，创建失败时使用负错误码。

[GUARANTEE]

实现与 `proc.h` 中的公开声明一致的接口。

```c
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
```

[SPECIFICATION]

**Pre-Condition**

普通进程链表、PID 哈希表和进程计数已初始化。`current` 指向有效的父内核线程，`tf` 指向可读的初始中断帧。`stack` 为 `0`，或是调用方提供的有效且对齐的恢复栈指针。

**Post-Condition**

- **Case 1 · 创建成功。** 返回唯一的正 PID，且 `0 < pid < MAX_PID`。子线程的 `parent` 指向调用时的 `current`，地址空间和首次运行现场准备完毕。子线程在普通进程链表和 PID 哈希表中各登记一次，`find_proc(pid)` 返回该 PCB，`nr_process` 增加 `1`，状态为 `PROC_RUNNABLE`。
- **Case 2 · 已达到进程上限。** 调用时若 `nr_process >= MAX_PROCESS`，在分配资源前返回 `-E_NO_FREE_PROC`。
- **Case 3 · PCB 或内核栈分配失败。** 返回 `-E_NO_MEM`。PCB 分配失败时没有资源需要释放。内核栈分配失败时释放已取得的 PCB。
- **Case 4 · 地址空间准备失败。** 原样返回 `copy_mm()` 的负错误码，先回收已分配的内核栈，再释放 PCB。

**Requirements**

- 将 `clone_flags` 原样交给 `copy_mm()`，并将 `stack` 和 `tf` 原样交给 `copy_thread()`。地址空间和首次运行现场就绪后，才进入 PID 分配和登记。
- 在同一个本地中断保护区内完成 PID 分配、两处登记和进程计数更新，离开时恢复进入前的中断状态。登记完成后，通过 `wakeup_proc()` 使线程就绪。
- 所有失败路径均应让进程表、PID 哈希表和进程计数保持调用前的状态。回收范围限于本次已成功取得的资源。

### `kernel_thread()`

[PROMPT]

**任务**

为 RISC-V uCore Lab4 实现 `kernel_thread()`。将调用方提供的内核函数和参数封装为初始中断帧，交给 `do_fork()` 创建线程，使线程第一次运行时能在 S 态通过 `kernel_thread_entry` 调用指定函数。

**操作要求**

在 `code/kern/process/proc.c` 的 `LAB4 function2 2412414` 区块补写 `kernel_thread()`，复用已完成的 `do_fork()` 和现有的汇编入口。本轮源码改动限于 `kernel_thread()`。

**输出要求**

- 在 `code/` 下运行 `make obj/kern/process/proc.o`，报告实际执行的命令和结果。
- 用有额外标志位的创建请求检查传给 `do_fork()` 的标志和栈参数。核对输入参数分别为有效指针和 `NULL` 时的中断帧字段，以及调用前中断开启或关闭时生成的 `status`。
- 核对 `do_fork()` 返回正 PID 或负错误码时，`kernel_thread()` 的返回值。模块二接齐后，通过 QEMU 和 `make grade` 验证实际线程入口。

[RELY]

- `code/kern/process/proc.h` 声明 `kernel_thread()` 和 `do_fork()`。后者返回新线程的正 PID 或负错误码。
- `code/kern/mm/pmm.h` 定义共享地址空间标志 `CLONE_VM`。
- `code/kern/trap/trap.h` 定义 `struct trapframe`。需要设置的字段为 `gpr.s0`、`gpr.s1`、`epc` 和 `status`，类型均为 `uintptr_t`。
- `code/libs/riscv.h` 提供 `read_csr(sstatus)`、`SSTATUS_SPP`、`SSTATUS_SPIE` 和 `SSTATUS_SIE`。
- `code/kern/process/entry.S` 中的 `kernel_thread_entry` 把 `s1` 中的参数放入 `a0`，调用 `s0` 指向的函数，并将函数返回值交给现有的 `do_exit()`。
- `do_fork()` 成功时在返回前通过 `copy_thread()` 把中断帧保存到子线程的内核栈。栈参数为 `0` 时，由 `copy_thread()` 设置子线程恢复后使用的内核栈指针。

[GUARANTEE]

实现与 `proc.h` 中的公开声明一致的接口。

```c
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);
```

[SPECIFICATION]

**Pre-Condition**

进程管理设施已经初始化，`current` 指向有效的父内核线程。`fn` 是符合接口签名的有效函数指针，`arg` 是应原样传给该函数的参数，可以为 `NULL`。

**Post-Condition**

交给 `do_fork()` 的临时中断帧满足以下条件。

- `gpr.s0` 保存 `fn` 的地址，`gpr.s1` 保存 `arg`，`epc` 指向 `kernel_thread_entry`。
- `status` 以调用时读取的 `sstatus` 为基础，置位 `SPP` 和 `SPIE`，清除 `SIE`。新线程执行 `sret` 时进入 S 态并打开 S 态中断。
- 其余初始字段均为零。传给 `do_fork()` 的标志保留所有传入位并加入 `CLONE_VM`，栈参数为 `0`。

- **Case 1 · 创建成功。** 返回 `do_fork()` 给出的正 PID。
- **Case 2 · 创建失败。** 原样返回 `do_fork()` 给出的负错误码。

**Requirements**

读取 `sstatus` 仅用于构造中断帧，新线程的状态通过后续的恢复流程生效。临时帧在 `do_fork()` 调用期间有效，新线程使用自己的帧副本。

### `proc_init()`

[PROMPT]

**任务**

为 RISC-V uCore Lab4 实现 `proc_init()`。初始化进程管理结构，把当前的内核启动现场设为 `idleproc`，创建第一个普通内核线程 `initproc`，为 `cpu_idle()` 发起首次调度做好准备。

**操作要求**

在 `code/kern/process/proc.c` 的 `LAB4 function2 2412414` 区块补写 `proc_init()`。复用现有的 PCB 分配、内核线程创建和 PID 查找接口，本轮源码改动限于 `proc_init()`。

**输出要求**

- 在 `code/` 下先执行 `make obj/kern/process/proc.o`，再执行 `make` 构建整核。
- 核对首次调度前的状态。检查 `current == idleproc`、`idleproc->need_resched` 已置位、`find_proc(1) == initproc`、`initproc` 已登记且处于就绪态，以及进程总数为 `2`。
- 用 `make qemu` 验证首次切换。观察到 `this initproc, pid = 1, name = "init"` 和 `To U: "Hello world!!".`，即可确认执行到了 `init_main()`。
- 用 `make grade` 核对 `[Test PMM] PASS`、`[Test Process Create] PASS`、`[Test Schedule] PASS` 及总分。报告实际执行的命令和结果。若启动在 `proc_init()` 之前受阻，记录首个失败位置及其对验收的影响。

[RELY]

- `code/kern/init/init.c` 在内存管理初始化后调用 `proc_init()`，并在完成其他启动设置后进入 `cpu_idle()`。`code/kern/mm/pmm.h` 声明 `bootstack` 和根页表物理地址 `boot_pgdir_pa`。
- `code/kern/process/proc.c` 提供普通进程链表 `proc_list`、含 `HASH_LIST_SIZE` 个桶的 PID 哈希表 `hash_list`、进程计数 `nr_process`，以及全局指针 `idleproc`、`initproc` 和 `current`。`code/libs/list.h` 提供 `list_init()`。
- `alloc_proc()` 返回未登记的 PCB，分配失败时返回 `NULL`。它已设置 `pgdir = boot_pgdir_pa`，并将 `mm`、`tf` 和上下文等字段初始化为空或零。
- `kernel_thread(fn, arg, clone_flags)` 通过 `do_fork()` 创建、登记并唤醒线程，成功返回正 PID，失败返回负错误码。`do_fork()` 使用 `current` 作为父线程。`find_proc(pid)` 根据返回的正 PID 查找对应的 PCB。
- 现有 `init_main()` 会打印当前线程的 PID、名称和启动参数。现有 `set_proc_name()` 从源地址读取固定的 `PROC_NAME_LEN` 字节，使用时须保证名称来源具有足够的可读空间。
- `code/kern/schedule/sched.c` 在当前线程是 `idleproc` 时，从普通进程链表的表头查找就绪线程。`code/tools/test_lab4.c` 的 `test_lab4_process_create()` 检查 `proc_init()` 结束时两个线程的状态。

[GUARANTEE]

实现与 `proc.h` 中的公开声明一致的接口。

```c
void proc_init(void);
```

[SPECIFICATION]

**Pre-Condition**

内核处于首次启动阶段，启动内核栈、根页表和内核内存分配器均已就绪。当前执行流正在使用启动栈，进程计数初始为 `0`，首次调度尚未执行。

**Post-Condition**

- **Case 1 · 初始化成功。**
  - `proc_list` 的表头和 `hash_list` 中所有桶的表头均已初始化。
  - `current == idleproc`。空闲线程的 PID 为 `0`，名称为 `idle`，状态为 `PROC_RUNNABLE`，`kstack == (uintptr_t)bootstack`，`need_resched` 已置位。其 `parent`、`tf` 为 `NULL`，初始运行计数、标志和上下文为零。空闲线程单独管理，并计入 `nr_process`。
  - `initproc` 是通过 `kernel_thread()` 用 `init_main` 和参数 `"Hello world!!"` 创建的内核线程。它的 PID 为 `1`，名称为 `init`，父线程为 `idleproc`，状态为 `PROC_RUNNABLE`。此时它是普通进程链表和 PID 哈希表中唯一的 PCB，`find_proc(1) == initproc`，并持有独立内核栈及可恢复的首次执行现场。
  - 两个线程的 `mm` 都为 `NULL`，`pgdir` 都为 `boot_pgdir_pa`，进程总数为 `2`。
- **Case 2 · 启动初始化失败。** 若无法分配 `idleproc`、`kernel_thread()` 返回创建错误，或无法按返回的 PID 找到对应 PCB，调用框架的 `panic()` 报告失败并终止初始化。

**Requirements**

- 先初始化链表头和哈希桶，再创建线程。先将 `idleproc` 设为 `current` 并计入进程总数，再创建 `initproc`，确保父线程和计数正确。
- 普通线程的 PID 分配、登记、计数更新和唤醒均由已有的创建链完成。`proc_init()` 根据返回的 PID 取得 `initproc` 并设置名称。首次切换由之后的 `cpu_idle()` 和调度器执行。

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
