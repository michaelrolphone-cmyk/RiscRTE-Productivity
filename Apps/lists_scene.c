#include "ListsScene.h"
#include "../lib/Lists/ListsStore.h"
#include "RiscRuntimeV1.h"
#include "RiscCivilClockV1.h"
#include "RiscSceneResidentV1.h"
#include <string.h>
static const risc_runtime_api_v1 *runtime;
static const risc_scene_api_v1 *scene;
static const risc_scene_components_api_v1 *components;
static const risc_civil_clock_api_v1 *civil;
static risc_runtime_capability_v1 grants[3];
static unsigned acquired;
static uint64_t session;
static bool terminal;
static risc_scene_resident_v1 resident;
__attribute__((visibility("default"))) const risc_resident_app_descriptor_v1_t risc_resident_app_descriptor_v1={1,sizeof(risc_resident_app_descriptor_v1_t),RISC_RESIDENT_ROLE_FOREGROUND,0};
static lists_ui ui;
static lists_store files;
static lists_model previous,candidate;
static void retain(void){if(!terminal){terminal=true;if(risc_runtime_get_api(1)&&runtime->retain_invocation)(void)runtime->retain_invocation();}}
static bool alive(void *ctx){(void)ctx;if(terminal)return false;if(!risc_runtime_get_api(1)){terminal=true;return false;}return true;}
static bool checked(int rc){if(rc==-9||!alive(NULL)){retain();return false;}return true;}
static bool load(void){int rc=lists_store_load(&files,&ui.data);if(!checked(rc))return false;ui.readonly=rc!=LISTS_STORE_OK;if(ui.readonly)lists_ui_notice(&ui,"SAVED LISTS UNAVAILABLE");else ui.page=LISTS_HOME;return true;}
static bool sample_clock(void){
    risc_civil_time_v1 time={.struct_size=sizeof(time)};int rc=civil->read(civil->context,&time);if(!checked(rc))return false;
    lists_ui_clock(&ui,rc==0?time.day:-1,rc==0?time.minute:0);return true;
}
static void release_grants(void){while(acquired&&!terminal){bool ok=runtime->release(&grants[--acquired]);if(!alive(NULL))return;if(!ok){retain();return;}}}
static bool dependencies(void){
    const char *names[]={RISC_SCENE_CAPABILITY,RISC_APP_DATA_CAPABILITY,RISC_CIVIL_CLOCK_CAPABILITY};
    for(unsigned i=0;i<3;i++){grants[i]=(risc_runtime_capability_v1){.struct_size=sizeof(grants[i])};bool ok=runtime->acquire(names[i],1,0,&grants[i]);if(!alive(NULL))return false;if(!ok)return false;++acquired;}
    scene=grants[0].api;components=risc_scene_components_get_v1(scene);files.api=grants[1].api;files.alive=alive;civil=grants[2].api;
    return components&&scene->next&&scene->close&&scene->snapshot&&files.api&&files.api->api_version==1&&files.api->struct_size>=sizeof(*files.api)&&files.api->stat&&files.api->read&&files.api->replace&&civil&&civil->api_version==1&&civil->struct_size>=sizeof(*civil)&&civil->read;
}
static bool close_scene(void){
    if(!alive(NULL))return false;
    if(!session)return true;
    for(unsigned i=0;i<1000;i++){
        int rc=scene->close(scene->context,session);if(!checked(rc))return false;
        if(rc==RISC_SCENE_OK){session=0;return true;}
        if(rc!=RISC_SCENE_AGAIN){retain();return false;}
        runtime->yield_ms(5);if(!alive(NULL))return false;
    }retain();return false;
}
static bool open_scene(const risc_scene_navigation_v1 *navigation){
    int rc=components->open(scene->context,&ui.document,navigation,&session);if(!checked(rc)||rc)return false;
    ui.last_event=0;
    const risc_scene_lifecycle_api_v1 *lifecycle=risc_scene_lifecycle_get_v1(scene);
    if(lifecycle){rc=lifecycle->configure(scene->context,session,resident.enabled?RISC_SCENE_FEATURE_SHARED_CONTROLS:0);if(!checked(rc)||rc)return false;}
    return true;
}
static bool shell_ui(unsigned reason){
    risc_scene_navigation_v1 navigation={.struct_size=sizeof(navigation)};uint32_t flags;
    int rc=scene->snapshot(scene->context,session,&navigation,&flags);if(!checked(rc)||rc)return false;
    if(!close_scene())return false;
    rc=risc_scene_resident_dispatch_v1(&resident,reason);
    if(rc==RISC_RESIDENT_RETAINED||!alive(NULL)){retain();return false;}
    if(rc!=RISC_RESIDENT_OK&&rc!=RISC_RESIDENT_BUSY)return false;
    return open_scene(&navigation);
}
__attribute__((visibility("default"))) void app_main(void){
    runtime=risc_runtime_get_api(1);if(!runtime||runtime->struct_size<RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE||!runtime->retain_invocation||!runtime->acquire||!runtime->release||!runtime->yield_ms)return;
    terminal=false;acquired=0;session=0;files=(lists_store){0};lists_ui_init(&ui);
    if(!dependencies()){if(!terminal&&runtime->diagnostic){runtime->diagnostic("Lists requires NOVA scene components, app-data and civil time");if(!alive(NULL))return;}release_grants();return;}
    if(!risc_scene_resident_bind_v1(runtime,&resident)){if(!alive(NULL))return;retain();return;}
    if(!load()||!sample_clock()){if(!terminal)release_grants();return;}lists_ui_declare(&ui);
    if(!open_scene(NULL)){if(!terminal&&close_scene())release_grants();return;}
    int rc;unsigned poll_ticks=0;
    unsigned ticks=0;
    for(;;){
        risc_scene_event_v1 event={.struct_size=sizeof(event)};rc=scene->next(scene->context,session,&event);if(!checked(rc))return;if(rc!=RISC_SCENE_OK&&rc!=RISC_SCENE_IDLE)break;
        bool redraw=false;
        if(rc==RISC_SCENE_OK){
            if(event.kind==RISC_SCENE_CONTROLS_EVENT&&resident.enabled&&event.document_revision==ui.document.revision){if(!shell_ui(RISC_RESIDENT_CHECKPOINT_CONTROLS))break;continue;}
            if(event.kind==RISC_SCENE_SUSPEND_EVENT&&event.document_revision==ui.document.revision&&event.sequence>ui.last_event)break;
            if(ui.readonly&&event.kind==RISC_SCENE_ACTION_EVENT&&event.action==LA_RETRY&&event.node&&event.node<=ui.document.node_count&&ui.document.nodes[event.node-1].action==LA_RETRY&&event.document_revision==ui.document.revision&&event.sequence>ui.last_event){ui.last_event=event.sequence;if(!load())return;}
            else {
                previous=ui.data;
                bool changed=lists_ui_event(&ui,&event);
                if(changed){candidate=ui.data;ui.data=previous;int saved=lists_store_save(&files,&ui.data,&candidate);if(!checked(saved))return;if(saved!=LISTS_STORE_OK){ui.undo_valid=false;ui.page=LISTS_HOME;ui.readonly=!files.ready;lists_ui_notice(&ui,saved==LISTS_STORE_CHANGED?"SAVE NOT CONFIRMED - RELOADED":"SAVE FAILED - RETRY STORAGE");}}
            }
            redraw=true;
        }
        if(++ticks>=250){ticks=0;unsigned old_page=ui.page,old_minute=ui.minute;int32_t old_day=ui.today;if(!sample_clock())return;redraw=redraw||old_page!=ui.page||old_minute!=ui.minute||old_day!=ui.today;}
        if(redraw){lists_ui_declare(&ui);rc=components->update(scene->context,session,&ui.document);if(!checked(rc))return;if(rc)break;}
        if(resident.enabled){
            risc_scene_navigation_v1 navigation={.struct_size=sizeof(navigation)};uint32_t flags=0;
            rc=scene->snapshot(scene->context,session,&navigation,&flags);if(!checked(rc))return;if(rc)break;
            if(flags&RISC_SCENE_ACTIVITY)resident.activity=true;
            if(++poll_ticks>=50){poll_ticks=0;int status=risc_scene_resident_poll_v1(&resident,flags);
                if(status==RISC_RESIDENT_RETAINED||!alive(NULL)){retain();return;}
                if(status==RISC_RESIDENT_EXIT)break;
                if(status!=RISC_RESIDENT_OK&&status!=RISC_RESIDENT_BUSY)break;
                if(resident.policy&&!(flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY))&&!shell_ui(RISC_RESIDENT_CHECKPOINT_POLICY))break;
            }
        }
        runtime->yield_ms(5);if(!alive(NULL))return;
    }
    if(close_scene())release_grants();
}
