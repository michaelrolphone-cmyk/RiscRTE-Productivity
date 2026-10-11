/* The selected production controller is linked to the real System renderer.
 * Only capabilities are fakes; artifacts are the renderer's actual pixels. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef PC_FAKE_BACKGROUND
#define PORTABLE_CONTEXTS_CLIENT
#define PORTABLE_BLE_BROADCAST
#endif
static void *catalog_alloc(size_t);
static void catalog_free(void *);
#define POINTS_CATALOG_ALLOC catalog_alloc
#define POINTS_CATALOG_FREE catalog_free
#define t5_app_get_api fixture_app_get
#include "../../Apps/points_catalog_app.c"
#undef t5_app_get_api
#include "PortableApps.h"
#include "RiscDisplayOutputV1.h"
#include "RiscTouchV1.h"
#include "RiscInputNavigationV1.h"
#include "RiscBatteryGaugeV1.h"
const t5_app_api_v1 *t5_app_get_api(uint32_t);
int app_module_init(void);void app_module_fini(void);
const t5_app_manifest_t portable_catalog[]={{.compatible=false}};
const unsigned portable_catalog_count=0;
static t5_app_api_v1 fixture_app;
static const t5_app_api_v1 *render_app;
static uint8_t pixels[240*240*2],*document;
static uint32_t document_size,storage_capacity=65536;
static uint64_t document_token;
static unsigned last_damage_area,last_present_intent;
static unsigned calls,live_grants,live_subs,live_frames,writes,legacy_gets,retains,frames,polls,launches,clock_reads;
static uint32_t ticks,frame_release_after;
static bool display_inflight;
static uint32_t fake_alarm_state=ALARM_STATE_READY;
static bool retain_projection;
static bool terminal,hold_frame,read_failure,unknown_save,storage_retained,release_failure,acquire_lost,runtime_lost,retain_on_read;
static risc_touch_snapshot_v1 contact;
static unsigned script_mode,script_at,home_test_page;
static unsigned entry_text_result;
static bool entry_text_alarm,entry_text_home;
static bool nav_home,launched_home;
static t5_app_input_t script[12];
static unsigned script_count;
static void io(void){assert(!terminal&&!runtime_lost);calls++;}
static void *catalog_alloc(size_t n){assert(!terminal&&!runtime_lost);return malloc(n);}
static void catalog_free(void *p){assert(!terminal&&!runtime_lost);free(p);}
#ifdef PC_FAKE_BACKGROUND
static unsigned background_phase,context_stops,broadcast_stops;
static bool refuse_context,refuse_broadcast;
bool portable_contexts_stop(void){io();context_stops++;background_phase=1;return !refuse_context;}
bool portable_broadcast_stop(void){io();assert(background_phase==1);broadcast_stops++;background_phase=2;return !refuse_broadcast;}
static void storage_io(void){io();assert(background_phase==2);background_phase=0;}
#else
#define storage_io io
#endif
static bool health(risc_runtime_health_v1 *h){io();h->uptime_ms=ticks;return true;}
static void yield_ms(uint32_t n){io();ticks+=n;}
static bool diagnostic(const char *s){io();(void)s;return true;}
static bool launch(const char *s){io();assert(!strcmp(s,"springboard.elf")||!strcmp(s,"default.elf"));launched_home=!strcmp(s,"default.elf");launches++;return true;}
static bool retain_invocation(void){assert(!terminal);retains++;terminal=true;return true;}
static int32_t file_stat(void *c,const char *name,uint32_t *size,uint64_t *rev) {
    (void)c;storage_io();assert(!strcmp(name,POINTS_CATALOG_FILE));*size=0;*rev=0;
    if(storage_retained)return RISC_APP_DATA_RETAINED;
    if(read_failure)return RISC_APP_DATA_IO;
    if(!document)return RISC_APP_DATA_NOT_FOUND;
    *size=document_size;*rev=document_token;return 0;
}
static int32_t file_read(void *c,const char *name,uint64_t expected,void *out,uint32_t cap,uint32_t *size,uint64_t *rev) {
    (void)c;storage_io();assert(!strcmp(name,POINTS_CATALOG_FILE)&&expected==document_token&&cap>=document_size);
    if(retain_on_read)return RISC_APP_DATA_CONTEXT;
    memcpy(out,document,document_size);*size=document_size;*rev=document_token;return 0;
}
static int32_t file_replace(void *c,const char *name,uint64_t expected,const void *bytes,uint32_t n) {
    (void)c;storage_io();assert(!strcmp(name,POINTS_CATALOG_FILE));writes++;
    if(storage_retained)return RISC_APP_DATA_RETAINED;
    if(expected!=document_token)return RISC_APP_DATA_STALE;
    if(n>storage_capacity)return RISC_APP_DATA_NO_SPACE;
    uint8_t *next=malloc(n);assert(next);memcpy(next,bytes,n);free(document);document=next;document_size=n;document_token++;
    if(unknown_save){read_failure=true;return RISC_APP_DATA_COMMIT_UNKNOWN;}return 0;
}
static const risc_app_data_v1 file_api={1,sizeof(file_api),NULL,file_stat,file_read,file_replace};
static int32_t kv_get(void *c,const char *key,void *out,uint32_t cap,uint32_t *size) {
#ifdef PC_FAKE_BACKGROUND
    /* Shared Quick Actions may read its own prefs during adapter entry. This
     * fixture's stop hooks instrument the editor's wrappers, not Quick's. */
    if(c==(void*)1&&background_phase==0)io();else storage_io();
#else
    storage_io();
