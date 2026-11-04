/* One semantic Timecard app. Providers own all geometry, input and lifecycle. */
#define app_main tc_legacy_entry
static void tc_legacy_entry(void);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wattributes"
#include "timecard.c"
#undef app_main
#pragma GCC diagnostic pop
#include "timecard_portable_validation.h"
#include "timecard_appdata_bridge.h"
#include "RiscRuntimeV1.h"
#include "PortableTimeFormat.h"
#include "RiscSceneComponentsV1.h"
#include "RiscCivilClockV1.h"
#include "RiscSceneResidentV1.h"
#include "SceneKeyboardV1.h"
#include <stdio.h>
enum { TC_BACK=1, TC_WEEK, TC_DAY, TC_EDIT, TC_PUNCH, TC_KEY, TC_RETRY };
static const risc_runtime_api_v1 *rt;
static const risc_scene_api_v1 *scene;
static const risc_scene_components_api_v1 *components;
static const risc_civil_clock_api_v1 *clock_api;
static const risc_app_data_v1 *data_api;
static risc_app_data_v1 guarded_data;
static tcp_appdata data;
static risc_runtime_capability_v1 grants[4];
static const risc_key_value_v1 *kv_api;
static unsigned time_format;
static unsigned acquired;
static risc_scene_resident_v1 resident;
static uint64_t session,last_event;
static uint32_t revision;
static bool terminal,editor,exit_app,clock_valid;
static int32_t today_ymd;
static unsigned now_minute,edit_punch,key_layer;
static char draft[13];
static risc_components_document_v1 doc;
static int32_t refs[RISC_COMPONENTS_MAX_NODES];
static tc_day_t before[MAX_DAYS];
__attribute__((visibility("default"))) const risc_resident_app_descriptor_v1_t risc_resident_app_descriptor_v1={1,sizeof(risc_resident_app_descriptor_v1_t),RISC_RESIDENT_ROLE_FOREGROUND,0};
static void retain(void){if(!terminal){terminal=true;if(risc_runtime_get_api(1)&&rt&&rt->retain_invocation)(void)rt->retain_invocation();}}
static bool alive(void){if(terminal)return false;if(!risc_runtime_get_api(1)){terminal=true;return false;}return true;}
static bool checked(int rc){if(rc==-9){retain();return false;}return alive();}
static int32_t stat_data(void *ctx,const char *name,uint32_t *size,uint64_t *token){
 (void)ctx;if(!alive())return RISC_APP_DATA_RETAINED;
 int rc=data_api->stat(data_api->context,name,size,token);return checked(rc)?rc:RISC_APP_DATA_RETAINED;
}
static int32_t read_data(void *ctx,const char *name,uint64_t token,void *bytes,uint32_t cap,uint32_t *size,uint64_t *next){
 (void)ctx;if(!alive())return RISC_APP_DATA_RETAINED;
 int rc=data_api->read(data_api->context,name,token,bytes,cap,size,next);return checked(rc)?rc:RISC_APP_DATA_RETAINED;
}
static int32_t replace_data(void *ctx,const char *name,uint64_t token,const void *bytes,uint32_t size){
 (void)ctx;if(!alive())return RISC_APP_DATA_RETAINED;
 int rc=data_api->replace(data_api->context,name,token,bytes,size);return checked(rc)?rc:RISC_APP_DATA_RETAINED;
}
static int32_t preference_get(void *ctx,const char *name,void *bytes,uint32_t cap,uint32_t *size){
 (void)ctx;if(!alive())return -9;
 int rc=kv_api->get(kv_api->context,name,bytes,cap,size);return checked(rc)?rc:-9;
}
static bool load_preference(void){
 time_format=0;
 if(!portable_time_format_api_valid(kv_api))return true;
 risc_key_value_v1 safe=*kv_api;safe.get=preference_get;
 (void)portable_time_format_load(&safe,&time_format);return alive();
}
static void format_time(int16_t minutes,char *out,size_t capacity){
 if(minutes<0||!time_format){format_ampm(minutes,out,capacity);return;}
 snprintf(out,capacity,"%02d:%02d",minutes/60,minutes%60);
}
static bool exists_file(const char *path){return tcp_appdata_exists(&data,path);}
static bool read_file(const char *path,void *bytes,size_t capacity,size_t *size){
 return tcp_appdata_read(&data,path,bytes,capacity,size)&&tcp_validate_json(bytes,*size);
}
static bool write_file(const char *path,const void *bytes,size_t size){
 return tcp_validate_json(bytes,size)&&tcp_appdata_write(&data,path,bytes,size);
}
static const t5_storage_api_v1 files={.api_version=1,.struct_size=sizeof(files),.exists=exists_file,.read_file=read_file,.write_file_atomic=write_file};
static bool reload(void){
 storage=&files;bool ok=load_store();if(!alive()||data.retained)return false;
 if(!ok)set_status("History unavailable; retry storage");
 else set_status("Select a week");
 return true;
}
static bool sample_clock(void){
 clock_valid=false;
 if(!clock_api)return true;
 risc_civil_time_v1 value={.struct_size=sizeof(value)};
 int rc=clock_api->read(clock_api->context,&value);if(!checked(rc))return false;
 if(rc||value.day<0||value.day>2932896||value.minute>1439)return true;
 /* Bounded calendar conversion reuses the authoritative Gregorian model. */
 int32_t left=value.day,year=1970;
 while(left>=(leap(year)?366:365)){left-=leap(year)?366:365;++year;}
 int32_t month=1;while(left>=month_days(year,month)){left-=month_days(year,month);++month;}
 today_ymd=make_ymd(year,month,left+1);now_minute=value.minute;clock_valid=true;return true;
}
static int32_t week_start(int offset){return add_days(today_ymd,-weekday(today_ymd)+offset*7);}
static bool mutate(int32_t date,unsigned punch,int16_t minutes){
 if(!store_ready||!tcpv_date(date)||punch>=PUNCH_COUNT||minutes< -1||minutes>1439)return false;
 if(!find_day(date)&&day_count>=MAX_DAYS){set_status("History full; no records removed");return false;}
 int32_t count=day_count;memcpy(before,days,sizeof(days));
 bool ok=set_punch(date,(uint8_t)punch,minutes);
 if(!alive()||data.retained)return false;
 if(!ok){memcpy(days,before,sizeof(days));day_count=count;store_ready=false;set_status("Save unconfirmed; retry storage");return false;}
 /* The authoritative serializer omits blank records. Keep RAM consistent. */
 int32_t out=0;for(int32_t i=0;i<day_count;i++)if(any_punch(&days[i]))days[out++]=days[i];day_count=out;
 set_status("Saved");return true;
}
static risc_scene_node_v1 *node(unsigned kind,const char *title,const char *text,unsigned action,int32_t ref){
 if(doc.node_count>=RISC_COMPONENTS_MAX_NODES)return NULL;
 unsigned i=doc.node_count++;risc_scene_node_v1 *n=&doc.nodes[i];
 *n=(risc_scene_node_v1){.id=i+1,.route=1,.kind=kind,.action=action};
 scopy(n->label,sizeof(n->label),title);scopy(n->text,sizeof(n->text),text);refs[i]=ref;return n;
}
static void declare(void){
 memset(&doc,0,sizeof(doc));memset(refs,0,sizeof(refs));
 doc=(risc_components_document_v1){.api_version=1,.struct_size=sizeof(doc),.revision=++revision,.root=1,.route_count=1,
 .screen_key=editor?4u:screen_id==SCREEN_WEEK_LIST?1u:screen_id==SCREEN_WEEK?100u+(unsigned)(-week_offset):1000u+(uint32_t)editing_ymd};
 doc.routes[0].id=1;doc.routes[0].back_action=TC_BACK;scopy(doc.routes[0].title,RISC_SCENE_LABEL,"TIME CARD");
 scopy(doc.subtitle,sizeof(doc.subtitle),status_text);
 if(!store_ready){node(RISC_COMPONENT_EMPTY,"HISTORY UNAVAILABLE","Your saved history has not been replaced.",0,0);node(RISC_SCENE_ACTION,"RETRY STORAGE","",TC_RETRY,0);return;}
 if(editor){
  scopy(doc.routes[0].title,RISC_SCENE_LABEL,punch_name((uint8_t)edit_punch));
  risc_scene_node_v1 *n=node(RISC_SCENE_KEYBOARD_NODE,"TIME",draft,TC_KEY,0);
  n->value=(int32_t)key_layer;n->minimum=0;n->maximum=3;n->step=1;n->target=sizeof(draft)-1;
  scopy(doc.subtitle,sizeof(doc.subtitle),"H:MM AM/PM or 24-hour; blank clears");return;
 }
 if(!clock_valid){node(RISC_COMPONENT_EMPTY,"CLOCK UNAVAILABLE","Set the clock to browse weeks or punch now.",0,0);return;}
 if(screen_id==SCREEN_WEEK_LIST){
  for(int i=0;i<WEEK_HISTORY;i++){int32_t start=week_start(-i);if(!tcpv_date(add_days(start,6)))break;char title[80];week_label(start,title,sizeof(title));node(RISC_COMPONENT_ROW,title,i?"":"This week",TC_WEEK,-i);}
 }else if(screen_id==SCREEN_WEEK){
  int32_t start=week_start(week_offset);char label[80];week_label(start,label,sizeof(label));scopy(doc.routes[0].title,RISC_SCENE_LABEL,label);
  for(int i=0;i<DAY_COUNT;i++){
   int32_t date=add_days(start,i);if(!tcpv_date(date))continue;
   tc_day_t d=get_day(date);char title[40],summary[72],in[24],out[24];
   snprintf(title,sizeof(title),"%s %d%s",day_name(date),split_ymd(date).day,date==today_ymd?" *":"");
   format_time(d.punches[0],in,sizeof(in));format_time(d.punches[3],out,sizeof(out));
   int total=worked(&d);if(total>=0)snprintf(summary,sizeof(summary),"%s - %s / %d:%02d",in,out,total/60,total%60);else snprintf(summary,sizeof(summary),"%s - %s",in,out);
   node(RISC_COMPONENT_ROW,title,summary,TC_DAY,date);
  }
  node(RISC_COMPONENT_SECTION,"PUNCH NOW","",0,0);
  for(unsigned p=0;p<PUNCH_COUNT;p++)node(RISC_SCENE_ACTION,punch_name((uint8_t)p),"",TC_PUNCH,(int32_t)p);
 }else{
  tc_day_t d=get_day(editing_ymd);char title[40];snprintf(title,sizeof(title),"%04d-%02d-%02d",editing_ymd/10000,editing_ymd/100%100,editing_ymd%100);scopy(doc.routes[0].title,RISC_SCENE_LABEL,title);
  for(unsigned p=0;p<PUNCH_COUNT;p++){char time[24];format_time(d.punches[p],time,sizeof(time));node(RISC_COMPONENT_ROW,punch_name((uint8_t)p),time,TC_EDIT,(int32_t)p);}
  int total=worked(&d);if(total>=0){char time[24];snprintf(time,sizeof(time),"%d:%02d",total/60,total%60);node(RISC_COMPONENT_STAT_ROW,"WORKED",time,0,0);}
 }
}
static bool event_valid(const risc_scene_event_v1 *e){
 if(!e||e->struct_size!=sizeof(*e)||e->document_revision!=doc.revision||!e->sequence||e->sequence<=last_event)return false;
 if(e->kind==RISC_SCENE_SUSPEND_EVENT)return true;
 if(!e->node)return e->kind==RISC_SCENE_ACTION_EVENT&&e->action==TC_BACK;
 if(e->node>doc.node_count)return false;
 const risc_scene_node_v1 *n=&doc.nodes[e->node-1];
 if(!n->action||n->flags&(RISC_SCENE_DISABLED|RISC_SCENE_HIDDEN)||e->action!=n->action)return false;
 return n->kind==RISC_SCENE_KEYBOARD_NODE?e->kind==RISC_SCENE_VALUE_EVENT:e->kind==RISC_SCENE_ACTION_EVENT;
}
static void handle(const risc_scene_event_v1 *e){
 if(!event_valid(e))return;
 last_event=e->sequence;
 if(e->kind==RISC_SCENE_SUSPEND_EVENT){exit_app=true;return;}
 int32_t ref=e->node?refs[e->node-1]:0;
 if(e->action==TC_BACK){if(editor)editor=false;else if(screen_id==SCREEN_DAY)screen_id=SCREEN_WEEK;else if(screen_id==SCREEN_WEEK)screen_id=SCREEN_WEEK_LIST;else exit_app=true;return;}
 if(e->action==TC_RETRY){(void)reload();return;}
 if(!store_ready)return;
 switch(e->action){
 case TC_WEEK:week_offset=ref;screen_id=SCREEN_WEEK;break;
 case TC_DAY:editing_ymd=ref;screen_id=SCREEN_DAY;break;
 case TC_EDIT:{tc_day_t d=get_day(editing_ymd);edit_punch=(unsigned)ref;draft[0]=0;if(d.punches[ref]>=0)format_time(d.punches[ref],draft,sizeof(draft));key_layer=0;editor=true;break;}
 case TC_PUNCH:
  if(sample_clock()&&clock_valid&&mutate(today_ymd,(unsigned)ref,(int16_t)now_minute)){week_offset=0;screen_id=SCREEN_WEEK;}break;
 case TC_KEY:{
  int value=e->value;size_t len=strlen(draft);
  if(value==RISC_SCENE_KEY_DONE){int16_t minutes;if(!tcp_parse_time(draft,&minutes))set_status("Enter a valid time");else if(mutate(editing_ymd,edit_punch,minutes))editor=false;}
  else if(value==RISC_SCENE_KEY_CANCEL)editor=false;
  else if(value==RISC_SCENE_KEY_LAYER)key_layer=(key_layer+1)%4;
  else if(value==RISC_SCENE_KEY_CLEAR)draft[0]=0;
  else if(value==RISC_SCENE_KEY_BACKSPACE){if(len)draft[len-1]=0;}
  else {if(value==RISC_SCENE_KEY_SPACE)value=' ';if(value>=32&&value<=126&&len+1<sizeof(draft)){draft[len]=(char)value;draft[len+1]=0;}}
  break;}
 default:break;
 }
}
static void release_all(void){while(acquired&&alive()){bool ok=rt->release(&grants[--acquired]);if(!alive())return;if(!ok){retain();return;}}}
static bool close_scene(void){
 if(!alive())return false;
 if(!session)return true;
 for(unsigned i=0;i<1000;i++){int rc=scene->close(scene->context,session);if(!checked(rc))return false;if(rc==RISC_SCENE_OK){session=0;return true;}if(rc!=RISC_SCENE_AGAIN){retain();return false;}rt->yield_ms(5);if(!alive())return false;}retain();return false;
}
static bool open_scene(const risc_scene_navigation_v1 *nav){
 int rc=components->open(scene->context,&doc,nav,&session);if(!checked(rc)||rc)return false;last_event=0;
 const risc_scene_lifecycle_api_v1 *life=risc_scene_lifecycle_get_v1(scene);
 if(life){rc=life->configure(scene->context,session,resident.enabled?RISC_SCENE_FEATURE_SHARED_CONTROLS:0);if(!checked(rc)||rc)return false;}return true;
}
static bool shell(unsigned reason){
 if(editor||!store_ready)return true;
 risc_scene_navigation_v1 nav={.struct_size=sizeof(nav)};uint32_t flags=0;
 int rc=scene->snapshot(scene->context,session,&nav,&flags);if(!checked(rc)||rc)return false;
 if(!close_scene())return false;
 rc=risc_scene_resident_dispatch_v1(&resident,reason);if(!checked(rc))return false;
 if(rc!=RISC_RESIDENT_OK&&rc!=RISC_RESIDENT_BUSY)return false;
 return open_scene(&nav);
}
static bool bind(void){
 const char *names[]={RISC_SCENE_CAPABILITY,RISC_APP_DATA_CAPABILITY,RISC_CIVIL_CLOCK_CAPABILITY,"storage.key-value"};
 for(unsigned i=0;i<4;i++){grants[acquired]=(risc_runtime_capability_v1){.struct_size=sizeof(grants[0])};bool ok=rt->acquire(names[i],1,0,&grants[acquired]);if(!alive())return false;
  if(!ok){if(i==0)return false;continue;}
  const void *api=grants[acquired++].api;
  if(i==0){scene=api;components=risc_scene_components_get_v1(scene);if(!components||!scene->next||!scene->close||!scene->snapshot)return false;}
  else if(i==1)data_api=api;else if(i==2)clock_api=api;else kv_api=api;
 }
 if(data_api&&data_api->api_version==1&&data_api->struct_size>=sizeof(*data_api)&&data_api->stat&&data_api->read&&data_api->replace){
  guarded_data=*data_api;guarded_data.stat=stat_data;guarded_data.read=read_data;guarded_data.replace=replace_data;(void)tcp_appdata_bind(&data,&guarded_data);
 }
 if(clock_api&&(clock_api->api_version!=1||clock_api->struct_size<sizeof(*clock_api)||!clock_api->read))clock_api=NULL;
 return true;
}
__attribute__((visibility("default"))) void app_main(void){
 rt=risc_runtime_get_api(1);if(!rt||rt->struct_size<RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE||!rt->retain_invocation||!rt->acquire||!rt->release||!rt->yield_ms)return;
 terminal=editor=exit_app=clock_valid=false;acquired=0;session=last_event=0;revision=0;screen_id=SCREEN_WEEK_LIST;week_offset=day_count=0;store_ready=false;data=(tcp_appdata){0};data_api=NULL;clock_api=NULL;kv_api=NULL;time_format=0;resident=(risc_scene_resident_v1){0};
 if(!bind()){if(alive())release_all();return;}
 if(!risc_scene_resident_bind_v1(rt,&resident)){if(alive())retain();return;}
 if(!load_preference()||!reload()||!sample_clock()){if(alive())release_all();return;}declare();
 if(!open_scene(NULL)){if(alive()&&close_scene())release_all();return;}
 unsigned ticks=0,polls=0;
 while(!exit_app&&alive()){
  risc_scene_event_v1 e={.struct_size=sizeof(e)};int rc=scene->next(scene->context,session,&e);if(!checked(rc))return;if(rc!=RISC_SCENE_OK&&rc!=RISC_SCENE_IDLE)break;
  bool changed=false;
  if(rc==RISC_SCENE_OK){
   if(e.kind==RISC_SCENE_CONTROLS_EVENT&&e.document_revision==doc.revision&&e.sequence>last_event&&resident.enabled){last_event=e.sequence;if(!shell(RISC_RESIDENT_CHECKPOINT_CONTROLS))break;continue;}
   handle(&e);if(!alive())return;changed=true;
  }
  if(++ticks>=250&&!editor){ticks=0;bool prior=clock_valid;int32_t day=today_ymd;if(!sample_clock())return;changed=changed||prior!=clock_valid||day!=today_ymd;}
  if(exit_app)break;
  if(changed){if(revision==UINT32_MAX)break;declare();rc=components->update(scene->context,session,&doc);if(!checked(rc))return;if(rc)break;}
  if(resident.enabled){
   risc_scene_navigation_v1 nav={.struct_size=sizeof(nav)};uint32_t flags=0;
   rc=scene->snapshot(scene->context,session,&nav,&flags);if(!checked(rc))return;if(rc)break;if(flags&RISC_SCENE_ACTIVITY)resident.activity=true;
   if(++polls>=50){polls=0;rc=risc_scene_resident_poll_work_v1(&resident,flags,(editor||!store_ready)?RISC_RESIDENT_POLL_INHIBIT_IDLE|RISC_RESIDENT_POLL_INHIBIT_POLICY:0);if(!checked(rc))return;if(rc==RISC_RESIDENT_EXIT)break;if(rc!=RISC_RESIDENT_OK&&rc!=RISC_RESIDENT_BUSY)break;
    if(resident.policy&&!editor&&store_ready&&!(flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY))&&!shell(RISC_RESIDENT_CHECKPOINT_POLICY))break;
   }
  }
  rt->yield_ms(5);if(!alive())return;
 }
 if(close_scene())release_all();
}

#ifdef TIMECARD_SCENE_FIXTURE
const risc_components_document_v1 *timecard_scene_fixture(unsigned page){
 revision=0;last_event=0;screen_id=(uint8_t)(page%3);editor=page==3;store_ready=clock_valid=true;
 today_ymd=20261104;week_offset=0;editing_ymd=today_ymd;edit_punch=0;key_layer=0;time_format=0;
 day_count=1;days[0]=blank_day(today_ymd);days[0].punches[0]=540;days[0].punches[1]=720;days[0].punches[2]=750;days[0].punches[3]=1020;
 scopy(draft,sizeof(draft),"9:00 AM");set_status("Shared Timecard");declare();return &doc;
}
#endif
