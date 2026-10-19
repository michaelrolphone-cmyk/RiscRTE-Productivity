#!/usr/bin/env python3
"""Fail-closed development-builder contract checks; no target or device writes."""
import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import build_points_native_ui as build


class ProfileTests(unittest.TestCase):
    def setUp(self):
        self.profile = json.loads(build.PROFILE.read_text())
        self.manifest = json.loads(build.MANIFEST.read_text())
        self.roots = {name: Path('/fixture') / name for name in
                      ('productivity', 'system', 'runtime', 'utilities')}

    def validate(self):
        def git(root, *args):
            if args == ('rev-parse', 'HEAD'):
                return self.profile['dependencies'][root.name]['base_revision']
            return ''
        def inventory(root, folders, files):
            return self.profile['dependencies'][root.name]['source_sha256']
        with patch.object(build, 'git', side_effect=git), patch.object(build, 'check_hashes'), \
             patch.object(build, 'inventory', side_effect=inventory):
            build.validate(self.profile, self.manifest, self.roots)

    def test_expected_profile(self):
        self.validate()
        self.assertNotIn('PORTABLE_RETURN_APP="springboard.elf"', build.DEFINES)
        self.assertIn('POINTS_RETURN_APP="springboard.elf"', build.DEFINES)
        self.assertEqual(self.profile['version'], '0.7.0')
        self.assertIn('PORTABLE_ALARM_NATIVE', build.DEFINES)

    def test_no_selector_drift(self):
        self.profile['defines'].remove('PORTABLE_POINTS_NATIVE_UI')
        with self.assertRaisesRegex(ValueError, 'selector'):
            self.validate()

    def test_native_alarm_selector_required(self):
        self.profile['defines'].remove('PORTABLE_ALARM_NATIVE')
        with self.assertRaisesRegex(ValueError, 'selector'):
            self.validate()

    def test_no_owner_drift(self):
        self.profile['private_owner'] = 10
        with self.assertRaisesRegex(ValueError, 'ownership'):
            self.validate()

    def test_no_capability_expansion(self):
        self.manifest['requires'].append({'capability': 'runtime.time-control', 'api': 1})
        with self.assertRaisesRegex(ValueError, 'manifest'):
            self.validate()

    def test_no_alarm_api_downgrade(self):
        self.manifest['requires'][-1]['api'] = 1
        with self.assertRaisesRegex(ValueError, 'manifest'):
            self.validate()

    def test_no_hardware_or_product_claim(self):
        for field in ('hardware_verified', 'product_recipe'):
            with self.subTest(field=field):
                self.profile[field] = True
                with self.assertRaisesRegex(ValueError, 'Development profile'):
                    self.validate()
                self.profile[field] = False

    def test_strict_compiler_identity(self):
        self.profile['compiler'] = 'gcc 15.2.0'
        with self.assertRaisesRegex(ValueError, 'GCC8.4'):
            self.validate()

    def test_source_mutation_and_unlisted_file(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'include').mkdir()
            header = root / 'include/test.h'
            header.write_text('first')
            expected = build.inventory(root, ['include'], [])
            build.check_hashes(root, expected)
            header.write_text('changed')
            with self.assertRaisesRegex(ValueError, 'Frozen source changed'):
                build.check_hashes(root, expected)
            header.write_text('first')
            (root / 'include/unlisted.h').write_text('new')
            self.assertNotEqual(build.inventory(root, ['include'], []), expected)

    def test_no_source_path_escape(self):
        with self.assertRaisesRegex(ValueError, 'Invalid frozen'):
            build.check_hashes(Path('/fixture'), {'../escape': '0' * 64})

    def test_empty_freeze_rejected(self):
        with self.assertRaisesRegex(ValueError, 'Missing frozen'):
            build.check_hashes(Path('/fixture'), {})

    def symbol_fixture(self):
        dynamic = '\n'.join('00000000 T ' + name for name in build.EXPORTS)
        dynamic += '\n         U risc_runtime_get_api\n'
        private = '\n'.join('1: 00001000 4 FUNC LOCAL DEFAULT 3 ' + name for name in build.PRIVATE)
        return dynamic, private

    def test_private_abi(self):
        dynamic, private = self.symbol_fixture()
        imports, exports, hooks = build.check_symbols(dynamic, private)
        self.assertEqual(imports, {'risc_runtime_get_api'})
        self.assertEqual(exports, build.EXPORTS)
        self.assertEqual(set(hooks), build.PRIVATE)

    def test_no_extra_public_or_weak_export(self):
        dynamic, private = self.symbol_fixture()
        for kind in ('T', 'W', 'V'):
            with self.subTest(kind=kind), self.assertRaisesRegex(ValueError, 'dynamic ABI'):
                build.check_symbols(dynamic + '\n00001000 ' + kind + ' unexpected', private)

    def test_no_extra_import(self):
        dynamic, private = self.symbol_fixture()
        with self.assertRaisesRegex(ValueError, 'dynamic ABI'):
            build.check_symbols(dynamic + '\n U rtc_direct_write', private)

    def test_missing_or_global_private_hook(self):
        dynamic, private = self.symbol_fixture()
        for changed in (private.split('\n', 1)[1], private.replace('LOCAL', 'GLOBAL', 1)):
            with self.assertRaisesRegex(ValueError, 'private hook'):
                build.check_symbols(dynamic, changed)

    def test_wrong_dependency_base(self):
        with patch.object(build, 'git', return_value='0' * 40):
            with self.assertRaisesRegex(ValueError, 'Exact dependency base'):
                build.validate(self.profile, self.manifest, self.roots)

    def test_compiler_dependency_must_be_frozen(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / 'new.h'
            source.write_text('unfrozen')
            profile = copy.deepcopy(self.profile)
            profile['application_source_sha256'] = {}
            with patch.object(build, 'capture', return_value='input: ' + str(source) + '\n'):
                with self.assertRaisesRegex(ValueError, 'Unfrozen compiler dependency'):
                    build.dependency_closure('cc', [], [], {'productivity': root}, profile, root / 'catalog.c')


if __name__ == '__main__':
    unittest.main()
