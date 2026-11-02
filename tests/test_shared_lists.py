#!/usr/bin/env python3
"""Reject unsafe inputs and deliberate executable changes."""
import io,json,sys,unittest,zipfile
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
try:
 import package_shared_lists as p
 from elftools.elf.elffile import ELFFile
except ModuleNotFoundError:
 p=None
@unittest.skipIf(p is None,'pyelftools is installed by the owning shared Lists workflow')
class Packaging(unittest.TestCase):
 def test_paths(self):
  for n in ('../escape','/absolute','a/../b','a\\b',''):
   with self.assertRaises(ValueError):p.safe(n)
 def test_duplicate_zip(self):
  b=io.BytesIO()
  with zipfile.ZipFile(b,'w') as z:z.writestr('same',b'1');z.writestr('same',b'2')
  with self.assertRaises(ValueError):p.unzip(io.BytesIO(b.getvalue()))
 def test_versions(self):
  base={'boot.json':json.dumps({'app_capabilities':[{'manifest':'lists.json'}]}).encode(),'lists.elf':b'app','lists.json':json.dumps({'id':'lists','file_name':'lists.elf','version':'0.1.2'}).encode()}
  self.assertEqual(p.locate(base,{})[0],b'app')
  with self.assertRaises(ValueError):p.locate(base,{},p.VERSION)
 def test_unknown_elf(self):
  with self.assertRaises(Exception):p.loaded(b'not elf')
 def test_actual_corruption(self):
  path=Path(__file__).resolve().parents[1]/'qualification/products/lists.elf'
  if not path.exists():self.skipTest('actual product supplied by owning workflow')
  data=path.read_bytes();e=ELFFile(io.BytesIO(data));s=next(s for s in e.iter_sections() if s['sh_flags']&2 and s['sh_type']=='SHT_PROGBITS' and s['sh_size'])
  b=bytearray(data);b[s['sh_offset']]^=1
  self.assertNotEqual(p.loaded(data),p.loaded(bytes(b)))
  b=bytearray(data);b[24]^=1
  self.assertNotEqual(p.loaded(data),p.loaded(bytes(b)))
if __name__=='__main__':unittest.main()
