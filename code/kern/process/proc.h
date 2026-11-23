#ifndef __KERN_PROCESS_PROC_H__
#define __KERN_PROCESS_PROC_H__

#include <defs.h>
#include <list.h>
#include <trap.h>
#include <memlayout.h>

// process's state in his life cycle
enum proc_state
{
    PROC_UNINIT = 0, // uninitialized
    PROC_SLEEPING,   // sleeping
    PROC_RUNNABLE,   // runnable(maybe running)
    PROC_ZOMBIE,     // almost dead, and wait parent proc to reclaim his resource
};

// 线程切换时保存的寄存器
// 因为线程切换在一个函数当中，所以编译器会自动帮助我们生成保存和恢复调用者保存寄存器的代码
// 在实际的进程切换过程中我们只需要保存被调用者保存寄存器即可
struct context
{
    uintptr_t ra; // 恢复后通过 ret 跳转到哪里
    uintptr_t sp; // 恢复后使用哪个栈、栈中的哪个位置
    uintptr_t s0;
    uintptr_t s1;
    uintptr_t s2;
    uintptr_t s3;
    uintptr_t s4;
    uintptr_t s5;
    uintptr_t s6;
    uintptr_t s7;
    uintptr_t s8;
    uintptr_t s9;
    uintptr_t s10;
    uintptr_t s11;
};

#define PROC_NAME_LEN 15
#define MAX_PROCESS 4096
#define MAX_PID (MAX_PROCESS * 2)

extern list_entry_t proc_list;

struct proc_struct
{
    enum proc_state state;        // Process state 调度器选择 PROC_RUNNABLE 的线程
    int pid;                      // Process ID 通过 find_proc(pid) 查找
    int runs;                     // the running times of Proces 被调度选中的次数，schedule() 中执行 next->runs++
    uintptr_t kstack;             // Process kernel stack 新建线程的内核栈是 8KiB
    volatile bool need_resched;   // bool value: need to be rescheduled to release CPU? cpu_idle() 检查它并调用 schedule()
    struct proc_struct *parent;   // the parent process
    struct mm_struct *mm;         // Process's memory management field 定义在 vmm.h
    struct context context;       // Switch here to run process 内核线程切换时恢复执行
    struct trapframe *tf;         // Trap frame for current interrupt 陷入返回，以及构造新线程首次执行现场
    uintptr_t pgdir;              // the base addr of Page Directroy Table(PDT)
    uint32_t flags;               // Process flag
    char name[PROC_NAME_LEN + 1]; // Process name
    list_entry_t list_link;       // Process link list 把控制块接入进程链表，便于遍历、调度
    list_entry_t hash_link;       // Process hash list 把控制块接入按 PID 组织的哈希桶，便于查找
};

// 根据线程控制块中的链表节点地址，反推出整个 proc_struct 的地址
#define le2proc(le, member) \
    to_struct((le), struct proc_struct, member)

extern struct proc_struct *idleproc, *initproc, *current;

void proc_init(void);
void proc_run(struct proc_struct *proc);
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);

char *set_proc_name(struct proc_struct *proc, const char *name);
char *get_proc_name(struct proc_struct *proc);
void cpu_idle(void) __attribute__((noreturn));

struct proc_struct *find_proc(int pid);
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
int do_exit(int error_code);

#endif /* !__KERN_PROCESS_PROC_H__ */
