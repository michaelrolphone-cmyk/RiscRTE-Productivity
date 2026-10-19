/* Production Points controller, native raster and separately linked adapter.
 * Runtime/display/storage doubles derive from points_native_adapter_test.c. */
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
static uint16_t pixels[413*502];
static unsigned native_width=240,native_height=240;
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
static bool fx_launch(const char *s){fx_io();assert(!strcmp(s,"springboard.elf")||!strcmp(s,"default.elf"));launches++;return true;}
static bool fx_retain(void){assert(!retained);retained=true;barriers++;return true;}
static bool fx_info(void*c,risc_display_info_v1*s){(void)c;fx_io();*s=(risc_display_info_v1){.width=native_width,.height=native_height,.supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565),.flags=RISC_DISPLAY_INFO_ASYNC_PRESENT|RISC_DISPLAY_INFO_PARTIAL_DAMAGE,.nominal_refresh_millihz=60000};return true;}
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
 if(kv_result&&(unsigned)(uintptr_t)c==5)return kv_result;
 for(unsigned i=0;i<16;i++)if(cells[i].instance==(unsigned)(uintptr_t)c&&!strcmp(cells[i].key,k)){assert(cap>=cells[i].size);memcpy(b,cells[i].bytes,cells[i].size);*size=cells[i].size;return 0;}
 return RISC_KEY_VALUE_NOT_FOUND;
}
static int32_t fx_put(void*c,const char*k,const void*b,uint32_t size){
 fx_io();assert((unsigned)(uintptr_t)c==5&&(!strcmp(k,POINTS_CONFIG_KEY)||!strcmp(k,POINTS_META_KEY)));puts_count++;if(kv_result&&(unsigned)(uintptr_t)c==5)return kv_result;
 unsigned i;for(i=0;i<16&&cells[i].key[0]&&strcmp(cells[i].key,k);i++){}assert(i<16&&size<=64);cells[i].instance=(unsigned)(uintptr_t)c;strcpy(cells[i].key,k);memcpy(cells[i].bytes,b,size);cells[i].size=size;return 0;
}
static const risc_key_value_v1 fx_prefs={1,sizeof(fx_prefs),(void*)1,fx_get,fx_put};
static const risc_key_value_v1 fx_store={1,sizeof(fx_store),(void*)5,fx_get,fx_put};
static int32_t fx_native(void*c,risc_realtime_snapshot_v1*s){(void)c;fx_io();native_samples++;if(native_result)return native_result;*s=(risc_realtime_snapshot_v1){.struct_size=sizeof(*s),.validity=unset?RISC_REALTIME_UNSET:RISC_REALTIME_VALID,.epoch_seconds=unset?0:1791115200,.monotonic_before_us=1,.monotonic_after_us=2};return 0;}
static const risc_realtime_api_v1 fx_native_api={1,sizeof(fx_native_api),(void*)1,fx_native};
static int32_t fx_alarm_status(void*c,alarm_status_v1*s){(void)c;fx_io();if(alarm_result)return alarm_result;*s=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*s),.state=alarm_alert?ALARM_STATE_ALERT:ALARM_STATE_READY,.mode=ALARM_MODE_BOTH,.occurrence=alarm_alert?(alarm_token_v1){.kind=ALARM_KIND_ALARM,.revision=1,.deadline=1,.generation=1}:(alarm_token_v1){0}};return 0;}
static int32_t fx_alarm_step(void*c){(void)c;fx_io();return alarm_result;}
static int32_t fx_alarm_ack(void*c,const alarm_token_v1*t){assert(t->generation==1);alarm_acks++;alarm_alert=false;return fx_alarm_step(c);}
static int32_t fx_alarm_prepare(void*c,alarm_sleep_v1*s){(void)s;return fx_alarm_step(c);}
static alarm_service_descriptor_v2 fx_alarm={.base={2,sizeof(fx_alarm),NULL,fx_alarm_status,fx_alarm_step,fx_alarm_step,fx_alarm_ack,fx_alarm_prepare,fx_alarm_step},.tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=1,.output_modes=3};
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
 else {assert(v==1);if(!strcmp(n,"display.output"))api=&fx_display;else if(!strcmp(n,"input.touch.raw"))api=&fx_touch;else if(!strcmp(n,"board.battery"))api=&fx_gauge;else if(!strcmp(n,"input.navigation"))api=&fx_nav;else if(!strcmp(n,"storage.key-value")){assert(instance==1||instance==5);api=instance==1?&fx_prefs:&fx_store;}else if(!strcmp(n,RISC_REALTIME_CAPABILITY)){assert(!native_live);native_live=1;api=&fx_native_api;}else return false;}
