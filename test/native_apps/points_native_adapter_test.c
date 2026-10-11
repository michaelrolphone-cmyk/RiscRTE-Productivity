/* Actual Points controller/helper + separately linked production System adapter. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../Apps/points_in_time.c"
#include "PortableApps.h"
#include "RiscDisplayOutputV1.h"
#include "RiscTouchV1.h"
#include "RiscBatteryGaugeV1.h"
#include "RiscInputNavigationV1.h"
int app_module_init(void);void app_module_fini(void);
const t5_app_manifest_t portable_catalog[1]={{.compatible=false}};
const unsigned portable_catalog_count=0;
static unsigned calls,live,frames,subs,presents,native_live,native_samples,puts_count,ticks,barriers;
static bool retained,release_fail,unset,home_requested;
static unsigned home_polls,launches;
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
static bool fx_launch(const char *s){fx_io();assert(!strcmp(s,"springboard.elf")||!strcmp(s,"default.elf"));launches++;return true;}
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
static bool fx_touch_poll(void*c,size_t n){(void)c;fx_io();assert(n==1);return true;}
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
 *s=(risc_touch_snapshot_v1){.width=480,.height=800};return true;}
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
 fx_io();assert((unsigned)(uintptr_t)c==5&&(!strcmp(k,POINTS_CONFIG_KEY)||!strcmp(k,POINTS_META_KEY)));puts_count++;if(kv_result)return kv_result;
 unsigned i;for(i=0;i<16&&cells[i].key[0]&&strcmp(cells[i].key,k);i++){}assert(i<16&&size<=64);cells[i].instance=(unsigned)(uintptr_t)c;strcpy(cells[i].key,k);memcpy(cells[i].bytes,b,size);cells[i].size=size;return 0;
}
static int32_t fx_seed_zone(void*c,const char*k,const void*b,uint32_t size){
 assert((unsigned)(uintptr_t)c==1&&!strcmp(k,PORTABLE_TIMEZONE_KEY)&&size<=64);
 cells[15].instance=1;strcpy(cells[15].key,k);memcpy(cells[15].bytes,b,size);cells[15].size=size;return 0;
}
static const risc_key_value_v1 fx_prefs={1,sizeof(fx_prefs),(void*)1,fx_get,fx_put};
static const risc_key_value_v1 fx_store={1,sizeof(fx_store),(void*)5,fx_get,fx_put};
static int32_t fx_native(void*c,risc_realtime_snapshot_v1*s){(void)c;fx_io();native_samples++;if(native_result)return native_result;*s=(risc_realtime_snapshot_v1){.struct_size=sizeof(*s),.validity=unset?RISC_REALTIME_UNSET:RISC_REALTIME_VALID,.epoch_seconds=unset?0:1791115200,.monotonic_before_us=1,.monotonic_after_us=2};return 0;}
static const risc_realtime_api_v1 fx_native_api={1,sizeof(fx_native_api),(void*)1,fx_native};
static int32_t fx_alarm_status(void*c,alarm_status_v1*s){(void)c;fx_io();if(alarm_result)return alarm_result;*s=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*s),.state=ALARM_STATE_READY};return 0;}
static int32_t fx_alarm_step(void*c){(void)c;fx_io();return alarm_result;}
static int32_t fx_alarm_ack(void*c,const alarm_token_v1*t){(void)t;return fx_alarm_step(c);}
static int32_t fx_alarm_prepare(void*c,alarm_sleep_v1*s){(void)s;return fx_alarm_step(c);}
static const alarm_service_descriptor_v2 fx_alarm={.base={2,sizeof(fx_alarm),NULL,fx_alarm_status,fx_alarm_step,fx_alarm_step,fx_alarm_ack,fx_alarm_prepare,fx_alarm_step},.tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=1,.output_modes=0};
static bool fx_nav_poll(void*c,risc_input_navigation_frame_v1*s){(void)c;fx_io();*s=(risc_input_navigation_frame_v1){0};
#ifdef TEST_PAPER_SHEET_MOTION
 motion_navigation(s);
#endif
 if(home_requested&&++home_polls==3)s->buttons=s->pressed=RISC_NAV_HOME;
 return true;}
static bool fx_nav_foreground(void*c,const risc_input_foreground_v1*s,size_t n){(void)c;(void)s;(void)n;fx_io();return true;}
static bool fx_nav_reset(void*c){(void)c;fx_io();
#ifdef TEST_PRODUCTIVITY_IDLE_FIXTURE
 idle_navigation_resets++;
#endif
 return true;}
static const risc_input_navigation_api_v1 fx_nav={1,sizeof(fx_nav),NULL,fx_nav_poll,fx_nav_foreground,fx_nav_reset};
static bool fx_acquire(const char*n,uint32_t v,uint64_t instance,risc_runtime_capability_v1*g){
 fx_io();const void*api=NULL;assert(strcmp(n,"rtc.clock")&&strcmp(n,"runtime.realtime-control"));
 #ifdef PORTABLE_BLE_BROADCAST
 if(!strcmp(n,TELEMETRY_BROADCAST_CAPABILITY)){assert(v==1&&instance==0);api=&fx_broadcast;}
 else
#endif
#ifdef TEST_PRODUCTIVITY_IDLE_FIXTURE
 if(idle_radio_acquire(n,v,instance,&api)){}
 else
#endif
 if(!strcmp(n,ALARM_SERVICE_CAPABILITY)){assert(v==2);api=&fx_alarm;}
 else {assert(v==1);if(!strcmp(n,"display.output"))api=&fx_display;else if(!strcmp(n,"input.touch.raw"))api=&fx_touch;else if(!strcmp(n,"board.battery"))api=&fx_gauge;else if(!strcmp(n,"input.navigation"))api=&fx_nav;else if(!strcmp(n,"storage.key-value")){assert(instance==1||instance==5);api=instance==1?&fx_prefs:&fx_store;}else if(!strcmp(n,RISC_REALTIME_CAPABILITY)){assert(!native_live);native_live=1;api=&fx_native_api;}else assert(!"Unexpected capability");}
#ifdef TEST_PRODUCTIVITY_IDLE_FIXTURE
 if(api==&fx_broadcast)idle_broadcast_grants++;
#endif
 live++;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g),.slot=live,.generation=1,.api=api};return true;
}
static bool fx_release(risc_runtime_capability_v1*g){fx_io();
 if(release_fail&&g->api==&fx_native_api)return false;
#ifdef TEST_PRODUCTIVITY_IDLE_FIXTURE
 if(g->api==&fx_broadcast){assert(idle_broadcast_grants);idle_broadcast_grants--;}
#endif
 if(g->api==&fx_native_api){assert(native_live);native_live=0;}assert(live);live--;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
static const risc_runtime_api_v1 fx_runtime={.api_version=1,.struct_size=sizeof(fx_runtime),.health=fx_health,.yield_ms=fx_yield,.diagnostic=fx_diag,.request_launch=fx_launch,.acquire=fx_acquire,.release=fx_release,.retain_invocation=fx_retain};
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){return v==1?&fx_runtime:NULL;}
int main(int argc,char**argv){
 assert(argc==2);unsigned scenario=(unsigned)atoi(argv[1]);assert(app_module_init()==0);
 app=t5_app_get_api(1);paper=paper_presentation_get();assert(paper&&paper->clock);ready=open_dependencies();assert(ready);load_catalog();assert(writer.loaded&&clock_valid&&!native_live&&!puts_count);
 pe_clean=true;page=PAGE_LIST;draw();assert(presents&&!frames);
 uint8_t hour=0,minute=0;for(unsigned i=0;i<10;i++){assert(paper->clock(&hour,&minute)&&hour==12&&minute==0&&!native_live);}

#ifdef TEST_PAPER_SHEET_MOTION
 if(scenario>=100){
  nova_new_point();draft.hour=9;draw();assert(page==PAGE_EDIT);
  unsigned saved_hour=draft.hour;motion_begin(scenario);
  while(ticks-motion_start<3800&&!launches){
   t5_app_input_t input={0};if(!app->poll(&input,20)){assert(scenario==107&&retained);break;}
   if(input.exit_requested)break;
   assert(!(input.buttons&T5_APP_BUTTON_BACK));
   (void)pe_input(&input);
  }
  if(scenario==107){
   assert(retained&&barriers==1&&motion_opens==1&&!motion_closes);
   unsigned frozen=calls;draw();save_action();close_dependencies();app_module_fini();assert(calls==frozen&&!puts_count&&!launches);
   printf("Sheet provider failure retains all controller custody without later calls\n");return 0;
  }
  assert(page==PAGE_EDIT&&draft.hour==saved_hour&&!puts_count);
  if(scenario==102){assert(launches==1&&motion_opens==1&&motion_closes==1&&!puts_count);motion_enabled=false;}
  else motion_check();
  close_dependencies();app_module_fini();assert(!live&&!subs&&!frames&&!retained);
 }else
#endif
 if(scenario==0){nova_new_point(); /* Existing defaults leave slot8 empty. */draft.hour=9;save_action();assert(puts_count==2&&!writer.uncertain);draw();close_dependencies();app_module_fini();assert(!live&&!subs&&!frames&&!retained);}
 else if(scenario==1){unset=true;assert(!paper->clock(&hour,&minute)&&!native_live);unset=false;assert(paper->clock(&hour,&minute));close_dependencies();app_module_fini();assert(!live);}
 else if(scenario==8){risc_key_value_v1 configured=fx_prefs;configured.put=fx_seed_zone;
  assert(portable_timezone_preference_save(&configured,"America/Denver",15)==PORTABLE_TIMEZONE_SAVED);
  assert(paper->clock(&hour,&minute)&&hour==6&&minute==0&&!native_live&&!puts_count);
  assert(portable_timezone_preference_save(&configured,"Asia/Tokyo",11)==PORTABLE_TIMEZONE_SAVED);
  assert(paper->clock(&hour,&minute)&&hour==21&&minute==0&&!native_live&&!puts_count);
  close_dependencies();app_module_fini();assert(!live&&!retained);
 }
 else if(scenario==7){close_dependencies();home_requested=true;app_main();app_module_fini();assert(launches==1&&!puts_count&&!live&&!retained);}
 else if(scenario==2){kv_result=RISC_KEY_VALUE_IO;uint32_t now;assert(!read_clock(&now)&&!retained&&!native_live);kv_result=0;assert(read_clock(&now));close_dependencies();app_module_fini();assert(!live);}

