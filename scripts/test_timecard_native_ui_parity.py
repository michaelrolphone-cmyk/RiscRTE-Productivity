#!/usr/bin/env python3
"""Exact selector-off tokens and actual Timecard paper RTC/UTC opt-in parity."""
import argparse, hashlib, json, os, re, subprocess
from pathlib import Path
from build_timecard_native_ui import stage_headers, UTC_SOURCES
ROOT=Path(__file__).resolve().parents[1];BASE='fd59829e841ac45354243cbdf2de19881071ac3e'
p=argparse.ArgumentParser(description=__doc__)
for name in ('system-apps','utilities','runtime'):p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--output',type=Path,default=ROOT/'build/timecard-native-ui-parity');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
roots={'productivity':ROOT,'system':a.system_apps.resolve(),'utilities':a.utilities.resolve(),'runtime':a.runtime.resolve()};system=roots['system'];incs=stage_headers(out,roots)+[ROOT/'Apps',ROOT/'lib/NativeApps/include',ROOT/'lib/PortableTimecard/include']
base=out/'timecard_portable.c';base.write_bytes(subprocess.check_output(['git','show',BASE+':Apps/timecard_portable.c'],cwd=ROOT));cc=os.environ.get('CC','cc');rows=[]
for paper in (False,True):
 for appdata in (False,True):
  for alarm in (False,True):
   for nav in (False,True):
    flags=['PORTABLE_NOVA_UI','PORTABLE_APP_OWNS_TOUCH_CHROME']+(['TIMECARD_PAPER'] if paper else [])+(['TIMECARD_APP_DATA'] if appdata else [])+(['PORTABLE_ALARM_CLIENT'] if alarm else [])+(['PORTABLE_INPUT_NAVIGATION'] if nav else [])
    values=[]
    for source in (base,ROOT/'Apps/timecard_portable.c'):
     text=subprocess.check_output([cc,'-E','-P',*['-D'+x for x in flags],*['-I'+str(x) for x in incs],str(source)],text=True)
     values.append('\n'.join(re.findall(r'''"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|[A-Za-z_0-9]+|[^\s]''',text)))
    assert values[0]==values[1],flags;assert 'portable_timecard_native_' not in values[1];rows.append({'defines':flags,'sha256':hashlib.sha256(values[0].encode()).hexdigest()})
# Preserve the historical fixture on disk. Its old anonymous grants predate the
# canonical slot/generation contract; stage only those two provider boundaries.
staged=out/'rtc-fixture';(staged/'test/native_apps').mkdir(parents=True,exist_ok=True)
(staged/'Apps').symlink_to(ROOT/'Apps',target_is_directory=True)
fixture=(ROOT/'test/native_apps/timecard_paper_fixture.h').read_text()
for old,new in [('grants++;return true;}', 'grants++;g->slot=grants;g->generation=1;return true;}'),('g->api=NULL;grants--;return true;}', '*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};grants--;return true;}')]:
 assert fixture.count(old)==1;fixture=fixture.replace(old,new)
