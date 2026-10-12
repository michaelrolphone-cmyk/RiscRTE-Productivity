#!/usr/bin/env python3
"""Build and validate the same declarative Lists application for Watch and X4."""
import argparse,hashlib,importlib.util,json,os,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def load(name,path):
 spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m

def stage(runtime,system,output,shared_input=False):
 sdk=load('scene_sdk',system/'scripts/scene_sdk.py');include=sdk.stage_sdk(runtime,system,output)
 if shared_input:
  # Preserve the exact selected Lists resident bridge; newer SDK wrappers are
  # not part of the text-entry migration. Source/hash are recorded beside it.
  frozen=ROOT/'sdk/lists-selected-profile/RiscSceneResidentV1.h'
  if hashlib.sha256(frozen.read_bytes()).hexdigest()!='a657fcd906217fdbb03894c15b10009b352d6a5b7466868afa603a03236b8851':
   raise ValueError('Selected Lists resident bridge changed')
  (include/frozen.name).write_bytes(frozen.read_bytes())
  for name in ('PortableTextInputClient.h','PortableTextInputHandoff.h','PortableTextInputCheckpointClient.h','PortableTextInputResidentCheckpoint.h','PortableResidentPolicyInput.h'):
   (include/name).write_bytes((system/'lib/PortableApps/include'/name).read_bytes())
  (include/'RiscTextEntryV1.h').write_bytes((system/'sdk/app/RiscTextEntryV1.h').read_bytes())
 return include

def run(runtime,system,output,shared_text_input=False):
 if output.exists():raise FileExistsError('Choose a new output directory')
 output.mkdir(parents=True)
 builder=load('scene_build',system/'scripts/scene_build.py')
 with tempfile.TemporaryDirectory(prefix='lists-sdk-') as tmp:
  include=stage(runtime,system,Path(tmp),shared_text_input);manifest=json.loads((ROOT/'Apps/lists_scene.json').read_text())
  if shared_text_input:
   manifest['version']='0.1.2'
   manifest['requires'].append({'capability':'ui.text-input','api':1})
  defines=['-DPORTABLE_LISTS_SHARED_TEXT_INPUT'] if shared_text_input else []
  sources=[ROOT/p for p in ('Apps/lists_scene.c','Apps/ListsScene.c','lib/Lists/ListsModel.c','lib/Lists/ListsStore.c')]
  compiler=builder.compiler_path();receipt=builder.build(compiler,include,output/'lists',manifest,sources,defines=defines,exports={'app_main','risc_resident_app_descriptor_v1'})
  repeat=builder.build(compiler,include,Path(tmp)/'repeat',manifest,sources,defines=defines,exports={'app_main','risc_resident_app_descriptor_v1'})
  if receipt['sha256']!=repeat['sha256']:raise ValueError('App must be byte-identical for both targets')
  validator=Path(tmp)/'validate'
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(system/'test/native_apps/stubs'),'-I'+str(system/'lib/elf_loader/include'),str(system/'lib/elf_loader/src/esp_elf_validate.c'),str(system/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
  subprocess.run([str(validator),str(output/'lists/lists.elf')],check=True)
  builder.json_write(output/'packages.json',{'schema':1,'packages':[receipt],'same_application_for_both_profiles':True,'physical_testing':'not performed'})
 print('Lists Xtensa ELF validated and reproducible for both profiles.')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('runtime','system-apps','output'):p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--shared-text-input',action='store_true')
 a=p.parse_args();run(a.runtime.resolve(),a.system_apps.resolve(),a.output.resolve(),a.shared_text_input)
