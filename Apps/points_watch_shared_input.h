#pragma once
/* Watch-only modal input client. The separately loaded shared host owns the
 * keyboard and ordered touch capture. DONE changes only the custom-type draft;
 * the existing custom Save remains the sole metadata commit operation. */
#ifdef PORTABLE_TEXT_INPUT_POLICY
static void p7_key_policy(void) {
    if(!points_text_checkpoint.pausing&&!points_text_checkpoint.paused) {
        risc_text_entry_checkpoint_state_v1 snapshot={.struct_size=sizeof(snapshot)};
        int rc=portable_text_checkpoint_snapshot(&points_text_client,&points_text_checkpoint,&snapshot);
        if(rc!=RISC_TEXT_ENTRY_OK||points_text_client.retained)return;
        rc=portable_text_adapter_policy_prepare(snapshot.flags,&points_text_policy_continuity);
        if(rc<0){(void)portable_text_client_retain(&points_text_client);return;}
        if(!rc)return;
    }
    int rc=portable_text_checkpoint_pause_mode(&points_text_client,&points_text_checkpoint,points_text_policy_continuity);
    if(rc!=RISC_TEXT_ENTRY_OK||points_text_client.retained)return;
    uint32_t flags=0;int policy=portable_text_adapter_policy_apply(points_text_policy_continuity,&flags);
    if(policy<0){(void)portable_text_client_retain(&points_text_client);return;}
    if(points_text_client.retained)return;
    if(policy>0){points_text_state.state=RISC_TEXT_ENTRY_CANCELLED;points_text_closing=true;return;}
    (void)portable_text_checkpoint_resume(&points_text_client,&points_text_checkpoint,flags);
}
#endif
static void p7_key_step(void) {
    int32_t rc;
#ifdef PORTABLE_TEXT_INPUT_POLICY
    if(points_text_client.retained)return;
    if(portable_text_adapter_policy_capture()<0){(void)portable_text_client_retain(&points_text_client);return;}
    if(!points_text_closing&&(points_text_checkpoint.pausing||points_text_checkpoint.paused)) {
        int attention=portable_text_adapter_attention();
        if(attention<0){(void)portable_text_client_retain(&points_text_client);return;}
        if(!attention){p7_key_policy();return;}
        points_text_state.state=RISC_TEXT_ENTRY_CANCELLED;points_text_closing=true;
    }
#endif
    if(!points_text_closing) {
        rc=portable_text_client_poll(&points_text_client,&points_text_state);
#ifdef PORTABLE_TEXT_INPUT_POLICY
        if(points_text_client.retained)return;
        if(portable_text_adapter_policy_capture()<0){(void)portable_text_client_retain(&points_text_client);return;}
#endif
        if(rc==RISC_TEXT_ENTRY_AGAIN)return;
        if(rc==RISC_TEXT_ENTRY_RETAINED||points_text_client.retained)return;
        if(rc!=RISC_TEXT_ENTRY_OK){points_text_state.state=RISC_TEXT_ENTRY_CANCELLED;notice="Text input unavailable";}
#ifdef PORTABLE_TEXT_INPUT_POLICY
        else if(points_text_state.state==RISC_TEXT_ENTRY_EDITING){p7_key_policy();return;}
#else
        else if(points_text_state.state==RISC_TEXT_ENTRY_EDITING)return;
#endif
        points_text_closing=true;
    }
    rc=portable_text_client_close(&points_text_client);
    if(rc==RISC_TEXT_ENTRY_AGAIN)return;
    if(rc!=RISC_TEXT_ENTRY_OK||points_text_client.retained)return;
    points_text_closing=false;page=PAGE_CUSTOM;custom_key_choice=0;
    points_text_home=(points_text_state.flags&RISC_TEXT_ENTRY_HOME_CANCEL)!=0;
#ifdef PORTABLE_TEXT_INPUT_POLICY
    /* A paused Runtime exit is not nested Back. Do not redraw or replace its
     * pending destination with the app's root-Back springboard request. */
    points_text_home|=risc_runtime_exit_requested_v1(runtime);
    if(!points_live())return;
#endif
    if(points_text_state.state==RISC_TEXT_ENTRY_ACCEPTED) {
        unsigned ci=custom_index(custom_kind);
        if(ci<POINTS_CUSTOM_COUNT){
            memcpy(custom_draft.custom[ci].name,points_text_state.text,POINTS_CUSTOM_NAME_MAX+1);
            notice="Name draft updated";
        }
    }else notice="Name edit discarded";
    memset(&points_text_state,0,sizeof(points_text_state));
}
static inline void p7_key_begin(void) {
    unsigned ci=custom_index(custom_kind);if(ci>=POINTS_CUSTOM_COUNT||!points_live())return;
    points_text_state=(risc_text_entry_state_v1){.struct_size=sizeof(points_text_state)};
    points_text_closing=false;
#ifdef PORTABLE_TEXT_INPUT_POLICY
    points_text_checkpoint=(portable_text_checkpoint_client){0};points_text_policy_continuity=false;
#endif
    int32_t rc=portable_text_client_begin(&points_text_client,runtime,"Custom type",
                                        custom_draft.custom[ci].name,POINTS_CUSTOM_NAME_MAX+1);
    if(rc==RISC_TEXT_ENTRY_RETAINED||points_text_client.retained)return;
    if(rc!=RISC_TEXT_ENTRY_OK){notice="Text input unavailable";return;}
    page=PAGE_CUSTOM_KEYBOARD;
#ifdef PORTABLE_TEXT_INPUT_POLICY
    rc=portable_text_checkpoint_enable(&points_text_client,&points_text_checkpoint);
    if(rc!=RISC_TEXT_ENTRY_OK&&!points_text_client.retained){points_text_closing=true;points_text_state.state=RISC_TEXT_ENTRY_CANCELLED;notice="Text input unavailable";}
#endif
}
static inline void p7_key_cancel(void) {
    if(page!=PAGE_CUSTOM_KEYBOARD)return;
    points_text_state.state=RISC_TEXT_ENTRY_CANCELLED;points_text_closing=true;p7_key_step();
}
