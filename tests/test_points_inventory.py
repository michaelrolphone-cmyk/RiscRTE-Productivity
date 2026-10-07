import json
import pathlib
import sys
import unittest
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'scripts'))
from build_points_in_time import ROOT, SYSTEM_PIN, UTILITIES_PIN, REQUIRES, inventory, verify_dependency

class PointsInventoryTests(unittest.TestCase):
    def test_distinct_original_profile_preserves_legacy(self):
        app, manifest = inventory()
        self.assertEqual(app['version'], '0.4.4')
        self.assertEqual(manifest['display_name'], 'Points in Time')
        data = json.loads((ROOT/'productivity-manifest.json').read_text())
        self.assertEqual([row['id'] for row in data['apps']], ['text_editor', 'timecard'])
        self.assertEqual([row['version'] for row in data['apps']], ['0.2.3', '1.0.4'])
    def test_authority_is_consumer_only(self):
        self.assertEqual(len(REQUIRES), 5)
        self.assertEqual({cap for cap, _ in REQUIRES}, {'display.output', 'input.touch.raw', 'rtc.clock', 'storage.key-value', 'alarm.service'})
        source = (ROOT/'Apps/points_in_time.c').read_text()
        self.assertNotIn('alarm.output', source)
        self.assertNotIn('board.vibration', source)
        self.assertNotIn('points_occ', source)
        self.assertIn('portable_app_sleep_retained()', source)
    def test_exact_dependency_pins(self):
        self.assertRegex(SYSTEM_PIN, r'^[0-9a-f]{40}$')
        self.assertRegex(UTILITIES_PIN, r'^[0-9a-f]{40}$')
        workflow = (ROOT/'.github/workflows/points-in-time.yml').read_text()
        self.assertIn('ref: '+SYSTEM_PIN, workflow)
        self.assertIn('ref: '+UTILITIES_PIN, workflow)
    def test_dependency_rejects_unpinned(self):
        with self.assertRaises(ValueError):
            verify_dependency(ROOT, 'main')
    def test_no_legacy_baseline_expansion(self):
        baseline = json.loads((ROOT/'sdk/release-baseline.json').read_text())
        self.assertNotIn('points_in_time', json.dumps(baseline))
        self.assertNotIn('points_in_time', (ROOT/'scripts/build_all_apps.py').read_text())
if __name__ == '__main__':
    unittest.main()
