#!/usr/bin/env python3
"""Measure lossless bounded-block trie storage; no source or candidate modification."""
import argparse,ctypes,hashlib,json,re,zlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--upstream',type=Path,required=True);a=p.parse_args()
lib=ctypes.CDLL('liblz4.so.1');lib.LZ4_compressBound.argtypes=[ctypes.c_int];lib.LZ4_compressBound.restype=ctypes.c_int
lib.LZ4_compress_HC.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_int,ctypes.c_int,ctypes.c_int];lib.LZ4_compress_HC.restype=ctypes.c_int
def hc(data):
 src=ctypes.create_string_buffer(data);cap=lib.LZ4_compressBound(len(data));out=ctypes.create_string_buffer(cap)
 return lib.LZ4_compress_HC(src,out,len(data),cap,12)
rows=[]
for path in sorted((a.upstream/'lib/Epub/Epub/hyphenation/generated').glob('hyph-*.trie.h')):
 text=path.read_text();m=re.search(r'constexpr uint8_t (\w+)_trie_data\[\] = \{(.*?)\};',text,re.S);assert m
 raw=bytes(int(x,16) for x in re.findall(r'0x([0-9a-fA-F]{2})\b',m[2]))
 row={'language':m[1],'bytes':len(raw),'sha256':hashlib.sha256(raw).hexdigest(),'lz4_original':hc(raw),'blocks':{}}
 for size in (1024,2048,4096,8192,16384):
  chunks=[]
  for pos in range(0,len(raw),size):
   original=raw[pos:pos+size];c=zlib.compressobj(9,zlib.DEFLATED,-15);data=c.compress(original)+c.flush()
   assert zlib.decompress(data,-15)==original;chunks.append(data)
  # Conservative 12-byte metadata per block, before accounting for added decoder.
  row['blocks'][str(size)]={'compressed':sum(map(len,chunks)),'blocks':len(chunks),'outer_hc_with_metadata':hc(b''.join(chunks))+12*len(chunks)}
 rows.append(row);print('TRIE OPTION '+json.dumps(row),flush=True)
assert {r['language'] for r in rows}==set('de en es fi fr it pl pt ru sv uk'.split())
print('TRIE TOTAL '+json.dumps({str(size):sum(r['lz4_original']-r['blocks'][str(size)]['outer_hc_with_metadata'] for r in rows) for size in (1024,2048,4096,8192,16384)}),flush=True)
