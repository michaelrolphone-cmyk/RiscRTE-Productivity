#!/usr/bin/env python3
"""Test actual Points app source, never hardware or deployment qualification."""
import argparse
import os
import subprocess
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--system-apps', required=True, type=Path)
    parser.add_argument('--utilities', required=True, type=Path)
    args = parser.parse_args()
    includes = [args.utilities/'lib/Alarm/include', args.system_apps/'lib/PortableApps/include', args.system_apps/'lib/NativeApps/include']
    output = ROOT/'build/points-in-time'
    output.mkdir(parents=True, exist_ok=True)
    for variant, defines in [('bare', []), ('shared', ['-DPORTABLE_ALARM_CLIENT']),
                             ('return', ['-DPORTABLE_ALARM_CLIENT', '-DPOINTS_RETURN_APP="springboard.elf"'])]:
        for sanitized in (False, True):
            binary = output/f'ui-{variant}-{int(sanitized)}'
            flags = list(defines)
            if sanitized:
                flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
            subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', *flags,
                            *['-I'+str(path) for path in includes], str(ROOT/'test/native_apps/points_in_time_test.c'), '-o', str(binary)], check=True, timeout=120)
            subprocess.run([str(binary)], check=True, timeout=120)
    print('Points app bare/shared-client/root-return normal and ASan/UBSan production-source tests passed')
if __name__ == '__main__':
    main()
