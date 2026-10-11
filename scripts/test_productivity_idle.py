#!/usr/bin/env python3
"""Test the exact built Points/Timecard idle profiles against the real adapter.

Pass the combined cohort's recipes.json. Production defines, include order and
linked sources come from its build evidence; only the app entry fixture and
native sleep result replace production translation units. No firmware is built
or changed. Results and reproducible compiler commands are saved under build/.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

import build_points_native_utc as points
import build_timecard_native_time as timecard
import native_paper_transitions as motion

CASES = ('editor-resumed', 'editor-refused', 'list-resumed', 'list-refused',
         'retained', 'pause-retained', 'active-contact', 'pending-frame',
         'radios-off', 'wake-contact')


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--recipes', type=Path, required=True)
    parser.add_argument('--case', choices=CASES, action='append')
    parser.add_argument('--app', choices=('points_in_time', 'timecard'), action='append')
    args = parser.parse_args()
    recipes = json.loads(args.recipes.read_text())
    output = points.ROOT / 'build/productivity-idle'
    output.mkdir(parents=True, exist_ok=True)
    results, commands, production_receipts = [], [], {}
    for name in args.app or ('points_in_time', 'timecard'):
        builder = points if name == 'points_in_time' else timecard
        recipe = recipes[name]
        if recipe['returncode'] != 0:
            raise ValueError('A successful combined production build is required')
        profile = argparse.ArgumentParser()
        for option in ('system-apps', 'adapter-system', 'utilities', 'runtime', 'output-dir'):
            profile.add_argument('--' + option, type=Path, required=True)
        motion.options(profile)
        selected = profile.parse_args(recipe['command'][2:])
        if not all((selected.paper_transitions, selected.ble_broadcast,
                    selected.touch_scrolling, selected.x4_idle_source)):
            raise ValueError('Combined idle/telemetry/scroll profile required')
        builder.verify(selected.system_apps, selected.adapter_system, selected.utilities, selected.runtime)
        adapter, _, _, _ = motion.select(selected, profile, selected.adapter_system, builder.DEFINES)
        evidence = json.loads((selected.output_dir / 'build-evidence.json').read_text())
        receipt = json.loads((selected.output_dir / 'x4-native-app.json').read_text())
        production_receipts[name] = {
            'app_revision': receipt['source_revision'],
            'system_revision': receipt['system_source_revision'],
            'build_evidence_sha256': digest(selected.output_dir / 'build-evidence.json'),
            'app_receipt_sha256': digest(selected.output_dir / 'x4-native-app.json'),
        }
        if evidence['defines'] != receipt['build_defines']:
            raise ValueError('Production define receipts disagree')
        for source, expected in evidence['source_sha256'].items():
            if digest(points.ROOT / source) != expected:
                raise ValueError('Production app source changed: ' + source)
        for source, expected in evidence['adapter_sha256'].items():
            if digest(adapter / source) != expected:
                raise ValueError('Production adapter source changed: ' + source)
        idle = receipt['idle_policy']
        if digest(selected.x4_idle_source) != idle['helper_sha256']:
            raise ValueError('Selected helper differs from production receipt')
        sdk = Path(idle['compiled_include_directory'])
        for header, expected in receipt['sdk_sha256'].items():
            if digest(sdk / header) != expected:
                raise ValueError('Final typed SDK differs: ' + header)
        production = evidence['compile_command']
        includes = list(dict.fromkeys(item for item in production if item.startswith('-I')))
        if includes[0] != '-I' + str(sdk):
            raise ValueError('Final typed SDK must precede all other headers')
        sources = [Path(item) for item in production if item.endswith('.c')]
        controller = 'points_in_time.c' if name == 'points_in_time' else 'timecard_portable.c'
        replacements = {controller, 'catalog.c', selected.x4_idle_source.name}
        if {path.name for path in sources} & replacements != replacements:
            raise ValueError('Unexpected production translation units')
        sources = [points.ROOT / 'test/native_apps/productivity_idle_test.c',
                   *(path for path in sources if path.name not in replacements)]
        defines = [item for item in receipt['build_defines'] if item.startswith('-D')]
        defines += ['-DTEST_PRODUCTIVITY_SCROLL_FIXTURE', '-DTEST_PRODUCTIVITY_IDLE_FIXTURE']
        if name == 'points_in_time':
            defines.append('-DTEST_POINTS_IDLE')
        for sanitized in (False, True):
            executable = output / f'{name}-{int(sanitized)}'
            flags = (['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                      '-fno-omit-frame-pointer', '-no-pie'] if sanitized else [])
            command = [os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g',
                       '-Wall', '-Wextra', '-Werror', *defines, *flags, *includes,
                       *map(str, sources), '-o', str(executable)]
            subprocess.run(command, check=True)
            commands.append(command)
            for flipped in (False, True):
                environment = {**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'}
                environment.pop('TEST_PAPER_FLIP', None)
                if flipped:
                    environment['TEST_PAPER_FLIP'] = '1'
                for case in args.case or CASES:
                    run = subprocess.run([str(executable), case], check=True,
                                         stdout=subprocess.PIPE, text=True, timeout=30,
                                         env=environment)
                    print(name, 'sanitized' if sanitized else 'normal',
                          'flipped' if flipped else 'portrait', run.stdout.strip(), flush=True)
                    results.append(dict(app=name, sanitized=sanitized, flipped=flipped, case=case))
    test_sources = ['scripts/test_productivity_idle.py',
                    'test/native_apps/productivity_idle_test.c',
                    'test/native_apps/productivity_idle_fixture.h',
                    'test/native_apps/productivity_scroll_fixture.h',
                    'test/native_apps/native_broadcast_fixture.h',
                    'test/native_apps/points_native_adapter_test.c',
                    'test/native_apps/timecard_native_time_test.c']
    (output / 'receipt.json').write_text(json.dumps({
        'runs': results, 'compile_commands': commands,
        'production_recipes': str(args.recipes.resolve()),
        'production_receipts': production_receipts,
        'source_sha256': {p: digest(points.ROOT / p) for p in test_sources},
        'sleep_backend': 'result stub; real helper and Runtime tested separately',
        'hardware_verified': False,
    }, indent=2) + '\n')
    print(f'{len(results)} production-profile automatic idle/resume checks passed')


if __name__ == '__main__':
    main()
