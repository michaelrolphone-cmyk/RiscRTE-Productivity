#include "RiscResidentShellV1.h"
static bool lifecycle_resident_get(risc_resident_client_v1 *);
#define TIMECARD_TEST_RESIDENT_GET lifecycle_resident_get
#define TIMECARD_POLICY_TEST
/* The included fixture names its native compatibility entry separately. */
#define main ordinary_shared_main
#include "timecard_shared_host_test.c"
#undef main
#include "PortableResidentPolicyInput.h"
static const char *lifecycle_kind;
static unsigned lifecycle_polls,lifecycle_policies,lifecycle_activity;
static bool lifecycle_dispatch_continuity,lifecycle_race;
static bool lifecycle_continuity,lifecycle_queue,lifecycle_exit,lifecycle_retain,lifecycle_bad_redraw;
static int32_t lifecycle_checkpoint(uint64_t invocation,const risc_resident_request_v1 *q,risc_resident_reply_v1 *r){
 assert(invocation==2&&q->struct_size==sizeof(*q)&&r->struct_size==sizeof(*r));
 if(q->reason==RISC_RESIDENT_CHECKPOINT_POLL){
  lifecycle_polls++;if(q->flags&RISC_RESIDENT_POLL_ACTIVITY)lifecycle_activity++;
  r->flags=tcp_text_active?RISC_RESIDENT_REPLY_POLICY_REQUEST:0;
  if(r->flags&&lifecycle_continuity)r->flags|=PORTABLE_RESIDENT_REPLY_INPUT_CONTINUITY;
  lifecycle_dispatch_continuity=lifecycle_continuity;
  if(r->flags&&lifecycle_race){hid(4,0);lifecycle_race=false;}
 }else if(q->reason==RISC_RESIDENT_CHECKPOINT_POLICY){
  assert(tcp_text_active&&policy_paused&&!subs&&!frames);
  assert(usb_active==lifecycle_dispatch_continuity);lifecycle_policies++;
  if(lifecycle_retain)return RISC_RESIDENT_RETAINED;
  if(lifecycle_exit)return RISC_RESIDENT_EXIT;
  r->flags=!lifecycle_dispatch_continuity||lifecycle_bad_redraw?RISC_RESIDENT_REPLY_REDRAW:RISC_RESIDENT_REPLY_CONFIGURATION_CHANGED;
  if(lifecycle_queue){hid(4,0);lifecycle_queue=false;}
 }
 return RISC_RESIDENT_OK;
}
static bool lifecycle_resident_get(risc_resident_client_v1 *out){
 *out=(risc_resident_client_v1){.api_version=1,.struct_size=sizeof(*out),.invocation=2,.role=RISC_RESIDENT_ROLE_FOREGROUND,.checkpoint=lifecycle_checkpoint};return true;
}
int main(int argc,char **argv){
 assert(argc==2);lifecycle_kind=argv[1];
 lifecycle_continuity=strstr(lifecycle_kind,"continuity")!=NULL;
 lifecycle_queue=strstr(lifecycle_kind,"queue")!=NULL;
 lifecycle_exit=!strcmp(lifecycle_kind,"exit");lifecycle_retain=!strcmp(lifecycle_kind,"retained");lifecycle_bad_redraw=!strcmp(lifecycle_kind,"continuity-invalid-redraw");
 lifecycle_race=!strcmp(lifecycle_kind,"pause-busy");
 initialize();begin(0);
 if(!strcmp(lifecycle_kind,"pause-again")||!strcmp(lifecycle_kind,"continuity-latched"))policy_pause_again=3;
 if(!strcmp(lifecycle_kind,"pause-retained"))policy_pause_fault=RISC_SCENE_RETAINED;
 if(!strcmp(lifecycle_kind,"resume-retained"))policy_resume_fault=RISC_SCENE_RETAINED;
 usb_connected=true;tcp_text_step();
 unsigned before_polls=lifecycle_polls;
 for(unsigned i=0;i<12&&!retained&&tcp_text_active&&!tcp_text_closing;i++){
  ticks+=1000;if(!strcmp(lifecycle_kind,"activity"))policy_activity=true;tcp_text_step();
  if(!strcmp(lifecycle_kind,"continuity-latched")&&tcp_text_checkpoint.pausing)lifecycle_continuity=false;
  if(lifecycle_exit&&lifecycle_policies)break;
 }
 if(retained){assert(strstr(lifecycle_kind,"retained")||lifecycle_bad_redraw);terminal();}
 else if(lifecycle_exit){assert(tcp_text_closing&&policy_paused&&!policy_resumes);tcp_text_step();assert(!tcp_editor&&!tcp_text_active&&!data_writes);end();}
 else{
  assert(lifecycle_polls>before_polls&&lifecycle_policies&&policy_pauses==policy_resumes);
  if(!strcmp(lifecycle_kind,"continuity-latched"))assert(policy_full_pauses&&policy_full_pauses<policy_pauses);
  else if(lifecycle_continuity)assert(!policy_full_pauses&&!usb_unsubs);
  else assert(policy_full_pauses==policy_pauses&&usb_unsubs==policy_pauses);
  if(strstr(lifecycle_kind,"queue")){assert(!strcmp(scene_document.nodes[0].text,lifecycle_continuity?"a":""));hid(5,0);tcp_text_step();assert(!strcmp(scene_document.nodes[0].text,lifecycle_continuity?"ab":"b"));}
  if(!strcmp(lifecycle_kind,"pause-busy"))assert(!strcmp(scene_document.nodes[0].text,"a")&&lifecycle_polls>lifecycle_policies);
  if(!strcmp(lifecycle_kind,"activity"))assert(lifecycle_activity);
  key(RISC_SCENE_KEY_CANCEL);end();
 }
 printf("Timecard X4 policy %s PASS polls=%u policy=%u pauses=%u full=%u resumes=%u retained=%u\n",lifecycle_kind,lifecycle_polls,lifecycle_policies,policy_pauses,policy_full_pauses,policy_resumes,retained);return 0;
}
