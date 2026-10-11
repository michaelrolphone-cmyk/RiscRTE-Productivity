#!/usr/bin/env python3
"""Build the Daily Paper ELF and its small HTTPS capability provider."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent

def sdk(runtime, system, destination):
    spec = importlib.util.spec_from_file_location('scene_sdk', system/'scripts/scene_sdk.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    include = module.stage_sdk(runtime, system, destination)
    for name in ('WifiApi.h', 'PortableWifiSavedNetwork.h', 'PortableWifiCredentials.h'):
        shutil.copyfile(system/'lib/PortableApps/include'/name, include/name)
    return include

def run(*command):
    subprocess.run(list(map(str, command)), check=True)

def target(compiler, include, system, output, manifest, sources, exports):
    output.mkdir(parents=True)
    flags = ['-Os', '-fPIC', '-mtext-section-literals', '-mlongcalls',
             '-fvisibility=hidden', '-ffreestanding', '-fno-builtin',
             '-Wall', '-Wextra', '-Werror', '-Wframe-larger-than=1536', '-fstack-usage',
             '-I'+str(include), '-I'+str(system/'Services/update')]
    objects = []
    for source in sources:
        obj = output/(source.name+'.o'); objects.append(obj)
        cpp = source.suffix == '.cpp'
        cc = compiler.removesuffix('gcc')+'g++' if cpp else compiler
        language = ['-std=c++11', '-fno-exceptions', '-fno-rtti', '-fno-threadsafe-statics'] if cpp else ['-std=c11']
        run(cc, *flags, *language, '-c', source, '-o', obj)
    mapping = output/'exports.map'
    mapping.write_text('{ global: '+'; '.join(sorted(exports))+'; local: *; };\n')
    elf = output/manifest['file_name']
    run(compiler, '-nostdlib', '-nostartfiles', '-shared', '-Wl,--no-relax', '-Wl,--hash-style=sysv',
        '-Wl,--version-script='+str(mapping), *objects, '-o', elf)
    content = elf.read_bytes()
    if content[:7] != b'\x7fELF\x01\x01\x01' or content[16:20] != b'\x03\x00\x5e\x00':
        raise ValueError('Not an Xtensa ELF32 ET_DYN module')
    symbols = subprocess.check_output([compiler.removesuffix('gcc')+'nm', '-D', str(elf)], text=True)
    imports = {line.split()[-1] for line in symbols.splitlines() if ' U ' in ' '+line}
    allowed = {'risc_runtime_get_api', 'memcpy', 'memset', 'memcmp', 'strcmp', 'strncmp', 'strlen', 'snprintf'}
    found = {line.split()[-1] for line in symbols.splitlines() if len(line.split()) >= 3 and line.split()[-2] in ('T','D','B','R')}
    if imports-allowed or found != exports:
        raise ValueError(f'Unexpected imports {imports-allowed} or exports {found}')
    name = 'daily_digest.json' if manifest['type'] == 'application' else 'manifest.json'
    (output/name).write_text(json.dumps(manifest, indent=2)+'\n')
    (output/'build.json').write_text(json.dumps({'id': manifest['id'], 'version': manifest['version'],
        'bytes': len(content), 'imports': sorted(imports), 'exports': sorted(found), 'hardware_tested': False}, indent=2)+'\n')
    mapping.unlink()
    for obj in objects: obj.unlink()
    print(f'{manifest["id"]}: {len(content)} bytes; imports/exports and stack-frame budget passed')
    return elf

def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('runtime','system','output'): p.add_argument('--'+name, type=Path, required=True)
    p.add_argument('--cc', default=os.environ.get('NATIVE_APP_CC') or str(Path.home()/'.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc'))
    a = p.parse_args()
    a.runtime = a.runtime.resolve(); a.system = a.system.resolve(); a.output = a.output.resolve()
    if a.output.exists(): raise FileExistsError('Use a new output directory')
    a.output.mkdir(parents=True)
    with tempfile.TemporaryDirectory(prefix='daily-paper-build-') as temporary:
        include = sdk(a.runtime, a.system, Path(temporary))
        app = json.loads((ROOT.parent/'Apps/daily_digest.json').read_text())
        provider = json.loads((ROOT/'provider/manifest.json').read_text())
        elfs = [target(a.cc, include, a.system, a.output/'daily-digest', app,
                       [ROOT/'app.c', ROOT/'metadata.cpp'], {'app_main','risc_resident_app_descriptor_v1'}),
                target(a.cc, include, a.system, a.output/'net-http-client', provider,
                       [ROOT/'provider/driver.c'], {'t5_driver_get'})]
        validator = Path(temporary)/'validate'
        run(os.environ.get('CC','cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
            '-I'+str(a.system/'test/native_apps/stubs'), '-I'+str(a.system/'lib/elf_loader/include'),
            a.system/'lib/elf_loader/src/esp_elf_validate.c', a.system/'test/native_apps/validate_test.c', '-o', validator)
        for elf in elfs: run(validator, elf)
    print('Both target ELFs passed the existing structural loader validator.')

if __name__ == '__main__':
    main()
