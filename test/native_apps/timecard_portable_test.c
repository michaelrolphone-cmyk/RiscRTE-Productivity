/* Real Timecard source, portable controller and production NOVA glyph renderer. */
#define TIMECARD_FILE_STORAGE_EXTERNAL
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../Apps/timecard_portable.c"
static uint16_t test_pixels[244*240];
static struct {unsigned frame,stride_bytes;void *pixels;} surface={1,488,test_pixels};
static bool list_mode;
static int width(void){return 240;}
static int height(void){return 240;}
static void clear_color(uint32_t color){assert(!color);for(unsigned y=0;y<240;y++)for(unsigned x=0;x<240;x++)test_pixels[y*244+x]=0;}
#include "RiscDisplayOutputV1.h"
static const uint32_t surface_format=RISC_DISPLAY_FORMAT_RGB565;
static void np_pixel(int x,int y,uint32_t rgb,unsigned alpha){(void)x;(void)y;(void)rgb;(void)alpha;assert(!"Watch fixture must stay RGB565");}
#include "nova_ui.inc"
static char persisted[JSON_CAPACITY];
static bool file_present,read_ok=true,write_ok=true,write_then_error,rtc_ok=true,provide_files=true;
static unsigned writes,reads,clock_reads,presentations,launches,grants,poll_count;
static t5_app_contact_t held;
static twatch_rtc_time_v1 now={2026,10,5,1,8,30,0};
static bool exists(const char *path){assert(!strcmp(path,STORE_PATH));return file_present;}
static bool read_file(const char *path,void *out,size_t cap,size_t *size){assert(!strcmp(path,STORE_PATH));reads++;*size=0;if(!read_ok)return false;size_t n=strlen(persisted);if(n>cap)return false;memcpy(out,persisted,n);*size=n;return true;}
static bool write_file(const char *path,const void *data,size_t size){assert(!strcmp(path,STORE_PATH));assert(size<sizeof(persisted));writes++;if(write_ok||write_then_error){memcpy(persisted,data,size);persisted[size]=0;file_present=true;}return write_ok;}
static const t5_storage_api_v1 file_api={1,sizeof(file_api),exists,read_file,write_file,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL};
#ifndef TIMECARD_APP_DATA
const t5_storage_api_v1 *timecard_portable_file_storage(void){return provide_files?&file_api:NULL;}
#else
static const risc_app_data_v1 *fixture_appdata_api(void);
#endif
static bool rtc_read(void *c,twatch_rtc_time_v1 *out){(void)c;clock_reads++;if(!rtc_ok)return false;*out=now;return true;}
static const twatch_rtc_api_v1 rtc_api={.api_version=2,.struct_size=sizeof(rtc_api),.read=rtc_read};
static int32_t kv_get(void *c,const char *key,void *out,uint32_t cap,uint32_t *size){(void)c;assert(!strcmp(key,"time_format")&&cap==4);uint8_t value[]={0x54,1,1,0xa4};memcpy(out,value,4);*size=4;return 0;}
static const risc_key_value_v1 pref_api={1,sizeof(pref_api),NULL,kv_get,NULL};
static bool acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *grant){
#ifdef TIMECARD_APP_DATA
 if(!strcmp(name,RISC_APP_DATA_CAPABILITY)){assert(version==1&&instance==1);if(!provide_files)return false;grant->api=fixture_appdata_api();grants++;return true;}
#endif
 if(!strcmp(name,"rtc.clock")){assert(version==2&&instance==0);grant->api=&rtc_api;}else {assert(!strcmp(name,"storage.key-value")&&version==1&&instance==1);grant->api=&pref_api;}grants++;return true;}
