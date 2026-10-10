#!/usr/bin/env python3
"""Qualify the exact resident target profile and actual entrypoint lifecycle."""
import argparse
import hashlib
import json
import os
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CASES = [
    'draft', 'uncertain', 'home', 'failure', 'preservation',
    *['home-entry-'+str(i) for i in (0,1,2,3,4,5,6,7,9)],
    'name-home','name-home-pending','name-home-retention',
    'acquire-loss', 'initial-draw-retention', 'replace-retention',
    'projection-retention', 'read-retention', 'resolve-read-retention',
    'release-retention', 'home-uncertain', 'old-service', 'service-busy',
    'controls', 'editor-navigation', 'text-input', 'text-retention',
    'text-abandoned', 'text-alarm', 'text-frame',
    *['text-runtime-' + str(i) for i in range(1, 7)],
    'types-scroll', 'geometry', 'reference',
    *['text-entrypoint-' + case for case in
      ('accept', 'cancel', 'pending', 'unavailable', 'home', 'alarm')],
]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build', type=Path, required=True)
    p.add_argument('--utilities', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--system', type=Path)
    p.add_argument('--sdk', type=Path)
    p.add_argument('--case', action='append', choices=CASES)
    a = p.parse_args()
    out = a.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    frames = out / 'frames'
    frames.mkdir(exist_ok=True)
    receipt_path = a.build / 'points_in_time/x4-native-app.json'
    receipt = json.loads(receipt_path.read_text())
    cmd = receipt['compile_command']
    # Recovered historical receipts retain original paths. Remap paths only;
    # defines and selected translation units remain exact to the installed app.
    replacements = {'/workspace/shared/x4-shared-keyboard-051/points-source': str(ROOT)}
    if a.system: replacements['/workspace/shared/system-shared-text-terminal-051'] = str(a.system.resolve())
    if a.sdk: replacements['/workspace/shared/x4-shared-keyboard-051/points-target/sdk/include'] = str(a.sdk.resolve())
    for old,new in replacements.items():cmd=[x.replace(old,new) for x in cmd]
    flags = [s for s in cmd if s.startswith(('-D', '-I'))]
    if '-DPORTABLE_APP_HOME_GUARD' not in flags:flags.append('-DPORTABLE_APP_HOME_GUARD')
    sources = [s for s in cmd if s.endswith('.c') and Path(s).name not in
               ('catalog.c', 'points_catalog_app.c')]
    results = []
    for sanitize in (False, True):
        san = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
               '-fno-omit-frame-pointer', '-fno-pie', '-no-pie'] if sanitize else []
        binary = out / ('resident-points-sanitized' if sanitize else 'resident-points-normal')
        command = ['cc', '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                   *san, *flags, '-DTEST_RESIDENT_CLIENT',
                   '-I' + str(a.utilities / 'test/native_apps'),
                   str(ROOT / 'test/native_apps/resident_points_test.c'), *sources,
                   '-o', str(binary)]
        subprocess.run(command, check=True)
        for case in a.case or CASES:
            subprocess.run([str(binary), case], check=True, timeout=60, env={
                **os.environ, 'ASAN_OPTIONS': 'detect_leaks=0',
                'POINTS_CATALOG_FRAMES': str(frames)})
            results.append(dict(case=case, sanitized=sanitize, passed=True))
        (out / ('compile-sanitized.json' if sanitize else 'compile-normal.json')).write_text(
            json.dumps(command, indent=2) + '\n')
    cases = a.case or CASES
    result = dict(target_version=receipt['version'], target_elf_sha256=receipt['elf_sha256'],
                  target_receipt_sha256=hashlib.sha256(receipt_path.read_bytes()).hexdigest(),
                  source_revision=subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip(),
                  test_source_sha256={name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest()
                                      for name in ('scripts/test_x4_shared_text_points.py',
                                                   'test/native_apps/resident_points_test.c',
                                                   'test/native_apps/points_catalog_app_test.c')},
                  runs=results, actual_entrypoint_cases=sum(case.startswith('text-entrypoint-') for case in cases),
                  repetitions_per_entrypoint_case=2,
                  exact_target_defines=receipt['build_defines'],
                  asan=True, ubsan=True, leak_sanitizer=False,
                  limits=['Capability providers and resident host dispatch are deterministic fixtures.',
                          'Alarm entrypoint scenario ends the fake alert after text close; full alarm UI is not exercised there.',
                          'LeakSanitizer disabled for executor ptrace compatibility.',
                          'Target Xtensa instructions and physical hardware were not executed.'],
                  hardware_verified=False)
    (out / 'test-evidence.json').write_text(json.dumps(result, indent=2) + '\n')
    print(f'{len(results)} exact-profile resident cases PASS')


if __name__ == '__main__':
    main()
