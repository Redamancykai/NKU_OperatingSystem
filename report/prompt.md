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