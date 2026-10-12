/* Lists owns the scene/reducer/store; the existing service owns every key and
 * draft. No app callback or buffer crosses the shared input ABI. */
#include "PortableTextInputResidentCheckpoint.h"
#include "RiscRuntimeHostControlV1.h"
static portable_text_client text_client;
static portable_text_checkpoint_client text_checkpoint;
static unsigned text_policy_ticks;
static bool text_policy_refresh,text_enable_failed,text_policy_continuity;
static risc_scene_navigation_v1 text_navigation;
static char text_accepted[LISTS_NAME];
static unsigned text_outcome;
static bool text_exit;
static bool lists_text_busy(void){return text_client.active||text_client.acquired||text_client.suspended;}
static void lists_text_reset(void){
    text_client=(portable_text_client){0};text_checkpoint=(portable_text_checkpoint_client){0};
    text_policy_ticks=0;text_policy_refresh=text_enable_failed=text_policy_continuity=false;
    text_navigation=(risc_scene_navigation_v1){.struct_size=sizeof(text_navigation)};
    memset(text_accepted,0,sizeof(text_accepted));text_outcome=0;text_exit=false;
}
static bool lists_host_exit(void){
    if(!alive(NULL))return true;
    bool requested=risc_runtime_exit_requested_v1(runtime);
    if(!alive(NULL))return true;
    if(requested)text_exit=true;
    return text_exit;
}
bool portable_text_adapter_retained(void){return terminal;}
void portable_text_adapter_retain(void){retain();}
int portable_text_adapter_attention(void){
    bool exiting=lists_host_exit();return alive(NULL)?(exiting?1:0):-1;
}
int portable_text_adapter_suspend(void){
    if(!alive(NULL))return RISC_TEXT_ENTRY_RETAINED;
    if(!session){retain();return RISC_TEXT_ENTRY_RETAINED;}
    text_navigation=(risc_scene_navigation_v1){.struct_size=sizeof(text_navigation)};uint32_t flags=0;
    int rc=scene->snapshot(scene->context,session,&text_navigation,&flags);
    if(!checked(rc))return RISC_TEXT_ENTRY_RETAINED;
    if(rc){retain();return RISC_TEXT_ENTRY_RETAINED;}
    if(flags&RISC_SCENE_ACTIVITY)resident.activity=true;
    return close_scene()?RISC_TEXT_ENTRY_OK:RISC_TEXT_ENTRY_RETAINED;
}
int portable_text_adapter_resume(void){
    bool exiting=lists_host_exit();
    if(!alive(NULL))return RISC_TEXT_ENTRY_RETAINED;
    /* Home cancels before rebuilding the scene, including a close/release race. */
    if(exiting)return RISC_TEXT_ENTRY_OK;
    lists_ui_declare(&ui);
    if(!open_scene(&text_navigation)){retain();return RISC_TEXT_ENTRY_RETAINED;}
    return RISC_TEXT_ENTRY_OK;
}
/* Keep the existing resident power-policy cadence while a draft is open.
 * The host retains editor state/passive input queues but relinquishes mutable/submitted
 * display custody before POLICY. A BUSY pause forces activity through a fresh POLL first. */
