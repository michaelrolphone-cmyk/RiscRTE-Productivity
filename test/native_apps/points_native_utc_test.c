/* Reuse actual legacy production controller fixture, replacing only providers. */
#define main legacy_points_main
#include "points_in_time_test.c"
#undef main
static unsigned native_reads,native_live,native_peak,retains,provider_calls;
static int native_result,zone_result,storage_result,put_result,service_result;
static bool bad_release,partial_acquire;
static int64_t native_epoch;
static alarm_service_descriptor_v2 native_alarm_descriptor;
static const char *zone_id;
static bool native_unset;
void portable_adapter_retain(void){retains++;}
static void native_called(void){assert(!points_retained);provider_calls++;}
static int32_t native_read(void *context,risc_realtime_snapshot_v1 *s){
 (void)context;native_called();native_reads++;
 if(native_result)return native_result;
 *s=(risc_realtime_snapshot_v1){.struct_size=sizeof(*s),.validity=native_unset?RISC_REALTIME_UNSET:RISC_REALTIME_VALID,
  .epoch_seconds=native_unset?0:native_epoch,.monotonic_before_us=1,.monotonic_after_us=2};return RISC_REALTIME_OK;
}
static const risc_realtime_api_v1 native_api={1,sizeof(native_api),(void*)1,native_read};
static int32_t native_get(void *context,const char *key,void *b,uint32_t cap,uint32_t *size){
 native_called();if(storage_result){*size=0;return storage_result;}return get(context,key,b,cap,size);
}
static int32_t native_put(void *context,const char *key,const void *b,uint32_t size){
 native_called();if(put_result)return put_result;return put(context,key,b,size);
}
static int32_t native_preferences(void *context,const char *key,void *b,uint32_t cap,uint32_t *size){
 native_called();if(!strcmp(key,PORTABLE_TIME_FORMAT_KEY))return get_preferences(context,key,b,cap,size);
 assert(!strcmp(key,PORTABLE_TIMEZONE_KEY));*size=0;if(zone_result)return zone_result;
 if(!zone_id)return RISC_KEY_VALUE_NOT_FOUND;
 uint8_t bytes[PORTABLE_TIMEZONE_RECORD_BYTES]={0};
 /* Encode using the exact public helper via a tiny captured write. */
 extern uint8_t zone_bytes[PORTABLE_TIMEZONE_RECORD_BYTES];
 memcpy(bytes,zone_bytes,sizeof(bytes));assert(cap>=sizeof(bytes));memcpy(b,bytes,sizeof(bytes));*size=sizeof(bytes);return 0;
}
uint8_t zone_bytes[PORTABLE_TIMEZONE_RECORD_BYTES];
static int32_t zone_put(void *context,const char *key,const void *b,uint32_t size){(void)context;assert(!strcmp(key,PORTABLE_TIMEZONE_KEY)&&size==sizeof(zone_bytes));memcpy(zone_bytes,b,size);zone_id="stored";return 0;}
static int32_t native_status(void *c,alarm_status_v1 *s){native_called();if(service_result)return service_result;return service_status(c,s);}
static int32_t native_refresh(void *c){native_called();if(service_result)return service_result;return refresh(c);}
static bool native_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *g){
 native_called();assert(strcmp(name,"rtc.clock")&&strcmp(name,"runtime.realtime-control"));
 if(!strcmp(name,RISC_REALTIME_CAPABILITY)){
  assert(version==1&&!instance&&!native_live);native_live++;if(native_live>native_peak)native_peak=native_live;g->api=&native_api;
 }else if(!strcmp(name,"storage.key-value")){assert(version==1&&(instance==1||instance==5));g->api=instance==1?&prefs_api:&store_api;}
 else{assert(!strcmp(name,ALARM_SERVICE_CAPABILITY)&&version==ALARM_SERVICE_API_V2&&!instance);g->api=&native_alarm_descriptor.base;}
 g->slot=1;g->generation=1;acquires++;if(partial_acquire)return false;return true;
}
static bool native_release(risc_runtime_capability_v1 *g){
 native_called();assert(g->api);releases++;if(bad_release)return false;if(g->api==&native_api){assert(native_live==1);native_live--;}
 *g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;
}
static int32_t native_prepare(void *c,alarm_sleep_v1 *s){(void)c;(void)s;native_called();return ALARM_OK;}
static void native_setup(void){
 setup();points_retained=false;points_clock=(portable_realtime_client){0};points_zone[0]=0;points_zone_index=0;
 points_storage_source=points_preferences_source=NULL;points_preview=(points_projection){0};
 native_reads=native_live=native_peak=retains=provider_calls=0;
 native_result=zone_result=storage_result=put_result=service_result=0;native_unset=bad_release=partial_acquire=false;
 zone_id=NULL;native_epoch=1791115200; /* 2026-10-04 12:00 UTC. */
 runtime_api.acquire=native_acquire;runtime_api.release=native_release;
 store_api.get=native_get;store_api.put=native_put;prefs_api.get=native_preferences;
 alarm_api.status=native_status;alarm_api.refresh=native_refresh;alarm_api.prepare_sleep=native_prepare;
 native_alarm_descriptor=(alarm_service_descriptor_v2){.base=alarm_api,.tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=ALARM_SERVICE_DESCRIPTOR_VERSION,.output_modes=0};
 native_alarm_descriptor.base.api_version=ALARM_SERVICE_API_V2;native_alarm_descriptor.base.struct_size=sizeof(native_alarm_descriptor);
}
static void zone(const char *id){
 risc_key_value_v1 kv={1,sizeof(kv),NULL,native_preferences,zone_put};zone_id=NULL;
 assert(portable_timezone_preference_save(&kv,id,strlen(id)+1)==PORTABLE_TIMEZONE_SAVED);zone_id=id;
}
static uint32_t utc(int y,int m,int d,int h,int minute){
 portable_timezone_civil c={y,(uint8_t)m,(uint8_t)d,(uint8_t)h,(uint8_t)minute,0,0};int64_t epoch;uint32_t result;
 assert(portable_timezone_civil_to_epoch(&c,&epoch)==PORTABLE_TIMEZONE_OK&&points_utc_from_unix(epoch,&result));return result;
}
static void schedule_tests(void){
 portable_timezone_rule rule;assert(portable_timezone_count()==419);
 assert(portable_timezone_resolve("America/Denver",15,&rule)==PORTABLE_TIMEZONE_OK);
 points_config c={.revision=1,.created=utc(2026,1,1,0,0)};c.points[0]=(points_item){.kind=POINTS_LUNCH,.enabled=1,.mode=3,.weekdays=127,.hour=2,.minute=30,.duration_minutes=60,.notify_end=1,.warn3=1};
 uint32_t day;assert(points_utc_local_day(&rule,utc(2026,3,8,12,0),&day));points_event e;uint32_t flags=0;
 assert(!points_utc_event_for_day(&rule,&c,0,day,POINTS_EDGE_START,&e,&flags)&&(flags&POINTS_FLAG_GAP));
 c.points[0].hour=1;assert(points_utc_local_day(&rule,utc(2026,11,1,12,0),&day));flags=0;
 assert(!points_utc_event_for_day(&rule,&c,0,day,POINTS_EDGE_START,&e,&flags)&&(flags&POINTS_FLAG_FOLD));
 c.points[0].hour=23;c.points[0].minute=30;c.points[0].duration_minutes=120;
 assert(points_utc_local_day(&rule,utc(2026,3,8,5,0),&day));
 points_event start,end,warning;assert(points_utc_event_for_day(&rule,&c,0,day,0,&start,NULL));assert(points_utc_event_for_day(&rule,&c,0,day,1,&end,NULL));assert(points_utc_event_for_day(&rule,&c,0,day,2,&warning,NULL));
 assert(end.deadline-start.deadline==7200&&warning.deadline==end.deadline-180&&start.parent_day==end.parent_day);
 points_projection projection;assert(points_utc_project(&rule,&c,start.deadline+1,&projection)&&projection.has_previous);
 points_ledger ledger={.revision=1,.generation=1,.timezone_index=(uint16_t)portable_timezone_find("America/Denver",15)};
 assert(points_utc_latest_for_edge(&rule,&c,&ledger,start.deadline,0,0,&e)&&e.deadline==start.deadline);
 assert(points_utc_due(&rule,&c,&ledger,start.deadline,&e));
 uint8_t bytes[64];points_ledger_encode(&ledger,bytes);assert(!memcmp(bytes,"PTU1",4));points_ledger decoded;assert(points_ledger_decode(&decoded,bytes,64)&&decoded.timezone_index==ledger.timezone_index);
 int64_t epoch;uint32_t seconds;assert(points_utc_from_unix(INT32_MAX,&seconds)&&seconds==1200798847u&&points_utc_to_unix(seconds,&epoch)&&epoch==INT32_MAX);
 assert(!points_utc_from_unix(INT64_C(2147483648),&seconds)&&!points_utc_from_unix(946684799,&seconds)&&!points_utc_to_unix(1200798848,&epoch));
 assert(!strcmp(POINTS_CONFIG_KEY,"points_utc_cfg")&&!strcmp(POINTS_META_KEY,"points_utc_meta")&&!strcmp(POINTS_OCCURRENCE_KEY,"points_utc_occ"));
}
static void controller_tests(void){
 native_setup();set_direct();assert(clock_valid&&native_peak==1&&!native_live&&!puts_count&&!meta_puts_count);
 zone("America/Denver");uint32_t now;assert(read_clock(&now));assert(!strcmp(points_time_zone(),"America/Denver"));
 nova_new_point();assert(draft.hour==6&&draft.minute==0); /* Local, not native UTC. */
 on_back();edit_slot(0);draft=(points_item){.kind=POINTS_BREAK,.enabled=1,.mode=3,.weekdays=127,.hour=8,.minute=30,.duration_minutes=15,.notify_end=1,.warn3=1};
 save_action();assert(writer.saved.created==now&&writer.saved.revision==2&&puts_count==1&&writer.saved.points[0].warn3);
 points_config prior=writer.saved;edit_slot(0);draft.hour=19;on_back();assert(!memcmp(&prior,&writer.saved,sizeof(prior))&&puts_count==1);
 alert=true;refresh_status();alarm_token_v1 original_occurrence=service_state.occurrence;
 zone("Asia/Tokyo");assert(read_clock(&now));refresh_status();assert(!memcmp(&original_occurrence,&service_state.occurrence,sizeof(original_occurrence)));
 assert(writer.saved.created==prior.created&&puts_count==1);alert=false;refresh_status(); /* Stored UTC identity survives zone changes. */
 for(unsigned i=0;i<10;i++){edit_slot(0);draft.minute++;save_action();assert(!writer.uncertain);}
 assert(native_peak==1&&!native_live);close_dependencies();assert(!retains);
 native_setup();native_unset=true;set_direct();assert(!clock_valid&&strstr(status_message(),"unset"));edit_slot(0);save_action();assert(!puts_count);close_dependencies();
 native_setup();native_epoch=INT32_MAX;set_direct();assert(clock_valid);edit_slot(0);save_action();assert(!puts_count);close_dependencies();
 native_setup();zone_result=RISC_KEY_VALUE_IO;set_direct();assert(!clock_valid&&!retains);zone_result=0;retry_action();assert(clock_valid);close_dependencies();
 native_setup();set_direct();edit_slot(0);draft.hour=9;readback_error=true;save_action();assert(writer.uncertain&&!retains);uint8_t pending[64];memcpy(pending,writer.pending,64);zone("Pacific/Auckland");native_epoch+=3600;readback_error=false;retry_action();assert(!writer.uncertain&&!memcmp(pending,stored,64));close_dependencies();
 native_setup();stored_size=0;set_direct();edit_slot(0);put_error=RISC_KEY_VALUE_IO;save_action();assert(writer.uncertain&&writer.meta_uncertain&&!retains&&!puts_count);memcpy(pending,writer.pending,64);put_error=0;retry_action();assert(!writer.uncertain&&!writer.meta_uncertain&&!memcmp(pending,stored,64));close_dependencies();
 native_setup();tap(80,42);events[event_count++]=(t5_app_input_t){.exit_requested=true};app_main();assert(!puts_count&&!retains&&!native_live);
}
static springboard_contact native_contact;
static unsigned native_frames;
static void paper_begin(void){native_called();rendered[0]=0;}
static void paper_text(int x,int y,int w,const char *text,unsigned scale,bool heading,bool black){
 (void)scale;(void)heading;(void)black;native_called();assert(x>=0&&x<480&&y>=0&&w>0&&w<=480&&y<800);
 assert(strlen(rendered)+strlen(text)+2<sizeof(rendered));strcat(rendered,text);strcat(rendered,"\n");
}
static int paper_measure(const char *text,bool heading){(void)heading;native_called();return (int)strlen(text)*8;}
static void paper_circle(int x,int y,int radius,bool black){(void)x;(void)y;(void)radius;(void)black;native_called();}
static void paper_contact(springboard_contact *out){native_called();*out=native_contact;}
static void paper_fill(int x,int y,int w,int h,bool black){(void)black;native_called();assert(x>=0&&y>=0&&w>0&&h>0&&x+w<=480&&y+h<=800);}
static void paper_present(bool full){(void)full;native_called();native_frames++;}
static const paper_presentation native_paper={sizeof(native_paper),paper_begin,paper_text,paper_measure,paper_circle,paper_contact,NULL,NULL};
static void paper_controller_tests(void){
 native_setup();set_direct();width_value=480;height_value=800;fake_app.fill_rect=paper_fill;fake_app.present=paper_present;paper=&native_paper;
 pe_first=pe_edit_first=pe_choice_first=pe_focus=pe_key_page=0;pe_down=pe_exit=pe_focus_visible=false;pe_clean=true;native_contact=(springboard_contact){0};
 zone("America/Denver");uint32_t seconds;assert(read_clock(&seconds));nova_new_point();page=PAGE_TIME;draw();assert(strstr(rendered,"America/Denver"));
 uint8_t hour=draft.hour;t5_app_input_t input={0};
 native_contact=(springboard_contact){.valid=true,.down=true,.began=true,.x=350,.y=280};pe_input(&input);
 for(unsigned i=0;i<8;i++){native_contact.began=false;pe_input(&input);}assert(draft.hour==hour);
 native_contact=(springboard_contact){.valid=true,.released=true,.tap_eligible=true,.x=350,.y=280};pe_input(&input);assert(draft.hour==(hour+1)%24);
 native_contact=(springboard_contact){.valid=true,.down=true,.began=true,.x=350,.y=280};pe_input(&input);
 native_contact=(springboard_contact){.valid=true,.cancelled=true,.released=true,.x=350,.y=280};pe_input(&input);assert(draft.hour==(hour+1)%24);
 native_contact=(springboard_contact){0};
 pe_tap(350,730);assert(page==PAGE_EDIT);pe_tap(100,730);assert(page==PAGE_LIST&&!puts_count);
 nova_new_point();draft.weekdays=127;draft.duration_minutes=720;draft.notify_end=draft.warn3=1;
 pe_tap(350,730);assert(page==PAGE_LIST&&writer.saved.points[0].duration_minutes==720&&writer.saved.points[0].warn3);
 for(unsigned i=0;i<10;i++){edit_slot(0);draft.hour++;pe_tap(100,730);assert(page==PAGE_LIST&&puts_count==1);}
 edit_slot(0);pe_edit_action(3);pe_edit_action(4);assert(!draft.notify_end&&!draft.warn3);pe_edit_action(1);assert(page==PAGE_TIME);pe_back();assert(page==PAGE_EDIT);pe_back();assert(page==PAGE_LIST&&puts_count==1);
 nova_new_point();page=PAGE_CUSTOM;custom_kind=POINTS_CUSTOM_1;custom_draft=writer.meta;pe_tap(100,170);assert(page==PAGE_CUSTOM_KEYBOARD);
 for(unsigned page_index=0;page_index<(95+pe_key_count()-1)/pe_key_count();page_index++)for(unsigned key=0;key<pe_key_count()&&32+page_index*pe_key_count()+key<=126;key++){pe_key_text[0]=0;pe_key_page=page_index;pe_key(key);assert((unsigned char)pe_key_text[0]==32+page_index*pe_key_count()+key);}
 strcpy(pe_key_text,"NATIVE LABEL");pe_tap(350,730);assert(page==PAGE_CUSTOM);pe_tap(350,730);assert(page==PAGE_EDIT&&!strcmp(writer.meta.custom[0].name,"NATIVE LABEL")&&meta_puts_count==1);
 pe_edit_first=6;pe_edit_action(8);assert(nova_delete_confirm);pe_edit_action(1);assert(!nova_delete_confirm);page=PAGE_EDIT;pe_edit_action(8);pe_edit_action(8);assert(page==PAGE_LIST&&puts_count==2);
 close_dependencies();paper=NULL;
}
static void retained_tests(void){
 for(unsigned scenario=0;scenario<7;scenario++){
  native_setup();set_direct();unsigned before;
  if(scenario==0){native_result=RISC_REALTIME_CONTEXT;uint32_t n;(void)read_clock(&n);}
  if(scenario==1){bad_release=true;uint32_t n;(void)read_clock(&n);}
  if(scenario==2){storage_result=RISC_KEY_VALUE_CONTEXT;load_catalog();}
  if(scenario==3){put_result=RISC_KEY_VALUE_CONTEXT;edit_slot(0);save_action();}
  if(scenario==4){service_result=ALARM_RETAINED;refresh_status();}
  if(scenario==5){zone_result=RISC_KEY_VALUE_CONTEXT;uint32_t n;(void)read_clock(&n);}
  if(scenario==6){service_result=ALARM_RETAINED;edit_slot(0);save_action();}
  assert(points_retained&&retains==1);before=provider_calls;
  load_catalog();retry_action();save_action();refresh_status();draw();close_dependencies();assert(!on_back());app_main();assert(provider_calls==before&&retains==1);
 }
 native_setup();native_alarm_descriptor.base.api_version=1;assert(!open_dependencies()&&!points_retained);close_dependencies();
 native_setup();native_alarm_descriptor.tag^=1;assert(!open_dependencies()&&!points_retained);close_dependencies();
 native_setup();native_alarm_descriptor.descriptor_version=2;assert(!open_dependencies()&&!points_retained);close_dependencies();
 native_setup();partial_acquire=true;assert(!open_dependencies()&&points_retained&&retains==1);unsigned before=provider_calls;close_dependencies();assert(before==provider_calls);
}
int main(void){schedule_tests();controller_tests();paper_controller_tests();retained_tests();puts("Native UTC Points: DST, elapsed duration, UTC keys/PTU1, paper edit/held/cancel/keyboard controls, exact save retry, timezone changes, native UNSET/2038 and terminal custody passed");return 0;}
