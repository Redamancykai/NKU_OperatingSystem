# Lab 4 成员 C 开发交接

负责人：郑奕杰（2412677）。进程调度、上下文切换、Lab 3 中断依赖核对、Challenge 1 和联调记录由 C 负责。A 汇总小组报告。

## 交接时的进度

开发基线为 [`lab4` 提交 `4511ec7`](https://github.com/Redamancykai/NKU_OperatingSystem/tree/4511ec7caae9a1e66009ad7632a4a94db78d8952)，已包含 A 的页映射实现。C 的四个函数和模块三提示词已在本地完成，三个目标文件均通过交叉编译，反汇编已核对。临时 QEMU 测试覆盖首次切换、单线程继续运行、双线程轮转、空闲回退和中断恢复。合并 B 的线程初始化后执行正式验收。实验要求见[项目内指导书](../lab4_ucore_实验指导书.pdf)。

## B 与 C 的接口约定

B 接入模块三时按下表核对线程初态、页表和首次恢复现场。

| 交接点 | B 准备的状态 | C 如何使用 |
|---|---|---|
| 首次调度 | `current == idleproc`，`idleproc->need_resched == 1`。idle 不挂入普通进程链表。PID 为 1 的 `initproc` 已挂入 `proc_list`，状态为 `PROC_RUNNABLE` | idle 发起调度，从链表头开始寻找普通就绪线程 |
| 页表 | 内核线程的 `pgdir` 保存根页表物理地址，本实验共享内核页表 | `proc_run()` 使用物理页号和 Sv39 模式构造 `satp` |
| 首次恢复 | `initproc->context.ra` 指向 `forkret`，`context.sp` 指向其内核栈上的 `trapframe` | `switch_to()` 恢复新上下文并返回到 `forkret` |
| 进入线程函数 | 中断帧的 `epc` 指向 `kernel_thread_entry`，`s0` 和 `s1` 分别保存线程函数和参数。返回状态支持进入 S 态并恢复中断 | 已有的 `forkret → forkrets → __trapret → sret` 路径接管执行，最终调用 `init_main(arg)` |

双方共用 [proc.c](../code/kern/process/proc.c)。B 编辑 `function2`，C 编辑 `function3`。合并时逐块核对，保留双方实现。

## 阶段一 · C 已完成的实现与局部验证

实现沿用现有接口和辅助函数，三个 `LAB4 function3` 注释均标为 `2412677`。

| 文件与函数 | 实现与检查重点 |
|---|---|
| [switch.S](../code/kern/process/switch.S) · `switch_to(from, to)` | 以 `a0`、`a1` 分别访问旧、新上下文。按 [proc.h](../code/kern/process/proc.h) 依次保存和恢复 `ra`、`sp`、`s0–s11`，再通过恢复后的 `ra` 返回。RV64 下 14 个槽位的偏移为 0 至 104 字节，间隔 8 字节。 |
| [proc.c](../code/kern/process/proc.c) · `proc_run(proc)` | 目标为当前线程时直接返回。发生切换时保存中断状态，在临界区记录旧线程、更新 `current`、按 [pmm.c](../code/kern/mm/pmm.c) 的 `enable_paging()` 写法装载目标页表并刷新 TLB，再调用 `switch_to()`。原线程日后从调用点恢复时，按其保存的标志恢复中断。 |
| [sched.c](../code/kern/schedule/sched.c) · `schedule()` | 保存中断状态，清除当前线程的 `need_resched`。从当前线程后继循环扫描，跳过链表哨兵；当前为 idle 时从表头开始。选中就绪线程或回退到 idle，更新所选线程的 `runs`，目标变化时调用 `proc_run()`，最后恢复中断状态。 |
| [proc.c](../code/kern/process/proc.c) · `cpu_idle()` | 保持空闲循环，仅在 `current->need_resched` 置位时调用 `schedule()`。沿用已有 Lab 3 中断处理和返回路径。 |

**局部验证。** 在配置了项目 RISC-V 工具链的 Linux 或 WSL 环境中，从项目根目录执行以下命令。若已配置其他 `GCCPREFIX`，反汇编器使用相同前缀。

```sh
cd code
make obj/kern/process/switch.o obj/kern/process/proc.o obj/kern/schedule/sched.o
riscv64-unknown-elf-objdump -d obj/kern/process/switch.o
```

以上命令可复验目标文件与寄存器偏移。局部运行日志保存在 C 工作区的 `tmp/member-c-check/green-fixture.log`。该测试在临时副本中准备测试线程、补充启动恒等映射，并跳过 `check_boot_pgdir()`。正式验收以合并后的实际源码为准。

## 阶段二 · B 合入后的验收

B 负责接入 C 的代码并执行下表各项测试。完成后，将联调提交号、执行命令、QEMU 输出、评分结果和边界用例的观察值交给 C。C 复核结果并汇总记录。

| 顺序 | 操作 | 通过依据 |
|---|---|---|
| 1. 核对 A、B 前置条件 | 同步队友改动，记录本次联调提交号。在 `code/` 中执行 `make`、`make qemu`，核对页表自检和上表的线程初态 | 页表自检通过。在首次调度前可观察到 `current == idleproc`，且 `initproc` 已就绪 |
| 2. 验证首次切换 | 跟踪 `cpu_idle → schedule → proc_run → switch_to → forkret`，检查 QEMU 输出 | `init_main()` 打印 PID 1、名称 `init` 和 `Hello world!!`。若在首次 `ret` 或 `sret` 后失去执行流，分别核对 `context.ra/sp` 和中断帧的 `epc/status/sp` |
| 3. 验证重复切换 | 在 `code/` 中执行 `make grade`。[评分目标](../code/Makefile) 会在临时副本中注入[现有用例](../code/tools/test_lab4.c) | 记录 `[Test PMM] PASS`、`[Test Process Create] PASS`、`[Test Schedule] PASS` 与评分结果。调度用例包含两个主动让出 CPU 的线程 |
| 4. 验证空闲回退 | 单独启动一次 QEMU，在普通启动路径的 `init_main` 处用调试器暂时将唯一普通线程置为 `PROC_SLEEPING`，触发一次 `schedule()`，观察 idle 之前挂起的调用点 | 调度选中 `idleproc`，`current` 与实际恢复执行的 idle 栈一致。记录断点和观察值。骨架的 `do_exit()` 会触发 panic，线程结束输出本身无法覆盖此用例 |
| 5. 验证中断恢复 | 对照 [sync.h](../code/kern/sync/sync.h)，观察 `schedule()` 外层和 `proc_run()` 内层保存的标志，以及原线程恢复后的 `sstatus.SIE` | 外层进入时已开中断，则由外层最终恢复；进入时已关中断，则离开后仍关闭。内层退出不会提前开启中断。新线程首次进入时另由中断帧和 `sret` 恢复状态 |

### 启动问题与介入条件

- **启动阶段。** B 若遇到写入 `satp` 后的取指页故障，或 `check_boot_pgdir()` 阶段重进内核初始化，把日志交给 A 一起复现。`check_boot_pgdir()` 由 A 核对，启动汇编的取指故障由 A、B 按复现结果确定修复归属。C 在临时副本中观察过这两个现象，原始日志分别保存在 C 工作区的 `tmp/member-c-check/qemu-int.log` 和 `tmp/member-c-check/green-qemu.log`。第二个现象的根因待确认。
- **首次切换接口。** B 先核对 `context.ra/sp`、`trapframe` 和页表物理地址。若需要确认模块二与模块三的接口，B 联系 C 一起核对。
- **调度和上下文切换。** 若日志指向调度选择、寄存器恢复或中断状态恢复，由 C 修复并复测。若 B 完成验收且各项通过，C 的代码工作即告完成。

## 验收通过后 C 的交付项

- 模块三的 Spec 和 Prompt 已整理。见 [prompt.md](prompt.md)。
- 补齐模块三的四个核心函数说明、从 idle 到 init 的执行路径，以及 Challenge 1 对中断保存和嵌套恢复的解释。见 [report.md](report.md)。
- 使用 B 提供的结果汇总联调记录，写明提交号、命令或断点、预期、实测与结论。实际出现的 Bug 记录复现条件、已确认的原因、修复和复测结果。将报告和联调记录交给 A 汇总。
