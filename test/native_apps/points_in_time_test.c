/* Production source fixtures: both bare client and shared alarm adapter mode. */
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include "../../Apps/points_in_time.c"
static uint8_t stored[POINTS_RECORD_SIZE],last_put[POINTS_RECORD_SIZE],stored_meta[POINTS_RECORD_SIZE],last_put_meta[POINTS_RECORD_SIZE];
static uint32_t stored_size,stored_meta_size,ticks,puts_count,meta_puts_count,gets_count,releases,steps,stops,refreshes,acknowledges;
static int32_t get_error,put_error;static bool persist_error,readback_error,rtc_good,deny,alert,blocked,retained_test,stop_fails;
static unsigned deny_index,acquires,launches;static bool launch_denied;static unsigned preference;
static twatch_rtc_time_v1 rtc_value;
static t5_app_input_t events[128];static unsigned event_count,event_index;static char rendered[4096],status_text[80];
static t5_app_api_v1 fake_app;static risc_runtime_api_v1 runtime_api;static risc_key_value_v1 store_api,prefs_api;
static twatch_rtc_api_v1 rtc_api;static alarm_service_v1 alarm_api;static jmp_buf retained_jump;static bool diagnosed;
static int width_value,height_value;
static int width(void){return width_value;}static int height(void){return height_value;}
static void clear(void){rendered[0]=0;}
static void render_label(int x,int y,int w,const char *s){assert(s && x>=0 && y>=0 && w>0 && x+w<=240 && y+7<=240);assert(strlen(s)*6<=(unsigned)w);if(y==225)snprintf(status_text,sizeof(status_text),"%s",s);assert(strlen(rendered)+strlen(s)+2<sizeof(rendered));strcat(rendered,s);strcat(rendered,"\n");}
static void fill(int x,int y,int w,int h,bool black){(void)black;assert(x>=0&&y>=0&&w>0&&h>0&&x+w<=240&&y+h<=240);}
static void present(bool full){assert(!full);}static uint32_t millis(void){return ticks;}
static void set_back(bool enabled){assert(!enabled);}
static bool poll(t5_app_input_t *out,uint32_t wait){ticks+=wait;assert(event_index<=event_count);if(event_index==event_count)return false;*out=events[event_index++];return true;}
static void yield_ms(uint32_t n){ticks+=n;if(retained_test&&n==50)longjmp(retained_jump,1);}
static bool diagnostic(const char*s){assert(strstr(s,"output-stop-unconfirmed"));diagnosed=true;return true;}
static int32_t get(void *context,const char *key,void *data,uint32_t cap,uint32_t *size){
 (void)context;gets_count++;*size=0;bool meta=!strcmp(key,POINTS_META_KEY);assert(meta||!strcmp(key,POINTS_CONFIG_KEY));
 if(get_error || (!meta&&readback_error&&puts_count))return RISC_KEY_VALUE_IO;
 const uint8_t *src=meta?stored_meta:stored;uint32_t n=meta?stored_meta_size:stored_size;
 if(!n)return RISC_KEY_VALUE_NOT_FOUND;
 if(cap<n){*size=n;return RISC_KEY_VALUE_BUFFER_SMALL;}
 memcpy(data,src,n);*size=n;return RISC_KEY_VALUE_OK;
}
static int32_t put(void *context,const char *key,const void *data,uint32_t size){
 (void)context;assert(size==POINTS_RECORD_SIZE);bool meta=!strcmp(key,POINTS_META_KEY);assert(meta||!strcmp(key,POINTS_CONFIG_KEY));
 uint8_t *dst=meta?stored_meta:stored,*last=meta?last_put_meta:last_put;uint32_t *n=meta?&stored_meta_size:&stored_size;
 if(meta)meta_puts_count++;else puts_count++;memcpy(last,data,size);
 if(!put_error||persist_error){memcpy(dst,data,size);*n=size;}return put_error;
}
static int32_t get_preferences(void *context,const char *key,void *data,uint32_t cap,uint32_t *size){(void)context;assert(!strcmp(key,PORTABLE_TIME_FORMAT_KEY)&&cap>=4);*size=0;if(preference==2)return RISC_KEY_VALUE_NOT_FOUND;uint8_t b[]={0x54,1,(uint8_t)preference,(uint8_t)(preference^0xa5u)};memcpy(data,b,4);*size=4;return 0;}
static bool read_rtc(void *context,twatch_rtc_time_v1 *out){(void)context;*out=rtc_value;return rtc_good;}
static int32_t service_status(void *context,alarm_status_v1 *out){(void)context;assert(out->struct_size==sizeof(*out));*out=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*out),.state=blocked?ALARM_STATE_BLOCKED:ALARM_STATE_READY,.error=blocked?ALARM_STORAGE:0};if(alert){out->state=ALARM_STATE_ALERT;out->occurrence=(alarm_token_v1){3,1,1000,1};strcpy(out->label,"WORK START");}return ALARM_OK;}
static int32_t step(void *context){(void)context;steps++;return ALARM_OK;}
static int32_t refresh(void *context){(void)context;refreshes++;return ALARM_PENDING;}
static int32_t acknowledge(void *context,const alarm_token_v1 *token){(void)context;assert(alert&&token->generation==1);alert=false;acknowledges++;return ALARM_PENDING;}
static int32_t stop(void *context){(void)context;stops++;if(stop_fails)return stops<3?ALARM_PENDING:ALARM_OUTPUT;return ALARM_OK;}
static bool acquire(const char *cap,uint32_t api,uint64_t instance,risc_runtime_capability_v1 *grant){unsigned index=!strcmp(cap,"storage.key-value")?(instance==5?0:3):!strcmp(cap,"rtc.clock")?1:2;acquires++;if(deny&&index==deny_index)return false;if(index==0){assert(!strcmp(cap,"storage.key-value")&&api==1&&instance==5);grant->api=&store_api;}else if(index==1){assert(!strcmp(cap,"rtc.clock")&&api==2&&instance==0);grant->api=&rtc_api;}else if(index==2){assert(!strcmp(cap,"alarm.service")&&api==1&&instance==0);grant->api=&alarm_api;}else{assert(!strcmp(cap,"storage.key-value")&&api==1&&instance==1);grant->api=&prefs_api;}return true;}
static bool request_launch(const char *path){assert(!strcmp(path,"springboard.elf"));launches++;return !launch_denied;}
static bool release(risc_runtime_capability_v1 *grant){assert(grant->api);releases++;return true;}
const t5_app_api_v1 *t5_app_get_api(uint32_t version){assert(version==1);return &fake_app;}
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){assert(version==1);return &runtime_api;}
#ifdef PORTABLE_ALARM_CLIENT
bool portable_app_sleep_retained(void){return retained_test;}
#endif
static void setup(void){
 memset(stored,0,sizeof(stored));memset(last_put,0,sizeof(last_put));memset(stored_meta,0,sizeof(stored_meta));memset(last_put_meta,0,sizeof(last_put_meta));
 stored_size=stored_meta_size=ticks=puts_count=meta_puts_count=gets_count=releases=steps=stops=refreshes=acknowledges=0;
 get_error=put_error=0;persist_error=readback_error=deny=alert=blocked=retained_test=stop_fails=diagnosed=false;rtc_good=true;deny_index=acquires=launches=0;launch_denied=false;preference=2;
 event_count=event_index=0;rendered[0]=status_text[0]=0;width_value=height_value=240;rtc_value=(twatch_rtc_time_v1){2026,10,4,0,12,0,0};
 fake_app=(t5_app_api_v1){.abi_version=1,.struct_size=sizeof(fake_app),.screen_width=width,.screen_height=height,.clear=clear,.fill_rect=fill,.present=present,.poll=poll,.millis=millis,.set_back_exits_app=set_back,.draw_label=render_label};
 runtime_api=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(runtime_api),.acquire=acquire,.release=release,.yield_ms=yield_ms,.diagnostic=diagnostic,.request_launch=request_launch};
 store_api=(risc_key_value_v1){1,sizeof(store_api),NULL,get,put};prefs_api=(risc_key_value_v1){1,sizeof(prefs_api),NULL,get_preferences,NULL};
 rtc_api=(twatch_rtc_api_v1){2,sizeof(rtc_api),NULL,read_rtc,NULL,NULL,NULL};alarm_api=(alarm_service_v1){1,sizeof(alarm_api),NULL,service_status,step,refresh,acknowledge,NULL,stop};
}
static void tap(int x,int y){assert(event_count<128);events[event_count++]=(t5_app_input_t){.tapped=true,.touch_x=x,.touch_y=y};}
static void back(void){events[event_count++]=(t5_app_input_t){.buttons=T5_APP_BUTTON_BACK};}
static void save_first(void){tap(80,42);tap(80,103);tap(190,145);tap(180,195);tap(40,199);tap(170,199);tap(170,192);}
static points_config saved(void){points_config c;assert(points_config_decode(&c,stored,stored_size));return c;}
static void set_direct(void){app=&fake_app;writer=(points_writer){0};page=PAGE_LIST;selected=list_page=0;time_format=0;notice="";service_valid=false;ready=open_dependencies();assert(ready);load_catalog();}
static void writer_tests(void){
 setup();points_writer w={0};assert(points_writer_load(&w,&store_api)==ALARM_OK&&w.loaded&&!w.saved.revision&&!puts_count);
 points_config c=w.saved;c.revision=1;c.created=1000;c.points[0]=(points_item){.kind=POINTS_LUNCH,.enabled=1,.mode=3,.weekdays=62,.hour=12,.minute=30,.duration_minutes=45};assert(points_writer_save(&w,&store_api,&c)==ALARM_OK&&w.saved.revision==1&&!w.uncertain&&puts_count==1);
 assert(points_writer_save(&w,&store_api,&c)==ALARM_EXHAUSTED&&puts_count==1);
 points_writer loaded={0};assert(points_writer_load(&loaded,&store_api)==ALARM_OK&&!memcmp(&loaded.saved,&w.saved,sizeof(w.saved)));
 c.revision=2;c.created=1100;c.points[7]=(points_item){.kind=POINTS_BEDTIME,.enabled=1,.mode=0,.weekdays=127,.hour=22,.minute=15};put_error=RISC_KEY_VALUE_IO;assert(points_writer_save(&w,&store_api,&c)==ALARM_STORAGE&&w.uncertain&&w.saved.revision==1);
 uint8_t pending[64];memcpy(pending,w.pending,64);c.points[7].hour=23;assert(points_writer_save(&w,&store_api,&c)==ALARM_BUSY);assert(points_writer_load(&w,&store_api)==ALARM_BUSY&&puts_count==2);assert(!memcmp(pending,w.pending,64));
 assert(points_writer_retry(&w,&store_api)==ALARM_STORAGE&&puts_count==3&&!memcmp(pending,last_put,64));put_error=0;assert(points_writer_retry(&w,&store_api)==ALARM_OK&&w.saved.revision==2&&w.saved.points[7].hour==22&&!w.uncertain);
 c=w.saved;c.revision++;c.created++;put_error=RISC_KEY_VALUE_IO;persist_error=true;assert(points_writer_save(&w,&store_api,&c)==ALARM_OK&&!w.uncertain);
 c=w.saved;c.revision++;c.created++;put_error=0;readback_error=true;assert(points_writer_save(&w,&store_api,&c)==ALARM_STORAGE&&w.uncertain);memcpy(pending,w.pending,64);readback_error=false;assert(points_writer_retry(&w,&store_api)==ALARM_OK&&!memcmp(last_put,pending,64));
 stored[2]^=1;assert(points_writer_load(&w,&store_api)==ALARM_STORAGE&&!w.loaded);
 setup();get_error=RISC_KEY_VALUE_IO;w=(points_writer){0};assert(points_writer_load(&w,&store_api)==ALARM_STORAGE&&!w.loaded&&!puts_count);
}
int main(void){
 writer_tests();
 setup();app_main();assert(!puts_count&&releases==4&&!writer.saved.revision);assert(strstr(rendered,"Empty"));
#ifdef PORTABLE_ALARM_CLIENT
 assert(!steps&&!stops);setup();retained_test=true;app_main();assert(!releases&&!stops&&!steps);
#else
 assert(!steps&&stops==1);setup();retained_test=stop_fails=true;if(!setjmp(retained_jump))app_main();assert(stops==3&&!releases&&diagnosed);
#endif
 setup();save_first();app_main();assert(puts_count==1&&saved().revision==1&&saved().points[0].weekdays==62&&saved().points[0].enabled&&saved().points[0].kind==POINTS_WORK_START);for(unsigned i=1;i<8;i++)assert(!saved().points[i].kind);
#ifdef PORTABLE_ALARM_CLIENT
 assert(!steps&&!stops);
#else
 assert(steps==event_count&&stops==1);
#endif
 setup();tap(80,42);tap(180,199);tap(180,192);app_main();assert(puts_count==1&&!saved().points[0].enabled&&!saved().points[0].weekdays);
 setup();tap(80,42);tap(40,199);tap(180,199);tap(180,192);app_main();assert(!puts_count&&page==PAGE_DAYS&&strstr(status_text,"Choose at least one day"));
 setup();tap(80,42);tap(80,75);tap(50,150);tap(160,150);tap(80,195);back();app_main();assert(!puts_count&&page==PAGE_LIST);
 setup();put_error=RISC_KEY_VALUE_IO;save_first();tap(80,42);back();tap(100,190);app_main();assert(puts_count==2&&writer.uncertain&&!stored_size&&page==PAGE_SAVE&&!launches);
 setup();readback_error=true;save_first();app_main();assert(puts_count==1&&writer.uncertain&&stored_size==64);
 setup();rtc_good=false;save_first();app_main();assert(!puts_count&&strstr(status_text,"RTC invalid"));
 setup();blocked=true;save_first();app_main();assert(!puts_count&&strstr(status_text,"Service error"));
 setup();get_error=RISC_KEY_VALUE_IO;save_first();app_main();assert(!puts_count&&!writer.loaded&&strstr(status_text,"Storage invalid"));
 for(unsigned i=0;i<4;i++){setup();deny=true;deny_index=i;app_main();assert(!puts_count&&releases==i&&strstr(rendered,"unavailable"));}
 #ifdef POINTS_RETURN_APP
 setup();back();app_main();assert(launches==1&&releases==4);
 setup();tap(80,42);tap(80,74);back();back();app_main();assert(!launches&&page==PAGE_LIST);
 setup();set_direct();launch_denied=true;assert(!on_back()&&launches==1&&!releases&&ready&&strstr(notice,"Return unavailable"));launch_denied=false;assert(on_back()&&launches==2&&!releases);close_dependencies();
#endif
 setup();width_value=239;app_main();assert(!acquires);setup();height_value=241;app_main();assert(!acquires);
 setup();set_direct();edit_slot(7);draft=(points_item){.kind=POINTS_LUNCH,.enabled=1,.mode=3,.weekdays=127,.hour=23,.minute=59,.duration_minutes=719};page=PAGE_DURATION;on_tap(170,140);assert(draft.duration_minutes==720);on_tap(170,170);assert(draft.duration_minutes==720);draft.duration_minutes=1;on_tap(50,140);assert(!draft.duration_minutes);on_tap(50,170);assert(!draft.duration_minutes);on_tap(170,110);assert(draft.duration_minutes==60);on_tap(50,110);assert(!draft.duration_minutes);on_tap(180,223);assert(page==PAGE_EDIT);
 page=PAGE_TYPE;on_tap(60,78);assert(draft.kind==POINTS_WORK_END&&!draft.duration_minutes);page=PAGE_TIME;draft.hour=draft.minute=0;on_tap(50,151);on_tap(160,151);assert(draft.hour==23&&draft.minute==59);on_tap(50,75);on_tap(160,75);assert(!draft.hour&&!draft.minute);
 page=PAGE_MODE;for(unsigned i=0;i<4;i++){page=PAGE_MODE;on_tap(50,50+(int)i*37);assert(draft.mode==i);}page=PAGE_DAYS;draft.weekdays=0;for(unsigned i=0;i<7;i++)on_tap(15+(int)(i%3)*76,48+(int)(i/3)*45);assert(draft.weekdays==127);on_tap(45,194);assert(!draft.weekdays);on_tap(170,144);assert(draft.weekdays==62);
 for(unsigned p=PAGE_LIST;p<=PAGE_SAVE;p++){page=p;draw();}assert(strstr(rendered,"Missing or repeated local")&&strstr(rendered,"times skip that date"));close_dependencies();
 setup();set_direct();char formatted[32];points_item item={.hour=0,.minute=5};format_time(&item,formatted,sizeof(formatted));assert(!strcmp(formatted,"12:05 AM"));item.hour=12;format_time(&item,formatted,sizeof(formatted));assert(!strcmp(formatted,"12:05 PM"));time_format=1;format_time(&item,formatted,sizeof(formatted));assert(!strcmp(formatted,"12:05"));close_dependencies();
 setup();preference=1;app_main();assert(time_format==1&&!puts_count);
 setup();set_direct();writer.saved.revision=UINT32_MAX;edit_slot(0);save_action();assert(!puts_count&&!writer.uncertain&&strstr(notice,"Revision limit"));close_dependencies();
 setup();set_direct();rtc_value=(twatch_rtc_time_v1){2099,12,31,4,23,59,50};edit_slot(0);save_action();assert(!puts_count&&!writer.uncertain&&strstr(notice,"RTC range"));close_dependencies();
 setup();set_direct();get_error=RISC_KEY_VALUE_IO;load_catalog();assert(!writer.loaded);get_error=0;retry_action();assert(writer.loaded&&!puts_count);close_dependencies();
 setup();app=&fake_app;writer=(points_writer){0};deny=true;deny_index=2;ready=open_dependencies();assert(!ready&&acquired==2);deny=false;retry_action();assert(ready&&writer.loaded&&releases==2);close_dependencies();assert(releases==6);
 setup();set_direct();edit_slot(0);draft.enabled=1;draft.weekdays=127;put_error=RISC_KEY_VALUE_IO;save_action();assert(writer.uncertain);uint8_t exact[64];memcpy(exact,writer.pending,64);rtc_value.hour=20;on_tap(50,75);assert(!memcmp(exact,writer.pending,64));put_error=0;retry_action();assert(!writer.uncertain&&!memcmp(exact,last_put,64)&&writer.saved.created%86400==12*3600);close_dependencies();
 setup();for(unsigned i=0;i<100;i++)events[event_count++]=(t5_app_input_t){0};app_main();assert(!puts_count&&!meta_puts_count&&gets_count==2);

 setup();set_direct();for(unsigned i=0;i<8;i++){edit_slot(i);draft=(points_item){.kind=(uint8_t)(i%5+1),.enabled=1,.mode=(uint8_t)(i%4),.weekdays=127,.hour=(uint8_t)(6+i),.minute=15};save_action();assert(writer.saved.revision==i+1);}assert(puts_count==8);for(unsigned i=0;i<8;i++)assert(writer.saved.points[i].hour==6+i);uint32_t rev=writer.saved.revision;edit_slot(0);draft.enabled=0;rtc_value.minute=1;save_action();assert(writer.saved.revision==rev+1&&!writer.saved.points[0].enabled&&writer.saved.points[7].enabled);close_dependencies();
 setup();set_direct();draft=(points_item){.kind=POINTS_CUSTOM_1,.enabled=1,.mode=3,.weekdays=127,.hour=14,.duration_minutes=30};page=PAGE_EDIT;nova_edit_scroll=0;
 nova_tap(100,175);assert(draft.notify_end);nova_edit_scroll=1;nova_tap(100,175);assert(draft.warn3);
 custom_kind=POINTS_CUSTOM_1;custom_draft=writer.meta;memcpy(custom_draft.custom[0].name,"MEDICINE",9);custom_draft.custom[0].color=2;page=PAGE_CUSTOM;custom_pos=0;
 nova_tap(180,205);assert(page==PAGE_EDIT&&draft.kind==POINTS_CUSTOM_1&&writer.meta.revision==1&&writer.meta.custom[0].color==2&&!strcmp(writer.meta.custom[0].name,"MEDICINE")&&meta_puts_count==1);
 points_meta persisted_meta;assert(points_meta_decode(&persisted_meta,stored_meta,stored_meta_size)&&persisted_meta.custom[0].color==2&&!strcmp(persisted_meta.custom[0].name,"MEDICINE"));close_dependencies();
 setup();tap(170,192);tap(80,150);app_main();assert(selected==7&&!puts_count);
#ifndef PORTABLE_ALARM_CLIENT
 setup();alert=true;back();tap(80,190);app_main();assert(acknowledges==1&&!alert&&!puts_count);
 setup();set_direct();edit_slot(0);put_error=RISC_KEY_VALUE_IO;save_action();assert(writer.uncertain);alert=true;refresh_status();draw();assert(strstr(rendered,"Dismiss to continue"));on_tap(80,190);assert(acknowledges==1&&writer.uncertain&&puts_count==1);close_dependencies();
#endif
 puts("Points app writer, custom metadata/colors, independent end/warning toggles, 8-slot persistence, retry, RTC, modes and lifecycle fixtures passed");return 0;
}
