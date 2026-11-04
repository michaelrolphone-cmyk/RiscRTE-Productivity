#!/usr/bin/env python3
"""Build one canonical Points scene ELF from immutable dependency commits.

No device/rendering switches; dependency working-tree changes are never read.
Requires Python 3.12+, pyelftools 0.32, host C/C++ compilers and an installed
ESP32-S3 compiler. NATIVE_APP_CC or --target-cc selects the compiler.
Qualified with Espressif GCC 15.2.0 esp-15.2.0_20251204; the script records
its actual version. Historical native Points GCC 8 profiles are not reused.
"""
import argparse
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile

ROOT = Path(__file__).resolve().parents[1]
LOCK = ROOT / 'sdk/points-scene-sources.json'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + '\n')


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def stage_repository(repository, pin, destination):
    """Use tracked commit bytes even if the caller supplies a dirty checkout."""
    actual = subprocess.check_output(['git', '-C', str(repository), 'rev-parse', pin+'^{commit}'], text=True).strip()
    if actual != pin:
        raise ValueError('Dependency pin is not a full immutable commit')
    data = subprocess.check_output(['git', '-C', str(repository), 'archive', '--format=tar', pin])
    destination.mkdir(parents=True)
    with tarfile.open(fileobj=io.BytesIO(data)) as archive:
        archive.extractall(destination, filter='data')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('runtime', 'system', 'utilities', 'output'):
        parser.add_argument('--'+name, required=True, type=Path)
    parser.add_argument('--target-cc')
    args = parser.parse_args()
    out = args.output.resolve()
    if out.exists():
        raise FileExistsError('Use a new output directory')
    out.mkdir(parents=True)
    lock = json.loads(LOCK.read_text())
    if lock.get('defines') != []:
        raise ValueError('Canonical scene app cannot carry compiler variants')
    import elftools
    if elftools.__version__ != lock['pyelftools_version']:
        raise ValueError('Use pinned pyelftools '+lock['pyelftools_version'])
    repos = {}
    for name, row in lock['dependencies'].items():
        repos[name] = out/'sources'/name
        stage_repository(getattr(args, name.lower()).resolve(), row['commit'], repos[name])
    system, runtime, utilities = (repos[k] for k in ('System', 'Runtime', 'Utilities'))
    sys.path.insert(0, str(system/'scripts'))
    toolchain = load('points_native_toolchain', system/'scripts/native_toolchain.py')
    compiler = toolchain.compiler_path(args.target_cc)
    compiler_version = subprocess.check_output([compiler, '--version'], text=True).splitlines()[0]
    if compiler_version != lock['compiler_version']:
        raise ValueError('Target compiler differs from qualification pin: '+compiler_version)
    # The order is explicit: public System ABI headers supersede historical
    # convenience copies. All compiled files see this single canonical copy.
    include = out/'include'
    include.mkdir()
    origins = {}
    folders = [*(utilities/'lib'/name/'include' for name in ('Alarm', 'Bluetooth', 'Contexts', 'ContextFingerprint')),
               utilities/'Apps', system/'lib/NativeApps/include', system/'lib/PortableApps/include',
               *(runtime/'sdk'/name for name in ('app', 'driver', 'hardware')), system/'sdk/app']
    for folder in folders:
        for source in sorted(folder.glob('*.h')):
            shutil.copyfile(source, include/source.name)
            origins[source.name] = {'source': str(source.relative_to(out)), 'sha256': sha(source)}
    # Alarm contracts belong to Utilities, not bundled compatibility copies.
    for source in sorted((utilities/'lib/Alarm/include').glob('*.h')):
        shutil.copyfile(source, include/source.name)
        origins[source.name] = {'source': str(source.relative_to(out)), 'sha256': sha(source)}
    for name in ('files_scene_ui.h', 'files_scene_resident.h', 'files_scene_effects.h', 'files_scene_background.h'):
        source = system/'Apps'/name
        shutil.copyfile(source, include/name)
        origins[name] = {'source': str(source.relative_to(out)), 'sha256': sha(source)}
    shutil.copytree(system/'lib/PortableApps/time', out/'time')
    write(out/'header-origins.json', origins)
    sources = [*[ROOT/path for path in lock['application_sources']], *[system/path for path in lock['system_sources']]]
    commands = []
    def run(command):
        commands.append(list(map(str, command)))
        write(out/'commands.json', commands)
        subprocess.run(commands[-1], check=True)
    flags = ['-std=c11', '-Os', '-fPIC', '-mtext-section-literals', '-mlongcalls',
             '-ffunction-sections', '-fdata-sections', '-fvisibility=hidden',
             '-ffreestanding', '-fno-builtin', '-Wall', '-Wextra', '-Werror',
             '-Wno-unused-function', '-Wno-misleading-indentation', '-fstack-usage', '-I'+str(include)]
    objects = []
    for source in sources:
        obj = out/(source.stem+'.o')
        command = [compiler, *flags, '-MMD', '-MF', str(obj)+'.d', '-c', source, '-o', obj]
        run(command)
        original_object = obj.read_bytes()
        run(command)
        if obj.read_bytes() != original_object:
            raise ValueError('Repeated compilation differed: '+str(source))
        objects.append(obj)
    libgcc = Path(subprocess.check_output([compiler, '-print-libgcc-file-name'], text=True).strip())
    for name in lock['compiler_support_members']:
        obj = out/name
        obj.write_bytes(subprocess.check_output([compiler.removesuffix('gcc')+'ar', 'p', str(libgcc), name]))
        objects.append(obj)
    exports = {'app_main', 'risc_resident_app_descriptor_v1'}
    mapping = out/'exports.map'
    mapping.write_text('{ global: '+ '; '.join(sorted(exports))+'; local: *; };\n')
    elf = out/'points_in_time.elf'
    link = [compiler, '-shared', '-nostdlib', '-nostartfiles', '-Wl,--no-relax', '-Wl,--gc-sections',
            '-Wl,--hash-style=sysv', '-Wl,--version-script='+str(mapping), *objects, '-o', elf]
    run(link)
    first = elf.read_bytes()
    run(link)
    if elf.read_bytes() != first:
        raise ValueError('Repeated link differed')
    compact = load('points_compact', system/'scripts/compact_current_elf.py').compact
    compaction = compact(elf, compiler, out/'points_in_time.full.elf')
    repeated = out/'points_in_time.repeat.elf'
    repeated.write_bytes(first)
    compact(repeated, compiler, out/'points_in_time.repeat.full.elf')
    if repeated.read_bytes() != elf.read_bytes():
        raise ValueError('Repeated compaction differed')
    symbols = subprocess.check_output([compiler.removesuffix('gcc')+'nm', '-D', str(elf)], text=True)
    imports = {line.split()[-1] for line in symbols.splitlines() if ' U ' in ' '+line}
    actual = {line.split()[-1] for line in symbols.splitlines() if len(line.split()) >= 3 and line.split()[-2] in ('T','D','B','R')}
    allowed = set(lock['allowed_imports'])
    if imports-allowed or actual != exports:
        raise ValueError('Invalid application ABI: '+repr((imports-allowed, actual)))
    if elf.read_bytes()[:7] != b'\x7fELF\x01\x01\x01' or elf.read_bytes()[16:20] != b'\x03\x00\x5e\x00':
        raise ValueError('Not an Xtensa ELF32 ET_DYN module')
    validator = out/'validate-elf'
    run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
         '-I'+str(system/'test/native_apps/stubs'), '-I'+str(runtime/'lib/elf_loader/include'),
         runtime/'lib/elf_loader/src/esp_elf_validate.c', system/'test/native_apps/validate_test.c', '-o', validator])
    run([validator, elf])
    manifest = ROOT/'Apps/points_scene.json'
    shutil.copyfile(manifest, out/'points_in_time.json')
    product_manifests = {}
    for product, version in lock['product_alarm_api'].items():
        selection = out/'selections'/product
        selection.mkdir(parents=True)
        selected = json.loads(manifest.read_text())
        next(row for row in selected['optional'] if row['capability']=='alarm.service')['api'] = version
        write(selection/'points_in_time.json', selected)
        shutil.copyfile(elf, selection/elf.name)
        if sha(selection/elf.name) != sha(elf):
            raise ValueError('Product selection changed canonical ELF')
        product_manifests[product] = {'alarm_api': version, 'manifest_sha256': sha(selection/'points_in_time.json'), 'elf_sha256': sha(selection/elf.name)}
    manifest_validator = out/'validate-manifest'
    run([os.environ.get('CXX', 'c++'), '-std=c++17', '-Wall', '-Wextra', '-Werror',
         '-I'+str(runtime/'src'), '-I'+str(runtime/'sdk/app'), '-I'+str(runtime/'sdk/driver'),
         '-I'+str(runtime/'lib/ArduinoJson/src'), system/'test/files_scene/manifest_test.cpp', '-o', manifest_validator])
    for product in product_manifests:
        run([manifest_validator, out/'selections'/product/'points_in_time.json'])
    closure = {}
    for depfile in out.glob('*.o.d'):
        for value in depfile.read_text().replace('\\\n', ' ').split()[1:]:
            path = Path(value)
            if path.is_file():
                closure[str(path)] = sha(path)
    write(out/'build.json', {'schema': 1, 'sha256': sha(elf), 'size_bytes': elf.stat().st_size,
          'repeat_identical': True, 'repeated_compilation_identical': True, 'defines': [], 'exports': sorted(actual), 'imports': sorted(imports),
          'source_lock_sha256': sha(LOCK), 'builder_sha256': sha(Path(__file__)),
          'source_pins': lock['dependencies'], 'dependency_sha256': closure,
          'manifest_sha256': sha(manifest), 'product_selections': product_manifests, 'compiler': subprocess.check_output([compiler, '--version'], text=True).splitlines()[0],
          'compiler_support_archive_sha256': sha(libgcc), 'compaction': compaction,
          'stack_usage': {p.name: p.read_text() for p in out.glob('*.su')},
          'physical_testing': 'not performed', 'deployment': 'not performed'})
    print('Canonical Points ELF validated; repeated link and compaction are byte-identical.')


if __name__ == '__main__':
    main()
