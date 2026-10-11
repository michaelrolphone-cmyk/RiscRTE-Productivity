#define main timecard_preservation_main
#include "timecard_native_time_test.c"
#undef main
static void poll_once(void){t5_app_input_t in={0};assert(app->poll(&in,20));}
int main(int argc,char **argv) {
 assert(argc==2);const char *scenario=argv[1];
 if(scenario[0]>='0'&&scenario[0]<='9')return timecard_preservation_main(argc,argv);
 zone("UTC");start();poll_once();
 if(!strcmp(scenario,"draft")){
  open_day(20261004);selected=0;tcp_activate();strcpy(tcp_entry,"7:45 PM");tcp_draw();assert(tcp_editor);
  assert(!portable_app_before_launch("default.elf"));
  resident_test_request_policy=true;for(unsigned i=0;i<4;i++)poll_once();
  assert(resident_test_policies==1&&tcp_editor&&!strcmp(tcp_entry,"7:45 PM")&&!data_writes);
  tcp_editor_cancel();assert(portable_app_before_launch("default.elf"));
 }else if(!strcmp(scenario,"uncertain")){
  commit_error=true;assert(!tcp_mutate(20261004,0,510));assert(tcp_data.status==RISC_APP_DATA_COMMIT_UNKNOWN&&!portable_app_before_launch("default.elf"));
  commit_error=false;assert(tcp_reload()&&portable_app_before_launch("default.elf"));
 }else if(!strcmp(scenario,"home")){
  home_requested=true;bool exited=false;
  for(unsigned i=0;i<5;i++){t5_app_input_t in={0};assert(app->poll(&in,20));if(in.exit_requested){exited=true;break;}}
  assert(exited&&!launches&&!data_writes);
 }else if(!strcmp(scenario,"failure")){
  resident_test_fail=true;t5_app_input_t in={0};assert(!app->poll(&in,20)&&portable_adapter_retained());
  unsigned before=calls;tcp_draw();tcp_dependencies_close();app_module_fini();assert(calls==before);puts("Timecard resident terminal dispatch fence PASS");return 0;
 }else assert(0);
 finish();printf("Timecard resident %s PASS\n",scenario);return 0;
}
