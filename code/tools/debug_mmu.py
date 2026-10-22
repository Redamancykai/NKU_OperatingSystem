#!/usr/bin/env python3
"""Reproduce dual-GDB page-walk observations with a debug QEMU 4.1.1."""
import argparse
import os
from pathlib import Path
import signal
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]
TEMP = ROOT / 'tmp/mmu-debug'
EVIDENCE = ROOT / 'report/evidence'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--qemu', type=Path,
                        default=ROOT/'tmp/qemu/build/riscv64-softmmu/qemu-system-riscv64')
    parser.add_argument('--source', type=Path, default=ROOT/'tmp/qemu/src')
    parser.add_argument('--port', type=int, default=12346)
    args = parser.parse_args()
    TEMP.mkdir(parents=True, exist_ok=True)
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    subprocess.run(['make', '--jobserver-style=pipe', '-C', 'code', '-j2', 'MMU_DEBUG=1'],
                   cwd=ROOT, check=True)
    kernel = ROOT/'tmp/build/buddy-debug/bin/kernel'
    image = kernel.with_name('ucore.img')
    symbols = subprocess.check_output(['riscv64-unknown-elf-nm', str(kernel)], text=True)
    values = {line.split()[-1]: int(line.split()[0], 16)
              for line in symbols.splitlines() if len(line.split()) == 3}
    target = values['mmu_probe_word'] + 8
    qemu = args.qemu.resolve()
    source = args.source.resolve()
    bare_store = values['boot_hartid'] - 0xffffffff40000000
    env = dict(os.environ, LAB2_MMU_TARGET=hex(target), LAB2_QEMU_SOURCE=str(source),
               LAB2_BARE_STORE=hex(bare_store))
    commands = TEMP/'host.gdb'
    commands.write_text('\n'.join([
        'set pagination off', 'set confirm off', 'set print pretty on',
        'set debuginfod enabled off', 'handle SIGPIPE nostop noprint pass',
        'handle SIGUSR1 nostop noprint pass',
        f'file {qemu}',
        f'set args -machine virt -nographic -bios default -kernel {image} -S -gdb tcp:127.0.0.1:{args.port}',
        f'source {ROOT}/code/tools/qemu_mmu_host.py', '']))
    guest = TEMP/'guest.gdb'
    guest.write_text('\n'.join([
        'set pagination off', 'set confirm off', 'set debuginfod enabled off',
        'set architecture riscv:rv64', 'set tcp connect-timeout 60',
        f'file {kernel}', f'target remote 127.0.0.1:{args.port}',
        'hbreak mmu_debug_load', 'continue',
        'printf "GUEST_LOAD_BEGIN\\n"', 'x/2i $pc',
        'info registers pc satp', 'p/x &mmu_probe_word[1]',
        'si', 'printf "FIRST_LOAD_DONE\\n"', 'info registers pc a4 a5',
        'si', 'printf "SECOND_LOAD_DONE\\n"', 'info registers pc a4 a5 a3',
        'detach', 'quit', '']))
    host_log = EVIDENCE/'mmu-host-gdb.log'
    guest_log = EVIDENCE/'mmu-guest-gdb.log'
    with host_log.open('w') as out:
        host = subprocess.Popen(['gdb', '-nx', '-batch', '-x', str(commands)],
                                cwd=ROOT, env=env, stdout=out, stderr=out,
                                start_new_session=True)
        try:
            time.sleep(1)
            result = subprocess.run(['gdb-multiarch', '-nx', '-batch', '-x', str(guest)],
                                    cwd=ROOT, text=True, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, timeout=120)
            guest_log.write_text(result.stdout, encoding='utf-8')
            if result.returncode:
                raise RuntimeError(result.stdout)
            time.sleep(0.5)
        finally:
            os.killpg(host.pid, signal.SIGTERM)
            host.wait(timeout=10)
    text = host_log.read_text()
    required = ['BARE_RESULT', 'BARE_DATA_RESULT', 'ROOT_PTE', 'TRANSLATED_PAGE_AND_FINAL_PA',
                'TLB_INSTALL', 'READY_FOR_SECOND_LOAD']
    if any(s not in text for s in required) or 'UNEXPECTED_SECOND_DATA_WALK' in text:
        raise RuntimeError(f'Incomplete trace: {host_log}')
    if 'SECOND_LOAD_DONE' not in result.stdout:
        raise RuntimeError(f'Second load not executed: {guest_log}')
    (EVIDENCE/'mmu-symbols.log').write_text(
        f'probe_va={target:#x}\nroot_va={values["boot_page_table_sv39"]:#x}\n'
        f'root_pa={values["boot_page_table_sv39"]-0xffffffff40000000:#x}\n'
        f'expected_pa={target-0xffffffff40000000:#x}\nbare_store_pa={bare_store:#x}\n'
        f'qemu={qemu}\n' + subprocess.check_output([str(qemu), '--version'], text=True))
    print('Dual GDB: Bare, Sv39 root leaf, VA/PA, TLB fill and second load: PASS')


if __name__ == '__main__':
    main()
