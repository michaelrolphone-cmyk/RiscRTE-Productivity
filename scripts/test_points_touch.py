#!/usr/bin/env python3
"""Run raw-touch provider events through production Points and its real adapter."""
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
    includes = [args.utilities/'lib/Alarm/include', args.system_apps/'lib/PortableApps/include',
                args.system_apps/'lib/NativeApps/include', args.system_apps/'Apps']
    output = ROOT/'build/points-touch'
    output.mkdir(parents=True, exist_ok=True)
    for sanitized in (False, True):
        binary = output/f'touch-{int(sanitized)}'
        flags = ['-DPORTABLE_NOVA_UI', '-DPORTABLE_ALARM_CLIENT', '-DPORTABLE_FORCE_FULL_FRAMES']
        if sanitized:
            flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', *flags,
                        *['-I'+str(path) for path in includes], str(ROOT/'test/native_apps/points_touch_adapter_test.c'),
                        str(args.system_apps/'lib/PortableApps/src/adapter.c'), '-o', str(binary)], check=True, timeout=120)
        for scenario in range(3):
            subprocess.run([str(binary), str(scenario)], check=True, timeout=30)
    print('Points actual-adapter touch, held drag, release de-duplication and keyboard normal/ASan/UBSan checks passed')
if __name__ == '__main__':
    main()