#ifdef TEST_PRODUCTIVITY_IDLE_FIXTURE
 if(api==&fx_broadcast)idle_broadcast_grants++;
#endif
 if(!api)return false;
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
static bool exited;
static void check_layout(void){
 assert(pn.count<=64);
 for(unsigned i=0;i<pn.count;i++){
  pn_rect r=pn.controls[i].r;assert(r.x>=0&&r.y>=0&&r.w>0&&r.h>0&&r.x+r.w<=pn.w&&r.y+r.h<=pn.h);
  for(unsigned j=0;j<i;j++){pn_rect q=pn.controls[j].r;assert(r.x+r.w<=q.x||q.x+q.w<=r.x||r.y+r.h<=q.y||q.y+q.h<=r.y);}
 }
}
static void frame(void){draw();check_layout();}
static void pump(void){
 t5_app_input_t in={0};assert(app->poll(&in,20));
 bool changed=pn_input(&in);
 if(in.buttons&T5_APP_BUTTON_BACK){exited=on_back();changed=true;}
 if(changed||pn.dirty||portable_points_native_repaint())frame();
}
static void settle(void){for(unsigned i=0;i<40;i++)pump();assert(portable_points_native_ready(NULL,NULL));}
static void begin(void){
 for(unsigned y=0;y<native_height;y++)for(unsigned x=0;x<native_width+3;x++)pixels[y*(native_width+3)+x]=x<native_width?0:0xa55a;
 assert(!live&&!subs&&!frames);assert(app_module_init()==0);app=t5_app_get_api(1);paper=paper_presentation_get();assert(!paper);
 assert(portable_points_native_available());assert(pn_open(app->screen_width(),app->screen_height()));
 ready=open_dependencies();assert(ready);load_catalog();assert(writer.loaded&&clock_valid);page=PAGE_LIST;notice="";time_format=PORTABLE_TIME_FORMAT_24;
 frame();
#ifdef PORTABLE_RASTER_SNAPSHOT
 /* A real snapshot seals without acquiring/presenting a native frame. */
 assert(!presents&&!frames);
#else
 assert(presents);
#endif
 settle();
}
static void end(void){close_dependencies();app_module_fini();assert(!live&&!subs&&!frames&&!retained);for(unsigned y=0;y<native_height;y++)for(unsigned x=native_width;x<native_width+3;x++)assert(pixels[y*(native_width+3)+x]==0xa55a);}
static void tap_at(int x,int y){touch_x=(uint16_t)x;touch_y=(uint16_t)y;touch_down=true;pump();touch_down=false;pump();settle();}
static int find_control(unsigned action,unsigned arg){for(unsigned i=0;i<pn.count;i++)if(pn.controls[i].action==action&&pn.controls[i].arg==arg)return (int)i;return -1;}
static void tap(unsigned action,unsigned arg){
 int index=find_control(action,arg);
 for(unsigned n=0;index<0&&n<40;n++){
  int scroll=find_control(PN_SCROLL_DOWN,0);if(scroll<0)fprintf(stderr,"No scroll action%u arg%u page%u first%u total%u uncertain%d meta%d\n",action,arg,page,pn.first,pn.total,writer.uncertain,writer.meta_uncertain);assert(scroll>=0);pn_rect r=pn.controls[scroll].r;tap_at(r.x+r.w/2,r.y+r.h/2);index=find_control(action,arg);
 }
 if(index<0){fprintf(stderr,"Missing action %u arg %u page %u first %u total %u\n",action,arg,page,pn.first,pn.total);abort();}
 pn_rect r=pn.controls[index].r;tap_at(r.x+r.w/2,r.y+r.h/2);
}
static void top(void){while(pn.first)tap(PN_SCROLL_UP,0);}
static void capture(const char *name){const char *dir=getenv("POINTS_NATIVE_FRAMES");if(!dir)return;char path[512];snprintf(path,sizeof(path),"%s/%ux%u-%s.ppm",dir,native_width,native_height,name);FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%u %u\n255\n",native_width,native_height);for(unsigned y=0;y<native_height;y++)for(unsigned x=0;x<native_width;x++){uint16_t p=pixels[y*(native_width+3)+x];unsigned char rgb[]={((p>>11)&31)*255/31,((p>>5)&63)*255/63,(p&31)*255/31};fwrite(rgb,1,3,f);}assert(!fclose(f));}
static void controls(void){
 capture("list");tap(PN_EDIT,0);assert(page==PAGE_EDIT);capture("edit");
 tap(PN_TIME,0);assert(page==PAGE_TIME);capture("time");
 draft.hour=23;draft.minute=59;frame();settle();tap(PN_PICK,2);tap(PN_PICK,5);assert(!draft.hour&&!draft.minute);
 time_format=PORTABLE_TIME_FORMAT_12;frame();settle();tap(PN_AMPM,0);assert(draft.hour==12);tap(PN_BACK,0);assert(page==PAGE_EDIT);
 tap(PN_DURATION,0);assert(page==PAGE_DURATION);draft.duration_minutes=719;frame();settle();tap(PN_PICK,2);assert(draft.duration_minutes==719);tap(PN_PICK,5);assert(draft.duration_minutes==719);
 draft.duration_minutes=660;frame();settle();tap(PN_PICK,2);assert(draft.duration_minutes==720);tap(PN_PICK,5);assert(draft.duration_minutes==720);
 draft.duration_minutes=1;draft.notify_end=draft.warn3=1;frame();settle();tap(PN_PICK,3);assert(!draft.duration_minutes&&!draft.notify_end&&!draft.warn3);
 draft.duration_minutes=30;tap(PN_BACK,0);top();tap(PN_END,0);tap(PN_WARN,0);assert(draft.notify_end&&draft.warn3);
 tap(PN_MODE,0);assert(page==PAGE_MODE);for(unsigned mode=0;mode<4;mode++){top();tap(PN_MODE,mode+1);assert(page==PAGE_EDIT&&draft.mode==mode);if(mode<3)tap(PN_MODE,0);}
 top();tap(PN_PRESET,62);assert(draft.weekdays==62);top();tap(PN_DAY,0);assert(draft.weekdays==63);tap(PN_PRESET,65);assert(draft.weekdays==65);tap(PN_ENABLED,0);unsigned enabled=draft.enabled;
 tap(PN_SAVE,0);assert(page==PAGE_LIST&&!writer.uncertain&&writer.saved.points[0].enabled==enabled&&puts_count==2);
 tap(PN_EDIT,0);tap(PN_DELETE,0);assert(nova_delete_confirm&&writer.saved.points[0].kind);tap(PN_DELETE,0);assert(page==PAGE_LIST&&!writer.saved.points[0].kind);
 puts("native controls: time/duration bounds, notification modes, weekdays, enable, save, confirmed delete");
}
static void custom(void){
 tap(PN_EDIT,0);tap(PN_TYPE,0);tap(PN_CUSTOM,POINTS_CUSTOM_1);assert(page==PAGE_CUSTOM);tap(PN_NAME,0);assert(page==PAGE_CUSTOM_KEYBOARD);capture("keyboard");
 unsigned before=(unsigned)strlen(p7_key_text);tap(PN_KEY,0);assert(strlen(p7_key_text)==before+1||before==POINTS_CUSTOM_NAME_MAX);
 tap(PN_KEY,P7_WATCH_KEY_PAGE);assert(custom_key_page==0);tap(PN_KEY,P7_WATCH_KEY_DELETE);tap(PN_KEY,P7_WATCH_KEY_DONE);assert(page==PAGE_CUSTOM);
 tap(PN_COLOR,7);assert(custom_draft.custom[0].color==7);kv_result=RISC_KEY_VALUE_IO;tap(PN_CUSTOM_SAVE,0);assert(writer.meta_uncertain);capture("uncertain");
 unsigned saves=puts_count;tap(PN_BACK,0);assert(!exited&&writer.meta_uncertain&&puts_count==saves);kv_result=0;tap(PN_RETRY,0);assert(!writer.meta_uncertain&&writer.meta.custom[0].color==7);
 puts("native custom type: keyboard/page/delete/done/color, metadata uncertainty, guarded Back, retry");
}
static void save_retry(void){
 tap(PN_EDIT,0);draft.hour=5;kv_result=RISC_KEY_VALUE_IO;tap(PN_SAVE,0);assert(writer.uncertain&&writer.meta_uncertain);
 unsigned saves=puts_count;tap(PN_BACK,0);assert(!exited&&writer.uncertain&&puts_count==saves);kv_result=0;tap(PN_RETRY,0);assert(!writer.uncertain&&!writer.meta_uncertain&&writer.saved.points[0].hour==5);
 puts("native config: uncertain metadata/config retry preserves pending bytes and Back guard");
}
static void gestures(void){
 int i=find_control(PN_EDIT,0);assert(i>=0);pn_rect r=pn.controls[i].r;
 touch_x=r.x+10;touch_y=r.y+r.h/2;touch_down=true;pump();touch_x+=20;pump();touch_x-=20;pump();touch_down=false;pump();settle();assert(page==PAGE_LIST);
 tap(PN_EDIT,0);tap(PN_TIME,0);draft.hour=10;frame();settle();i=find_control(PN_PICK,1);r=pn.controls[i].r;
 touch_x=r.x+r.w/2;touch_y=r.y+r.h/2;touch_down=true;pump();assert(p7_picker.active);
 touch_y-=(pn.bottom-pn.top)/3;pump();assert(draft.hour==11&&p7_picker.active);touch_down=false;pump();settle();assert(draft.hour==11&&!p7_picker.active);
 /* A released half-row uses the same rounding and settling as shared picker. */
 touch_y=r.y+r.h/2;touch_down=true;pump();touch_y-=(pn.bottom-pn.top)/6+1;pump();touch_down=false;pump();settle();assert(draft.hour==12&&!p7_picker.offset);
 nav_buttons=RISC_NAV_RIGHT;pump();assert(draft.minute==31&&p7_picker.column==1);
 nav_buttons=RISC_NAV_LEFT;pump();assert(draft.hour==11&&p7_picker.column==0);
 nav_buttons=RISC_NAV_DOWN;pump();assert(draft.hour==12);
 /* An explicit raw queue GAP cannot round a pending partial wheel movement. */
 touch_y=r.y+r.h/2;touch_down=true;pump();touch_y-=(pn.bottom-pn.top)/6;pump();unsigned hour=draft.hour;
 touch_gap=true;touch_down=false;touch_state.contact_count=0;pump();settle();assert(draft.hour==hour&&!p7_picker.active&&!p7_picker.offset);
 /* A new hit map cancels contact and cannot activate the replacement controls. */
 touch_y=r.y+r.h/2;touch_down=true;pump();page=PAGE_EDIT;frame();touch_down=false;pump();settle();assert(page==PAGE_EDIT&&draft.hour==12);
 puts("native gestures: horizontal out-and-back, full/half shared-picker rows, settling and new-map cancellation");
}
static void policy(void){
 tap(PN_EDIT,0);unsigned old=draft.mode;fx_alarm.output_modes=ALARM_MODE_VISUAL;tap(PN_MODE,0);assert(page==PAGE_EDIT&&draft.mode==old);fx_alarm.output_modes=3;
 draft.weekdays=0;tap(PN_SAVE,0);assert(page==PAGE_EDIT&&!puts_count&&!writer.uncertain);draft.weekdays=127;
 unset=true;tap(PN_SAVE,0);assert(page==PAGE_EDIT&&!puts_count&&!writer.uncertain);unset=false;
 tap(PN_BACK,0);assert(page==PAGE_LIST);tap(PN_NEW,0);assert(page==PAGE_EDIT&&draft.kind);tap(PN_BACK,0);assert(page==PAGE_LIST&&!puts_count);
 puts("native policy: visual-only saved mode, zero-weekday/invalid-clock guards and add/cancel");
}
static unsigned driver_stage,driver_edge;
static void drive_app(void){
 assert(ticks<30000);
 if(driver_edge==1){touch_down=false;driver_edge=2;return;}
 if(!portable_points_native_ready(NULL,NULL))return;
 if(driver_edge==2){driver_edge=0;driver_stage++;return;}
 unsigned action=driver_stage==0?PN_EDIT:PN_BACK;
 if(driver_stage>2)return;
 assert(page==(driver_stage==1?PAGE_EDIT:PAGE_LIST));
 int i=find_control(action,0);assert(i>=0);pn_rect r=pn.controls[i].r;
 touch_x=r.x+r.w/2;touch_y=r.y+r.h/2;touch_down=true;driver_edge=1;
}
static void actual_navigation(void){
 assert(app_module_init()==0);poll_driver=drive_app;app_main();poll_driver=NULL;
 assert(pn.active&&!nova&&page==PAGE_LIST&&launches==1&&!puts_count&&driver_stage>=2);
 app_module_fini();assert(!live&&!subs&&!frames&&!retained);
 puts("native actual app_main: raw edit, nested Back, root Back and clean exit");
}
static void alarm_interrupt(void){
 tap(PN_EDIT,0);unsigned old_hour=draft.hour,old_page=page;
 int i=find_control(PN_TIME,0);pn_rect r=pn.controls[i].r;touch_x=r.x+r.w/2;touch_y=r.y+r.h/2;touch_down=true;pump();
 alarm_alert=true;alarm_dismiss_at=ticks+80;pump();assert(alarm_acks==1&&modal_frames&&!alarm_alert);
 touch_down=false;pump();settle();assert(page==old_page&&draft.hour==old_hour&&!puts_count&&portable_points_native_ready(NULL,NULL));
 tap(PN_TIME,0);assert(page==PAGE_TIME);
 puts("native Alarm: real modal draw, consumed hardware dismissal, held-contact cancellation, native repaint and fresh navigation");
}
#ifdef PORTABLE_QUICK_ACTIONS
static void quick_interrupt(void){
 tap(PN_EDIT,0);unsigned old_hour=draft.hour;
 touch_x=native_width/2;touch_y=5;touch_down=true;pump();
 touch_y=native_height/2;quick_requested=true;quick_dismiss_at=ticks+160;pump();quick_requested=false;
 assert(quick_frames&&!quick_dismiss_at);
 touch_down=false;pump();settle();assert(page==PAGE_EDIT&&draft.hour==old_hour&&!puts_count&&!exited);
 tap(PN_TIME,0);assert(page==PAGE_TIME);
 puts("native Quick: actual top-edge drag, modal raster, consumed Back, held-contact cancellation, restored readiness and fresh navigation");
}
#endif
static void retained_failure(void){native_result=RISC_REALTIME_CONTEXT;uint32_t now;assert(!read_clock(&now)&&retained);unsigned frozen=calls;frame();save_action();retry_action();close_dependencies();app_module_fini();assert(calls==frozen);puts("native retention: no post-retention calls");}
int main(int argc,char**argv){
 assert(argc==4);native_width=(unsigned)atoi(argv[1]);native_height=(unsigned)atoi(argv[2]);unsigned scenario=(unsigned)atoi(argv[3]);
 if(scenario==7){actual_navigation();return 0;}
 if(scenario==5){for(unsigned n=0;n<2;n++){assert(app_module_init()==0);home_requested=true;home_polls=0;app_main();assert(pn.active&&!nova&&launches==n+1&&presents);app_module_fini();assert(!live&&!subs&&!frames&&!retained);event_head=event_count=0;}puts("native actual app_main: RGB selection, Home exit and repeated invocation");return 0;}
 begin();switch(scenario){case 0:controls();break;case 1:custom();break;case 2:save_retry();break;case 3:gestures();break;case 4:retained_failure();return 0;case 6:policy();break;case 8:alarm_interrupt();break;
#ifdef PORTABLE_QUICK_ACTIONS
 case 9:quick_interrupt();break;
#endif
 default:assert(0);}end();
 printf("PASS Points native %ux%u scenario%u frames%u\n",native_width,native_height,scenario,presents);return 0;
}
