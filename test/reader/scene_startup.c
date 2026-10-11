/* Real scene provider with its existing display/input fixture. No scene is
 * opened here: Reader must establish one before querying page geometry. */
#define main scene_fixture_main
#define risc_runtime_get_api scene_fixture_runtime_api
#include "host_test.c"
#undef risc_runtime_get_api
#undef main
#include "RiscScenePageV1.h"
static unsigned exit_after_frame;
void reader_test_scene_allow_exit(void){exit_after_frame=submits+1;}

const void *reader_test_scene_start(void) {
    w=800;h=480;format=1;
    prof=(risc_scene_profile_v1){1,sizeof(prof),1,3,88,20,0,0xffff,0,270,0,0};
    driver=t5_driver_get(2);api=driver->capability;
    risc_provider_dependency_v1 deps[]={{"display.output",1,&disp},{"input.touch.raw",1,&raw},{"input.navigation",1,&nav},{"platform.clock",1,&clk},{"ui.presentation-profile",1,&prof}};
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