# Upgrade this derived fixture's snapshot-only touch generator to the current
# ordered provider contract. The original fixture and all app assertions stay intact.
start=fixture.index('static bool fake_touch_poll(');end=fixture.index('static const risc_touch_api_v1 touch_api=',start)
fixture=fixture[:start]+r"""
static risc_touch_snapshot_v1 touch_state={.width=480,.height=800};
static risc_touch_event_v1 touch_events[32];static unsigned touch_head,touch_count;static bool touch_gap;
static void touch_emit(unsigned kind,unsigned id,int x,int y){assert(touch_count<32);touch_events[(touch_head+touch_count++)%32]=(risc_touch_event_v1){.sequence=++touch_state.sequence,.timestamp_ms=ticks,.kind=kind,.id=id,.x=x,.y=y};}
static bool fake_touch_poll(void*c,size_t n){(void)c;assert(n==1);polls++;bool down=false;int x=0,y=0;
 for(unsigned i=0;i<action_count;i++)if(actions[i].at==polls){down=true;x=actions[i].x;y=actions[i].y;}
 bool before=touch_state.contact_count!=0;
 if(before&&!down)touch_emit(RISC_TOUCH_EVENT_UP,1,touch_state.contacts[0].x,touch_state.contacts[0].y);
 else if(!before&&down)touch_emit(RISC_TOUCH_EVENT_DOWN,1,x,y);
 else if(before&&down&&(x!=touch_state.contacts[0].x||y!=touch_state.contacts[0].y))touch_emit(RISC_TOUCH_EVENT_MOVE,1,x,y);
 bool button=polls==primary_at;if(button!=(touch_state.buttons!=0))touch_emit(button?RISC_TOUCH_EVENT_BUTTON_DOWN:RISC_TOUCH_EVENT_BUTTON_UP,0,0,0);
 touch_state.contact_count=down?1:0;touch_state.contacts[0]=(risc_touch_contact_v1){.id=1,.x=x,.y=y};touch_state.buttons=button?RISC_TOUCH_BUTTON_PRIMARY:0;touch_state.timestamp_ms=ticks;
 if(lost_at&&polls==lost_at){touch_count=0;touch_head=0;touch_gap=true;}return true;}
static int32_t fake_next(void*c,uint64_t n,risc_touch_event_v1*e){(void)c;assert(n==1);if(touch_gap){touch_gap=false;return -1;}if(!touch_count)return 0;*e=touch_events[touch_head];touch_head=(touch_head+1)%32;touch_count--;return 1;}
static bool fake_snapshot(void*c,risc_touch_snapshot_v1*s){(void)c;*s=touch_state;return true;}
"""+fixture[end:]
(staged/'test/native_apps/timecard_paper_fixture.h').write_text(fixture)
(staged/'test/native_apps/timecard_paper_test.c').write_bytes((ROOT/'test/native_apps/timecard_paper_test.c').read_bytes())
utc_fixture=(ROOT/'test/native_apps/timecard_native_time_test.c').read_text()
start=utc_fixture.index('static bool fx_touch_poll(');end=utc_fixture.index('static const risc_touch_api_v1 fx_touch=',start)
utc_fixture=utc_fixture[:start]+r"""
static risc_touch_snapshot_v1 touch_state={.width=480,.height=800};
static risc_touch_event_v1 touch_events[32];static unsigned touch_head,touch_count;
static void touch_emit(unsigned kind,int x,int y){assert(touch_count<32);touch_events[(touch_head+touch_count++)%32]=(risc_touch_event_v1){.sequence=++touch_state.sequence,.timestamp_ms=ticks,.kind=kind,.id=1,.x=x,.y=y};}
static bool fx_touch_poll(void*c,size_t n){(void)c;fx_io();assert(n==1);touch_polls++;
 bool down=quick_test&&(touch_polls==5||touch_polls==6),before=touch_state.contact_count!=0;int y=touch_polls==5?20:100;
 if(before&&!down)touch_emit(RISC_TOUCH_EVENT_UP,200,touch_state.contacts[0].y);
 else if(!before&&down)touch_emit(RISC_TOUCH_EVENT_DOWN,200,y);
 else if(before&&down&&y!=touch_state.contacts[0].y)touch_emit(RISC_TOUCH_EVENT_MOVE,200,y);
 touch_state.contact_count=down?1:0;touch_state.contacts[0]=(risc_touch_contact_v1){.id=1,.x=200,.y=y};touch_state.timestamp_ms=ticks;return true;}
static int32_t fx_next(void*c,uint64_t n,risc_touch_event_v1*e){(void)c;fx_io();assert(n==1);if(!touch_count)return 0;*e=touch_events[touch_head];touch_head=(touch_head+1)%32;touch_count--;return 1;}
static bool fx_snapshot(void*c,risc_touch_snapshot_v1*s){(void)c;fx_io();*s=touch_state;return true;}
"""+utc_fixture[end:]
(staged/'test/native_apps/timecard_native_time_test.c').write_text(utc_fixture)
common=['PORTABLE_NOVA_UI','PORTABLE_APP_OWNS_TOUCH_CHROME','TIMECARD_PAPER','PORTABLE_QUICK_ACTIONS','PORTABLE_ALARM_CLIENT','PORTABLE_INPUT_NAVIGATION','PORTABLE_HOME_APP="default.elf"']
receipts=[];comparisons={}
for utc in (False,True):
 for landscape in ((False,) if utc else (False,True)):
  for enabled in (False,True):
   for sanitized in (False,True):
    name=f'paper-{int(utc)}-{int(landscape)}-{int(enabled)}-{int(sanitized)}';folder=out/name;folder.mkdir(exist_ok=True)
    flags=common+(['TIMECARD_APP_DATA','TIMECARD_NATIVE_TIME','ALARM_NATIVE_UTC','ALARM_SERVICE_TAGGED_V2','PORTABLE_NATIVE_TIME_TOOLBAR','PORTABLE_NATIVE_CUSTODY_FENCE'] if utc else ['PORTABLE_RTC_WALL_TIME'])
    if utc or landscape:flags+=['PORTABLE_DISPLAY_ROTATION=90']
    if landscape:flags+=['TEST_NATIVE_LANDSCAPE']
    if enabled:flags+=['PORTABLE_TIMECARD_NATIVE_UI','PORTABLE_NATIVE_CANVAS']
    san=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
    selected_incs=[*incs,ROOT/'test/native_apps'] if utc else [system/'Apps',system/'lib/NativeApps/include',ROOT/'lib/NativeApps/include',system/'lib/PortableApps/include']
    sources=[staged/'test/native_apps'/('timecard_native_time_test.c' if utc else 'timecard_paper_test.c'),*[system/'lib/PortableApps/src'/x for x in ('adapter.c','quick_actions.c','quick_render.c','quick_session.c',*(UTC_SOURCES if utc else ()))]]
    if not utc:
     catalog=folder/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n');sources.append(catalog)
    exe=folder/'test';cmd=[cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*(['-Wno-missing-field-initializers'] if not utc else []),*san,*['-D'+x for x in flags],*['-I'+str(x) for x in selected_incs],*map(str,sources),'-o',str(exe)]
    (folder/'command.json').write_text(json.dumps(cmd,indent=2)+'\n');subprocess.run(cmd,check=True)
    hashes={};logs=[]
    for scenario in range(33 if utc else 18):
     frames=folder/f'frames-{scenario}';frames.mkdir(exist_ok=True)
     result=subprocess.run([str(exe),str(scenario),*([str(frames)] if not utc else [])],capture_output=True,text=True,timeout=60,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1'})
     (folder/f'{scenario}.log').write_text(result.stdout+result.stderr);result.check_returncode();logs.append(hashlib.sha256(result.stdout.encode()).hexdigest())
     for f in frames.iterdir():hashes[f'{scenario}/{f.name}']=hashlib.sha256(f.read_bytes()).hexdigest()
    identity=(utc,landscape);value={'frames':hashes,'logs':logs};assert identity not in comparisons or comparisons[identity]==value,name;comparisons[identity]=value
    receipts.append({'composition':name,'scenarios':len(logs),**value});print(f'PASS {name}: {len(logs)} actual paper scenarios, {len(hashes)} retained frame hashes',flush=True)
(out/'results.json').write_text(json.dumps({'base':BASE,'selector_off':rows,'paper_compositions':receipts,'hardware_verified':False},indent=2)+'\n')
print('PASS Timecard feature-off tokens and actual RTC portrait/landscape + UTC paper selector-on/off normal/sanitized parity')
