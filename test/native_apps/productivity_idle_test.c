/* Real app controllers and separately linked System adapter. Only the native
 * sleep result is stubbed: helper/Runtime custody has its own test suite. */
#define main baseline_main
#ifdef TEST_POINTS_IDLE
#include "points_native_adapter_test.c"
#define scroll pe_scroll
#define redraw draw
static void handle(const t5_app_input_t *input){if(pe_input(input))draw();}
static void begin(void) {
 assert(app_module_init()==0);app=t5_app_get_api(1);paper=paper_presentation_get();
 ready=open_dependencies();assert(ready);load_catalog();
 for(unsigned i=0;i<POINTS_MAX;i++)writer.saved.points[i]=(points_item){.kind=POINTS_WORK_START,.enabled=1,.hour=8+i,.minute=i,.weekdays=127};
 page=PAGE_LIST;pe_clean=true;draw();
}
static void editor(void){edit_slot(0);draft.hour=9;draft.minute=45;draft.weekdays=65;draw();}
static void finish(void){close_dependencies();app_module_fini();assert(!live&&!subs&&!frames&&!retained&&!puts_count);}
static void check_editor(void){assert(page==PAGE_EDIT&&selected==0&&draft.hour==9&&draft.minute==45&&draft.weekdays==65&&writer.saved.points[0].hour==8&&writer.saved.points[0].minute==0);}
static void check_list(void){assert(page==PAGE_LIST&&writer.saved.points[0].hour==8);}
static void check_writes(void){assert(!puts_count);}
static void frozen(void) {
 assert(retained&&barriers==1&&portable_adapter_retained());unsigned at=calls;
 t5_app_input_t input={0};assert(!app->poll(&input,20));draw();retry_action();save_action();
 close_dependencies();app_module_fini();assert(calls==at&&!puts_count);
}
#else
#include "timecard_native_time_test.c"
#define scroll tcs
#define redraw tcp_draw
static void handle(const t5_app_input_t *input){tcp_input(input);if(tcp_dirty)tcp_draw();}
static void begin(void){zone("UTC");start();open_week(-3);tcp_draw();}
static void editor(void){open_day(20261004);selected=1;tcp_activate();strcpy(tcp_entry,"7:45 PM");tcp_key_page=2;tcp_draw();}
static void check_editor(void){assert(tcp_editor&&screen_id==SCREEN_DAY&&editing_ymd==20261004&&selected==1&&tcp_key_page==2&&!strcmp(tcp_entry,"7:45 PM"));}
static void check_list(void){assert(!tcp_editor&&screen_id==SCREEN_WEEK&&week_offset==-3);}
static void check_writes(void){assert(!data_writes&&!puts_count);}
#endif
#undef main

static int sleep_result=1;
static unsigned sleeps,paused_before_sleep,disables_before_sleep;
static bool expect_radios,expect_broadcast,wake_contact;
static unsigned nav_before_sleep;
int portable_app_idle_sleep(const risc_runtime_api_v1 *runtime,
 const risc_display_output_api_v1 *display,const risc_battery_gauge_api_v1 *gauge,
 const alarm_service_v1 *alarms) {
 fx_io();assert(runtime&&runtime->api_version==1&&runtime->retain_invocation==fx_retain);assert(display==&fx_display);assert(gauge==&fx_gauge);assert(alarms&&alarms->api_version==2);
 assert(!frames&&!subs&&!native_live&&!retained&&!idle_broadcast_grants);
 assert(!bt_active&&bt_pauses>paused_before_sleep);
 assert(idle_navigation_resets>nav_before_sleep);
 /* Automatic idle leaves saved background intent for the typed helper. It
  * must not apply the separate manual-sleep Bluetooth-Off policy first. */
 assert(idle_bluetooth==expect_radios&&idle_radio_disables==disables_before_sleep);
 check_writes();sleeps++;
 if(sleep_result>=0){idle_bluetooth=false;ticks+=3000;if(wake_contact){sc_down=true;sc_x=120;sc_y=250;}}
 return sleep_result;
}

