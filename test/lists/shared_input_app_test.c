#define _POSIX_C_SOURCE 200809L
/* The actual Lists app_main, reducers, store, text host and scene host. Only
 * device transports and the disk are fixtures. All ordinary input is touch
 * or USB input consumed by the production service, never direct draft edits. */
#define main scene_transport_tests
#define session transport_session
#define risc_runtime_get_api host_fixture_runtime
#include "host_test.c"
#undef risc_runtime_get_api
#undef session
#undef main
#include "RiscSceneComponentsV1.h"
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t);
#ifndef LISTS_APP_SOURCE
#define LISTS_APP_SOURCE "../../Apps/lists_scene.c"
#endif
#include LISTS_APP_SOURCE
#include "SceneKeyboardV1.h"
#include "RiscTextEntryV1.h"
#include "RiscUsbHidV1.h"
#include "PortableResidentPolicyInput.h"
#include <time.h>
extern bool lists_test_target(uint32_t,int,int*,int*);
extern bool lists_test_header(bool,int*,int*);
extern uint64_t lists_test_session(void);
static const char *device,*folder,*mode,*scenario;
static unsigned phase,loops,app_acquires,app_releases,writes,scene_opens,scene_closes,checkpoints;
static bool native_exit_pending,storage_bad,write_bad,write_unknown,write_loss,inject_stale;
static risc_scene_event_v1 stale_event;
static uint8_t stored[LISTS_WIRE_SIZE],initial_stored[LISTS_WIRE_SIZE];
static uint64_t disk_revision=1;
static lists_model initial_model;
static uint32_t work_id,home_id,task_id;
static const risc_scene_components_api_v1 *real_components;
static risc_scene_components_api_v1 app_scene;
static unsigned reason,clock_minute=660;
static bool navigation_owned;
static uint32_t expected_resume_flags;
static bool policy_request_invalidated,policy_continuity_granted;
static unsigned policy_invalidated_at_wait;
static uint64_t last_touch_poll_ms,last_usb_poll_ms;
typedef struct {uint64_t before_touch,before_usb,start,end,after_touch,after_usb,wall_start,wall_ns;unsigned sleep_delay,transport_delay;int status;bool focus;} policy_measurement;
static policy_measurement policy_measurements[64];
static unsigned policy_measurement_count;
static bool awaiting_touch_measurement,awaiting_usb_measurement;
static uint64_t monotonic_ns(void){struct timespec t;assert(!clock_gettime(CLOCK_MONOTONIC,&t));return (uint64_t)t.tv_sec*1000000000u+(uint64_t)t.tv_nsec;}
static void observe_input_poll(bool usb){
 uint64_t*last=usb?&last_usb_poll_ms:&last_touch_poll_ms;
 bool*waiting=usb?&awaiting_usb_measurement:&awaiting_touch_measurement;
 if(*waiting&&policy_measurement_count){policy_measurement*m=&policy_measurements[policy_measurement_count-1];if(usb)m->after_usb=ms;else m->after_touch=ms;*waiting=false;}
 *last=ms;
}
static int policy_return(int status){
 policy_measurement*m=&policy_measurements[policy_measurement_count-1];m->end=ms;m->wall_ns=monotonic_ns()-m->wall_start;m->status=status;
 awaiting_touch_measurement=awaiting_usb_measurement=true;return status;
}
static unsigned policy_polls,policy_dispatches,policy_wait,policy_wait_second,policy_second_start,policy_activity_polls;
static bool policy_armed,policy_input_blocked;
static bool is_mode(const char*s);
static bool policy_mode(void){return strstr(mode,"policy")||strstr(mode,"checkpoint");}
static bool __attribute__((unused)) policy_usb_mode(void){return policy_mode()&&strstr(mode,"usb");}
static bool policy_sleep_mode(void){return policy_mode()&&strstr(mode,"sleep");}
static bool __attribute__((unused)) policy_cancel_mode(void){return is_mode("policy-gap-cancel")||is_mode("policy-gap-native-cancel")||is_mode("policy-gap-usb-cancel");}
static bool edit_case;
static bool is_mode(const char*s){return !strcmp(mode,s);}
#ifdef PORTABLE_LISTS_SHARED_TEXT_INPUT
#include "shared_input_fixture.h"
#endif
static void frame(const char*name){
 transport_session=lists_test_session();settle();unsigned lw=w==800?h:w,lh=w==800?w:h;
 char path[1024];snprintf(path,sizeof(path),"%s/%s-%s.ppm",folder,device,name);FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%u %u\n255\n",lw,lh);
 for(unsigned y=0;y<lh;y++)for(unsigned x=0;x<lw;x++){
  unsigned px=x,py=y;if(prof.display_rotation==270){px=y;py=h-1-x;}uint8_t rgb[3];
  if(format==5){unsigned at=16+(py*w+px)*2;uint16_t c=pixels[at]|(uint16_t)pixels[at+1]<<8;rgb[0]=(uint8_t)((c>>11)*255/31);rgb[1]=(uint8_t)(((c>>5)&63)*255/63);rgb[2]=(uint8_t)((c&31)*255/31);}
  else{unsigned bit=py*w+px;rgb[0]=rgb[1]=rgb[2]=pixels[16+bit/8]&(128>>(bit%8))?0:255;}
  assert(fwrite(rgb,1,3,f)==3);
 }assert(!fclose(f));
}
static void tap(unsigned action,int value){
 transport_session=lists_test_session();int x,y;
 for(unsigned i=0;i<150;i++){
  if(lists_test_target(action,value,&x,&y)){touch_tap((unsigned)x,(unsigned)y);return;}
  nav_pressed=RISC_NAV_DOWN;risc_scene_event_v1 e;assert(tick(&e)==RISC_SCENE_IDLE);
 }
 fprintf(stderr,"No target action=%u value=%d page=%u phase=%u mode=%s\n",action,value,ui.page,phase,mode);assert(0);
}
static bool input_pending(void){return event_at!=event_count||nav_pressed;}
static void back_input(void){nav_pressed=RISC_NAV_BACK;}
#ifdef PORTABLE_LISTS_SHARED_TEXT_INPUT
static void boundary(const char*site){assert(native_alive);if(edit_case&&!strncmp(mode,"loss-",5)&&!strcmp(mode+5,site))native_alive=false;}
#endif
static int32_t file_stat(void*c,const char*n,uint32_t*size,uint64_t*rev){(void)c;assert(native_alive&&!strcmp(n,"lists.bin"));*size=sizeof(stored);*rev=disk_revision;return storage_bad?RISC_APP_DATA_IO:0;}
static int32_t file_read(void*c,const char*n,uint64_t rev,void*p,uint32_t cap,uint32_t*size,uint64_t*actual){(void)c;assert(native_alive&&!strcmp(n,"lists.bin")&&rev==disk_revision&&cap==sizeof(stored));memcpy(p,stored,sizeof(stored));*size=sizeof(stored);*actual=disk_revision;return 0;}
static int32_t file_replace(void*c,const char*n,uint64_t rev,const void*p,uint32_t size){
 (void)c;assert(native_alive&&!strcmp(n,"lists.bin")&&rev==disk_revision&&size==sizeof(stored));++writes;
#ifdef PORTABLE_LISTS_SHARED_TEXT_INPUT
 assert(!text_active&&!text_granted&&session); /* release/resume precede save */
#endif
 if(write_loss){native_alive=false;return RISC_APP_DATA_RETAINED;}
 if(write_bad)return RISC_APP_DATA_IO;
 memcpy(stored,p,size);disk_revision++;return write_unknown?RISC_APP_DATA_COMMIT_UNKNOWN:0;
}
static const risc_app_data_v1 file_api={1,sizeof(file_api),NULL,file_stat,file_read,file_replace};
static int civil_read(void*c,risc_civil_time_v1*t){(void)c;assert(native_alive);*t=(risc_civil_time_v1){sizeof(*t),lists_day(2026,10,10),(int32_t)clock_minute,"UTC"};return 0;}
static const risc_civil_clock_api_v1 civil_api={1,sizeof(civil_api),NULL,civil_read};
#ifdef PORTABLE_LISTS_SHARED_TEXT_INPUT
static bool scene_fault(const char*site){
 char name[96];snprintf(name,sizeof(name),"loss-%s",site);
 if(is_mode(name)){native_alive=false;return true;}
 snprintf(name,sizeof(name),"%s-retained",site);return is_mode(name);
}
#endif
static int scene_snapshot(void*c,uint64_t s,risc_scene_navigation_v1*n,uint32_t*f){
 assert(native_alive);
#ifdef PORTABLE_LISTS_SHARED_TEXT_INPUT
 if(text_granted&&ui.page==LISTS_NAME_EDIT&&scene_fault("scene-snapshot"))return RISC_SCENE_RETAINED;
#endif
 return api->snapshot(c,s,n,f);
}
static int scene_configure(void*c,uint64_t s,uint32_t flags){
 assert(native_alive);int rc=real_components->lifecycle.configure(c,s,flags);
#ifdef PORTABLE_LISTS_SHARED_TEXT_INPUT
 if(scene_opens>1&&scene_fault("scene-configure"))return RISC_SCENE_RETAINED;
#endif
 return rc;
}
static int scene_open(void*c,const risc_components_document_v1*d,const risc_scene_navigation_v1*n,uint64_t*out){
 assert(native_alive);++scene_opens;
#ifdef PORTABLE_LISTS_SHARED_TEXT_INPUT
 assert(!text_active&&!text_granted);for(unsigned i=0;i<d->node_count;i++)assert(d->nodes[i].kind!=RISC_SCENE_KEYBOARD_NODE);
#endif
 int rc=real_components->open(c,d,n,out);
#ifdef PORTABLE_LISTS_SHARED_TEXT_INPUT
 if(scene_opens>1&&scene_fault("scene-open"))return RISC_SCENE_RETAINED;
#endif
 return rc;
}
static int scene_update(void*c,uint64_t s,const risc_components_document_v1*d){
 assert(native_alive);
#ifdef PORTABLE_LISTS_SHARED_TEXT_INPUT
 assert(!text_active&&!text_granted);for(unsigned i=0;i<d->node_count;i++)assert(d->nodes[i].kind!=RISC_SCENE_KEYBOARD_NODE);
#endif
 return real_components->update(c,s,d);
}
static int scene_close(void*c,uint64_t s){
 assert(native_alive);++scene_closes;
#ifdef PORTABLE_LISTS_SHARED_TEXT_INPUT
 if(text_granted&&ui.page==LISTS_NAME_EDIT&&scene_fault("scene-close"))return RISC_SCENE_RETAINED;
#endif
 return api->close(c,s);
}
static int scene_next(void*c,uint64_t s,risc_scene_event_v1*e){
 assert(native_alive);if(inject_stale){inject_stale=false;*e=stale_event;return RISC_SCENE_OK;}return api->next(c,s,e);
}
static bool app_acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){
 (void)id;assert(native_alive&&v==1);
#ifdef PORTABLE_LISTS_SHARED_TEXT_INPUT
 if(!strcmp(n,RISC_TEXT_ENTRY_CAPABILITY))return text_test_acquire(g);
#endif
 const void*p=!strcmp(n,RISC_SCENE_CAPABILITY)?&app_scene:!strcmp(n,RISC_APP_DATA_CAPABILITY)?(const void*)&file_api:!strcmp(n,RISC_CIVIL_CLOCK_CAPABILITY)?(const void*)&civil_api:NULL;
 assert(p);g->api=p;g->slot=++app_acquires;g->generation=1;return true;
}
static bool app_release(risc_runtime_capability_v1*g){assert(native_alive&&g->api);
#ifdef PORTABLE_LISTS_SHARED_TEXT_INPUT
 if(text_test_is_grant(g))return text_test_release(g);
#endif
 assert(!session);++app_releases;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;
}
static bool app_retain(void){assert(native_alive);native_alive=false;return true;}
static bool app_exit_requested(void){
#ifdef PORTABLE_LISTS_SHARED_TEXT_INPUT
 return text_test_exit_requested();
#else
 return native_exit_pending;
#endif
}
static bool observed_touch_poll(void*c,size_t n){observe_input_poll(false);return poll(c,n);}
static bool observed_foreground(void*c,const risc_input_foreground_v1*f,size_t n){bool ok=foreground(c,f,n);if(ok)navigation_owned=n!=0;return ok;}
static void observed_startup(void){
 driver=t5_driver_get(2);assert(driver);api=driver->capability;
 static risc_input_navigation_api_v1 observed_nav;observed_nav=nav;observed_nav.foreground=observed_foreground;
 static risc_touch_api_v1 observed_raw;observed_raw=raw;observed_raw.poll=observed_touch_poll;
 risc_provider_dependency_v1 deps[]={{"display.output",1,&disp},{"input.touch.raw",1,&observed_raw},{"input.navigation",1,&observed_nav},{"platform.clock",1,&clk},{"ui.presentation-profile",1,&prof}};
 assert(driver->start(deps,5));make_document();assert(!api->open(NULL,&doc,NULL,&transport_session));
}
static int app_checkpoint(uint64_t invocation,const risc_resident_request_v1*q,risc_resident_reply_v1*r){
 assert(native_alive&&invocation==99);++checkpoints;r->flags=0;
 if(q->reason==RISC_RESIDENT_CHECKPOINT_CONTROLS)assert(!session);
 if(edit_case&&ui.page==LISTS_NAME_EDIT&&policy_mode()){
  if(q->reason==RISC_RESIDENT_CHECKPOINT_POLL){
   assert(!presenting&&!held&&!policy_input_blocked);++policy_polls;if(q->flags&RISC_RESIDENT_POLL_ACTIVITY)++policy_activity_polls;
   if(policy_armed&&(!is_mode("policy-request-reset")||!policy_request_invalidated||policy_wait>=policy_invalidated_at_wait+100)){
    r->flags=RISC_RESIDENT_REPLY_POLICY_REQUEST;
    bool continuity=!policy_sleep_mode()&&!is_mode("policy-no-marker")&&!(is_mode("policy-reply-alternate")&&policy_dispatches%2==1)&&!is_mode("policy-marker-only")&&!is_mode("policy-poll-busy-marker");
    if(continuity)r->flags|=PORTABLE_RESIDENT_REPLY_INPUT_CONTINUITY;
    if(is_mode("policy-poll-extra-marker"))r->flags|=RISC_RESIDENT_REPLY_REDRAW;
    if(is_mode("policy-poll-malformed"))r->struct_size=0;
    if(is_mode("policy-marker-only")&&policy_polls==1)r->flags=PORTABLE_RESIDENT_REPLY_INPUT_CONTINUITY;
    if(is_mode("policy-poll-busy-marker")&&policy_polls==1){r->flags|=PORTABLE_RESIDENT_REPLY_INPUT_CONTINUITY;return RISC_RESIDENT_BUSY;}
    policy_continuity_granted=r->flags==(RISC_RESIDENT_REPLY_POLICY_REQUEST|PORTABLE_RESIDENT_REPLY_INPUT_CONTINUITY);
   }
   if(is_mode("policy-poll-exit"))return RISC_RESIDENT_EXIT;
   if(is_mode("policy-poll-retained"))return RISC_RESIDENT_RETAINED;
  }
  if(q->reason==RISC_RESIDENT_CHECKPOINT_POLICY){
   assert(!session&&!presenting&&!held);++policy_dispatches;
   assert(policy_measurement_count<64);policy_measurement*m=&policy_measurements[policy_measurement_count++];
   *m=(policy_measurement){.before_touch=last_touch_poll_ms,.before_usb=last_usb_poll_ms,.start=ms,.wall_start=monotonic_ns()};
#ifdef PORTABLE_LISTS_SHARED_TEXT_INPUT
   assert(text_paused);
   if(policy_continuity_granted)assert(subscribed&&text_usb_subscribed&&navigation_owned);
   else assert(!subscribed&&!text_usb_subscribed&&!navigation_owned);
   policy_boundary_input();
#else
   assert(!subscribed&&!navigation_owned);
#endif
   bool focus_changed=is_mode("policy-marker-violation")||policy_sleep_mode()||(is_mode("policy-reply-alternate")&&policy_dispatches%2==0);
   r->flags=focus_changed?RISC_RESIDENT_REPLY_REDRAW:0;m->focus=!policy_continuity_granted;
   if(is_mode("policy-sleep-delay")){m->sleep_delay=1000;ms+=1000;}
   if(is_mode("policy-gap-delay-50")){m->transport_delay=50;ms+=50;}
   if(is_mode("policy-gap-delay-250")){m->transport_delay=250;ms+=250;}
   expected_resume_flags=1u|(policy_continuity_granted?0u:2u);
   bool busy_reply=(is_mode("policy-busy")||is_mode("policy-busy-sleep")||is_mode("policy-busy-malformed"))&&policy_dispatches==1;
#ifdef PORTABLE_LISTS_SHARED_TEXT_INPUT
   if(focus_changed&&!busy_reply)assert(observed_foreground(NULL,NULL,0));
#endif
   if(is_mode("policy-home")||is_mode("policy-sleep-home"))native_exit_pending=true;
   if(is_mode("policy-exit")||is_mode("policy-sleep-exit"))return policy_return(RISC_RESIDENT_EXIT);
   if(is_mode("policy-retained"))return policy_return(RISC_RESIDENT_RETAINED);
   if(busy_reply){if(is_mode("policy-busy-malformed")){r->struct_size=0;r->flags=RISC_RESIDENT_REPLY_REDRAW;}return policy_return(RISC_RESIDENT_BUSY);}
   if(is_mode("policy-malformed"))r->struct_size=0;
   return policy_return(RISC_RESIDENT_OK);
  }
 }
 return RISC_RESIDENT_OK;
}
static bool app_shell(risc_resident_client_v1*r){if(is_mode("no-resident"))return false;*r=(risc_resident_client_v1){.api_version=1,.struct_size=sizeof(*r),.invocation=99,.role=RISC_RESIDENT_ROLE_FOREGROUND,.checkpoint=app_checkpoint};return true;}
/* Reach all four name operations through actual navigation, without directly
 * mutating the reducer or synthesizing a new app document. */
