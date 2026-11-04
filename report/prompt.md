# 功能模块一：Trap 上下文恢复与 Trap 入口功能
### 负责人：黄泽恺 2411268

[PROMPT]

**任务**

在 riscv64-ucore Lab3 中完成“Trap 上下文恢复与 Trap 入口功能”，使内核能够保存被打断的执行现场，将现场交给 C 语言 Trap 处理函数，并在处理结束后恢复现场、返回正确的执行位置。

实际工作区为 WSL Ubuntu-22.04 中的：

`/home/zekai/projects/OS_labs/code`

本次只修改：

`/home/zekai/projects/OS_labs/code/kern/trap/trapentry.S`

需要补全的两处位置均带有 `#lab3 function1 YOUR CODE` 标记：

1. `.macro RESTORE_ALL` 与对应 `.endm` 之间的宏体。
2. `__alltraps:` 中，现有 `SAVE_ALL` 之后、`__trapret:` 之前的入口处理代码。

**操作要求**

- 先阅读项目规则和下列相关代码，再直接修改 WSL 工作区中的实际文件。
- 保留现有代码和任务标记，只在上述两处补全实现；不修改其他位置的项目副本。
- 不修改 `SAVE_ALL`、`struct pushregs`、`struct trapframe`、`trap()` 的函数签名及现有 `__trapret` 返回框架。

**输出要求**

使用下面 [RELY]、[GUARANTEE] 和 [SPECIFICATION] 中的信息，以工程真实代码为准完成实现。完成后说明修改位置、关键状态关系、验证结果及尚未满足的测试依赖，不引入无关功能。


[RELY]

**1. 模块在项目中的位置**

- `/home/zekai/projects/OS_labs/code/kern/trap/trapentry.S`：提供 Trap 汇编入口、上下文保存与恢复及返回框架。
- `/home/zekai/projects/OS_labs/code/kern/trap/trap.h`：定义保存现场的数据布局，以及 C 语言处理接口。
- `/home/zekai/projects/OS_labs/code/kern/trap/trap.c`：`idt_init()` 将 `stvec` 指向 `__alltraps`；`trap()` 将中断与异常分发给对应处理函数。
- `/home/zekai/projects/OS_labs/code/libs/riscv.h`：提供汇编访存宏、寄存器宽度定义和相关体系结构常量。

**2. 已有的数据布局**

项目中 `uintptr_t` 为 64 位无符号整数。`struct pushregs` 按 x0 到 x31 的编号顺序存放通用寄存器，其字段顺序为：

`zero, ra, sp, gp, tp, t0, t1, t2, s0, s1, a0, a1, a2, a3, a4, a5, a6, a7, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, t3, t4, t5, t6`

`struct trapframe` 的真实定义为：

```c
struct trapframe {
    struct pushregs gpr;
    uintptr_t status;
    uintptr_t epc;
    uintptr_t badvaddr;
    uintptr_t cause;
};
```

设 Trap frame 首地址为 F，其布局为：

| 内容 | 相对 F 的偏移 |
|---|---|
| x0～x31 对应的通用寄存器槽位 | 寄存器编号 × REGBYTES |
| 原始 sp，即 `gpr.sp` | 2 × REGBYTES |
| `status`，对应保存的 sstatus | 32 × REGBYTES |
| `epc`，对应保存的 sepc | 33 × REGBYTES |
| `badvaddr`，对应保存的 sbadaddr | 34 × REGBYTES |
| `cause`，对应保存的 scause | 35 × REGBYTES |

整个 Trap frame 占 `36 × REGBYTES`，当前 RV64 环境下为 288 字节。

**3. 已有宏和保存行为**

当前 RV64 分支中，工程定义：

```asm
#define STORE sd
#define LOAD ld
#define LOG_REGBYTES 3
#define REGBYTES (1 << LOG_REGBYTES)
```

已有 `SAVE_ALL` 会在当前内核栈上建立上述 Trap frame。其关键语义为：

