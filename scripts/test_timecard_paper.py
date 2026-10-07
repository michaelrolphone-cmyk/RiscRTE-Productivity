#!/usr/bin/env python3
"""Exercise Timecard with the pinned real adapter, paper pixels and QuickActions."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
from build_timecard_portable import verify
ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--system-apps', required=True, type=Path)
    args = parser.parse_args()
    system = args.system_apps.resolve()
    verify(system)
    out = ROOT/'build/timecard-paper';out.mkdir(parents=True, exist_ok=True)
    catalog = out/'catalog.c'
    catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
    for sanitized in (False, True):
        for landscape in (False, True):
            flags = ['-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
                     '-DTIMECARD_PAPER','-DPORTABLE_NOVA_UI','-DPORTABLE_APP_OWNS_TOUCH_CHROME',
                     '-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_RTC_WALL_TIME',
                     '-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_HOME_APP="default.elf"']
            if sys.platform == 'darwin':
                flags += ['-Wno-misleading-indentation','-Wno-unused-function']
            if landscape:flags += ['-DTEST_NATIVE_LANDSCAPE','-DPORTABLE_DISPLAY_ROTATION=90']
            if sanitized:flags += ['-fsanitize='+os.environ.get('TIMECARD_SANITIZERS','address,undefined'),'-fno-sanitize-recover=all','-fno-omit-frame-pointer']
            includes = [system/'Apps',system/'lib/NativeApps/include', ROOT/'lib/NativeApps/include',system/'lib/PortableApps/include']
            sources = [ROOT/'test/native_apps/timecard_paper_test.c',catalog]
            sources += [system/'lib/PortableApps/src'/name for name in ('adapter.c','quick_actions.c','quick_render.c','quick_session.c')]
            binary = out/f'paper-{int(landscape)}-{int(sanitized)}'
            subprocess.run([os.environ.get('CC','cc'),*flags,*['-I'+str(p) for p in includes],*map(str,sources),'-o',str(binary)],check=True)
            for case in range(18):
                frames = out/f'frames-{int(landscape)}-{int(sanitized)}-{case}';frames.mkdir(exist_ok=True)
                subprocess.run([str(binary),str(case),str(frames)],check=True,timeout=30,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
    print('72 normal/sanitized portrait/native-rotation Timecard paper scenarios passed')
if __name__ == '__main__':main()
