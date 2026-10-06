import hashlib
import json
from pathlib import Path
import unittest
import importlib.util
from unittest.mock import patch
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('timecard_build', ROOT/'scripts/build_timecard_portable.py')
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)

class TimecardProfileTests(unittest.TestCase):
    def test_original_model_is_not_a_watch_copy(self):
        data = (ROOT/'Apps/timecard.c').read_bytes()
        blob = hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
        self.assertEqual(blob, 'fee216e8b5507b7b2fd4ee5488e34767f4bc13d2')
        source = (ROOT/'Apps/timecard_portable.c').read_text()
        self.assertIn('#include "timecard.c"', source)
        self.assertIn('if(tcp_retained())return;', source)
        self.assertIn('if(!find_day(date) && day_count>=MAX_DAYS)', source)

    def test_appdata_declaration_is_exact_recorded_consumer_copy(self):
        provenance = json.loads((ROOT/'lib/PortableTimecard/SOURCES.json').read_text())
        data = (ROOT/'lib/PortableTimecard/include/RiscAppDataV1.h').read_bytes()
        self.assertEqual(hashlib.sha256(data).hexdigest(), provenance['sha256'])
        self.assertEqual(hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest(), provenance['git_blob'])
        self.assertEqual(provenance['repository'], 'michaelrolphone-cmyk/RiscRTE')
        self.assertIn('consumer-only', provenance['purpose'])

    def test_default_remains_development_and_package_is_explicit(self):
        source = (ROOT/'scripts/build_timecard_portable.py').read_text()
        self.assertIn("manifest = package_manifest(runtime) if args.package_profile else None", source)
        self.assertIn('NOT_INSTALLABLE.txt', source)
        self.assertEqual(json.loads((ROOT/'Apps/timecard.json').read_text())['version'], '1.0.4')

    def test_package_rejects_missing_or_unpublished_runtime(self):
        with self.assertRaises(ValueError):
            builder.package_manifest(None)
        with patch.object(builder.json, 'loads', return_value={'publication_state': 'local prototype'}):
            with self.assertRaises(ValueError):
                builder.package_manifest(ROOT)

    def test_package_requires_exact_clean_runtime_and_complete_authority(self):
        sdk = {'publication_state': 'published', 'source_commit': 'a'*40}
        header = (ROOT/'lib/PortableTimecard/include/RiscAppDataV1.h').read_bytes()
        loads = json.loads
        parse = lambda text: sdk if 'source_commit' in text else loads(text)
        with patch.object(builder.json, 'loads', side_effect=parse), patch.object(Path, 'read_bytes', return_value=header):
            for results in [('b'*40,), ('a'*40, ' M src/backend.cpp')]:
                with patch.object(builder, 'git', side_effect=results), self.assertRaises(ValueError):
                    builder.package_manifest(ROOT)
            with patch.object(builder, 'git', side_effect=['a'*40, '']):
                manifest = builder.package_manifest(ROOT)
        self.assertEqual((manifest['id'], manifest['file_name'], manifest['version']), ('timecard', 'timecard.elf', '0.1.1'))
        self.assertEqual([(x['capability'], x['api']) for x in manifest['requires']], builder.REQUIRES)
        self.assertEqual(len(manifest['requires']), 7)
        self.assertNotIn('optional', manifest)
        self.assertNotIn('instance_id', json.dumps(manifest))

if __name__ == '__main__':
    unittest.main()
