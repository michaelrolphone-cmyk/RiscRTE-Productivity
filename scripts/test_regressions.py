#!/usr/bin/env python3
"""Exercise the real synchronized functions, including rejected transitions."""
import os
import subprocess
from check_baseline import ROOT, check_sdk
check_sdk()
out = ROOT / 'build/regressions'
out.mkdir(parents=True, exist_ok=True)
for name, fixture in [('text_editor_discard', 'tests/text_editor_discard_test.c'),
                      ('timecard_overlap', 'tests/timecard_overlap_test.c'),
                      ('text_editor_open', 'test/native_apps/text_editor_open_test.c'),
                      ('timecard_clock_failure', 'test/native_apps/timecard_clock_failure_test.c'),
                      ('timecard_store_failure', 'test/native_apps/timecard_store_failure_test.c')]:
    binary = out / name
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=undefined', '-fno-sanitize-recover=all',
                    '-I' + str(ROOT / 'lib/NativeApps/include'), '-I' + str(ROOT / 'sdk/driver'),
                    str(ROOT / fixture), '-o', str(binary)], check=True, timeout=60)
    subprocess.run([str(binary)], check=True, timeout=30)

subprocess.run([os.environ.get("PYTHON", "python3"), str(ROOT / "test/native_apps/timecard_clock_failure_source_test.py")], check=True, timeout=30)
subprocess.run([os.environ.get("PYTHON", "python3"), str(ROOT / "test/native_apps/timecard_store_failure_source_test.py")], check=True, timeout=30)
