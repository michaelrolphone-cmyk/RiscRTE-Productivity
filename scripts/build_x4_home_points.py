#!/usr/bin/env python3
"""Clean-build Points0.6.14 from recovered source and the frozen .6.12 profile.
Old target bytes are verification evidence only; no product ELF is copied.
"""
import argparse,hashlib,json,os,shutil,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for name in ('baseline-target','baseline-sdk','system','runtime','utilities','output','reservation'):p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--common-system',action='store_true',help='Stage the complete selected clean System and Runtime SDK for the recovered source union')
a=p.parse_args();out=a.output.resolve()
if out.exists():raise FileExistsError(out)
sha=lambda q:hashlib.sha256(Path(q).read_bytes()).hexdigest()
base=json.loads((a.baseline_target/'x4-native-app.json').read_text())
assert sha(a.baseline_target/'x4-native-app.json')=='4db91aeec690e6b4168b553f75597b97e080b77c05f9f3b7794abacebf7648f2'
assert base['version']=='0.6.12'
assert json.loads(a.reservation.read_text())['Points']=='0.6.14'
assert sha(a.utilities/'scripts/resident_client_build.py')=='1f7209b1f74d8f706b7b5f73be9aaf52061ce5679bbb99dea79f8aa7bd0779de'
# Verify every frozen SDK header before changing only the two negotiated text
# headers and optional physical-Home callback declaration from selected source.
for name,digest in base['sdk_sha256'].items():assert sha(a.baseline_sdk/'include'/name)==digest,name
cc=os.environ['NATIVE_APP_CC'];version=subprocess.check_output([cc,'--version'],text=True).splitlines()[0]
assert '8.4.0' in version and '2021r2-patch5' in version
sys.path.insert(0,str(a.utilities.resolve()/'scripts'));import resident_client_build as resident
if a.common_system:
 resident.RUNTIME=resident.git(a.runtime,'rev-parse','HEAD')
 options=argparse.Namespace(system_apps=a.system,system_revision=resident.git(a.system,'rev-parse','HEAD'),runtime=a.runtime,output=out,display_sdk=a.baseline_sdk/'include',development_system=False)
 c=resident.prepare(options,p,a.utilities.resolve());inc=c['inc']
else:
 out.mkdir(parents=True);shutil.copytree(a.baseline_sdk,out/'sdk');inc=out/'sdk/include'
 for name in ('RiscTextEntryV1.h','PortableTextInputClient.h','PortableAppLaunchGuard.h'):
  shutil.copyfile(a.system/'lib/PortableApps/include'/name,inc/name)
 resident.RUNTIME='615fb236b591bc6974a35ae23c7b2b785c0a5016'
 c=dict(system=a.system.resolve(),runtime=a.runtime.resolve(),out=out,inc=inc,flags=[],receipt=base['resident_shell'],cc=cc,compiler=version,utilities=a.utilities.resolve())
defines=base['build_defines']+['-DPORTABLE_APP_HOME_GUARD']
record=resident.build(c,ROOT,'points_in_time','0.6.14',defines,[ROOT/'Apps/points_catalog_app.c'],base['required_grants'],base['features'])
manifest=json.loads((out/'points_in_time/points_in_time.json').read_text());assert manifest==json.loads((ROOT/'Apps/native/points_catalog_x4_resident.json').read_text())
assert record['requires']==base['requires'] and record['required_grants']==base['required_grants']
record['recovery_custody']=dict(baseline_source='53e0a82bca754a3d302c2cefd26cb596a8734bab',baseline_tree='d78de5c647212f34694987a9fedeb0c6d5a14de1',baseline_receipt_sha256=sha(a.baseline_target/'x4-native-app.json'),baseline_sdk_headers_verified=len(base['sdk_sha256']),changed_sdk_headers=[name for name,digest in record['sdk_sha256'].items() if base['sdk_sha256'].get(name)!=digest],added_defines=['-DPORTABLE_APP_HOME_GUARD'],added_grants=[],native_product_binary_reused=False,baseline_elf_used_as_input=False)
record['recovery_custody']['common_system']=a.common_system
if not a.common_system:assert set(record['recovery_custody']['changed_sdk_headers'])=={'RiscTextEntryV1.h','PortableTextInputClient.h','PortableAppLaunchGuard.h'}
record['selected_build_helper_sha256']=sha(Path(__file__));record['reservation_sha256']=sha(a.reservation)
record['stack_frames']=[]
for path in (out/'points_in_time').glob('*.su'):
 for line in path.read_text().splitlines():
  f=line.split('\t')
  if len(f)>=3:record['stack_frames'].append(dict(function=f[0],bytes=int(f[1]),kind=f[2]))
assert record['stack_frames'] and max(x['bytes'] for x in record['stack_frames'])<16384
resident.write(out/'points_in_time/x4-native-app.json',record)
print('Clean source build complete; full product integration and hardware validation remain separate')