- 保存 x0～x31 对应的通用寄存器现场。
- 原始 sp 通过 `sscratch` 临时保存，再写入 `gpr.sp` 槽位；它不是分配 Trap frame 后的 sp。
- 保存 `sstatus`、`sepc`、`sbadaddr` 和 `scause`。
- 执行结束后，sp 指向 Trap frame 首地址，`sscratch` 已被置零。

恢复过程必须与该真实保存布局一致。

**4. 已有调用接口和返回框架**

C 语言接口为：

```c
void trap(struct trapframe *tf);
```

RISC-V 调用约定使用 a0 传递第一个参数，调用 C 函数时需要满足栈对齐等 ABI 约束。

汇编文件已经提供：

```asm
__trapret:
    RESTORE_ALL
    sret
```

`trap()` 及其下游处理函数可能修改 Trap frame 中的字段，例如调整 `epc`。返回时应使用处理结束后的 Trap frame 内容。

`badvaddr` 和 `cause` 是现场诊断信息，不应因为它们位于 Trap frame 中，就机械地将其写回对应 CSR。

**5. 可参考的现有测试**

- `/home/zekai/projects/OS_labs/code/tools/test_lab3_context.S`
- `/home/zekai/projects/OS_labs/code/tools/test_lab3.c`
- `/home/zekai/projects/OS_labs/code/tools/grade.sh`
- `/home/zekai/projects/OS_labs/code/tools/inject.py`
- `/home/zekai/projects/OS_labs/code/Makefile`

其中上下文测试通过断点触发 Trap，并检查返回后的多组寄存器值。该运行测试也依赖功能模块二正确处理断点异常及更新返回地址，不能将它视为完全独立的模块一测试。


[GUARANTEE]

本次必须完成两个接口：

1. **`RESTORE_ALL` 汇编宏**
   - 保留 `.macro RESTORE_ALL` 和 `.endm`。
   - 输入为 sp 指向的当前 Trap frame。
   - 完成返回所需的处理器状态恢复，使后续现有 `sret` 可以正确执行。

2. **`__alltraps` 汇编入口**
   - 保留现有入口标签和对齐声明。
   - 在现有 `SAVE_ALL` 之后，建立到 `void trap(struct trapframe *tf)` 的正确调用关系。
   - 在 C 函数返回后衔接已有 `__trapret`，完成 Trap 返回。

上述两个接口共同完成同一个功能模块，不新增或改变公开接口，不额外定义同名 C 函数，不新增辅助模块。


[SPECIFICATION]

**一、RESTORE_ALL**

**Pre-Condition**

- 当前在 S 模式的 Trap 返回路径中。
- sp 指向一个有效、可访问且布局与 `struct trapframe` 一致的 Trap frame。
- C 语言 Trap 处理已经结束，Trap frame 中保存的是本次应恢复的最终现场。
- `status`、`epc` 及保存的原始 sp 满足当前实验的合法返回条件。

**Post-Condition**

- x1～x31 的值与最终 Trap frame 中对应字段一致，包括调用 C 函数时可能被改写的 caller-saved 寄存器；x0 保持硬连线零的语义。
- sp 恢复为 Trap frame 的 `gpr.sp` 字段值，不再指向本次现场保存区。
- `sstatus` 与 `sepc` 恢复为最终 Trap frame 中的 `status` 与 `epc`，供现有 `sret` 使用。
- 恢复所使用的临时寄存器最终也恢复为相应现场值。
- 不破坏原执行流的栈内容及 Trap frame 范围之外的内存。
- 宏本身不执行普通函数返回，完成后继续执行现有 `sret`。

**Case 1：处理函数未修改返回现场**

当处理函数未修改相关寄存器、状态和返回地址时，恢复后应能够继续被打断的执行流，原程序所依赖的通用寄存器和栈状态保持一致。

**Case 2：处理函数修改了返回现场**

