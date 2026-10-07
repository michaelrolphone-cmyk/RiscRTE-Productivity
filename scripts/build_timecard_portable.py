#!/usr/bin/env python3
"""Build the shared Timecard profile or an explicit app-data integration package.

The original Reader application and its released identity stay unchanged.
Packaging requires the exact published Runtime dependency; it does not install.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
ROOT = Path(__file__).resolve().parents[1]
SYSTEM_PIN = 'dbd358746bc8f1254619ca42073e1d44f6ad45df'
MODEL_SHA256 = 'c17b0fed28eb44d2397c740ef56c8f07bc51c3f707d3080ec9ad5849e25a31a9'
MODEL_VERSION = '1.0.4'
PROFILE_VERSION = '0.1.1'
EXPORTS = {'app_main', 'app_module_init', 'app_module_fini'}
IMPORTS = {'risc_runtime_get_api', 'memcpy', 'memset', 'memcmp', 'strcmp', 'strlen', 'snprintf', 'malloc', 'free', 'strcpy'}
REQUIRES = [('display.output', 1), ('input.touch.raw', 1), ('input.navigation', 1),
            ('rtc.clock', 2), ('storage.key-value', 1), ('alarm.service', 1), ('storage.app-data', 1)]

def package_manifest(runtime):
    sdk = json.loads((ROOT/'lib/PortableTimecard/SOURCES.json').read_text())
    if not runtime or sdk['publication_state'] != 'published':
        raise ValueError('Packaging requires the published app-data Runtime dependency')
    if git(runtime, 'rev-parse', 'HEAD') != sdk['source_commit'] or git(runtime, 'status', '--porcelain', '--untracked-files=all'):
        raise ValueError('Packaging requires a clean exact Runtime dependency')
    if (runtime/'sdk/app/RiscAppDataV1.h').read_bytes() != (ROOT/'lib/PortableTimecard/include/RiscAppDataV1.h').read_bytes():
        raise ValueError('Runtime app-data declaration differs from the recorded consumer bytes')
    expected = {'type': 'application', 'id': 'timecard', 'version': PROFILE_VERSION,
            'architecture': 'xtensa-esp32s3', 'file_name': 'timecard.elf', 'entry': 'app_main',
            'requires': [{'capability': name, 'api': api} for name, api in REQUIRES]}
    manifest = json.loads((ROOT/'Apps/native/timecard.json').read_text())
    if manifest != expected:
        raise ValueError('Portable manifest identity or declared authority differs')
    return manifest

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
    p.add_argument('--runtime-appdata', type=Path, help='Verify this Runtime SDK; package mode requires the exact published source pin')
    p.add_argument('--package-profile', action='store_true', help='Emit timecard.elf and its capability manifest for integration; requires the exact published Runtime and enables alarm/navigation clients')
    args = p.parse_args()
    system = args.system_apps.resolve()
    verify(system)
    cc = os.environ.get('NATIVE_APP_CC') or shutil.which('xtensa-esp32s3-elf-gcc')
    if not cc:
        cc = str(Path(os.environ.get('PLATFORMIO_CORE_DIR', Path.home()/'.platformio'))/'packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc')
    runtime = args.runtime_appdata.resolve() if args.runtime_appdata else None
    manifest = package_manifest(runtime) if args.package_profile else None
    if manifest:
        args.alarm_client = args.navigation = True
    app_data_client = bool(runtime or args.app_data_client)
    out = ROOT/('dist/timecard-portable' if manifest else 'dist/timecard-appdata-development' if app_data_client else 'dist/timecard-ui-development')
    out.mkdir(parents=True, exist_ok=True)
    catalog = out/'catalog.c'
    catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
    mapping = out/'exports.map'
    mapping.write_text('{ global: '+ '; '.join(sorted(EXPORTS))+'; local: *; };\n')
    elf = out/('timecard.elf' if manifest else 'timecard-appdata-development.elf' if app_data_client else 'timecard-ui-development.elf')
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
             ROOT/'Apps/timecard_appdata_bridge.h', ROOT/'Apps/native/timecard.json', ROOT/'lib/PortableTimecard/SOURCES.json', ROOT/'lib/PortableTimecard/LICENSE', ROOT/'lib/PortableTimecard/include/RiscAppDataV1.h', Path(__file__)]
    dependencies = [f for folder in ('lib/PortableApps', 'lib/NativeApps/include') for f in (system/folder).rglob('*') if f.is_file()]
    record = {'schema': 1, 'purpose': 'app-data-integration-package' if manifest else 'app-data-client-development-not-watch-deployment' if app_data_client else 'ui-model-development-only-no-writable-backend',
              'deployable': bool(manifest), 'model_version': MODEL_VERSION, 'profile_version': PROFILE_VERSION,
              'repository_sha': git(ROOT, 'rev-parse', 'HEAD'), 'working_tree_dirty': bool(git(ROOT, 'status', '--porcelain')),
              'system_apps_sha': SYSTEM_PIN, 'compiler': subprocess.check_output([cc, '--version'], text=True).splitlines()[0],
              'defines': flags, 'imports': sorted(imports), 'exports': sorted(exports),
              'size_bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest(),
              'storage': 'storage.app-data@1 namespace 1; a provisioned writable backend is required' if app_data_client else 'unbound; runtime entry displays unavailable and creates no history',
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
    if manifest:
        elf.with_suffix('.json').write_text(json.dumps(manifest, indent=2)+'\n')
        (out/'INTEGRATION.txt').write_text('Requires storage.app-data@1 with namespace 1, shared time-format KV namespace 1, and every declared provider. The board packager owns provider instance IDs and layout compatibility. A Watch app-data image requires the explicit ABI 2 layout and provisioned LittleFS volume. This package does not install or migrate storage.\n')
    else:
        (out/'NOT_INSTALLABLE.txt').write_text('Development evidence only. No deployable manifest or approved Watch package. Use the explicit package profile with the exact published Runtime dependency for integration.\n')
    licenses = out/'licenses'; licenses.mkdir(exist_ok=True)
    shutil.copyfile(ROOT/'LICENSE', licenses/'Productivity-LICENSE.txt')
    shutil.copyfile(system/'LICENSE', licenses/'System-Apps-LICENSE.txt')
    shutil.copyfile(ROOT/'lib/PortableTimecard/LICENSE', licenses/'Runtime-SDK-LICENSE.txt')
    for directory in ('fonts', 'settings_fonts', 'time'):
        for f in (system/'lib/PortableApps'/directory).rglob('*'):
            if f.is_file() and (f.name.startswith('LICENSE') or f.name == 'SOURCES.json'):
                target=licenses/directory/f.relative_to(system/'lib/PortableApps'/directory)
                target.parent.mkdir(parents=True, exist_ok=True);shutil.copyfile(f,target)
    print('Timecard: exact model/pins, target validator and ABI checks passed; '+('integration package prepared' if manifest else 'development evidence only'))
if __name__ == '__main__':
    main()
