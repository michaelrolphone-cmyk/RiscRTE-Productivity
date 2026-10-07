#!/usr/bin/env python3
"""Build the distinct Points app with exact shared adapter/records dependencies.

Development ELF evidence only; does not release, deploy, merge, or modify a device.
The legacy Reader migration inventory, SDK pins and parity checks are unchanged.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
from app_manifest import validate_manifest
ROOT = Path(__file__).resolve().parents[1]
SYSTEM_PIN = '969d2210e1bf517c1299801c8ad205c956e06810'
UTILITIES_PIN = '23a4887f1eeb7b7ce867c0e243158b899e43f0f8'
IMPORTS = {'risc_runtime_get_api', 'memcpy', 'memset', 'memcmp', 'strcmp', 'strlen', 'snprintf', 'malloc', 'free', 'strcpy'}
EXPORTS = {'app_main', 'app_module_init', 'app_module_fini'}
REQUIRES = [('display.output', 1), ('input.touch.raw', 1), ('rtc.clock', 2), ('storage.key-value', 1), ('alarm.service', 1)]

def git(path, *args):
    return subprocess.check_output(['git', '-C', str(path), *args], text=True).strip()

def verify_dependency(path, pin):
    if not re.fullmatch('[0-9a-f]{40}', pin):
        raise ValueError('Dependency has no finalized exact commit pin')
    if git(path, 'rev-parse', 'HEAD') != pin or git(path, 'status', '--porcelain', '--untracked-files=all'):
        raise ValueError(f'Clean exact dependency checkout required: {pin}')

def inventory():
    apps = json.loads((ROOT/'productivity-manifest.json').read_text())['portable_apps']
    if len(apps) != 1:
        raise ValueError('Expected exactly one original portable productivity app')
    app = apps[0]
    if any(app.get(k) != v for k, v in {'id': 'points_in_time', 'version': '0.5.1', 'origin': 'original',
            'runtime_profile': 'portable-riscrte-v1', 'source_path': 'Apps/points_in_time.c',
            'manifest_path': 'Apps/points_in_time.json', 'file_name': 'points_in_time.elf'}.items()):
        raise ValueError('Invalid Points identity/provenance')
    if app.get('additional_sources') != ['Apps/points_writer.h', 'Apps/points_nova7.inc', 'Apps/points_nova7_picker.inc', 'Apps/points_watch_keyboard.h', 'Apps/points_paper.inc']:
        raise ValueError('Unexpected Points support source')
    validate_manifest(ROOT/app['source_path'], app['file_name'])
    manifest = json.loads((ROOT/app['manifest_path']).read_text())
    requires = [(row['capability'], int(row['api'][2:])) for row in manifest['requires']]
    if manifest['version'] != app['version'] or requires != REQUIRES or manifest.get('optional'):
        raise ValueError('Points manifest version or authority mismatch')
    return app, manifest

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--system-apps', required=True, type=Path)
    parser.add_argument('--utilities', required=True, type=Path)
    parser.add_argument('--denver', action='store_true', help='RTC fixed UTC+08, schedule/display America/Denver; match service build')
    parser.add_argument('--display-rotation',type=int,choices=[0,90],default=0)
    parser.add_argument('--navigation',action='store_true')
    parser.add_argument('--partial-damage',action='store_true',help='Use reported partial-damage support; Watch default retains full frames')
    parser.add_argument('--return-app',help='Explicit app-owned root Back destination')
    parser.add_argument('--output-dir',type=Path,default=ROOT/'dist/points-in-time')
    args = parser.parse_args()
    system, utilities = args.system_apps.resolve(), args.utilities.resolve()
    verify_dependency(system, SYSTEM_PIN)
    verify_dependency(utilities, UTILITIES_PIN)
    app, source_manifest = inventory()
    cc = os.environ.get('NATIVE_APP_CC') or shutil.which('xtensa-esp32s3-elf-gcc')
    if not cc:
        cc = str(Path(os.environ.get('PLATFORMIO_CORE_DIR', Path.home()/'.platformio'))/'packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc')
    output = args.output_dir.resolve()
    if output.exists():
        shutil.rmtree(output)
    output.mkdir(parents=True)
    catalog = output/'catalog.c'
    catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
    mapping = output/'exports.map'
    mapping.write_text('{ global: '+ '; '.join(sorted(EXPORTS))+'; local: *; };\n')
    elf = output/app['file_name']
    defines = ['-DPORTABLE_ALARM_CLIENT','-DPORTABLE_NOVA_UI','-DPORTABLE_DISPLAY_ROTATION='+str(args.display_rotation)]
    if not args.partial_damage:defines.append('-DPORTABLE_FORCE_FULL_FRAMES')
    if args.navigation:defines.append('-DPORTABLE_INPUT_NAVIGATION')
    if args.return_app:defines.append('-DPOINTS_RETURN_APP="'+args.return_app+'"')
    if args.denver:
        defines.append('-DPORTABLE_RTC_UTC8_DENVER')
    includes = [utilities/'lib/Alarm/include', system/'lib/PortableApps/include', system/'lib/NativeApps/include', system/'Apps']
    sources = [ROOT/app['source_path'], system/'lib/PortableApps/src/adapter.c', catalog]
    subprocess.run([cc, '-std=c11', '-Os', '-fPIC', '-mtext-section-literals', '-mlongcalls', '-fvisibility=hidden',
                    '-ffreestanding', '-fno-builtin', '-nostdlib', '-nostartfiles', '-shared', '-Wl,--no-relax',
                    '-Wl,--hash-style=sysv', '-Wl,--version-script='+str(mapping), '-Wall', '-Wextra', '-Werror',
                    *defines, *['-I'+str(path) for path in includes], *map(str, sources), '-lgcc', '-o', str(elf)], check=True, timeout=120)
    symbols = subprocess.check_output([cc.removesuffix('gcc')+'nm', '-D', str(elf)], text=True)
    imports = {line.split()[-1] for line in symbols.splitlines() if ' U ' in ' '+line}
    exports = {line.split()[-1] for line in symbols.splitlines() if len(line.split()) >= 3 and line.split()[-2] in ('T', 'D', 'B', 'R')}
    if not imports <= IMPORTS or exports != EXPORTS:
        raise ValueError(f'Unapproved imports or exports: {imports - IMPORTS}, {exports}')
    data = elf.read_bytes()
    if data[:7] != b'\x7fELF\x01\x01\x01' or data[16:20] != b'\x03\x00\x5e\x00':
        raise ValueError('Expected ELF32 little-endian Xtensa ET_DYN')
    validator = ROOT/'build/points-in-time/validate-elf'
    validator.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-I'+str(ROOT/'test/native_apps/stubs'), '-I'+str(ROOT/'lib/elf_loader/include'),
                    str(ROOT/'lib/elf_loader/src/esp_elf_validate.c'), str(ROOT/'test/native_apps/validate_test.c'),
                    '-o', str(validator)], check=True, timeout=60)
    subprocess.run([str(validator), str(elf)], check=True, timeout=60)
    sidecar = {'type': 'application', 'id': app['id'], 'version': source_manifest['version'],
               'architecture': 'xtensa-esp32s3', 'file_name': elf.name, 'entry': 'app_main',
               'requires': [{'capability': name, 'api': version} for name, version in REQUIRES]}
    if args.navigation:sidecar['requires'].append({'capability':'input.navigation','api':1})
    elf.with_suffix('.json').write_text(json.dumps(sidecar, indent=2)+'\n')
    files = [ROOT/'Apps/points_in_time.c', ROOT/'Apps/points_writer.h', ROOT/'Apps/points_nova7.inc', ROOT/'Apps/points_nova7_picker.inc', ROOT/'Apps/points_watch_keyboard.h', ROOT/'Apps/points_paper.inc', ROOT/'Apps/points_in_time.json',
             ROOT/'scripts/build_points_in_time.py', ROOT/'productivity-manifest.json']
    dependencies = [p for p in (system/'lib/PortableApps').rglob('*') if p.is_file()]
    dependencies += list((system/'lib/NativeApps/include').glob('*.h'))
    dependencies += [system/'Apps/SpringboardPresentation.h',system/'Apps/PaperPresentation.h']
    dependencies += [utilities/'lib/Alarm/include'/name for name in ('AlarmServiceV1.h', 'AlarmRecords.h', 'PointsRecords.h', 'PointsSchedule.h')]
    evidence = {'schema': 1, 'purpose': 'points-development-artifact-not-install-catalog', 'version': source_manifest['version'],
                'repository_sha': git(ROOT, 'rev-parse', 'HEAD'), 'working_tree_dirty': bool(git(ROOT, 'status', '--porcelain')),
                'system_apps_sha': SYSTEM_PIN, 'utilities_sha': UTILITIES_PIN,
                'compiler': subprocess.check_output([cc, '--version'], text=True).splitlines()[0],
                'time_policy': 'rtc-utc8-to-America-Denver' if args.denver else 'identity-raw', 'build_defines': defines,
                'sha256': hashlib.sha256(data).hexdigest(), 'size_bytes': len(data), 'imports': sorted(imports), 'exports': sorted(exports),
                'storage_grants': [{'instance': 5, 'access': 'read-write', 'key': 'points_cfg'},
                                   {'instance': 5, 'access': 'read-write', 'key': 'points_meta'},
                                   {'instance': 1, 'access': 'read-only-client', 'key': 'time_format'}],
                'source_sha256': {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in files},
                'dependency_sha256': {('system/' if p.is_relative_to(system) else 'utilities/')+str(p.relative_to(system if p.is_relative_to(system) else utilities)): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(dependencies)}}
    (output/'build-evidence.json').write_text(json.dumps(evidence, indent=2)+'\n')
    for repo, name in [(system, 'System-Apps'), (utilities, 'Utilities'), (ROOT, 'Productivity')]:
        dest = output/'licenses'/name
        dest.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(repo/'LICENSE', dest/'LICENSE')
    for directory in ('fonts', 'settings_fonts', 'paper_fonts', 'time'):
        for path in (system/'lib/PortableApps'/directory).rglob('*'):
            if path.is_file() and (path.name.startswith('LICENSE') or path.name == 'SOURCES.json'):
                dest = output/'licenses'/'System-Apps'/directory/path.relative_to(system/'lib/PortableApps'/directory)
                dest.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(path, dest)
    print('Points in Time 0.5.0: exact pins, target ELF validator and import/export checks passed')
if __name__ == '__main__':
    main()