当处理函数合法修改 `epc` 或其他可恢复字段时，恢复结果必须采用这些最终字段值，不能使用处理前的旧值覆盖它们。

**Requirements**

- 恢复过程必须保持 Trap frame 的可访问性，不能过早改变作为现场访问基址的 sp。
- 原始 sp 必须按保存槽位的语义恢复，不能忽略 `gpr.sp` 而只根据固定帧大小推算。
- 不自行增加 `epc`，异常指令跳过逻辑属于功能模块二。
- 不强制修改返回特权级，不无条件开启中断；尊重最终 `status` 和 `sret` 的返回语义。
- 不要求恢复诊断用途的 `sbadaddr`、`scause`。
- 保持当前内核环境中已有的 `sscratch` 约定。


**二、__alltraps**

**Pre-Condition**

- `stvec` 已指向该入口，处理器因 Trap 进入 S 模式。
- 当前内核栈有效，并满足框架的栈空间和对齐前提。
- 入口处的通用寄存器仍属于被打断的执行现场，尚未完成软件保存。
- 已有 `SAVE_ALL` 和 C 语言 `trap()` 接口可用。

**Post-Condition**

- 在调用任何 C 处理代码之前，已有 `SAVE_ALL` 完成一次完整的现场保存。
- `trap()` 接收到的参数指向刚建立的实际 Trap frame。
- 调用 C 函数满足 RISC-V ABI，不能破坏尚待恢复的现场。
- `trap()` 正常返回时，sp 与调用前一致，仍指向同一个 Trap frame 首地址。
- 随后进入已有 `__trapret`，使用最终 Trap frame 恢复现场，通过 `sret` 返回其指定的执行位置。

**Requirements**

- 不重复执行现场保存，不额外创建与既有布局不一致的 Trap frame。
- 正确处理调用 C 函数对 ra、参数寄存器和临时寄存器的影响，确保原执行现场最终可恢复。
- 不将 `__alltraps` 当作普通 C 函数入口，也不使用普通函数返回方式代替 Trap 返回。
- 不在此入口中实现中断分类、时钟计数、异常输出或 `epc` 调整。


**三、验证与交付**

- 检查实际改动仅涉及指定文件中的两个 `lab3 function1` 补全位置。
- 核对每个恢复字段的偏移、寄存器宽度及恢复结果，确认 sp 不会过早失去现场访问基址。
- 核对 Trap frame 参数传递、C 调用前后的 sp 一致性，以及最终返回路径。

---

# 功能模块二：中断与异常处理功能
### 负责人：黄子恒 2412414

## 1. 中断处理函数 interrupt_handler

[PROMPT]

**任务**

在 riscv64-ucore Lab3 中完成“中断处理”功能：识别 Trap 现场记录的中断原因，使 Supervisor 时钟中断能够持续发生、正确累计时钟计数并定期输出统计信息，在达到规定输出次数后请求关机；其他已知中断输出类型提示，未知中断输出原因和完整现场。

本次只实现模块二的第一个函数：

```c
void interrupt_handler(struct trapframe *tf);
```

当前项目根目录为：

`C:/Users/黄子恒/Desktop/操作系统/lab3`

本次代码修改范围仅为根目录下的：

`code/kern/trap/trap.c`

函数定义应放在 `//lab3 function2 YOUR CODE` 标记之后、已有 `trap_dispatch()` 之前。

**操作要求**

- 先阅读相关真实代码，再修改当前项目文件。
- 保留现有任务标记和代码，将标记中的 `YOUR CODE` 替换为 `2412414`。
- 允许在 `trap.c` 的头文件区域添加必需的 `<sbi.h>`，并在本文件内补充关机配置及必要的 `static` 辅助函数或内部状态。
- 不修改 `print_ticks()`、`print_trapframe()`、`print_regs()`、`idt_init()`、`trap_dispatch()`、`trap()` 等已有函数，不改变 `struct trapframe` 和汇编现场保存、恢复逻辑。
- 本次不实现 `exception_handler()`，也不添加空函数或伪造实现来绕过该函数尚未完成的依赖。
- 本次不实现异常指令跳过、系统调用、进程调度、设备中断控制器驱动、外设服务或其他特权级的实际中断处理。
- 不修改测试、Makefile、其他源文件或项目副本。

