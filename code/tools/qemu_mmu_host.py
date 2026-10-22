"""Run inside native x86-64 GDB, sourced by debug_mmu.py (QEMU 4.1.1)."""
import os
from pathlib import Path
import gdb

target = int(os.environ['LAB2_MMU_TARGET'], 16)
source = Path(os.environ['LAB2_QEMU_SOURCE'])
helper = source / 'target/riscv/cpu_helper.c'
lines = helper.read_text().splitlines()
pte_line = next(i for i, s in enumerate(lines, 1) if 'target_ulong ppn = pte >>' in s)
physical_line = next(i for i, s in enumerate(lines, 1) if '*physical = (ppn |' in s)
cputlb_lines = (source/'accel/tcg/cputlb.c').read_text().splitlines()
load_line = next(i for i, s in enumerate(cputlb_lines, 1) if s.startswith('load_helper('))
miss_line = next(i for i, s in enumerate(cputlb_lines, 1)
                 if i > load_line and 'if (!tlb_hit(tlb_addr, addr))' in s)


def show(label, expressions):
    gdb.write('\n' + label + '\n')
    for expr in expressions:
        gdb.execute('p/x ' + expr)
    gdb.flush()


class LoadLookup(gdb.Breakpoint):
    def stop(self):
        if int(gdb.parse_and_eval('addr')) != target:
            return False
        show('C_TLB_LOOKUP', ['addr', 'index', 'tlb_addr', 'entry->addend', 'mmu_idx'])
        return False


class CountWalk(gdb.Breakpoint):
    def stop(self):
        if int(gdb.parse_and_eval('addr')) != target or int(gdb.parse_and_eval('access_type')) != 0:
            return False
        caller = gdb.newest_frame().older()
        if caller is None or caller.name() != 'riscv_cpu_tlb_fill':
            return False  # GDB memory inspection is not a CPU data access.
        gdb.write('UNEXPECTED_SECOND_DATA_WALK\n')
        gdb.flush()
        return False


bare = gdb.Breakpoint('get_physical_address')
bare.condition = 'addr == 0x80200000 && access_type == 2 && env->satp == 0'
bare_store = gdb.Breakpoint('get_physical_address')
bare_store.condition = f'addr == {int(os.environ["LAB2_BARE_STORE"], 16):#x} && access_type == 1 && env->satp == 0'
walk = gdb.Breakpoint('get_physical_address')
walk.condition = f'addr == {target:#x} && access_type == 0'
lookup = LoadLookup(f'cputlb.c:{miss_line}')
lookup.condition = f'addr == {target:#x}'

gdb.execute('run')
show('BARE_TRANSLATION', ['addr', 'env->satp', 'mmu_idx', 'access_type'])
gdb.execute('set $bare_out = physical')
gdb.execute('finish')
show('BARE_RESULT', ['*$bare_out'])
bare.enabled = False

gdb.execute('continue')
show('BARE_DATA_STORE', ['addr', 'env->satp', 'mmu_idx', 'access_type'])
gdb.execute('bt 6')
gdb.execute('set $bare_store_out = physical')
gdb.execute('finish')
show('BARE_DATA_RESULT', ['*$bare_store_out'])
bare_store.enabled = False

gdb.execute('continue')
show('SV39_WALK_BEGIN', ['addr', 'env->satp', 'mmu_idx', 'access_type'])
gdb.execute('bt 8')
gdb.execute('set $walk_out = physical')
gdb.execute(f'tbreak cpu_helper.c:{pte_line} if addr == {target:#x}')
gdb.execute('continue')
show('ROOT_PTE', ['levels', 'ptidxbits', 'ptesize', 'i', 'ptshift', 'base', 'idx', 'pte_addr', 'pte'])
# Step over the PPN calculation and validity/leaf branches before reaching
# the superpage composition. This is actual source-level execution.
gdb.execute('next')
show('DECODED_PPN', ['ppn'])
gdb.execute(f'tbreak cpu_helper.c:{physical_line} if addr == {target:#x}')
gdb.execute('continue')
show('SUPERPAGE_COMPOSITION', ['ppn', 'vpn', 'ptshift'])
gdb.execute('next')
show('TRANSLATED_PAGE_AND_FINAL_PA', ['*$walk_out', f'(*$walk_out | ({target:#x} & 0xfff))'])
gdb.execute('finish')
walk.enabled = False

fill = gdb.Breakpoint('tlb_set_page_with_attrs')
fill.condition = f'vaddr == {target & ~4095:#x} && mmu_idx == 1'
gdb.execute('continue')
show('TLB_INSTALL', ['vaddr', 'paddr', 'prot', 'mmu_idx', 'size'])
gdb.execute('bt 6')
fill.enabled = False
again = CountWalk('get_physical_address')
again.condition = f'addr == {target:#x} && access_type == 0'
gdb.write('READY_FOR_SECOND_LOAD\n')
gdb.flush()
gdb.execute('continue')
