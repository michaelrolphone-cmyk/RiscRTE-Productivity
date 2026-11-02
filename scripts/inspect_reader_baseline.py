#!/usr/bin/env python3
"""Read exact published cumulative product inventories without mutating them."""
import argparse,json,sys,urllib.request,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser()
for n in ('system','bootfs-tools','output'):p.add_argument('--'+n,type=Path,required=True)
a=p.parse_args();sys.path[:0]=[str(a.system.resolve()/'scripts'),str(a.bootfs_tools.resolve())]
from compose_shared_app_cohort import digest,safe,write_tree,encode
from assemble_shared_usb_cohort import decode_store
from read_only_spiffs import read_image
lock=json.loads((ROOT/'reader/shared-baseline.json').read_text())
a.output.mkdir(parents=True,exist_ok=False)
for asset in lock['assets']:
 with urllib.request.urlopen(asset['url'],timeout=180) as response:data=response.read()
 assert len(data)==asset['bytes'] and digest(data)==asset['sha256'],'Published asset differs'
 (a.output/asset['name']).write_bytes(data)
for lane in ('watch','wave','x4'):
 raw=(a.output/f'product-{lane}-store.bin').read_bytes()
 stored=read_image(raw,len(raw));before=decode_store(stored)
 with zipfile.ZipFile(a.output/f'product-{lane}-sd-apps.zip') as z:
  assert len(z.namelist())==len(set(z.namelist()))
  sd={safe(n):z.read(n) for n in z.namelist()}
 for name,entries in [('before',before),('before-stored',stored),('before-sd',sd)]:write_tree(a.output/lane/name,entries)
 boot=json.loads(before['boot.json'])
 manifests={n:json.loads(v) for n,v in before.items() if n.endswith('.json') and n!='boot.json'}
 report={'boot':boot,'manifests':manifests,'sd_files':{n:{'bytes':len(b),'sha256':digest(b)} for n,b in sd.items()}}
 (a.output/lane/'inventory.json').write_bytes(encode(report))
 print('READER_BASELINE '+lane+' '+json.dumps(report,sort_keys=True),flush=True)
