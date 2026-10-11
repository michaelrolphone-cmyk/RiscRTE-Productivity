/* Selected Points catalog source; the selected manifest owns its version.
 * The legacy source/build remains intact.
 * Storage capacity determines catalog size; the app never writes legacy KV. */
#ifdef PORTABLE_RETURN_APP
#error Points requires app-owned POINTS_RETURN_APP
#endif
#include "T5AppApi.h"
#include "SpringboardPresentation.h"
#include "PaperPresentation.h"
#include "PaperFrame.h"
#include "RiscRuntimeV1.h"
#include "PortableRtcClock.h"
#include "PortableTimeFormat.h"
#include "PortableNovaUi.h"
#include "PortableTextInputClient.h"
#include "PortableAppSleep.h"
#include "points_catalog_controller.h"
#include "PointsServiceProjection.h"
#ifdef ALARM_NATIVE_UTC
#include "AlarmServiceV2.h"
#include "PortableRealtimeClient.h"
#include "PortableTimeZonePreference.h"
#include "PointsUtcSchedule.h"
#include "PortableTouchScroll.h"
#ifndef ALARM_SERVICE_TAGGED_V2
#error Native catalog editor requires alarm.service API2
#endif
#endif
#include <stddef.h>
#include <stdlib.h>
/* App-local one-way custody fence supplied by the selected shared adapter. */
void portable_adapter_retain(void);
bool portable_adapter_retained(void);
#if defined(PORTABLE_NATIVE_CUSTODY_FENCE) && defined(PORTABLE_ALARM_TERMINAL_RETENTION)
void portable_adapter_retain_silent(void);
#endif
#ifdef PORTABLE_CONTEXTS_CLIENT
bool portable_contexts_stop(void);
#endif
#ifdef PORTABLE_BLE_BROADCAST
bool portable_broadcast_stop(void);
#endif
static const t5_app_api_v1 *app;
static const risc_runtime_api_v1 *runtime;
static portable_text_client name_client;
static risc_text_entry_state_v1 name_result;
static bool name_finished;
static bool pc_name_close(void);
static const alarm_service_v1 *service;
__attribute__((weak)) const paper_presentation *paper_presentation_get(void){return NULL;}
static const paper_presentation *paper;
static const springboard_presentation *nova;
static const risc_app_data_v1 *file_source;
static const risc_key_value_v1 *legacy_source,*prefs_source;
static risc_app_data_v1 files;
static risc_key_value_v1 legacy,prefs;
static risc_runtime_capability_v1 grants[5];
static unsigned acquired,time_format;
static points_editor editor;
static bool retained,ready,clock_valid,service_valid;
static alarm_status_v1 alarm_state;
static points_catalog_projection catalog_projection;
static const char *notice;
#ifdef ALARM_NATIVE_UTC
static portable_realtime_client clock_client;
static char zone[PORTABLE_TIMEZONE_ID_BYTES];
#else
static const twatch_rtc_api_v1 *rtc;
#endif
#ifdef ALARM_NATIVE_UTC
static void pc_paper_page_reset(void);
#endif
static void pc_retain(void);
static bool pc_live(void) {
    if(retained)return false;
    if(portable_adapter_retained()||portable_app_sleep_retained()){retained=true;editor.store.retained=true;name_client.retained=true;
        return false;}
    if(runtime&&!risc_runtime_get_api(1)){pc_retain();return false;}
    return true;
}
static void pc_retain(void) {
    if(retained)return;
    retained=true;editor.store.retained=true;name_client.retained=true;
#ifdef ALARM_NATIVE_UTC
    portable_realtime_stop(&clock_client,true);
#endif
#if defined(PORTABLE_NATIVE_CUSTODY_FENCE) && defined(PORTABLE_ALARM_TERMINAL_RETENTION)
    portable_adapter_retain_silent();
#else
    portable_adapter_retain();
#endif
}
static bool pc_storage_ready(void) {
    if(!pc_live())return false;
#ifdef PORTABLE_CONTEXTS_CLIENT
    if(!portable_contexts_stop()){pc_retain();return false;}
    if(!pc_live())return false;
#endif
#ifdef PORTABLE_BLE_BROADCAST
    if(!portable_broadcast_stop()){pc_retain();return false;}
#endif
    return pc_live();
}
static int32_t pc_file_status(int32_t rc) {
    if(rc==RISC_APP_DATA_CONTEXT||rc==RISC_APP_DATA_RETAINED||rc>0||rc<RISC_APP_DATA_STALE)pc_retain();
    return pc_live()?rc:RISC_APP_DATA_RETAINED;
}
static int32_t pc_stat(void *c,const char *name,uint32_t *size,uint64_t *rev) {
    (void)c;if(!pc_storage_ready())return RISC_APP_DATA_RETAINED;
    return pc_file_status(file_source->stat(file_source->context,name,size,rev));
}
static int32_t pc_read(void *c,const char *name,uint64_t expected,void *out,uint32_t cap,uint32_t *size,uint64_t *rev) {
    (void)c;if(!pc_storage_ready())return RISC_APP_DATA_RETAINED;
    return pc_file_status(file_source->read(file_source->context,name,expected,out,cap,size,rev));
}
static int32_t pc_replace(void *c,const char *name,uint64_t expected,const void *data,uint32_t size) {
    (void)c;if(!pc_storage_ready())return RISC_APP_DATA_RETAINED;
    return pc_file_status(file_source->replace(file_source->context,name,expected,data,size));
}
static int32_t pc_get(void *c,const char *key,void *out,uint32_t cap,uint32_t *size) {
    if(!pc_storage_ready())return RISC_KEY_VALUE_CONTEXT;
    const risc_key_value_v1 *kv=c;int32_t rc=kv->get(kv->context,key,out,cap,size);
    if(rc!=RISC_KEY_VALUE_OK&&rc!=RISC_KEY_VALUE_NOT_FOUND&&rc!=RISC_KEY_VALUE_BUFFER_SMALL&&rc!=RISC_KEY_VALUE_INVALID&&rc!=RISC_KEY_VALUE_IO)pc_retain();
    return pc_live()?rc:RISC_KEY_VALUE_CONTEXT;
}
static bool pc_service_result(int32_t rc) {
    if(rc==ALARM_RETAINED||rc==ALARM_OUTPUT||rc<ALARM_RETAINED||rc>ALARM_PENDING)pc_retain();
    return pc_live();
}
static void pc_status(void) {
    if(!pc_live()||!service)return;
    alarm_state=(alarm_status_v1){.struct_size=sizeof(alarm_state)};
    int32_t rc=service->status(service->context,&alarm_state);
    if(!pc_service_result(rc)||!pc_service_result(alarm_state.error))return;
    service_valid=rc==ALARM_OK&&alarm_state.api_version==1&&alarm_state.struct_size>=sizeof(alarm_state)&&alarm_state.state<=ALARM_STATE_CUE;
    catalog_projection=(points_catalog_projection){.struct_size=sizeof(catalog_projection)};
    if(service_valid){int32_t projected=points_service_project(service,&catalog_projection);if(!pc_service_result(projected))return;if(projected!=ALARM_OK||catalog_projection.count>POINTS_CATALOG_NEXT_COUNT)catalog_projection.count=0;}
}
#ifdef ALARM_NATIVE_UTC
static int pc_phase(void *c){(void)c;return pc_live()?PORTABLE_REALTIME_GUARD_SAFE:PORTABLE_REALTIME_GUARD_RETAINED;}
static bool pc_clock_result(int rc) {
    if(rc==PORTABLE_REALTIME_CONTEXT||rc==PORTABLE_REALTIME_UNCERTAIN||rc==PORTABLE_REALTIME_RETAINED)pc_retain();
    return pc_live()&&rc==PORTABLE_REALTIME_OK;
}
static bool pc_sample(uint32_t *seconds,portable_timezone_civil *civil) {
    if(!pc_live()||!pc_storage_ready())return false;
    int rc=portable_realtime_open(&clock_client,runtime,PORTABLE_REALTIME_READER,PORTABLE_REALTIME_TIMER_ONLY,pc_phase,NULL);
    if(!pc_clock_result(rc))return false;
    risc_realtime_snapshot_v1 sample={0};rc=portable_realtime_read(&clock_client,&sample);
    if(!pc_live())return false;
    if(rc==PORTABLE_REALTIME_CONTEXT||rc==PORTABLE_REALTIME_UNCERTAIN||rc==PORTABLE_REALTIME_RETAINED){pc_retain();return false;}
    int closed=portable_realtime_close(&clock_client);
    if(!pc_clock_result(closed)||rc!=PORTABLE_REALTIME_OK||!points_utc_from_unix(sample.epoch_seconds,seconds))return false;
    int z=portable_timezone_preference_load(&prefs,zone);
    if(!pc_live()||(z!=PORTABLE_TIMEZONE_LOADED&&z!=PORTABLE_TIMEZONE_MISSING))return false;
    portable_timezone_rule rule;portable_timezone_civil local;
    if(portable_timezone_resolve(zone,sizeof(zone),&rule)!=PORTABLE_TIMEZONE_OK||
       portable_timezone_utc_to_local(&rule,sample.epoch_seconds,&local,NULL)!=PORTABLE_TIMEZONE_OK)return false;
    if(civil)*civil=local;
    return true;
}
__attribute__((visibility("hidden"))) bool portable_app_native_local_time(twatch_rtc_time_v1 *out) {
    uint32_t seconds;portable_timezone_civil local;
    if(!out||!ready||!pc_sample(&seconds,&local))return false;
    *out=(twatch_rtc_time_v1){(uint16_t)local.year,local.month,local.day,local.weekday,local.hour,local.minute,local.second};return true;
}
static bool pc_clock(uint32_t *seconds){return pc_sample(seconds,NULL);}
#define PC_DOMAIN POINTS_TIME_NATIVE_UTC
#else
static bool pc_clock(uint32_t *seconds) {
    if(!pc_live())return false;
    twatch_rtc_time_v1 t;
    bool ok=rtc&&rtc->read(rtc->context,&t);
    return pc_live()&&ok&&t.weekday<=6&&alarm_calendar_seconds(t.year,t.month,t.day,t.hour,t.minute,t.second,seconds);
}
#define PC_DOMAIN POINTS_TIME_RAW_RTC
#endif
static bool pc_kv_valid(const risc_key_value_v1 *kv) {return kv&&kv->api_version==1&&kv->struct_size>=sizeof(*kv)&&kv->get;}
static bool pc_open(void) {
    if(!pc_live())return false;
    runtime=risc_runtime_get_api(1);
    if(!runtime||runtime->api_version!=1||runtime->struct_size<RISC_RUNTIME_CAPABILITIES_V1_SIZE||!runtime->acquire||!runtime->release||!runtime->yield_ms)return false;
#ifdef ALARM_NATIVE_UTC
    const char *names[]={RISC_APP_DATA_CAPABILITY,"storage.key-value","storage.key-value",ALARM_SERVICE_CAPABILITY};
    const unsigned versions[]={1,1,1,2};const uint64_t instances[]={5,5,1,0};
#else
    const char *names[]={RISC_APP_DATA_CAPABILITY,"storage.key-value","storage.key-value",ALARM_SERVICE_CAPABILITY,"rtc.clock"};
    const unsigned versions[]={1,1,1,1,2};const uint64_t instances[]={5,5,1,0,0};
#endif
    if(!pc_storage_ready())return false;
    for(unsigned i=acquired;i<sizeof(versions)/sizeof(versions[0]);i++) {
        grants[i]=(risc_runtime_capability_v1){.struct_size=sizeof(grants[i])};
        bool ok=runtime->acquire(names[i],versions[i],instances[i],&grants[i]);
        if(!pc_live())return false;
        if(!ok){if(grants[i].api||grants[i].slot||grants[i].generation)pc_retain();return false;}
        if(!grants[i].api||!grants[i].slot||!grants[i].generation){pc_retain();return false;}
        acquired++;
    }
    file_source=grants[0].api;legacy_source=grants[1].api;prefs_source=grants[2].api;service=grants[3].api;
    if(!points_catalog_storage_api(file_source)||!pc_kv_valid(legacy_source)||!pc_kv_valid(prefs_source))return false;
    files=(risc_app_data_v1){1,sizeof(files),NULL,pc_stat,pc_read,pc_replace};
    legacy=(risc_key_value_v1){1,sizeof(legacy),(void*)legacy_source,pc_get,NULL};
    prefs=(risc_key_value_v1){1,sizeof(prefs),(void*)prefs_source,pc_get,NULL};
#ifdef ALARM_NATIVE_UTC
    if(!alarm_service_descriptor(service))return false;
#else
    rtc=grants[4].api;
    if(!rtc||rtc->api_version!=2||rtc->struct_size<sizeof(*rtc)||!rtc->read||!service||service->api_version!=1||service->struct_size<sizeof(*service))return false;
#endif
    /* The selected writer needs a catalog-aware provider. API1/API2 alone
     * also describes older eight-slot providers that ignore this document. */
    const points_service_projection_suffix *projection=NULL;
    if(service->api_version==1&&service->struct_size>=sizeof(points_service_v1))projection=&((const points_service_v1*)service)->points;
    else if(service->api_version==2&&service->struct_size>=sizeof(points_service_v2))projection=&((const points_service_v2*)service)->points;
    if(!projection||projection->tag!=POINTS_SERVICE_PROJECTION_TAG||projection->version!=POINTS_SERVICE_PROJECTION_VERSION||!projection->projection)return false;
    return service->status&&service->refresh&&service->step&&service->stop_only&&service->acknowledge;
}
static void pc_close(void) {
    if(!pc_live()||!pc_name_close()||(acquired&&!pc_storage_ready()))return;
    while(acquired) {
        risc_runtime_capability_v1 released=grants[acquired-1];bool ok=runtime->release(&released);
        if(!pc_live())return;
        if(!ok||released.api||released.slot||released.generation){pc_retain();return;}
        grants[--acquired]=released;
    }
    points_editor_dispose(&editor);ready=false;runtime=NULL;service=NULL;file_source=NULL;legacy_source=prefs_source=NULL;
}
static void pc_load(void) {
    if(!pc_live()||!ready)return;
    int32_t rc=points_editor_load(&editor,&files,&legacy,PC_DOMAIN);if(!pc_live())return;
    notice=rc==0?"Choose a point or add one":"Storage unavailable: retry";
    (void)portable_time_format_load(&prefs,&time_format);if(!pc_live())return;
    uint32_t now;clock_valid=pc_clock(&now);if(!pc_live())return;pc_status();
}
static bool pc_visual_only(void) {
#ifdef ALARM_NATIVE_UTC
    const alarm_service_descriptor_v2 *d=alarm_service_descriptor(service);return d&&d->output_modes==ALARM_MODE_VISUAL;
#else
    return false; /* Selected raw-RTC Watch service has sound/vibration. */
#endif
}
static const char *pc_error(void) {
    if(!ready)return "Dependencies unavailable";
    if(editor.store.uncertain)return "Save unconfirmed: check storage";
    if(editor.conflict)return "Storage changed: reload to retry";
    if(editor.error==RISC_APP_DATA_NO_SPACE)return "Storage full: draft kept";
    if(editor.store.loaded&&!editor.index_ready)return "List unavailable: retry memory";
    if(editor.error==POINTS_STORAGE_MEMORY)return "Not enough memory: draft kept";
    if(!editor.store.loaded)return "Saved data unavailable: retry";
    if(!clock_valid)return "Time unavailable: retry";
    if(!service_valid)return "Alarm service unavailable";
    return notice?notice:"";
}
enum { PC_LIST,PC_EDIT,PC_TYPES,PC_TIME,PC_DAYS,PC_MODE,PC_DURATION,PC_CUSTOM,PC_NAME,PC_COLOR };
static unsigned page,first,focus,return_page,name_return_page;
static bool dirty,clean_frame,exit_app,delete_confirm,touch_down,touch_moved;
#ifdef ALARM_NATIVE_UTC
static bool paper_input_ready;
#endif
static int touch_x,touch_y,touch_last_y;
static char name_draft[POINTS_CATALOG_NAME_MAX+1];
static points_catalog_item subpage_event;
static points_catalog_type subpage_type;
static const char *const mode_names[]={"System default","Vibrate","Sound","Sound + vibrate"};
static const char *const day_names[]={"S","M","T","W","T","F","S"};
static const uint32_t palette[]={0xffd24a,0xff7a1a,0xff3d71,0xb24dff,0x6d7bff,0x3d9bff,0x19e3ff,0x3dff9a};
static bool pc_name_close(void){
    if(!name_client.active&&!name_client.acquired&&!name_client.suspended)return pc_live();
    int rc=portable_text_client_close(&name_client);
    if(rc==RISC_TEXT_ENTRY_AGAIN)return false;
    if(rc!=RISC_TEXT_ENTRY_OK){pc_retain();return false;}
    return pc_live();
}
static void pc_page(unsigned next) {
    if(page==PC_NAME&&next!=PC_NAME&&!pc_name_close())return;
    if(next==PC_NAME&&page!=PC_NAME){
        int rc=portable_text_client_begin(&name_client,runtime,"Type name",name_draft,sizeof(name_draft));
        if(rc==RISC_TEXT_ENTRY_RETAINED){pc_retain();return;}
        if(rc!=RISC_TEXT_ENTRY_OK){notice=rc==RISC_TEXT_ENTRY_BUSY?"Text input busy: retry":"Text input unavailable: draft kept";dirty=true;return;}
        name_finished=false;name_result=(risc_text_entry_state_v1){0};
    }
#ifdef ALARM_NATIVE_UTC
    paper_input_ready=false;pc_paper_page_reset();
#endif
    if(next==PC_TIME||next==PC_DURATION||next==PC_MODE||next==PC_DAYS||next==PC_COLOR){subpage_event=editor.event;subpage_type=editor.type;}page=next;first=focus=0;delete_confirm=false;touch_down=touch_moved=false;dirty=clean_frame=true;}
