# NKU Operating System · Lab2

默认构建使用 Best-Fit；Buddy 和 First-Fit 通过 PMM 参数选择。请在 WSL/Linux 中操作。依赖 make、RISC-V GCC/binutils、QEMU，调试另需 RISC-V GDB 或 gdb-multiarch。

```sh
# 仓库根目录
make -C code qemu
make -C code PMM=buddy qemu
make -C code PMM=default qemu
make -C code grade
```

产物为 `code/bin/kernel`、`code/bin/ucore.img` 和 `code/obj/`。切换 PMM 会重新编译；不传 PMM 则回到 Best-Fit。QEMU 自检结束后内核停留在空循环，退出按 Ctrl+A，再按 X。

普通内核调试使用两个终端，均在 `code/` 目录：

```sh
# 终端一
make debug
# 终端二
make gdb
```

调试 Buddy 时两个终端都加 `PMM=buddy`。也可在 `make debug` 后运行 `gdb-multiarch -x tools/gdbinit`。无需 MMU_DEBUG 参数。不要在调试运行期间切换算法或重新构建另一份内核。

分支任务请按实验指导书 PDF 第 37～42 页亲自操作 QEMU 源码调试，观察断点、地址转换和 TLB，并解释实际结果。

Buddy/SLUB 独立测试保留为算法验证工具：

```sh
python3 code/tools/check_member_c.py
python3 code/tools/check_member_c.py --host-only
```

测试覆盖、历史结果见 [小组报告](report/report.md)。Buddy 要求按原首指针和原请求页数整块释放；SLUB 是单核教学实现。

启动兼容性保留两项：使用 QEMU `-kernel` 向 OpenSBI 提供内核镜像；根页表第 2 项保持物理 PC 在开启分页后到跳往高地址之间可取指，第 511 项仍提供高地址映射。它们不依赖特殊调试模式。

临时测试、评分副本和调试版 QEMU 放仓库 `tmp/`；标准构建产物保留在 `code/bin`、`code/obj` 并由 Git 忽略。

评分脚本只调整日志等待与本次 QEMU 子进程清理：等到第二项测试结果或超时再判断，原有两项 PASS 条件不变。
