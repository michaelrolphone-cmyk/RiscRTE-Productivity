#pragma once
/* Watch-only modal input client. The separately loaded shared host owns the
 * keyboard and ordered touch capture. DONE changes only the custom-type draft;
 * the existing custom Save remains the sole metadata commit operation. */
static void p7_key_step(void) {
    int32_t rc;
    if(!points_text_closing) {
        rc=portable_text_client_poll(&points_text_client,&points_text_state);
        if(rc==RISC_TEXT_ENTRY_AGAIN)return;
        if(rc==RISC_TEXT_ENTRY_RETAINED||points_text_client.retained)return;
        if(rc!=RISC_TEXT_ENTRY_OK){points_text_state.state=RISC_TEXT_ENTRY_CANCELLED;notice="Text input unavailable";}
        else if(points_text_state.state==RISC_TEXT_ENTRY_EDITING)return;
        points_text_closing=true;
    }
    rc=portable_text_client_close(&points_text_client);
    if(rc==RISC_TEXT_ENTRY_AGAIN)return;
    if(rc!=RISC_TEXT_ENTRY_OK||points_text_client.retained)return;
    points_text_closing=false;page=PAGE_CUSTOM;custom_key_choice=0;
    points_text_home=(points_text_state.flags&RISC_TEXT_ENTRY_HOME_CANCEL)!=0;
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
    int32_t rc=portable_text_client_begin(&points_text_client,runtime,"Custom type",
                                        custom_draft.custom[ci].name,POINTS_CUSTOM_NAME_MAX+1);
    if(rc==RISC_TEXT_ENTRY_RETAINED||points_text_client.retained)return;
    if(rc!=RISC_TEXT_ENTRY_OK){notice="Text input unavailable";return;}
    page=PAGE_CUSTOM_KEYBOARD;
}
static inline void p7_key_cancel(void) {
    if(page!=PAGE_CUSTOM_KEYBOARD)return;
    points_text_state.state=RISC_TEXT_ENTRY_CANCELLED;points_text_closing=true;p7_key_step();
}
