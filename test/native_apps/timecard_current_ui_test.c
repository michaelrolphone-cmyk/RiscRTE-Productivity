/* Current Timecard controller/scene/Quick owning composition.
 * Adapted from the frozen native UI fixture; original remains unchanged.
 * Provider tables below are test doubles, not Runtime execution evidence. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../Apps/timecard_portable.c"
#include "PortableApps.h"
#include "RiscRealtimeV1.h"
#include "PortableTimeZonePreference.h"
#include "RiscDisplayOutputV1.h"
#include "RiscTouchV1.h"
#include "RiscBatteryGaugeV1.h"
#include "RiscInputNavigationV1.h"
#include "RiscPreferenceStoreV1.h"
#include "PortableQuickActions.h"
#include "PortableQuickLayout.h"
int app_module_init(void);void app_module_fini(void);
const t5_app_manifest_t portable_catalog[1]={{.compatible=false}};
const unsigned portable_catalog_count=0;
static unsigned calls,live,frames,subs,presents,native_live,native_samples,puts_count,ticks,barriers;
static bool retained,release_fail,unset,home_requested;
static int stat_result,read_result,replace_result;
static bool data_present=true,commit_error;
static uint64_t data_revision=1;
static char history[JSON_CAPACITY];
static unsigned data_writes;
static int64_t epoch=1791100800;
static twatch_rtc_time_v1 rtc_value={2026,10,4,0,8,0,0};
static unsigned home_polls,launches;
static char launched[40];
static int native_result,kv_result,alarm_result;
static uint16_t pixels[643*640];
static unsigned native_width=466,native_height=466;
static unsigned inset;
static unsigned latency=40,submit_tick;static bool submit_fail,status_fail;
static bool touch_down,touch_gap;static uint16_t touch_x,touch_y;
static risc_touch_snapshot_v1 touch_state;
static risc_touch_event_v1 events[RISC_TOUCH_QUEUE_LENGTH];
static unsigned event_head,event_count;
static bool alarm_alert;static unsigned alarm_dismiss_at,alarm_acks,modal_frames;
static unsigned nav_buttons;
#ifdef PORTABLE_QUICK_ACTIONS
static bool quick_requested;static unsigned quick_dismiss_at,quick_frames;
#endif
static void (*poll_driver)(void);

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
static bool fx_launch(const char *s){fx_io();assert(!frames&&ticks-submit_tick>=latency);assert(!strcmp(s,"springboard.elf")||!strcmp(s,"default.elf")||!strcmp(s,"wifi_settings.elf"));snprintf(launched,sizeof(launched),"%s",s);launches++;return true;}
static bool fx_retain(void){assert(!retained);retained=true;barriers++;return true;}
static bool fx_info(void*c,risc_display_info_v1*s){(void)c;fx_io();*s=(risc_display_info_v1){.width=native_width,.height=native_height,.supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565),.flags=RISC_DISPLAY_INFO_ASYNC_PRESENT|RISC_DISPLAY_INFO_PARTIAL_DAMAGE,.nominal_refresh_millihz=60000,.safe_insets={inset,inset,inset,inset}};return true;}
static bool fx_frame(void*c,uint32_t format,risc_display_surface_v1*s){(void)c;fx_io();assert(!frames&&format==RISC_DISPLAY_FORMAT_RGB565);frames=1;*s=(risc_display_surface_v1){.frame=1,.pixels=pixels,.width=native_width,.height=native_height,.stride_bytes=(native_width+3)*2,.size_bytes=sizeof(pixels),.pixel_format=format};return true;}
static void fx_frame_release(void*c,risc_display_frame_v1 f){(void)c;fx_io();assert(frames&&f==1);frames=0;}
static bool fx_submit(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*t){(void)c;(void)r;(void)n;(void)o;fx_io();assert(frames&&f==1);if(submit_fail)return false;frames=0;*t=++presents;submit_tick=ticks;if(alarm_alert)modal_frames++;
#ifdef PORTABLE_QUICK_ACTIONS
 if(quick_requested)quick_frames++;
#endif
 return true;}
static bool fx_present(void*c,risc_display_present_token_v1 t,risc_display_present_status_v1*s){(void)c;fx_io();assert(t);s->state=status_fail?RISC_DISPLAY_PRESENT_FAILED:ticks-submit_tick<latency?RISC_DISPLAY_PRESENT_ACTIVE:RISC_DISPLAY_PRESENT_COMPLETE;return true;}
static const risc_display_output_api_v1 fx_display={.api_version=1,.struct_size=sizeof(fx_display),.get_info=fx_info,.acquire=fx_frame,.release=fx_frame_release,.submit=fx_submit,.present_status=fx_present};
static uint64_t fx_sub(void*c){(void)c;fx_io();assert(!subs);subs=1;touch_state=(risc_touch_snapshot_v1){.width=native_width,.height=native_height};return 1;}
static bool fx_unsub(void*c,uint64_t n){(void)c;fx_io();assert(n==1&&subs);subs=0;return true;}
static void emit(unsigned kind,uint16_t x,uint16_t y){assert(event_count<RISC_TOUCH_QUEUE_LENGTH);events[(event_head+event_count++)%RISC_TOUCH_QUEUE_LENGTH]=(risc_touch_event_v1){.sequence=++touch_state.sequence,.timestamp_ms=ticks,.kind=kind,.id=1,.x=x,.y=y};}
static bool fx_touch_poll(void*c,size_t n){(void)c;fx_io();assert(n==1);if(poll_driver)poll_driver();bool before=touch_state.contact_count!=0;
 if(before&&!touch_down)emit(RISC_TOUCH_EVENT_UP,touch_x,touch_y);
 else if(!before&&touch_down)emit(RISC_TOUCH_EVENT_DOWN,touch_x,touch_y);
 else if(before&&touch_down&&(touch_state.contacts[0].x!=touch_x||touch_state.contacts[0].y!=touch_y))emit(RISC_TOUCH_EVENT_MOVE,touch_x,touch_y);
 touch_state.contact_count=touch_down?1:0;touch_state.timestamp_ms=ticks;
 touch_state.contacts[0]=(risc_touch_contact_v1){.id=1,.x=touch_x,.y=touch_y};return true;}
static int32_t fx_next(void*c,uint64_t n,risc_touch_event_v1*e){(void)c;fx_io();assert(n==1);if(touch_gap){touch_gap=false;return -1;}if(!event_count)return 0;*e=events[event_head];event_head=(event_head+1)%RISC_TOUCH_QUEUE_LENGTH;event_count--;return 1;}
static bool fx_snapshot(void*c,risc_touch_snapshot_v1*s){(void)c;fx_io();*s=touch_state;return true;}
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
static int32_t fx_put(void*c,const char*k,const void*b,uint32_t size){(void)c;(void)k;(void)b;(void)size;fx_io();puts_count++;assert(!"Unexpected preference write");return RISC_KEY_VALUE_IO;}
static const risc_key_value_v1 fx_prefs={1,sizeof(fx_prefs),(void*)1,fx_get,fx_put};
static bool fx_rtc(void*c,twatch_rtc_time_v1*s){(void)c;fx_io();if(unset)return false;*s=rtc_value;return true;}
static const twatch_rtc_api_v1 fx_rtc_api={.api_version=2,.struct_size=sizeof(fx_rtc_api),.read=fx_rtc};
static int32_t fx_native(void*c,risc_realtime_snapshot_v1*s){(void)c;fx_io();native_samples++;if(native_result)return native_result;*s=(risc_realtime_snapshot_v1){.struct_size=sizeof(*s),.validity=unset?RISC_REALTIME_UNSET:RISC_REALTIME_VALID,.epoch_seconds=unset?0:epoch,.monotonic_before_us=1,.monotonic_after_us=2};return 0;}
static const risc_realtime_api_v1 fx_native_api={1,sizeof(fx_native_api),(void*)1,fx_native};
static int32_t fx_alarm_status(void*c,alarm_status_v1*s){(void)c;fx_io();if(alarm_result)return alarm_result;*s=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*s),.state=alarm_alert?ALARM_STATE_ALERT:ALARM_STATE_READY,.mode=ALARM_MODE_BOTH,.occurrence=alarm_alert?(alarm_token_v1){.kind=ALARM_KIND_ALARM,.revision=1,.deadline=1,.generation=1}:(alarm_token_v1){0}};return 0;}
static int32_t fx_alarm_step(void*c){(void)c;fx_io();return alarm_result;}
static int32_t fx_alarm_ack(void*c,const alarm_token_v1*t){assert(t->generation==1);alarm_acks++;alarm_alert=false;return fx_alarm_step(c);}
static int32_t fx_alarm_prepare(void*c,alarm_sleep_v1*s){(void)s;return fx_alarm_step(c);}
#ifdef TIMECARD_NATIVE_TIME
static alarm_service_descriptor_v2 fx_alarm={.base={2,sizeof(fx_alarm),NULL,fx_alarm_status,fx_alarm_step,fx_alarm_step,fx_alarm_ack,fx_alarm_prepare,fx_alarm_step},.tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=1,.output_modes=3};
#else
static alarm_service_v1 fx_alarm={1,sizeof(fx_alarm),NULL,fx_alarm_status,fx_alarm_step,fx_alarm_step,fx_alarm_ack,fx_alarm_prepare,fx_alarm_step};
#endif
static bool fx_nav_poll(void*c,risc_input_navigation_frame_v1*s){(void)c;fx_io();*s=(risc_input_navigation_frame_v1){.buttons=nav_buttons,.pressed=nav_buttons};nav_buttons=0;
#ifdef TEST_PAPER_SHEET_MOTION
 motion_navigation(s);
#endif
#ifdef PORTABLE_QUICK_ACTIONS
 if(quick_dismiss_at&&ticks>=quick_dismiss_at){s->buttons=s->pressed=RISC_NAV_BACK;quick_dismiss_at=0;}
#endif
 if(alarm_alert&&ticks>=alarm_dismiss_at)s->buttons=s->pressed=RISC_NAV_BACK;
 if(home_requested&&++home_polls==3)s->buttons=s->pressed=RISC_NAV_HOME;
 return true;}
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
static int32_t fx_preference_state(void*c,risc_preference_store_state_v1*s){(void)c;fx_io();*s=(risc_preference_store_state_v1){.struct_size=sizeof(*s),.flags=RISC_PREFERENCE_STORE_COPIED_INPUT|RISC_PREFERENCE_STORE_ATOMIC_GROUP,.namespace_id=1,.max_mutations=8,.max_value_bytes=64,.max_group_bytes=512,.max_intent_bytes=176,.revision=1};return 0;}
static int32_t fx_preference_get(void*c,uint64_t revision,const char*k,void*b,uint32_t cap,uint32_t*n){assert(revision==1);int r=fx_get(c,k,b,cap,n);return r==RISC_KEY_VALUE_NOT_FOUND?RISC_ATOMIC_NOT_FOUND:r;}
static int32_t fx_preference_submit(void*c,uint64_t revision,const risc_key_value_atomic_mutation_v1*m,uint32_t count,const void*i,uint32_t size,uint64_t*token){(void)c;(void)revision;(void)m;(void)count;(void)i;(void)size;(void)token;fx_io();assert(!"Unexpected Quick preference write in modal-only test");return RISC_ATOMIC_INVALID;}
static int32_t fx_preference_collect(void*c,uint64_t token,risc_key_value_atomic_receipt_v1*r){(void)c;(void)token;(void)r;fx_io();assert(!"Unexpected Quick operation collection");return RISC_ATOMIC_INVALID;}
static int32_t fx_preference_cancel(void*c,uint64_t token){(void)c;(void)token;fx_io();assert(!"Unexpected Quick operation cancellation");return RISC_ATOMIC_INVALID;}
static const risc_preference_store_v1 fx_preference_store={1,sizeof(fx_preference_store),(void*)1,fx_preference_state,fx_preference_get,fx_preference_submit,fx_preference_collect,fx_preference_cancel};
static int32_t fx_checkpoint(void){fx_io();return 0;}
static bool fx_acquire(const char*n,uint32_t v,uint64_t instance,risc_runtime_capability_v1*g){
 fx_io();const void*api=NULL;assert(strcmp(n,"runtime.realtime-control"));
 if(!strcmp(n,ALARM_SERVICE_CAPABILITY)){
#ifdef TIMECARD_NATIVE_TIME
  assert(v==2);
#else
  assert(v==1);
#endif
  api=&fx_alarm;
 }else if(!strcmp(n,"rtc.clock")){
#ifdef TIMECARD_NATIVE_TIME
  assert(!"Unexpected RTC grant");
#endif
  assert(v==2&&instance==0);api=&fx_rtc_api;
 }else {assert(v==1);if(!strcmp(n,"display.output"))api=&fx_display;else if(!strcmp(n,"input.touch.raw"))api=&fx_touch;else if(!strcmp(n,"board.battery"))api=&fx_gauge;else if(!strcmp(n,"input.navigation"))api=&fx_nav;else if(!strcmp(n,"storage.key-value")){assert(instance==1);api=&fx_prefs;}else if(!strcmp(n,RISC_PREFERENCE_STORE_CAPABILITY)){assert(instance==1);api=&fx_preference_store;}else if(!strcmp(n,RISC_APP_DATA_CAPABILITY)){assert(instance==1);api=&fx_data;}else if(!strcmp(n,RISC_REALTIME_CAPABILITY)){assert(!native_live);native_live=1;api=&fx_native_api;}else return false;}
 live++;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g),.slot=live,.generation=1,.api=api};return true;
}
static bool fx_release(risc_runtime_capability_v1*g){fx_io();
 if(release_fail&&g->api==&fx_native_api)return false;
#ifdef TEST_PRODUCTIVITY_IDLE_FIXTURE
 if(g->api==&fx_broadcast){assert(idle_broadcast_grants);idle_broadcast_grants--;}
#endif
 if(g->api==&fx_native_api){assert(native_live);native_live=0;}assert(live);live--;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
static const risc_runtime_api_v1 fx_runtime={.api_version=1,.struct_size=sizeof(fx_runtime),.health=fx_health,.yield_ms=fx_yield,.diagnostic=fx_diag,.request_launch=fx_launch,.acquire=fx_acquire,.release=fx_release,.retain_invocation=fx_retain,.service_checkpoint=fx_checkpoint};
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){return v==1?&fx_runtime:NULL;}
static void check_layout(void){
 assert(tcn.active&&tcn.count<=40);
 for(unsigned i=0;i<tcn.count;i++){
  tcn_rect r=tcn.controls[i].r;assert(r.x>=0&&r.y>=0&&r.w>0&&r.h>0&&r.x+r.w<=tcn.w&&r.y+r.h<=tcn.h);
  for(unsigned j=0;j<i;j++){tcn_rect q=tcn.controls[j].r;assert(r.x+r.w<=q.x||q.x+q.w<=r.x||r.y+r.h<=q.y||q.y+q.h<=r.y);}
 }
}
static void frame(void){tcp_dirty=true;tcp_draw();check_layout();}
static void pump(void){
 t5_app_input_t in={0};assert(app->poll(&in,20));tcp_input(&in);
 if(tcp_dirty||portable_timecard_native_repaint())frame();
}
static void settle(void){for(unsigned i=0;i<40;i++)pump();assert(portable_timecard_native_ready(NULL,NULL));}
static void seed(void){strcpy(history,"{\"days\":[{\"d\":20261004,\"in\":480,\"ls\":720,\"le\":750,\"out\":1020}]}");}
static void begin(void){
 for(unsigned y=0;y<native_height;y++)for(unsigned x=0;x<native_width+3;x++)pixels[y*(native_width+3)+x]=x<native_width?0:0xa55a;
 assert(!live&&!subs&&!frames);assert(app_module_init()==0);app=t5_app_get_api(1);
#ifdef TIMECARD_PAPER
 tcp_paper=paper_presentation_get();assert(!tcp_paper);
#endif
 assert(portable_timecard_native_available());assert(tcn_open(app->screen_width(),app->screen_height()));
 tcp_home=tcp_editor=tcp_external_exit=false;tcp_dirty=true;tcp_scroll=0;tcp_draw_screen=UINT32_MAX;tcp_reset_gesture();
 screen_id=SCREEN_WEEK_LIST;week_offset=selected=editing_ymd=day_count=0;store_ready=false;status_text[0]=0;
 tcp_dependencies_open();tcp_files=timecard_portable_file_storage();assert(tcp_files);
 storage=&tcp_store;system_api=&tcp_clock;system_ui=&tcp_system_ui;fwui=&tcp_ui;
 app->set_back_exits_app(false);assert(tcp_reload());frame();assert(tcp_clock_valid);
#ifdef PORTABLE_RASTER_SNAPSHOT
 assert(!presents&&!frames);
#else
 assert(presents);
#endif
 settle();
}
static void end(void){tcp_dependencies_close();app_module_fini();assert(!live&&!subs&&!frames&&!retained&&!puts_count);for(unsigned y=0;y<native_height;y++)for(unsigned x=native_width;x<native_width+3;x++)assert(pixels[y*(native_width+3)+x]==0xa55a);}
static void tap_at(int x,int y){touch_x=(uint16_t)(x+inset);touch_y=(uint16_t)(y+inset);touch_down=true;pump();touch_down=false;pump();settle();}
static int find_control(unsigned action,unsigned arg){for(unsigned i=0;i<tcn.count;i++)if(tcn.controls[i].action==action&&tcn.controls[i].arg==arg)return (int)i;return -1;}
static void tap(unsigned action,unsigned arg){
 int index=find_control(action,arg);
 for(unsigned n=0;index<0&&n<40;n++){
  int scroll=find_control(TCN_DOWN,0);assert(scroll>=0);tcn_rect r=tcn.controls[scroll].r;tap_at(r.x+r.w/2,r.y+r.h/2);index=find_control(action,arg);
 }
 if(index<0){fprintf(stderr,"Missing action %u arg %u screen%u first%u total%u editor%d\n",action,arg,screen_id,tcn.first,tcn.total,tcp_editor);abort();}
 tcn_rect r=tcn.controls[index].r;tap_at(r.x+r.w/2,r.y+r.h/2);
}
static void capture(const char *name){const char*dir=getenv("TIMECARD_NATIVE_FRAMES");if(!dir)return;char path[512];snprintf(path,sizeof(path),"%s/%ux%u-%s.ppm",dir,native_width,native_height,name);FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%u %u\n255\n",native_width,native_height);for(unsigned y=0;y<native_height;y++)for(unsigned x=0;x<native_width;x++){uint16_t p=pixels[y*(native_width+3)+x];unsigned char rgb[]={((p>>11)&31)*255/31,((p>>5)&63)*255/63,(p&31)*255/31};assert(fwrite(rgb,1,3,f)==3);}assert(!fclose(f));}
static void edit(void){tap(TCN_ROW,0);assert(screen_id==SCREEN_WEEK);tap(TCN_ROW,0);assert(screen_id==SCREEN_DAY);tap(TCN_ROW,0);assert(tcp_editor);}
static void controls(void){
 capture("weeks");tap(TCN_ROW,0);assert(screen_id==SCREEN_WEEK);capture("week");
 tap(TCN_ROW,0);assert(screen_id==SCREEN_DAY);capture("day");
 tc_day_t day=get_day(20261004);assert(worked(&day)==510);tap(TCN_ROW,0);assert(tcp_editor);capture("keyboard");
 unsigned initial=tcp_key_page;tap(TCN_KEY,PWK_PAGE);assert(tcp_key_page==(initial+1)%PWK_PAGES);tap(TCN_KEY,PWK_DELETE);
 strcpy(tcp_entry,"9:45 PM");frame();settle();tap(TCN_KEY,PWK_DONE);assert(!tcp_editor&&get_day(20261004).punches[0]==1305&&data_writes==1);
 tap(TCN_ROW,0);strcpy(tcp_entry,"7:30 AM");frame();settle();tap(TCN_BACK,0);assert(!tcp_editor&&get_day(20261004).punches[0]==1305&&data_writes==1);
 tap(TCN_BACK,0);assert(screen_id==SCREEN_WEEK);tap(TCN_ROW,DAY_COUNT);assert(data_writes==2&&get_day(20261004).punches[0]==480);
 for(unsigned i=0;i<12;i++){frame();settle();}assert(data_writes==2);
 puts("native weeks/day/edit/key pages/delete/done/cancel, current punch and unchanged worked-time arithmetic");
}
static void save_retry(void){
 edit();strcpy(tcp_entry,"9:45 AM");replace_result=RISC_APP_DATA_IO;frame();settle();tap(TCN_KEY,PWK_DONE);
 assert(tcp_editor&&!store_ready&&!data_writes&&get_day(20261004).punches[0]==480&&!strcmp(tcp_entry,"9:45 AM"));capture("save-error");
 replace_result=0;commit_error=true;tap(TCN_KEY,PWK_DONE);assert(tcp_editor&&!store_ready&&data_writes==1&&get_day(20261004).punches[0]==480);
 commit_error=false;tap(TCN_KEY,PWK_DONE);assert(!tcp_editor&&store_ready&&data_writes==2&&get_day(20261004).punches[0]==585);
 puts("native failed/commit-unknown save retains draft and old displayed snapshot; explicit reload/retry reconciles");
}
static void gestures(void){
 int i=find_control(TCN_ROW,0);tcn_rect r=tcn.controls[i].r;
 touch_x=r.x+8+inset;touch_y=r.y+r.h/2+inset;touch_down=true;pump();touch_x+=15;pump();touch_x-=15;pump();touch_down=false;pump();settle();assert(screen_id==SCREEN_WEEK_LIST);
 touch_down=true;pump();touch_gap=true;pump();touch_down=false;pump();settle();assert(screen_id==SCREEN_WEEK_LIST);
 unsigned before=tcn.first;touch_x=native_width/2;touch_y=tcn.bottom-3+inset;touch_down=true;pump();touch_y=tcn.top+1+inset;pump();touch_down=false;pump();settle();assert(tcn.first>before);
 while(tcn.first)tap(TCN_UP,0);
 tap(TCN_ROW,19);assert(screen_id==SCREEN_WEEK&&week_offset== -19);
 tap(TCN_BACK,0);assert(screen_id==SCREEN_WEEK_LIST);tap(TCN_ROW,19);tap(TCN_ROW,0);tap(TCN_ROW,0);assert(tcp_editor);
 nav_buttons=RISC_NAV_BACK;pump();settle();assert(!tcp_editor&&!data_writes);
 puts("native cancelled/held/out-and-back contacts, all 20 weeks reachable, crown cancels keyboard");
}
static void clock_failure(void){
 unset=true;frame();settle();assert(!tcp_clock_valid&&!data_writes);capture("clock-error");tap(TCN_SELECT,0);assert(!data_writes);
 unset=false;tap(TCN_SELECT,0);frame();settle();assert(tcp_clock_valid&&!data_writes);
 puts("native unavailable clock never writes or creates a 1970 record");
}
static void alarm_interrupt(void){
 edit();strcpy(tcp_entry,"7:45 PM");frame();settle();int i=find_control(TCN_KEY,PWK_DONE);tcn_rect r=tcn.controls[i].r;
 touch_x=r.x+r.w/2+inset;touch_y=r.y+r.h/2+inset;touch_down=true;pump();alarm_alert=true;alarm_dismiss_at=ticks+80;pump();assert(alarm_acks==1&&modal_frames&&!alarm_alert);
 touch_down=false;pump();settle();assert(tcp_editor&&!strcmp(tcp_entry,"7:45 PM")&&!data_writes&&portable_timecard_native_ready(NULL,NULL));
 tap(TCN_KEY,PWK_DONE);assert(!tcp_editor&&data_writes==1);
 puts("native Alarm interruption cancels held save, preserves draft and accepts only fresh post-return input");
}
static void quick_interrupt(void){
 edit();strcpy(tcp_entry,"7:45 PM");frame();settle();
 touch_x=native_width/2;touch_y=2;touch_down=true;pump();touch_y=native_height/2;quick_requested=true;quick_dismiss_at=ticks+160;pump();quick_requested=false;
 assert(quick_frames&&!quick_dismiss_at);touch_down=false;pump();settle();assert(tcp_editor&&!strcmp(tcp_entry,"7:45 PM")&&!data_writes);
 tap(TCN_KEY,PWK_DONE);assert(!tcp_editor&&data_writes==1);
 puts("native Quick actual top-edge drag/modal/dismiss restores draft and native readiness");
}
static unsigned wifi_phase,wifi_origin,wifi_polls;
static void drive_quick_wifi(void){
 assert(ticks-wifi_origin<10000);
 if(!wifi_polls++)return;
 if(ticks-wifi_origin<1000){touch_down=false;return;}
 if(!wifi_phase){
  pqa_state q;pqa_init(&q);assert(pqa_set_dimensions(&q,native_width,native_height));
  assert(pqa_set_safe_content(&q,inset,inset,native_width-2*inset,native_height-2*inset));
  pqa_layout layout=pqa_layout_for(&q);pqa_rect tile=layout.tiles[3];
  touch_x=tile.x+tile.w/2;touch_y=tile.y+tile.h/2;touch_down=true;wifi_phase=1;
 }else if(wifi_phase==1){wifi_phase=2;}else touch_down=false;
}
static void quick_wifi_handoff(void){
 edit();strcpy(tcp_entry,"7:45 PM");frame();settle();
 touch_x=native_width/2;touch_y=2;touch_down=true;pump();touch_y=native_height/2;
 quick_requested=true;wifi_origin=ticks;poll_driver=drive_quick_wifi;
 pump();
 for(unsigned i=0;!launches&&i<1000;i++)pump();
 poll_driver=NULL;quick_requested=false;
 assert(launches==1&&!strcmp(launched,"wifi_settings.elf")&&tcp_external_exit&&!data_writes);
 assert(tcp_editor&&!strcmp(tcp_entry,"7:45 PM"));
 puts("actual Quick Wi-Fi Setup routes to wifi_settings.elf; accepted handoff never saves draft ledger");
}
static void retained_failure(void){
 edit();stat_result=RISC_APP_DATA_RETAINED;tcp_editor_done();assert(tcp_retained());unsigned frozen=calls;
 frame();t5_app_input_t in={.buttons=T5_APP_BUTTON_CONFIRM};tcp_input(&in);tcp_dependencies_close();app_module_fini();assert(calls==frozen);
 puts("native retained storage forbids later display,input,release or launch calls");
}

static void keyboard_contract(void){
 edit();
 for(unsigned page=0;page<PWK_PAGES;page++){
  for(unsigned key=0;key<PWK_CHARACTERS;key++){
   tcp_entry[0]=0;frame();settle();unsigned ch=portable_watch_key_character(tcp_key_page,key);
   tap(TCN_KEY,key);assert(ch?strlen(tcp_entry)==1&&(unsigned char)tcp_entry[0]==ch:!tcp_entry[0]);
  }
  tap(TCN_KEY,PWK_PAGE);
 }
 strcpy(tcp_entry,"123456789012");frame();settle();tap(TCN_KEY,0);assert(strlen(tcp_entry)==12&&!data_writes);
 strcpy(tcp_entry,"9:99 PM");frame();settle();tap(TCN_KEY,PWK_DONE);assert(tcp_editor&&!data_writes&&strstr(tcp_editor_status,"Use H:MM"));
 tcp_entry[0]=0;frame();settle();tap(TCN_KEY,PWK_DONE);assert(!tcp_editor&&data_writes==1&&get_day(20261004).punches[0]== -1);
 puts("native shared keyboard all ASCII pages/cells, 12-character limit, invalid input refusal and blank clear");
}
static void storage_contract(void){
 read_result=RISC_APP_DATA_IO;assert(!tcp_reload());frame();settle();assert(!store_ready&&!data_writes);capture("read-error");
 tap(TCN_SELECT,0);assert(!store_ready&&!data_writes);read_result=0;tap(TCN_SELECT,0);assert(store_ready&&!data_writes);
 unsigned used=(unsigned)snprintf(history,sizeof(history),"{\"days\":[");
 for(unsigned i=0;i<MAX_DAYS;i++)used+=(unsigned)snprintf(history+used,sizeof(history)-used,"%s{\"d\":%d,\"in\":480}",i?",":"",add_days(20200101,(int)i));
 snprintf(history+used,sizeof(history)-used,"]}");data_revision++;assert(tcp_reload()&&day_count==MAX_DAYS);
 open_week(0);frame();settle();tap(TCN_ROW,DAY_COUNT);assert(!data_writes&&day_count==MAX_DAYS&&strstr(status_text,"400"));
 open_day(20200101);selected=0;tcp_activate();assert(tcp_editor);tcp_entry[0]=0;frame();settle();tap(TCN_KEY,PWK_DONE);assert(data_writes==1&&day_count==MAX_DAYS-1);
 tcp_punch(0);frame();settle();assert(data_writes==2&&day_count==MAX_DAYS&&get_day(20261004).punches[0]==480);
 puts("native read failure/retry, 400-day no eviction, empty-day reclamation and new punch");
}
static void pending_layout(void){
 edit();strcpy(tcp_entry,"7:45 PM");frame();settle();int i=find_control(TCN_KEY,PWK_DONE);tcn_rect r=tcn.controls[i].r;
 touch_x=r.x+r.w/2+inset;touch_y=r.y+r.h/2+inset;touch_down=true;pump();tcp_key_page=(tcp_key_page+1)%PWK_PAGES;frame();
 touch_down=false;pump();settle();assert(tcp_editor&&!data_writes&&!strcmp(tcp_entry,"7:45 PM"));
 tap(TCN_KEY,PWK_DONE);assert(!tcp_editor&&data_writes==1);
 puts("native changed pending hit map cancels held save until a fresh visible press/release");
}
static void time_contract(void){
 tcp_time_format=PORTABLE_TIME_FORMAT_24;open_day(20261004);frame();settle();char value[32];tcp_display_time("8:00 AM",value,sizeof(value));assert(!strcmp(value,"8:00"));capture("day-24h");
 rtc_value=(twatch_rtc_time_v1){2026,12,31,4,23,59,59};epoch=(int64_t)serial_day(20261231)*86400+86399;
 tcp_punch(0);assert(get_day(20261231).punches[0]==1439);
 rtc_value=(twatch_rtc_time_v1){2027,1,1,5,0,0,0};epoch++;
 tcp_punch(3);assert(get_day(20270101).punches[3]==0&&get_day(20261231).punches[0]==1439);
 tcp_time_format=PORTABLE_TIME_FORMAT_12;tcp_display_time("8:00 AM",value,sizeof(value));assert(!strcmp(value,"8:00 AM"));
 puts("native 12/24-hour preference presentation and fresh year-midnight punch snapshot");
}

static unsigned driver_stage,driver_edge;
static void drive_app(void){
 assert(ticks<30000);
 if(driver_edge==1){touch_down=false;driver_edge=2;return;}
 if(!portable_timecard_native_ready(NULL,NULL))return;
 if(driver_edge==2){driver_edge=0;driver_stage++;return;}
 unsigned action=driver_stage<3?TCN_ROW:driver_stage==3?TCN_KEY:TCN_BACK;
 unsigned arg=driver_stage==3?PWK_DONE:0;if(driver_stage>6)return;
 int i=find_control(action,arg);assert(i>=0);tcn_rect r=tcn.controls[i].r;touch_x=r.x+r.w/2+inset;touch_y=r.y+r.h/2+inset;touch_down=true;driver_edge=1;
}
static void actual_navigation(void){
 assert(app_module_init()==0);poll_driver=drive_app;app_main();poll_driver=NULL;
 assert(tcn.active&&screen_id==SCREEN_WEEK_LIST&&launches==1&&data_writes==1&&driver_stage>=6);app_module_fini();assert(!live&&!subs&&!frames&&!retained);
 puts("actual app_main native raw weeks/day/keyboard/Done/nested/root Back and clean exit");
}
int main(int argc,char**argv){
 assert(argc==5);native_width=(unsigned)atoi(argv[1]);native_height=(unsigned)atoi(argv[2]);unsigned scenario=(unsigned)atoi(argv[3]);latency=(unsigned)atoi(argv[4]);assert(native_width<=640&&native_height<=640);inset=native_width==466&&native_height==466?68:0;seed();
 if(scenario==7){actual_navigation();return 0;}
 if(scenario==8){for(unsigned n=0;n<3;n++){assert(app_module_init()==0);home_requested=true;home_polls=0;app_main();assert(tcn.active&&launches==n+1&&presents);app_module_fini();assert(!live&&!subs&&!frames&&!retained&&!data_writes);event_head=event_count=0;}puts("actual app_main physical Home and repeated native invocation");return 0;}
 begin();switch(scenario){case 0:controls();break;case 1:save_retry();break;case 2:gestures();break;case 3:clock_failure();break;case 4:alarm_interrupt();break;case 5:quick_interrupt();break;case 6:retained_failure();return 0;case 9:keyboard_contract();break;case 10:storage_contract();break;case 11:pending_layout();break;case 12:time_contract();break;case 13:quick_wifi_handoff();break;default:assert(0);}end();
 printf("PASS Timecard native %ux%u scenario%u latency%u frames%u\n",native_width,native_height,scenario,latency,presents);return 0;
}