static bool release(risc_runtime_capability_v1 *grant){assert(grant->api&&grants);grant->api=NULL;grants--;return true;}
static bool launch(const char *name){assert(!strcmp(name,"springboard.elf"));launches++;return true;}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.request_launch=launch,.acquire=acquire,.release=release};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){assert(version==1);return &runtime;}
static bool back_exits=true;
static int view_width=240,view_height=240;
static int32_t api_width(void){return view_width;}
static int32_t api_height(void){return view_height;}
static void set_back(bool value){back_exits=value;}
static void present(bool full){(void)full;presentations++;for(unsigned y=0;y<240;y++)for(unsigned x=240;x<244;x++)assert(test_pixels[y*244+x]==0xa5a5);}
static bool touch(t5_app_contact_t *out){*out=held;return true;}
static bool poll(t5_app_input_t *out,uint32_t wait){(void)wait;*out=(t5_app_input_t){0};poll_count++;if(poll_count==2)out->buttons=T5_APP_BUTTON_BACK;return poll_count<4;}
static const t5_app_api_v1 app_api={.abi_version=1,.struct_size=sizeof(app_api),.screen_width=api_width,.screen_height=api_height,.present=present,.poll=poll,.set_back_exits_app=set_back,.touch_contact=touch};
const t5_app_api_v1 *t5_app_get_api(uint32_t version){assert(version==1);return &app_api;}
static void init(void){
 memset(test_pixels,0xa5,sizeof(test_pixels));memset(days,0,sizeof(days));memset(loaded_days,0,sizeof(loaded_days));
 view_width=view_height=240;app=&app_api;storage=&tcp_store;system_api=&tcp_clock;system_ui=&tcp_system_ui;fwui=&tcp_ui;tcp_files=&file_api;tcp_rtc=&rtc_api;
 screen_id=SCREEN_WEEK_LIST;week_offset=selected=editing_ymd=day_count=0;store_ready=false;status_text[0]=0;
 tcp_home=tcp_editor=tcp_external_exit=false;tcp_dirty=true;tcp_scroll=0;tcp_draw_screen=UINT32_MAX;tcp_reset_gesture();
 tcp_clock_valid=tcp_read_datetime(&tcp_snapshot);tcp_time_format=PORTABLE_TIME_FORMAT_12;
 file_present=false;read_ok=write_ok=rtc_ok=provide_files=true;write_then_error=false;writes=reads=0;held=(t5_app_contact_t){0};assert(tcp_reload());
}
static void frame(const char *directory,const char *name){
 if(!directory)return;
 char path[1024];snprintf(path,sizeof(path),"%s/%s.ppm",directory,name);FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n240 240\n255\n");
 for(unsigned y=0;y<240;y++)for(unsigned x=0;x<240;x++){uint16_t p=test_pixels[y*244+x];unsigned char rgb[]={(p>>11)*255/31,((p>>5)&63)*255/63,(p&31)*255/31};assert(fwrite(rgb,1,3,f)==3);}
 assert(!fclose(f));
}
static void input_contact(bool down,int x,int y,bool tap){held=(t5_app_contact_t){down,(int16_t)x,(int16_t)y};t5_app_input_t in={.tapped=tap,.touch_x=(int16_t)x,.touch_y=(int16_t)y};tcp_input(&in);}
static void storage_tests(void){
 init();assert(tcp_mutate(20261005,0,480));assert(writes==1&&find_day(20261005)->punches[0]==480);
 char before[JSON_CAPACITY];strcpy(before,persisted);write_ok=false;assert(!tcp_mutate(20261005,0,600));assert(!store_ready&&find_day(20261005)->punches[0]==480&&!strcmp(before,persisted));
 assert(strstr(status_text,"unconfirmed"));unsigned count=writes;assert(!tcp_mutate(20261006,0,500)&&writes==count);
 assert(tcp_reload());write_then_error=true;assert(!tcp_mutate(20261005,0,700));assert(!store_ready&&find_day(20261005)->punches[0]==480);assert(tcp_reload()&&find_day(20261005)->punches[0]==700);
 write_then_error=false;write_ok=true;strcpy(before,persisted);strcpy(persisted,"{\"days\":[{\"d\":20260230,\"in\":480}]}");assert(!tcp_reload()&&!store_ready&&day_count==1&&find_day(20261005)->punches[0]==700);strcpy(persisted,before);assert(tcp_reload());
 read_ok=false;assert(!tcp_reload()&&day_count==1);read_ok=true;assert(tcp_reload());
 init();day_count=MAX_DAYS;for(int i=0;i<MAX_DAYS;i++){days[i]=blank_day(add_days(20240101,i));days[i].punches[0]=480;}
 tc_day_t first=days[0];assert(save_store());count=writes;assert(!tcp_mutate(20261005,0,480));assert(writes==count&&day_count==MAX_DAYS&&!memcmp(&first,&days[0],sizeof(first)));assert(!strcmp(status_text,"History full: 400 days"));
 assert(tcp_mutate(days[399].ymd,3,1020));assert(tcp_reload()&&day_count==400);
 int32_t cleared=days[0].ymd;assert(tcp_mutate(cleared,0,-1));assert(day_count==399&&!find_day(cleared));
 assert(tcp_mutate(20261005,0,510)&&day_count==400);assert(tcp_reload()&&day_count==400);
}
static void editor_tests(void){
 init();assert(tcp_mutate(20261005,0,480));open_day(20261005);tcp_activate();assert(tcp_editor&&!strcmp(tcp_entry,"8:00 AM"));unsigned count=writes;
 strcpy(tcp_entry,"12:345");tcp_editor_done();assert(tcp_editor&&writes==count);strcpy(tcp_entry,"12:30 junk");tcp_editor_done();assert(tcp_editor&&writes==count);
 strcpy(tcp_entry,"18:45");tcp_editor_done();assert(!tcp_editor&&find_day(20261005)->punches[0]==1125);
 tcp_time_format=PORTABLE_TIME_FORMAT_24;tcp_activate();assert(tcp_editor&&!strcmp(tcp_entry,"18:45"));strcpy(tcp_entry,"");tcp_editor_done();assert(!tcp_editor&&get_day(20261005).punches[0]==-1);
 tcp_activate();strcpy(tcp_entry,"7:45 AM");count=writes;tcp_editor_cancel();assert(writes==count&&get_day(20261005).punches[0]==-1);
 tcp_activate();tcp_cookie=0;tcp_editor_done();assert(tcp_editor&&writes==count);tcp_editor_cancel();
 tcp_activate();strcpy(tcp_entry,"9:15");write_ok=false;tcp_editor_done();assert(tcp_editor&&!store_ready&&strstr(tcp_editor_status,"unconfirmed"));write_ok=true;tcp_editor_done();assert(!tcp_editor&&get_day(20261005).punches[0]==555);
 for(unsigned page=0;page<PWK_PAGES;page++)for(unsigned key=0;key<PWK_CHARACTERS;key++){unsigned ch=portable_watch_key_character(page,key);assert(!ch||(ch>=32&&ch<=126));}
 tcp_activate();memset(tcp_entry,'1',12);tcp_entry[12]=0;tcp_key_page=0;tcp_key(0);assert(strlen(tcp_entry)==12);tcp_editor_cancel();
}
static void ui_tests(const char *directory){
 init();tcp_draw();frame(directory,"weeks");assert(tcp_render_rows==20&&tcp_render_height==48);
 uint16_t top[82][240],bottom[60][240];for(unsigned y=0;y<82;y++)memcpy(top[y],test_pixels+y*244,sizeof(top[y]));for(unsigned y=180;y<240;y++)memcpy(bottom[y-180],test_pixels+y*244,sizeof(bottom[0]));
 for(unsigned offset=1;offset<=200;offset++){tcp_scroll=(int)offset;tcp_draw();for(unsigned y=0;y<82;y++)assert(!memcmp(top[y],test_pixels+y*244,sizeof(top[y])));for(unsigned y=180;y<240;y++)assert(!memcmp(bottom[y-180],test_pixels+y*244,sizeof(bottom[0])));}
 tcp_scroll=0;tcp_draw();input_contact(false,0,0,false);input_contact(true,90,150,false);input_contact(true,90,100,false);assert(tcp_scroll==50);input_contact(false,90,100,true);assert(screen_id==SCREEN_WEEK_LIST&&tcp_scroll==50);
 selected=0;tcp_activate();assert(screen_id==SCREEN_WEEK);tcp_draw();frame(directory,"week-12h");assert(tcp_render_rows==11);selected=7;tcp_activate();assert(find_day(20261005)->punches[0]==510);
 selected=1;tcp_activate();assert(screen_id==SCREEN_DAY);tcp_draw();frame(directory,"day-12h");tcp_activate();tcp_draw();frame(directory,"standard-keyboard");
 t5_app_input_t back={.buttons=T5_APP_BUTTON_BACK};tcp_input(&back);assert(!tcp_editor&&screen_id==SCREEN_DAY);tcp_input(&back);assert(screen_id==SCREEN_WEEK);tcp_input(&back);assert(screen_id==SCREEN_WEEK_LIST);tcp_input(&back);assert(tcp_home);
 init();open_day(20261005);tcp_draw();input_contact(false,0,0,false);input_contact(true,60,120,false);input_contact(true,102,122,false);assert(screen_id==SCREEN_WEEK);input_contact(false,102,122,true);assert(screen_id==SCREEN_WEEK);
 init();open_day(20261005);tcp_draw();input_contact(false,0,0,false);input_contact(true,60,120,false);input_contact(false,120,120,false);assert(screen_id==SCREEN_DAY); /* A lost held sample is not a swipe. */
 init();tcp_time_format=PORTABLE_TIME_FORMAT_24;assert(tcp_mutate(20261005,0,0));assert(tcp_mutate(20261005,3,780));open_week(0);tcp_draw();frame(directory,"week-24h");char value[24];tcp_display_time("12:00 AM",value,sizeof(value));assert(!strcmp(value,"0:00"));tcp_display_time("1:00 PM",value,sizeof(value));assert(!strcmp(value,"13:00"));
 /* The shared NOVA renderer centers its 240px surface. Raw touch coordinates
  * use the matching origin on a larger display; outside contacts never act. */
 init();tcp_draw();view_width=320;view_height=300;input_contact(false,0,0,false);
 input_contact(false,130,135,true);assert(screen_id==SCREEN_WEEK);
 screen_id=SCREEN_WEEK_LIST;tcp_scroll=0;tcp_render_height=48;tcp_render_rows=20;tcp_reset_gesture();
 input_contact(false,0,0,false);input_contact(true,130,180,false);input_contact(true,130,130,false);assert(tcp_scroll==50);
 input_contact(false,130,130,true);assert(screen_id==SCREEN_WEEK_LIST);
 input_contact(true,5,120,false);input_contact(true,120,120,false);assert(screen_id==SCREEN_WEEK_LIST);
 input_contact(false,120,120,true);assert(screen_id==SCREEN_WEEK_LIST);
 tcp_files=NULL;tcp_home=false;tcp_reset_gesture();input_contact(false,0,0,false);
 input_contact(true,140,271,false);input_contact(true,140,269,false);input_contact(false,140,269,true);assert(!tcp_home);
 input_contact(true,140,265,false);input_contact(false,140,265,true);assert(tcp_home);tcp_home=false;tcp_files=&file_api;view_width=view_height=240;
 clock_reads=0;tcp_draw();assert(clock_reads==1);unsigned count=writes;rtc_ok=false;tcp_punch(2);assert(writes==count&&!strcmp(status_text,"Clock unavailable"));tcp_draw();assert(!tcp_clock_valid);frame(directory,"clock-unavailable");rtc_ok=true;tcp_draw();assert(tcp_clock_valid);
}
int main(int argc,char **argv){storage_tests();editor_tests();ui_tests(argc>1?argv[1]:NULL);init();poll_count=launches=grants=0;app_main();assert(launches==1&&!grants&&back_exits);init();provide_files=false;poll_count=launches=grants=0;unsigned count=writes;app_main();assert(launches==1&&!grants&&writes==count);puts("Timecard portable model/storage/editor/navigation/real NOVA pixel tests passed");return 0;}
