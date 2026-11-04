#!/usr/bin/env python3
"""Qualify the exact source closure of a canonical Points build on the host.

Host fixtures exercise public service contracts, not physical hardware.
Requires a successful tools/build_points_scene.py output; rejects source drift.
Use --sanitizers for ASan/UBSan (leak detection disabled for retained custody).
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--sanitizers', action='store_true')
    args = parser.parse_args()
    build = args.build.resolve()
    receipt = json.loads((build/'build.json').read_text())
    for filename, digest in receipt['dependency_sha256'].items():
        if sha(Path(filename)) != digest:
            raise ValueError('Built source changed; rebuild before qualification: '+filename)
    if sha(build/'points_in_time.elf') != receipt['sha256']:
        raise ValueError('Built ELF changed')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    system = build/'sources/System'
    utilities = build/'sources/Utilities'
    common = ['-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
              '-Wno-unused-function', '-Wno-misleading-indentation', '-Wno-missing-field-initializers',
              '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections',
              '-I'+str(build/'include'), '-I'+str(ROOT/'Apps'), '-I'+str(utilities/'Apps')]
    if args.sanitizers:
        common += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
    cases = {
        'background': [utilities/'test/files_background/controller_test.c'],
        'civil_background': [utilities/'test/files_background/civil_clock_test.c'],
        'model': [ROOT/'test/native_apps/points_scene_model_test.c'],
        'services': [ROOT/'test/native_apps/points_scene_services_test.c', system/'lib/PortableApps/src/PortableRealtimeClient.c'],
        'storage': [ROOT/'test/native_apps/points_scene_storage_test.c'],
        'controller': [ROOT/'test/native_apps/points_catalog_controller_test.c'],
        'root': [ROOT/'test/points_scene/root_test.c', ROOT/'Apps/points_scene_schedule.c', system/'Apps/files_scene_background.c',
                 *[system/'lib/PortableApps/src'/name for name in ('PortableRealtimeClient.c', 'PortableTimeZone.c', 'PortableTimeZoneCatalog.c', 'PortableTimeZonePreference.c')]],
        'schedule': [ROOT/'test/native_apps/points_scene_schedule_test.c', ROOT/'Apps/points_scene_schedule.c',
                     *[system/'lib/PortableApps/src'/name for name in ('PortableTimeZone.c', 'PortableTimeZoneCatalog.c', 'PortableTimeZonePreference.c')]],
    }
    commands, results, dependencies = [], {}, {}
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0')
    def run(command):
        command = list(map(str, command))
        result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=env)
        commands.append({'command': command, 'returncode': result.returncode, 'output': result.stdout})
        (output/'commands.json').write_text(json.dumps(commands, indent=2)+'\n')
        print(result.stdout, end='')
        result.check_returncode()
        return result.stdout
    for name, sources in cases.items():
        exe = output/name
        objects = []
        for index, source in enumerate(sources):
            obj = output/(name+'-'+str(index)+'.o')
            run([os.environ.get('CC','cc'), *common, '-MMD', '-MF', str(obj)+'.d', '-c', source, '-o', obj])
            objects.append(obj)
        run([os.environ.get('CC','cc'), *common, *objects, '-o', exe])
        if name == 'root':
            root_cases = ['api1', 'api2', 'text-suffix-invalid', 'no-scene', 'loop-reentry', 'publish-revoked', 'publish-state', 'save-refresh', 'close-again', 'host-controls', 'host-alarm', 'text-accept', 'text-cancel', 'text-host-pause', 'text-held', 'custody-revoked', 'publish-busy-candidate', 'type-save-no-clock', 'workflow-invalid-events', 'text-workflow-serialized', 'text-open-busy', 'background-settings', 'catalog-save-native', 'save-revoked', 'format-clean', 'format-io', 'format-missing', 'format-revoked']
            results[name] = {case: run([exe, case]) for case in root_cases}
        else:
            results[name] = run([exe])
    for dep in output.glob('*.o.d'):
        for filename in dep.read_text().replace('\\\n', ' ').split()[1:]:
            path = Path(filename)
            if path.is_file():
                dependencies[str(path)] = sha(path)
    for filename, digest in receipt['dependency_sha256'].items():
        if sha(Path(filename)) != digest:
            raise ValueError('Source changed during qualification: '+filename)
    (output/'qualification.json').write_text(json.dumps({
        'schema': 1, 'status': 'passed', 'sanitizers': args.sanitizers,
        'asan_options': env['ASAN_OPTIONS'], 'build_receipt_sha256': sha(build/'build.json'),
        'canonical_elf_sha256': receipt['sha256'], 'runner_sha256': sha(Path(__file__)),
        'dependency_sha256': dependencies, 'results': results,
        'physical_testing': 'not performed'}, indent=2, sort_keys=True)+'\n')
    print('Points host qualification passed; source closure matches canonical ELF build.')


if __name__ == '__main__':
    main()
