# 成员 B 后续现场截图文字转录

来源：成员 B 在本次指导会话提供的真实终端截图。以下转录关键命令和输出，省略部分源码行及符号提示；不是 GDB 自动日志。自动日志 gdb_boot_current.log 保留原样。

## 栈设置与进入 C 函数

```text
(gdb) p/x &bootstacktop
$1 = 0x80203000
(gdb) si
0x0000000080200004 in kern_entry () at kern/init/entry.S:7
(gdb) info registers pc sp
pc  0x80200004 <kern_entry+4>
sp  0x80203000 <SBI_CONSOLE_PUTCHAR>
(gdb) si
0x0000000080200008 in kern_entry () at kern/init/entry.S:9
(gdb) info registers pc sp
pc  0x80200008 <kern_entry+8>
sp  0x80203000 <SBI_CONSOLE_PUTCHAR>
(gdb) si
kern_init () at kern/init/init.c:8
8  memset(edata, 0, end - edata);
(gdb) info registers pc
pc  0x8020000a <kern_init>
```

## 继续运行与暂停观察

```text
(gdb) continue
Continuing.
^C
Program received signal SIGINT, Interrupt.
kern_init () at kern/init/init.c:12
12  while (1)
(gdb) info registers pc
pc  0x8020003a <kern_init+48>
(gdb) x/3i $pc
=> 0x8020003a <kern_init+48>: j 0x8020003a <kern_init+48>
   0x8020003c <cputch>: addi sp,sp,-16
   0x8020003e <cputch+2>: sd s0,0(sp)
```

证据范围：已到达无限循环；本次调试未观察到启动消息，未进一步定位原因。普通 make qemu 的输出成功另有会话记录。