#endif
    *size=0;
    if(c==(void*)5) {
        legacy_gets++;assert(cap==64);
        if(!strcmp(key,POINTS_CONFIG_KEY)){points_config old=points_default_config();points_config_encode(&old,out);}
        else {assert(!strcmp(key,POINTS_META_KEY));points_meta m=points_default_meta();points_meta_encode(&m,out);}
        *size=64;return 0;
    }
    if(!strcmp(key,PORTABLE_TIME_FORMAT_KEY)){assert(cap>=4);const uint8_t b[]={0x54,1,1,0xa4};memcpy(out,b,4);*size=4;return 0;}
    return RISC_KEY_VALUE_NOT_FOUND;
}
static int32_t forbidden_put(void *c,const char *key,const void *data,uint32_t n){(void)c;(void)key;(void)data;(void)n;assert(!"Legacy/preferences writes are forbidden");return -1;}
static const risc_key_value_v1 legacy_api={1,sizeof(legacy_api),(void*)5,kv_get,NULL};
static const risc_key_value_v1 preference_api={1,sizeof(preference_api),(void*)1,kv_get,forbidden_put};
static int32_t status(void *c,alarm_status_v1 *s){(void)c;io();*s=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*s),.state=fake_alarm_state};return 0;}
static int32_t step(void *c){(void)c;io();return 0;}
static int32_t ack(void *c,const alarm_token_v1 *t){(void)t;return step(c);}
static int32_t prepare(void *c,alarm_sleep_v1 *s){(void)s;return step(c);}
static uint32_t projected_id;
static bool old_service;
static int32_t project(void *c,points_catalog_projection *out){(void)c;io();if(retain_projection)return ALARM_RETAINED;assert(out->struct_size==sizeof(*out));*out=(points_catalog_projection){.struct_size=sizeof(*out),.count=projected_id?1:0};out->next[0].event_id=projected_id;return 0;}
#ifdef ALARM_NATIVE_UTC
static const alarm_service_descriptor_v2 old_alarm_api={.base={2,sizeof(old_alarm_api),NULL,status,step,step,ack,prepare,step},.tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=1,.output_modes=0};
static const points_service_v2 alarm_api={.base={.base={2,sizeof(alarm_api),NULL,status,step,step,ack,prepare,step},.tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=1,.output_modes=0},.points={POINTS_SERVICE_PROJECTION_TAG,1,project}};
static int32_t realtime_read(void *c,risc_realtime_snapshot_v1 *s) {
    (void)c;io();clock_reads++;*s=(risc_realtime_snapshot_v1){.struct_size=sizeof(*s),.validity=RISC_REALTIME_VALID,.epoch_seconds=1791543600,.monotonic_before_us=1,.monotonic_after_us=2};return 0;
}
static const risc_realtime_api_v1 time_api={1,sizeof(time_api),(void*)1,realtime_read};
#else
static const alarm_service_v1 old_alarm_api={1,sizeof(old_alarm_api),NULL,status,step,step,ack,prepare,step};
static const points_service_v1 alarm_api={.base={.base={1,sizeof(alarm_api),NULL,status,step,step,ack,prepare,step}},.points={POINTS_SERVICE_PROJECTION_TAG,1,project}};
static bool rtc_read(void *c,twatch_rtc_time_v1 *t){(void)c;io();clock_reads++;*t=(twatch_rtc_time_v1){2026,10,9,5,9,3,5};return true;}
static const twatch_rtc_api_v1 time_api={.api_version=2,.struct_size=sizeof(time_api),.read=rtc_read};
#endif
static bool display_info(void *c,risc_display_info_v1 *s) {
    (void)c;io();
#ifdef ALARM_NATIVE_UTC
    *s=(risc_display_info_v1){.width=800,.height=480,.flags=RISC_DISPLAY_INFO_RETAINS_IMAGE|RISC_DISPLAY_INFO_PARTIAL_DAMAGE|RISC_DISPLAY_INFO_CLEAN_PRESENT|RISC_DISPLAY_INFO_ASYNC_PRESENT,.supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_MONO1),.damage_x_alignment=8,.damage_width_alignment=8};
#else
    *s=(risc_display_info_v1){.width=240,.height=240,.nominal_refresh_millihz=60000,.flags=RISC_DISPLAY_INFO_PARTIAL_DAMAGE,.supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565)};
#endif
    return true;
}
static bool display_acquire(void *c,uint32_t format,risc_display_surface_v1 *s) {
    (void)c;io();assert(!live_frames);live_frames++;
#ifdef ALARM_NATIVE_UTC
    assert(format==RISC_DISPLAY_FORMAT_MONO1);*s=(risc_display_surface_v1){.frame=1,.pixels=pixels,.width=800,.height=480,.stride_bytes=100,.size_bytes=48000,.pixel_format=format};
#else
    assert(format==RISC_DISPLAY_FORMAT_RGB565);*s=(risc_display_surface_v1){.frame=1,.pixels=pixels,.width=240,.height=240,.stride_bytes=480,.size_bytes=sizeof(pixels),.pixel_format=format};
#endif
    return true;
}
static void display_release(void *c,risc_display_frame_v1 f){(void)c;io();assert(f==1&&live_frames);live_frames--;}
static bool display_submit(void *c,risc_display_frame_v1 f,const risc_display_rect_v1 *r,size_t n,const risc_display_present_options_v1 *o,risc_display_present_token_v1 *t) {
    (void)c;(void)o;
#ifdef TEST_EXPECT_FAST_PAPER
    assert(o->intent==RISC_DISPLAY_PRESENT_LOW_LATENCY);
#endif
    io();last_present_intent=o->intent;last_damage_area=0;for(size_t i=0;i<n;i++)last_damage_area+=(unsigned)r[i].width*r[i].height;assert(f==1&&live_frames);live_frames--;display_inflight=true;*t=++frames;return true;
}
static bool display_status(void *c,risc_display_present_token_v1 t,risc_display_present_status_v1 *s){(void)c;io();assert(t);if(frame_release_after&&ticks>=frame_release_after){hold_frame=false;frame_release_after=0;}s->state=hold_frame?RISC_DISPLAY_PRESENT_ACTIVE:RISC_DISPLAY_PRESENT_COMPLETE;if(!hold_frame)display_inflight=false;return true;}
static const risc_display_output_api_v1 display_api={.api_version=1,.struct_size=sizeof(display_api),.get_info=display_info,.acquire=display_acquire,.release=display_release,.submit=display_submit,.present_status=display_status};
/* Model the actual queued raw-touch contract, including report watermarks.
 * contact is physical state; snapshot exposes only the last polled report. */
