#!/usr/bin/env python3
"""Reproduce the pinned CrossPoint-derived, capability-only Reader ELF."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from reader_sources import ROOT, stage


def build(a):
    a.output.mkdir(parents=True, exist_ok=True)
    staged = stage(a.crosspoint, a.output / 'upstream')
    lock = json.loads((ROOT / 'reader/upstream.json').read_text())
    for name in ('png', 'jpeg'):
        checkout = getattr(a, name)
        actual = subprocess.check_output(['git', '-C', str(checkout), 'rev-parse', 'HEAD'], text=True).strip()
        if actual != lock['dependencies'][name]['commit']:
            raise ValueError(name + ' does not match the source lock')
        if subprocess.run(['git', '-C', str(checkout), 'diff-index', '--quiet', 'HEAD', '--']).returncode:
            raise ValueError(name + ' tracked source is modified')
    objects = a.output / 'objects'
    subprocess.run([sys.executable, str(ROOT / 'scripts/compile_reader.py'), '--target',
                    '--upstream', str(staged), '--runtime', str(a.runtime), '--system', str(a.system),
                    '--png', str(a.png), '--jpeg', str(a.jpeg), '--output', str(objects)], check=True)
    sys.path.insert(0, str(a.runtime / 'scripts'))
    from link_cpp_module import link
    compiler = os.environ.get('NATIVE_APP_CXX', str(Path.home() / '.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-g++'))
    package = a.output / 'ebook-reader'
    package.mkdir(exist_ok=True)
    elf = package / 'ebook_reader.elf'
    receipt = link(compiler, json.loads((objects / 'objects.json').read_text()), elf,
                   {'app_main', 'app_module_init', 'app_module_fini', 'risc_resident_app_descriptor_v1'})
    validator = a.output / 'validate-elf'
    subprocess.run(['cc', '-std=c11', '-I' + str(a.runtime / 'test/native_apps/stubs'),
                    '-I' + str(a.runtime / 'lib/elf_loader/include'),
                    str(a.runtime / 'lib/elf_loader/src/esp_elf_validate.c'),
                    str(a.runtime / 'test/native_apps/validate_test.c'), '-o', str(validator)], check=True)
    subprocess.run([str(validator), str(elf)], check=True)
    shutil.copyfile(ROOT / 'Apps/ebook_reader.json', package / 'ebook_reader.json')
    # Upstream streamed fonts preserve all styles without mapping every size
    # into the ELF. These assets are copied to the user's SD card separately.
    for family in ('NotoSerif', 'NotoSans'):
        original = a.crosspoint / 'lib/EpdFont/builtinFonts/source' / family
        target = package / 'sd/fonts' / family.replace('Noto', 'Noto ')
        target.mkdir(parents=True, exist_ok=True)
        for source in sorted(original.iterdir()):
            if source.suffix == '.ttf' or source.name == 'OFL.txt':
                shutil.copyfile(source, target / source.name)
    if elf.stat().st_size > 2097152:
        raise ValueError('Reader exceeds the current native app admission limit')
    # Preserve upstream notice files and exact source locations with the binary.
    licenses = package / 'licenses'
    for label, root in [('crosspoint', a.crosspoint), ('freeink', a.crosspoint / 'freeink-sdk'),
                        ('png', a.png), ('jpeg', a.jpeg), ('cxx', a.runtime / 'sdk/cxx/upstream')]:
        for source in sorted(root.rglob('*')):
            if source.is_file() and not '.git' in source.parts and any(key in source.name.lower() for key in ('license', 'copying', 'notice', 'ofl')):
                target = licenses / label / source.relative_to(root)
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(source, target)
    (licenses / 'SOURCES.json').write_text(json.dumps(lock, indent=2) + '\n')
    receipt.update({'upstream': lock, 'physical_testing': 'not performed',
                    'native_admission': 'requires matching native candidate verification',
                    'asset_sha256': {str(p.relative_to(package)): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted((package / 'sd').rglob('*')) if p.is_file()},
                    'source_trees': {name: subprocess.check_output(['git', '-C', str(path), 'rev-parse', 'HEAD^{tree}'], text=True).strip() for name, path in [('runtime', a.runtime), ('system', a.system), ('productivity', ROOT)]},
                    'tracked_source_dirty': {name: bool(subprocess.check_output(['git', '-C', str(path), 'status', '--porcelain', '--untracked-files=no'], text=True)) for name, path in [('runtime', a.runtime), ('system', a.system), ('productivity', ROOT)]},
                    'sources': {name: subprocess.check_output(['git', '-C', str(path), 'rev-parse', 'HEAD'], text=True).strip()
                                for name, path in [('runtime', a.runtime), ('system', a.system), ('productivity', ROOT)]},
                    'input_sha256': {str(p): hashlib.sha256(p.read_bytes()).hexdigest()
                                     for root in (ROOT / 'reader', a.runtime / 'sdk/cxx')
                                     for p in sorted(root.rglob('*')) if p.is_file()}})
    (package / 'build.json').write_text(json.dumps(receipt, indent=2) + '\n')
    return receipt


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('crosspoint', 'runtime', 'system', 'png', 'jpeg', 'output'):
        p.add_argument('--' + name, type=lambda x: Path(x).resolve(), required=True)
    print(json.dumps(build(p.parse_args()), indent=2))
