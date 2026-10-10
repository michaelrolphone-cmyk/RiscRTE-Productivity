import importlib.util
import json
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
spec = importlib.util.spec_from_file_location('timecard_native_build', ROOT / 'scripts/build_timecard_native_time.py')
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)


class TimecardNativeProfileTests(unittest.TestCase):
    def test_exact_native_identity_and_minimum_authority(self):
        manifest = builder.manifest_contract()
        self.assertEqual(manifest['version'], '0.2.9')
        self.assertEqual(manifest['id'], 'timecard')
        self.assertIn({'capability': 'runtime.realtime', 'api': 1}, manifest['requires'])
        self.assertIn({'capability': 'alarm.service', 'api': 2}, manifest['requires'])
        self.assertNotIn('rtc.clock', json.dumps(manifest))
        self.assertNotIn('realtime-control', json.dumps(manifest))

    def test_builder_rejects_expanded_authority_or_reused_identity(self):
        original = builder.manifest_contract()
        for field, value in [('version', '0.1.4'), ('file_name', '../timecard.elf'),
                             ('requires', original['requires'] + [{'capability': 'runtime.realtime-control', 'api': 1}])]:
            changed = {**original, field: value}
            with patch.object(builder.json, 'loads', return_value=changed):
                with self.assertRaises(ValueError):
                    builder.manifest_contract()

    def test_old_profiles_remain_separate(self):
        self.assertEqual(json.loads((ROOT / 'Apps/native/timecard.json').read_text())['version'], '0.2.9')
        self.assertEqual(json.loads((ROOT / 'Apps/timecard.json').read_text())['version'], '1.0.4')
        self.assertNotIn('TIMECARD_NATIVE_TIME', (ROOT / 'scripts/build_timecard_portable.py').read_text())
        self.assertNotIn('-DPORTABLE_RTC_WALL_TIME', builder.DEFINES)
        self.assertNotIn('-DPORTABLE_RTC_UTC8_DENVER', builder.DEFINES)


if __name__ == '__main__':
    unittest.main()
