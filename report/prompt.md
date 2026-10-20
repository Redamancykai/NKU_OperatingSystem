# 功能模块一：物理内存初始化功能
## 任务一：实现物理内存探测与页描述符初始化功能
### 负责人：黄泽恺 2411268

[PROMPT]

**任务**：完成 ucore Lab2“功能模块一：物理内存初始化功能”的任务一，在 [pmm.c](C:/Users/hzk65/Desktop/操作系统/lab2/kern/mm/pmm.c) 中实现 `static void page_init(void)`，根据设备树提供的物理内存信息建立 Page 描述符数组，并将排除 OpenSBI、内核镜像及描述符数组占用空间后的完整空闲页面交给当前物理内存管理器。

**操作要求**：必须修改项目中的实际文件，在 `//lab2 function1 YOUR CODE` 标记处补全函数。保留已有代码和接口，本次修改限于该任务，不修改内存分配算法或切换 `pmm_manager`。

**输出要求**：依据下面的上下文、接口清单和行为规格完成实现，以项目真实代码为准。完成后简要说明内存布局、边界处理及实际执行的验证结果；无法执行的验证应明确说明。

[RELY]

**1. 初始化流程**

`kern_init()` 已在调用 `pmm_init()` 前执行 `dtb_init()`，解析并保存设备树中的内存信息。`pmm_init()` 先调用 `init_pmm_manager()` 初始化当前管理器，再调用待实现的 `page_init()`，随后执行已有的分配器检查。

**2. 已有宏定义**

以下定义已存在于工程中，直接使用，不要重复定义：

```c
#define DRAM_BASE               0x80000000
#define PGSIZE                  4096
#define PGSHIFT                 12
#define PPN(la)                 (((uintptr_t)(la)) >> PGSHIFT)

#define KERNBASE                0xFFFFFFFFC0200000
#define PHYSICAL_MEMORY_END     0x88000000
#define PHYSICAL_MEMORY_OFFSET  0xFFFFFFFF40000000
#define KERNEL_BEGIN_PADDR      0x80200000
```

工程还提供：

- `ROUNDUP(a, n)`、`ROUNDDOWN(a, n)`：向上、向下对齐。
- `PADDR(kva)`：将内核虚拟地址转换为物理地址，转换依赖 `va_pa_offset`，必须先正确设置该变量。
- `SetPageReserved(page)`、`PageReserved(page)`：设置、检查页面的保留状态。
- `panic(...)`：报告错误并终止执行。

当前工程中的 `KADDR` 宏被注释，不能直接调用。

**3. 已有数据结构与全局变量**

```c
struct Page {
    int ref;
    uint64_t flags;
    unsigned int property;
    list_entry_t page_link;
};

struct Page *pages;
size_t npage = 0;
uint64_t va_pa_offset;
const size_t nbase = DRAM_BASE / PGSIZE;
```

上述全局变量已定义，不要再次定义。

必须明确以下地址关系：

- `nbase` 是 DRAM 起始物理页号。
- `npage` 是可管理物理页号的排他上界，描述符数量为 `npage - nbase`。
- `pages` 保存描述符数组的**虚拟地址**。
- `pages[i]` 描述的物理页起始地址为 `(nbase + i) * PGSIZE`。
- 链接符号 `end` 表示内核镜像结束位置的**虚拟地址**，使用时可声明 `extern char end[];`。
- 本实验中 `[0x80000000, 0x80200000)` 为 OpenSBI 占用区，其后为内核镜像。
- 设备树中的内存信息已被保存，按本实验约定，无须额外保留原始 DTB 所占页面。

**4. 已有可调用接口**

```c
uint64_t get_memory_base(void);
uint64_t get_memory_size(void);
int cprintf(const char *fmt, ...);

static void init_memmap(struct Page *base, size_t n);
static inline struct Page *pa2page(uintptr_t pa);
```

`get_memory_base()` 和 `get_memory_size()` 返回设备树解析得到的物理内存起点与字节数。

`init_memmap()` 是当前文件中已有的包装函数，调用当前 `pmm_manager->init_memmap()`。传入的页面数量必须大于零，且对应描述符必须预先处于保留状态；空闲链表和空闲块属性由管理器负责初始化。

`pa2page()` 根据物理页号与 `nbase` 的差值定位描述符，调用时必须保证物理地址落在有效的描述符覆盖范围内。

[GUARANTEE]

必须实现以下接口，保持函数名、返回类型、参数及 `static` 属性不变：

```c
static void page_init(void);
```

函数不接收参数、不返回结果，通过更新全局变量和注册初始空闲区完成初始化。

如确有必要，可在当前文件增加少量 `static` 辅助函数，但不得改变公开接口，也不需要重新实现已有依赖函数。

[SPECIFICATION]

**page_init**

**Pre-Condition**

- 系统处于启动初始化阶段，已建立内核启动映射。
- `dtb_init()` 已执行，内存信息可通过已有接口查询；查询结果仍需检查有效性。
- 当前 `pmm_manager` 已选定并初始化，其初始空闲区为空。
- 目标为当前工程采用的 QEMU RISC-V 单段连续 DRAM 布局。

**Post-Condition**

成功完成初始化后：

1. `va_pa_offset` 与当前启动映射一致，值为 `PHYSICAL_MEMORY_OFFSET`。
2. 已输出设备树报告的物理内存起点、大小和范围，正确区分排他末端与最后一个有效字节。
3. 可管理物理内存上界不超过设备树报告的末端和 `PHYSICAL_MEMORY_END`，末尾不足一页的部分不参与管理；`npage` 与该上界对应。
4. `pages` 位于内核镜像结束位置之后，并按 `PGSIZE` 对齐；数组连续覆盖物理页号区间 `[nbase, npage)`。
5. 描述符数组自身及其覆盖的物理内存范围合法，不越过可管理上界。
6. 注册空闲区之前，全部描述符先处于保留状态。OpenSBI、内核镜像、描述符数组及相关对齐空隙所在页面不得进入空闲区。
7. 只有描述符数组之后、可管理上界之前的完整页面可以交给物理内存管理器。

**Case 1：内存信息有效，存在完整空闲页面**

建立上述描述符布局和保留状态。将描述符数组结束位置转换为物理地址，再向上按页对齐，确定空闲区起点；以向下按页对齐后的可管理上界作为空闲区末端。通过 `pa2page()` 获得起始描述符，并调用已有 `init_memmap()` 注册该连续区间，页面数量必须为正且计算准确。

**Case 2：布局有效，但没有完整空闲页面**

保持初始空闲区为空，不调用 `init_memmap()`。不得因起点大于或等于末端而发生无符号减法下溢，也不得注册零页或不完整页面。

**Case 3：内存信息无效或布局无法成立**

当设备树返回的起点或大小为零、起点加大小发生溢出、报告范围与当前平台布局不兼容，或内存无法容纳内核与描述符数组时，使用 `panic()` 终止初始化。应在写入描述符数组前确认其范围有效，避免越界访问。

**Requirements**

- 严格区分虚拟地址、物理地址、物理页号和描述符指针，采用与工程一致的地址及长度类型。
- 先设置地址转换所需状态，再使用相关转换接口；先完成描述符保留初始化，再注册空闲区。
- 从设备树读取实际内存大小，不能将默认的 128 MiB 写死为探测结果。
- 使用 `sizeof(struct Page)` 计算描述符存储空间，不假定结构体大小。
- 通过已有 `init_memmap()` 完成空闲区注册，保留后续分配器检查逻辑。