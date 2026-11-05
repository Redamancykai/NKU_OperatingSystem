# 操作系统实验报告

## 实验基本信息

| 项目 | 内容 |
|------|------|
| **实验名称** | Lab 3: 断，都可以断 |
| **小组成员** | 学号1-姓名1、学号2-姓名2、学号3-姓名3 |
| **完成日期** | YYYY-MM-DD |

### 小组分工

练习如何分工？

| 成员 | 负责的练习/模块 |
|------|----------------|
| 学号1-姓名1 | [练习或模块名称] |
| 学号2-姓名2 | [练习或模块名称] |
| 学号3-姓名3 | [练习或模块名称] |

实验报告如何分工？

---

## 一、实验目的

<!-- 说明本次实验的主要目标和预期成果 -->

本实验的主要目的是：

1. [目的1：例如，理解用户态进程如何通过系统调用进入内核]
2. [目的2：例如，掌握进程创建、执行、退出的完整生命周期]
3. [目的3：例如，学习使用 AI 辅助开发操作系统核心功能]

---

## 二、实验环境

你们使用的 AI 工具

| 成员 | AI 编程工具 | 底层模型 | 备注 |
|------|------------|---------|------|
| 学号1-姓名1 | [例如：Cline（VS Code插件） / Claude Code（终端 Agent） / DeepSeek Harness等] | [例如：Claude Sonnet 4.5 / GPT-5.5 / DeepSeek-V4-Flash等] | [可选：特殊配置等] |
| 学号2-姓名2 | | | |
| 学号3-姓名3 | | | |

**说明：**
- **AI 编程工具**：指具体使用的终端工具、编辑器插件、桌面应用或浏览器界面
- **底层模型**：指该工具使用的大语言模型及版本

---

## 三、实验整体逻辑分析

### 2.1 本章节的逻辑主线

[请用自己的话描述本章实验的核心主线，例如：
- 本章围绕哪个核心主题展开？（例如：用户进程管理、虚拟内存、文件系统等）
- 整个实验解决什么问题？（例如：如何让用户程序运行在操作系统之上）]

### 2.2 功能的逐步实现

[说明本章实验是按照什么顺序逐步实现各个功能的，以及为什么要按这个顺序，例如：

1. 首先实现**功能A** → 为什么首先实现这个？（例如：建立系统调用接口是后续所有用户态功能的基础）
2. 接着再完善**功能B** → 为什么接着实现这个？（例如：有了系统调用后，需要能够加载和启动第一个用户进程）
3. 然后完成**功能C** → 为什么然后实现这个？（例如：有了第一个进程后，需要实现进程复制功能）
4. 最后补充了**功能D** → 为什么最后实现这个？（例如：最后完善进程退出和清理机制，形成完整闭环）]

---

## 四、实验内容与实现

<!-- 
说明：本部分按照 exercises 文档中的练习顺序组织
每个功能模块或者练习中可能包含多项任务，请根据实际情况调整
-->

### 功能模块一：Trap 上下文恢复与 Trap 入口功能

**负责人** 2411268-黄泽恺

#### 模块功能描述

**需要实现或修改的接口**

| 接口 | 形式 | 职责 |
| --- | --- | --- |
| `RESTORE_ALL` | 汇编宏 | 依据当前 Trap frame 恢复通用寄存器及返回所需的 `sstatus`、`sepc`，供 `sret` 使用。 |
| `__alltraps` | 汇编入口 | 保存 Trap 上下文，将 Trap frame 地址传给 C 函数 `void trap(struct trapframe *tf)`，处理结束后进入 `__trapret`。 |

**功能说明**

本模块负责衔接 RISC-V 的硬件 Trap 入口与内核的 C 语言处理流程。`idt_init()` 将 `stvec` 指向 `__alltraps`。S 模式下发生时钟中断、断点或非法指令等 Trap 时，汇编入口先在内核栈上保存现场，处理函数随后读取或更新这份现场，最后由 `sret` 返回。

