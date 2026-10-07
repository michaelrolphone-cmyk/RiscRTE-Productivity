#!/usr/bin/env python3
"""Test the original Timecard model through its bounded NOVA portable profile."""
import argparse
import os
import sys
from pathlib import Path
import subprocess
ROOT = Path(__file__).resolve().parents[1]

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--system-apps', required=True, type=Path)
    p.add_argument('--runtime-appdata', type=Path, help='Also test the explicit app-data prototype bridge against this SDK')
    args = p.parse_args()
    system = args.system_apps.resolve()
    if args.runtime_appdata and (args.runtime_appdata.resolve()/'sdk/app/RiscAppDataV1.h').read_bytes() != (ROOT/'lib/PortableTimecard/include/RiscAppDataV1.h').read_bytes():
        raise ValueError('Selected Runtime app-data declaration differs from tested consumer bytes')
    includes = [ROOT/'lib/PortableTimecard/include', system/'lib/NativeApps/include', ROOT/'lib/NativeApps/include',
                system/'lib/PortableApps/include', system/'lib/PortableApps/src']
    out = ROOT/'build/timecard-portable'
    out.mkdir(parents=True, exist_ok=True)
    catalog = out/'catalog.c'
    catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
    for sanitized in (False, True):
        flags = ['-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror']
        if sys.platform == 'darwin':
            flags += ['-Wno-string-concatenation', '-Wno-misleading-indentation', '-Wno-unused-function']
        if sanitized:
            flags += ['-fsanitize='+os.environ.get('TIMECARD_SANITIZERS','address,undefined'), '-fno-sanitize-recover=all',
                      '-fno-omit-frame-pointer']
            if sys.platform != 'darwin':
                flags += ['-fno-pie', '-no-pie']
        env = {**os.environ, 'ASAN_OPTIONS': os.environ.get('ASAN_OPTIONS', 'detect_leaks=0')}
        for name, sources, defines in [
            ('validation', [ROOT/'tests/timecard_portable_validation_test.c'], []),
            ('profile', [ROOT/'test/native_apps/timecard_portable_test.c'],
             ['-DPORTABLE_NOVA_UI', '-DPORTABLE_APP_OWNS_TOUCH_CHROME']),
            ('adapter', [ROOT/'test/native_apps/timecard_portable_adapter_test.c', ROOT/'Apps/timecard_portable.c',
                         system/'lib/PortableApps/src/adapter.c', catalog],
             ['-DPORTABLE_NOVA_UI', '-DPORTABLE_APP_OWNS_TOUCH_CHROME', '-DPORTABLE_FORCE_FULL_FRAMES',
              '-DPORTABLE_INPUT_NAVIGATION', '-DPORTABLE_INPUT_NAVIGATION_LOCAL']),
            ('watch-home', [ROOT/'test/native_apps/timecard_watch_navigation_test.c', ROOT/'Apps/timecard_portable.c',
                         system/'lib/PortableApps/src/adapter.c', catalog],
             ['-DPORTABLE_NOVA_UI', '-DPORTABLE_APP_OWNS_TOUCH_CHROME', '-DPORTABLE_FORCE_FULL_FRAMES',
              '-DPORTABLE_INPUT_NAVIGATION', '-DPORTABLE_INPUT_NAVIGATION_LOCAL', '-DPORTABLE_HOME_APP="default.elf"']),
            ('time-denver', [ROOT/'test/native_apps/timecard_portable_time_test.c'],
             ['-DPORTABLE_NOVA_UI', '-DPORTABLE_APP_OWNS_TOUCH_CHROME', '-DPORTABLE_RTC_UTC8_DENVER']),
            ('shared-retention', [ROOT/'test/native_apps/timecard_shared_retention_test.c'],
             ['-DPORTABLE_NOVA_UI', '-DPORTABLE_APP_OWNS_TOUCH_CHROME']),
            ('legacy-ui', [ROOT/'Apps/timecard.c', ROOT/'test/native_apps/timecard_ui_test.c'], []),
            ('legacy-overlap', [ROOT/'tests/timecard_overlap_test.c'], []),
            ('legacy-clock', [ROOT/'test/native_apps/timecard_clock_failure_test.c'], []),
            ('legacy-storage', [ROOT/'test/native_apps/timecard_store_failure_test.c'], []),
        ]:
            binary = out/f'{name}-{int(sanitized)}'
            subprocess.run([os.environ.get('CC', 'cc'), *flags, *defines,
                            *['-I'+str(path) for path in includes], *map(str, sources),
                            '-o', str(binary)], check=True, timeout=120)
            run = [str(binary)]
            if name in ('profile', 'adapter', 'watch-home'):
                frames = out/f'{name}-frames-{int(sanitized)}'
                frames.mkdir(exist_ok=True)
                run.append(str(frames))
            subprocess.run(run, check=True, timeout=120, env=env)
        api_include = ROOT/'lib/PortableTimecard/include'
        for name, source in [('appdata-bridge', ROOT/'tests/timecard_appdata_bridge_test.c'),
                             ('appdata-profile', ROOT/'test/native_apps/timecard_appdata_profile_test.c')]:
            binary = out/f'{name}-{int(sanitized)}'
            subprocess.run([os.environ.get('CC', 'cc'), *flags, '-DPORTABLE_NOVA_UI', '-DPORTABLE_APP_OWNS_TOUCH_CHROME',
                            '-I'+str(api_include), *['-I'+str(path) for path in includes], str(source), '-o', str(binary)], check=True, timeout=120)
            subprocess.run([str(binary)], check=True, timeout=120, env=env)
    for name in ('timecard_clock_failure_source_test.py', 'timecard_store_failure_source_test.py'):
        subprocess.run(['python3', str(ROOT/'test/native_apps'/name)], check=True, timeout=30)
    print('Timecard original source, strict validation and portable real-renderer tests passed normally and with selected sanitizers')
if __name__ == '__main__':
    main()