static void edit_route(void){
 switch(phase++){
 case 0:assert(ui.page==LISTS_HOME);tap(reason==0?LA_NEW_LIST:LA_LIST,-999);break;
 case 1:if(reason==0){tap(LA_CUSTOM_LIST,-999);phase=10;}else if(reason==1)tap(LA_NEW_TASK,-999);else if(reason==2)tap(LA_OPTIONS,-999);else tap(LA_TASK,-999);break;
 case 2:tap(reason==1?LA_CUSTOM_TASK:reason==2?LA_RENAME_LIST:LA_RENAME_TASK,-999);phase=10;break;
 default:assert(0);
 }
}
static char expected_name[LISTS_NAME],initial_name[LISTS_NAME];
static const char *edit_input;
static unsigned edit_action;
static void prepare_edit(void){
 strcpy(initial_name,ui.draft);strcpy(expected_name,ui.draft);
 edit_input=is_mode("limit")?"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA":is_mode("hardware")?"Az 09!?_[]{}|~":!strncmp(mode,"frozen",6)||is_mode("delayed")||strstr(mode,"burst")?"QWERTYUIOPASDF":is_mode("empty")?"":"A";
 size_t n=strlen(expected_name);for(unsigned i=0;edit_input[i]&&n+1<LISTS_NAME;i++)expected_name[n++]=edit_input[i];expected_name[n]=0;
 if(is_mode("layers"))strcpy(expected_name,"A1q!");
 if(policy_mode()){
  size_t length=strlen(initial_name);assert(length+4<sizeof(expected_name));
  memcpy(expected_name,initial_name,length);const char*suffix=is_mode("policy-gap-done")||is_mode("policy-gap-usb-done")?"A1":is_mode("policy-gap-delay-50")||is_mode("policy-gap-delay-250")?"A198q!":is_mode("policy-gap-touch")?"A199q!":is_mode("policy-gap-touch-held")?"A19q!":
   is_mode("policy-usb-queued")||is_mode("policy-gap-usb")||is_mode("policy-gap-usb-held")?"A1Xq!":(is_mode("policy-touch-queued")||is_mode("policy-request-reset"))?"A19q!":"A1q!";
  assert(length+strlen(suffix)<sizeof(expected_name));strcpy(expected_name+length,suffix);
 }
}
static void validate_edit(bool cancel){
 lists_model decoded;assert(lists_decode(&decoded,stored,sizeof(stored)));assert(!memcmp(&decoded,&ui.data,sizeof(decoded)));
 if(cancel||is_mode("empty")||is_mode("save-failure")){assert(!memcmp(stored,initial_stored,sizeof(stored)));assert(disk_revision==1);return;}
 char normalized[LISTS_NAME];assert(!lists_name(normalized,expected_name));
 const char*name=reason==0?ui.data.lists[ui.data.list_count-1].name:reason==1?ui.data.tasks[ui.data.task_count-1].name:reason==2?lists_find_list(&ui.data,work_id)->name:lists_find_task(&ui.data,task_id)->name;
 if(strcmp(name,normalized))fprintf(stderr,"name actual=%s expected=%s\n",name,normalized);
 assert(!strcmp(name,normalized));assert(writes==1&&disk_revision==2);
 assert(ui.data.list_count==initial_model.list_count+(reason==0));assert(ui.data.task_count==initial_model.task_count+(reason==1));
}
#ifndef PORTABLE_LISTS_SHARED_TEXT_INPUT
static void legacy_edit(void){
 if(phase<10){edit_route();return;}
 if(phase==10){assert(ui.page==LISTS_NAME_EDIT);prepare_edit();phase=11;return;}
 if(phase==11){
  if(policy_mode()){
   static const unsigned keys[]={'A',RISC_SCENE_KEY_LAYER,'1',RISC_SCENE_KEY_LAYER,'q',RISC_SCENE_KEY_LAYER,'!'};
   if(edit_action==3&&(policy_wait<125||policy_dispatches<2||policy_polls<2)){assert(++policy_wait<2000);policy_armed=true;return;}
   if(is_mode("policy-repeat")&&edit_action==5){if(!policy_wait_second)policy_second_start=policy_dispatches;if(policy_wait_second<125||policy_dispatches<policy_second_start+2){assert(++policy_wait_second<2000);policy_armed=true;return;}}
   policy_armed=false;
   if(edit_action<sizeof(keys)/sizeof(keys[0])){tap(LA_KEY,(int)keys[edit_action++]);return;}
  }
  else if(is_mode("empty")&&edit_action<strlen(initial_name)){tap(LA_KEY,RISC_SCENE_KEY_BACKSPACE);edit_action++;return;}
  if(policy_mode()){}
  else if(is_mode("layers")){if(edit_action<strlen(initial_name)){tap(LA_KEY,RISC_SCENE_KEY_BACKSPACE);edit_action++;return;}static const unsigned keys[]={'A',RISC_SCENE_KEY_LAYER,'1',RISC_SCENE_KEY_LAYER,'q',RISC_SCENE_KEY_LAYER,'!'};if(edit_action-strlen(initial_name)<sizeof(keys)/sizeof(keys[0])){tap(LA_KEY,(int)keys[edit_action++-strlen(initial_name)]);return;}}
  else if(!is_mode("empty")&&edit_input[edit_action]){tap(LA_KEY,(unsigned char)edit_input[edit_action++]);return;}
  if(is_mode("cancel"))nav_pressed=RISC_NAV_BACK;else tap(LA_KEY,RISC_SCENE_KEY_DONE);phase=12;return;
 }
 assert(phase==12);if(policy_mode())assert(policy_polls>=2&&policy_dispatches>=2);validate_edit(is_mode("cancel"));nav_pressed=RISC_NAV_HOME;phase=99;
}
#endif
static void feature_flow(void){
 switch(phase++){
 case 0:assert(ui.page==LISTS_HOME);frame("home");tap(LA_LIST,-999);break;
 case 1:assert(ui.page==LISTS_VIEW);frame("list");tap(LA_DONE,1);break;
 case 2:assert(ui.data.tasks[0].done);allow_complete=false;tap(LA_FILTER,1);break;
 case 3:assert(ui.filter==1);transport_session=session;begin_frame();assert(presenting);tap(LA_FILTER,0);allow_complete=true;break;
 case 4:assert(!ui.filter);tap(LA_FILTER,1);break;
 case 5:frame("list-all");tap(LA_DONE,0);break;
 case 6:assert(!ui.data.tasks[0].done);tap(LA_TASK,-999);break;
 case 7:assert(ui.page==LISTS_TASK);frame("task");tap(LA_PRIORITY,2);break;
 case 8:assert(lists_find_task(&ui.data,ui.task)->priority==2);tap(LA_DUE,2);break;
 case 9:assert(lists_find_task(&ui.data,ui.task)->due_day==ui.today+1);tap(LA_TIME,-999);break;
 case 10:assert(ui.page==LISTS_TIME);frame("time");tap(LA_SET_TIME,-999);break;
 case 11:assert(lists_find_task(&ui.data,ui.task)->minute==540);tap(LA_REMINDER,1);break;
 case 12:assert(lists_find_task(&ui.data,ui.task)->reminder==1);tap(LA_REPEAT,1);break;
 case 13:assert(lists_find_task(&ui.data,ui.task)->repeat==1);tap(LA_DELETE_TASK,-999);break;
 case 14:assert(ui.page==LISTS_CONFIRM);frame("confirm");tap(LA_KEEP,-999);break;
 case 15:assert(ui.page==LISTS_TASK);back_input();break;
 case 16:assert(ui.page==LISTS_VIEW);tap(LA_OPTIONS,-999);break;
 case 17:assert(ui.page==LISTS_OPTIONS);frame("options");tap(LA_SHOW_DONE,1);break;
 case 18:assert(lists_find_list(&ui.data,ui.view)->show_completed);tap(LA_CLEAR_DONE,-999);break;
 case 19:assert(ui.page==LISTS_CONFIRM);tap(LA_CONFIRM,-999);break;
 case 20:assert(ui.page==LISTS_OPTIONS);back_input();break;
 case 21:tap(LA_NEW_TASK,-999);break;
 case 22:assert(ui.page==LISTS_ADD_TASK);frame("new-task");tap(LA_SUGGEST_TASK,-999);break;
 case 23:assert(ui.page==LISTS_TASK&&ui.data.task_count==4);tap(LA_DELETE_TASK,-999);break;
 case 24:tap(LA_CONFIRM,-999);break;
 case 25:assert(ui.page==LISTS_VIEW&&ui.data.task_count==3);back_input();break;
 case 26:tap(LA_NEW_LIST,-999);break;
 case 27:assert(ui.page==LISTS_ADD_LIST);frame("new-list");tap(LA_MARKER,3);break;
 case 28:assert(ui.marker==3);tap(LA_SUGGEST_LIST,-999);break;
 case 29:assert(ui.data.list_count==3&&ui.data.lists[2].marker==3);tap(LA_OPTIONS,-999);break;
 case 30:tap(LA_DELETE_LIST,-999);break;
 case 31:tap(LA_CONFIRM,-999);break;
 case 32:assert(ui.page==LISTS_HOME&&ui.data.list_count==2);tap(LA_TODAY,-999);break;
 case 33:assert(ui.view==LISTS_VIEW_TODAY);frame("today");back_input();break;
 case 34:tap(LA_ALL,-999);break;
 case 35:assert(ui.view==LISTS_VIEW_ALL);frame("all");nav_pressed=RISC_NAV_HOME;phase=99;break;
 default:assert(0);
 }
}
static void app_yield(uint32_t delay){
 assert(native_alive);ms+=delay;assert(++loops<20000);transport_session=lists_test_session();
 if(native_exit_pending){allow_complete=true;return;}
#ifdef PORTABLE_LISTS_SHARED_TEXT_INPUT
 if(edit_case){text_test_yield();return;}
#endif
 if(!session||input_pending())return;
 if(edit_case){
#ifndef PORTABLE_LISTS_SHARED_TEXT_INPUT
 legacy_edit();
#endif
 return;}
 if(is_mode("storage")){assert(ui.readonly);frame("unavailable");nav_pressed=RISC_NAV_HOME;phase=99;return;}
 if(is_mode("storage-retry")){
  if(!phase){assert(ui.readonly);storage_bad=false;tap(LA_RETRY,-999);phase++;return;}
  assert(!ui.readonly&&ui.data.list_count==2);nav_pressed=RISC_NAV_HOME;phase=99;return;
 }
 if(!strncmp(mode,"reminder",8)){
  if(phase==0){assert(ui.page==LISTS_ALERT);frame("alert");tap(is_mode("reminder-done")?LA_ALERT_DONE:LA_SNOOZE,-999);phase=1;return;}
  if(is_mode("reminder-done")){assert(ui.data.task_count==4&&ui.data.tasks[0].done&&ui.data.tasks[0].spawned);nav_pressed=RISC_NAV_HOME;phase=99;return;}
  if(phase==1){assert(ui.data.tasks[0].snooze==(uint32_t)ui.today*1440+670);clock_minute=670;phase=2;return;}
  if(phase==2){if(ui.page!=LISTS_ALERT)return;frame("alert-snoozed");tap(LA_DISMISS,-999);phase=3;return;}
  assert(ui.page==LISTS_HOME&&ui.data.tasks[0].acknowledged==lists_reminder_deadline(&ui.data.tasks[0]));nav_pressed=RISC_NAV_HOME;phase=99;return;
 }
 feature_flow();
}
static const risc_runtime_api_v1 app_runtime={.api_version=1,.struct_size=sizeof(app_runtime),.yield_ms=app_yield,.acquire=app_acquire,.release=app_release,.retain_invocation=app_retain,.exit_requested=app_exit_requested,.resident_shell=app_shell};
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){return v==1&&native_alive?&app_runtime:NULL;}
#ifdef PORTABLE_LISTS_SHARED_TEXT_INPUT
#include "shared_input_fixture.inc"
#endif
int main(int argc,char**argv){
 assert(argc==4);device=argv[1];folder=argv[2];scenario=argv[3];edit_case=!strncmp(scenario,"edit-",5);mode=edit_case?scenario+7:scenario;
 if(edit_case){assert(scenario[5]>='0'&&scenario[5]<='3'&&scenario[6]=='-');reason=(unsigned)(scenario[5]-'0');}
 storage_bad=is_mode("storage")||is_mode("storage-retry");write_bad=is_mode("save-failure");write_unknown=is_mode("save-unknown");write_loss=is_mode("save-loss");
 if(!strcmp(device,"paper")){w=800;h=480;format=1;prof=(risc_scene_profile_v1){1,sizeof(prof),1,3,88,20,0,65535,0,270,0,0};}
 observed_startup();assert(!api->close(NULL,transport_session));expected_intent=RISC_DISPLAY_PRESENT_LOW_LATENCY;
 real_components=risc_scene_components_get_v1(api);assert(real_components);app_scene=*real_components;app_scene.open=scene_open;app_scene.update=scene_update;app_scene.lifecycle.base.next=scene_next;app_scene.lifecycle.base.close=scene_close;app_scene.lifecycle.base.snapshot=scene_snapshot;app_scene.lifecycle.configure=scene_configure;
 lists_init(&initial_model);assert(!lists_add_list(&initial_model,"WORK",1,&work_id));assert(!lists_add_list(&initial_model,"HOME",2,&home_id));
 assert(!lists_add_task(&initial_model,work_id,"SEND REPORT",lists_day(2026,10,10),&task_id));assert(!lists_add_task(&initial_model,work_id,"REVIEW PR",-1,NULL));assert(!lists_add_task(&initial_model,home_id,"WATER PLANTS",-1,NULL));if(!strncmp(mode,"reminder",8)){initial_model.tasks[0].minute=660;initial_model.tasks[0].reminder=1;initial_model.tasks[0].repeat=1;}assert(lists_encode(&initial_model,stored));memcpy(initial_stored,stored,sizeof(stored));
#ifdef PORTABLE_LISTS_SHARED_TEXT_INPUT
 text_test_setup();
#endif
 app_main();
#ifdef PORTABLE_LISTS_SHARED_TEXT_INPUT
 text_test_finish();
#endif
 if(native_alive){assert(app_acquires==app_releases&&!held&&!presenting&&!subscribed&&!navigation_owned);assert(driver->quiesce());driver->stop();}
 char path[1024];snprintf(path,sizeof(path),"%s/%s-%s.bin",folder,device,scenario);FILE*f=fopen(path,"wb");assert(f&&fwrite(stored,1,sizeof(stored),f)==sizeof(stored)&&!fclose(f));
 if(policy_measurement_count){
  snprintf(path,sizeof(path),"%s/%s-%s.timing.json",folder,device,scenario);f=fopen(path,"w");assert(f);
  fputs("{\"measurement\":\"host transport doubles; virtual input time and synchronous fixture callback wall time\",\"hardware_latency_measured\":false,\"physical_sampling_pauses_during_synchronous_policy\":true,\"policy_samples\":[",f);
  for(unsigned i=0;i<policy_measurement_count;i++){
   const policy_measurement*m=&policy_measurements[i];assert(m->end-m->start==m->sleep_delay+m->transport_delay);
   fprintf(f,"%s{\"focus_changed\":%s,\"status\":%d,\"touch_poll_before_ms\":%llu,\"touch_poll_after_ms\":%llu,\"usb_poll_before_ms\":%llu,\"usb_poll_after_ms\":%llu,\"policy_start_ms\":%llu,\"policy_end_ms\":%llu,\"intentional_sleep_ms\":%u,\"simulated_transport_delay_ms\":%u,\"touch_poll_resumed\":%s,\"usb_poll_resumed\":%s,\"fixture_callback_wall_ns\":%llu}",i?",":"",m->focus?"true":"false",m->status,(unsigned long long)m->before_touch,(unsigned long long)m->after_touch,(unsigned long long)m->before_usb,(unsigned long long)m->after_usb,(unsigned long long)m->start,(unsigned long long)m->end,m->sleep_delay,m->transport_delay,m->after_touch?"true":"false",m->after_usb?"true":"false",(unsigned long long)m->wall_ns);
  }
  assert(fputs("]}\n",f)>=0&&!fclose(f));
 }
 printf("Lists actual app_main %s %s PASS (writes=%u scene_opens=%u closes=%u)\n",device,scenario,writes,scene_opens,scene_closes);return 0;
}
