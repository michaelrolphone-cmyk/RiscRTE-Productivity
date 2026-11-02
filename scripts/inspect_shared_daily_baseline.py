#!/usr/bin/env python3
"""Read published baseline without modifying candidates."""
import argparse,json,sys,zipfile
from pathlib import Path
p=argparse.ArgumentParser()
for n in ('system','baseline','bootfs-tools','output'):p.add_argument('--'+n,type=Path,required=True)
a=p.parse_args()
sys.path[:0]=[str(a.system.resolve()/'scripts'),str(a.bootfs_tools.resolve())]
from compose_shared_app_cohort import digest,encode,safe
from assemble_shared_home_cohort import inventory,fingerprint,providers
from assemble_shared_usb_cohort import decode_store
from read_only_spiffs import read_image
lock=json.loads((Path(__file__).resolve().parents[1]/'docs/shared-daily/baseline.json').read_text())
assert lock['repository']=='michaelrolphone-cmyk/RiscRTE-Watch'
assert lock['tag']=='bringup-shared-233e43841483adf570ae833a88d9b6fc08d68f1a'
for item in lock['assets']:
 data=(a.baseline/item['name']).read_bytes()
 assert len(data)==item['bytes'] and digest(data)==item['sha256']
result={}
for lane in ('watch','wave','x4'):
 raw=(a.baseline/f'product-{lane}-store.bin').read_bytes()
 stored=read_image(raw,len(raw));before=decode_store(stored)
 with zipfile.ZipFile(a.baseline/f'product-{lane}-sd-apps.zip') as z:
  assert len(z.namelist())==len(set(z.namelist()))
  sd={safe(n):z.read(n) for n in z.namelist()}
 record=inventory(before,sd);boot=record['boot']
 rows=[r for r in boot['app_capabilities'] if json.loads(before[r['manifest']]).get('id')=='daily-digest']
 details=[]
 for row in rows:
  m=json.loads(before[row['manifest']])
  route=row.get('image_source')
  path=route['path'] if route else str(Path(row['manifest']).parent/m['file_name'])
  blob=(sd if route else before)[safe(path)]
  details.append({'policy':row,'manifest':m,'manifest_path':row['manifest'],
   'manifest_sha256':digest(before[row['manifest']]),'route':{'storage':'sd' if route else 'internal','path':path},
   'elf_sha256':digest(blob),'elf_bytes':len(blob)})
 result[lane]={'inventory_sha256':fingerprint(record),'inventory':record,'daily':details,
  'driver_providers':[{'capability':k[0],'api':k[1],'instance_id':k[2],'manifest':v} for k,v in sorted(providers(before,boot).items())],
  'stored_files':{n:digest(b) for n,b in sorted(stored.items())}}
 print(lane+' DAILY '+json.dumps(details),flush=True)
 print(lane+' BOOT '+json.dumps(boot),flush=True)
 print(lane+' DAILY_SD '+json.dumps({n:{'sha256':digest(b),'bytes':len(b),'text':b.decode('utf-8') if n.endswith('.json') else None} for n,b in sd.items() if n.startswith('Apps/daily-digest/')}),flush=True)
 print(lane+' SUMMARY '+json.dumps({'inventory_sha256':result[lane]['inventory_sha256'],'driver_providers':result[lane]['driver_providers']}),flush=True)
a.output.write_bytes(encode(result))
