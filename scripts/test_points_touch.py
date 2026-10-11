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
    profiles = [
        ('watch', [], 4),
        ('home', ['-DPORTABLE_INPUT_NAVIGATION', '-DPORTABLE_HOME_APP="home.elf"',
                  '-DPOINTS_RETURN_APP="springboard.elf"'], 6),
    ]
    for profile, defines, scenario_count in profiles:
        for sanitized in (False, True):
            binary = output/f'touch-{profile}-{int(sanitized)}'
            flags = ['-DPORTABLE_NOVA_UI', '-DPORTABLE_ALARM_CLIENT', '-DPORTABLE_FORCE_FULL_FRAMES', *defines]
            if sanitized:
                flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
            subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', *flags,
                            *['-I'+str(path) for path in includes], str(ROOT/'test/native_apps/points_touch_adapter_test.c'),
                            str(args.system_apps/'lib/PortableApps/src/adapter.c'), '-o', str(binary)], check=True, timeout=120)
            for scenario in range(scenario_count):
                subprocess.run([str(binary), str(scenario)], check=True, timeout=30)
    print('Points actual-adapter touch, drag, keyboard, local Back and terminal Home normal/ASan/UBSan checks passed')
if __name__ == '__main__':
    main()