static void seed(void) {
 uint8_t radio=(uint8_t)(expect_radios?PORTABLE_RADIO_WIFI|PORTABLE_RADIO_BLUETOOTH:0);
 cells[12].instance=1;strcpy(cells[12].key,PORTABLE_RADIO_KEY);cells[12].size=4;
 memcpy(cells[12].bytes,(uint8_t[]){0x51,1,radio,(uint8_t)(radio^0xa5)},4);
 cells[13].instance=1;strcpy(cells[13].key,TELEMETRY_BROADCAST_KEY);cells[13].size=4;
 memcpy(cells[13].bytes,(uint8_t[]){0x62,1,expect_broadcast,(uint8_t)(expect_broadcast^0xa5)},4);
}
static bool last_poll_broadcast;
static bool poll_only(unsigned elapsed) {
 ticks+=elapsed;t5_app_input_t input={0};bool ok=app->poll(&input,1);
 if(ok){last_poll_broadcast=bt_active;assert(!input.buttons&&!input.tapped&&!input.exit_requested);handle(&input);}
 return ok;
}
static void settled(void) {
 assert(paper_frame_drain());assert(poll_only(20));assert(paper_frame_drain());
 productivity_scroll_visible(&scroll);
}
int main(int argc,char **argv) {
 assert(argc==2);const char *test=argv[1];
 bool editing=!strstr(test,"list");expect_radios=strcmp(test,"radios-off")!=0;
 expect_broadcast=expect_radios;wake_contact=!strcmp(test,"wake-contact");
 if(strstr(test,"refused"))sleep_result=0;
 if(!strcmp(test,"retained"))sleep_result=-2;
 sc_enabled=true;seed();begin();settled();if(editing)editor();settled();
 if(scroll.motion.limit){scroll.motion.position_q8=128*256;scroll.dirty=true;redraw();settled();}
 int saved_scroll=scroll.motion.position_q8;assert(saved_scroll||editing);
 if(editing)check_editor();else check_list();
 assert(idle_bluetooth==expect_radios);check_writes();
 /* Drive a real foreground service tick before arming timeout. */
 assert(poll_only(1001));assert(bt_active==expect_broadcast);assert(!idle_broadcast_grants);
 paused_before_sleep=bt_pauses;nav_before_sleep=idle_navigation_resets;
 disables_before_sleep=idle_radio_disables;unsigned initial_sleeps=sleeps;
 if(!strcmp(test,"active-contact")) {
  sc_down=true;sc_x=10;sc_y=330;assert(poll_only(20));
  assert(poll_only(60001));assert(sleeps==initial_sleeps&&subs);
  sc_down=false;assert(poll_only(20));settled();
  saved_scroll=scroll.motion.position_q8;
 } else if(!strcmp(test,"pending-frame")) {
  ticks+=60001;sc_busy=true;paper_presentation_get()->begin();
  app->fill_rect(10,110,10,10,true);app->present(true);assert(!paper_frame_ready());
  assert(poll_only(20));assert(sleeps==initial_sleeps&&subs);
  sc_busy=false;assert(paper_frame_drain());
 } else {
  assert(poll_only(58000));assert(sleeps==initial_sleeps);
 }
 if(!strcmp(test,"pause-retained"))bt_pause_fail=true;
 bool ok=poll_only(60001);
 if(sleep_result==-2||bt_pause_fail) {
  assert(!ok&&sleeps==(bt_pause_fail?0u:1u));
  unsigned held=live,subscriptions=subs,at=calls;
  t5_app_input_t input={0};assert(!app->poll(&input,20)&&calls==at);frozen();
  assert(live==held&&subs==subscriptions);check_writes();
  printf("%s: no subsequent provider I/O, release, save or cleanup\n",test);return 0;
 }
 assert(ok&&sleeps==1&&!retained&&subs==1&&!frames&&!native_live&&!idle_broadcast_grants);
 assert(scroll.motion.position_q8==saved_scroll);if(editing)check_editor();else check_list();
 assert(idle_bluetooth==expect_radios);check_writes();
 if(wake_contact){assert(poll_only(20));sc_down=false;assert(poll_only(20));check_editor();check_writes();}
 /* A repeated poll must observe the refreshed deadline, and telemetry may
  * resume only on the next ordinary service tick using the saved policy. */
 assert(paper_frame_drain());assert(poll_only(20));assert(sleeps==1&&last_poll_broadcast==expect_broadcast);
 assert(scroll.motion.position_q8==saved_scroll);check_writes();
 if(editing)check_editor();else check_list();
 sc_down=false;assert(paper_frame_drain());finish();
 printf("%s: automatic Light result %d preserves controller, scroll, radio intent and input neutrality\n",test,sleep_result);
 return 0;
}