static risc_touch_snapshot_v1 touch_report;
static risc_touch_event_v1 touch_events[32];
static unsigned touch_at,touch_count;
static uint64_t touch_sequence;
static int touch_find(const risc_touch_snapshot_v1 *s,unsigned id){for(unsigned i=0;i<s->contact_count;i++)if(s->contacts[i].id==id)return (int)i;return -1;}
static void touch_edge(unsigned kind,const risc_touch_contact_v1 *c){assert(touch_count<32);touch_events[touch_count++]=(risc_touch_event_v1){++touch_sequence,ticks,(uint8_t)kind,c->id,c->x,c->y};}
static uint64_t subscribe(void *c){(void)c;io();live_subs++;touch_at=touch_count=0;touch_report=contact;touch_report.sequence=touch_sequence;touch_report.timestamp_ms=ticks;return 1;}
static bool unsubscribe(void *c,uint64_t token){(void)c;io();assert(token==1&&live_subs);live_subs--;return true;}
static bool touch_poll(void *c,size_t n){
    (void)c;io();assert(n==1);polls++;if(touch_at==touch_count)touch_at=touch_count=0;
    for(unsigned i=0;i<touch_report.contact_count;i++)if(touch_find(&contact,touch_report.contacts[i].id)<0)touch_edge(RISC_TOUCH_EVENT_UP,&touch_report.contacts[i]);
    for(unsigned i=0;i<contact.contact_count;i++){
        int old=touch_find(&touch_report,contact.contacts[i].id);
        if(old<0)touch_edge(RISC_TOUCH_EVENT_DOWN,&contact.contacts[i]);
        else if(contact.contacts[i].x!=touch_report.contacts[old].x||contact.contacts[i].y!=touch_report.contacts[old].y)touch_edge(RISC_TOUCH_EVENT_MOVE,&contact.contacts[i]);
    }
    touch_report=contact;touch_report.sequence=touch_sequence;touch_report.timestamp_ms=ticks;return true;
}
static int32_t touch_next(void *c,uint64_t t,risc_touch_event_v1 *e){(void)c;(void)t;io();if(touch_at==touch_count)return 0;*e=touch_events[touch_at++];return 1;}
static bool touch_snapshot(void *c,risc_touch_snapshot_v1 *s){(void)c;io();*s=touch_report;return true;}
static const risc_touch_api_v1 touch_api={1,sizeof(touch_api),NULL,subscribe,unsubscribe,touch_poll,touch_next,touch_snapshot};
static bool nav_poll(void *c,risc_input_navigation_frame_v1 *out){(void)c;io();*out=(risc_input_navigation_frame_v1){0};if(nav_home){out->buttons=out->pressed=RISC_NAV_HOME;nav_home=false;}return true;}
static bool nav_foreground(void *c,const risc_input_foreground_v1 *claims,size_t count){(void)c;(void)claims;io();assert(count<=1);return true;}
static bool nav_reset(void *c){(void)c;io();return true;}
static const risc_input_navigation_api_v1 nav_api={1,sizeof(nav_api),NULL,nav_poll,nav_foreground,nav_reset};
static bool battery_read(void *c,risc_battery_sample_v1 *s){(void)c;io();*s=(risc_battery_sample_v1){.percent=72};return true;}
static const risc_battery_gauge_api_v1 battery_api={.api_version=1,.struct_size=sizeof(battery_api),.read=battery_read};
#ifdef TEST_RESIDENT_CLIENT
#define TEST_RESIDENT_IO() io()
#include "resident_telemetry_fixture.h"
#endif
static bool text_enabled=true,text_opened;
static unsigned text_runtime_loss;
static unsigned text_opens,text_polls,text_closes,text_pending;
static int text_close_error;
static risc_text_entry_state_v1 text_state;
static risc_text_entry_request_v1 text_request;
static int32_t text_open(void *c,const risc_text_entry_request_v1 *request,uint64_t *session){
    (void)c;io();assert(!live_frames&&!live_subs&&!text_opened&&!display_inflight);text_opens++;text_opened=true;
    text_request=*request;text_state=(risc_text_entry_state_v1){.struct_size=sizeof(text_state),.revision=1};
    memcpy(text_state.text,request->text,sizeof(text_state.text));*session=42;if(text_runtime_loss==2)runtime_lost=true;return RISC_TEXT_ENTRY_OK;
}
static int32_t text_poll(void *c,uint64_t session,risc_text_entry_state_v1 *out){
    (void)c;io();assert(text_opened&&session==42&&!live_frames&&!live_subs);text_polls++;
    if(script_mode==3){
        assert(script_at==3&&!writes&&!live_frames&&!live_subs);
        if(entry_text_alarm)fake_alarm_state=ALARM_STATE_ALERT;
        else {text_state.state=entry_text_result;strcpy(text_state.text,"Entrypoint name");}
    }
    if(script_mode==5){
        assert(text_request.reserved==RISC_TEXT_ENTRY_REQUEST_HOME_REASON);
        text_state.state=RISC_TEXT_ENTRY_CANCELLED;
        text_state.flags=RISC_TEXT_ENTRY_HOME_CANCEL|(text_pending?RISC_TEXT_ENTRY_PRESENTING:0u);
        strcpy(text_state.text,"Discard this typed name");
    }
    *out=text_state;if(text_runtime_loss==3)runtime_lost=true;return RISC_TEXT_ENTRY_OK;
}
static int32_t text_close(void *c,uint64_t session){
    (void)c;io();assert(text_opened&&session==42&&!live_frames&&!live_subs);text_closes++;
    if(text_close_error)return text_close_error;
    if(text_pending){text_pending--;return RISC_TEXT_ENTRY_AGAIN;}
    text_opened=false;
    /* This entrypoint scenario qualifies shared-session alarm cancellation;
     * the independent alarm foreground fixture owns its rendering protocol. */
    if(script_mode==3&&entry_text_alarm)fake_alarm_state=ALARM_STATE_READY;
    if(text_runtime_loss==4)runtime_lost=true;
    return RISC_TEXT_ENTRY_OK;
}
static const risc_text_entry_api_v1_home_reason text_api={
    {1,sizeof(text_api),NULL,text_open,text_poll,text_close},
    RISC_TEXT_ENTRY_HOME_REASON_TAG,RISC_TEXT_ENTRY_HOME_REASON_VERSION};
