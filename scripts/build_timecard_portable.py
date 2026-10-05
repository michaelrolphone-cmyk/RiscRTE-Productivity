#!/usr/bin/env python3
"""Build a Timecard UI/model development ELF. No writable backend or install artifact.

The original Reader application and its released identity stay unchanged. This
profile deliberately has no deployable manifest while app-data support is absent.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
ROOT = Path(__file__).resolve().parents[1]
SYSTEM_PIN = 'a708a45ef47a4d625c1da9fd334bbbd817aa3e15'
MODEL_SHA256 = 'c17b0fed28eb44d2397c740ef56c8f07bc51c3f707d3080ec9ad5849e25a31a9'
MODEL_VERSION = '1.0.4'
PROFILE_VERSION = '0.1.0'
EXPORTS = {'app_main', 'app_module_init', 'app_module_fini'}
IMPORTS = {'risc_runtime_get_api', 'memcpy', 'memset', 'memcmp', 'strcmp', 'strlen', 'snprintf', 'malloc', 'free', 'strcpy'}

def git(repo, *args):
    return subprocess.check_output(['git', '-C', str(repo), *args], text=True).strip()

def verify(system):
    sdk = json.loads((ROOT/'lib/PortableTimecard/SOURCES.json').read_text())
    if hashlib.sha256((ROOT/'lib/PortableTimecard/include/RiscAppDataV1.h').read_bytes()).hexdigest() != sdk['sha256']:
        raise ValueError('Timecard app-data declaration differs from its recorded source')
    if git(system, 'rev-parse', 'HEAD') != SYSTEM_PIN or git(system, 'status', '--porcelain', '--untracked-files=all'):
        raise ValueError('A clean exact System Apps dependency is required: '+SYSTEM_PIN)
    if hashlib.sha256((ROOT/'Apps/timecard.c').read_bytes()).hexdigest() != MODEL_SHA256:
        raise ValueError('Authoritative Timecard source changed; review/rebaseline the portable profile explicitly')
    if json.loads((ROOT/'Apps/timecard.json').read_text())['version'] != MODEL_VERSION:
        raise ValueError('Authoritative Timecard version changed')

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--system-apps', required=True, type=Path)
    p.add_argument('--denver', action='store_true')
    p.add_argument('--alarm-client', action='store_true', help='Link the existing shared foreground alarm/retention client')
    p.add_argument('--navigation', action='store_true', help='Use the existing generic input.navigation capability')
    p.add_argument('--app-data-client', action='store_true', help='Target-build the versioned client against its recorded consumer SDK; no backend or deployment')
    p.add_argument('--runtime-appdata', type=Path, help='Compile the explicit app-data prototype profile against this local SDK; still not installable')
    args = p.parse_args()
    system = args.system_apps.resolve()
    verify(system)
    cc = os.environ.get('NATIVE_APP_CC') or shutil.which('xtensa-esp32s3-elf-gcc')
    if not cc:
        cc = str(Path(os.environ.get('PLATFORMIO_CORE_DIR', Path.home()/'.platformio'))/'packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc')
    runtime = args.runtime_appdata.resolve() if args.runtime_appdata else None
    app_data_client = bool(runtime or args.app_data_client)
    out = ROOT/('dist/timecard-appdata-development' if app_data_client else 'dist/timecard-ui-development')
    out.mkdir(parents=True, exist_ok=True)
    catalog = out/'catalog.c'
    catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
    mapping = out/'exports.map'
    mapping.write_text('{ global: '+ '; '.join(sorted(EXPORTS))+'; local: *; };\n')
    elf = out/('timecard-appdata-development.elf' if app_data_client else 'timecard-ui-development.elf')
    flags = ['-DPORTABLE_FORCE_FULL_FRAMES', '-DPORTABLE_NOVA_UI', '-DPORTABLE_APP_OWNS_TOUCH_CHROME']
    if args.denver:
        flags.append('-DPORTABLE_RTC_UTC8_DENVER')
    if args.alarm_client:
        flags.append('-DPORTABLE_ALARM_CLIENT')
    if args.navigation:
        flags.append('-DPORTABLE_INPUT_NAVIGATION')
    includes = [system/'lib/NativeApps/include', ROOT/'lib/NativeApps/include', system/'lib/PortableApps/include']
    if app_data_client:
        if runtime and (runtime/'sdk/app/RiscAppDataV1.h').read_bytes() != (ROOT/'lib/PortableTimecard/include/RiscAppDataV1.h').read_bytes():
            raise ValueError('Selected Runtime app-data header differs from the tested consumer declaration')
        flags.append('-DTIMECARD_APP_DATA')
        # Compile the byte-identical consumer declaration without shadowing the
        # shared adapter's deliberately pinned Runtime ABI prefix headers.
        includes.insert(0, ROOT/'lib/PortableTimecard/include')
    sources = [ROOT/'Apps/timecard_portable.c', system/'lib/PortableApps/src/adapter.c', catalog]
    subprocess.run([cc, '-std=c11', '-Os', '-fPIC', '-mtext-section-literals', '-mlongcalls',
                    '-fvisibility=hidden', '-ffreestanding', '-fno-builtin', '-nostdlib', '-nostartfiles',
                    '-shared', '-Wl,--no-relax', '-Wl,--hash-style=sysv', '-Wl,--version-script='+str(mapping),
                    '-Wall', '-Wextra', '-Werror', *flags, *['-I'+str(x) for x in includes],
                    *map(str, sources), '-lgcc', '-o', str(elf)], check=True, timeout=120)
    symbols = subprocess.check_output([cc.removesuffix('gcc')+'nm', '-D', str(elf)], text=True)
    imports = {line.split()[-1] for line in symbols.splitlines() if ' U ' in ' '+line}
    exports = {line.split()[-1] for line in symbols.splitlines() if len(line.split()) >= 3 and line.split()[-2] in ('T', 'D', 'B', 'R')}
    if not imports <= IMPORTS or exports != EXPORTS:
        raise ValueError(f'Unapproved ELF imports/exports: {imports-IMPORTS}, {exports}')
    data = elf.read_bytes()
    if data[:7] != b'\x7fELF\x01\x01\x01' or data[16:20] != b'\x03\x00\x5e\x00':
        raise ValueError('Expected Xtensa ELF32 ET_DYN')
    validator = out/'validate-elf'
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-I'+str(ROOT/'test/native_apps/stubs'), '-I'+str(ROOT/'lib/elf_loader/include'),
                    str(ROOT/'lib/elf_loader/src/esp_elf_validate.c'), str(ROOT/'test/native_apps/validate_test.c'),
                    '-o', str(validator)], check=True, timeout=60)
    subprocess.run([str(validator), str(elf)], check=True, timeout=60)
    files = [ROOT/'Apps/timecard.c', ROOT/'Apps/timecard.json', ROOT/'Apps/timecard_portable.c',
             ROOT/'Apps/timecard_portable_validation.h', ROOT/'lib/NativeApps/include/T5SystemApi.h',
             ROOT/'Apps/timecard_appdata_bridge.h', ROOT/'lib/PortableTimecard/SOURCES.json', ROOT/'lib/PortableTimecard/include/RiscAppDataV1.h', Path(__file__)]
    dependencies = [f for folder in ('lib/PortableApps', 'lib/NativeApps/include') for f in (system/folder).rglob('*') if f.is_file()]
    record = {'schema': 1, 'purpose': 'app-data-client-development-not-watch-deployment' if app_data_client else 'ui-model-development-only-no-writable-backend',
              'deployable': False, 'model_version': MODEL_VERSION, 'profile_version': PROFILE_VERSION,
              'repository_sha': git(ROOT, 'rev-parse', 'HEAD'), 'working_tree_dirty': bool(git(ROOT, 'status', '--porcelain')),
              'system_apps_sha': SYSTEM_PIN, 'compiler': subprocess.check_output([cc, '--version'], text=True).splitlines()[0],
              'defines': flags, 'imports': sorted(imports), 'exports': sorted(exports),
              'size_bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest(),
              'storage': 'prototype storage.app-data@1 namespace 1; explicit new-layout Runtime required' if app_data_client else 'unbound; runtime entry displays unavailable and creates no history',
              'time_policy': 'rtc-utc8-to-America-Denver' if args.denver else 'identity-raw',
              'source_sha256': {str(f.relative_to(ROOT)): hashlib.sha256(f.read_bytes()).hexdigest() for f in files},
              'dependency_sha256': {str(f.relative_to(system)): hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(dependencies)}}
    if app_data_client:
        record['app_data_sdk'] = json.loads((ROOT/'lib/PortableTimecard/SOURCES.json').read_text())
    if runtime:
        record['runtime_api'] = {'repository_sha': git(runtime, 'rev-parse', 'HEAD'), 'working_tree_dirty': bool(git(runtime, 'status', '--porcelain')),
                                 'header_sha256': hashlib.sha256((runtime/'sdk/app/RiscAppDataV1.h').read_bytes()).hexdigest()}
        record['source_sha256']['Apps/timecard_appdata_bridge.h'] = hashlib.sha256((ROOT/'Apps/timecard_appdata_bridge.h').read_bytes()).hexdigest()
    (out/'build-evidence.json').write_text(json.dumps(record, indent=2)+'\n')
    (out/'NOT_INSTALLABLE.txt').write_text('Development evidence only. No deployable manifest or approved Watch package. App-data builds require the separately reviewed opt-in Runtime/layout and provisioned filesystem. Do not add this ELF to a launcher or package.\n')
    licenses = out/'licenses'; licenses.mkdir(exist_ok=True)
    shutil.copyfile(ROOT/'LICENSE', licenses/'Productivity-LICENSE.txt')
    shutil.copyfile(system/'LICENSE', licenses/'System-Apps-LICENSE.txt')
    for directory in ('fonts', 'settings_fonts', 'time'):
        for f in (system/'lib/PortableApps'/directory).rglob('*'):
            if f.is_file() and (f.name.startswith('LICENSE') or f.name == 'SOURCES.json'):
                target=licenses/directory/f.relative_to(system/'lib/PortableApps'/directory)
                target.parent.mkdir(parents=True, exist_ok=True);shutil.copyfile(f,target)
    print('Timecard UI/model development ELF: exact model/pin, target validator and ABI checks passed; not installable')
if __name__ == '__main__':
    main()
