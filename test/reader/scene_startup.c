/* Real scene provider with its existing display/input fixture. No scene is
 * opened here: Reader must establish one before querying page geometry. */
#define main scene_fixture_main
#define risc_runtime_get_api scene_fixture_runtime_api
#include "host_test.c"
#undef risc_runtime_get_api
#undef main
#include "RiscScenePageV1.h"
static unsigned exit_after_frame;
static unsigned clean_frames;
static bool reader_submit(void *c,uint64_t f,const risc_display_rect_v1*d,size_t n,const risc_display_present_options_v1*o,uint64_t*t){
 assert(o->intent==RISC_DISPLAY_PRESENT_CLEAN||o->intent==RISC_DISPLAY_PRESENT_LOW_LATENCY);
 if(o->intent==RISC_DISPLAY_PRESENT_CLEAN){assert(!d&&!n&&o->queue_policy==RISC_DISPLAY_QUEUE_FIFO);++clean_frames;}
 expected_intent=o->intent;return submit(c,f,d,n,o,t);
}
static risc_display_output_api_v1 reader_display;
unsigned reader_test_scene_clean_count(void){return clean_frames;}
void reader_test_scene_allow_exit(void){exit_after_frame=submits+1;}
uint64_t reader_test_scene_now(void){return ms;}
void reader_test_scene_checkpoint(void){assert(!subscribed&&!presenting&&!held);}

const void *reader_test_scene_start(void) {
    w=800;h=480;format=1;
    prof=(risc_scene_profile_v1){1,sizeof(prof),1,3,88,20,0,0xffff,0,270,0,0};
    driver=t5_driver_get(2);api=driver->capability;
    reader_display=disp;reader_display.submit=reader_submit;
    risc_provider_dependency_v1 deps[]={{"display.output",1,&reader_display},{"input.touch.raw",1,&raw},{"input.navigation",1,&nav},{"platform.clock",1,&clk},{"ui.presentation-profile",1,&prof}};
    assert(driver->start(deps,5));
    risc_scene_page_geometry_v1 g={.struct_size=sizeof(g)};
    assert(risc_scene_page_get_v1(api)->geometry(NULL,&g)==RISC_SCENE_UNAVAILABLE);
    expected_intent=RISC_DISPLAY_PRESENT_LOW_LATENCY;
    return api;
}
void reader_test_scene_tick(unsigned amount) {
    ms+=amount;
    if(ms>=200000){fprintf(stderr,"scene timeout submits=%u polls=%u presenting=%d subscribed=%d nav=%u calls=%u\n",submits,polls,presenting,subscribed,nav_pressed,calls);abort();}
    if(exit_after_frame && submits>=exit_after_frame && ms>1000)nav_pressed=RISC_NAV_HOME;
}
void reader_test_scene_finish(void) {
    assert(submits>=1 && !subscribed && !presenting && !held);
    assert(driver->quiesce());driver->stop();
}
