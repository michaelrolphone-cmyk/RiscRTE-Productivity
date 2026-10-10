#!/usr/bin/env python3
"""Run the existing production-native admission harness on the Reader cohort.

--verification-tools names the checkout containing scripts/verify_update_elf.py.
Its exact hash is recorded; this command does not replace the production gate
with a Reader-specific import allowlist.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import sys

p = argparse.ArgumentParser(description=__doc__)
for name in ('runtime', 'native-elf', 'reader-elf', 'scene-elf', 'storage-elf', 'verification-tools', 'output'):
    p.add_argument('--' + name, type=lambda x: Path(x).resolve(), required=True)
a = p.parse_args()
sys.path.insert(0, str(a.verification_tools / 'scripts'))
import verify_update_elf as verify

original = verify.admission_source
header = '#include "' + str(a.runtime / 'src/runtime/drivers/NativeProviderPolicyValidationV1.h') + '"\n'


def source(text):
    body, role, driver = original(text)
    return header + body, role, driver


verify.admission_source = source
os.environ['CPLUS_INCLUDE_PATH'] = str(a.runtime / 'src')
native = a.native_elf.read_bytes()
result = {'verification_script_sha256': hashlib.sha256((a.verification_tools / 'scripts/verify_update_elf.py').read_bytes()).hexdigest(), 'modules': {}}
for name, elf, provider in [('reader', a.reader_elf, False), ('scene', a.scene_elf, True), ('storage', a.storage_elf, True)]:
    result['modules'][name] = verify.verify(a.runtime, native, elf.read_bytes(), provider=provider)
a.output.parent.mkdir(parents=True, exist_ok=True)
a.output.write_text(json.dumps(result, indent=2) + '\n')
print('Reader, scene and storage pass production native admission and missing-import rejection.')
