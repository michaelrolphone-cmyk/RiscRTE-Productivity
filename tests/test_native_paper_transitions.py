import argparse
import contextlib
import copy
import io
import json
import hashlib
import sys
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
import native_paper_transitions as motion
import build_points_native_utc as points
import build_timecard_native_time as timecard


class NativePaperTransitions(unittest.TestCase):
    def parser(self):
        parser = argparse.ArgumentParser()
        motion.options(parser)
        return parser

    def test_off_preserves_inputs_versions_and_authority(self):
        parser = self.parser()
        manifest = timecard.manifest_contract()
        original = copy.deepcopy(manifest)
        adapter = Path('/original-adapter')
        selected, flags, result, receipt = motion.select(parser.parse_args([]), parser, adapter, timecard.DEFINES, manifest)
        self.assertEqual((selected, flags, result, receipt), (adapter, timecard.DEFINES, original, None))
        self.assertEqual(manifest, original)
        self.assertNotIn(motion.PIN['define'], points.DEFINES)
        self.assertNotIn(motion.PIN['define'], timecard.DEFINES)

    def test_incomplete_selection_fails(self):
        parser = self.parser()
        for flags in [['--paper-transitions'], ['--motion-system', '/missing'], ['--ble-broadcast']]:
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                motion.select(parser.parse_args(flags), parser, Path('/original'), [])

    def test_exact_motion_identity_required(self):
        parser = self.parser()
        args = parser.parse_args(['--paper-transitions', '--motion-system', '/motion'])
        for replies in [['0' * 40], [motion.PIN['adapter_sha'], ' M source.c']]:
            with patch.object(motion.subprocess, 'check_output', side_effect=replies):
                with self.assertRaisesRegex(ValueError, 'Clean exact motion'):
                    motion.select(args, parser, Path('/original'), [])

    def test_broadcast_identity_cannot_use_old_motion_source(self):
        parser = self.parser()
        args = parser.parse_args(['--ble-broadcast', '--paper-transitions', '--motion-system', '/motion'])
        with patch.object(motion.subprocess, 'check_output', return_value=motion.PIN['adapter_sha']):
            with self.assertRaisesRegex(ValueError, 'Clean exact motion'):
                motion.select(args, parser, Path('/original'), [])

    def test_raw_watch_manifests_and_runtime_alarm_pins_unchanged(self):
        expected = json.loads((ROOT / "tests/preserved-profile-baseline.json").read_text())["sha256"]
        for name in ['sdk/points-native-utc-sources.json', 'sdk/timecard-native-time-sources.json',
                     'Apps/native/points_utc.json', 'Apps/native/timecard_native_time.json',
                     'Apps/native/timecard.json', 'Apps/points_in_time.json', 'Apps/timecard.json',
                     'productivity-manifest.json', 'scripts/build_points_in_time.py',
                     'scripts/build_timecard_portable.py']:
            self.assertEqual(hashlib.sha256((ROOT / name).read_bytes()).hexdigest(), expected[name], name)
        self.assertEqual(motion.PIN['versions'], {'points_in_time': '0.6.3', 'timecard': '0.2.9'})
        self.assertEqual(json.loads((ROOT / 'Apps/native/points_utc.json').read_text())['version'], '0.6.2')
        self.assertEqual(timecard.manifest_contract()['version'], '0.2.9')


if __name__ == '__main__':
    unittest.main()