**输出要求**

使用下列 [RELY]、[GUARANTEE] 和 [SPECIFICATION]，以当前工程真实定义为准完成实现。完成后说明修改位置、关键状态关系、关机阈值的实际取值及验证结果。

检查中断分类、统计与关机边界，以及输入现场是否保持不变。对无法执行的检查说明原因；当前完整构建及评分还依赖尚未实现的 `exception_handler()`，应如实记录该依赖。

[RELY]

**1. 相关文件及调用关系**

- `code/kern/trap/trap.c`：包含本次补全位置、已有统计输出与现场输出函数，以及统一分发框架。
- `code/kern/trap/trap.h`：定义 `struct trapframe`。
- `code/libs/defs.h`：当前工程中 `uintptr_t`、`size_t` 为 64 位无符号整数，`intptr_t` 为 64 位有符号整数。
- `code/libs/riscv.h`：定义中断原因编号。
- `code/kern/driver/clock.h`、`clock.c`：提供时钟计数和事件设置接口。
- `code/libs/sbi.h`、`sbi.c`：提供关机接口。

已有 `trap()` 调用 `trap_dispatch(tf)`。真实 Trap 分发时，后者通过 `(intptr_t)tf->cause < 0` 判断中断，并调用 `interrupt_handler(tf)`；异常交给 `exception_handler(tf)`。

本函数正常返回后，已有汇编框架负责恢复现场并执行 `sret`。

**2. 输入现场和原因编码**

已有真实结构体定义为：

```c
struct trapframe {
    struct pushregs gpr;
    uintptr_t status;
    uintptr_t epc;
    uintptr_t badvaddr;
    uintptr_t cause;
};
```

其中 `gpr` 保存通用寄存器，`status` 保存状态，`epc` 保存返回位置，`badvaddr` 保存辅助诊断信息，`cause` 保存原始 Trap 原因。

在当前 RV64 环境中，`cause` 的第 63 位是中断标志，其余位表示原因编号。正常中断分发保留该标志位；现有测试也会绕过分发框架，直接向本函数传入不带该标志的原因编号。

`code/libs/riscv.h` 中定义了以下中断原因编号，分别对应 U/S/M 的软件、时钟和外部中断：

```c
#define IRQ_U_SOFT   0
#define IRQ_S_SOFT   1
#define IRQ_M_SOFT   3
#define IRQ_U_TIMER  4
#define IRQ_S_TIMER  5
#define IRQ_M_TIMER  7
#define IRQ_U_EXT    8
#define IRQ_S_EXT    9
#define IRQ_M_EXT    11
```

**3. 已有时钟和输出接口**

`clock.h` 提供：

```c
extern volatile size_t ticks;
void clock_init(void);
void clock_set_next_event(void);
```

`clock_init()` 已在启动流程中调用，负责设置第一次时钟事件并将 `ticks` 初始化为 0。`clock_set_next_event()` 通过 SBI 设置下一次时钟事件。

`trap.c` 中已有：

```c
#define TICK_NUM 100

static void print_ticks() {
    cprintf("%d ticks\n", TICK_NUM);
#ifdef DEBUG_GRADE
    cprintf("End of Test.\n");
    panic("EOT: kernel seems ok.");
#endif
}
```

现有诊断、输出和关机接口为：

```c
// code/kern/trap/trap.h
void print_trapframe(struct trapframe *tf);

// code/libs/stdio.h
int cprintf(const char *fmt, ...);

// code/libs/sbi.h
void sbi_shutdown(void);
```

`print_trapframe()` 输出完整 Trap frame；`sbi_shutdown()` 请求系统关机。

