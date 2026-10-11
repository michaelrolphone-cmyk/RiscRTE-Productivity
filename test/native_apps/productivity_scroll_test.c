#define main baseline_main
#ifdef TEST_POINTS_SCROLL
#include "points_native_adapter_test.c"
#define scroll pe_scroll
#define prepare pe_scroll_prepare
#define redraw draw
static bool handle(const t5_app_input_t *input){return pe_input(input);}
static bool list_open(void){return page==PAGE_LIST;}
static void activate_row_check(unsigned row){unsigned order[POINTS_MAX];nova_configured(order);assert(page==PAGE_EDIT&&selected==order[row]);}
static void begin(void){
 assert(app_module_init()==0);app=t5_app_get_api(1);paper=paper_presentation_get();ready=open_dependencies();load_catalog();
 for(unsigned i=0;i<POINTS_MAX;++i)writer.saved.points[i]=(points_item){.kind=POINTS_WORK_START,.enabled=1,.hour=8+i,.minute=i,.weekdays=127};
 page=PAGE_LIST;pe_clean=true;draw();
}
static void end(void){close_dependencies();app_module_fini();assert(!live&&!subs&&!frames&&!retained&&!puts_count);}
#else
#include "timecard_native_time_test.c"
#define scroll tcs
#define prepare tcs_prepare
#define redraw tcp_draw
static bool handle(const t5_app_input_t *input){tcp_input(input);return tcp_dirty;}
static bool list_open(void){return screen_id==SCREEN_WEEK_LIST;}
static void activate_row_check(unsigned row){assert(screen_id==SCREEN_WEEK&&week_offset==-(int)row);}
static void begin(void){zone("UTC");start();}
static void end(void){assert(!data_writes);finish();}
#endif
#undef main
static const char *frame_dir;
static uint8_t initial_pixels[48000];
static void check_header(void){
 for(int y=0;y<scroll.motion.view.y;++y)for(unsigned x=0;x<480;++x){unsigned px=(unsigned)y,py=479-x;
  if(getenv("TEST_PAPER_FLIP")){px=799-px;py=479-py;}
  unsigned bit=1u<<(7-px%8),at=py*100+px/8;assert((pixels[at]&bit)==(initial_pixels[at]&bit));
 }
}
static void capture(const char *name) {
 if(!frame_dir)return;
 char path[512];snprintf(path,sizeof(path),"%s/%s.pbm",frame_dir,name);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P4\n480 800\n");
 for(unsigned y=0;y<800;++y)for(unsigned x=0;x<480;x+=8){unsigned byte=0;for(unsigned bit=0;bit<8;++bit){unsigned px=y,py=479-x-bit;
  if(getenv("TEST_PAPER_FLIP")){px=799-px;py=479-py;}
  if(pixels[py*100+px/8]&(1u<<(7-px%8)))byte|=1u<<(7-bit);
 }fputc((int)byte,f);}fclose(f);
}
static void step(bool down,unsigned x,unsigned y,unsigned elapsed) {
 sc_down=down;sc_x=x;sc_y=y;ticks+=elapsed;t5_app_input_t input={0};assert(app->poll(&input,1));if(handle(&input))redraw();
}
static void settle(void){sc_busy=false;step(false,120,200,20);step(false,120,200,20);productivity_scroll_visible(&scroll);}
static void tap(unsigned x,unsigned y){step(true,x,y,20);step(false,x,y,20);}
static void drag(void){step(true,120,490,20);step(true,120,415,20);assert(portable_scroll_offset(&scroll.motion)==75);step(true,120,330,20);assert(portable_scroll_offset(&scroll.motion)==160);}
int main(int argc,char **argv) {
 assert(argc>=2);const char *test=argv[1];frame_dir=argc>2?argv[2]:NULL;sc_enabled=true;begin();settle();capture("initial");memcpy(initial_pixels,pixels,sizeof(pixels));
 if(!strcmp(test,"drag")) {
  drag();assert(list_open());check_header();capture("drag-held");step(false,120,330,20);int prior=portable_scroll_offset(&scroll.motion),v=scroll.motion.velocity_q8;assert(v>0);
  step(false,120,330,16);assert(portable_scroll_offset(&scroll.motion)>prior&&scroll.motion.velocity_q8<v);capture("momentum");
  for(unsigned i=0;i<100;++i)step(false,120,330,16);
  assert(!scroll.motion.velocity_q8&&list_open());assert(portable_scroll_offset(&scroll.motion)<=scroll.motion.limit);
 }else if(!strcmp(test,"bounds")) {
  step(true,120,160,20);step(true,120,560,20);assert(!portable_scroll_offset(&scroll.motion));step(false,120,560,100);
  scroll.motion.position_q8=scroll.motion.limit*256;scroll.dirty=true;redraw();settle();step(true,120,490,20);step(true,120,120,20);assert(portable_scroll_offset(&scroll.motion)==scroll.motion.limit);step(false,120,120,100);assert(!scroll.motion.velocity_q8);capture("bottom");
 }else if(!strcmp(test,"tap")){tap(120,scroll.motion.view.y+88+40);activate_row_check(1);capture("selected");
 }else if(!strcmp(test,"busy-hit")) {
  sc_busy=true;scroll.motion.position_q8=176*256;scroll.dirty=true;redraw();assert(!paper_frame_ready());
  step(true,120,scroll.motion.view.y+40,20);assert(scroll.hit_row==0);sc_busy=false;
  step(false,120,scroll.motion.view.y+40,20);activate_row_check(0);capture("old-frame-selected");
 }else if(!strcmp(test,"busy-drag")) {
  sc_busy=true;scroll.motion.position_q8=20*256;scroll.dirty=true;redraw();unsigned submitted=presents;
  step(true,120,500,20);step(true,120,400,20);step(true,120,320,20);assert(portable_scroll_offset(&scroll.motion)==(scroll.motion.limit<200?scroll.motion.limit:200)&&presents==submitted&&scroll.dirty);
  sc_busy=false;step(true,120,300,20);assert(presents==submitted+1&&portable_scroll_offset(&scroll.motion)==(scroll.motion.limit<220?scroll.motion.limit:220));step(false,120,300,100);capture("busy-latest");
 }else if(!strcmp(test,"stop-tap")) {
  drag();step(false,120,330,20);assert(scroll.motion.velocity_q8);tap(120,220);assert(list_open()&&!scroll.motion.velocity_q8);
 }else if(!strcmp(test,"queued-up")) {
  step(true,120,450,20);sc_up=true;step(false,120,320,20);assert(portable_scroll_offset(&scroll.motion)==130&&list_open());
 }else if(!strcmp(test,"cancelled")||!strcmp(test,"replaced")) {
  step(true,120,450,20);sc_invalid=!strcmp(test,"cancelled");sc_replace=!strcmp(test,"replaced");step(true,120,350,20);sc_invalid=sc_replace=false;step(false,120,350,20);assert(list_open()&&!scroll.motion.velocity_q8);
 }else if(!strcmp(test,"reordered")||!strcmp(test,"deleted")||!strcmp(test,"changed")) {
#ifndef TEST_POINTS_SCROLL
  open_week(0);int32_t date=sunday(0);days[0]=blank_day(date);days[0].punches[0]=480;
  days[1]=blank_day(add_days(date,1));days[1].punches[0]=510;day_count=2;redraw();settle();
#endif
  step(true,120,scroll.motion.view.y+40,20);
#ifdef TEST_POINTS_SCROLL
  if(!strcmp(test,"reordered")){points_item row=writer.saved.points[0];writer.saved.points[0]=writer.saved.points[1];writer.saved.points[1]=row;}
  else if(!strcmp(test,"deleted"))writer.saved.points[0]=(points_item){0};else writer.saved.points[0].minute++;
#else
  if(!strcmp(test,"reordered")){tc_day_t row=days[0];days[0]=days[1];days[1]=row;}
  else if(!strcmp(test,"deleted")){days[0]=days[1];day_count=1;}else days[0].punches[0]++;
#endif
  step(false,120,scroll.motion.view.y+40,20);
#ifdef TEST_POINTS_SCROLL
  assert(list_open());
#else
  if(!strcmp(test,"reordered"))assert(screen_id==SCREEN_DAY&&editing_ymd==date);
  else assert(screen_id==SCREEN_WEEK);
#endif
 }else if(!strcmp(test,"horizontal")) {
  step(true,180,330,20);step(true,100,335,20);step(false,100,335,20);assert(list_open()&&!portable_scroll_offset(&scroll.motion));
 }else if(!strcmp(test,"footer-drag")) {
  step(true,350,730,20);step(true,350,400,20);step(false,350,400,20);assert(list_open()&&!portable_scroll_offset(&scroll.motion));
 }else if(!strcmp(test,"fit")) {
#ifdef TEST_POINTS_SCROLL
  for(unsigned i=3;i<POINTS_MAX;++i)writer.saved.points[i]=(points_item){0};
#else
  open_day(20261004);
#endif
  redraw();settle();assert(!scroll.motion.limit);step(true,120,430,20);step(true,120,200,20);step(false,120,200,20);assert(!portable_scroll_offset(&scroll.motion));capture("fits");
 }else if(!strcmp(test,"keyboard")) {
#ifdef TEST_POINTS_SCROLL
  nova_new_point();custom_kind=POINTS_CUSTOM_1;custom_draft=writer.meta;page=PAGE_CUSTOM_KEYBOARD;pe_key_page=0;pe_key_text[0]=0;
#else
  open_day(20261004);selected=0;tcp_activate();tcp_entry[0]=0;
#endif
  redraw();settle();assert(!scroll.motion.limit);step(true,80,320,20);step(true,80,200,20);step(false,80,200,20);
#ifdef TEST_POINTS_SCROLL
  assert(!pe_key_text[0]&&page==PAGE_CUSTOM_KEYBOARD);tap(80,250);assert(strlen(pe_key_text)==1);
#else
  assert(!tcp_entry[0]&&tcp_editor);tap(80,270);assert(strlen(tcp_entry)==1);
#endif
  capture("keyboard");
 }else if(!strcmp(test,"editor-list")) {
#ifdef TEST_POINTS_SCROLL
  edit_slot(0);page=PAGE_EDIT;redraw();settle();assert(scroll.motion.count==9);
  scroll.motion.position_q8=scroll.motion.limit*256;scroll.dirty=true;redraw();settle();capture("edit-bottom");
  unsigned y=(unsigned)portable_scroll_row_y(&scroll.motion,8)+40;sc_busy=true;tap(120,y);assert(nova_delete_confirm&&!puts_count);
  tap(120,y);assert(nova_delete_confirm&&!puts_count);sc_busy=false;settle();
  pe_edit_action(6);redraw();settle();assert(page==PAGE_DAYS&&scroll.motion.count==10);
  scroll.motion.position_q8=scroll.motion.limit*256;scroll.dirty=true;redraw();settle();capture("days-bottom");
  tap(120,(unsigned)portable_scroll_row_y(&scroll.motion,9)+40);assert(draft.weekdays==65);
#else
  open_week(0);redraw();settle();assert(scroll.motion.count==11);scroll.motion.position_q8=scroll.motion.limit*256;scroll.dirty=true;redraw();settle();capture("week-bottom");
#endif
 }else if(!strcmp(test,"keyboard-busy")) {
#ifdef TEST_POINTS_SCROLL
  nova_new_point();custom_kind=POINTS_CUSTOM_1;custom_draft=writer.meta;page=PAGE_CUSTOM_KEYBOARD;pe_key_page=0;pe_key_text[0]=0;
#else
  open_day(20261004);selected=0;tcp_activate();tcp_entry[0]=0;
#endif
  redraw();settle();step(true,80,270,20);
#ifdef TEST_POINTS_SCROLL
  pe_key_page=1;
#else
  tcp_key_page=1;
#endif
  step(false,80,270,20);
#ifdef TEST_POINTS_SCROLL
  assert(!pe_key_text[0]);
#else
  assert(!tcp_entry[0]);
#endif
 }else if(!strcmp(test,"back")) {
  step(true,120,330,20);step(true,200,332,20);step(false,200,332,20);
#ifdef TEST_POINTS_SCROLL
  assert(pe_exit);
#else
  assert(tcp_home);
#endif
 }else if(!strcmp(test,"modal")) {
  drag();step(false,120,330,20);sc_enabled=false;motion_begin(100);
  while(ticks-motion_start<3800){t5_app_input_t input={0};assert(app->poll(&input,20));if(handle(&input))redraw();}
  assert(list_open()&&!scroll.motion.velocity_q8&&motion_opens==2&&motion_closes==2);
  motion_enabled=false;sc_enabled=true;settle();capture("after-modal");
 }else if(!strcmp(test,"confirm-hidden")) {
  scroll.motion.position_q8=scroll.motion.limit*256;scroll.dirty=true;redraw();settle();
  t5_app_input_t input={.buttons=T5_APP_BUTTON_CONFIRM};handle(&input);assert(list_open());redraw();settle();handle(&input);activate_row_check(0);
 }else assert(!"Unknown scenario");
 sc_down=sc_busy=false;assert(paper_frame_drain());end();printf("%s passed: native pixels, input, bounds and custody\n",test);return 0;
}
