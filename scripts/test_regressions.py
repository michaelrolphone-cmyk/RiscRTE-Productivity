#!/usr/bin/env python3
"""Exercise the real synchronized functions, including rejected transitions."""
import os
import subprocess
from check_baseline import ROOT, check_sdk
check_sdk()
out = ROOT / 'build/regressions'
out.mkdir(parents=True, exist_ok=True)
for name in ['text_editor_discard', 'timecard_overlap']:
    binary = out / name
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=undefined', '-fno-sanitize-recover=all',
                    '-I' + str(ROOT / 'lib/NativeApps/include'), '-I' + str(ROOT / 'sdk/driver'),
                    str(ROOT / f'tests/{name}_test.c'), '-o', str(binary)], check=True, timeout=60)
    subprocess.run([str(binary)], check=True, timeout=30)
