import copy
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
import build_lists_native_qualification as build
import build_lists_scene

class Qualification(unittest.TestCase):
    def setUp(self):self.profile=json.loads(build.PROFILE.read_text())
    def valid_git(self,path,*args):
        part='runtime' if str(path)=='runtime' else 'system'
        return self.profile[part]['commit'] if args==('rev-parse','HEAD') else self.profile[part]['tree'] if args==('rev-parse','HEAD^{tree}') else ''
    def validate(self,profile):
        with patch.object(build,'git',side_effect=self.valid_git):return build.validate(Path('runtime'),Path('system'),profile)
    def test_current_exact_profile(self):self.assertIn('system',self.validate(self.profile))
    def test_wrong_commit_tree_and_dirty_checkout(self):
        for rejected in ('b'*40,' M Services/scene_host/components.inc'):
            with patch.object(build,'git',return_value=rejected),self.assertRaises(ValueError):build.validate(Path('runtime'),Path('system'),self.profile)
        def bad_tree(path,*args):return 'b'*40 if args==('rev-parse','HEAD^{tree}') else self.valid_git(path,*args)
        with patch.object(build,'git',side_effect=bad_tree),self.assertRaises(ValueError):build.validate(Path('runtime'),Path('system'),self.profile)
        def dirty(path,*args):return ' M source' if args[0]=='status' else self.valid_git(path,*args)
        with patch.object(build,'git',side_effect=dirty),self.assertRaises(ValueError):build.validate(Path('runtime'),Path('system'),self.profile)
    def test_scope_profile_and_closure_mutations(self):
        for key,value in (('schema',2),('id','product-release'),('host_profile','watch-compact'),('sizes',['240x240']),('capabilities',['ui.text-input@1']),('compiler','gcc8'),('preserved_source_sha256',{})):
            with self.subTest(key=key),self.assertRaises(ValueError):p=copy.deepcopy(self.profile);p[key]=value;self.validate(p)
    def test_preserved_source_hash_rejects_changes(self):
        p=copy.deepcopy(self.profile);p['preserved_source_sha256']['Apps/ListsScene.c']='0'*64
        with self.assertRaisesRegex(ValueError,'Preserved source changed'):self.validate(p)
    def test_builder_loads_selected_sibling_without_pythonpath(self):
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp);(path/'lists_unique_toolchain.py').write_text('VALUE=17\n');(path/'builder.py').write_text('from lists_unique_toolchain import VALUE\n')
            old=list(sys.path)
            try:self.assertEqual(build_lists_scene.load('lists_import_test',path/'builder.py').VALUE,17)
            finally:sys.modules.pop('lists_unique_toolchain',None)
            self.assertEqual(sys.path,old)
    def test_failed_builder_import_restores_path(self):
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp)/'builder.py';path.write_text('raise ValueError("test")\n');old=list(sys.path)
            with self.assertRaises(ValueError):build_lists_scene.load('lists_import_fail',path)
            self.assertEqual(sys.path,old)

if __name__=='__main__':unittest.main()