static const points_catalog_type *pc_type(uint32_t id){return points_catalog_find_type(&editor.store.saved,id);}
static const char *pc_name(uint32_t id){const points_catalog_type *t=pc_type(id);return t?t->name:"Unknown type";}
static uint32_t pc_color(uint32_t id){const points_catalog_type *t=pc_type(id);return t?t->color:NOVA_CYAN;}
static bool pc_duration(void){const points_catalog_type *t=pc_type(editor.event.type_id);return t&&(t->flags&POINTS_TYPE_DURATION);}
static void pc_saved(int32_t rc,unsigned action) {
    if(!pc_live())return;
    if(rc==RISC_APP_DATA_OK) {
        if(action==POINTS_EDITOR_TYPE) {
            if(editor.editing_event)(void)points_catalog_apply_type(&editor.event,&editor.store.saved,editor.type.id);
            pc_page(editor.editing_event?PC_EDIT:PC_TYPES);notice="Type saved";
        } else if(action==POINTS_EDITOR_DELETE_TYPE){pc_page(PC_TYPES);notice="Type deleted";}
        else {pc_page(PC_LIST);notice="Point saved";}
        (void)pc_service_result(service->refresh(service->context));
    } else if(editor.store.uncertain)notice="Check reads storage; it never rewrites";
    else if(rc==RISC_APP_DATA_NO_SPACE)notice="Storage full: free space, then Save";
    else if(rc==RISC_APP_DATA_STALE)notice="Catalog changed; check before saving";
    else if(rc==RISC_APP_DATA_UNAVAILABLE)notice="Type is used by saved points";
    else if(rc==RISC_APP_DATA_INVALID)notice="Check name, days and duration";
    else notice="Save failed: draft kept";
    dirty=true;
}
static void pc_save(bool remove) {
    if(!pc_live()||!points_editor_editable(&editor))return;
    pc_status();if(!pc_live())return;
    if(!service_valid||alarm_state.output_uncertain||alarm_state.occurrence.generation||alarm_state.state==ALARM_STATE_BLOCKED||alarm_state.state==ALARM_STATE_CUE||alarm_state.state==ALARM_STATE_ALERT||alarm_state.state==ALARM_STATE_DISMISSING){notice="Alarm service busy: retry";dirty=true;return;}
    if(page==PC_CUSTOM) {
        size_t n=strlen(editor.type.name);while(n&&editor.type.name[n-1]==' ')editor.type.name[--n]=0;
        pc_saved(points_editor_save_type(&editor,&files,remove),remove?POINTS_EDITOR_DELETE_TYPE:POINTS_EDITOR_TYPE);return;
    }
    if(editor.event.enabled&&!editor.event.weekdays&&!remove){notice="Choose at least one weekday";pc_page(PC_DAYS);return;}
    uint32_t now;clock_valid=pc_clock(&now);if(!pc_live())return;
    if(!clock_valid){notice="Time unavailable: draft kept";dirty=true;return;}
    if(!service_valid||alarm_state.state==ALARM_STATE_BLOCKED){notice="Alarm service unavailable: retry";dirty=true;return;}
    if(alarm_state.output_uncertain||alarm_state.occurrence.generation||alarm_state.state==ALARM_STATE_CUE||alarm_state.state==ALARM_STATE_ALERT||alarm_state.state==ALARM_STATE_DISMISSING){notice="Finish the alert before saving";dirty=true;return;}
    pc_saved(points_editor_save_event(&editor,&files,now,remove),remove?POINTS_EDITOR_DELETE:POINTS_EDITOR_EVENT);
}
static void pc_retry(void) {
    if(!pc_live())return;
    if(!ready){pc_close();if(!pc_live())return;ready=pc_open();if(ready)pc_load();dirty=true;return;}
    if(editor.store.uncertain){unsigned action=editor.pending_action;pc_saved(points_editor_resolve(&editor,&files),action);return;}
    if(editor.conflict){int32_t rc=points_editor_load(&editor,&files,&legacy,PC_DOMAIN);notice=rc==0?"Catalog reloaded; review then Save":"Reload failed: retry";dirty=true;return;}
    if(editor.store.loaded&&!editor.index_ready){(void)points_editor_reindex(&editor);dirty=true;return;}
    if(!editor.store.loaded)pc_load();
    else {uint32_t now;clock_valid=pc_clock(&now);if(pc_live())pc_status();}
    dirty=true;
}
#if defined(PORTABLE_RESIDENT_SHELL_CLIENT) || defined(PORTABLE_TEXT_INPUT_CLIENT)
__attribute__((visibility("hidden"))) bool portable_app_before_launch(const char *destination) {
    (void)destination;
    if(!pc_live()||name_client.active||name_client.suspended||name_client.acquired)return false;
#ifdef PORTABLE_RESIDENT_SHELL_CLIENT
    if(editor.store.uncertain){notice="Check save before leaving";dirty=true;return false;}
    if(editor.editing_event||editor.editing_type){notice="Save or cancel the draft first";dirty=true;return false;}
#endif
    return true;
}
#endif
#ifdef PORTABLE_APP_HOME_GUARD
/* Physical Home alone discards local drafts on the normal clean close path.
 * General launches and shared controls retain their existing edit guard. */
