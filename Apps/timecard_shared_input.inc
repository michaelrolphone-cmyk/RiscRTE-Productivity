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
static void tcp_text_step(void) {
    if(!tcp_text_active || tcp_retained())return;
    if(!tcp_text_closing) {
        int rc=portable_text_client_poll(&tcp_text_client,&tcp_text_state);
        if(rc==RISC_TEXT_ENTRY_AGAIN || rc==RISC_TEXT_ENTRY_RETAINED || tcp_retained())return;
        if(rc!=RISC_TEXT_ENTRY_OK)tcp_text_state.state=RISC_TEXT_ENTRY_CANCELLED;
        else if(tcp_text_state.state==RISC_TEXT_ENTRY_EDITING)return;
        tcp_text_closing=true;
    }
    int rc=portable_text_client_close(&tcp_text_client);
    if(rc!=RISC_TEXT_ENTRY_OK || tcp_retained())return;
    tcp_text_active=tcp_text_closing=false;
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