**4. 当前框架缺口及测试依赖**

当前工程尚未定义 `SHUTDOWN_AFTER`。

可参考 `code/tools/test_lab3.c`、`grade.sh`、`inject.py` 和 `code/Makefile`。其中：

- 软件中断测试传入带第 63 位中断标志的 `IRQ_S_SOFT`。
- 未知中断测试直接传入 `tf->cause = 15`。
- 时钟测试要求计数达到 200，且输出中出现 `100 ticks` 与 `ticks=200`。
- 完整评分同时依赖模块一和 `exception_handler()`；当前后者尚未实现。

[GUARANTEE]

本次必须新增并完成以下接口：

```c
void interrupt_handler(struct trapframe *tf);
```

- 函数负责中断分类、Supervisor 时钟事件续设与计数、统计输出、达到阈值后的关机请求，以及其他中断的提示或现场诊断。
- 保持上述函数签名和外部可见性，供现有分发代码及测试直接调用，不能将其定义为 `static`。
- 如需辅助函数，仅允许作为本文件内部的 `static` 函数；不新增其他公开函数或修改已有公开接口。

[SPECIFICATION]

**一、interrupt_handler**

**Pre-Condition**

- 当前执行环境为本实验的 RV64 内核环境。
- `tf` 指向有效且可读取的 `struct trapframe`，其字段完整；不要求为无效指针另设恢复机制。
- 输入原因可以是带中断标志的完整原因值，也可以是直接调用测试提供的不带标志的原因编号。
- 时钟场景下，框架已完成时钟初始化，从 `ticks = 0` 开始累计；除本函数响应 Supervisor 时钟事件外，没有其他执行流并发修改该计数。计数处于本实验的有效范围内，不要求处理整数溢出。
- 常规持续计时及关机验证时未定义 `DEBUG_GRADE`，已有统计输出和 SBI 接口可用。

**Post-Condition**

- 按原因编号完成下列对应行为；同一编号是否携带中断标志，不影响其分类结果。
- 原始 Trap frame 的全部字段保持不变，包括 `cause`、`epc`、`status`、`badvaddr` 和通用寄存器现场。
- 只有 Supervisor 时钟中断可以增加 `ticks`、安排后续时钟事件及触发统计输出和关机条件判断。
- 未达到关机条件的正常处理路径返回已有分发框架，不自行执行 `sret`，不长期轮询或停机。
- 达到关机条件时请求系统关机；不要求该关机路径正常返回。

**Case 1：Supervisor 时钟中断**

令本次进入函数时的计数为 N，处理后的计数为 M。

- M = N + 1；一次 Supervisor 时钟事件恰好计数一次。
- 在正常计时路径返回前，已通过 `clock_set_next_event()` 安排下一次时钟事件，保证后续能够继续触发时钟中断。
- 当 M 是 `TICK_NUM` 的正整数倍时，调用已有 `print_ticks()` 输出一次统计；其他计数值不输出该统计。
- 每一行 `100 ticks` 表示本轮新增了 100 次时钟中断，不表示累计值被重置为 100，也不把后续输出改成 `200 ticks`、`300 ticks`。
- 统计输出累计达到 `SHUTDOWN_AFTER` 次时，先完成该次统计输出，再通过 `sbi_shutdown()` 请求关机；达到阈值前不请求关机。

- 阈值按统计输出次数计量，而不是按单次时钟中断次数计量。若需补充缺失的配置，自行选择合理取值并在完成说明中记录；所用配置应允许现有时钟测试达到 `ticks = 200` 并输出通过信息。

**Case 2：其他已知中断**

- 识别 U/S/M 的软件中断、外部中断，以及 U/M 的时钟中断，输出对应类型的提示并换行。
- `IRQ_S_SOFT` 的提示必须包含 `Supervisor software interrupt`。
- 提示完成后正常返回，`ticks` 保持不变，不设置时钟事件，也不请求关机。
- 这些中断仅作类型识别和提示，不实现外设驱动、中断源确认或其他特权级的实际服务。