1. **保存现场并传参。** `__alltraps` 先展开已有的 `SAVE_ALL`，在栈上建立 288 字节的 Trap frame。其中 `gpr.sp` 保存进入 Trap 时的栈指针，此时寄存器 `sp` 则指向帧首地址。按照 RISC-V 调用约定，将该地址放入 `a0` 后调用 `trap()`。调用返回时，`sp` 仍指向同一份 Trap frame。
2. **分发中断与异常。** `trap()` 依据 `tf->cause` 将处理交给模块二。时钟中断处理后沿用保存的返回地址。断点和非法指令异常由相应的处理函数更新 `tf->epc`，使返回后可以继续执行后续指令。
3. **恢复现场并返回。** `__trapret` 展开 `RESTORE_ALL`，从最终 Trap frame 的第 32、33 个槽位（从 0 开始编号）读取 `status`、`epc` 并写回 `sstatus`、`sepc`。恢复其余通用寄存器期间仍用当前 `sp` 定位各槽位，最后才从 `2 × REGBYTES` 处恢复原始 `sp`。随后由 `sret` 按恢复后的状态和地址继续执行。

#### 最终提示词

以下是经过迭代优化后，最终成功实现该功能的提示词：

````markdown
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
````

#### 实现迭代过程

##### 第一次迭代

依据 `SAVE_ALL` 与 `struct trapframe` 的对应关系补全 `RESTORE_ALL`，在 `__alltraps` 中加入向 `trap()` 传参和调用的指令。检查返回路径时，重点确认所有现场读取完成后才恢复 `sp`，并确认写入 `sepc` 的值来自 C 函数处理后的 `tf->epc`。

**遇到的问题与处理方法**

1. **工具链不识别原框架中的 CSR 名称。** 汇编时，`SAVE_ALL` 中的 `sbadaddr` 触发 `unknown CSR` 报错。本机工具链使用 `stval` 指代同一 CSR，因此在构建时传入 `DEFS+=-Dsbadaddr=stval`，由预处理器处理名称兼容。使用该选项后，`make obj/kern/trap/trapentry.o` 通过。反汇编确认了 `a0` 传参、`sstatus` 与 `sepc` 写回，以及最后恢复 `sp` 后执行 `sret` 的顺序。
2. **初次整体构建时模块二尚未合入。** `trap_dispatch()` 引用的 `interrupt_handler()` 和 `exception_handler()` 当时尚未定义，编译报 `implicit declaration of function`。因此先保留模块一的汇编验证结果，待模块二提供中断与异常处理后再运行集成测试。

##### 模块二合入后的联调

合入后，使用上述 CSR 兼容选项运行 `make grade`。第一次按评分脚本的默认方式加载镜像时，三个用例均未进入内核，OpenSBI 日志显示 `Domain0 Next Address` 为 `0x0`。复测时在临时评测环境中以 `-kernel bin/ucore.img` 向 QEMU 7.0.0 指定启动镜像，执行仓库原有的三个评分用例。

| 验收用例 | 验证内容 | 得分 |
| --- | --- | --- |
| Trap 入口、寄存器恢复与断点返回 | 断点处理后从更新的地址继续执行，测试覆盖的寄存器恢复为触发断点前的值。 | 35/35 |
| 非法指令异常返回 | 非法指令处理后继续执行，并完成处理函数的兜底分支检查。 | 25/25 |
| 连续 S 模式时钟中断 | 时钟事件重新设置后能多次进入并返回 Trap，观察到 `100 ticks` 输出且计数达到 200。 | 40/40 |
| **合计** | **三项构建和 QEMU 运行检查通过。** | **100/100** |

---

### 练习：[练习标题]

**负责人：** [学号-姓名]

[按照练习的具体要求进行解答]

---

### Challenge：[Challenge标题]

**负责人：** [学号-姓名]

[按照Challenge的具体要求进行解答]

---

## 五、测试与验证

<!-- 根据该实验的具体情况，提供完整的测试运行截图，应包含：
- 编译及运行成功的输出（make qemu），可能有多个
- 测试通过的结果（make grade），只有一个
-->

**测试截图：**

![测试结果截图](./images/test_result.png)

---

## 六、实验总结与收获

### 对操作系统的理解

1. 列出你们认为本实验中重要的知识点，以及与对应的 OS 原理中的知识点，并简要说明二者的含义、关系和差异
2. 列出你们认为 OS 原理中很重要、但在本实验中没有对应上的知识点

### AI 协作开发的经验

[写出你们在与 AI 协作过程中实际收获的经验或者感悟]

---

