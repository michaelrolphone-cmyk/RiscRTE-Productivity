#!/usr/bin/env python3
"""Exercise actual Points wheel code using the production NOVA pixel renderer."""
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
                args.system_apps/'lib/PortableApps/src', args.system_apps/'lib/NativeApps/include', args.system_apps/'Apps']
    output = ROOT/'build/points-picker'
    output.mkdir(parents=True, exist_ok=True)
    for sanitized in (False, True):
        binary = output/f'picker-{int(sanitized)}'
        frames = output/f'frames-{int(sanitized)}'
        frames.mkdir(exist_ok=True)
        flags = ['-DPORTABLE_NOVA_UI']
        if sanitized:
            flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', *flags,
                        *['-I'+str(path) for path in includes], str(ROOT/'test/native_apps/points_picker_renderer_test.c'),
                        '-o', str(binary)], check=True, timeout=120)
        subprocess.run([str(binary), str(frames)], check=True, timeout=120,
                       env={**os.environ, 'ASAN_OPTIONS': os.environ.get('ASAN_OPTIONS', 'detect_leaks=0')})
    print('Points actual-renderer picker normal and ASan/UBSan tests passed')
if __name__ == '__main__':
    main()
