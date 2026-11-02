#!/usr/bin/env python3
"""Shared Daily Paper source contract, separate from actual-entrypoint host tests."""
import json,re
from pathlib import Path
root=Path(__file__).resolve().parent
manifest=json.loads((root.parent/'Apps/daily_digest.json').read_text())
assert manifest['id']=='daily-digest' and manifest['file_name']=='daily_digest.elf'
assert manifest['requires']==[{'capability':'ui.scene','api':1}]
assert manifest['optional']==[{'capability':cap,'api':1} for cap in
 ('file.open','storage.volume','runtime.realtime','net.wifi','storage.key-value','net.http-client')]
source=(root/'app.c').read_text()
assert not re.search(r'(?i)\b(X4|T_WATCH|WAVESHARE|device_class|screen_width|screen_height)\b',source)
assert 'RISC_COMPONENT_PROGRESS' in source and 'RISC_COMPONENT_EMPTY' in source
assert 'reader_service()' in source and 'READER SERVICE UNAVAILABLE' in source
assert 'recover_replace' in source and 'replace_file' in source and 'file_sync' in source
assert 'READER_COOKIE' in source and 'open_take_result' in source
print('PASS optional capability declarations, generic scene UI, transactional storage and Reader-return contracts')
