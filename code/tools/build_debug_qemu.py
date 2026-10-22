#!/usr/bin/env python3
"""Build the lab's QEMU version locally, without replacing installed QEMU."""
import argparse
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / 'tmp/qemu'
COMMIT = '99c5874a9b6c9f70aef285d6eff85d4f46de3c52'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--jobs', type=int, default=4)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    BASE.mkdir(parents=True, exist_ok=True)
    source = BASE / 'src'
    if not source.exists():
        subprocess.run(['git', '-c', 'core.autocrlf=false', 'clone', '--depth', '1',
                        '--branch', 'v4.1.1', 'https://github.com/qemu/qemu.git',
                        str(source)], check=True)
    sha = subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip()
    if sha != COMMIT:
        raise RuntimeError(f'Expected QEMU v4.1.1 {COMMIT}, found {sha}')
    if b'\r\n' in (source/'configure').read_bytes():
        raise RuntimeError('QEMU source has CRLF; clone with core.autocrlf=false in WSL')
    build = BASE / 'build'
    build.mkdir(exist_ok=True)
    flags = ['--target-list=riscv64-softmmu', '--enable-debug', '--disable-werror',
             '--disable-docs', '--disable-capstone', '--disable-slirp',
             '--disable-gtk', '--disable-sdl', '--disable-vnc', '--disable-curl',
             '--disable-gnutls', '--disable-nettle', '--disable-gcrypt']
    env = dict(os.environ, TMPDIR=str(ROOT/'tmp'))
    for command, logname in [([str(source/'configure'), *flags], 'configure.log'),
                              (['make', '--jobserver-style=pipe', f'-j{args.jobs}'], 'build.log')]:
        print('Running:', command[0], '(log:', BASE/logname, ')', flush=True)
        with (BASE/logname).open('w') as log:
            subprocess.run(command, cwd=build, env=env, stdout=log,
                           stderr=subprocess.STDOUT, check=True)
    print('Debug QEMU:', build/'riscv64-softmmu/qemu-system-riscv64')


if __name__ == '__main__':
    main()
