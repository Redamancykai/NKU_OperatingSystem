# 郑奕杰 Buddy／SLUB 测试记录

使用该提交的独立代码副本执行测试，以下项目均通过：

| 测试项目 | 日志 |
|---|---|
| 宿主 Buddy／SLUB、ASan/UBSan、最大阶与耗尽恢复 | `host-tests.log` |
| 7 项非法释放的预期断言及退出码检查 | `invalid-free-tests.log` |
| QEMU 10.2.1：Buddy 32／128／256 MiB，含 SLUB | `qemu-buddy-*.log` |
| QEMU 10.2.1：First-Fit 128 MiB，含 SLUB | `qemu-default-128M.log` |
| QEMU 4.1.1：Buddy 128 MiB，含 SLUB | `qemu41/qemu-buddy-128M.log` |

专项测试命令（仓库根目录）：

```sh
python3 code/tools/check_member_c.py
```

QEMU 4.1.1 补充测试使用 `PMM=buddy` 构建，以 128 MiB RAM 和 `-kernel` 参数启动。具体命令、工具版本及源码校验值保存在对应日志和 `source-manifest.json` 中。

内核自检成功后主动结束模拟器。256 MiB 为模拟 RAM 容量，框架物理内存管理上界为 `0x88000000`。
