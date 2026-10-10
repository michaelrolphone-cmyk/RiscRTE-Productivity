#!/usr/bin/env python3
"""Timecard0.2.12 shared-adapter successor build using verified installed source dependencies."""
import argparse,json,hashlib,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for name in ('baseline-custody','display-sdk','system','runtime','utilities','output'):p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--raster-snapshot',action='store_true')
a=p.parse_args()
if a.output.exists():raise FileExistsError(a.output)
r=json.loads(a.baseline_custody.read_text())['build_receipts']['timecard'];assert r['version']=='0.2.11'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
checked={}
for name,digest in r['compiled_dependencies_sha256'].items():
 if name.startswith('Source/'):
  path=ROOT/name[7:];assert sha(path)==digest,name;checked[name[7:]]=digest
assert len(checked)==10
helper_sha=sha(a.utilities/'scripts/resident_client_build.py')
assert helper_sha in {'1f7209b1f74d8f706b7b5f73be9aaf52061ce5679bbb99dea79f8aa7bd0779de','d8be689c466ec52b11b97710af3b3758d489865029b764f57578293b6ee7bfb1'}
if a.raster_snapshot:assert helper_sha=='d8be689c466ec52b11b97710af3b3758d489865029b764f57578293b6ee7bfb1'
sys.path.insert(0,str(a.utilities.resolve()/'scripts'));import resident_client_build as resident
resident.RUNTIME=resident.git(a.runtime,'rev-parse','HEAD')
options=argparse.Namespace(system_apps=a.system,system_revision=resident.git(a.system,'rev-parse','HEAD'),runtime=a.runtime,output=a.output,display_sdk=a.display_sdk,development_system=False,raster_snapshot=a.raster_snapshot,runtime_revision=resident.git(a.runtime,'rev-parse','HEAD'))
c=resident.prepare(options,p,a.utilities.resolve())
record=resident.build(c,ROOT,'timecard','0.2.12',r['build_defines'],[ROOT/'Apps/timecard_portable.c'],r['required_grants'],r['features'])
assert [x for x in record['build_defines'] if x!='-DPORTABLE_RASTER_SNAPSHOT']==r['build_defines'] and record['required_grants']==r['required_grants'] and record['requires']==r['requires']
record['recovery_custody']=dict(baseline_source=r['source_revision'],verified_application_sources=checked,baseline_custody_sha256=sha(a.baseline_custody),clean_build=True,prior_product_binary_inputs=[])
record['raster_snapshot']=a.raster_snapshot
assert ('-DPORTABLE_RASTER_SNAPSHOT' in record['build_defines'])==a.raster_snapshot
record['selected_build_helper_sha256']=sha(Path(__file__))
resident.write(c['out']/'timecard/x4-native-app.json',record)
print('Timecard0.2.12 source dependencies byte-verified; fresh target/profile/loader PASS')