static void text_complete(unsigned state,const char *value){
    assert(text_opened);text_state.state=state;if(value){memset(text_state.text,0,sizeof(text_state.text));strcpy(text_state.text,value);}pc_name_step();
    if(!name_client.active&&pc_live()){t5_app_input_t neutral={0};contact.contact_count=0;assert(render_app->poll(&neutral,20));}
}
static bool acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *g) {
    io();const void *api=NULL;
    if(!strcmp(name,RISC_TEXT_ENTRY_CAPABILITY)){
        if(text_runtime_loss==6){runtime_lost=true;return false;}
        if(!text_enabled)return false;
        api=&text_api;if(text_runtime_loss==1)runtime_lost=true;
    }
    else if(!strcmp(name,"display.output")){assert(version==1&&!instance);api=&display_api;}
    else if(!strcmp(name,"input.touch.raw")){assert(version==1&&!instance);api=&touch_api;}
    else if(!strcmp(name,"input.navigation")){assert(version==1&&!instance);api=&nav_api;}
    else if(!strcmp(name,"board.battery")){assert(version==1&&!instance);api=&battery_api;}
    else if(!strcmp(name,"storage.app-data")){
#ifdef PC_FAKE_BACKGROUND
        assert(background_phase==2);
#endif
        assert(version==1&&instance==5);if(acquire_lost){runtime_lost=true;return false;}api=&file_api;}
    else if(!strcmp(name,"storage.key-value")){assert(version==1&&(instance==1||instance==5));api=instance==5?&legacy_api:&preference_api;}
    else if(!strcmp(name,ALARM_SERVICE_CAPABILITY)){
#ifdef ALARM_NATIVE_UTC
        assert(version==2&&!instance);
#else
        assert(version==1&&!instance);
#endif
        api=old_service?(const void*)&old_alarm_api:(const void*)&alarm_api;
    }
#ifdef ALARM_NATIVE_UTC
    else if(!strcmp(name,"runtime.realtime")){assert(version==1&&!instance);api=&time_api;}
#else
    else if(!strcmp(name,"rtc.clock")){assert(version==2&&!instance);api=&time_api;}
#endif
    #ifdef TEST_RESIDENT_CLIENT
    else if(!strcmp(name,"telemetry.broadcast"))api=&resident_bt;
#endif
    else {fprintf(stderr,"Unexpected capability %s@%u / %llu\n",name,version,(unsigned long long)instance);assert(false);}
    g->api=api;g->slot=++live_grants;g->generation=1;return true;
}
static bool release(risc_runtime_capability_v1 *g){io();assert(live_grants&&g->api);
#ifdef PC_FAKE_BACKGROUND
    if(g->api==&file_api)assert(background_phase==2);
#endif
    if(release_failure)return false;
    if(g->api==&text_api&&text_runtime_loss==5)runtime_lost=true;
    live_grants--;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
#ifdef TEST_RESIDENT_CLIENT
#include "resident_test_bridge.h"
#else
#define TEST_RESIDENT_BINDING
#endif
static const risc_runtime_api_v1 runtime_api={TEST_RESIDENT_BINDING .api_version=1,.struct_size=sizeof(runtime_api),.health=health,.yield_ms=yield_ms,.diagnostic=diagnostic,.request_launch=launch,.acquire=acquire,.release=release,.retain_invocation=retain_invocation};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1&&!runtime_lost?&runtime_api:NULL;}
static bool fixture_poll(t5_app_input_t *in,uint32_t delay) {
    bool ok=render_app->poll(in,delay);
    if(!ok)return false;
    if(script_mode==1){assert(script_at<script_count);*in=script[script_at++];}
    else if(script_mode==2){
        if(!script_at){assert(points_editor_begin_event(&editor,0));unknown_save=true;pc_save(false);assert(editor.store.uncertain);script_at++;nav_home=true;}
        else {
#ifdef ALARM_NATIVE_UTC
#ifdef TEST_RESIDENT_CLIENT
            assert(script_at==1&&in->exit_requested&&!launched_home&&editor.store.uncertain&&writes==1);
#else
            assert(script_at==1);
            assert(in->exit_requested&&launched_home);
#endif
#else
            assert(script_at==1);
            *in=(t5_app_input_t){.exit_requested=true};
#endif
            script_at++;
        }
    } else if(script_mode==5){
        assert(script_at==0&&page==PC_LIST&&!writes);
        assert(points_editor_begin_type(&editor,0));pc_page(PC_CUSTOM);
        strcpy(editor.type.name,"Unsaved type");pc_custom_action(0);
        assert(page==PC_NAME&&name_client.active);script_at++;
    } else if(script_mode==4){
        if(!script_at){
            if(home_test_page==PC_CUSTOM||home_test_page==PC_COLOR)assert(points_editor_begin_type(&editor,0));
            else if(home_test_page!=PC_LIST&&home_test_page!=PC_TYPES)assert(points_editor_begin_event(&editor,0));
            pc_page(home_test_page);nav_home=true;script_at++;
        }else{
            fprintf(stderr,"Home page %u: exit=%u draft=%u/%u notice=%s\n",home_test_page,in->exit_requested,editor.editing_event,editor.editing_type,notice?notice:"");
            assert(script_at==1&&in->exit_requested&&!launches&&!writes);script_at++;
        }
    } else if(script_mode==3){
        assert(!name_client.active&&!text_opened);
        if(script_at==0){assert(page==PC_LIST);in->buttons=T5_APP_BUTTON_CONFIRM;script_at++;}
        else if(script_at==1){assert(page==PC_EDIT);in->buttons=T5_APP_BUTTON_CONFIRM;script_at++;}
        else if(script_at==2){assert(page==PC_TYPES);focus=editor.store.saved.type_count;in->buttons=T5_APP_BUTTON_CONFIRM;script_at++;}
        else {
            assert(script_at>=3&&!writes&&!text_pending);
            if(script_at==3){
                if(text_enabled&&entry_text_result==RISC_TEXT_ENTRY_ACCEPTED&&!entry_text_alarm)
                    assert(page==PC_CUSTOM&&!strcmp(editor.type.name,"Entrypoint name"));
                else if(text_enabled)assert(page==PC_TYPES&&!editor.editing_type);
                else assert(page==PC_CUSTOM&&editor.editing_type&&strstr(notice,"unavailable"));
                fake_alarm_state=ALARM_STATE_READY;
                if(entry_text_home){nav_home=true;script_at++;return true;}
            }
            if(entry_text_home){
#ifdef TEST_RESIDENT_CLIENT
                assert(script_at==4&&in->exit_requested&&!launched_home&&editor.editing_type);
                script_at++;return true;
#else
                assert(in->exit_requested&&launched_home);
                script_at++;return true;
#endif
            }
            in->buttons=T5_APP_BUTTON_BACK;script_at++;
        }
    }
    return true;
}
const t5_app_api_v1 *fixture_app_get(uint32_t v){return v==1?&fixture_app:NULL;}
static void settle(void) {
    hold_frame=false;
#ifdef ALARM_NATIVE_UTC
    for(unsigned i=0;i<20&&!paper_frame_ready();i++){t5_app_input_t in={0};assert(render_app->poll(&in,20));}
    assert(paper_frame_ready());
#endif
}
static void paint(void){settle();dirty=true;pc_draw();settle();}
static void frame(const char *name) {
    const char *dir=getenv("POINTS_CATALOG_FRAMES");if(!dir)return;char path[512];
#ifdef ALARM_NATIVE_UTC
    snprintf(path,sizeof(path),"%s/%s.pbm",dir,name);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P4\n800 480\n");assert(fwrite(pixels,1,48000,f)==48000);fclose(f);
#else
    snprintf(path,sizeof(path),"%s/%s.ppm",dir,name);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n240 240\n255\n");
    for(unsigned i=0;i<240*240;i++){uint16_t p=((uint16_t*)pixels)[i];uint8_t rgb[3]={(uint8_t)(((p>>11)&31)*255/31),(uint8_t)(((p>>5)&63)*255/63),(uint8_t)((p&31)*255/31)};assert(fwrite(rgb,1,3,f)==3);}fclose(f);
#endif
}
static void start(void) {
    assert(!live_grants&&!live_subs&&!live_frames&&!terminal);retained=false;
    script_mode=script_at=script_count=0;hold_frame=false;contact=(risc_touch_snapshot_v1){.width=480,.height=800};
#ifndef ALARM_NATIVE_UTC
    contact.width=contact.height=240;
#endif
    int init_result=app_module_init();if(init_result)fprintf(stderr,"init=%d calls=%u grants=%u subs=%u terminal=%u\n",init_result,calls,live_grants,live_subs,terminal);assert(init_result==0);render_app=t5_app_get_api(1);fixture_app=*render_app;fixture_app.poll=fixture_poll;app=&fixture_app;
    paper=paper_presentation_get();nova=paper?NULL:springboard_presentation_get();
#ifdef ALARM_NATIVE_UTC
    assert(paper&&pc_width()==480&&pc_height()==800);
#else
    assert(!paper&&nova&&pc_width()==240&&pc_height()==240);
#endif
    acquired=0;ready=pc_open();assert(ready);pc_load();if(!(pc_live()&&editor.store.loaded&&clock_valid&&service_valid))fprintf(stderr,"start live=%u loaded=%u clock=%u service=%u error=%d\n",pc_live(),editor.store.loaded,clock_valid,service_valid,editor.error);assert(pc_live()&&editor.store.loaded&&clock_valid&&service_valid);
    notice="";return_page=PC_EDIT;pc_page(PC_LIST);paint();
}
static void finish(void){settle();pc_close();assert(!retained);app_module_fini();assert(!live_grants&&!live_subs&&!live_frames);}
static void tap_footer(bool right){
    if(page==PC_NAME){text_complete(right?RISC_TEXT_ENTRY_ACCEPTED:RISC_TEXT_ENTRY_CANCELLED,right?name_draft:NULL);return;}
#ifdef ALARM_NATIVE_UTC
    if(paper&&ready&&editor.store.loaded&&editor.index_ready&&!editor.store.uncertain&&!editor.conflict) {
        if(page==PC_LIST){if(right)pc_tap(420,46);else exit_app=pc_back();return;}
        if(page==PC_EDIT){if(right){pcp_edit_offset=248;pc_tap(348,834-pcp_edit_offset);}else exit_app=pc_back();return;}
        if(page==PC_CUSTOM){pc_tap(right?348:132,740);return;}
        if(page==PC_NAME){if(right)pc_tap(406,688);else pc_tap(56,46);return;}
    }
#endif
    pc_tap(right?pc_width()-65:65,paper?740:pc_footer_top()+15);
}
static void retain_present(bool full){(void)full;pc_retain();}
#ifdef ALARM_NATIVE_UTC
static int32_t short_screen(void){return 480;}
#endif
#ifdef ALARM_NATIVE_UTC
static void key_contact(unsigned count,int x,int y,unsigned id) {
    contact.contact_count=(uint8_t)count;contact.contacts[0]=(risc_touch_contact_v1){.id=(uint8_t)id,.x=(uint16_t)x,.y=(uint16_t)y};
    t5_app_input_t input={0};assert(render_app->poll(&input,20));pc_input(&input);if(dirty)pc_draw();
}

