import hashlib
import json
from pathlib import Path
import unittest
ROOT = Path(__file__).resolve().parents[1]

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

    def test_development_builder_has_no_install_manifest(self):
        source = (ROOT/'scripts/build_timecard_portable.py').read_text()
        self.assertIn("'deployable': False", source)
        self.assertIn('NOT_INSTALLABLE.txt', source)
        self.assertNotIn("elf.with_suffix('.json')", source)
        self.assertEqual(json.loads((ROOT/'Apps/timecard.json').read_text())['version'], '1.0.4')

if __name__ == '__main__':
    unittest.main()
