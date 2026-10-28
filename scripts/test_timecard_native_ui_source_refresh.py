#!/usr/bin/env python3
"""Prove legacy native translations retain the frozen pre-current-profile tokens."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
from build_timecard_native_ui import APPLICATION_FILES, PROFILE, ROOT, selected_defines, stage_headers


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('system-apps', 'runtime', 'utilities', 'output'):
        p.add_argument('--' + name, type=Path, required=True)
    a = p.parse_args()
    output = a.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    profile = json.loads(PROFILE.read_text())
    refresh = profile['source_refresh']
    baseline = refresh['baseline_commit']
    assert baseline == 'bb8d0d99b3a857103d2b01e6b4c756b913aa14d3'
    roots = {'productivity': ROOT, 'system': a.system_apps.resolve(),
             'runtime': a.runtime.resolve(), 'utilities': a.utilities.resolve()}
    includes = stage_headers(output, roots)
    staged = output / 'prior'
    prior = refresh['previous_sha256']
    assert set(prior) == {'Apps/timecard_native_time.h', 'Apps/timecard_native_ui.inc',
                          'Apps/timecard_portable.c'}
    for name in APPLICATION_FILES:
        current = (ROOT / name).read_bytes()
        assert hashlib.sha256(current).hexdigest() == profile['application_source_sha256'][name], name
        data = subprocess.check_output(['git', '-C', str(ROOT), 'show', baseline + ':' + name]) if name in prior else current
        if name in prior:
            assert hashlib.sha256(data).hexdigest() == prior[name], name
        destination = staged / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(data)
    rows = []
    for utc in (False, True):
        for snapshot in (False, True):
            defines = selected_defines(utc, snapshot)
            assert 'TIMECARD_CURRENT_PROFILE' not in defines
            values = []
            for root in (staged, ROOT):
                command = [os.environ.get('CC', 'cc'), '-E', '-P',
                           *['-D' + value for value in defines],
                           *['-I' + str(path) for path in [*includes, root / 'lib/NativeApps/include']],
                           str(root / 'Apps/timecard_portable.c')]
                text = subprocess.check_output(command, text=True)
                tokens = '\n'.join(re.findall(r'''"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|[A-Za-z_0-9]+|[^\s]''', text))
                values.append(tokens)
            assert values[0] == values[1], ('legacy translation changed', utc, snapshot)
            rows.append({'utc': utc, 'snapshot': snapshot, 'defines': defines,
                         'tokens_sha256': hashlib.sha256(values[0].encode()).hexdigest()})
    (output / 'receipt.json').write_text(json.dumps({'passed': True, 'baseline': baseline,
        'compositions': rows, 'scope': 'Host preprocessor token parity; target bytes and runtime behavior require their retained matrices.',
        'hardware_tested': False}, indent=2) + '\n')
    print('PASS four complete legacy native RTC/UTC direct/snapshot translation token comparisons')


if __name__ == '__main__':
    main()
