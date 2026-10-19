#!/usr/bin/env python3
"""Build the explicit, source-frozen native Points development target only."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
PROFILE = ROOT / 'sdk/points-native-ui-sources.json'
MANIFEST = ROOT / 'Apps/native/points_native_ui.json'
IMPORTS = {'risc_runtime_get_api', 'memcpy', 'memset', 'memcmp', 'strcmp',
           'strlen', 'snprintf', 'malloc', 'free', 'strcpy'}
EXPORTS = {'app_main', 'app_module_init', 'app_module_fini'}
PRIVATE = {'portable_points_native_' + name for name in
           ('available', 'begin', 'layout', 'present', 'ready', 'repaint', 'back', 'contact')}
REQUIRES = [('display.output', 1), ('input.touch.raw', 1), ('input.navigation', 1),
            ('runtime.realtime', 1), ('storage.key-value', 1), ('alarm.service', 2)]
DEFINES = ['PORTABLE_POINTS_NATIVE_UI', 'PORTABLE_NOVA_UI', 'PORTABLE_NATIVE_CANVAS',
           'PORTABLE_APP_OWNS_TOUCH_CHROME', 'PORTABLE_RETAINED_RGB565_HANDOFF',
           'PORTABLE_FORCE_FULL_FRAMES', 'ALARM_NATIVE_UTC', 'ALARM_SERVICE_TAGGED_V2',
           'PORTABLE_ALARM_CLIENT', 'PORTABLE_ALARM_NATIVE', 'PORTABLE_NATIVE_TIME_TOOLBAR',
           'PORTABLE_NATIVE_CUSTODY_FENCE', 'PORTABLE_INPUT_NAVIGATION',
           'PORTABLE_QUICK_ACTIONS', 'PORTABLE_QUICK_NATIVE',
           'POINTS_RETURN_APP="springboard.elf"', 'PORTABLE_HOME_APP="default.elf"']
SOURCES = ('adapter.c', 'PortableRealtimeClient.c', 'PortableTimeZone.c',
           'PortableTimeZoneCatalog.c', 'PortableTimeZonePreference.c',
           'quick_actions.c', 'quick_render.c', 'quick_session.c')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def capture(command):
    return subprocess.check_output(list(map(str, command)), text=True, timeout=120)


def git(path, *args):
    return capture(['git', '-C', path, *args]).strip()


def run(command):
    subprocess.run(list(map(str, command)), check=True, timeout=120)


def inventory(root, folders, files):
    paths = set(files)
    for folder in folders:
        paths.update(str(p.relative_to(root)) for p in (root / folder).rglob('*') if p.is_file())
    return {name: sha(root / name) for name in sorted(paths)}


def check_hashes(root, expected):
    if not expected:
        raise ValueError('Missing frozen source hashes: ' + str(root))
    for name, digest in expected.items():
        path = root / name
        if Path(name).is_absolute() or '..' in Path(name).parts or not re.fullmatch('[0-9a-f]{64}', digest):
            raise ValueError('Invalid frozen source entry: ' + name)
        if not path.is_file() or sha(path) != digest:
            raise ValueError('Frozen source changed: ' + str(path))


def validate(profile, manifest, roots):
    if profile.get('schema') != 1 or profile.get('id') != 'points-native-ui-development':
        raise ValueError('Unsupported native Points profile')
    if profile.get('defines') != DEFINES or profile.get('private_owner') != 12:
        raise ValueError('Native Points selector, ownership or define closure changed')
    if profile.get('compiler') != 'xtensa-esp32s3-elf-gcc (crosstool-NG esp-2021r2-patch5) 8.4.0':
        raise ValueError('Native Points requires the pinned GCC8.4 toolchain')
    if profile.get('hardware_verified') is not False or profile.get('product_recipe') is not False:
        raise ValueError('Development profile cannot claim hardware or product qualification')
    expected_manifest = {'type': 'application', 'id': 'points_in_time',
        'version': profile['version'], 'architecture': 'xtensa-esp32s3',
        'file_name': 'points_in_time.elf', 'entry': 'app_main',
        'requires': [{'capability': name, 'api': api} for name, api in REQUIRES]}
    if manifest != expected_manifest:
        raise ValueError('Native Points manifest capability/identity closure changed')
    for name in ('system', 'runtime', 'utilities'):
        root, pin = roots[name], profile['dependencies'][name]
        if git(root, 'rev-parse', 'HEAD') != pin['base_revision']:
            raise ValueError('Exact dependency base required: ' + name)
        if name != 'system' and git(root, 'status', '--porcelain', '--untracked-files=all'):
            raise ValueError('Clean exact dependency required: ' + name)
        check_hashes(root, pin['source_sha256'])
        if inventory(root, pin['folders'], pin['files']) != pin['source_sha256']:
            raise ValueError('Frozen source inventory changed: ' + name)
    check_hashes(roots['productivity'], profile['application_source_sha256'])


def compiler_path(profile):
    compiler = os.environ.get('NATIVE_APP_CC') or shutil.which('xtensa-esp32s3-elf-gcc')
    if not compiler:
        raise ValueError('Set NATIVE_APP_CC to the pinned GCC8.4 compiler')
    version = capture([compiler, '--version']).splitlines()[0]
    if version != profile['compiler']:
        raise ValueError('Pinned GCC8.4 compiler required: ' + version)
    macros = capture([compiler, '-dM', '-E', '-xc', '/dev/null']).splitlines()
    if '#define __XTENSA_EL__ 1' not in macros or '#define __XTENSA__ 1' not in macros:
        raise ValueError('Little-endian Xtensa compiler required')
    return compiler, version


def stage_headers(output, roots):
    include = output / 'sdk/include'
    include.mkdir(parents=True)
    headers = {p.name: p for p in (roots['system'] / 'lib/PortableApps/include').glob('*.h')}
    for name in ('RiscRuntimeV1.h', 'RiscRealtimeV1.h'):
        headers[name] = roots['runtime'] / 'sdk/app' / name
    for name in ('AlarmServiceV1.h', 'AlarmServiceV2.h'):
        headers[name] = roots['utilities'] / 'lib/Alarm/include' / name
    for name, source in headers.items():
        (include / name).symlink_to(source)
    (output / 'sdk/time').symlink_to(roots['system'] / 'lib/PortableApps/time', target_is_directory=True)
    return [include, roots['utilities'] / 'lib/Alarm/include', roots['runtime'] / 'sdk/app',
            roots['system'] / 'lib/NativeApps/include', roots['system'] / 'Apps']


def dependency_closure(compiler, flags, sources, roots, profile, catalog):
    dependency_text = capture([compiler, *flags, '-MM', '-MT', 'input', *sources])
    consumed = {}
    for line in dependency_text.replace('\\\n', ' ').splitlines():
        if not line:
            continue
        for token in shlex.split(line.split(':', 1)[1]):
            path = Path(token).resolve()
            if path == catalog:
                continue
            for name, root in roots.items():
                if path.is_relative_to(root):
                    relative = str(path.relative_to(root))
                    expected = (profile['application_source_sha256'] if name == 'productivity'
                                else profile['dependencies'][name]['source_sha256'])
                    if relative not in expected or sha(path) != expected[relative]:
                        raise ValueError('Unfrozen compiler dependency: ' + str(path))
                    consumed[name + '/' + relative] = expected[relative]
                    break
            else:
                raise ValueError('Compiler dependency outside frozen inputs: ' + str(path))
    if not consumed:
        raise ValueError('Compiler reported no source dependencies')
    return dict(sorted(consumed.items()))


def check_symbols(dynamic, all_symbols):
    imports = {line.split()[-1] for line in dynamic.splitlines()
               if len(line.split()) >= 2 and line.split()[-2] in ('U', 'w', 'v')}
    exports = {line.split()[-1] for line in dynamic.splitlines()
               if len(line.split()) >= 3 and line.split()[-2] != 'U'}
    if not imports <= IMPORTS or exports != EXPORTS:
        raise ValueError(('Unexpected dynamic ABI', imports - IMPORTS, exports))
    private = {}
    for name in sorted(PRIVATE):
        rows = [line.split() for line in all_symbols.splitlines() if line.split()[-1:] == [name]]
        if len(rows) != 1 or rows[0][4] != 'LOCAL' or rows[0][6] == 'UND':
            raise ValueError('Points private hook missing or externally visible: ' + name)
        private[name] = {'binding': rows[0][4], 'visibility': rows[0][5]}
    return imports, exports, private


def build(args):
    roots = {'productivity': ROOT, 'system': args.system_apps.resolve(),
             'utilities': args.utilities.resolve(), 'runtime': args.runtime.resolve()}
    profile = json.loads(PROFILE.read_text())
    manifest = json.loads(MANIFEST.read_text())
    validate(profile, manifest, roots)
    compiler, version = compiler_path(profile)
    if args.verify_only:
        print('PASS: frozen native Points sources, exact dependency bases, manifest and GCC8.4 compiler')
        return
    output = args.output_dir.resolve()
    # Never remove or overwrite a prior artifact or any dependency source tree.
    output.mkdir(parents=True, exist_ok=False)
    includes = stage_headers(output, roots)
    catalog = output / 'catalog.c'
    catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
    mapping = output / 'exports.map'
    mapping.write_text('{ global: ' + '; '.join(sorted(EXPORTS)) + '; local: *; };\n')
    sources = [ROOT / 'Apps/points_in_time.c', *[roots['system'] / 'lib/PortableApps/src' / name for name in SOURCES], catalog]
    defines = ['-D' + name for name in DEFINES]
    if args.snapshot:
        defines.append('-DPORTABLE_RASTER_SNAPSHOT')
    flags = ['-std=c11', '-Os', '-fPIC', '-mtext-section-literals', '-mlongcalls',
             '-fvisibility=hidden', '-ffreestanding', '-fno-builtin', '-Wall', '-Wextra', '-Werror',
             *defines, *['-I' + str(path) for path in includes]]
    consumed = dependency_closure(compiler, flags, sources, roots, profile, catalog)
    elf = output / manifest['file_name']
    command = [compiler, *flags, '-nostdlib', '-nostartfiles', '-shared', '-Wl,--no-relax',
               '-Wl,--hash-style=sysv', '-Wl,--version-script=' + str(mapping), *sources, '-lgcc', '-o', elf]
    (output / 'compile-command.json').write_text(json.dumps(list(map(str, command)), indent=2) + '\n')
    run(command)
    dynamic = capture([compiler.removesuffix('gcc') + 'nm', '-D', elf])
    symbols = capture([compiler.removesuffix('gcc') + 'readelf', '--wide', '--symbols', elf])
    imports, exports, private = check_symbols(dynamic, symbols)
    (output / 'dynamic-symbols.txt').write_text(dynamic)
    (output / 'symbols.txt').write_text(symbols)
    data = elf.read_bytes()
    if data[:7] != b'\x7fELF\x01\x01\x01' or data[16:20] != b'\x03\x00\x5e\x00':
        raise ValueError('Expected ELF32 little-endian Xtensa ET_DYN')
    validator = output / 'validate-elf'
    run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
         '-I' + str(ROOT / 'test/native_apps/stubs'), '-I' + str(ROOT / 'lib/elf_loader/include'),
         ROOT / 'lib/elf_loader/src/esp_elf_validate.c', ROOT / 'test/native_apps/validate_test.c',
         '-o', validator])
    validation = capture([validator, elf])
    (output / 'elf-validation.txt').write_text(validation)
    # Refuse evidence if any source changed while the compiler was reading it.
    validate(profile, manifest, roots)
    elf.with_suffix('.json').write_text(json.dumps(manifest, indent=2) + '\n')
    evidence = {'schema': 1, 'purpose': 'native-points-ui-development-not-install-catalog',
        'profile': profile, 'snapshot': args.snapshot, 'hardware_verified': False,
        'product_recipe': False, 'compiler': version, 'compile_command': list(map(str, command)),
        'elf_sha256': sha(elf), 'elf_bytes': len(data), 'imports': sorted(imports),
        'exports': sorted(exports), 'private_hooks': private, 'consumed_source_sha256': consumed, 'loader_validation': validation.strip(),
        'dependencies': {name: {'path': str(path), 'revision': git(path, 'rev-parse', 'HEAD'),
            'working_tree_status': git(path, 'status', '--porcelain', '--untracked-files=all')}
            for name, path in roots.items()},
        'build_source_sha256': {name: sha(ROOT / name) for name in
            ('scripts/build_points_native_ui.py', 'sdk/points-native-ui-sources.json',
             'Apps/native/points_native_ui.json', 'lib/elf_loader/src/esp_elf_validate.c',
             'lib/elf_loader/include/private/elf_types.h', 'test/native_apps/validate_test.c')}}
    for name, root in roots.items():
        for path in [root / 'LICENSE', *sorted((root / 'lib/PortableApps').rglob('*'))]:
            if path.is_file() and (path.name.startswith('LICENSE') or 'OFL' in path.name or path.name.endswith('SOURCES.json') or path.name == 'TIMEZONE_PROVENANCE.json'):
                destination = output / 'licenses' / name / path.relative_to(root)
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(path, destination)
    (output / 'build-evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print('PASS: native Points GCC8.4 target, loader validation and private ABI; '
          + str(len(data)) + ' bytes; snapshot=' + str(args.snapshot))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('system-apps', 'utilities', 'runtime', 'output-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--snapshot', action='store_true', help='Select the immutable raster snapshot path')
    parser.add_argument('--verify-only', action='store_true', help='Validate frozen source/profile/compiler inputs without writing')
    build(parser.parse_args())


if __name__ == '__main__':
    main()
