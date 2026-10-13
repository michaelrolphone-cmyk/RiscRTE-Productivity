/* Deterministic provider boundary; the actual X4 app and adapter are linked. */
static unsigned policy_enables,policy_pauses,policy_resumes,policy_pending;
static uint32_t policy_snapshot_flags,policy_resume_flags;
static int policy_enable_result,policy_resume_result,policy_pause_result;
static bool policy_paused,policy_focus,policy_runtime_exit;
static int32_t policy_enable(void *c,uint64_t s){(void)c;io();assert(s==42&&text_opened);policy_enables++;return policy_enable_result;}
static int32_t policy_snapshot(void *c,uint64_t s,risc_text_entry_checkpoint_state_v1 *out){
 (void)c;io();assert(s==42&&text_opened);*out=(risc_text_entry_checkpoint_state_v1){sizeof(*out),policy_paused?RISC_TEXT_ENTRY_CHECKPOINT_PAUSED:policy_snapshot_flags};return 0;
}
static int32_t policy_pause(void *c,uint64_t s){
 (void)c;io();assert(s==42&&text_opened&&!live_frames&&!live_subs);policy_pauses++;
 if(policy_pause_result)return policy_pause_result;
 if(policy_pending){policy_pending--;return RISC_TEXT_ENTRY_AGAIN;}
 policy_paused=true;return 0;
}
static int32_t policy_pause_focus(void *c,uint64_t s){policy_focus=true;return policy_pause(c,s);}
static int32_t policy_resume(void *c,uint64_t s,uint32_t flags){
 (void)c;io();assert(s==42&&text_opened&&policy_paused);policy_resumes++;policy_resume_flags=flags;
 if(policy_resume_result)return policy_resume_result;
 policy_paused=false;return 0;
}
static int32_t unused_open_extended(void *c,const risc_text_entry_request_v1_extended *r,uint64_t *s){(void)c;(void)r;(void)s;assert(0);return -1;}
static int32_t unused_poll_extended(void *c,uint64_t s,risc_text_entry_state_v1_extended *r){(void)c;(void)s;(void)r;assert(0);return -1;}
static int32_t unused_open_long(void *c,const risc_text_entry_request_v1_long *r,uint64_t *s){(void)c;(void)r;(void)s;assert(0);return -1;}
static int32_t unused_poll_long(void *c,uint64_t s,risc_text_entry_state_v1_long *r){(void)c;(void)s;(void)r;assert(0);return -1;}
static risc_text_entry_api_v1_checkpoint text_api={
 .base={.base={.base={.base={.base={1,sizeof(text_api),NULL,text_open,text_poll,text_close},
  .home_reason_tag=RISC_TEXT_ENTRY_HOME_REASON_TAG,.home_reason_version=RISC_TEXT_ENTRY_HOME_REASON_VERSION},
  .masked_tag=RISC_TEXT_ENTRY_MASKED_TAG,.masked_version=RISC_TEXT_ENTRY_MASKED_VERSION},
  .extended_tag=RISC_TEXT_ENTRY_EXTENDED_TAG,.extended_version=RISC_TEXT_ENTRY_EXTENDED_VERSION,
  .open_extended=unused_open_extended,.poll_extended=unused_poll_extended},
  .long_tag=RISC_TEXT_ENTRY_LONG_TAG,.long_version=RISC_TEXT_ENTRY_LONG_VERSION,.open_long=unused_open_long,.poll_long=unused_poll_long},
 .checkpoint_tag=RISC_TEXT_ENTRY_CHECKPOINT_TAG,.checkpoint_version=RISC_TEXT_ENTRY_CHECKPOINT_VERSION,
 .checkpoint_enable=policy_enable,.checkpoint_snapshot=policy_snapshot,.pause=policy_pause,.resume=policy_resume,.pause_for_focus_change=policy_pause_focus};
