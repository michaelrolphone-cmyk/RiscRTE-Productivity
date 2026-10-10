#!/usr/bin/env python3
"""Rasterize exact licensed fonts for the supplied480x800 Points reference."""
import argparse,hashlib,json
from pathlib import Path
from PIL import ImageFont
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--font-dir',type=Path,required=True);a=p.parse_args()
out=ROOT/'lib/PointsCatalog/fonts'
expected={'Orbitron-wght.ttf':'f42db2dd16e642258e35782916eceb1dcdbea06fb958d77ad71dc5963587e8fd','Rajdhani-700.ttf':'691470dd3286a14e9677940d0bf75796179841ba5215cbda1a2c8910a3226afd','Rajdhani-600.ttf':'94bbd25a18ca665999feb05a537de9fd2b860dcfb78bbe9ca00270825bf235da'}
for name,digest in expected.items():assert hashlib.sha256((a.font_dir/name).read_bytes()).hexdigest()==digest
faces=[('O900_26',900,26,'Orbitron-wght.ttf'),('O700_23',700,23,'Orbitron-wght.ttf'),('O700_28',700,28,'Orbitron-wght.ttf'),('O700_22',700,22,'Orbitron-wght.ttf'),('O700_20',700,20,'Orbitron-wght.ttf'),('O700_14',700,14,'Orbitron-wght.ttf'),('O700_52',700,52,'Orbitron-wght.ttf'),('O900_88',900,88,'Orbitron-wght.ttf'),('O700_80',700,80,'Orbitron-wght.ttf'),('O900_46',900,46,'Orbitron-wght.ttf'),('O900_38',900,38,'Orbitron-wght.ttf'),('O900_30',900,30,'Orbitron-wght.ttf')]
faces += [(f'R700_{size}',700,size,'Rajdhani-700.ttf') for size in (14,16,17,18,20,21,22,23,28)]
faces += [('R600_19',600,19,'Rajdhani-600.ttf'),('R600_18',600,18,'Rajdhani-600.ttf')]
content=['/* Generated from licensed Orbitron and Rajdhani. See SOURCES.json. */','typedef struct {uint8_t width,height;int8_t left,top;uint16_t advance;const uint8_t *bits;} pc_glyph;','typedef struct {uint8_t first,second;int16_t adjust;} pc_kern;','typedef struct {const pc_glyph *glyphs;const pc_kern *kern;unsigned kern_count;} pc_font;']
manifest={'source_system_revision':'5a5cdbf92a6d307708cfa432b28caa9218c1a109','license':'SIL OFL1.1','anchor':'left-baseline','threshold':128,'fonts':[]}
for ident,weight,size,name in faces:
 charset='0123456789:' if size>=52 else ''.join(chr(c) for c in range(32,127))
 font=ImageFont.truetype(str(a.font_dir/name),size)
 if name=='Orbitron-wght.ttf':font.set_variation_by_axes([weight])
 rows=[];kern=[]
 for cp in range(32,127):
  if chr(cp) not in charset:rows.append('{0,0,0,0,0,NULL}');continue
  mask,offset=font.getmask2(chr(cp),mode='L',anchor='ls');w,h=mask.size;bits=bytearray((w*h+7)//8 or 1)
  for i,value in enumerate(mask):
   if value>=128:bits[i//8]|=128>>(i%8)
  symbol=f'pc_font_{ident}_{cp}';content.append('static const uint8_t '+symbol+'[]={'+','.join(map(str,bits))+'};')
  rows.append('{%d,%d,%d,%d,%d,%s}'%(w,h,*offset,round(font.getlength(chr(cp))*64),symbol))
 for first in charset:
  for second in charset:
   value=round((font.getlength(first+second)-font.getlength(first)-font.getlength(second))*64)
   if value:kern.append('{%d,%d,%d}'%(ord(first),ord(second),value))
 content.append('static const pc_glyph pc_glyph_'+ident+'[]={'+','.join(rows)+'};')
 content.append('static const pc_kern pc_kern_'+ident+'[]={'+(','.join(kern) if kern else '{0,0,0}')+'};')
 content.append('static const pc_font PCF_'+ident+'={pc_glyph_'+ident+',pc_kern_'+ident+','+str(len(kern))+'};')
 manifest['fonts'].append({'id':ident,'file':name,'size':size,'weight':weight,'sha256':expected[name],'characters':charset})
text='\n'.join(content)+'\n';(out/'text.inc').write_text(text);manifest['text_sha256']=hashlib.sha256(text.encode()).hexdigest();(out/'SOURCES.json').write_text(json.dumps(manifest,indent=2)+'\n')
for name in ('LICENSE-Orbitron.txt','LICENSE-Rajdhani.txt'):(out/name).write_bytes((a.font_dir/name).read_bytes())
print('Points reference glyphs:',len(text),'source bytes')