**Case 3：未知中断**

- 原因编号不在 Case 1、Case 2 的范围内时，输出 `Unknown interrupt: cause <编号>` 并换行，其中编号使用去除中断标志后的十进制值。
- 同时调用已有 `print_trapframe(tf)` 输出完整原始现场。
- 不修改现场以便分类或打印，不将其转换成另一种中断，不执行时钟计数或关机操作，输出后正常返回。
- 对现有测试输入 `tf->cause = 15`，输出必须包含 `Unknown interrupt: cause 15`。

**Requirements**

- 使用工程已有中断编号宏和接口，不重新定义 `ticks`，不复制实现已有时钟、打印或 SBI 功能。
- 保持现有初始化和返回框架的职责；不重置 `ticks`，不在本函数中重复初始化时钟。
- 不直接改变保存的返回地址、返回特权级或全局中断使能策略。
- 正常处理路径不进行长期轮询，不因已知非时钟中断或未知中断进入 `panic()` 或停机。

**二、验收条件**

- 检查改动范围仅为 `trap.c` 中的目标函数、必要头文件、配置和内部辅助内容，现有函数及队友代码未被改写。
- 核对带标志与不带标志的同一原因编号分类一致，且输入现场在正常返回后保持原值。
- 核对连续时钟事件下的 99→100、100→101、199→200 等边界：计数逐次增加，仅整百输出统计，正常返回前已安排下一次事件。
- 按实际采用的阈值 K，核对未达到第 K 次统计输出时不关机，第 K 次统计输出完成后请求关机。
- 核对已知软件、外部及其他特权级时钟中断只输出类型提示；未知原因 15 同时输出规定文本和完整现场，且不增加计数。
- 现有时钟测试能够达到 200 次，并输出通过信息。

## 2. 异常处理函数 exception_handler

[PROMPT]

**任务**

在 riscv64-ucore Lab3 中完成异常处理功能：根据 Trap 现场识别非法指令异常和断点异常，输出异常发生的位置，并正确更新返回地址，使程序跳过触发异常的指令后继续执行；对其他异常输出完整现场，供调试使用。

本次只实现模块二的第二个函数：

```c
void exception_handler(struct trapframe *tf);
```

当前项目根目录为：

`C:/Users/黄子恒/Desktop/操作系统/lab3`

本次只修改根目录下的：

`code/kern/trap/trap.c`

在已有 `interrupt_handler()` 之后、`trap_dispatch()` 之前补全函数定义。

**操作要求**

- 先阅读相关真实代码，再直接修改当前项目文件。
- 保留现有 `//lab3 function2 2412414` 任务标记和已完成的中断处理代码。
- 允许在本文件内添加必要的 `static` 辅助函数。
- 不修改已有 `interrupt_handler()`、`print_ticks()`、`print_trapframe()`、`print_regs()`、`idt_init()`、`trap_dispatch()`、`trap()` 等函数，不改变时钟计数和关机配置。
- 不修改 `struct trapframe`、汇编现场保存与恢复逻辑、测试、Makefile 或其他源文件。
- 本次不实现系统调用、缺页恢复、进程调度或其他异常的专用恢复机制。

**输出要求**

使用下列 [RELY]、[GUARANTEE] 和 [SPECIFICATION]，以工程真实定义为准完成实现。完成后说明修改位置、指令长度与返回地址的关系，以及实际验证结果。

检查 2 字节和 4 字节指令的处理、诊断地址是否为原异常位置、其余现场字段是否保持不变，以及兜底分支是否保留原始现场。运行适用的编译、QEMU 和评分检查；对未执行或失败的检查说明原因，不修改测试来绕过问题。

[RELY]

**1. 相关文件及调用关系**

