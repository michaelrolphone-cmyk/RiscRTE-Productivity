/* Actual production Points app_main plus the separately compiled production
 * portable adapter. Fake providers follow the existing System-Apps
 * test/native_apps/nova_peripherals.h contract; raw DOWN/MOVE/UP and snapshots
 * drive every interaction. UI handlers are never called by this fixture. */
#include "PortableApps.h"
#include "RiscDisplayOutputV1.h"
#include "RiscTouchV1.h"
#include "RiscBatteryGaugeV1.h"
#include "PortableRtcClock.h"
#include "RiscKeyValueV1.h"
#include "PortableNavigation.h"
#include "PortableAppSleep.h"
#include "AlarmServiceV1.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../Apps/points_in_time.c"
int app_module_init(void);void app_module_fini(void);
const t5_app_manifest_t portable_catalog[1]={{.compatible=false}};
const unsigned portable_catalog_count=0;
static unsigned ticks,polls,fixture_grants,frames,subs,presents;
static unsigned stop_poll=120;
static uint16_t pixels[240*244];
static unsigned writes;
static struct {bool seen,active;unsigned page,scroll,hour,minute;uint32_t hash;} shown[256];
static bool contact_down,event_pending;
static int contact_x,contact_y;
static risc_touch_event_v1 touch_event;
static struct {unsigned at;int x,y;} actions[256];static unsigned action_count;
static struct {char key[16];unsigned char bytes[64];uint32_t size;} cells[32];
static void save_frame(void) {
 assert(polls<256);uint32_t hash=2166136261u;
 for(unsigned y=0;y<240;y++) {
  for(unsigned x=0;x<240;x++){hash^=pixels[y*244+x];hash*=16777619u;}
  for(unsigned x=240;x<244;x++)assert(pixels[y*244+x]==0xa5a5);
 }
 shown[polls].seen=true;shown[polls].active=p7_picker_active()||p7_contact_active;
 shown[polls].page=page;shown[polls].scroll=nova_list_scroll;
 shown[polls].hour=draft.hour;shown[polls].minute=draft.minute;shown[polls].hash=hash;
}
static bool fake_health(risc_runtime_health_v1*h){h->uptime_ms=ticks;return polls<stop_poll;}
static void fake_yield(uint32_t n){ticks+=n;}
static bool fake_diag(const char*s){fprintf(stderr,"%s\n",s);return true;}
static bool fake_launch(const char*s){printf("launch=%s\n",s);stop_poll=polls;return true;}
static bool fake_info(void*c,risc_display_info_v1*s){(void)c;*s=(risc_display_info_v1){.width=240,.height=240,.nominal_refresh_millihz=60000,.typical_present_latency_us=16000,.supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565)};return true;}
static bool fake_frame(void*c,uint32_t f,risc_display_surface_v1*s){(void)c;assert(!frames);frames=1;*s=(risc_display_surface_v1){.frame=1,.pixels=pixels,.width=240,.height=240,.stride_bytes=488,.size_bytes=sizeof(pixels),.pixel_format=f};return true;}
static void fake_frame_release(void*c,risc_display_frame_v1 f){(void)c;assert(frames&&f==1);frames=0;}
static bool fake_submit(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*t){(void)c;(void)r;(void)n;(void)o;assert(frames&&f==1);frames=0;*t=++presents;save_frame();return true;}
static bool fake_present(void*c,risc_display_present_token_v1 t,risc_display_present_status_v1*s){(void)c;assert(t);s->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;}
static const risc_display_output_api_v1 display_api={.api_version=1,.struct_size=sizeof(display_api),.get_info=fake_info,.acquire=fake_frame,.release=fake_frame_release,.submit=fake_submit,.present_status=fake_present};
static uint64_t fake_sub(void*c){(void)c;subs++;return 1;}
static bool fake_unsub(void*c,uint64_t n){(void)c;assert(n==1&&subs);subs--;return true;}
static bool fake_touch_poll(void*c,size_t n){
 (void)c;assert(n==1);polls++;bool down=false;int x=contact_x,y=contact_y;
 for(unsigned i=0;i<action_count;i++)if(actions[i].at==polls){down=true;x=actions[i].x;y=actions[i].y;}
 unsigned kind=down?(contact_down?RISC_TOUCH_EVENT_MOVE:RISC_TOUCH_EVENT_DOWN):RISC_TOUCH_EVENT_UP;
 event_pending=(down!=contact_down)||(down&&(x!=contact_x||y!=contact_y));
 touch_event=(risc_touch_event_v1){.sequence=polls,.timestamp_ms=ticks,.kind=(uint8_t)kind,.id=1,.x=(uint16_t)x,.y=(uint16_t)y};
 contact_down=down;contact_x=x;contact_y=y;return true;
}
static int32_t fake_next(void*c,uint64_t n,risc_touch_event_v1*e){(void)c;assert(n==1);if(!event_pending)return 0;*e=touch_event;event_pending=false;return 1;}
static bool fake_snapshot(void*c,risc_touch_snapshot_v1*s){(void)c;*s=(risc_touch_snapshot_v1){.width=240,.height=240};if(contact_down){s->contact_count=1;s->contacts[0]=(risc_touch_contact_v1){.id=1,.x=(uint16_t)contact_x,.y=(uint16_t)contact_y};}return true;}
static const risc_touch_api_v1 touch_api={1,sizeof(touch_api),NULL,fake_sub,fake_unsub,fake_touch_poll,fake_next,fake_snapshot};
static bool fake_battery(void*c,risc_battery_sample_v1*s){(void)c;*s=(risc_battery_sample_v1){.percent=73,.millivolts=3970,.flags=RISC_BATTERY_CHARGING};return true;}
static const risc_battery_gauge_api_v1 battery_api={1,sizeof(battery_api),NULL,fake_battery};
static bool fake_rtc(void*c,twatch_rtc_time_v1*s){(void)c;*s=(twatch_rtc_time_v1){2026,10,4,0,20,34,12};return true;}
static bool fake_write(void*c,const twatch_rtc_time_v1*s){(void)c;(void)s;assert(!"Unexpected RTC write in read-only audit");return false;}
static const twatch_rtc_api_v1 rtc_api={.api_version=2,.struct_size=sizeof(rtc_api),.read=fake_rtc,.write=fake_write};
static int32_t fake_get(void*c,const char*k,void*b,uint32_t cap,uint32_t*s){(void)c;*s=0;for(unsigned i=0;i<32;i++)if(!strcmp(k,cells[i].key)){*s=cells[i].size;if(cap<*s)return RISC_KEY_VALUE_BUFFER_SMALL;memcpy(b,cells[i].bytes,*s);return 0;}return RISC_KEY_VALUE_NOT_FOUND;}
static int32_t fake_put(void*c,const char*k,const void*b,uint32_t n){(void)c;writes++;assert(n<=64&&strlen(k)<16);unsigned i;for(i=0;i<32&&cells[i].key[0]&&strcmp(k,cells[i].key);i++);assert(i<32);strcpy(cells[i].key,k);memcpy(cells[i].bytes,b,n);cells[i].size=n;return 0;}
static const risc_key_value_v1 kv_api={1,sizeof(kv_api),NULL,fake_get,fake_put};
static int32_t fake_alarm_status(void*c,alarm_status_v1*s){(void)c;*s=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*s),.state=ALARM_STATE_READY,.mode=ALARM_MODE_BOTH};return ALARM_OK;}
static int32_t fake_alarm_step(void*c){(void)c;return ALARM_OK;}
static int32_t fake_alarm_ack(void*c,const alarm_token_v1*t){(void)c;(void)t;return ALARM_OK;}
static int32_t fake_alarm_prepare(void*c,alarm_sleep_v1*s){(void)c;*s=(alarm_sleep_v1){.struct_size=sizeof(*s)};return ALARM_OK;}
static const alarm_service_v1 alarm_api={1,sizeof(alarm_api),NULL,fake_alarm_status,fake_alarm_step,fake_alarm_step,fake_alarm_ack,fake_alarm_prepare,fake_alarm_step};
static bool fake_nav(void*c,risc_input_navigation_frame_v1*s){(void)c;*s=(risc_input_navigation_frame_v1){0};return true;}
static bool fake_foreground(void*c,const risc_input_foreground_v1*s,size_t n){(void)c;(void)s;(void)n;return true;}
static bool fake_reset(void*c){(void)c;return true;}
static const risc_input_navigation_api_v1 nav_api={1,sizeof(nav_api),NULL,fake_nav,fake_foreground,fake_reset};
const risc_input_navigation_api_v1 *portable_input_navigation_open(const risc_runtime_api_v1*r){(void)r;return &nav_api;}
void portable_input_navigation_close(const risc_runtime_api_v1*r){(void)r;}
int portable_app_alarm_sleep(const risc_runtime_api_v1*r,const risc_display_output_api_v1*d,const risc_battery_gauge_api_v1*b,const alarm_service_v1*a){(void)r;(void)d;(void)b;(void)a;assert(!"Unexpected hardware sleep in audit");return 0;}
static bool fake_acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){(void)id;assert(g->struct_size==sizeof(*g));if(!strcmp(n,"display.output")&&v==1)g->api=&display_api;else if(!strcmp(n,"input.touch.raw")&&v==1)g->api=&touch_api;else if(!strcmp(n,"board.battery")&&v==1)g->api=&battery_api;else if(!strcmp(n,"rtc.clock")&&v==2)g->api=&rtc_api;else if(!strcmp(n,"storage.key-value")&&v==1)g->api=&kv_api;else if(!strcmp(n,"alarm.service")&&v==1)g->api=&alarm_api;else return false;fixture_grants++;return true;}
static bool fake_release(risc_runtime_capability_v1*g){assert(g->api&&fixture_grants);g->api=NULL;fixture_grants--;return true;}
static const risc_runtime_api_v1 runtime_api={1,sizeof(runtime_api),fake_health,fake_yield,fake_diag,fake_launch,fake_acquire,fake_release};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1?&runtime_api:NULL;}
static void sample(unsigned at,int x,int y){assert(action_count<256);actions[action_count].at=at;actions[action_count].x=x;actions[action_count++].y=y;}
static void seed(unsigned count) {
 points_config config={.revision=1,.created=1000};
 for(unsigned i=0;i<count;i++)config.points[i]=(points_item){.kind=POINTS_CUSTOM_1,.enabled=1,.mode=3,.weekdays=127,.hour=10,.minute=25,.duration_minutes=45,.notify_end=1,.warn3=1};
 uint8_t bytes[POINTS_RECORD_SIZE];points_config_encode(&config,bytes);fake_put(NULL,POINTS_CONFIG_KEY,bytes,sizeof(bytes));
 points_meta meta={.revision=1};strcpy(meta.custom[0].name,"Name");meta.custom[0].color=6;points_meta_encode(&meta,bytes);fake_put(NULL,POINTS_META_KEY,bytes,sizeof(bytes));writes=0;
}
int main(int argc,char**argv) {
 assert(argc==2);unsigned scenario=(unsigned)strtoul(argv[1],NULL,10);assert(scenario<3);
 memset(pixels,0xa5,sizeof(pixels));stop_poll=60;seed(scenario==1?8:1);
 if(scenario==0) {
  sample(3,80,100);sample(6,170,142); /* List -> editor -> time. */
  sample(10,170,146);sample(11,70,110);sample(12,70,110); /* Cross into hours while minute is captured. */
 } else if(scenario==1) {
  sample(3,170,210);sample(4,170,195);sample(5,170,174);sample(6,170,174);
 } else {
  sample(3,80,100);sample(6,170,100); /* List -> editor -> type. */
  sample(9,150,195);sample(10,150,35);sample(11,150,35); /* Scroll existing custom type into view. */
  sample(14,190,130);sample(17,60,103); /* Edit custom -> keyboard. */
  sample(20,45,80);sample(23,190,190); /* a -> DONE; still only draft. */
  sample(26,60,103);sample(29,72,80);sample(32,20,20); /* b -> Back: discard keyboard change. */
  sample(35,175,220); /* Explicit metadata Save. */
 }
 assert(app_module_init()==0);app_main();app_module_fini();
 assert(!fixture_grants&&!frames&&!subs);
 if(scenario==0) {
  assert(page==PAGE_TIME && draft.hour==10 && draft.minute==26 && !writes);
  assert(shown[10].seen&&shown[10].page==PAGE_TIME&&shown[10].active);
  assert(shown[11].seen&&shown[11].active&&shown[11].hour==10&&shown[11].minute==26);
  assert(shown[10].hash!=shown[11].hash); /* Renderer advances while finger stays down. */
  assert(shown[13].seen&&!shown[13].active&&shown[13].hour==10&&shown[13].minute==26);
  assert(!p7_picker_active()); /* Release did not synthesize a second step or hour change. */
 } else if(scenario==1) {
  assert(page==PAGE_LIST && nova_list_scroll==36 && !writes);
  assert(shown[4].seen&&shown[4].active&&shown[4].scroll==15);
  assert(shown[5].seen&&shown[5].active&&shown[5].scroll==36);
  assert(shown[4].hash!=shown[5].hash); /* Pixel scrolling during the held contact. */
 } else {
  assert(page==PAGE_EDIT && !strcmp(custom_draft.custom[0].name,"Namea") && writes==1);
  uint8_t bytes[POINTS_RECORD_SIZE];uint32_t size=0;points_meta meta;
  assert(fake_get(NULL,POINTS_META_KEY,bytes,sizeof(bytes),&size)==0&&points_meta_decode(&meta,bytes,size));
  assert(!strcmp(meta.custom[0].name,"Namea")&&meta.revision==2&&meta.custom[0].color==6);
  points_config config;assert(fake_get(NULL,POINTS_CONFIG_KEY,bytes,sizeof(bytes),&size)==0&&points_config_decode(&config,bytes,size));
  assert(config.revision==1&&config.points[0].hour==10&&config.points[0].minute==25);
 }
 printf("Points real-adapter touch scenario %u: %u frames, raw DOWN/MOVE/UP through app_main, all grants released, stride guards intact\n",scenario,presents);
 return 0;
}
