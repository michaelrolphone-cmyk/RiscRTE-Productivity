"""Share only byte-identical immutable fallback arrays; never reduce glyphs/styles."""
import hashlib,json,re,subprocess
from pathlib import Path

def deduplicate(upstream):
 root=Path(upstream)/'lib/EpdFont/builtinFonts'
 paths=[root/('notoserif_14_'+s+'.h') for s in ('regular','bold','italic','bolditalic')]
 sources={p:p.read_text() for p in paths}
 pattern=re.compile(r'static const (\w+) (\w+)\[[0-9]*\] = \{(.*?)\n\};',re.S)
 groups={}
 def normalized(body):
  return re.sub(r'\s+','',re.sub(r'//[^\n]*|/\*.*?\*/','',body,flags=re.S))
 for path,text in sources.items():
  for m in pattern.finditer(text):
   key=(m[1],normalized(m[3]))
   groups.setdefault(key,[]).append((path,m))
 common=['#pragma once','#include "EpdFontData.h"','// Generated exact-value sharing, not a reduced font.']
 proof=['#include <cassert>','#include <cstring>']
 proof += ['#include "builtinFonts/'+p.name+'"' for p in paths]
 checks=[];replacements={};rows=[]
 for key,items in groups.items():
  if len(items)<2:continue
  ident='reader_shared_metadata_'+str(len(rows));first=items[0][1]
  # Numeric-only metadata cannot reference style-specific addresses.
  assert not re.search(r'[G-Zg-z_]',re.sub(r'0[xX][0-9a-fA-F]+','',key[1]))
  common.append('static const '+first[1]+' '+ident+'[] = {'+first[3]+'\n};')
  original='reader_original_metadata_'+str(len(rows))
  proof.append('static const '+first[1]+' '+original+'[] = {'+first[3]+'\n};')
  names=[]
  for path,m in items:
   assert (m[1],normalized(m[3]))==key
   replacements[m[2]]='#define '+m[2]+' '+ident
   names.append(m[2]);checks.append('static_assert(sizeof('+m[2]+')==sizeof('+original+'));assert(!std::memcmp('+m[2]+','+original+',sizeof('+original+')));')
  rows.append({'aliases':names,'type':key[0],'normalized_initializer_sha256':hashlib.sha256(key[1].encode()).hexdigest()})
 assert rows,'No exact duplicate metadata found'
 (root/'reader_shared_metadata.h').write_text('\n'.join(common)+'\n')
 for path,text in sources.items():
  changed=pattern.sub(lambda m:replacements.get(m[2],m[0]),text)
  path.write_text(changed.replace('#include "EpdFontData.h"','#include "EpdFontData.h"\n#include "reader_shared_metadata.h"'))
 proof+=['int main(){'+''.join(checks)+'}']
 source=Path(upstream)/'font-sharing-proof.cpp';source.write_text('\n'.join(proof)+'\n')
 binary=Path(upstream)/'font-sharing-proof'
 subprocess.run(['g++','-std=c++17','-I'+str(root.parent),str(source),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
 record={'method':'Exact immutable metadata array aliases; all original bitmaps, glyph values, styles and font ABI retained','native_byte_equality_verified':True,'groups':rows,
  'original_header_sha256':{p.name:hashlib.sha256(text.encode()).hexdigest() for p,text in sources.items()},
  'generated_header_sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}}
 (Path(upstream)/'font-sharing.json').write_text(json.dumps(record,indent=2)+'\n')
 print('FONT SHARING '+json.dumps(record),flush=True)
 return record
