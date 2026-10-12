#include "RiscBatteryGaugeV1.h"
#include "RiscInputNavigationV1.h"
static bool lifecycle_navigation(void*,risc_input_navigation_frame_v1*);
#define TIMECARD_TEST_NAV_CALLBACK lifecycle_navigation
static bool lifecycle_battery(void *,risc_battery_sample_v1 *);
static int lifecycle_sleep(void);
#define TIMECARD_TEST_BATTERY_CALLBACK lifecycle_battery
#define TIMECARD_TEST_SLEEP_CALLBACK lifecycle_sleep
#define main ordinary_watch_main
#include "timecard_shared_watch_host_test.c"
#undef main
#include "PortableLowBattery.h"
#include "timecard_policy_background_fixture.h"
static unsigned lifecycle_samples,lifecycle_sleeps,lifecycle_start,lifecycle_duration;
static bool lifecycle_low,lifecycle_unsupported;
static unsigned lifecycle_unsupported_seen;
static int lifecycle_sleep(void){
 assert(tcp_editor&&tcp_text_active&&policy_paused&&!policy_continuity&&!frames&&!subs);
 lifecycle_sleeps++;return 1;
}
static bool lifecycle_battery(void *c,risc_battery_sample_v1 *s){
 (void)c;if(tcp_text_active){assert(policy_paused||lifecycle_background);lifecycle_samples++;}
 *s=(risc_battery_sample_v1){.percent=lifecycle_low&&tcp_text_active?9:73,.millivolts=3970};return true;
}
static void lifecycle_yield(uint32_t n){fake_yield(n);if(tcp_text_active)ticks+=1000;if(lifecycle_busy_frame&&tcp_text_active&&lifecycle_context_steps){host_busy=(ticks-lifecycle_start)<4000&&lifecycle_start!=0;}}
static bool lifecycle_brightness(void *c,uint16_t level,uint16_t maximum){(void)c;assert(maximum==100&&level<=100);if(tcp_text_active)assert(policy_paused&&!frames);return true;}
static int32_t lifecycle_next(void *c,uint64_t session,risc_scene_event_v1 *e){
 /* Fill a bounded draft first, then hold it across elapsed-time boundaries. */
 if(text_at==6){
  if(!lifecycle_start)lifecycle_start=ticks;
  if((uint32_t)(ticks-lifecycle_start)<lifecycle_duration)return RISC_SCENE_IDLE;
 }
 return scene_next(c,session,e);
}
static bool lifecycle_watch_poll(t5_app_input_t *in,uint32_t wait){bool ok=watch_poll(in,wait);if(lifecycle_unsupported&&tcp_editor&&!tcp_text_active){assert(strstr(tcp_editor_status,"unavailable"));lifecycle_unsupported_seen++;in->buttons=T5_APP_BUTTON_BACK;}return ok;}
int main(int argc,char **argv){
 assert(argc==2);const char *kind=argv[1];scenario="cancel";script_cancel=true;
 lifecycle_background=strstr(kind,"background")!=NULL;lifecycle_message=!strcmp(kind,"background-message");lifecycle_background_fail=!strcmp(kind,"background-retained");lifecycle_busy_frame=!strcmp(kind,"background-busy");
 lifecycle_duration=90000;lifecycle_low=!strcmp(kind,"low-battery");if(lifecycle_low)lifecycle_duration=12000;if(lifecycle_background)lifecycle_duration=10000;
 if(!strcmp(kind,"pause-again"))policy_pause_again=3;
 if(!strcmp(kind,"pause-retained"))policy_pause_fault=RISC_SCENE_RETAINED;
 if(!strcmp(kind,"resume-retained"))policy_resume_fault=RISC_SCENE_RETAINED;
 if(!strcmp(kind,"unsupported")){policy_configure_fault=RISC_SCENE_INVALID;lifecycle_unsupported=true;}
 memset(pixels,0xa5,sizeof(pixels));directory=NULL;stop_poll=1000;
 watch_display=display_api;watch_display.get_info=watch_display_info;watch_display.set_brightness=lifecycle_brightness;watch_alarm=alarm_api;watch_alarm.status=watch_alarm_status;
 lc_bt=fx_broadcast;lc_bt.step=lc_broadcast;
 if(lifecycle_background){const uint8_t enabled[]={'C',1,1,0xa4};fake_put(NULL,"contexts_on",enabled,4);}
 shared_runtime=runtime_api;shared_runtime.acquire=lifecycle_acquire;shared_runtime.release=watch_release;shared_runtime.retain_invocation=watch_retain;shared_runtime.yield_ms=lifecycle_yield;
 text_driver=t5_driver_get(2);assert(text_driver);text_api=text_driver->capability;
 risc_scene_api_v1 selected_scene=scene_api;selected_scene.next=lifecycle_next;
 risc_provider_dependency_v1 dependency={RISC_SCENE_CAPABILITY,1,timecard_policy_scene_api(&selected_scene)};
 assert(text_driver->start(&dependency,1));
 assert(app_module_init()==0);adapter_app=__real_t5_app_get_api(1);watch_app=*adapter_app;watch_app.poll=lifecycle_watch_poll;
 app_main();
 if(watch_retained){
  assert(strstr(kind,"retained"));unsigned h=host_calls,g=grants,p=presents;tcp_text_step();app_module_fini();assert(h==host_calls&&g==grants&&p==presents&&!history_writes&&!launch_count);
 }else{
  assert(!host_active&&!tcp_text_active&&!tcp_editor&&text_driver->quiesce());text_driver->stop();app_module_fini();assert(!grants&&!frames&&!subs&&!history_writes);
  if(!strcmp(kind,"unsupported")){assert(!policy_pauses&&!lifecycle_samples&&lifecycle_unsupported_seen==1);}
  else if(lifecycle_background){assert(lifecycle_context_steps&&lifecycle_capture_calls&&lifecycle_broadcast_steps);if(lifecycle_busy_frame)assert(lifecycle_capture_busy);if(lifecycle_message)assert(lifecycle_message_sent&&lifecycle_message_dismissals>=2&&policy_full_pauses);}
  else if(lifecycle_low){bool low=false;assert(portable_low_battery_load(&kv_api,&low)&&low);assert(lifecycle_samples>=2&&!lifecycle_sleeps&&policy_pauses==policy_resumes);}
  else assert(lifecycle_sleeps>=1&&lifecycle_samples>=17&&policy_full_pauses>=1&&policy_pauses==policy_resumes);
 }
 if(lifecycle_background)printf("background steps=%u capture=%u busy-capture=%u broadcast=%u message-dismiss=%u\n",lifecycle_context_steps,lifecycle_capture_calls,lifecycle_capture_busy,lifecycle_broadcast_steps,lifecycle_message_dismissals);
 printf("Timecard Watch policy %s PASS samples=%u sleeps=%u pauses=%u full=%u resumes=%u retained=%u\n",kind,lifecycle_samples,lifecycle_sleeps,policy_pauses,policy_full_pauses,policy_resumes,watch_retained);return 0;
}
