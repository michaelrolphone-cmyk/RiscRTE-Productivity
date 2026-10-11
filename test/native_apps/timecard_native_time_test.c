/* Actual Timecard controller, versioned storage, shared native source and adapter. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../Apps/timecard_portable.c"
#include "PortableApps.h"
#include "RiscDisplayOutputV1.h"
#include "RiscTouchV1.h"
#include "RiscBatteryGaugeV1.h"
#include "RiscInputNavigationV1.h"
#include "RiscRealtimeV1.h"
#include "PortableTimeZonePreference.h"
int app_module_init(void);void app_module_fini(void);
const t5_app_manifest_t portable_catalog[1]={{.compatible=false}};
const unsigned portable_catalog_count=0;
static unsigned calls,live,frames,subs,presents,native_live,native_samples,puts_count,ticks,barriers;
static bool retained,release_fail,unset,home_requested;
static unsigned home_polls,launches,home_depth,touch_polls;
static bool quick_test;
static bool back_requested,release_dirty,malformed;
static char last_launch[32];
static int64_t epoch=1791115200;
static int stat_result,read_result,replace_result;
static bool data_present,commit_error;
static uint64_t data_revision=1;
static char history[JSON_CAPACITY];
static unsigned data_writes;
static const void *fail_release_api;
static const char *fail_acquire_name;
static int native_result,kv_result,alarm_result;
static uint8_t pixels[48000];
#ifdef TEST_PRODUCTIVITY_SCROLL_FIXTURE
#include "productivity_scroll_fixture.h"
#endif
#ifdef TEST_PAPER_SHEET_MOTION
#include "paper_sheet_fixture.h"
#endif
static struct {unsigned instance;char key[16];uint8_t bytes[64];uint32_t size;} cells[16];
static void fx_io(void){assert(!retained);calls++;}
#include "native_broadcast_fixture.h"
#ifdef TEST_PRODUCTIVITY_IDLE_FIXTURE
#include "productivity_idle_fixture.h"
#endif
static bool fx_health(risc_runtime_health_v1 *s){fx_io();s->uptime_ms=ticks;return true;}
static void fx_yield(uint32_t n){fx_io();ticks+=n;}
static bool fx_diag(const char *s){fx_io();(void)s;
#ifdef TEST_PAPER_SHEET_MOTION
 motion_diagnostic(s);
#endif
 return true;}
static bool fx_launch(const char *s){fx_io();assert(!strcmp(s,"springboard.elf")||!strcmp(s,"default.elf"));launches++;strcpy(last_launch,s);return true;}
static bool fx_retain(void){assert(!retained);retained=true;barriers++;return true;}
static bool fx_info(void *c,risc_display_info_v1 *s){(void)c;fx_io();*s=(risc_display_info_v1){.width=800,.height=480,.supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_MONO1),.flags=
#ifdef TEST_PRODUCTIVITY_SCROLL_FIXTURE
 (sc_enabled?RISC_DISPLAY_INFO_ASYNC_PRESENT:0)|
#endif
 RISC_DISPLAY_INFO_RETAINS_IMAGE|RISC_DISPLAY_INFO_PARTIAL_DAMAGE};return true;}
static bool fx_frame(void *c,uint32_t format,risc_display_surface_v1 *s){(void)c;fx_io();assert(!frames&&format==RISC_DISPLAY_FORMAT_MONO1);frames=1;*s=(risc_display_surface_v1){.frame=1,.pixels=pixels,.width=800,.height=480,.stride_bytes=100,.size_bytes=sizeof(pixels),.pixel_format=format};return true;}
static void fx_frame_release(void*c,risc_display_frame_v1 f){(void)c;fx_io();assert(frames&&f==1);frames=0;}
static bool fx_submit(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*t){(void)c;(void)r;(void)n;(void)o;fx_io();assert(frames&&f==1);frames=0;*t=++presents;
#ifdef TEST_PAPER_SHEET_MOTION
 motion_capture();
#endif
 return true;}
static bool fx_present(void*c,risc_display_present_token_v1 t,risc_display_present_status_v1*s){(void)c;fx_io();assert(t);s->state=
#ifdef TEST_PRODUCTIVITY_SCROLL_FIXTURE
 sc_enabled&&sc_busy?RISC_DISPLAY_PRESENT_QUEUED:
#endif
 RISC_DISPLAY_PRESENT_COMPLETE;return true;}
static const risc_display_output_api_v1 fx_display={.api_version=1,.struct_size=sizeof(fx_display),.get_info=fx_info,.acquire=fx_frame,.release=fx_frame_release,.submit=fx_submit,.present_status=fx_present};
static uint64_t fx_sub(void*c){(void)c;fx_io();assert(!subs);subs=1;return 1;}
static bool fx_unsub(void*c,uint64_t n){(void)c;fx_io();assert(n==1&&subs);subs=0;return true;}
static bool fx_touch_poll(void*c,size_t n){(void)c;fx_io();assert(n==1);touch_polls++;return true;}
static int32_t fx_next(void*c,uint64_t n,risc_touch_event_v1*e){(void)c;(void)e;fx_io();assert(n==1);
#ifdef TEST_PRODUCTIVITY_SCROLL_FIXTURE
 if(sc_enabled)return sc_next(e);
#endif
 return 0;}
static bool fx_snapshot(void*c,risc_touch_snapshot_v1*s){(void)c;fx_io();
#ifdef TEST_PRODUCTIVITY_SCROLL_FIXTURE
 if(sc_enabled)return sc_snapshot(s);
#endif
#ifdef TEST_PAPER_SHEET_MOTION
 if(motion_enabled){bool ok=motion_snapshot(s);motion_transform_touch(s);return ok;}
#endif
 *s=(risc_touch_snapshot_v1){.width=480,.height=800};
 if(quick_test&&(touch_polls==5||touch_polls==6)){s->contact_count=1;s->contacts[0]=(risc_touch_contact_v1){.id=1,.x=200,.y=touch_polls==5?20:100};}
 return true;
}
static const risc_touch_api_v1 fx_touch={1,sizeof(fx_touch),NULL,fx_sub,fx_unsub,fx_touch_poll,fx_next,fx_snapshot};
static bool fx_battery(void*c,risc_battery_sample_v1*s){(void)c;fx_io();*s=(risc_battery_sample_v1){.percent=70,.millivolts=3900};return true;}
static const risc_battery_gauge_api_v1 fx_gauge={1,sizeof(fx_gauge),NULL,fx_battery};
static int32_t fx_get(void*c,const char*k,void*b,uint32_t cap,uint32_t*size){
 fx_io();assert(strcmp(k,"points_cfg")&&strcmp(k,"points_meta")&&strcmp(k,"points_occ")&&strcmp(k,"points_utc_occ"));*size=0;
#ifdef PORTABLE_PAPER_PREFERENCES
 if(!strcmp(k,"reader_flip_ui")&&getenv("TEST_PAPER_FLIP")){const uint8_t value[]={0x52,1,1,0xa4};assert(cap>=4);memcpy(b,value,4);*size=4;return 0;}
#endif
 if(kv_result)return kv_result;
 for(unsigned i=0;i<16;i++)if(cells[i].instance==(unsigned)(uintptr_t)c&&!strcmp(cells[i].key,k)){assert(cap>=cells[i].size);memcpy(b,cells[i].bytes,cells[i].size);*size=cells[i].size;return 0;}
 return RISC_KEY_VALUE_NOT_FOUND;
}
static int32_t fx_put(void*c,const char*k,const void*b,uint32_t size){
 (void)c;(void)k;(void)b;(void)size;fx_io();puts_count++;assert(!"Unexpected preference write");return RISC_KEY_VALUE_IO;
}
static int32_t fx_seed_zone(void*c,const char*k,const void*b,uint32_t size){
 assert((unsigned)(uintptr_t)c==1&&!strcmp(k,PORTABLE_TIMEZONE_KEY)&&size<=64);
 cells[15].instance=1;strcpy(cells[15].key,k);memcpy(cells[15].bytes,b,size);cells[15].size=size;return 0;
}
static const risc_key_value_v1 fx_prefs={1,sizeof(fx_prefs),(void*)1,fx_get,fx_put};

static int32_t fx_native(void*c,risc_realtime_snapshot_v1*s){(void)c;fx_io();native_samples++;if(native_result)return native_result;*s=(risc_realtime_snapshot_v1){.struct_size=sizeof(*s),.validity=unset?RISC_REALTIME_UNSET:RISC_REALTIME_VALID,.epoch_seconds=unset?0:epoch,.monotonic_before_us=1,.monotonic_after_us=2,.nanoseconds=malformed?1:0};return 0;}
static const risc_realtime_api_v1 fx_native_api={1,sizeof(fx_native_api),(void*)1,fx_native};
static int32_t fx_alarm_status(void*c,alarm_status_v1*s){(void)c;fx_io();if(alarm_result)return alarm_result;*s=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*s),.state=ALARM_STATE_READY};return 0;}
static int32_t fx_alarm_step(void*c){(void)c;fx_io();return alarm_result;}
static int32_t fx_alarm_ack(void*c,const alarm_token_v1*t){(void)t;return fx_alarm_step(c);}
static int32_t fx_alarm_prepare(void*c,alarm_sleep_v1*s){(void)s;return fx_alarm_step(c);}
static const alarm_service_descriptor_v2 fx_alarm={.base={2,sizeof(fx_alarm),NULL,fx_alarm_status,fx_alarm_step,fx_alarm_step,fx_alarm_ack,fx_alarm_prepare,fx_alarm_step},.tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=1,.output_modes=0};
static bool fx_nav_poll(void*c,risc_input_navigation_frame_v1*s){
 (void)c;fx_io();*s=(risc_input_navigation_frame_v1){0};

#ifdef TEST_PAPER_SHEET_MOTION
 motion_navigation(s);
#endif
 if(quick_test&&touch_polls==15)s->buttons=s->pressed=RISC_NAV_BACK;
 if(home_requested||back_requested){
  home_polls++;
  if(home_polls==1&&home_depth){open_week(0);if(home_depth>=2)open_day(20261004);if(home_depth==3){selected=0;tcp_activate();assert(tcp_editor);strcpy(tcp_entry,"9:45 PM");}tcp_dirty=true;}
  if(home_polls==3)s->buttons=s->pressed=home_requested?RISC_NAV_HOME:RISC_NAV_BACK;
 }
 return true;
}
static bool fx_nav_foreground(void*c,const risc_input_foreground_v1*s,size_t n){(void)c;(void)s;(void)n;fx_io();return true;}
static bool fx_nav_reset(void*c){(void)c;fx_io();
#ifdef TEST_PRODUCTIVITY_IDLE_FIXTURE
 idle_navigation_resets++;
#endif
 return true;}
static const risc_input_navigation_api_v1 fx_nav={1,sizeof(fx_nav),NULL,fx_nav_poll,fx_nav_foreground,fx_nav_reset};
static int32_t fx_stat(void*c,const char*n,uint32_t*size,uint64_t*revision){
#ifdef PORTABLE_BLE_BROADCAST
 assert(!bt_active);
#endif

 (void)c;fx_io();assert(!strcmp(n,TCP_APPDATA_NAME));*size=0;*revision=0;
 if(stat_result)return stat_result;
 if(!data_present)return RISC_APP_DATA_NOT_FOUND;
 *size=(uint32_t)strlen(history);*revision=data_revision;return 0;
}
static int32_t fx_read(void*c,const char*n,uint64_t revision,void*buffer,uint32_t cap,uint32_t*size,uint64_t*actual){
#ifdef PORTABLE_BLE_BROADCAST
 assert(!bt_active);
#endif

 (void)c;fx_io();assert(!strcmp(n,TCP_APPDATA_NAME));*size=0;*actual=0;
 if(read_result)return read_result;
 if(revision!=data_revision)return RISC_APP_DATA_STALE;
 unsigned used=(unsigned)strlen(history);assert(cap>=used);memcpy(buffer,history,used);*size=used;*actual=data_revision;return 0;
}
static int32_t fx_replace(void*c,const char*n,uint64_t revision,const void*data,uint32_t size){
#ifdef PORTABLE_BLE_BROADCAST
 assert(!bt_active);
#endif

 (void)c;fx_io();assert(!frames&&!strcmp(n,TCP_APPDATA_NAME));
 if(replace_result)return replace_result;
 if(revision!=(data_present?data_revision:0))return RISC_APP_DATA_STALE;
 assert(size<sizeof(history));memcpy(history,data,size);history[size]=0;data_revision++;data_present=true;data_writes++;
 return commit_error?RISC_APP_DATA_COMMIT_UNKNOWN:0;
}
static const risc_app_data_v1 fx_data={1,sizeof(fx_data),NULL,fx_stat,fx_read,fx_replace};
static bool fx_acquire(const char*n,uint32_t v,uint64_t instance,risc_runtime_capability_v1*g){
 fx_io();const void*api=NULL;if(fail_acquire_name&&!strcmp(n,fail_acquire_name))return false;assert(strcmp(n,"rtc.clock")&&strcmp(n,"runtime.realtime-control"));
 #ifdef PORTABLE_BLE_BROADCAST
 if(!strcmp(n,TELEMETRY_BROADCAST_CAPABILITY)){assert(v==1&&instance==0);api=&fx_broadcast;}
 else
#endif
#ifdef TEST_PRODUCTIVITY_IDLE_FIXTURE
 if(idle_radio_acquire(n,v,instance,&api)){}
 else
#endif
 if(!strcmp(n,ALARM_SERVICE_CAPABILITY)){assert(v==2);api=&fx_alarm;}
 else {assert(v==1);if(!strcmp(n,"display.output"))api=&fx_display;else if(!strcmp(n,"input.touch.raw"))api=&fx_touch;else if(!strcmp(n,"board.battery"))api=&fx_gauge;else if(!strcmp(n,"input.navigation"))api=&fx_nav;else if(!strcmp(n,"storage.key-value")){assert(instance==1);api=&fx_prefs;}else if(!strcmp(n,RISC_APP_DATA_CAPABILITY)){assert(instance==1);api=&fx_data;}else if(!strcmp(n,RISC_REALTIME_CAPABILITY)){assert(instance==0&&!native_live);native_live=1;api=&fx_native_api;}else assert(!"Unexpected capability");}
#ifdef TEST_PRODUCTIVITY_IDLE_FIXTURE
 if(api==&fx_broadcast)idle_broadcast_grants++;
#endif
 live++;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g),.slot=live,.generation=1,.api=api};return true;
}
static bool fx_release(risc_runtime_capability_v1*g){fx_io();
 if(release_dirty&&g->api==&fx_native_api)return true;
 if((release_fail&&g->api==&fx_native_api)||g->api==fail_release_api)return false;
#ifdef TEST_PRODUCTIVITY_IDLE_FIXTURE
 if(g->api==&fx_broadcast){assert(idle_broadcast_grants);idle_broadcast_grants--;}
#endif
 if(g->api==&fx_native_api){assert(native_live);native_live=0;}assert(live);live--;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
#ifdef TEST_RESIDENT_CLIENT
#include "resident_test_bridge.h"
#else
#define TEST_RESIDENT_BINDING
#endif
static const risc_runtime_api_v1 fx_runtime={TEST_RESIDENT_BINDING .api_version=1,.struct_size=sizeof(fx_runtime),.health=fx_health,.yield_ms=fx_yield,.diagnostic=fx_diag,.request_launch=fx_launch,.acquire=fx_acquire,.release=fx_release,.retain_invocation=fx_retain};
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){return v==1?&fx_runtime:NULL;}
static void zone(const char *id){
 risc_key_value_v1 seed=fx_prefs;seed.put=fx_seed_zone;
 /* Explicit UTC selection must be materialized, not a missing virtual default. */
 if(!cells[15].key[0]){cells[15].instance=1;strcpy(cells[15].key,PORTABLE_TIMEZONE_KEY);cells[15].size=1;}
 int result=portable_timezone_preference_save(&seed,id,strlen(id)+1);assert(result==PORTABLE_TIMEZONE_SAVED||result==PORTABLE_TIMEZONE_UNCHANGED);
}
static int64_t instant(int date,int hour,int minute){return (int64_t)serial_day(date)*86400+hour*3600+minute*60;}
static void start(void){
 assert(app_module_init()==0);app=t5_app_get_api(1);tcp_paper=paper_presentation_get();assert(tcp_paper&&tcp_paper->clock);
 tcp_dependencies_open();tcp_files=timecard_portable_file_storage();assert(tcp_files);
 storage=&tcp_store;system_api=&tcp_clock;system_ui=&tcp_system_ui;fwui=&tcp_ui;
 tcp_home=tcp_editor=tcp_external_exit=false;tcp_draw_screen=UINT32_MAX;screen_id=SCREEN_WEEK_LIST;
 assert(tcp_reload());tcp_draw();assert(tcp_clock_valid&&!native_live&&!puts_count);
}
static void finish(void){tcp_dependencies_close();app_module_fini();assert(!live&&!subs&&!frames&&!retained&&!puts_count);}
static void frozen(void){
 assert(retained&&barriers==1&&portable_adapter_retained());unsigned at=calls;uint8_t h,m;
 assert(!tcp_paper->clock(&h,&m));tcp_draw();assert(!tcp_reload());assert(!tcp_mutate(20261004,0,10));tcp_punch(0);
 t5_app_input_t input={0};tcp_input(&input);tcp_dependencies_close();app_module_fini();assert(calls==at&&!puts_count);
}
int main(int argc,char**argv){
 assert(argc==2);unsigned scenario=(unsigned)atoi(argv[1]);zone("UTC");start();

#ifdef TEST_PAPER_SHEET_MOTION
 if(scenario>=100){
  open_day(20261004);selected=0;tcp_activate();strcpy(tcp_entry,"7:45 PM");tcp_draw();assert(tcp_editor);
  motion_begin(scenario);
  while(ticks-motion_start<3800&&!launches){
   t5_app_input_t input={0};if(!app->poll(&input,20)){assert(scenario==107&&retained);break;}tcp_input(&input);
   if(tcp_dirty)tcp_draw();
  }
  if(scenario==107){
   assert(retained&&barriers==1&&motion_opens==1&&!motion_closes&&!data_writes&&!launches);
   frozen();printf("Sheet provider failure retains all controller custody without later calls\n");return 0;
  }
  assert(tcp_editor&&!strcmp(tcp_entry,"7:45 PM")&&!data_writes&&!puts_count);
  if(scenario==102){assert(launches==1&&motion_opens==1&&motion_closes==1&&!strcmp(last_launch,"default.elf"));motion_enabled=false;}
  else motion_check();
  tcp_editor_cancel();finish();
 }else
#endif
 if(scenario==0){
  epoch=instant(20261004,8,0);tcp_punch(0);epoch=instant(20261004,17,0);tcp_punch(3);
  tc_day_t day=get_day(20261004);assert(day.punches[0]==480&&day.punches[3]==1020&&worked(&day)==540&&data_writes==2);
  for(unsigned i=0;i<10;i++){uint8_t h,m;assert(tcp_paper->clock(&h,&m)&&h==17&&m==0&&!native_live);tcp_draw();}assert(data_writes==2);finish();
 }else if(scenario==1){
  zone("America/Denver");epoch=instant(20260308,8,59);tcp_punch(0);epoch=instant(20260308,9,0);tcp_punch(3);
  tc_day_t day=get_day(20260308);assert(day.punches[0]==119&&day.punches[3]==180&&worked(&day)==61);
  /* Manual civil entry stays valid even inside a DST gap; no UTC inversion. */
  assert(tcp_mutate(20260308,1,150));finish();
 }else if(scenario==2){
  zone("America/Denver");epoch=instant(20261101,7,30);tcp_punch(0);epoch=instant(20261101,8,30);tcp_punch(3);
  tc_day_t day=get_day(20261101);assert(day.punches[0]==90&&day.punches[3]==90&&worked(&day)==0);finish();
 }else if(scenario==3){
  epoch=instant(20261004,20,0);tcp_punch(0);char saved[JSON_CAPACITY];strcpy(saved,history);
  zone("America/Denver");tcp_draw();assert(!strcmp(saved,history)&&get_day(20261004).punches[0]==1200);
  epoch+=300;tcp_punch(3);tc_day_t day=get_day(20261004);assert(day.punches[3]==845&&worked(&day)==-1);
  open_day(20261004);selected=0;tcp_activate();assert(tcp_editor);strcpy(tcp_entry,"8:15 PM");
  zone("Asia/Tokyo");tcp_editor_done();assert(!tcp_editor&&get_day(20261004).punches[0]==1215);
  tcp_punch(3);assert(get_day(20261005).punches[3]==305&&get_day(20261004).punches[3]==845);finish();
 }else if(scenario==4){
  zone("America/Denver");epoch=instant(20270101,0,1);tcp_draw();assert(today()==20261231);tcp_punch(0);assert(get_day(20261231).punches[0]==1021);
  zone("Asia/Tokyo");epoch=instant(20281231,15,0);tcp_draw();assert(today()==20290101);tcp_punch(3);assert(get_day(20290101).punches[3]==0);
  epoch=instant(20280229,0,0);tcp_draw();assert(today()==20280229&&tcp_snapshot.yearday==59);finish();
 }else if(scenario==5){
  memset(cells,0,sizeof(cells));unsigned samples=native_samples;tcp_draw();assert(tcp_clock_valid&&native_samples==samples+1&&!data_writes&&!puts_count);
  zone("UTC");cells[15].bytes[0]^=1;tcp_draw();assert(!tcp_clock_valid);zone("UTC");tcp_draw();assert(tcp_clock_valid);finish();
 }else if(scenario==6){
  unset=true;tcp_punch(0);assert(!data_writes&&!native_live);unset=false;native_result=RISC_REALTIME_IO;tcp_punch(0);assert(!data_writes&&!native_live);
  native_result=0;epoch=-1;tcp_punch(0);assert(!data_writes);epoch=instant(20261004,12,0);tcp_punch(0);assert(data_writes==1);finish();
 }else if(scenario==7){
  assert(tcp_mutate(20261004,0,480));data_revision++;assert(!tcp_mutate(20261004,0,500)&&!store_ready&&get_day(20261004).punches[0]==480);assert(tcp_reload());
  commit_error=true;assert(!tcp_mutate(20261004,0,510)&&!store_ready&&get_day(20261004).punches[0]==480);commit_error=false;assert(tcp_reload()&&get_day(20261004).punches[0]==510);
  replace_result=RISC_APP_DATA_NO_SPACE;assert(!tcp_mutate(20261004,0,520)&&!store_ready);replace_result=0;assert(tcp_reload()&&get_day(20261004).punches[0]==510);
  read_result=RISC_APP_DATA_IO;assert(!tcp_reload()&&!store_ready);read_result=0;assert(tcp_reload());finish();
 }else if(scenario==8){
  assert(tcp_mutate(20261004,0,480));open_day(20261004);selected=0;tcp_activate();strcpy(tcp_entry,"25:99");tcp_editor_done();assert(tcp_editor&&data_writes==1);
  strcpy(tcp_entry,"10:30 PM");tcp_editor_cancel();assert(!tcp_editor&&data_writes==1);tcp_activate();strcpy(tcp_entry,"");tcp_editor_done();assert(!tcp_editor&&!day_count&&data_writes==2);finish();
 }else if(scenario==9){
  tcp_dependencies_close();home_requested=true;app_main();app_module_fini();assert(launches==1&&!data_writes&&!live&&!retained);
 }else if(scenario==10){
  open_week(0);open_day(20261004);tcp_activate();assert(tcp_editor);tcp_back();assert(!tcp_editor&&screen_id==SCREEN_DAY);tcp_back();assert(screen_id==SCREEN_WEEK);tcp_back();assert(screen_id==SCREEN_WEEK_LIST);tcp_back();assert(tcp_home);finish();
 }else if(scenario>=11&&scenario<=20){
  if(scenario==11){native_result=RISC_REALTIME_CONTEXT;tcp_draw();}
  if(scenario==12){release_fail=true;tcp_draw();}
  if(scenario==13){kv_result=RISC_KEY_VALUE_CONTEXT;tcp_draw();}
  if(scenario==14){replace_result=RISC_APP_DATA_RETAINED;(void)tcp_mutate(20261004,0,480);}
  if(scenario==15){stat_result=RISC_APP_DATA_CONTEXT;(void)tcp_reload();}
  if(scenario==16){fail_release_api=&fx_data;tcp_dependencies_close();}
  if(scenario==17){fail_release_api=&fx_prefs;tcp_dependencies_close();}
  if(scenario==18){alarm_result=ALARM_RETAINED;t5_app_input_t input;(void)app->poll(&input,20);}
  if(scenario==19){fail_acquire_name="storage.key-value";tcp_draw();}
  if(scenario==20){kv_result=-333;tcp_draw();}
  frozen();
 }else if(scenario==21){
  for(int i=0;i<400;i++){days[i]=blank_day(add_days(20200101,i));days[i].punches[0]=480;}day_count=400;
  assert(!tcp_mutate(20261004,0,480)&&day_count==400&&!data_writes&&store_ready);
  assert(tcp_mutate(20200101,0,-1)&&day_count==399);assert(tcp_mutate(20261004,0,480)&&day_count==400);finish();
 }else if(scenario==22){
  kv_result=RISC_KEY_VALUE_IO;tcp_draw();assert(!tcp_clock_valid&&!retained&&!data_writes);kv_result=0;tcp_draw();assert(tcp_clock_valid);finish();
 }else if(scenario>=23&&scenario<=25){
  tcp_dependencies_close();home_depth=scenario-22;home_requested=true;app_main();app_module_fini();assert(launches==1&&!strcmp(last_launch,"default.elf")&&!data_writes&&!live&&!retained);
 }else if(scenario==26){
  tcp_dependencies_close();back_requested=true;app_main();app_module_fini();assert(launches==1&&!strcmp(last_launch,"springboard.elf")&&!data_writes&&!live&&!retained);
 }else if(scenario==27){
  malformed=true;tcp_punch(0);assert(!data_writes&&!native_live&&!retained);malformed=false;
  fail_acquire_name=RISC_REALTIME_CAPABILITY;tcp_punch(0);assert(!data_writes&&!native_live&&!retained);fail_acquire_name=NULL;tcp_punch(0);assert(data_writes==1);finish();
 }else if(scenario==28){release_dirty=true;tcp_draw();frozen();
 }else if(scenario==29){
  assert(tcp_mutate(20261004,0,480));read_result=-333;assert(!tcp_reload());frozen();
 }else if(scenario==30){
  tcp_dependencies_close();fail_acquire_name=RISC_APP_DATA_CAPABILITY;tcp_dependencies_open();frozen();
 }else if(scenario==31){
  tcp_dependencies_close();kv_result=RISC_KEY_VALUE_CONTEXT;tcp_dependencies_open();frozen();
 }else if(scenario==32){
  open_day(20261004);selected=0;tcp_activate();strcpy(tcp_entry,"7:45 PM");tcp_draw();assert(tcp_editor);
  unsigned samples=native_samples,old_presents=presents;quick_test=true;
  for(unsigned i=0;i<20;i++){t5_app_input_t input;assert(app->poll(&input,20));tcp_input(&input);if(tcp_dirty)tcp_draw();}
  assert(tcp_editor&&!strcmp(tcp_entry,"7:45 PM")&&!data_writes&&native_samples>samples&&presents>=old_presents+2);tcp_editor_cancel();finish();

#ifdef PORTABLE_BLE_BROADCAST
 }else if(scenario==33){
  bt_active=true;unsigned pauses=bt_pauses;assert(tcp_mutate(20261004,0,480));
  assert(!bt_active&&bt_pauses>pauses&&data_writes==1);finish();
 }else if(scenario==34){
  bt_active=true;bt_pause_fail=true;assert(!tcp_mutate(20261004,0,480));assert(!data_writes);frozen();
 }else if(scenario==35){
  bt_active=true;fail_release_api=&fx_broadcast;assert(!tcp_mutate(20261004,0,480));assert(!data_writes);frozen();
 }else if(scenario==36){
  bt_active=true;replace_result=RISC_APP_DATA_RETAINED;assert(!tcp_mutate(20261004,0,480));assert(!bt_active&&!data_writes);frozen();
#endif
 }else assert(0);
 printf("Native Timecard scenario%u passed; samples%u frames%u writes%u retained%u\n",scenario,native_samples,presents,data_writes,barriers);return 0;
}