__attribute__((visibility("hidden"))) bool portable_app_before_home(void) {
    return pc_live()&&!name_client.active&&!name_client.suspended&&!name_client.acquired;
}
#endif
static bool pc_back(void) {
    if(!pc_live()||name_client.active)return false;
    if(editor.store.uncertain){notice="Check save before leaving";dirty=true;return false;}
    if(page==PC_LIST) {
#ifdef POINTS_RETURN_APP
        if(!runtime||!runtime->request_launch||!runtime->request_launch(POINTS_RETURN_APP)){notice="Return unavailable: retry";dirty=true;return false;}
        if(!pc_live())return false;
#endif
        return true;
    }
    if(page==PC_EDIT){points_editor_cancel_event(&editor);pc_page(PC_LIST);notice="Draft discarded";}
    else if(page==PC_NAME){if(name_return_page==PC_TYPES){points_editor_cancel_type(&editor);pc_page(PC_TYPES);notice="Type draft discarded";}else{pc_page(PC_CUSTOM);notice="Name draft discarded";}}
    else if(page==PC_CUSTOM){points_editor_cancel_type(&editor);pc_page(PC_TYPES);notice="Type draft discarded";}
    else if(page==PC_TYPES)pc_page(editor.editing_event?PC_EDIT:PC_LIST);
    else {if(page==PC_TIME||page==PC_DURATION||page==PC_MODE||page==PC_DAYS||page==PC_COLOR){editor.event=subpage_event;editor.type=subpage_type;}pc_page(return_page==PC_CUSTOM?PC_CUSTOM:PC_EDIT);}
    return false;
}
#include "points_catalog_view.inc"
void app_main(void) {
    if(!pc_live())return;
    app=t5_app_get_api(1);
    if(!app||app->abi_version!=1||app->struct_size<offsetof(t5_app_api_v1,draw_label)+sizeof(app->draw_label)||!app->poll||!app->millis||!app->screen_width||!app->screen_height||!app->fill_rect||!app->present)return;
    points_editor_dispose(&editor);acquired=0;ready=clock_valid=service_valid=false;notice="";time_format=PORTABLE_TIME_FORMAT_12;
    name_client=(portable_text_client){0};name_result=(risc_text_entry_state_v1){0};name_finished=false;
    page=PC_LIST;first=focus=return_page=0;dirty=clean_frame=true;exit_app=delete_confirm=touch_down=touch_moved=false;
#ifdef ALARM_NATIVE_UTC
    clock_client=(portable_realtime_client){0};zone[0]=0;
#endif
    memset(grants,0,sizeof(grants));memset(name_draft,0,sizeof(name_draft));
    paper=paper_presentation_get();nova=paper?NULL:springboard_presentation_get();
    if(paper&&(paper->struct_size<sizeof(*paper)||!paper->begin||!paper->text||!paper->measure||!paper->contact))return;
    /* This selected paper layout is portrait. Reject other geometries before
     * unsigned row/key calculations or any raster callback. */
    if(paper&&(app->screen_width()!=480||app->screen_height()!=800))return;
    if(!paper&&(!nova||!nova->contact||app->screen_width()!=240||app->screen_height()!=240))return;
    if(app->set_back_exits_app)app->set_back_exits_app(false);
    ready=pc_open();if(!pc_live())return;if(ready)pc_load();if(!pc_live())return;
    pc_draw();if(!pc_live())return;
    uint32_t last=app->millis();
    for(;;) {
        if(name_client.active){pc_name_step();if(!pc_live())return;if(exit_app)break;if(name_client.active){runtime->yield_ms(1);continue;}continue;}
        t5_app_input_t input={0};bool ok=app->poll(&input,20);if(!pc_live())return;
        if(!ok)break;
        if(input.exit_requested&&!(input.buttons&T5_APP_BUTTON_BACK))break;
        if((uint32_t)(app->millis()-last)>=500) {
            alarm_status_v1 old=alarm_state;bool valid=service_valid;
            uint32_t next_id=catalog_projection.count?catalog_projection.next[0].event_id:0;
            if(ready)pc_status();
            if(!pc_live())return;
            if(valid!=service_valid||old.state!=alarm_state.state||old.error!=alarm_state.error||next_id!=(catalog_projection.count?catalog_projection.next[0].event_id:0))dirty=true;
            last=app->millis();
        }
        pc_input(&input);if(!pc_live())return;
        if(exit_app)break;
        if(dirty)pc_draw();
        if(!pc_live())return;
    }
    if(paper&&!paper_frame_drain()){if(pc_live())pc_retain();return;}
    pc_close();
}
