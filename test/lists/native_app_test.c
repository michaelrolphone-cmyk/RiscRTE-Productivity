/* Real app_main, reducer, storage protocol and shared scene/keyboard host.
 * Only provider transports and native/resident custody are test doubles. */
#define main scene_transport_tests
#define risc_runtime_get_api host_fixture_runtime
#include "host_test.c"
#undef risc_runtime_get_api
#undef main
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t);
#define session app_session
#include "../../Apps/lists_scene.c"
#undef session
#include "SceneKeyboardV1.h"
extern bool lists_test_target(uint32_t,int,int*,int*);
extern bool lists_test_header(bool,int*,int*);
static const char *mode,*folder;
static uint8_t disk[LISTS_WIRE_SIZE],original[LISTS_WIRE_SIZE];
static uint64_t generation=1;
static unsigned phase,steps,app_acquires,app_releases,writes,stats,reads,retains,checkpoints,opens,closes,updates;
static unsigned close_waits,faults;
static bool present=true,exit_pending,done,write_started;
static risc_scene_components_api_v1 wrapped;
static const risc_scene_components_api_v1 *actual;
static bool is(const char*s){return !strcmp(mode,s);}
static bool starts(const char*s){return !strncmp(mode,s,strlen(s));}
static void live_io(void){assert(native_alive&&"application I/O after terminal custody");}
static bool fault(const char*site){
 char name[80];snprintf(name,sizeof(name),"loss-%s",site);
 if(is(name)){++faults;native_alive=false;return true;}
 snprintf(name,sizeof(name),"retained-%s",site);if(is(name)){++faults;return true;}return false;
}
static int fault_status(void){return starts("loss-")?0:-9;}
static int file_stat(void*c,const char*n,uint32_t*size,uint64_t*revision){
 (void)c;live_io();assert(!strcmp(n,"lists.bin"));++stats;
 if(fault(write_started?"reload-stat":"stat"))return fault_status();
 *size=present?sizeof(disk):0;*revision=present?generation:0;
 if((is("retry-io")&&stats==1)||is("storage-io")||(write_started&&is("reload-io")))return RISC_APP_DATA_IO;
 if(is("storage-size")||(write_started&&is("reload-size")))--*size;
 if(is("storage-revision"))*revision=0;
 return present?RISC_APP_DATA_OK:RISC_APP_DATA_NOT_FOUND;
}
static int file_read(void*c,const char*n,uint64_t revision,void*out,uint32_t cap,uint32_t*size,uint64_t*actual_revision){
 (void)c;live_io();assert(!strcmp(n,"lists.bin")&&revision==generation&&cap==sizeof(disk));++reads;
 if(fault(write_started?"reload-read":"read"))return fault_status();
 memcpy(out,disk,sizeof(disk));*size=sizeof(disk);*actual_revision=generation;
 if((is("retry-corrupt")&&reads==1)||is("storage-corrupt")||(write_started&&is("reload-corrupt")))((uint8_t*)out)[100]^=1;
 if(is("storage-stale")||(write_started&&is("reload-stale")))return RISC_APP_DATA_STALE;
 return RISC_APP_DATA_OK;
}
static int file_replace(void*c,const char*n,uint64_t revision,const void*data,uint32_t size){
 (void)c;live_io();assert(!strcmp(n,"lists.bin")&&revision==(present?generation:0)&&size==sizeof(disk));++writes;write_started=true;
 if(fault("replace"))return fault_status();
 if(is("save-io"))return RISC_APP_DATA_IO;
 if(is("save-stale"))return RISC_APP_DATA_STALE;
 if(is("unknown-rejected"))return RISC_APP_DATA_COMMIT_UNKNOWN;
 memcpy(disk,data,size);present=true;++generation;
 if(is("unknown-committed"))return RISC_APP_DATA_COMMIT_UNKNOWN;
 return RISC_APP_DATA_OK;
}
static const risc_app_data_v1 storage={1,sizeof(storage),NULL,file_stat,file_read,file_replace};
static int civil_read(void*c,risc_civil_time_v1*t){(void)c;live_io();if(fault("clock"))return fault_status();*t=(risc_civil_time_v1){sizeof(*t),lists_day(2026,10,19),600,"UTC"};return is("clock-invalid")?-1:0;}
static const risc_civil_clock_api_v1 civil_api={1,sizeof(civil_api),NULL,civil_read};
static int wrap_open(void*c,const risc_components_document_v1*d,const risc_scene_navigation_v1*n,uint64_t*out){
 live_io();++opens;if(fault(opens>1?"reopen":"open"))return fault_status();if(is("open-error"))return -4;
 return actual->open(c,d,n,out);
}
static int wrap_update(void*c,uint64_t token,const risc_components_document_v1*d){live_io();++updates;if(fault("update"))return fault_status();if(is("update-error"))return -4;return actual->update(c,token,d);}
static int wrap_next(void*c,uint64_t token,risc_scene_event_v1*e){live_io();if(fault("next"))return fault_status();return actual->lifecycle.base.next(c,token,e);}
static int wrap_snapshot(void*c,uint64_t token,risc_scene_navigation_v1*n,uint32_t*f){live_io();if(fault("snapshot"))return fault_status();return actual->lifecycle.base.snapshot(c,token,n,f);}
static int wrap_close(void*c,uint64_t token){
 live_io();++closes;if(fault("close"))return fault_status();if(is("close-error"))return -4;
 if(is("close-delay")&&close_waits++<3)return RISC_SCENE_AGAIN;
 return actual->lifecycle.base.close(c,token);
}
static int wrap_configure(void*c,uint64_t token,uint32_t flags){live_io();if(fault("configure"))return fault_status();if(is("configure-error"))return -4;return actual->lifecycle.configure(c,token,flags);}
static bool app_acquire(const char*n,uint32_t version,uint64_t id,risc_runtime_capability_v1*g){
 (void)id;live_io();assert(version==1);++app_acquires;
 char point[30];snprintf(point,sizeof(point),"acquire-%u",app_acquires);if(fault(point))return true;
 char failure[40];snprintf(failure,sizeof(failure),"missing-%u",app_acquires);if(is(failure)){--app_acquires;return false;}
 g->slot=app_acquires;g->generation=1;
 g->api=!strcmp(n,RISC_SCENE_CAPABILITY)?(const void*)&wrapped:!strcmp(n,RISC_APP_DATA_CAPABILITY)?(const void*)&storage:(const void*)&civil_api;return true;
}
static bool app_release(risc_runtime_capability_v1*g){live_io();assert(g->api&&!app_session);++app_releases;if(fault("release"))return starts("loss-");return !is("release-error");}
static bool app_retain(void){++retains;native_alive=false;return true;}
static bool app_exit(void){live_io();if(fault("exit"))return true;return exit_pending;}
static bool app_diagnostic(const char*s){(void)s;live_io();return true;}
static int app_checkpoint(uint64_t invocation,const risc_resident_request_v1*q,risc_resident_reply_v1*r){
 live_io();assert(invocation==99);if(q->reason==RISC_RESIDENT_CHECKPOINT_POLL){assert(!presenting&&!held);if(is("policy")||(is("checkpoint")&&phase==11))r->flags=RISC_RESIDENT_REPLY_POLICY_REQUEST;return RISC_RESIDENT_OK;}
 assert(!app_session&&!held&&!presenting&&!subscribed);++checkpoints;
 if(fault("checkpoint"))return starts("loss-")?RISC_RESIDENT_OK:RISC_RESIDENT_RETAINED;
 if(is("checkpoint-error"))return RISC_RESIDENT_DENIED;
 r->flags=RISC_RESIDENT_REPLY_REDRAW;return is("checkpoint-busy")?RISC_RESIDENT_BUSY:RISC_RESIDENT_OK;
}
static bool app_shell(risc_resident_client_v1*q){live_io();if(fault("bind"))return false;if(is("nonresident"))return false;*q=(risc_resident_client_v1){.api_version=1,.struct_size=sizeof(*q),.invocation=99,.role=RISC_RESIDENT_ROLE_FOREGROUND,.checkpoint=app_checkpoint};return true;}
static void sync_host(void){session=app_session;}
static void controls_input(void){
 event_at=0;event_count=3;
 events[0]=(risc_touch_event_v1){++seq,ms,1,0,(uint16_t)(w/2),1};
 events[1]=(risc_touch_event_v1){++seq,ms+40,2,0,(uint16_t)(w/2),(uint16_t)(h/2)};
 events[2]=(risc_touch_event_v1){++seq,ms+80,3,0,(uint16_t)(w/2),(uint16_t)(h/2)};
}
static void tap(unsigned action,int value){
 sync_host();int x,y;
 for(unsigned i=0;i<250;i++){
  if(lists_test_target(action,value,&x,&y)){touch_tap(x,y);return;}
  nav_pressed=RISC_NAV_DOWN;risc_scene_event_v1 e;assert(tick(&e)==RISC_SCENE_IDLE);
 }
 fprintf(stderr,"No target action %u value %d page %u size %ux%u mode %s\n",action,value,ui.page,w,h,mode);assert(0);
}
static void frame(const char*name){
 sync_host();settle();char path[1024];snprintf(path,sizeof(path),"%s/%s.ppm",folder,name);FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%u %u\n255\n",w,h);
 for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){unsigned at=16+(y*w+x)*2;uint16_t pixel=pixels[at]|(uint16_t)pixels[at+1]<<8;uint8_t rgb[]={((pixel>>11)*255/31),(((pixel>>5)&63)*255/63),((pixel&31)*255/31)};assert(fwrite(rgb,1,3,f)==3);}
 assert(!fclose(f));
}
static void workflow(void){
 /* Every step is sent through the actual rendered hit map; production state
  * is inspected only to assert results, never to manufacture navigation. */
 static const struct {unsigned page,action;int value;} flow[]={
  {LISTS_HOME,LA_LIST,-999},{LISTS_VIEW,LA_TASK,-999},
  {LISTS_TASK,LA_RENAME_TASK,-999},{LISTS_NAME_EDIT,LA_KEY,RISC_SCENE_KEY_CLEAR},
  {LISTS_NAME_EDIT,LA_KEY,'N'},{LISTS_NAME_EDIT,LA_KEY,'E'},{LISTS_NAME_EDIT,LA_KEY,'W'},
  {LISTS_NAME_EDIT,LA_KEY,RISC_SCENE_KEY_DONE},
  {LISTS_TASK,LA_TASK_LIST,1},{LISTS_TASK,LA_DUE,1},{LISTS_TASK,LA_TIME,-999},
  {LISTS_TIME,LA_TIME_VALUE,600},{LISTS_TIME,LA_SET_TIME,-999},
  {LISTS_TASK,LA_PRIORITY,2},{LISTS_TASK,LA_REMINDER,1},{LISTS_TASK,LA_REPEAT,1},
  {LISTS_TASK,LA_DONE,1},{LISTS_TASK,LA_DONE,0},{LISTS_TASK,LA_DONE,1},
  {LISTS_TASK,LA_DELETE_TASK,-999},{LISTS_CONFIRM,LA_KEEP,-999},
  {LISTS_TASK,LA_DELETE_TASK,-999},{LISTS_CONFIRM,LA_CONFIRM,-999},
  {LISTS_VIEW,LA_BACK,-999},{LISTS_HOME,LA_LIST,-999},{LISTS_VIEW,LA_OPTIONS,-999},
  {LISTS_OPTIONS,LA_RENAME_LIST,-999},{LISTS_NAME_EDIT,LA_KEY,RISC_SCENE_KEY_CLEAR},
  {LISTS_NAME_EDIT,LA_KEY,'L'},{LISTS_NAME_EDIT,LA_KEY,'I'},{LISTS_NAME_EDIT,LA_KEY,'S'},
  {LISTS_NAME_EDIT,LA_KEY,'T'},{LISTS_NAME_EDIT,LA_KEY,'X'},{LISTS_NAME_EDIT,LA_KEY,RISC_SCENE_KEY_DONE},
  {LISTS_OPTIONS,LA_SHOW_DONE,1},{LISTS_OPTIONS,LA_DELETE_LIST,-999},
  {LISTS_CONFIRM,LA_KEEP,-999},{LISTS_OPTIONS,LA_DELETE_LIST,-999},{LISTS_CONFIRM,LA_CONFIRM,-999}
 };
 if(phase==8)assert(!strcmp(ui.data.tasks[0].name,"NEW")&&writes==1);
 if(phase==16)assert(ui.data.tasks[0].priority==2&&ui.data.tasks[0].minute==600&&ui.data.tasks[0].reminder==1&&ui.data.tasks[0].repeat==1&&ui.data.tasks[0].list_id==ui.data.lists[1].id);
 if(phase==17||phase==18||phase==19)assert(ui.data.task_count==4&&ui.data.tasks[3].due_day==ui.today+1);
 if(phase==23)assert(ui.data.task_count==3);
 if(phase==34)assert(!strcmp(ui.data.lists[0].name,"LISTX"));
 if(phase==sizeof(flow)/sizeof(flow[0])){
  assert(ui.page==LISTS_HOME&&ui.data.list_count==1&&ui.data.task_count==2&&writes==14&&lists_valid(&ui.data));
  assert(!strcmp(ui.data.lists[0].name,"HOME")&&!strcmp(ui.data.tasks[0].name,"OTHER TASK")&&!strcmp(ui.data.tasks[1].name,"NEW"));
  frame("workflow-final");done=true;nav_pressed=RISC_NAV_HOME;return;
 }
 if(ui.page!=flow[phase].page){fprintf(stderr,"workflow phase %u wanted page %u got %u\n",phase,flow[phase].page,ui.page);assert(0);}
 if(flow[phase].action==LA_KEY&&flow[phase].value==RISC_SCENE_KEY_CLEAR){
  if(ui.draft[0]){tap(LA_KEY,RISC_SCENE_KEY_BACKSPACE);return;}
  ++phase;return;
 }
 if(flow[phase].action==LA_BACK)nav_pressed=RISC_NAV_BACK;else tap(flow[phase].action,flow[phase].value);
 ++phase;
}
static void app_yield(uint32_t delay){
 live_io();ms+=delay;assert(++steps<10000&&"application did not exit");sync_host();
 if(!app_session)return;
 if(exit_pending){allow_complete=true;return;}
 if(event_at!=event_count||nav_pressed)return;
 if(is("checkpoint")&&phase==11&&!checkpoints)return;
 if(is("workflow")){workflow();return;}
 if(is("retry-io")||is("retry-corrupt")){
  if(ui.readonly){assert(!writes&&stats==1);tap(LA_RETRY,-999);return;}
  assert(stats==2&&!writes&&!memcmp(disk,original,sizeof(disk)));done=true;nav_pressed=RISC_NAV_HOME;return;
 }
 if(is("fresh")){
  if(phase==0){assert(ui.data.list_count==0&&ui.data.task_count==0);tap(LA_NEW_LIST,-999);}
  else if(phase==1){assert(ui.page==LISTS_ADD_LIST);tap(LA_MARKER,5);}
  else if(phase==2){assert(ui.marker==5);tap(LA_SUGGEST_LIST,-999);}
  else {assert(ui.data.list_count==1&&ui.data.lists[0].marker==5&&ui.data.task_count==0&&writes==1);done=true;nav_pressed=RISC_NAV_HOME;}
  ++phase;return;
 }
 if(starts("storage-")||starts("missing-")||is("open-error")||is("configure-error")){
  assert(ui.readonly);frame("unavailable");done=true;nav_pressed=RISC_NAV_HOME;return;
 }
 if(is("policy")){if(!checkpoints)return;done=true;nav_pressed=RISC_NAV_HOME;return;}
 if(is("exit-idle")||is("exit-busy")){
  if(is("exit-busy")){allow_complete=false;begin_frame();}
  exit_pending=true;done=true;return;
 }
 if(starts("retained-")||starts("loss-")||is("close-error")||is("close-delay")||is("release-error")||is("update-error")||is("checkpoint-error")||is("checkpoint-busy")||is("nonresident")){
  if(strstr(mode,"checkpoint")||strstr(mode,"reopen")){if(!checkpoints){controls_input();return;}}
  else if(strstr(mode,"update")){if(!updates){tap(LA_ALL,-999);return;}}
  else if(strstr(mode,"replace")||strstr(mode,"reload")){} /* use the mutation below */
  else{done=true;nav_pressed=RISC_NAV_HOME;return;}
 }
 switch(phase++){
 case 0:assert(ui.page==LISTS_HOME);frame("home");tap(LA_LIST,-999);break;
 case 1:assert(ui.page==LISTS_VIEW);tap(LA_NEW_TASK,-999);break;
 case 2:assert(ui.page==LISTS_ADD_TASK);tap(LA_CUSTOM_TASK,-999);break;
 case 3:assert(ui.page==LISTS_NAME_EDIT);frame("keyboard");if(is("exit-edit")){exit_pending=true;done=true;return;}tap(LA_KEY,'A');break;
 case 4:assert(!strcmp(ui.draft,"A"));
  if(is("cancel")){tap(LA_KEY,RISC_SCENE_KEY_CANCEL);phase=10;break;}
  if(is("back")){nav_pressed=RISC_NAV_BACK;phase=10;break;}
  if(is("home")){nav_pressed=RISC_NAV_HOME;done=true;break;}
  if(is("checkpoint")){phase=11;break;}
  tap(LA_KEY,RISC_SCENE_KEY_DONE);break;
 case 5:
  if(starts("save-")||is("unknown-rejected")){assert(ui.page==LISTS_HOME&&!ui.readonly&&!ui.undo_valid&&writes==1&&!memcmp(disk,original,sizeof(disk)));frame("save-reloaded");}
  else if(starts("reload-")){assert(ui.readonly&&ui.page==LISTS_HOME&&!ui.undo_valid&&writes==1);frame("reload-unavailable");}
  else {assert(ui.page==LISTS_TASK&&ui.data.task_count==4&&writes==1);assert(!strcmp(ui.data.tasks[3].name,"A"));frame("saved");}
  done=true;nav_pressed=RISC_NAV_HOME;break;
 case 10:assert(ui.page==LISTS_ADD_TASK&&!writes&&!memcmp(disk,original,sizeof(disk)));done=true;nav_pressed=RISC_NAV_HOME;break;
 case 11:assert(checkpoints==1&&opens==2&&ui.page==LISTS_NAME_EDIT&&!strcmp(ui.draft,"A")&&!writes);tap(LA_KEY,RISC_SCENE_KEY_DONE);phase=5;break;
 default:assert(0);
 }
}
static const risc_runtime_api_v1 app_runtime={.api_version=1,.struct_size=sizeof(app_runtime),.yield_ms=app_yield,.diagnostic=app_diagnostic,.acquire=app_acquire,.release=app_release,.retain_invocation=app_retain,.exit_requested=app_exit,.resident_shell=app_shell};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){return version==1&&native_alive?&app_runtime:NULL;}
int main(int argc,char**argv){
 assert(argc==4);assert(sscanf(argv[1],"%ux%u",&w,&h)==2&&w>=160&&h>=160&&w*h<=800*480);folder=argv[2];mode=argv[3];
 startup();assert(!api->close(NULL,session));actual=risc_scene_components_get_v1(api);assert(actual);wrapped=*actual;
 wrapped.open=wrap_open;wrapped.update=wrap_update;wrapped.lifecycle.base.next=wrap_next;wrapped.lifecycle.base.snapshot=wrap_snapshot;wrapped.lifecycle.base.close=wrap_close;wrapped.lifecycle.configure=wrap_configure;
 expected_intent=RISC_DISPLAY_PRESENT_LOW_LATENCY;
 lists_model seed;lists_init(&seed);uint32_t first,second;
 assert(!lists_add_list(&seed,"WORK",1,&first)&&!lists_add_list(&seed,"HOME",2,&second));
 assert(!lists_add_task(&seed,first,"FIRST TASK",-1,NULL));assert(!lists_add_task(&seed,first,"ABCDEFGHIJKLMNOPQRSTUVWX",-1,NULL));assert(!lists_add_task(&seed,second,"OTHER TASK",-1,NULL));assert(lists_encode(&seed,disk));memcpy(original,disk,sizeof(disk));
 if(is("fresh"))present=false;
 app_main();
 if(starts("loss-")||starts("retained-"))assert(faults==1);
 if(starts("loss-")||starts("retained-")||is("close-error")||is("release-error")){assert(!native_alive);assert(app_releases<app_acquires||strstr(mode,"release"));}
 else {assert(!held&&!presenting&&!subscribed&&app_acquires==app_releases);assert(driver->quiesce());driver->stop();if(!starts("missing-")&&!is("open-error")&&!is("configure-error")&&!is("update-error")&&!is("checkpoint-error"))assert(done);}
 if(is("cancel")||is("back")||is("home")||starts("exit-"))assert(!writes&&!memcmp(original,disk,sizeof(disk)));
 if(is("checkpoint")||is("checkpoint-busy"))assert(checkpoints==1&&opens==2);
 if(is("unknown-committed"))assert(writes==1&&ui.data.task_count==4);
 if(is("close-delay"))assert(close_waits>=4);
 char path[1024];snprintf(path,sizeof(path),"%s/store.bin",folder);FILE*f=fopen(path,"wb");assert(f&&fwrite(disk,1,sizeof(disk),f)==sizeof(disk)&&!fclose(f));
 printf("Lists actual app/host storage/lifecycle %s %s PASS: writes=%u grants=%u/%u checkpoints=%u\n",argv[1],mode,writes,app_releases,app_acquires,checkpoints);return 0;
}