- `code/kern/trap/trap.c`：包含本次补全位置、已有中断处理函数、现场输出函数和统一分发框架。
- `code/kern/trap/trap.h`：定义 `struct trapframe` 和现场输出接口。
- `code/libs/defs.h`：当前工程中 `uintptr_t` 为 64 位无符号整数，`intptr_t` 为 64 位有符号整数，`uint16_t` 和 `uint32_t` 分别为 16 位和 32 位无符号整数。
- `code/libs/riscv.h`：定义异常原因编号和指令编码常量。
- `code/libs/stdio.h`：提供格式化输出接口。
- `code/kern/trap/trapentry.S`：提供现场保存、恢复以及 Trap 返回代码。

已有 `trap()` 调用 `trap_dispatch(tf)`。当 `(intptr_t)tf->cause < 0` 时分发给 `interrupt_handler()`，否则分发给 `exception_handler()`。

模块一的 `RESTORE_ALL` 会将处理后的 `tf->epc` 恢复到 `sepc`，之后执行 `sret`；`tf->status` 和通用寄存器现场也由已有框架恢复。

**2. 输入现场和异常编号**

`trap.h` 中的真实定义为：

```c
struct trapframe {
    struct pushregs gpr;
    uintptr_t status;
    uintptr_t epc;
    uintptr_t badvaddr;
    uintptr_t cause;
};
```

`cause` 保存异常原因；`epc` 保存触发异常的指令地址；`badvaddr` 保存辅助诊断信息。`status` 和 `gpr` 保存处理器状态及通用寄存器现场。

`riscv.h` 中相关的真实定义为：

```c
#define CAUSE_ILLEGAL_INSTRUCTION 0x2
#define CAUSE_BREAKPOINT 0x3

#define MATCH_EBREAK 0x100073
#define MASK_EBREAK  0xffffffff
#define MATCH_C_EBREAK 0x9002
#define MASK_C_EBREAK  0xffff
```

**3. 已有输出接口**

```c
// code/kern/trap/trap.h
void print_trapframe(struct trapframe *tf);

// code/libs/stdio.h
int cprintf(const char *fmt, ...);
```

`print_trapframe()` 输出完整 Trap frame，包括 `epc`、`cause`、状态和通用寄存器现场。

**4. 现有实现及测试**

`interrupt_handler()` 已完成中断分类及 Supervisor 时钟处理；`exception_handler()` 尚未定义。

可参考 `code/tools/test_lab3_context.S`、`test_lab3.c`、`grade.sh`、`inject.py` 和 `code/Makefile`：

- 上下文测试通过汇编中的 `ebreak` 触发断点异常，返回后检查多组寄存器值。
- 非法指令测试执行 `.word 0x0000007b`，该测试指令占 4 字节，异常返回后继续执行后续测试。
- 兜底测试将 Trap frame 清零后设置 `tf.cause = 20`，直接调用 `exception_handler()`；此时 `tf.epc = 0`。
- 评分脚本检查输出中包含 `Breakpoint at`、`Illegal instruction at`，以及兜底现场中的 `cause    0x00000014`。
- 完整评分还检查持续时钟中断、软件中断提示和未知中断诊断。
- `make grade` 在临时副本中注入测试，不会将测试代码直接写入正式 `init.c`。

[GUARANTEE]

本次必须新增并完成以下接口：

```c
void exception_handler(struct trapframe *tf);
```

- 接收当前 Trap frame，完成非法指令、断点异常的诊断及返回地址更新，对其他异常打印原始现场。
- 保持上述函数签名和外部可见性，供已有分发代码及测试直接调用，不能定义为 `static`。
- 如需辅助函数，仅作为本文件内部的 `static` 函数，不新增其他公开接口。

[SPECIFICATION]

**一、exception_handler**

**Pre-Condition**