#endif
static int fault_test(const char *kind) {
    start();
    if(!strncmp(kind,"text-entrypoint-",16)) {
        entry_text_result=strstr(kind,"cancel")?RISC_TEXT_ENTRY_CANCELLED:RISC_TEXT_ENTRY_ACCEPTED;
        entry_text_alarm=strstr(kind,"alarm")!=NULL;entry_text_home=strstr(kind,"home")!=NULL;
        text_enabled=strstr(kind,"unavailable")==NULL;
        for(unsigned pass=0;pass<2;pass++) {
            pc_close();script_mode=3;script_at=0;text_pending=strstr(kind,"pending")?3:0;
            unsigned opens=text_opens,closes=text_closes,before=writes;
            app_main();
            if(retained||name_client.active||text_opened||acquired||writes!=before||script_at<5)
                fprintf(stderr,"entrypoint result: retained=%u active=%u opened=%u acquired=%u writes=%u steps=%u terminal=%u\n",retained,name_client.active,text_opened,acquired,writes,script_at,terminal);
            assert(!retained&&!name_client.active&&!text_opened&&!acquired&&writes==before&&script_at>=5);
            assert(text_opens==opens+(text_enabled?1u:0u));
            assert(text_closes==closes+(text_enabled?(strstr(kind,"pending")?4u:1u):0u));
            script_mode=0;app_module_fini();assert(!live_grants&&!live_subs&&!live_frames);
            if(!pass)start();
        }
        puts("Actual app_main shared text loop: result/close/release, Back or Home, fini, repeated invocation PASS");return 0;
    } else if(!strcmp(kind,"editor-navigation")) {
        const unsigned before=writes,count=editor.order_count;
        uint32_t existing_id=points_editor_at(&editor,0)->id;
        for(unsigned pass=0;pass<4;pass++) {
            assert(points_editor_begin_event(&editor,pass&1?existing_id:0));pc_page(PC_EDIT);
            assert(!strcmp(pc_edit_title(),pass&1?"EDIT POINT":"ADD POINT"));
            points_catalog_item original=editor.event;editor.event.hour=(editor.event.hour+1)%24;
#ifdef ALARM_NATIVE_UTC
            pcp_edit_offset=pass>=2?248:0;
#endif
            paint();frame(pass&1?"edit-visible-back":"add-visible-back");
            pc_tap(paper?56:50,paper?46:219);
            assert(page==PC_LIST&&!editor.editing_event&&writes==before&&editor.order_count==count);
            if(original.id)assert(points_catalog_find_event(&editor.store.saved,original.id)->hour==original.hour);
        }
        tap_footer(true);assert(page==PC_EDIT&&!editor.event.id&&!strcmp(pc_edit_title(),"ADD POINT"));
        editor.event.hour=7;tap_footer(true);assert(page==PC_LIST&&writes==before+1&&editor.order_count==count+1);
        uint32_t saved=editor.store.saved.events[editor.store.saved.event_count-1].id;
        assert(points_editor_begin_event(&editor,saved));pc_page(PC_EDIT);
        assert(!strcmp(pc_edit_title(),"EDIT POINT")&&editor.event.hour==7);paint();frame("saved-point-edit-heading");
        pc_tap(paper?56:50,paper?46:219);assert(page==PC_LIST&&writes==before+1);
        finish();puts("Add/Edit titles and visible Back hit targets preserve discard/save semantics in both layouts");return 0;
    } else if(!strcmp(kind,"types-scroll")) {
#ifdef ALARM_NATIVE_UTC
        for(unsigned i=0;i<5;i++){assert(points_editor_begin_type(&editor,0));snprintf(editor.type.name,sizeof(editor.type.name),"EXTRA TYPE %u",i);assert(points_editor_save_type(&editor,&files,false)==RISC_APP_DATA_OK);}
        assert(points_editor_begin_event(&editor,0));pc_page(PC_TYPES);paint();key_contact(0,0,0,0);
        uint32_t chosen=editor.event.type_id;unsigned before=writes;
        key_contact(1,240,500,1);hold_frame=true;key_contact(1,240,450,1);
        assert(portable_scroll_offset(&pcp_types_scroll)==50&&page==PC_TYPES&&editor.event.type_id==chosen);
        unsigned submitted=frames;key_contact(1,240,420,1);
        assert(portable_scroll_offset(&pcp_types_scroll)==80&&frames==submitted);
        key_contact(0,0,0,0);assert(page==PC_TYPES&&writes==before);
        hold_frame=false;settle();pcp_types_scroll.velocity_q8=0;paint();key_contact(0,0,0,0);frame("types-scrolled");
        key_contact(1,240,180,2);key_contact(0,240,180,2);assert(page==PC_EDIT&&editor.event.type_id!=chosen&&writes==before);
        pc_page(PC_TYPES);paint();pc_select_type(editor.store.saved.type_count,false);
        assert(page==PC_NAME&&editor.editing_type&&name_return_page==PC_TYPES);paint();frame("new-type-direct-keyboard");
        text_complete(RISC_TEXT_ENTRY_ACCEPTED,"Q");assert(page==PC_CUSTOM&&!strcmp(editor.type.name,"Q"));paint();frame("type-default-controls");
        pc_tap(56,46);assert(page==PC_TYPES&&!editor.editing_type&&writes==before);
        pc_select_type(editor.store.saved.type_count,false);assert(page==PC_NAME);text_complete(RISC_TEXT_ENTRY_CANCELLED,NULL);assert(page==PC_TYPES&&!editor.editing_type&&writes==before);
#endif
        finish();puts("Actual contact type scrolling tracks finger during pending display; drag never selects; shared host naming and defaults retain cancel semantics");return 0;
    } else if(!strcmp(kind,"text-input")) {
        const unsigned before=writes;assert(points_editor_begin_type(&editor,0));pc_page(PC_CUSTOM);
        strcpy(editor.type.name,"Original");pc_custom_action(0);assert(name_client.active&&page==PC_NAME);assert(!portable_app_before_launch("default.elf")&&!pc_back());
        assert(!strcmp(text_request.label,"Type name")&&text_request.capacity==sizeof(name_draft)&&!strcmp(text_request.text,"Original"));
        unsigned count=frames;pc_draw();assert(frames==count);t5_app_input_t blocked={0};unsigned io_before=calls;assert(!render_app->poll(&blocked,20)&&calls==io_before);
        text_complete(RISC_TEXT_ENTRY_ACCEPTED,"Host copy");assert(page==PC_CUSTOM&&!strcmp(editor.type.name,"Host copy")&&!name_client.active&&live_subs==1&&writes==before);
        pc_custom_action(0);text_complete(RISC_TEXT_ENTRY_CANCELLED,"Discard");assert(page==PC_CUSTOM&&!strcmp(editor.type.name,"Host copy")&&writes==before);
        pc_custom_action(0);text_complete(RISC_TEXT_ENTRY_ACCEPTED,"   ");assert(page==PC_CUSTOM&&!strcmp(editor.type.name,"Host copy")&&writes==before);
        text_enabled=false;pc_custom_action(0);assert(page==PC_CUSTOM&&!name_client.active&&strstr(notice,"unavailable")&&live_subs==1);text_enabled=true;
        pc_custom_action(0);text_pending=2;unsigned polls_before=text_polls;count=frames;text_complete(RISC_TEXT_ENTRY_ACCEPTED,"Pending");assert(name_client.active&&name_client.closing&&text_opened&&!live_subs&&!strcmp(editor.type.name,"Host copy"));
        pc_draw();pc_name_step();assert(name_client.active&&frames==count&&text_polls==polls_before+1&&!live_subs&&writes==before);
        pc_name_step();assert(page==PC_CUSTOM&&!name_client.active&&live_subs==1&&!strcmp(editor.type.name,"Pending"));
        /* A held Home from the host cannot launch the app after restoration. */
        nav_home=true;assert(render_app->poll(&blocked,20));assert(!blocked.buttons&&!blocked.exit_requested&&!launches);
        finish();puts("Host naming: copied draft, accept/cancel, validation, absent provider, pending close, no modal app I/O, neutral restore passed");return 0;
    } else if(!strcmp(kind,"text-frame")) {
        assert(points_editor_begin_type(&editor,0));pc_page(PC_CUSTOM);paint();
#ifdef ALARM_NATIVE_UTC
        strcpy(editor.type.name,"Drain pending");dirty=true;hold_frame=true;frame_release_after=ticks+32;pc_draw();assert(display_inflight&&!paper_frame_ready());
        pc_custom_action(0);assert(name_client.active&&!display_inflight&&!live_frames&&!live_subs);
#else
        portable_nova_begin();assert(live_frames==1);pc_custom_action(0);assert(name_client.active&&!live_frames&&!live_subs);
#endif
        text_complete(RISC_TEXT_ENTRY_CANCELLED,NULL);finish();puts("Text handoff drains pending async frame and releases writable app lease before host open");return 0;
    } else if(!strncmp(kind,"text-runtime-",13)) {
        text_runtime_loss=(unsigned)strtoul(kind+13,NULL,10);assert(text_runtime_loss>=1&&text_runtime_loss<=6);
        assert(points_editor_begin_type(&editor,0));pc_page(PC_CUSTOM);pc_custom_action(0);
        if(text_runtime_loss>=3&&text_runtime_loss<=5){text_state.state=RISC_TEXT_ENTRY_ACCEPTED;strcpy(text_state.text,"Unsafe");pc_name_step();}
        assert(terminal&&retained&&retains==1&&runtime_lost&&!writes);
    } else if(!strcmp(kind,"text-abandoned")) {
        assert(points_editor_begin_type(&editor,0));pc_page(PC_CUSTOM);pc_custom_action(0);
        unsigned before=calls,closes_before=text_closes;
        assert(!portable_app_before_launch("default.elf"));app_module_fini();
        assert(terminal&&retains==1&&text_opened&&name_client.active&&text_closes==closes_before&&calls==before);
        assert(!pc_live());assert(retained);
    } else if(!strcmp(kind,"text-retention")) {
        assert(points_editor_begin_type(&editor,0));pc_page(PC_CUSTOM);pc_custom_action(0);
        text_close_error=RISC_TEXT_ENTRY_RETAINED;text_complete(RISC_TEXT_ENTRY_ACCEPTED,"Retained");assert(terminal&&retained&&retains==1&&text_opened&&name_client.active&&!live_subs&&!writes);
    } else if(!strcmp(kind,"text-alarm")) {
        assert(points_editor_begin_type(&editor,0));pc_page(PC_CUSTOM);pc_custom_action(0);
        unsigned before_close=text_closes;
        fake_alarm_state=ALARM_STATE_ALERT;text_state.flags=RISC_TEXT_ENTRY_PRESENTING;text_pending=2;pc_name_step();assert(name_client.active);
#ifdef TEST_DECOUPLED_UI
        assert(name_client.closing&&text_closes==before_close+1);
#else
        assert(!name_client.closing&&text_closes==before_close);
#endif
        text_pending=0;text_state.flags=0;pc_name_step();assert(!name_client.active&&page==PC_CUSTOM&&!editor.type.name[0]&&!writes);fake_alarm_state=ALARM_STATE_READY;
        finish();puts("Alarm attention follows selected logical policy; close custody must complete before app resumes");return 0;
    } else if(!strcmp(kind,"service-busy")) {
        assert(points_editor_begin_event(&editor,0));pc_page(PC_EDIT);fake_alarm_state=ALARM_STATE_ALERT;
        pc_save(false);assert(!writes&&editor.editing_event);fake_alarm_state=ALARM_STATE_READY;pc_save(false);assert(writes==1&&!editor.editing_event);finish();
        puts("Save rechecks current alarm ownership before mutation and allows a later retry");return 0;
    } else if(!strcmp(kind,"old-service")) {
        pc_close();old_service=true;unsigned before=writes;script_mode=1;script_at=0;script_count=1;script[0]=(t5_app_input_t){.buttons=T5_APP_BUTTON_BACK};
        app_main();assert(!ready&&!retained&&writes==before&&!acquired);app_module_fini();assert(!live_grants&&!live_subs&&!live_frames);
        puts("Old alarm service refused before catalog load/edit; no AppData write");return 0;
    } else if(!strcmp(kind,"controls")) {
        unsigned before=writes;assert(points_editor_begin_event(&editor,0));pc_page(PC_EDIT);
        pc_edit_action(1);uint8_t hour=editor.event.hour,minute=editor.event.minute;
        pc_tap(paper?130:175,paper?174:142);pc_tap(paper?350:175,paper?174:178);
        assert(editor.event.hour==(hour+1)%24&&editor.event.minute==(minute+1)%60);
        tap_footer(false);assert(page==PC_EDIT&&editor.event.hour==hour&&editor.event.minute==minute);
        assert(points_editor_begin_type(&editor,0));pc_page(PC_CUSTOM);uint32_t color=editor.type.color;
        pc_custom_action(1);pc_tap(pc_left()+5,paper?190:112);assert(editor.type.color==palette[0]);tap_footer(false);assert(editor.type.color==color);
#ifdef ALARM_NATIVE_UTC
        points_editor_cancel_type(&editor);assert(points_editor_begin_event(&editor,4));pcp_edit_offset=0;pc_page(PC_EDIT);
        uint8_t notify=editor.event.notify_end,warn=editor.event.warn3,enabled=editor.event.enabled;
        pc_tap(415,339);assert(editor.event.notify_end!=notify);pc_tap(415,401);assert(editor.event.warn3!=warn);
        editor.event.weekdays=0;for(unsigned i=0;i<7;i++)pc_tap(58+(int)i*60,564);assert(editor.event.weekdays==127);
        pc_tap(240,620);assert(editor.event.weekdays==62);pc_tap(386,620);assert(editor.event.weekdays==65);
        pc_tap(415,688);assert(editor.event.enabled!=enabled);tap_footer(false);assert(!writes);
        assert(points_editor_begin_type(&editor,0));pc_page(PC_CUSTOM);pc_custom_action(0);
        text_complete(RISC_TEXT_ENTRY_ACCEPTED,"Q");assert(page==PC_CUSTOM&&!strcmp(editor.type.name,"Q"));
        pc_custom_action(1);pc_tap(186,576);assert(editor.type.symbol==5);tap_footer(true);paint();frame("catalog-name-done");
#endif
        assert(writes==before);finish();puts("Production control coordinates: time/color Cancel, inline weekdays/checks/toggle, host naming and domain symbol selection passed");return 0;
    } else if(!strcmp(kind,"reference")) {
#ifdef ALARM_NATIVE_UTC
        points_config sample={.revision=1,.points={
            {6,1,1,127,6,30,0,0,0},{1,1,1,62,8,30,0,0,0},
            {4,1,1,62,10,15,15,1,1},{3,1,3,62,12,0,60,1,1},
            {4,1,1,62,15,0,15,1,1},{2,1,3,62,17,0,0,0,0},
            {7,1,1,127,18,30,45,1,1},{5,0,1,127,22,0,0,0,0}}};
        points_meta names={.revision=1,.custom={{.color=0,.name="WAKE UP"},{.color=6,.name="DINNER"}}};
        assert(points_catalog_storage_migrate(&editor.store,&sample,&names,PC_DOMAIN)==0);assert(points_editor_reindex(&editor)==0);
        projected_id=4;pc_status();assert(catalog_projection.count==1&&catalog_projection.next[0].event_id==4);pc_page(PC_LIST);paint();frame("reference-points");
        assert(points_editor_begin_event(&editor,4));pcp_edit_offset=0;pc_page(PC_EDIT);paint();frame("reference-edit");
        pc_edit_action(1);paint();frame("reference-time");
        assert(points_editor_begin_type(&editor,0));pc_page(PC_CUSTOM);pc_custom_action(0);text_complete(RISC_TEXT_ENTRY_ACCEPTED,"POWER NAP");editor.type.symbol=6;paint();frame("reference-type-defaults");
#endif
        finish();puts("Reference data rendered through the production Points UI");return 0;
    } else if(!strncmp(kind,"background-",11)) {
#ifdef PC_FAKE_BACKGROUND
        bool opening=!strcmp(kind,"background-open"),closing=!strcmp(kind,"background-close");
        if(opening)pc_close();
        unsigned ctx=context_stops,ble=broadcast_stops,before=calls;
        refuse_context=!strcmp(kind,"background-context")||closing;refuse_broadcast=!refuse_context;
        if(opening)(void)pc_open();else if(closing)pc_close();else pc_load();
        assert(terminal&&retained&&retains==1&&context_stops==ctx+1);
        assert(broadcast_stops==ble+(refuse_context?0u:1u));assert(calls==before+(refuse_context?1u:2u));
#else
        assert(!"Background fixture required");
#endif
    } else if(!strcmp(kind,"acquire-loss")) {
        pc_close();acquire_lost=true;app_main();assert(terminal&&retained&&retains==1);
    } else if(!strcmp(kind,"initial-draw-retention")) {
        pc_close();fixture_app.present=retain_present;app_main();assert(terminal&&retained&&retains==1);
    } else if(!strcmp(kind,"read-retention")||!strcmp(kind,"resolve-read-retention")) {
        assert(points_editor_begin_event(&editor,0));pc_page(PC_EDIT);
        unknown_save=!strcmp(kind,"resolve-read-retention");pc_save(false);assert(document);
        read_failure=false;retain_on_read=true;if(unknown_save)pc_retry();else pc_load();assert(terminal&&retained&&retains==1);
    } else if(!strcmp(kind,"projection-retention")) {
        retain_projection=true;pc_status();assert(terminal&&retained&&retains==1);
    } else if(!strcmp(kind,"replace-retention")) {
        assert(points_editor_begin_event(&editor,0));pc_page(PC_EDIT);storage_retained=true;pc_save(false);assert(terminal&&retained&&retains==1&&editor.staged.events);
    } else if(!strcmp(kind,"release-retention")) {
        release_failure=true;pc_close();assert(terminal&&retained&&retains==1);
    } else if(!strcmp(kind,"geometry")) {
#ifdef ALARM_NATIVE_UTC
        pc_close();fixture_app.screen_height=short_screen;unsigned before=calls;app_main();assert(calls==before&&!acquired&&!retained);fixture_app.screen_height=render_app->screen_height;
#endif
        finish();puts("Unsupported paper geometry rejected before drawing/storage");return 0;
    } else if(!strcmp(kind,"home-uncertain")) {
        /* Home exits without issuing another write or resolving/rolling back
         * an uncertain prior save. Reopen reloads the complete saved file. */
        pc_close();script_mode=2;script_at=0;app_main();assert(!terminal&&!retained);
        assert(script_at==2);
        script_mode=0;app_module_fini();assert(!live_grants&&!live_subs&&!live_frames);
        read_failure=unknown_save=false;start();assert(editor.order_count==8&&writes==1);finish();
        puts("Profile-correct global Home custody: no rewrite or rollback, clean discard/exit, complete document reloaded");return 0;
    } else assert(!"Unknown fault test");
    unsigned before=calls;pc_load();pc_retry();pc_draw();pc_close();points_editor_dispose(&editor);app_main();app_module_fini();assert(calls==before&&retains==1);
    printf("Selected app terminal boundary passed: %s\n",kind);return 0;
}
int main(int argc,char **argv) {
    if(argc>1)return fault_test(argv[1]);
    start();assert(legacy_gets==2&&!writes);frame("catalog-list");
    tap_footer(true);assert(page==PC_EDIT);uint8_t original=editor.event.hour;pc_edit_action(1);paint();
    pc_tap(paper?130:pc_width()-65,paper?174:142);assert(editor.event.hour==(original+1)%24);paint();frame("catalog-time");
    tap_footer(true);tap_footer(false);assert(page==PC_LIST&&!writes);
    tap_footer(true);pc_edit_action(2);editor.event.weekdays=0;paint();
    int l=pc_left(),gap=paper?6:2,bw=(pc_width()-2*l-6*gap)/7;
    for(unsigned i=0;i<7;i++)pc_tap(paper?32+(int)i*60+3:l+(int)i*(bw+gap)+3,paper?226:110);
    assert(editor.event.weekdays==127);paint();frame("catalog-days");tap_footer(true);tap_footer(true);assert(writes==1&&page==PC_LIST&&editor.order_count==8);
    for(unsigned i=0;i<28;i++){tap_footer(true);editor.event.hour=(uint8_t)(i%24);editor.event.minute=3;tap_footer(true);assert(page==PC_LIST);}
    assert(editor.order_count==36);paint();frame("catalog-over-eight");pc_scroll(1);paint();frame("catalog-next-page");
    tap_footer(true);pc_edit_action(0);pc_select_type(editor.store.saved.type_count,false);assert(page==(paper?PC_NAME:PC_CUSTOM));
    pc_custom_action(0);assert(page==PC_NAME);text_complete(RISC_TEXT_ENTRY_CANCELLED,NULL);assert(!editor.type.name[0]);
    if(!editor.editing_type){assert(points_editor_begin_type(&editor,0));pc_page(PC_CUSTOM);}
    pc_custom_action(0);text_complete(RISC_TEXT_ENTRY_ACCEPTED,"Afternoon Walk");assert(!strcmp(editor.type.name,"Afternoon Walk"));
    pc_custom_action(1);pc_tap(pc_left()+5,paper?190:112);tap_footer(true);assert(editor.type.color==palette[0]);
    editor.type.duration_minutes=25;editor.type.mode=3;editor.type.flags=7;paint();frame("catalog-custom-type");tap_footer(true);
    assert(page==PC_EDIT&&editor.event.duration_minutes==25&&editor.event.mode==3&&editor.event.notify_end&&editor.event.warn3);
    uint32_t type_id=editor.event.type_id;tap_footer(true);assert(page==PC_LIST&&points_catalog_find_type(&editor.store.saved,type_id));
    tap_footer(true);unsigned before=writes;storage_capacity=document_size;tap_footer(true);assert(page==PC_EDIT&&editor.error==RISC_APP_DATA_NO_SPACE&&editor.editing_event&&writes==before+1);paint();frame("catalog-storage-full");
    storage_capacity=65536;tap_footer(true);assert(page==PC_LIST);
    tap_footer(true);unknown_save=true;tap_footer(true);assert(editor.store.uncertain);before=writes;paint();frame("catalog-save-unconfirmed");
    tap_footer(false);assert(editor.store.uncertain&&writes==before);tap_footer(true);assert(editor.store.uncertain&&writes==before);
    read_failure=unknown_save=false;tap_footer(true);assert(!editor.store.uncertain&&writes==before&&page==PC_LIST);
    /* Same renderer/controller coordinates after each cancellation and entry. */
    for(unsigned i=0;i<8;i++){tap_footer(true);pc_edit_action(2);tap_footer(false);tap_footer(false);assert(page==PC_LIST&&writes==before);}
#ifdef ALARM_NATIVE_UTC
    /* A pending real async present owns the frame. Inputs cannot act on its
     * unseen buttons, and app drawing retains dirty state until completion. */
    tap_footer(true);pcp_edit_offset=248;paint();unsigned count=editor.order_count;hold_frame=true;dirty=true;editor.event.enabled^=1;pc_draw();assert(!paper_frame_ready());
    t5_app_input_t save={.tapped=true,.touch_x=348,.touch_y=586};pc_input(&save);assert(editor.order_count==count&&writes==before);dirty=true;unsigned submitted=frames;pc_draw();assert(dirty&&frames==submitted);
    settle();pc_draw();settle();assert(!dirty);tap_footer(false);assert(page==PC_LIST);
#endif
    finish();unsigned saved_count=editor.order_count;assert(!saved_count);
    start();assert(editor.order_count==39&&legacy_gets==2);finish();
    /* app_main owns Back and returns only at root. A nested Back cancels an
     * event draft; it cannot queue an app launch until the following Back. */
    start();pc_close();script_mode=1;script_at=0;script_count=3;
    script[0]=(t5_app_input_t){.buttons=T5_APP_BUTTON_CONFIRM};
    script[1]=(t5_app_input_t){.buttons=T5_APP_BUTTON_BACK};script[2]=(t5_app_input_t){.buttons=T5_APP_BUTTON_BACK};
    before=writes;unsigned launch_before=launches;app_main();assert(script_at==3&&writes==before&&launches==launch_before+1);script_mode=0;app_module_fini();assert(!live_grants&&!live_subs&&!live_frames);
    /* AppData retention fences every subsequent provider, draw, teardown,
     * retry and repeated entry, including app_module_fini in the real adapter. */
    start();storage_retained=true;pc_load();assert(terminal&&retained&&retains==1);unsigned calls_before=calls;
    pc_load();pc_retry();pc_save(false);pc_status();pc_draw();pc_close();assert(!pc_back());app_main();app_module_fini();assert(calls==calls_before&&retains==1);
    /* Retained allocations intentionally belong to the frozen invocation. */
    printf("Points catalog %s production app/renderer: 39 saved events, days/custom type/host naming, full and uncertain saves, repeated entry, Back, async frame gating and terminal retention passed\n",paper?"X4":"Watch");
    return 0;
}
