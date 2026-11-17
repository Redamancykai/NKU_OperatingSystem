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