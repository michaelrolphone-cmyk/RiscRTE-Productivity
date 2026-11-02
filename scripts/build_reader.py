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
    from reader_stack_budget import check
    stack_frames=check(objects)
    sys.path.insert(0, str(a.runtime / 'scripts'))
    from link_cpp_module import link
    compiler = os.environ.get('NATIVE_APP_CXX', str(Path.home() / '.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-g++'))
    package = a.output / 'ebook-reader'
    package.mkdir(exist_ok=True)
    elf = package / 'ebook_reader.elf'
    def produce(elf, flag):
        receipt = link(compiler, json.loads((objects / 'objects.json').read_text()) + ([flag] if flag else []), elf,
                       {'app_main', 'app_module_init', 'app_module_fini', 'risc_resident_app_descriptor_v1'})
        # Production shared objects do not need the static linker/debug symbol table.
        # Preserve every loadable section, relocation, dynamic import/export and
        # virtual address byte-for-byte. This is NOT feature stripping or a reduced
        # engine; it lets the complete engine fit the existing Watch store.
        from elftools.elf.elffile import ELFFile
        from link_cpp_module import mapped_relocations
        def runtime_image(path):
            with path.open('rb') as stream:
                image = ELFFile(stream)
                sections = list(image.iter_sections())
                return {section.name: {
                    'type': section['sh_type'], 'flags': section['sh_flags'],
                    'address': section['sh_addr'], 'size': section['sh_size'],
                    'alignment': section['sh_addralign'], 'entry_size': section['sh_entsize'],
                    'link': sections[section['sh_link']].name if section['sh_link'] else '',
                    'sha256': hashlib.sha256(section.data() if section['sh_type'] != 'SHT_NOBITS' else b'').hexdigest()
                } for section in sections if section['sh_flags'] & 2 or section['sh_type'] in ('SHT_REL', 'SHT_RELA')}
        before = runtime_image(elf)
        nm = compiler.removesuffix('g++') + 'nm'
        dynamic_symbols = subprocess.check_output([nm, '-D', str(elf)])
        largest_symbols = subprocess.check_output([nm, '-S', '--size-sort', '-C', str(elf)], text=True).splitlines()[-60:]
        print('Largest complete Reader symbols:\n' + '\n'.join(largest_symbols), flush=True)
        unstripped = {'bytes': elf.stat().st_size, 'sha256': hashlib.sha256(elf.read_bytes()).hexdigest()}
        subprocess.run([compiler.removesuffix('g++') + 'strip', '--strip-unneeded', '--remove-section=.xt.prop', '--remove-section=.xt.lit', '--remove-section=.xt.insn', '--remove-section=.comment', str(elf)], check=True)
        if runtime_image(elf) != before or subprocess.check_output([nm, '-D', str(elf)]) != dynamic_symbols:
            raise ValueError('Stripping changed loadable bytes, relocation semantics or dynamic symbols')
        if mapped_relocations(elf) != receipt['mapped_relocations']:
            raise ValueError('Stripping changed mapped relocations')
        receipt.update({'bytes': elf.stat().st_size, 'sha256': hashlib.sha256(elf.read_bytes()).hexdigest(),
                        'production_symbols': {'method': 'strip --strip-unneeded; remove non-ALLOC Xtensa linker property/literal/instruction metadata and compiler comment', 'unstripped': unstripped,
                        'runtime_sections': before, 'runtime_bytes_and_dynamic_symbols_unchanged': True}})
        return receipt
    # Layout-only alternatives preserve the complete code/data and ABI. Select
    # deterministically by the actual existing LZ4 storage codec, not raw size.
    import ctypes
    lib=ctypes.CDLL('liblz4.so.1')
    lib.LZ4_versionNumber.restype=ctypes.c_int
    assert lib.LZ4_versionNumber()==10904
    lib.LZ4_compressBound.argtypes=[ctypes.c_int];lib.LZ4_compressBound.restype=ctypes.c_int
    lib.LZ4_compress_HC.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_int,ctypes.c_int,ctypes.c_int];lib.LZ4_compress_HC.restype=ctypes.c_int
    def storage_size(path):
        data=path.read_bytes();source=ctypes.create_string_buffer(data);bound=lib.LZ4_compressBound(len(data));target=ctypes.create_string_buffer(bound)
        result=lib.LZ4_compress_HC(source,target,len(data),bound,12)
        assert result>0
        return result
    candidates=[];layouts={}
    for label,flag in [('default',''),('alignment','-Wl,--sort-section=alignment'),('name','-Wl,--sort-section=name')]:
        path=a.output/'layout-candidates'/(label+'.elf')
        result=produce(path,flag);stored=storage_size(path)
        layouts[label]={'flag':flag,'bytes':result['bytes'],'sha256':result['sha256'],'lz4_block_bytes':stored}
        candidates.append((stored,label,path,result))
        print('COMPLETE READER LAYOUT '+json.dumps({'layout':label,**layouts[label]}),flush=True)
    _,selected,path,receipt=min(candidates,key=lambda x:(x[0],x[1]))
    shutil.copyfile(path,elf)
    receipt['layout_selection']={'method':'Minimum LZ4HC1.9.4level12 size among deterministic complete linker layouts','selected':selected,'candidates':layouts}
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
    receipt.update({'font_sharing': json.loads((staged / 'font-sharing.json').read_text()),
                    'optimization': 'Support-unit LTO; historically stack-budgeted ReaderApp/ReaderEngine remain direct target objects',
                    'upstream': lock, 'physical_testing': 'not performed', 'target_stack_frames':stack_frames,
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
