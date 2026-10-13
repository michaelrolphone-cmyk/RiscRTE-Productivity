/* Watch color app/adapter and shared host, with deterministic device providers.
 * Frame hashes capture every actual RGB565 submit without image-file expansion. */
#include "RiscRuntimeV1.h"
static risc_runtime_api_v1 shared_runtime;
static void watch_frame(void);
#define TIMECARD_TEST_RUNTIME (&shared_runtime)
#define TIMECARD_TEST_FRAME_CALLBACK watch_frame
#include "../../Apps/timecard_portable.c"
#include "timecard_adapter_fixture.h"
#include "RiscProviderV2.h"
#include "RiscSceneV1.h"
#include "RiscTextEntryV1.h"
#ifndef PORTABLE_TIMECARD_SHARED_TEXT_INPUT
static const bool tcp_text_active=false;
bool portable_app_before_home(void){return true;}
#endif
#include "SceneKeyboardV1.h"
#ifdef PORTABLE_TEXT_INPUT_POLICY
#include "timecard_policy_scene_fixture.h"
#else
#define timecard_policy_scene_api(s) (s)
#endif
const t5_app_api_v1 *__real_t5_app_get_api(uint32_t);
const t5_app_manifest_t portable_catalog[1]={{.compatible=false}};
const unsigned portable_catalog_count=0;
static t5_app_api_v1 watch_app;
static const t5_app_api_v1 *adapter_app;
static char history[JSON_CAPACITY]="{\"days\":[]}";
static uint64_t history_revision=1;
static unsigned history_writes,script_at,field_index,text_at,text_opens,text_closes,host_calls;
static bool watch_retained,host_active,host_busy,host_failure,open_missing,script_home,script_cancel,script_invalid,script_limit,script_retry,script_alarm;
static risc_scene_document_v1 document;
static const risc_driver_v2 *text_driver;
static const risc_text_entry_api_v1 *text_api;
static const char *scenario;
static uint64_t frame_hash=UINT64_C(1469598103934665603);
static void watch_frame(void){
 const char *frames_path=getenv("TIMECARD_RETRY_FRAMES");
 if(frames_path&&tcp_editor&&!tcp_text_active&&strstr(tcp_editor_status,"unavailable")){
  char path[1024];snprintf(path,sizeof(path),"%s/watch-retry-%s.ppm",frames_path,script_limit?"limit":"invalid");FILE *file=fopen(path,"wb");assert(file);fprintf(file,"P6\n%d %d\n255\n",TIMECARD_TEST_WIDTH,TIMECARD_TEST_HEIGHT);
  for(unsigned y=0;y<TIMECARD_TEST_HEIGHT;y++)for(unsigned x=0;x<TIMECARD_TEST_WIDTH;x++){uint16_t pixel=pixels[y*TIMECARD_TEST_STRIDE+x];unsigned char rgb[]={(pixel>>11)*255/31,((pixel>>5)&63)*255/63,(pixel&31)*255/31};assert(fwrite(rgb,1,3,file)==3);}assert(!fclose(file));
 }
 for(unsigned y=0;y<TIMECARD_TEST_HEIGHT;y++)for(unsigned x=0;x<TIMECARD_TEST_STRIDE;x++){
  uint16_t value=pixels[y*TIMECARD_TEST_STRIDE+x];if(x>=TIMECARD_TEST_WIDTH)assert(value==0xa5a5);
  else {frame_hash=(frame_hash^(value&255))*UINT64_C(1099511628211);frame_hash=(frame_hash^(value>>8))*UINT64_C(1099511628211);}
 }
}
static int32_t history_stat(void *c,const char *n,uint32_t *size,uint64_t *revision){(void)c;assert(!watch_retained&&!strcmp(n,TCP_APPDATA_NAME));*size=(uint32_t)strlen(history);*revision=history_revision;return 0;}
static int32_t history_read(void *c,const char *n,uint64_t revision,void *data,uint32_t cap,uint32_t *size,uint64_t *actual){(void)c;assert(!watch_retained&&!strcmp(n,TCP_APPDATA_NAME)&&revision==history_revision&&cap>=strlen(history));*size=(uint32_t)strlen(history);*actual=history_revision;memcpy(data,history,*size);return 0;}
static int32_t history_replace(void *c,const char *n,uint64_t revision,const void *data,uint32_t size){(void)c;assert(!watch_retained&&!strcmp(n,TCP_APPDATA_NAME)&&revision==history_revision&&size<sizeof(history));memcpy(history,data,size);history[size]=0;history_revision++;history_writes++;return 0;}
static const risc_app_data_v1 history_api={1,sizeof(history_api),NULL,history_stat,history_read,history_replace};
static int32_t scene_open(void *c,const risc_scene_document_v1 *d,const risc_scene_navigation_v1 *p,uint64_t *session){
 (void)c;(void)p;assert(!watch_retained&&!host_active&&!frames&&!subs);host_calls++;document=*d;assert(d->nodes[0].target==12);host_active=true;*session=++text_opens;text_at=0;return 0;
}
static int32_t scene_update(void *c,uint64_t session,const risc_scene_document_v1 *d){(void)c;assert(!watch_retained&&host_active&&session==text_opens&&d->revision>document.revision);host_calls++;document=*d;return 0;}
static int32_t scene_next(void *c,uint64_t session,risc_scene_event_v1 *e){
 (void)c;assert(!watch_retained&&host_active&&session==text_opens);host_calls++;assert(host_calls<10000);
 if(host_failure)return RISC_SCENE_RETAINED;
 static const char *values[]={"08:15","12:00","12:45","17:30"};
 const char *value=script_invalid&&text_opens==1?"25:99":script_limit&&text_opens==1?"123456789012345":values[field_index];
 /* Reopened invalid drafts are cleared through the host before retry. */
 unsigned key;if(text_at==0)key=RISC_SCENE_KEY_CLEAR;else if(text_at<=strlen(value))key=(unsigned char)value[text_at-1];else key=script_cancel?RISC_SCENE_KEY_CANCEL:RISC_SCENE_KEY_DONE;
 *e=(risc_scene_event_v1){.struct_size=sizeof(*e),.kind=script_home&&text_at>strlen(value)?RISC_SCENE_SUSPEND_EVENT:RISC_SCENE_VALUE_EVENT,.document_revision=document.revision,.node=1,.action=1,.value=(int32_t)key,.sequence=text_at+1};text_at++;
 if(key==RISC_SCENE_KEY_DONE&&host_busy)script_retry=true;
 return 0;
}
static int32_t scene_snapshot(void *c,uint64_t session,risc_scene_navigation_v1 *p,uint32_t *flags){(void)c;assert(!watch_retained&&host_active&&session==text_opens);host_calls++;*p=(risc_scene_navigation_v1){.struct_size=sizeof(*p)};*flags=host_busy?RISC_SCENE_PRESENTING:0;return 0;}
static int32_t scene_close(void *c,uint64_t session){(void)c;assert(!watch_retained&&host_active&&session==text_opens);host_calls++;text_closes++;if(host_busy){if(text_closes%4==0)host_busy=false;return RISC_SCENE_AGAIN;}host_active=false;return 0;}
static const risc_scene_api_v1 scene_api={1,sizeof(scene_api),NULL,scene_open,scene_update,scene_next,NULL,scene_snapshot,scene_close};
static bool watch_display_info(void *c,risc_display_info_v1 *info){bool ok=fake_info(c,info);info->flags|=RISC_DISPLAY_INFO_ASYNC_PRESENT;return ok;}
static risc_display_output_api_v1 watch_display;
static int32_t watch_alarm_status(void *c,alarm_status_v1 *s){int32_t rc=fake_alarm_status(c,s);if(script_alarm&&host_active)s->state=ALARM_STATE_ALERT;return rc;}
static alarm_service_v1 watch_alarm;
static void fx_io(void){assert(!watch_retained);}
#include "native_broadcast_fixture.h"
#include "productivity_idle_fixture.h"
static bool watch_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *g){
 assert(!watch_retained);bool ok;
 if(!strcmp(name,RISC_TEXT_ENTRY_CAPABILITY)){if(open_missing)return false;assert(version==1&&!instance);g->api=text_api;grants++;ok=true;}
 else if(!strcmp(name,TELEMETRY_BROADCAST_CAPABILITY)){g->api=&fx_broadcast;grants++;ok=true;}
 else if(!strcmp(name,RISC_APP_DATA_CAPABILITY)){assert(version==1&&instance==1);g->api=&history_api;grants++;ok=true;}
 else if(idle_radio_acquire(name,version,instance,&g->api)){grants++;ok=true;}
 else ok=fake_acquire(name,version,instance,g);
 if(ok){g->slot=grants;g->generation=1;if(!strcmp(name,"display.output"))g->api=&watch_display;if(!strcmp(name,"alarm.service"))g->api=&watch_alarm;}return ok;
}
static bool watch_release(risc_runtime_capability_v1 *g){assert(!watch_retained);bool ok=fake_release(g);if(ok)*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return ok;}
static bool watch_retain(void){assert(!watch_retained);watch_retained=true;return true;}
static bool watch_poll(t5_app_input_t *input,uint32_t delay){
 assert(!tcp_text_active&&!watch_retained);bool ok=adapter_app->poll(input,delay);if(!ok)return false;
 if(!strcmp(scenario,"raw-touch"))return true;
 *input=(t5_app_input_t){0};assert(script_at<20);
 if(!strcmp(scenario,"parity")){
  if(script_at==0)open_week(0);else if(script_at==1)open_day(20261004);
  else if(script_at==2){assert(tcp_mutate(20261004,0,480));assert(tcp_mutate(20261004,3,1020));}
  else if(script_at==3)tcp_time_format=PORTABLE_TIME_FORMAT_24;
  else if(script_at>=4)input->buttons=T5_APP_BUTTON_BACK;
  tcp_dirty=true;script_at++;return true;
 }
 if(!script_at){open_day(20261004);selected=0;tcp_dirty=true;script_at++;return true;}
 if(script_at==1){field_index=0;selected=0;input->buttons=T5_APP_BUTTON_CONFIRM;script_at++;return true;}
 if(tcp_editor){input->buttons=T5_APP_BUTTON_CONFIRM;script_at++;return true;}
 if(!strcmp(scenario,"fields")&&field_index<3){field_index++;selected=(int)field_index;input->buttons=T5_APP_BUTTON_CONFIRM;return true;}
 if(open_missing){open_missing=false;selected=0;input->buttons=T5_APP_BUTTON_CONFIRM;return true;}
 input->buttons=T5_APP_BUTTON_BACK;script_at++;return true;
}
const t5_app_api_v1 *__wrap_t5_app_get_api(uint32_t version){return version==1?&watch_app:NULL;}
int main(int argc,char **argv){
 (void)idle_navigation_resets;(void)idle_broadcast_grants;
 assert(argc==2);scenario=argv[1];memset(pixels,0xa5,sizeof(pixels));directory=NULL;stop_poll=1000;
 watch_display=display_api;watch_display.get_info=watch_display_info;watch_alarm=alarm_api;watch_alarm.status=watch_alarm_status;
 shared_runtime=runtime_api;shared_runtime.acquire=watch_acquire;shared_runtime.release=watch_release;shared_runtime.retain_invocation=watch_retain;
 text_driver=t5_driver_get(2);assert(text_driver);text_api=text_driver->capability;
 risc_provider_dependency_v1 dependency={RISC_SCENE_CAPABILITY,1,timecard_policy_scene_api(&scene_api)};assert(text_driver->start(&dependency,1));
 script_alarm=!strcmp(scenario,"alarm");script_home=!strcmp(scenario,"home");script_cancel=!strcmp(scenario,"cancel");script_invalid=!strcmp(scenario,"invalid");script_limit=!strcmp(scenario,"limit");host_busy=!strcmp(scenario,"busy");host_failure=!strcmp(scenario,"retained");open_missing=!strcmp(scenario,"unavailable");
 if(!strcmp(scenario,"raw-touch")){
  const unsigned at[]={5,15,25,45,55,65};
  for(unsigned i=0;i<6;i++)for(unsigned j=0;j<2;j++){actions[action_count].at=at[i]+j;actions[action_count].x=i<3?90:45;actions[action_count].y=i<3?100:224;action_count++;}
 }
 assert(app_module_init()==0);adapter_app=__real_t5_app_get_api(1);watch_app=*adapter_app;watch_app.poll=watch_poll;
 app_main();
 if(watch_retained){unsigned h=host_calls,g=grants,p=presents;app_module_fini();assert(h==host_calls&&g==grants&&p==presents&&!history_writes&&!launch_count);}
 else{
  assert(!host_active&&!tcp_text_active&&!tcp_editor&&text_driver->quiesce());text_driver->stop();app_module_fini();assert(!grants&&!frames&&!subs&&launch_count==1);
  unsigned expected=!strcmp(scenario,"parity")?2:!strcmp(scenario,"fields")?4:script_home||script_cancel||script_alarm?0:1;assert(history_writes==expected);
  if(!strcmp(scenario,"fields")){assert(strstr(history,"\"in\":495")&&strstr(history,"\"ls\":720")&&strstr(history,"\"le\":765")&&strstr(history,"\"out\":1050"));}
 }
 printf("Watch real adapter/host %s PASS frames=%u hash=%016llx writes=%u opens=%u closes=%u retained=%u\n",scenario,presents,(unsigned long long)frame_hash,history_writes,text_opens,text_closes,watch_retained);
 return 0;
}