static bool lists_text_policy(void){
    if(!resident.enabled)return true;
    if(lists_host_exit())return alive(NULL);
    if(!text_checkpoint.pausing&&!text_checkpoint.paused){
        risc_text_entry_checkpoint_state_v1 snapshot={.struct_size=sizeof(snapshot)};
        int rc=portable_text_checkpoint_snapshot(&text_client,&text_checkpoint,&snapshot);
        if(rc==RISC_TEXT_ENTRY_BUSY)return true;
        if(rc!=RISC_TEXT_ENTRY_OK||!alive(NULL))return false;
        uint32_t flags=0;
        if(snapshot.flags&RISC_TEXT_ENTRY_CHECKPOINT_PRESENTING)flags|=RISC_SCENE_PRESENTING;
        if(snapshot.flags&RISC_TEXT_ENTRY_CHECKPOINT_INPUT_BUSY)flags|=RISC_SCENE_INPUT_BUSY;
        if(snapshot.flags&RISC_TEXT_ENTRY_CHECKPOINT_ACTIVITY){flags|=RISC_SCENE_ACTIVITY;resident.activity=true;}
        if(++text_policy_ticks>=50||text_policy_refresh){
            text_policy_ticks=0;
            int status=portable_text_resident_poll(&resident,flags,0,&text_policy_continuity);
            if(status==RISC_RESIDENT_RETAINED||!alive(NULL)){retain();return false;}
            if(lists_host_exit())return alive(NULL);
            if(status==RISC_RESIDENT_EXIT){text_exit=true;return true;}
            if(status!=RISC_RESIDENT_OK&&status!=RISC_RESIDENT_BUSY){retain();return false;}
            if(status==RISC_RESIDENT_OK)text_policy_refresh=false;
        }
        if(!resident.policy||text_policy_refresh||(flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY)))return true;
    }
    int rc=portable_text_checkpoint_pause_mode(&text_client,&text_checkpoint,text_policy_continuity);
    if(rc==RISC_TEXT_ENTRY_BUSY){resident.policy=false;text_policy_continuity=false;text_policy_refresh=true;return true;}
    if(rc==RISC_TEXT_ENTRY_AGAIN)return true;
    if(rc!=RISC_TEXT_ENTRY_OK||!alive(NULL))return false;
    if(lists_host_exit())return alive(NULL);
    uint32_t resume_flags=0;
    int status=portable_text_resident_policy(&resident,text_policy_continuity,&resume_flags);
    if(status==RISC_RESIDENT_RETAINED||!alive(NULL)){retain();return false;}
    if(lists_host_exit())return alive(NULL);
    if(status==RISC_RESIDENT_EXIT){text_exit=true;return true;}
    if(status!=RISC_RESIDENT_OK&&status!=RISC_RESIDENT_BUSY){retain();return false;}
    /* The selected predecessor reopened after every checkpoint; always redraw
     * rather than assuming old pixels survived a sleep or policy screen. */
    rc=portable_text_checkpoint_resume(&text_client,&text_checkpoint,resume_flags);
    if(rc!=RISC_TEXT_ENTRY_OK||!alive(NULL))return false;
    (void)lists_host_exit();return alive(NULL);
}
static bool lists_text_step(void){
    if(!alive(NULL))return false;
    if(!lists_text_busy()){
        memset(text_accepted,0,sizeof(text_accepted));text_outcome=0;
        text_checkpoint=(portable_text_checkpoint_client){0};text_policy_ticks=0;text_policy_refresh=text_enable_failed=text_policy_continuity=false;
        int rc=portable_text_client_begin(&text_client,runtime,"NAME",ui.draft,LISTS_NAME);
        if(rc==RISC_TEXT_ENTRY_RETAINED||!alive(NULL))return false;
        if(rc!=RISC_TEXT_ENTRY_OK){ui.text_retry=true;lists_ui_notice(&ui,"TEXT INPUT UNAVAILABLE / RETRY");}
        else {
            rc=portable_text_checkpoint_enable(&text_client,&text_checkpoint);
            if(rc==RISC_TEXT_ENTRY_RETAINED||!alive(NULL))return false;
            if(rc!=RISC_TEXT_ENTRY_OK){text_enable_failed=true;text_outcome=RISC_TEXT_ENTRY_CANCELLED;}
        }
        return true;
    }
    if(text_checkpoint.pausing||text_checkpoint.paused)return lists_text_policy();
    if(!text_client.closing&&!text_enable_failed){
        risc_text_entry_state_v1 state={.struct_size=sizeof(state)};
        int rc=portable_text_client_poll(&text_client,&state);
        if(rc==RISC_TEXT_ENTRY_AGAIN)return true;
        if(rc!=RISC_TEXT_ENTRY_OK||!alive(NULL))return false;
        if(state.state==RISC_TEXT_ENTRY_EDITING){
            (void)portable_text_client_state_result(&state,0);
            return lists_text_policy();
        }
        resident.activity=true;text_outcome=state.state;
        if(state.flags&RISC_TEXT_ENTRY_HOME_CANCEL)text_exit=true;
        if(state.state==RISC_TEXT_ENTRY_ACCEPTED&&!text_exit)memcpy(text_accepted,state.text,strlen(state.text)+1);
        (void)portable_text_client_state_result(&state,0);
    }
    int rc=portable_text_client_close(&text_client);
    if(rc==RISC_TEXT_ENTRY_AGAIN)return true;
    if(rc!=RISC_TEXT_ENTRY_OK||!alive(NULL))return false;
    bool exiting=lists_host_exit();
    if(!alive(NULL))return false;
    text_checkpoint=(portable_text_checkpoint_client){0};
    if(!exiting){
        if(text_enable_failed){ui.text_retry=true;lists_ui_notice(&ui,"TEXT INPUT UNAVAILABLE / RETRY");}
        else if(text_outcome==RISC_TEXT_ENTRY_ACCEPTED){
            previous=ui.data;
            bool changed=lists_ui_text_accept(&ui,text_accepted);
            if(changed){
                candidate=ui.data;ui.data=previous;
                int saved=lists_store_save(&files,&ui.data,&candidate);
                if(!checked(saved))return false;
                if(saved!=LISTS_STORE_OK){ui.undo_valid=false;ui.page=LISTS_HOME;ui.readonly=!files.ready;lists_ui_notice(&ui,saved==LISTS_STORE_CHANGED?"SAVE NOT CONFIRMED - RELOADED":"SAVE FAILED - RETRY STORAGE");}
            }
        }else lists_ui_text_cancel(&ui);
    }
    memset(text_accepted,0,sizeof(text_accepted));text_outcome=0;text_enable_failed=false;
    return alive(NULL);
}
static bool lists_text_cleanup(void){
    text_exit=true;memset(text_accepted,0,sizeof(text_accepted));text_outcome=0;
    lists_ui_text_cancel(&ui);
    for(unsigned i=0;lists_text_busy()&&i<1000;i++){
        int rc=portable_text_client_close(&text_client);
        if(rc==RISC_TEXT_ENTRY_OK)return true;
        if(rc!=RISC_TEXT_ENTRY_AGAIN||!alive(NULL))return false;
        runtime->yield_ms(5);if(!alive(NULL))return false;
    }
    if(lists_text_busy()){retain();return false;}
    return alive(NULL);
}
