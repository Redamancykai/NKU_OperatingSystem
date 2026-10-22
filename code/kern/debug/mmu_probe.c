// lab2 branch task 2412677: deterministic load miss followed by a TLB hit.
// Included only by make MMU_DEBUG=1; the regular kernel has no probe overhead.
#ifdef MMU_DEBUG
#include <defs.h>
volatile uint64_t mmu_probe_word[2] __attribute__((aligned(4096))) = {0, 0x2412677};
volatile uint64_t mmu_probe_result;
void __attribute__((noinline)) mmu_debug_probe(void) {
    uint64_t first, second;
    asm volatile(
        "sfence.vma\n"
        ".global mmu_debug_load\n"
        "mmu_debug_load:\n"
        "ld %0, 0(%2)\n"
        ".global mmu_debug_load_again\n"
        "mmu_debug_load_again:\n"
        "ld %1, 0(%2)\n"
        : "=&r"(first), "=&r"(second)
        : "r"(&mmu_probe_word[1])
        : "memory");
    mmu_probe_result = first + second;
}
#endif
