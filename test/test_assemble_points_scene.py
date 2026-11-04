#!/usr/bin/env python3
"""Qualify exact Points assembly against a supplied pinned app and product set."""
import argparse
import copy
import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('points_assembly', ROOT / 'tools/assemble_points_scene.py')
ASSEMBLY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(ASSEMBLY)
CONFIG = None


class PointsAssemblyTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        sys.path[:0] = [str(CONFIG.application / 'sources/System/scripts'), str(CONFIG.bootfs_tools)]
        from image_envelope import MAGIC, decode_image
        from read_only_spiffs import read_image
        cls.magic, cls.decode_image, cls.read_image = MAGIC, staticmethod(decode_image), staticmethod(read_image)
        cls.lock = json.loads((ROOT / 'docs/shared-points/baseline.json').read_text())
        ASSEMBLY.verify_baseline(CONFIG.baseline, cls.lock)
        cls.app, cls.record, cls.manifests = ASSEMBLY.validate_application(CONFIG.application)
        cls.canonical = json.loads((CONFIG.application / ASSEMBLY.MANIFEST).read_text())
        cls.inputs = {}
        for lane in ASSEMBLY.LANES:
            raw = (CONFIG.baseline / (lane + '-store.bin')).read_bytes()
            stored = read_image(raw, len(raw))
            before = {n: decode_image(b) if b.startswith(MAGIC) else b for n, b in stored.items()}
            sd = ASSEMBLY.read_archive(CONFIG.baseline / (lane + '-sd-apps.zip'))
            cls.inputs[lane] = before, sd, stored, len(raw)
        cls.temp = tempfile.TemporaryDirectory(prefix='points-assembly-test-')
        cls.work = Path(cls.temp.name)
        cls.output = cls.work / 'first'
        cls.run_assembly(cls.output)
        cls.receipt = json.loads((cls.output / 'assembly.json').read_text())

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    @classmethod
    def run_assembly(cls, output):
        command = [sys.executable, str(ROOT / 'tools/assemble_points_scene.py'),
                   '--application', str(CONFIG.application), '--baseline', str(CONFIG.baseline),
                   '--bootfs-tools', str(CONFIG.bootfs_tools), '--output', str(output)]
        result = subprocess.run(command, text=True, capture_output=True, check=False)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)

    def lane(self, lane='watch', before=None, sd=None, manifest=None):
        original, old_sd, _, _ = self.inputs[lane]
        return ASSEMBLY.assemble_lane(lane, original if before is None else before,
                                      old_sd if sd is None else sd,
                                      self.manifests[lane] if manifest is None else manifest, self.app)

    def modified_boot(self, change, lane='watch'):
        before = dict(self.inputs[lane][0])
        boot = json.loads(before['boot.json'])
        change(boot)
        before['boot.json'] = ASSEMBLY.encode(boot)
        return before

    def assert_bad_manifest(self, change, message):
        manifest = copy.deepcopy(self.manifests['watch'])
        change(manifest)
        raw = ASSEMBLY.encode(manifest)
        selection = dict(self.record['product_selections']['watch'], manifest_sha256=ASSEMBLY.digest(raw))
        with self.assertRaisesRegex(ValueError, message):
            ASSEMBLY.validate_selection('watch', raw, selection, ASSEMBLY.digest(self.app), self.canonical)

    def test_three_packed_products_preserve_authority_and_bytes(self):
        hashes = set()
        self.assertEqual(set(self.receipt['products']), set(ASSEMBLY.LANES))
        for lane, alarm_api in ASSEMBLY.LANES.items():
            with self.subTest(lane=lane):
                before, sd0, stored0, size = self.inputs[lane]
                product = self.output / lane
                receipt = self.receipt['products'][lane]
                raw = (product / 'store.bin').read_bytes()
                self.assertEqual(len(raw), size)
                self.assertEqual(receipt['store_bytes'], size)
                stored = self.read_image(raw, size)
                out = {n: self.decode_image(b) if b.startswith(self.magic) else b for n, b in stored.items()}
                sd = ASSEMBLY.read_archive(product / 'sd-apps.zip')
                unpacked = {p.relative_to(product / 'stored').as_posix(): p.read_bytes()
                            for p in (product / 'stored').rglob('*') if p.is_file()}
                self.assertEqual(stored, unpacked)
                self.assertEqual(set(stored), set(stored0))
                actual = {n for n in out if out[n] != before[n]}
                self.assertEqual(actual, set(receipt['changed_internal']))
                for name in set(stored0) - actual:
                    self.assertEqual(stored[name], stored0[name], name)
                self.assertEqual(set(sd) - set(sd0), set(receipt['added_sd']))
                self.assertEqual(set(sd0) - set(sd), set(receipt['removed_sd']))
                sd_changed = {n for n in set(sd) & set(sd0) if sd[n] != sd0[n]}
                self.assertEqual(sd_changed, set(receipt['changed_sd']))
                for name in set(sd0) - sd_changed - set(receipt['removed_sd']):
                    self.assertEqual(sd[name], sd0[name], name)
                old_boot, new_boot = json.loads(before['boot.json']), json.loads(out['boot.json'])
                rows = [r for r in new_boot['app_capabilities'] if r['manifest'] == ASSEMBLY.MANIFEST]
                self.assertEqual(rows, [receipt['app_row']])
                old_row = next(r for r in old_boot['app_capabilities'] if r['manifest'] == ASSEMBLY.MANIFEST)
                restored = copy.deepcopy(new_boot)
                restored['app_capabilities'][restored['app_capabilities'].index(rows[0])] = old_row
                self.assertEqual(restored, old_boot)
                storage = lambda row: [g for g in row['grants'] if g['capability'].startswith('storage.')]
                self.assertEqual(storage(rows[0]), storage(old_row))
                self.assertEqual([g['api'] for g in rows[0]['grants'] if g['capability'] == 'alarm.service'], [alarm_api])
                manifest = json.loads(out[ASSEMBLY.MANIFEST])
                ASSEMBLY.validate_manifest(manifest, alarm_api, source=False)
                old_manifest = json.loads(before[ASSEMBLY.MANIFEST])
                for key, value in old_manifest.items():
                    if key not in ('requires', 'optional', 'version', 'entry', 'architecture', 'runtime_profile'):
                        self.assertEqual(manifest[key], value)
                for driver in old_boot['drivers']:
                    self.assertEqual(out[driver['manifest']], before[driver['manifest']])
                    volume, path = ASSEMBLY.route(before, driver)
                    old_target, new_target = (sd0, sd) if volume == 'sd' else (before, out)
                    if path in old_target:
                        self.assertEqual(new_target[path], old_target[path])
                    else:
                        self.assertTrue(driver.get('image_source', {}).get('internal_fallback'), path)
                    # Any internal fallback remains byte-identical even when
                    # its optional SD content address is absent in the baseline.
                    driver_manifest = json.loads(before[driver['manifest']])
                    internal = str(Path(driver['manifest']).parent / driver_manifest['file_name'])
                    if internal in before:
                        self.assertEqual(out[internal], before[internal])
                if ('time.civil', 1) in ASSEMBLY.declarations(manifest, 'optional'):
                    self.assertIn({'capability': 'time.civil', 'api': 1, 'instance_id': 0}, rows[0]['grants'])
                route = receipt['app_route']
                payload = (sd if route['storage'] == 'sd' else out)[route['path']]
                self.assertEqual(payload, self.app)
                hashes.add(ASSEMBLY.digest(payload))
                self.assertEqual(out.get('cohort.json'), before.get('cohort.json'))
                self.assertTrue(receipt['store_roundtrip'] and receipt['sd_roundtrip'])
                if lane == 'x4':
                    old_path = old_row['image_source']['path']
                    self.assertNotIn(old_path, sd)
                    self.assertIn('/image-' + ASSEMBLY.digest(self.app) + '/', route['path'])
                    self.assertEqual([n for n in sd if n.startswith('Apps/points_in_time/') and n.endswith('/points_in_time.elf')], [route['path']])
                    self.assertEqual(json.loads(sd[str(Path(route['path']).parent / 'manifest.json')]), manifest)
                if lane == 'wave':
                    self.assertEqual(route['path'], old_row['image_source']['path'])
                if lane == 'watch':
                    self.assertEqual(route, {'storage': 'internal', 'path': ASSEMBLY.ELF})
        self.assertEqual(hashes, {self.record['sha256']})

    def test_repeated_packing_is_identical(self):
        second = self.work / 'second'
        self.run_assembly(second)
        self.assertEqual((second / 'assembly.json').read_bytes(), (self.output / 'assembly.json').read_bytes())
        for lane in ASSEMBLY.LANES:
            for name in ('store.bin', 'sd-apps.zip'):
                self.assertEqual((second / lane / name).read_bytes(), (self.output / lane / name).read_bytes())

    def test_wrong_manifest_identity_fields(self):
        for key, value in [('id', 'other'), ('file_name', 'other.elf'), ('entry', 'other_main'),
                           ('type', 'driver'), ('architecture', 'other'), ('runtime_profile', 'physical')]:
            with self.subTest(key=key):
                self.assert_bad_manifest(lambda m: m.update({key: value}), 'Wrong application')

    def test_wrong_selection_hash(self):
        raw = (CONFIG.application / 'selections/watch' / ASSEMBLY.MANIFEST).read_bytes()
        for key in ('manifest_sha256', 'elf_sha256'):
            selection = dict(self.record['product_selections']['watch'], **{key: '0' * 64})
            with self.assertRaisesRegex(ValueError, 'Selection differs'):
                ASSEMBLY.validate_selection('watch', raw, selection, ASSEMBLY.digest(self.app), self.canonical)

    def test_wrong_alarm_selection_and_declarations(self):
        self.assert_bad_manifest(lambda m: m['optional'].append({'capability': 'alarm.service', 'api': 2}), 'Alarm declaration')
        raw = (CONFIG.application / 'selections/watch' / ASSEMBLY.MANIFEST).read_bytes()
        selection = dict(self.record['product_selections']['watch'], alarm_api=2)
        with self.assertRaisesRegex(ValueError, 'Selection differs'):
            ASSEMBLY.validate_selection('watch', raw, selection, ASSEMBLY.digest(self.app), self.canonical)

    def test_bad_scene_declaration(self):
        self.assert_bad_manifest(lambda m: m.update(requires=[]), 'scene/physical')
        self.assert_bad_manifest(lambda m: m['optional'].append({'capability': 'input.touch.raw', 'api': 1}), 'scene/physical')

    def test_duplicate_and_overlapping_declarations(self):
        self.assert_bad_manifest(lambda m: m['requires'].append(dict(m['requires'][0])), 'Duplicate')
        self.assert_bad_manifest(lambda m: m['optional'].append(dict(m['requires'][0])), 'Overlapping')
        self.assert_bad_manifest(lambda m: m['optional'][0].update(api=True), 'Invalid capability')

    def test_selection_cannot_silently_change_other_metadata(self):
        self.assert_bad_manifest(lambda m: m.update(display_name='Another app'), 'canonical application')

    def test_absent_scene_provider(self):
        before = self.modified_boot(lambda b: b.update(drivers=[d for d in b['drivers'] if d['manifest'] != 'ui-scene/manifest.json']))
        with self.assertRaisesRegex(ValueError, 'Scene provider missing or ambiguous'):
            self.lane(before=before)

    def test_ambiguous_scene_provider(self):
        before = self.modified_boot(lambda b: b['drivers'].append(copy.deepcopy(next(d for d in b['drivers'] if d['manifest'] == 'ui-scene/manifest.json'))))
        with self.assertRaisesRegex(ValueError, 'Scene provider missing or ambiguous'):
            self.lane(before=before)

    def test_time_civil_grant_preserves_existing_provider(self):
        for lane in ASSEMBLY.LANES:
            with self.subTest(lane=lane):
                manifest = copy.deepcopy(self.manifests[lane])
                if ('time.civil', 1) not in ASSEMBLY.declarations(manifest, 'optional'):
                    manifest['optional'].append({'capability': 'time.civil', 'api': 1})
                before, sd0, _, _ = self.inputs[lane]
                out, sd, r = self.lane(lane, manifest=manifest)
                self.assertIn({'capability': 'time.civil', 'api': 1, 'instance_id': 0}, r['app_row']['grants'])
                boot = json.loads(before['boot.json'])
                provider = next(d for d in boot['drivers'] if ('time.civil', 1) in
                                ASSEMBLY.declarations(json.loads(before[d['manifest']]), 'provides'))
                self.assertEqual(before[provider['manifest']], out[provider['manifest']])
                volume, path = ASSEMBLY.route(before, provider)
                self.assertEqual((sd0 if volume == 'sd' else before)[path], (sd if volume == 'sd' else out)[path])

    def test_absent_or_ambiguous_time_civil_provider(self):
        for mode in ('absent', 'ambiguous', 'wrong-instance'):
            manifest = copy.deepcopy(self.manifests['watch'])
            if ('time.civil', 1) not in ASSEMBLY.declarations(manifest, 'optional'):
                manifest['optional'].append({'capability': 'time.civil', 'api': 1})
            def change(boot):
                provider = next(d for d in boot['drivers'] if d['manifest'] == 'civil-clock/manifest.json')
                if mode == 'absent':
                    boot['drivers'].remove(provider)
                elif mode == 'ambiguous':
                    boot['drivers'].append(copy.deepcopy(provider))
                else:
                    provider['instance_id'] = 1
            with self.subTest(mode=mode), self.assertRaisesRegex(ValueError, 'Civil time provider missing or ambiguous'):
                self.lane(before=self.modified_boot(change), manifest=manifest)

    def test_ambiguous_points_authority(self):
        before = self.modified_boot(lambda b: b['app_capabilities'].append(copy.deepcopy(next(r for r in b['app_capabilities'] if r['manifest'] == ASSEMBLY.MANIFEST))))
        with self.assertRaisesRegex(ValueError, 'Points authority ambiguous'):
            self.lane(before=before)

    def test_aliased_points_identity(self):
        before = dict(self.inputs['watch'][0])
        before['other-points.json'] = before[ASSEMBLY.MANIFEST]
        boot = json.loads(before['boot.json'])
        row = copy.deepcopy(next(r for r in boot['app_capabilities'] if r['manifest'] == ASSEMBLY.MANIFEST))
        row['manifest'] = 'other-points.json'
        boot['app_capabilities'].append(row)
        before['boot.json'] = ASSEMBLY.encode(boot)
        with self.assertRaisesRegex(ValueError, 'Points authority ambiguous'):
            self.lane(before=before)

    def test_wrong_alarm_authority(self):
        def change(boot):
            row = next(r for r in boot['app_capabilities'] if r['manifest'] == ASSEMBLY.MANIFEST)
            next(g for g in row['grants'] if g['capability'] == 'alarm.service')['api'] = 2
        with self.assertRaisesRegex(ValueError, 'Unreviewed grant removal|Alarm authority'):
            self.lane(before=self.modified_boot(change))

    def test_duplicate_grant(self):
        def change(boot):
            row = next(r for r in boot['app_capabilities'] if r['manifest'] == ASSEMBLY.MANIFEST)
            row['grants'].append(copy.deepcopy(next(g for g in row['grants'] if g['capability'] == 'storage.key-value')))
        with self.assertRaisesRegex(ValueError, 'Duplicate grant'):
            self.lane(before=self.modified_boot(change))

    def test_required_capability_without_grant(self):
        manifest = copy.deepcopy(self.manifests['watch'])
        manifest['requires'].append({'capability': 'test.missing', 'api': 1})
        with self.assertRaisesRegex(ValueError, 'Missing required'):
            self.lane(manifest=manifest)

    def test_missing_application_route(self):
        before = dict(self.inputs['watch'][0])
        del before[ASSEMBLY.ELF]
        with self.assertRaisesRegex(ValueError, 'Application route missing'):
            self.lane(before=before)

    def test_changed_sd_sidecar(self):
        sd = dict(self.inputs['wave'][1])
        sd['Apps/points_in_time/manifest.json'] = b'{}'
        with self.assertRaisesRegex(ValueError, 'SD sidecar differs'):
            self.lane('wave', sd=sd)

    def test_wrong_original_content_address(self):
        before, sd0, _, _ = self.inputs['x4']
        row = next(r for r in json.loads(before['boot.json'])['app_capabilities'] if r['manifest'] == ASSEMBLY.MANIFEST)
        sd = dict(sd0)
        sd[row['image_source']['path']] += b'changed'
        with self.assertRaisesRegex(ValueError, 'Original Points content address differs'):
            self.lane('x4', sd=sd)

    def test_stale_content_addressed_duplicate(self):
        sd = dict(self.inputs['x4'][1])
        sd['Apps/points_in_time/image-' + '0' * 64 + '/points_in_time.elf'] = self.app
        with self.assertRaisesRegex(ValueError, 'Stale or ambiguous'):
            self.lane('x4', sd=sd)

    def test_unknown_content_addressed_assets(self):
        before, sd0, _, _ = self.inputs['x4']
        row = next(r for r in json.loads(before['boot.json'])['app_capabilities'] if r['manifest'] == ASSEMBLY.MANIFEST)
        sd = dict(sd0)
        sd[str(Path(row['image_source']['path']).parent / 'unreviewed.txt')] = b'other'
        with self.assertRaisesRegex(ValueError, 'Unknown content-addressed assets'):
            self.lane('x4', sd=sd)

    def test_modified_unrelated_data_is_rejected(self):
        for target in ('internal', 'sd', 'boot', 'storage'):
            with self.subTest(target=target):
                before, sd0, _, _ = self.inputs['x4']
                out, sd, r = self.lane('x4')
                row = copy.deepcopy(r['app_row'])
                if target == 'internal':
                    name = next(n for n in out if n not in r['changed_internal'])
                    out[name] += b'changed'
                elif target == 'sd':
                    name = next(n for n in sd if n not in r['changed_sd'] + r['added_sd'])
                    sd[name] += b'changed'
                else:
                    boot = json.loads(out['boot.json'])
                    if target == 'boot':
                        boot['unexpected'] = True
                    else:
                        next(g for g in row['grants'] if g['capability'] == 'storage.key-value')['instance_id'] = 999
                        boot['app_capabilities'][boot['app_capabilities'].index(r['app_row'])] = row
                    out['boot.json'] = ASSEMBLY.encode(boot)
                with self.assertRaisesRegex(ValueError, 'Unrelated|Storage namespace'):
                    ASSEMBLY.verify_preservation(before, sd0, out, sd, r['original_app_row'], row,
                        set(r['changed_internal']), set(r['changed_sd']), set(r['removed_sd']), set(r['added_sd']))

    def test_modified_baseline_asset_rejected(self):
        with tempfile.TemporaryDirectory(dir=self.work) as directory:
            directory = Path(directory)
            for row in self.lock['assets']:
                (directory / row['name']).symlink_to((CONFIG.baseline / row['name']).resolve())
            path = directory / 'watch-sd-apps.zip'
            path.unlink()
            path.write_bytes(b'changed')
            with self.assertRaisesRegex(ValueError, 'Baseline differs'):
                ASSEMBLY.verify_baseline(directory, self.lock)

    def test_wrong_application_hash_and_repeat_rejected(self):
        for changed in ('points_in_time.elf', 'points_in_time.repeat.elf'):
            with tempfile.TemporaryDirectory(dir=self.work) as directory:
                directory = Path(directory)
                for name in ('points_in_time.elf', 'points_in_time.repeat.elf', 'build.json', ASSEMBLY.MANIFEST, 'selections'):
                    (directory / name).symlink_to((CONFIG.application / name).resolve())
                path = directory / changed
                path.unlink()
                path.write_bytes(self.app + b'changed')
                with self.assertRaisesRegex(ValueError, 'Application build differs'):
                    ASSEMBLY.validate_application(directory)

    def test_unsafe_and_duplicate_zip_paths(self):
        for name in ('../escape', '/absolute', 'a//b', 'a/./b', 'a\\b'):
            with self.subTest(name=name):
                with self.assertRaisesRegex(ValueError, 'Unsafe'):
                    ASSEMBLY.safe(name)
        path = self.work / 'duplicate.zip'
        import warnings
        with warnings.catch_warnings():
            warnings.simplefilter('ignore', UserWarning)
            with zipfile.ZipFile(path, 'w') as archive:
                archive.writestr('same', b'a')
                archive.writestr('same', b'b')
        with self.assertRaisesRegex(ValueError, 'Duplicate ZIP'):
            ASSEMBLY.read_archive(path)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('application', 'baseline', 'bootfs-tools'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--report', type=Path)
    CONFIG = parser.parse_args()
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(PointsAssemblyTest)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    if CONFIG.report:
        CONFIG.report.parent.mkdir(parents=True, exist_ok=True)
        report = {'schema': 1, 'passed': result.wasSuccessful(), 'tests': result.testsRun,
                  'failures': len(result.failures), 'errors': len(result.errors),
                  'application': str(CONFIG.application), 'app_sha256': PointsAssemblyTest.record['sha256'],
                  'assembler_sha256': ASSEMBLY.digest((ROOT / 'tools/assemble_points_scene.py').read_bytes()),
                  'test_sha256': ASSEMBLY.digest(Path(__file__).read_bytes()),
                  'baseline_lock_sha256': ASSEMBLY.digest((ROOT / 'docs/shared-points/baseline.json').read_bytes()),
                  'products': {lane: {'store_bytes': PointsAssemblyTest.inputs[lane][3], 'alarm_api': api}
                               for lane, api in ASSEMBLY.LANES.items()},
                  'fixed_size_store_roundtrips': True if result.wasSuccessful() else None,
                  'physical_execution': False, 'publication': False}
        CONFIG.report.write_bytes(ASSEMBLY.encode(report))
    sys.exit(not result.wasSuccessful())
