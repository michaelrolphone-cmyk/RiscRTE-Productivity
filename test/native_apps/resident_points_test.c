#define TEST_EXPECT_FAST_PAPER
#define main points_preservation_main
#include "points_catalog_app_test.c"
#undef main
static void poll_once(void){t5_app_input_t in={0};assert(render_app->poll(&in,20));}
static void pull(void){key_contact(0,0,0,0);key_contact(1,240,20,21);key_contact(1,240,120,21);key_contact(0,0,0,0);settle();}
int main(int argc,char **argv) {
 assert(argc==2);const char *scenario=argv[1];
 if(!strncmp(scenario,"name-home",9)){
  bool pending=strstr(scenario,"pending")!=NULL, failure=strstr(scenario,"retention")!=NULL;
  for(unsigned pass=0;pass<(failure?1u:2u);pass++){
   start();pc_close();script_mode=5;script_at=0;text_pending=pending?3:0;
   text_close_error=failure?RISC_TEXT_ENTRY_RETAINED:0;
   unsigned before=writes,closes=text_closes;
   app_main();assert(script_at==1&&writes==before&&!launches);
   if(failure){
    assert(terminal&&retained&&retains==1&&text_opened&&name_client.active);
    before=calls;pc_name_step();pc_close();app_main();app_module_fini();assert(calls==before);
   }else{
    assert(!retained&&!acquired&&!name_client.active&&!text_opened);
    assert(text_closes==closes+(pending?4u:1u));
    script_mode=0;app_module_fini();assert(!live_grants&&!live_subs&&!live_frames);
    start();assert(!editor.editing_event&&!editor.editing_type&&page==PC_LIST&&writes==before);finish();
   }
  }
  printf("Actual app_main shared-keyboard Home discard and cleanup %s PASS\n",scenario);return 0;
 }
 if(!strncmp(scenario,"home-entry-",11)){
  home_test_page=(unsigned)strtoul(scenario+11,NULL,10);
  for(unsigned pass=0;pass<2;pass++){
   start();pc_close();script_mode=4;script_at=0;app_main();
   assert(script_at==2&&!retained&&!acquired&&!writes);
   script_mode=0;app_module_fini();assert(!live_grants&&!live_subs&&!live_frames);
  }
  printf("Actual app_main Home page %u exits cleanly twice PASS\n",home_test_page);return 0;
 }
 if(strcmp(scenario,"draft")&&strcmp(scenario,"uncertain")&&strcmp(scenario,"home")&&strcmp(scenario,"failure")&&strcmp(scenario,"preservation"))return points_preservation_main(argc,argv);
 if(!strcmp(scenario,"preservation"))return points_preservation_main(1,argv);
 start();assert(resident_test_polls>0&&!resident_test_controls);
 if(!strcmp(scenario,"draft")){
  assert(points_editor_begin_event(&editor,0));pc_page(PC_EDIT);editor.event.hour=7;paint();pull();
  assert(editor.editing_event&&editor.event.hour==7&&!writes&&!resident_test_controls&&!portable_app_before_launch("default.elf")&&portable_app_before_home());
  resident_test_request_policy=true;for(unsigned i=0;i<4;i++)poll_once();
  assert(resident_test_policies==1&&editor.editing_event&&editor.event.hour==7&&!writes);
  points_editor_cancel_event(&editor);pc_page(PC_LIST);paint();pull();assert(resident_test_controls==1);
 }else if(!strcmp(scenario,"uncertain")){
  assert(points_editor_begin_event(&editor,0));pc_page(PC_EDIT);unknown_save=true;pc_save(false);paint();pull();
  assert(editor.store.uncertain&&writes==1&&!resident_test_controls&&!portable_app_before_launch("default.elf")&&portable_app_before_home());
  read_failure=unknown_save=false;pc_retry();paint();pull();assert(!editor.store.uncertain&&writes==1&&resident_test_controls==1);
 }else if(!strcmp(scenario,"home")){
  nav_home=true;t5_app_input_t in={0};assert(render_app->poll(&in,20)&&in.exit_requested&&!launches);
 }else if(!strcmp(scenario,"failure")){
  resident_test_fail=true;t5_app_input_t in={0};assert(!render_app->poll(&in,20)&&portable_adapter_retained());
  unsigned before=calls;pc_load();pc_draw();pc_close();app_module_fini();assert(calls==before);puts("Points resident terminal dispatch fence PASS");return 0;
 }else assert(0);
 finish();printf("Points resident %s PASS\n",scenario);return 0;
}
