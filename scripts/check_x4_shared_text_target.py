#!/usr/bin/env python3
"""Inspect actual target/firmware ELF32 bytes, exports and resident identity."""
import argparse
import hashlib
import json
import struct
from pathlib import Path


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class Elf32:
    def __init__(self, path):
        self.data = path.read_bytes()
        assert self.data[:7] == b'\x7fELF\x01\x01\x01'
        header = struct.unpack_from('<HHIIIIIHHHHHH', self.data, 16)
        assert header[1] == 94
        offset, stride, count, names = header[5], header[10], header[11], header[12]
        self.sections = [struct.unpack_from('<10I', self.data, offset + i * stride) for i in range(count)]
        strings = self.section_bytes(self.sections[names])
        self.by_name = {strings[s[0]:].split(b'\0', 1)[0].decode(): s for s in self.sections}

    def section_bytes(self, section):
        return self.data[section[4]:section[4] + section[5]]

    def symbols(self, table):
        section = self.by_name[table]
        strings = self.section_bytes(self.sections[section[6]])
        result = {}
        for at in range(section[4], section[4] + section[5], section[9]):
            name, value, size, info, other, index = struct.unpack_from('<IIIBBH', self.data, at)
            name = strings[name:].split(b'\0', 1)[0].decode()
            if name:
                result[name] = dict(value=value, size=size, info=info, other=other, index=index)
        return result

    def address(self, address, size):
        for section in self.sections:
            if section[2] & 2 and section[1] != 8 and section[3] <= address and address + size <= section[3] + section[5]:
                offset = section[4] + address - section[3]
                return self.data[offset:offset + size]
        raise ValueError(f'No allocated bytes at {address:#x} size {size}')

    def text(self, address):
        value = bytearray()
        for i in range(256):
            byte = self.address(address + i, 1)[0]
            if not byte:
                return value.decode('ascii')
            value.append(byte)
        raise ValueError('Unterminated export name')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--target', type=Path, required=True)
    parser.add_argument('--native-elf', type=Path, required=True)
    parser.add_argument('--native-bin', type=Path, required=True)
    parser.add_argument('--native-evidence', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    a = parser.parse_args()
    target, native = Elf32(a.target), Elf32(a.native_elf)
    dynamic, all_symbols = target.symbols('.dynsym'), target.symbols('.symtab')
    imports = sorted(name for name, sym in dynamic.items() if sym['index'] == 0)
    exports = sorted(name for name, sym in dynamic.items() if sym['index'] != 0)
    assert exports == ['app_main', 'app_module_fini', 'app_module_init', 'risc_resident_app_descriptor_v1']
    descriptor = dynamic['risc_resident_app_descriptor_v1']
    assert descriptor['size'] == 16 and descriptor['info'] & 15 == 1
    descriptor_words = struct.unpack('<4I', target.address(descriptor['value'], descriptor['size']))
    assert descriptor_words == (1, 16, 2, 0)
    forbidden = sorted(name for name in all_symbols if any(part in name.lower() for part in
                       ('keyboard', 'pqa_render', 'pqa_sheet', 'pqa_font')))
    assert not forbidden
    evidence = json.loads(a.native_evidence.read_text())
    assert sha(a.native_elf) == evidence['elf_sha256']
    assert sha(a.native_bin) == evidence['firmware_sha256']
    exports_native = {}
    native_symbols = native.symbols('.symtab')
    for row in evidence['tables']:
        assert row['table'] in ('g_esp_libc_elfsyms', '_ZZN8RiscBoot7Runtime3runEvE7symbols')
        symbol = native_symbols[row['table']]
        data = native.address(symbol['value'], symbol['size'])
        assert hashlib.sha256(data).hexdigest() == row['sha256']
        for text_address, address in struct.iter_unpack('<II', data):
            if not text_address:
                assert not address
                continue
            name = native.text(text_address)
            assert address and name not in exports_native
            exports_native[name] = address
    assert exports_native == evidence['exports']
    missing = sorted(set(imports) - exports_native.keys())
    assert not missing, missing
    result = dict(target=str(a.target), target_sha256=sha(a.target),
                  native_elf_sha256=sha(a.native_elf), native_bin_sha256=sha(a.native_bin),
                  native_export_evidence_sha256=sha(a.native_evidence),
                  imports=imports, exports=exports, missing_imports=missing,
                  resolved_imports={name: exports_native[name] for name in imports},
                  resident_descriptor=dict(api=1, size=16, role=2, reserved=0),
                  local_keyboard_or_shared_control_symbols=forbidden,
                  target_instructions_executed=False, hardware_verified=False)
    a.output.write_text(json.dumps(result, indent=2) + '\n')
    print('Exact native binary export table closure and foreground descriptor PASS')


if __name__ == '__main__':
    main()
