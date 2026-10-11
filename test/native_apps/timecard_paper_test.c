/* Real Timecard controller + separately linked, unmodified System adapter. */
#include "timecard_paper_fixture.h"
const t5_app_api_v1 *tcp_source_app_api(uint32_t version);
#define TIMECARD_FILE_STORAGE_EXTERNAL
#include "../../Apps/timecard_portable.c"
static char history[JSON_CAPACITY];
static unsigned history_writes;
static bool read_ok=true,write_ok=true,commit_then_error,provide_files=true,clock_ok=true;
static bool tc_exists(const char *path){assert(!strcmp(path,STORE_PATH));return true;}
static bool tc_read(const char *path,void *out,size_t cap,size_t *size){assert(tc_exists(path)&&!frames);*size=0;if(!read_ok)return false;size_t n=strlen(history);if(n>cap)return false;memcpy(out,history,n);*size=n;return true;}
static bool tc_write(const char *path,const void *data,size_t size){assert(tc_exists(path)&&size<sizeof(history)&&!frames);history_writes++;if(write_ok||commit_then_error){memcpy(history,data,size);history[size]=0;}return write_ok;}
static const t5_storage_api_v1 tc_files={.api_version=1,.struct_size=sizeof(tc_files),.exists=tc_exists,.read_file=tc_read,.write_file_atomic=tc_write};
const t5_storage_api_v1 *timecard_portable_file_storage(void){return provide_files?&tc_files:NULL;}
static void tap(unsigned at,int x,int y){for(unsigned i=0;i<2;i++){actions[action_count].at=at+i;actions[action_count].x=x;actions[action_count].y=y;action_count++;}}
static void reset(const char *path){
 directory=path;memset(pixels,0xa5,sizeof(pixels));memset(cells,0,sizeof(cells));ticks=polls=grants=frames=subs=presents=0;
 action_count=launch_count=launch_at=history_writes=crown_at=home_at=primary_at=lost_at=kv_writes=radio_acquires=0;stop_poll=120;
 expected_launch="springboard.elf";read_ok=write_ok=provide_files=clock_ok=true;commit_then_error=false;
 strcpy(history,"{\"days\":[{\"d\":20261004,\"in\":480}]}");
}
static void open_editor(void){tap(5,100,165);tap(15,100,165);tap(25,100,165);}
static void run(void){assert(app_module_init()==0);app_main();app_module_fini();assert(!grants&&!frames&&!subs&&!radio_acquires&&!tcp_editor&&!tcp_entry[0]);}
static void setup_controller(void){
 assert(app_module_init()==0);app=t5_app_get_api(1);tcp_paper=paper_presentation_get();assert(tcp_paper);
 tcp_dependencies_open();tcp_files=&tc_files;storage=&tcp_store;system_api=&tcp_clock;system_ui=&tcp_system_ui;fwui=&tcp_ui;
 tcp_home=tcp_editor=tcp_external_exit=false;tcp_paper_first=0;tcp_draw_screen=UINT32_MAX;screen_id=SCREEN_WEEK_LIST;selected=week_offset=editing_ymd=0;
 assert(tcp_reload());tcp_draw();
}
static void close_controller(void){tcp_dependencies_close();app_module_fini();assert(!frames&&!grants&&!subs);}
int main(int argc,char **argv){
 assert(argc==3);unsigned scenario=(unsigned)atoi(argv[1]);reset(argv[2]);
 if(scenario==0){
  open_editor();tap(35,340,735); /* save unchanged value */
  tap(45,100,165);tap(55,340,735); /* repeat editing */
  tap(65,75,740);tap(75,75,740);tap(85,75,740);run();
  assert(history_writes==2&&launch_count==1&&!strcmp(history,"{\"days\":[{\"d\":20261004,\"in\":480}]}"));
 }else if(scenario>=1&&scenario<=8){
  /* Global Home from weeks, week, day and dirty editor, via both providers. */
  unsigned depth=(scenario-1)%4;for(unsigned d=0;d<depth;d++)tap(5+d*10,100,165);
  if(depth==3)tap(35,50,265);
  if(scenario<=4)home_at=45;else primary_at=45;
  expected_launch="default.elf";run();assert(launch_count==1&&!history_writes);
 }else if(scenario==9){
  open_editor();tap(35,50,265);crown_at=45;
  tap(55,75,740);tap(65,75,740);tap(75,75,740);run();assert(launch_count==1&&!history_writes);
 }else if(scenario==10){
  open_editor();tap(35,50,265);lost_at=36;tap(45,75,740);
  tap(55,75,740);tap(65,75,740);tap(75,75,740);run();assert(launch_count==1&&!history_writes);
 }else if(scenario==11){
  open_editor();tap(30,340,630); /* Remove M, retain the partial draft through QuickActions. */
  tap(35,200,20);actions[action_count-1].y=100;
  tap(45,130,345); /* unavailable sound: no prefs write */
  tap(55,340,450); /* unavailable Wi-Fi */
  tap(65,340,553); /* unavailable torch */
  tap(75,240,675);tap(80,60,350);tap(85,340,735);home_at=95;expected_launch="default.elf";run();
  assert(history_writes==1&&launch_count==1&&!kv_writes);
 }else if(scenario==12){
  setup_controller();open_day(20261004);tcp_activate();assert(tcp_editor);strcpy(tcp_entry,"9:15");
  write_ok=false;tcp_editor_done();assert(tcp_editor&&!store_ready&&strstr(tcp_editor_status,"unconfirmed"));tcp_draw();
  write_ok=true;tcp_editor_done();assert(!tcp_editor&&get_day(20261004).punches[0]==555);
  for(unsigned i=0;i<10;i++){tcp_activate();assert(tcp_editor);strcpy(tcp_entry,"7:30 PM");tcp_editor_cancel();assert(!tcp_editor&&get_day(20261004).punches[0]==555);}
  tcp_activate();strcpy(tcp_entry,"");tcp_editor_done();assert(!tcp_editor&&day_count==0);tcp_draw();close_controller();
 }else if(scenario==13){
  setup_controller(); /* Every week/day/action and every keyboard cell remains reachable. */
  for(unsigned i=0;i<20;i++){selected=(int32_t)i;tcp_draw();assert(tcp_paper_first==i/5*5);}
  open_week(0);for(unsigned i=0;i<11;i++){selected=(int32_t)i;tcp_draw();assert(tcp_paper_first==i/5*5);}
  open_day(20261004);for(unsigned i=0;i<4;i++){selected=(int32_t)i;tcp_activate();for(unsigned p=0;p<5;p++){tcp_key_page=p;tcp_entry[0]=0;tcp_draw();for(unsigned k=0;k<20;k++){unsigned c=32+p*20+k;if(c>126)break;tcp_entry[0]=0;tpp_tap(60+(int)(k%5)*84,268+(int)(k/5)*82);assert((unsigned char)tcp_entry[0]==c);}}tcp_editor_cancel();}
  assert(!history_writes);close_controller();
 }else if(scenario==14){
  provide_files=false;tap(5,75,740);run();assert(launch_count==1&&!history_writes);
 }else if(scenario==15){
  setup_controller();read_ok=false;assert(!tcp_reload());tcp_draw();assert(!store_ready&&!history_writes);read_ok=true;tcp_activate();assert(store_ready);close_controller();
 }else if(scenario==16){
  open_editor();tap(35,50,265);stop_poll=40;run();assert(!launch_count&&!history_writes);
 }else if(scenario==17){
  setup_controller();open_day(20261004);tcp_activate();strcpy(tcp_entry,"11:25 PM");commit_then_error=true;write_ok=false;tcp_editor_done();assert(tcp_editor&&!store_ready);tcp_draw();write_ok=true;commit_then_error=false;tcp_editor_done();assert(!tcp_editor&&get_day(20261004).punches[0]==1405);close_controller();
 }else assert(!"Unknown scenario");
 printf("Paper Timecard %u passed: %u frames, %u history writes, %u launches\n",scenario,presents,history_writes,launch_count);return 0;
}
