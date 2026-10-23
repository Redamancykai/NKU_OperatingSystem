#!/usr/bin/env python3
"""Run from WSL/Linux; all scratch/build files stay inside the repository."""
import argparse
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
TEMP = ROOT / 'tmp'
EVIDENCE = ROOT / 'report/evidence'


def run(args, name, *, env=None, timeout=120):
    result = subprocess.run(args, cwd=ROOT, env=env, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            timeout=timeout)
    (EVIDENCE / name).write_text('$ ' + ' '.join(map(str, args)) + '\n' +
                                 result.stdout, encoding='utf-8')
    if result.returncode:
        raise RuntimeError(f'{name}: exit {result.returncode}\n{result.stdout[-3000:]}')
    return result.stdout


def host_checks():
    env = dict(os.environ, TMPDIR=str(TEMP))
    target = TEMP / 'alloc-host'
    run(['gcc', '-std=gnu99', '-Wall', '-Wextra', '-Werror', '-fno-builtin',
         '-DTEST_SLUB', '-fsanitize=address,undefined', '-g',
         '-Icode/libs', '-Icode/kern/mm', '-Icode/kern/debug',
         'code/tests/buddy_host.c', 'code/kern/mm/buddy_test.c',
         'code/kern/mm/buddy_pmm.c', 'code/kern/mm/slub_test.c',
         'code/kern/mm/slub.c', '-o', str(target)], 'host-build.log', env=env)
    run([str(target)], 'host-tests.log', env=env)
    log = []
    expected = {'d': 'ALLOC_HEAD', 's': 'base->property == n',
                'i': 'ALLOC_HEAD', 'r': 'page_ref', 'j': 'offset % object_size',
                'l': 'ptr ==', 'f': 'allocated[index / 64] & bit'}
    for mode, message in expected.items():
        result = subprocess.run([str(target), mode], cwd=ROOT, env=env,
                                text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, timeout=30)
        if result.returncode != 77 or message not in result.stdout:
            raise RuntimeError(f'negative case {mode}: {result.returncode}\n{result.stdout}')
        log.append(f'{mode}: rejected as expected (exit 77)\n{result.stdout}')
    (EVIDENCE / 'invalid-free-tests.log').write_text('\n'.join(log), encoding='utf-8')
    print('Host sanitizers, exhaustion, maximum order, 7 invalid-free cases: PASS', flush=True)


def boot(qemu, manager, ram):
    image = ROOT / 'code/bin/ucore.img'
    args = [qemu, '-machine', 'virt', '-m', str(ram), '-nographic',
            '-bios', 'default', '-kernel', str(image)]
    process = subprocess.Popen(args, cwd=ROOT, text=True,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    try:
        output, _ = process.communicate(timeout=40)
    except subprocess.TimeoutExpired:
        process.terminate()
        output, _ = process.communicate(timeout=5)
    path = EVIDENCE / f'qemu-{manager}-{ram}M.log'
    path.write_text('$ ' + ' '.join(map(str, args)) + '\n' + output, encoding='utf-8')
    required = [f'memory management: {manager}_pmm_manager',
                'check_alloc_page() succeeded!',
                '[SLUB] 3000 mixed-size allocations, no page leaks: PASS']
    if manager == 'buddy':
        required += ['[Buddy] real memory accounting: PASS']
    if any(marker not in output for marker in required) or 'panic' in output.lower():
        raise RuntimeError(f'Kernel {manager} {ram}M failed; see {path}')
    print(f'QEMU {manager} {ram} MiB: PASS (kernel idle loop terminated after checks)', flush=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--qemu', default='qemu-system-riscv64')
    parser.add_argument('--host-only', action='store_true')
    parser.add_argument('--ram', type=int, nargs='+', default=[32, 128, 256])
    args = parser.parse_args()
    TEMP.mkdir(exist_ok=True)
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    run(['git', 'rev-parse', 'HEAD'], 'base-commit.log')
    host_checks()
    if args.host_only:
        return
    for manager in ['buddy', 'default']:
        run(['make', '-C', 'code', '-j1', f'PMM={manager}'],
            f'build-{manager}.log')
        for ram in (args.ram if manager == 'buddy' else [128]):
            boot(args.qemu, manager, ram)
    # Leave the shared build ready for the default Best-Fit lab workflow.
    run(['make', '-C', 'code', '-j1'], 'build-best_fit.log')


if __name__ == '__main__':
    main()