#ifdef PORTABLE_BLE_BROADCAST
 else if(scenario==9){
  nova_new_point();draft.hour=9;bt_active=true;unsigned pauses=bt_pauses;save_action();
  assert(!bt_active&&bt_pauses>pauses&&puts_count==2);close_dependencies();app_module_fini();assert(!live&&!retained);
 }
 else if(scenario==10){
  nova_new_point();draft.hour=9;bt_active=true;bt_pause_fail=true;save_action();
  assert(retained&&barriers==1&&!puts_count);unsigned frozen=calls;draw();retry_action();save_action();close_dependencies();app_module_fini();assert(calls==frozen);
 }
#endif
 else{
  if(scenario==3)native_result=RISC_REALTIME_CONTEXT;
  else if(scenario==4)release_fail=true;
  else if(scenario==5)kv_result=RISC_KEY_VALUE_CONTEXT;
  else if(scenario==6)alarm_result=ALARM_RETAINED;
  else assert(0);
  if(scenario==6)refresh_status();else (void)paper->clock(&hour,&minute);
  assert(retained&&barriers==1&&portable_adapter_retained());unsigned frozen=calls;
  assert(!paper->clock(&hour,&minute));draw();retry_action();save_action();close_dependencies();app_module_fini();assert(calls==frozen);
 }
 printf("Native Points/adapter scenario%u passed; samples%u frames%u retained%u\n",scenario,native_samples,presents,barriers);return 0;
}