- 当前执行环境为本实验的 RV64 内核环境，`tf` 指向有效、可读写的 `struct trapframe`。
- `tf->cause` 为异常原因编号，不包含中断标志。
- 对非法指令和断点异常，`tf->epc` 指向有效、至少 2 字节对齐的指令地址，指令编码所需的字节可读取，地址更新不溢出。
- 本次需要跳过的指令长度限定为 2 字节或 4 字节，不要求支持更长的指令编码。
- 对其他异常，不保证 `tf->epc` 指向可读取的指令内存；此类输入仍具有有效的 Trap frame。
- 已有控制台输出、现场恢复和 Trap 返回机制可用。

**Post-Condition**

- 根据 `tf->cause` 选择对应异常处理行为。
- 对非法指令和断点异常，设进入函数时的异常地址为 E、触发异常的指令长度为 L，则输出中的异常地址为 E，处理后 `tf->epc = E + L`。
- 除这两类异常的 `epc` 更新外，Trap frame 的其他字段保持不变；兜底处理保留全部字段原值。
- 处理完成后正常返回统一 Trap 流程，由已有框架恢复现场并返回。
- 时钟计数、下一次时钟事件和关机状态不受本函数影响。

**Case 1：非法指令异常**

- 当原因为 `CAUSE_ILLEGAL_INSTRUCTION` 时，输出包含 `Illegal instruction at` 的提示和原异常地址，并换行；地址应完整表示当前 RV64 地址值。
- 根据触发异常指令的实际编码确定其长度，使返回位置指向该指令之后，不重复执行同一条非法指令。
- 对 2 字节非法指令，返回地址为 E + 2；对 4 字节非法指令，返回地址为 E + 4。
- 对现有 `.word 0x0000007b` 测试，返回地址必须推进 4 字节。

**Case 2：断点异常**

- 当原因为 `CAUSE_BREAKPOINT` 时，输出包含 `Breakpoint at` 的提示和原断点地址，并换行；地址应完整表示当前 RV64 地址值。
- 兼容 2 字节的 `c.ebreak` 和 4 字节的 `ebreak`，使返回位置分别为 E + 2 和 E + 4。
- 返回后能够继续执行断点后的指令，原执行流依赖的通用寄存器值保持不变。

**Case 3：其他异常**

- 对上述两类之外的异常，调用已有 `print_trapframe(tf)` 输出完整原始现场，然后正常返回。
- 保持 `epc` 和其他现场字段不变，不跳过指令，也不添加专用恢复行为。
- 不读取 `epc` 所指向的指令内存，保证 `cause = 20、epc = 0` 的现有兜底测试能够完成现场输出。
- 兜底输出不保证原异常已被消除；若通过真实 Trap 返回，执行流是否再次触发该异常不属于本次恢复范围。

**Requirements**

- 使用工程已有异常编号宏和输出接口。
- 指令长度判断以原异常地址处的实际编码为依据，不能仅按异常类别固定偏移，也不能无条件将 `badvaddr` 当作完整指令编码。
- 指令读取应兼容 2 字节对齐的地址；不要因不必要的额外读取或更严格的对齐假设引入新的访存异常。
- 只有非法指令和断点分支可以修改 `tf->epc`，不直接改写 CSR，不改变返回特权级或全局中断使能策略。
- 不在这些处理分支中增加 `panic()`、关机、死循环或交互式调试器，保持与现有返回流程衔接。

**二、验收条件**

- 核对实际改动仅涉及目标函数和必要的内部辅助内容，已完成的中断处理及模块一代码保持不变。
- 分别检查 2 字节和 4 字节非法指令、压缩断点和普通断点的返回地址，且输出地址始终为更新前的 E。
- 检查 2 字节对齐但非 4 字节对齐的指令地址，确保长度判断和读取不依赖 4 字节对齐。
- 检查其他异常原因及 `cause = 20、epc = 0` 的兜底场景：只输出现场，全部现场字段保持原值。
- 核对非法指令和断点处理仅更新 `epc`，不破坏其他寄存器现场或时钟相关状态。
- 编译通过，QEMU 能继续产生时钟输出；`make grade` 中断点上下文、非法指令返回及持续时钟三组测试通过。
