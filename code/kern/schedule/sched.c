#include <list.h>
#include <sync.h>
#include <proc.h>
#include <sched.h>
#include <assert.h>

void
wakeup_proc(struct proc_struct *proc) {
    assert(proc->state != PROC_ZOMBIE && proc->state != PROC_RUNNABLE);
    proc->state = PROC_RUNNABLE;
}

//LAB4 function3 2412677
void
schedule(void) {
    // 保存中断状态、关闭本地中断，再清除当前线程的调度请求
    bool intr_flag;
    local_intr_save(intr_flag);
    current->need_resched = 0;
    // 当前进程不是 idleproc：从当前进程的下一个位置开始查找
    // 当前进程是 idleproc：从链表头开始查找，给所有进程平等机会
    // next 是准备运行的线程，默认用 idleproc 兜底
    list_entry_t *start = (current == idleproc) ? &proc_list : &current->list_link;
    list_entry_t *entry = start;
    struct proc_struct *next = idleproc;
    // 找到下一个就绪线程
    do {
        entry = list_next(entry);
        if (entry != &proc_list) {
            struct proc_struct *proc = le2proc(entry, list_link);
            if (proc->state == PROC_RUNNABLE) {
                next = proc;
                break;
            }
        }
    } while (entry != start);

    next->runs++;
    if (next != current) {
        proc_run(next);
    }
    local_intr_restore(intr_flag); // 恢复中断
}


