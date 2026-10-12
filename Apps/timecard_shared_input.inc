/* The host returns only a bounded draft. Saving remains the original Timecard
 * validation, reread and mutation operation, after close/release/resume. */
static void tcp_text_retry(void) {
    if(tcp_text_active || tcp_retained())return;
    if(!tcp_text_begin(tcp_editor_status)) {
        if(!tcp_retained())scopy(tcp_editor_status,sizeof(tcp_editor_status),"Text input unavailable; Retry");
        tcp_dirty=true;
    }
}
__attribute__((visibility("hidden"))) bool portable_app_before_home(void) {
    if(tcp_retained() || tcp_text_active || tcp_text_client.active ||
       tcp_text_client.acquired || tcp_text_client.suspended)return false;
    /* A clean COMMIT_UNKNOWN may already be durable. Home neither reloads nor
     * rewrites it; the next invocation reads the authoritative ledger. */
    tcp_editor_cancel();memset(&tcp_text_state,0,sizeof(tcp_text_state));
    return true;
}
static void tcp_text_home(void) {
    if(!portable_app_before_home())return;
#ifdef PORTABLE_HOME_APP
    const char *destination=PORTABLE_HOME_APP;
#else
    const char *destination=TIMECARD_RETURN_APP;
#endif
#ifdef PORTABLE_RESIDENT_SHELL_CLIENT
    /* Returning this child resumes the existing resident Home owner. */
    if(!strcmp(destination,"default.elf")){tcp_external_exit=true;return;}
#endif
    bool launched=tcp_runtime && tcp_runtime->request_launch && tcp_runtime->request_launch(destination);
    if(tcp_retained())return;
    if(launched)tcp_external_exit=true;
    else {set_status("Exit failed; Retry Home");tcp_dirty=true;}
}
#ifdef PORTABLE_TEXT_INPUT_POLICY
static void tcp_text_policy(void) {
    if(!tcp_text_checkpoint.pausing&&!tcp_text_checkpoint.paused){
        risc_text_entry_checkpoint_state_v1 snapshot={.struct_size=sizeof(snapshot)};
        int rc=portable_text_checkpoint_snapshot(&tcp_text_client,&tcp_text_checkpoint,&snapshot);
        if(rc==RISC_TEXT_ENTRY_BUSY)return;
        if(rc!=RISC_TEXT_ENTRY_OK||tcp_retained())return;
        rc=portable_text_adapter_policy_prepare(snapshot.flags,&tcp_text_policy_continuity);
        if(rc<0){(void)portable_text_client_retain(&tcp_text_client);return;}
        if(!rc)return;
    }
    int rc=portable_text_checkpoint_pause_mode(&tcp_text_client,&tcp_text_checkpoint,tcp_text_policy_continuity);
    if(rc==RISC_TEXT_ENTRY_BUSY||rc==RISC_TEXT_ENTRY_AGAIN)return;
    if(rc!=RISC_TEXT_ENTRY_OK||tcp_retained())return;
    uint32_t resume_flags=0;
    int policy=portable_text_adapter_policy_apply(tcp_text_policy_continuity,&resume_flags);
    if(policy<0){
        (void)portable_text_client_retain(&tcp_text_client);return;
    }
    if(tcp_retained())return;
    if(policy>0){tcp_text_state.state=RISC_TEXT_ENTRY_CANCELLED;tcp_text_closing=true;return;}
    (void)portable_text_checkpoint_resume(&tcp_text_client,&tcp_text_checkpoint,resume_flags);
}
#endif
static void tcp_text_step(void) {
    if(!tcp_text_active || tcp_retained())return;
#ifdef PORTABLE_TEXT_INPUT_POLICY
    if(portable_text_adapter_policy_capture()<0){(void)portable_text_client_retain(&tcp_text_client);return;}
    if(!tcp_text_closing&&(tcp_text_checkpoint.pausing||tcp_text_checkpoint.paused)){
        int attention=portable_text_adapter_attention();
        if(attention<0){(void)portable_text_client_retain(&tcp_text_client);return;}
        if(!attention){tcp_text_policy();return;}
        tcp_text_state.state=RISC_TEXT_ENTRY_CANCELLED;tcp_text_closing=true;
    }
#endif
    if(!tcp_text_closing) {
        int rc=portable_text_client_poll(&tcp_text_client,&tcp_text_state);
#ifdef PORTABLE_TEXT_INPUT_POLICY
        if(tcp_retained())return;
        if(portable_text_adapter_policy_capture()<0){(void)portable_text_client_retain(&tcp_text_client);return;}
#endif
        if(rc==RISC_TEXT_ENTRY_AGAIN || rc==RISC_TEXT_ENTRY_RETAINED || tcp_retained())return;
        if(rc!=RISC_TEXT_ENTRY_OK)tcp_text_state.state=RISC_TEXT_ENTRY_CANCELLED;
        else if(tcp_text_state.state==RISC_TEXT_ENTRY_EDITING){
#ifdef PORTABLE_TEXT_INPUT_POLICY
            tcp_text_policy();
#endif
            return;
        }
        tcp_text_closing=true;
    }
    int rc=portable_text_client_close(&tcp_text_client);
    if(rc!=RISC_TEXT_ENTRY_OK || tcp_retained())return;
    tcp_text_active=tcp_text_closing=false;
#ifdef PORTABLE_TEXT_INPUT_POLICY
    tcp_text_checkpoint=(portable_text_checkpoint_client){0};
    if(tcp_text_policy_unavailable){
        tcp_text_policy_unavailable=false;
        scopy(tcp_editor_status,sizeof(tcp_editor_status),"Text input unavailable; Retry");
        tcp_reset_gesture();tcp_dirty=true;return;
    }
#endif
    bool home=(tcp_text_state.flags&RISC_TEXT_ENTRY_HOME_CANCEL)!=0;
    bool accepted=tcp_text_state.state==RISC_TEXT_ENTRY_ACCEPTED;
    if(accepted)scopy(tcp_entry,sizeof(tcp_entry),tcp_text_state.text);
    memset(&tcp_text_state,0,sizeof(tcp_text_state));
    tcp_reset_gesture();tcp_dirty=true;
    if(home){tcp_text_home();return;}
    if(!accepted){tcp_editor_cancel();return;}
    tcp_editor_done();
    if(tcp_retained())return;
    /* Invalid time and clean save failures keep the complete copied draft.
     * Reopening displays the existing error without changing saved history. */
    if(tcp_editor)tcp_text_retry();
}
