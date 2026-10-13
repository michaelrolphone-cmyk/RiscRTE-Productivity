#define main original_points_main
#define TEST_EXPECT_FAST_PAPER
#include "points_catalog_app_test.c"
#undef main
int main(int argc,char **argv){
 assert(argc==2);const char *s=argv[1];start();
 if(!strcmp(s,"missing"))text_api.checkpoint_tag=0;
 if(!strcmp(s,"unavailable"))policy_enable_result=RISC_TEXT_ENTRY_UNAVAILABLE;
 assert(points_editor_begin_type(&editor,0));pc_page(PC_CUSTOM);strcpy(editor.type.name,"Kept draft");pc_custom_action(0);
 assert(name_client.active&&!writes);ticks+=1000;
 if(!strcmp(s,"missing")||!strcmp(s,"unavailable")){
  pc_name_step();assert(!name_client.active&&!writes&&!strcmp(editor.type.name,"Kept draft"));finish();return 0;
 }
 assert(policy_enables==1);resident_test_request_policy=true;
 if(!strcmp(s,"resident-poll-exit"))resident_test_exit=true;
 if(!strcmp(s,"resident-policy-exit"))policy_exit_during_apply=true;
 if(!strcmp(s,"continuity"))policy_continuity=true;
 if(!strcmp(s,"pending")||!strcmp(s,"pending-home")||!strcmp(s,"pending-alarm"))policy_pending=2;
 if(!strcmp(s,"busy"))policy_pause_result=RISC_TEXT_ENTRY_BUSY;
 if(!strcmp(s,"retained-pause"))policy_pause_result=RISC_TEXT_ENTRY_RETAINED;
 if(!strcmp(s,"retained-resume"))policy_resume_result=RISC_TEXT_ENTRY_RETAINED;
 if(!strcmp(s,"again-resume"))policy_resume_result=RISC_TEXT_ENTRY_AGAIN;
 if(!strcmp(s,"presenting"))policy_snapshot_flags=RISC_TEXT_ENTRY_CHECKPOINT_PRESENTING;
 if(!strcmp(s,"input-busy"))policy_snapshot_flags=RISC_TEXT_ENTRY_CHECKPOINT_INPUT_BUSY|RISC_TEXT_ENTRY_CHECKPOINT_ACTIVITY;
 if(!strcmp(s,"activity"))policy_snapshot_flags=RISC_TEXT_ENTRY_CHECKPOINT_ACTIVITY;
 unsigned before=resident_test_policies;pc_name_step();
 if(!strcmp(s,"resident-poll-exit")||!strcmp(s,"resident-policy-exit")){
  unsigned redraws=frames;pc_name_step();assert(exit_app&&!name_client.active&&!writes&&!launches&&frames==redraws);
  resident_test_exit=false;finish();return 0;
 }
 if(strstr(s,"retained")||!strcmp(s,"again-resume")){
  assert(retained&&terminal&&retains==1&&name_client.active&&!writes);
  unsigned n=calls,policies=resident_test_policies;pc_name_step();pc_close();app_module_fini();assert(calls==n&&resident_test_policies==policies);return 0;
 }
 if(!strcmp(s,"pending-home")||!strcmp(s,"pending-alarm")){
  assert(name_checkpoint.pausing&&resident_test_policies==before);
  if(!strcmp(s,"pending-home"))policy_runtime_exit=true;else fake_alarm_state=ALARM_STATE_ALERT;
  pc_name_step();assert(!name_client.active&&!writes&&!launches&&resident_test_policies==before);
  assert(exit_app==!strcmp(s,"pending-home"));fake_alarm_state=ALARM_STATE_READY;policy_runtime_exit=false;finish();return 0;
 }
 if(!strcmp(s,"pending")){assert(name_checkpoint.pausing&&resident_test_policies==before);pc_name_step();assert(resident_test_policies==before);pc_name_step();}
 if(!strcmp(s,"busy")||!strcmp(s,"presenting")||!strcmp(s,"input-busy")){
  assert(resident_test_policies==before&&policy_resumes==0);policy_pause_result=0;policy_snapshot_flags=0;ticks+=1000;pc_name_step();
 }
 assert(resident_test_policies==before+1&&policy_resumes==1&&!policy_paused&&name_client.active&&!writes);
 assert(policy_focus==!policy_continuity);assert(!!(policy_resume_flags&RISC_TEXT_ENTRY_RESUME_FOCUS_CHANGED)==!policy_continuity);
 if(!strcmp(s,"activity"))assert(policy_poll_flags&RISC_RESIDENT_POLL_ACTIVITY);
 text_complete(RISC_TEXT_ENTRY_ACCEPTED,"New draft");assert(!writes&&!strcmp(editor.type.name,"New draft"));
 finish();printf("X4 Points modal %s PASS\n",s);return 0;
}
