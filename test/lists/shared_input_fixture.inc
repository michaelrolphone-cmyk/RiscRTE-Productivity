/* Boundary faults wrap the shipping host. The success path always calls it. */
static const risc_driver_v2 *text_driver;
static const risc_text_entry_api_v1 *text_real;
static risc_text_entry_api_v1_checkpoint text_api;
static const risc_text_entry_api_v1_checkpoint *text_checkpoint_api;
static unsigned checkpoint_enables,checkpoint_snapshots,checkpoint_pauses,checkpoint_resumes,checkpoint_focus_pauses;
static bool text_usb_held,queued_policy_key;
static unsigned checkpoint_pause_busy,policy_activity_before_queue;
static unsigned resume_focus_changes,resume_normal,gap_hold_yields;
static bool gap_input_sent;
static int gap_key_x,gap_key_y,gap_next_x,gap_next_y,gap_done_x,gap_done_y,gap_back_x,gap_back_y,gap_home_x,gap_home_y;
static bool gap_input_mode(void){return !strncmp(mode,"policy-gap-",11)||!strncmp(mode,"policy-sleep-wake-",18)||is_mode("policy-sleep-touch")||is_mode("policy-sleep-held")||is_mode("policy-sleep-usb")||is_mode("policy-sleep-usb-held");}
static risc_text_entry_api_v1 text_old;
static bool text_closing,text_frozen,text_attention_pending,text_usb_connected;
static unsigned text_opens,text_polls,text_closes,text_releases,text_close_again,text_close_wait,text_poll_frozen;
static uint64_t text_token;
static unsigned edit_cycles;
static char cycle_input[2];
static lists_model text_before;
static uint8_t text_stored_before[LISTS_WIRE_SIZE];
static risc_text_entry_state_v1 text_observed;
static risc_usb_keyboard_event_v1 text_keys[2];
static unsigned text_key_at,text_key_count;
static uint64_t text_key_sequence;
static uint64_t text_usb_subscribe(void*c,uint64_t d){(void)c;assert(native_alive&&!d&&!text_usb_subscribed);text_usb_subscribed=true;return 78;}
static bool text_usb_unsubscribe(void*c,uint64_t s){(void)c;assert(native_alive&&s==78&&text_usb_subscribed);text_usb_subscribed=false;return true;}
static bool text_usb_poll(void*c,size_t n){(void)c;assert(native_alive&&n<=8);observe_input_poll(true);return true;}
static int32_t text_usb_next(void*c,uint64_t s,risc_usb_keyboard_event_v1*out){(void)c;assert(native_alive&&s==78);if(text_key_at==text_key_count)return 0;*out=text_keys[text_key_at++];return 1;}
static bool text_usb_snapshot(void*c,risc_usb_keyboard_state_v1*out,size_t*n){(void)c;assert(native_alive&&*n);*n=text_usb_connected?1:0;if(*n)*out=(risc_usb_keyboard_state_v1){.device=9,.connected=1,.keys={text_usb_held?4:0}};return true;}
static const risc_usb_keyboard_api_v1 text_usb={1,sizeof(text_usb),NULL,text_usb_subscribe,text_usb_unsubscribe,text_usb_poll,text_usb_next,text_usb_snapshot};
static void text_hardware(unsigned ch){
 unsigned usage=0,mods=0;
 if(ch>='a'&&ch<='z')usage=4+ch-'a';else if(ch>='A'&&ch<='Z'){usage=4+ch-'A';mods=2;}
 else if(ch>='1'&&ch<='9')usage=30+ch-'1';else if(ch=='0')usage=39;
 else switch(ch){case 200:usage=0x39;break;case ' ':usage=0x2c;break;case '!':usage=30;mods=2;break;case '?':usage=0x38;mods=2;break;case '_':usage=0x2d;mods=2;break;case '[':usage=0x2f;break;case ']':usage=0x30;break;case '{':usage=0x2f;mods=2;break;case '}':usage=0x30;mods=2;break;case '|':usage=0x31;mods=2;break;case '~':usage=0x35;mods=2;break;case RISC_SCENE_KEY_BACKSPACE:usage=0x2a;break;case RISC_SCENE_KEY_DONE:usage=0x28;break;case RISC_SCENE_KEY_CANCEL:usage=0x29;break;default:assert(0);}
 assert(text_key_at==text_key_count);text_key_at=0;text_key_count=1;text_keys[0]=(risc_usb_keyboard_event_v1){++text_key_sequence,9,3,(uint8_t)usage,(uint8_t)mods,0};
}
static int text_open(void*c,const risc_text_entry_request_v1*q,uint64_t*out){
 (void)c;assert(native_alive&&!session&&!text_active&&!held&&!presenting&&!subscribed&&text_granted);
 assert(q->capacity==LISTS_NAME&&!strcmp(q->label,"NAME")&&q->reserved==RISC_TEXT_ENTRY_REQUEST_HOME_REASON);
 assert(!strcmp(q->text,ui.draft));for(unsigned i=0;i<ui.document.node_count;i++)assert(ui.document.nodes[i].kind!=RISC_SCENE_KEYBOARD_NODE);
 ++text_opens;text_before=ui.data;memcpy(text_stored_before,stored,sizeof(stored));
 boundary("open");if(!native_alive)return RISC_TEXT_ENTRY_RETAINED;
 if((is_mode("retry")&&text_opens==1)||is_mode("open-unavailable"))return RISC_TEXT_ENTRY_UNAVAILABLE;
 if(is_mode("open-busy"))return RISC_TEXT_ENTRY_BUSY;
 if(is_mode("open-invalid"))return RISC_TEXT_ENTRY_INVALID;
 if(is_mode("open-retained"))return RISC_TEXT_ENTRY_RETAINED;
 if(is_mode("open-token")){*out=41;return RISC_TEXT_ENTRY_UNAVAILABLE;}
 int rc=text_real->open(text_real->context,q,out);if(!rc){text_active=true;text_closing=false;text_token=*out;}return rc;
}
static int text_poll(void*c,uint64_t s,risc_text_entry_state_v1*out){
 (void)c;assert(native_alive&&text_active&&text_granted&&!session&&s==text_token&&!text_closing&&!text_paused);++text_polls;
 assert(!memcmp(stored,text_stored_before,sizeof(stored))&&!memcmp(&ui.data,&text_before,sizeof(ui.data)));
 assert(!strcmp(ui.draft,initial_name)||!edit_action);
 boundary("poll");if(!native_alive)return RISC_TEXT_ENTRY_RETAINED;
 if(is_mode("poll-retained"))return RISC_TEXT_ENTRY_RETAINED;
 if(is_mode("poll-error"))return RISC_TEXT_ENTRY_INVALID;
 assert(checkpoint_enables);
 if(is_mode("poll-again")&&text_polls<7)return RISC_TEXT_ENTRY_AGAIN;
 int rc=text_real->poll(text_real->context,s,out);
 if(!rc){
  if(is_mode("poll-size"))out->struct_size=0;
  if(is_mode("poll-revision"))out->revision=0;
  if(is_mode("poll-flags"))out->flags|=128;
  if(is_mode("poll-home-state"))out->flags|=RISC_TEXT_ENTRY_HOME_CANCEL;
  if(is_mode("poll-state"))out->state=99;
  if(is_mode("poll-unterminated"))memset(out->text,'A',sizeof(out->text));
  if(is_mode("poll-control"))out->text[0]='\n';
  text_observed=*out;text_attention_pending=true;if(text_frozen)++text_poll_frozen;
 }
 return rc;
}
static int text_close(void*c,uint64_t s){
 (void)c;assert(native_alive&&text_active&&text_granted&&!session&&s==text_token);text_closing=true;++text_closes;
 assert(!memcmp(&ui.data,&text_before,sizeof(ui.data))&&!memcmp(stored,text_stored_before,sizeof(stored)));
 if(is_mode("home-close"))native_exit_pending=true;
 boundary("close");if(!native_alive)return RISC_TEXT_ENTRY_RETAINED;
 if(is_mode("close-retained"))return RISC_TEXT_ENTRY_RETAINED;
 if(is_mode("close-error"))return RISC_TEXT_ENTRY_INVALID;
 int rc=text_real->close(text_real->context,s);if(rc==RISC_TEXT_ENTRY_AGAIN)++text_close_again;if(!rc){text_active=false;text_paused=false;text_token=0;}return rc;
}
static bool checkpoint_fault(const char*site){
 char name[80];snprintf(name,sizeof(name),"checkpoint-%s",site);boundary(name);if(!native_alive)return true;
 snprintf(name,sizeof(name),"checkpoint-%s-sleep",site);boundary(name);if(!native_alive)return true;
 snprintf(name,sizeof(name),"checkpoint-%s-retained",site);if(is_mode(name))return true;
 snprintf(name,sizeof(name),"checkpoint-%s-retained-sleep",site);return is_mode(name);
}
static void checkpoint_home(const char*site){char name[80];snprintf(name,sizeof(name),"home-checkpoint-%s",site);if(is_mode(name))native_exit_pending=true;snprintf(name,sizeof(name),"home-checkpoint-%s-sleep",site);if(is_mode(name))native_exit_pending=true;}
static int checkpoint_enable(void*c,uint64_t s){
 (void)c;assert(native_alive&&text_active&&text_granted&&!text_paused);++checkpoint_enables;
 if(checkpoint_fault("enable"))return RISC_TEXT_ENTRY_RETAINED;
 if(is_mode("checkpoint-enable-unavailable"))return RISC_TEXT_ENTRY_UNAVAILABLE;
 if(is_mode("checkpoint-enable-busy"))return RISC_TEXT_ENTRY_BUSY;
 if(is_mode("checkpoint-enable-error"))return RISC_TEXT_ENTRY_INVALID;
 int rc=text_checkpoint_api->checkpoint_enable(text_real->context,s);checkpoint_home("enable");return rc;
}
static int checkpoint_snapshot(void*c,uint64_t s,risc_text_entry_checkpoint_state_v1*out){
 (void)c;assert(native_alive&&text_active&&text_granted);++checkpoint_snapshots;
 if(checkpoint_fault("snapshot"))return RISC_TEXT_ENTRY_RETAINED;
 if(is_mode("checkpoint-snapshot-busy")&&checkpoint_snapshots<6)return RISC_TEXT_ENTRY_BUSY;
 int rc=text_checkpoint_api->checkpoint_snapshot(text_real->context,s,out);
 if(!rc){if(is_mode("checkpoint-snapshot-size"))out->struct_size=0;if(is_mode("checkpoint-snapshot-flags"))out->flags|=128;if(is_mode("checkpoint-snapshot-paused"))out->flags|=RISC_TEXT_ENTRY_CHECKPOINT_PAUSED;}
 checkpoint_home("snapshot");return rc;
}
static int checkpoint_pause_mode(void*c,uint64_t s,bool focus_change){
 (void)c;assert(native_alive&&text_active&&text_granted&&!policy_input_blocked);++checkpoint_pauses;
 assert(focus_change==!policy_continuity_granted);if(focus_change)++checkpoint_focus_pauses;
 if(gap_input_mode()&&!gap_input_sent&&!policy_usb_mode()){
  assert(lists_test_target(1,'9',&gap_key_x,&gap_key_y));
  assert(lists_test_target(1,'8',&gap_next_x,&gap_next_y));
  assert(lists_test_target(1,RISC_SCENE_KEY_DONE,&gap_done_x,&gap_done_y));
  assert(lists_test_header(false,&gap_back_x,&gap_back_y));assert(lists_test_header(true,&gap_home_x,&gap_home_y));
 }
 if(checkpoint_fault("pause"))return RISC_TEXT_ENTRY_RETAINED;
 if(is_mode("checkpoint-pause-error"))return RISC_TEXT_ENTRY_INVALID;
 if((is_mode("checkpoint-pause-again")||is_mode("home-checkpoint-pause-again"))&&checkpoint_pauses<4){checkpoint_home("pause-again");return RISC_TEXT_ENTRY_AGAIN;}
 if((is_mode("policy-usb-queued")||is_mode("policy-touch-queued")||is_mode("policy-request-reset"))&&!queued_policy_key){
  policy_activity_before_queue=policy_activity_polls;
  if(is_mode("policy-request-reset")){policy_request_invalidated=true;policy_invalidated_at_wait=policy_wait;}
  if(is_mode("policy-usb-queued"))text_hardware('x');
  else{int x,y;assert(lists_test_target(1,'9',&x,&y));touch_tap((unsigned)x,(unsigned)y);}
  queued_policy_key=true;
 }
 int rc=focus_change?text_checkpoint_api->pause_for_focus_change(text_real->context,s):text_checkpoint_api->pause(text_real->context,s);if(rc==RISC_TEXT_ENTRY_BUSY)++checkpoint_pause_busy;if(!rc)text_paused=true;checkpoint_home("pause");return rc;
}
static int checkpoint_pause(void*c,uint64_t s){return checkpoint_pause_mode(c,s,false);}
static int checkpoint_pause_focus(void*c,uint64_t s){return checkpoint_pause_mode(c,s,true);}
static int checkpoint_resume(void*c,uint64_t s,uint32_t flags){
 (void)c;assert(native_alive&&text_active&&text_granted&&text_paused);++checkpoint_resumes;
 assert(!(flags&~(RISC_TEXT_ENTRY_RESUME_REDRAW|RISC_TEXT_ENTRY_RESUME_FOCUS_CHANGED)));
 assert(flags==expected_resume_flags);
 if(flags&RISC_TEXT_ENTRY_RESUME_FOCUS_CHANGED)++resume_focus_changes;else ++resume_normal;
 if(checkpoint_fault("resume"))return RISC_TEXT_ENTRY_RETAINED;
 if(is_mode("checkpoint-resume-error"))return RISC_TEXT_ENTRY_INVALID;
 int rc=text_checkpoint_api->resume(text_real->context,s,flags);if(!rc){text_paused=false;assert(navigation_owned);}checkpoint_home("resume");return rc;
}
static void policy_boundary_input(void){
 if(!gap_input_mode()||gap_input_sent)return;
 assert(text_paused&&!presenting&&!held);
 if(policy_continuity_granted)assert(subscribed&&text_usb_subscribed&&navigation_owned);else assert(!subscribed&&!text_usb_subscribed&&!navigation_owned);
 gap_input_sent=true;
 if(is_mode("policy-gap-done")||is_mode("policy-sleep-wake-done")){touch_tap((unsigned)gap_done_x,(unsigned)gap_done_y);return;}
 if(is_mode("policy-gap-usb-done")||is_mode("policy-sleep-wake-usb-done")){text_hardware(RISC_SCENE_KEY_DONE);return;}
 if(is_mode("policy-sleep-wake-back")){touch_tap((unsigned)gap_back_x,(unsigned)gap_back_y);return;}
 if(is_mode("policy-sleep-wake-home")){touch_tap((unsigned)gap_home_x,(unsigned)gap_home_y);return;}
 if(is_mode("policy-sleep-wake-native-back")){nav_pressed=RISC_NAV_BACK;return;}
 if(is_mode("policy-sleep-wake-usb-cancel")){text_hardware(RISC_SCENE_KEY_CANCEL);return;}
 if(is_mode("policy-gap-native-cancel")){nav_pressed=RISC_NAV_BACK;return;}
 if(is_mode("policy-gap-native-home")||is_mode("policy-gap-usb-home")){nav_pressed=RISC_NAV_HOME;return;}
 if(is_mode("policy-gap-usb-cancel")){text_hardware(RISC_SCENE_KEY_CANCEL);return;}
 if(is_mode("policy-gap-cancel")){touch_tap((unsigned)gap_back_x,(unsigned)gap_back_y);return;}
 if(is_mode("policy-gap-home")){touch_tap((unsigned)gap_home_x,(unsigned)gap_home_y);return;}
 if(policy_usb_mode()){
  text_hardware('x');text_usb_held=strstr(mode,"held")!=NULL;return;
 }
 bool held_key=strstr(mode,"held")!=NULL;event_at=event_count=0;
 unsigned taps=held_key?1:2;
 for(unsigned i=0;i<taps;i++){
  bool ordered=i&&(is_mode("policy-gap-delay-50")||is_mode("policy-gap-delay-250"));
  unsigned x=ordered?(unsigned)gap_next_x:(unsigned)gap_key_x,y=ordered?(unsigned)gap_next_y:(unsigned)gap_key_y;
  events[event_count++]=(risc_touch_event_v1){++seq,ms+30*i,1,0,(uint16_t)x,(uint16_t)y};
  if(!held_key)events[event_count++]=(risc_touch_event_v1){++seq,ms+30*i+20,3,0,(uint16_t)x,(uint16_t)y};
 }
 held_input=held_key;
}
static bool text_test_acquire(risc_runtime_capability_v1*g){
 assert(native_alive&&!text_granted);text_before=ui.data;memcpy(text_stored_before,stored,sizeof(stored));boundary("acquire");if(!native_alive)return false;
 if(is_mode("missing"))return false;
 if(is_mode("acquire-dirty")){g->slot=13;return false;}
 text_granted=true;g->api=is_mode("old-host")?(const void*)&text_old:&text_api;g->slot=++app_acquires;g->generation=1;return true;
}
static bool text_test_is_grant(const risc_runtime_capability_v1*g){return g->api==&text_api||g->api==&text_old;}
static bool text_test_release(risc_runtime_capability_v1*g){
 assert(native_alive&&text_granted&&!text_active);boundary("release");if(!native_alive)return false;
 if(is_mode("release-fail"))return false;
 if(is_mode("release-dirty"))return true;
 if(is_mode("home-release"))native_exit_pending=true;
 text_granted=false;++text_releases;++app_releases;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};
 if(is_mode("resume-fault"))bad_snapshot=true;
 return true;
}
static bool text_test_exit_requested(void){
 assert(native_alive);
 bool after_release=text_releases&&!text_granted;
 if((text_attention_pending&&is_mode("loss-attention"))||(after_release&&!session&&is_mode("loss-resume"))||(after_release&&session&&is_mode("loss-post-resume")))native_alive=false;
 if((after_release&&!session&&is_mode("home-resume"))||(after_release&&session&&is_mode("home-post-resume")))native_exit_pending=true;
 text_attention_pending=false;return native_exit_pending;
}
static void text_key(unsigned value){if(text_usb_connected){text_hardware(value);return;}int x,y;assert(lists_test_target(1,(int)value,&x,&y));touch_tap((unsigned)x,(unsigned)y);}
static void text_burst(void){
 event_at=event_count=0;
 for(unsigned i=0;i<strlen(edit_input)+2;i++){
  unsigned value=i<strlen(edit_input)?(unsigned char)edit_input[i]:i==strlen(edit_input)?RISC_SCENE_KEY_DONE:'Z';int x,y;assert(lists_test_target(1,(int)value,&x,&y));
  assert(event_count+2<=sizeof(events)/sizeof(events[0]));
  events[event_count++]=(risc_touch_event_v1){++seq,ms+i*30,1,0,(uint16_t)x,(uint16_t)y};
  events[event_count++]=(risc_touch_event_v1){++seq,ms+i*30+20,3,0,(uint16_t)x,(uint16_t)y};
 }
 edit_action=(unsigned)strlen(edit_input);phase=20;
}
static void text_header(bool home){int x,y;assert(lists_test_header(home,&x,&y));touch_tap((unsigned)x,(unsigned)y);}
static void text_test_yield(void){
 if(gap_input_sent&&(held_input||text_usb_held)){
  if(++gap_hold_yields==8){
   if(held_input){event_at=0;event_count=1;events[0]=(risc_touch_event_v1){++seq,ms,3,0,(uint16_t)gap_key_x,(uint16_t)gap_key_y};}
   held_input=false;text_usb_held=false;
  }else return;
 }
 if(text_closing&&text_active){assert(++text_close_wait<1000);if(text_close_wait==12){allow_complete=true;busy_acquires=0;}return;}
 if(input_pending()||text_key_at!=text_key_count)return;
 if(phase<10){if(session)edit_route();return;}
 if(text_active){
  if(phase==10){
   prepare_edit();phase=11;
   if(is_mode("reopen")){cycle_input[0]=(char)('A'+edit_cycles);cycle_input[1]=0;edit_input=cycle_input;size_t length=strlen(initial_name);assert(length+1<sizeof(expected_name));memcpy(expected_name,initial_name,length);expected_name[length]=cycle_input[0];expected_name[length+1]=0;}
   if(reason==2&&is_mode("done"))frame("keyboard");
   if(is_mode("delayed"))presentation_delay=500;
   if(!strncmp(mode,"frozen",6)){allow_complete=false;text_frozen=true;}
   return;
  }
  if((text_frozen||is_mode("delayed"))&&!presenting&&!edit_action)return;
  if(strstr(mode,"burst")&&!edit_action){text_burst();return;}
  if(policy_mode()){
   static const unsigned keys[]={'A',RISC_SCENE_KEY_LAYER,'1',RISC_SCENE_KEY_LAYER,'q',RISC_SCENE_KEY_LAYER,'!'};
   if(policy_usb_mode()){
    if(edit_action==0){text_hardware(200);++edit_action;return;}
    if(edit_action==1){text_hardware('a');++edit_action;return;}
    if(edit_action==2){text_hardware('1');++edit_action;return;}
   }
   if(edit_action==3&&(policy_wait<225||policy_dispatches<2||policy_polls<2)){
    assert(++policy_wait<2000);policy_armed=true;
    if(is_mode("policy-request-reset")&&policy_request_invalidated&&policy_wait<policy_invalidated_at_wait+100)assert(!policy_dispatches);
    bool blocking=is_mode("policy-touch")||is_mode("policy-usb")||is_mode("policy-presenting");
    if(blocking&&policy_wait==1){
     assert(!policy_dispatches);policy_input_blocked=true;
     if(is_mode("policy-touch")){held_input=true;event_at=0;event_count=1;events[0]=(risc_touch_event_v1){++seq,ms,1,0,(uint16_t)(w==800?h/2:w/2),60};}
     if(is_mode("policy-usb"))text_usb_held=true;
     if(is_mode("policy-presenting"))allow_complete=false;
    }
    if(blocking&&policy_wait==101){
     assert(!policy_dispatches);policy_input_blocked=false;held_input=false;text_usb_held=false;allow_complete=true;
     if(is_mode("policy-touch")){event_at=0;event_count=1;events[0]=(risc_touch_event_v1){++seq,ms,3,0,(uint16_t)(w==800?h/2:w/2),60};}
    }
    return;
   }
   if((policy_usb_mode())&&edit_action>=3){
    if(edit_action==3){text_hardware('q');++edit_action;return;}
    if(edit_action==4){text_hardware('!');++edit_action;return;}
    edit_action=(unsigned)(sizeof(keys)/sizeof(keys[0]));
   }
   if(is_mode("policy-repeat")&&edit_action==5){if(!policy_wait_second)policy_second_start=policy_dispatches;if(policy_wait_second<125||policy_dispatches<policy_second_start+2){assert(++policy_wait_second<2000);policy_armed=true;return;}}
   policy_armed=false;
   if(edit_action<sizeof(keys)/sizeof(keys[0])){text_key(keys[edit_action++]);return;}
  }
  else if(is_mode("empty")&&edit_action<strlen(initial_name)){text_key(RISC_SCENE_KEY_BACKSPACE);edit_action++;return;}
  if(policy_mode()){}
  else if(is_mode("layers")){if(edit_action<strlen(initial_name)){text_key(RISC_SCENE_KEY_BACKSPACE);edit_action++;return;}static const unsigned keys[]={'A',RISC_SCENE_KEY_LAYER,'1',RISC_SCENE_KEY_LAYER,'q',RISC_SCENE_KEY_LAYER,'!'};if(edit_action-strlen(initial_name)<sizeof(keys)/sizeof(keys[0])){text_key(keys[edit_action++-strlen(initial_name)]);return;}}
  else if(!is_mode("empty")&&edit_input[edit_action]){text_key((unsigned char)edit_input[edit_action++]);return;}
  if(is_mode("home")||is_mode("frozen-home"))nav_pressed=RISC_NAV_HOME;
  else if(is_mode("header-home"))text_header(true);
  else if(is_mode("native-home"))native_exit_pending=true;
  else if(is_mode("cancel")||is_mode("frozen-back")||(is_mode("reopen")&&edit_cycles==1))nav_pressed=RISC_NAV_BACK;
  else if(is_mode("header-back"))text_header(false);
  else if(is_mode("back"))nav_pressed=RISC_NAV_BACK;
  else text_key(RISC_SCENE_KEY_DONE);
  phase=20;return;
 }
 if(!session)return;
 if(phase==10&&!ui.text_retry)return;
 if(is_mode("retry")&&text_opens==1&&ui.text_retry){assert(text_releases==1);tap(LA_TEXT_RETRY,-999);phase=10;return;}
 if(phase==40){assert(ui.page==ui.parent);native_exit_pending=true;phase=99;return;}
 if(phase==41){assert(ui.last_event!=UINT64_MAX);validate_edit(false);native_exit_pending=true;phase=99;return;}
 if(ui.text_retry){
  assert(ui.page==LISTS_NAME_EDIT&&!memcmp(stored,initial_stored,sizeof(stored)));
  bool retry_node=false,cancel_node=false;for(unsigned i=0;i<ui.document.node_count;i++){const risc_scene_node_v1*n=&ui.document.nodes[i];assert(n->kind!=RISC_SCENE_KEYBOARD_NODE);retry_node|=n->action==LA_TEXT_RETRY;cancel_node|=n->action==LA_BACK;}
  assert(retry_node&&cancel_node&&ui.notice[0]);if(is_mode("missing"))frame("unavailable-text");tap(LA_BACK,-999);phase=40;return;
 }
 assert(!text_granted&&text_releases==text_opens);
 if(is_mode("reopen")){
  const char*want=edit_cycles==1?initial_name:expected_name;
  assert(!strcmp(lists_find_list(&ui.data,work_id)->name,want));assert(writes==(edit_cycles<2?1u:2u));
  if(edit_cycles<2){++edit_cycles;edit_action=0;tap(LA_RENAME_LIST,-999);phase=10;return;}
  assert(!strcmp(want,"WORKAC")&&disk_revision==3);native_exit_pending=true;phase=99;return;
 }
 if(policy_usb_mode()&&!policy_cancel_mode()){
  char want[LISTS_NAME];size_t length=strlen(initial_name);const char*suffix=is_mode("policy-gap-usb-done")?"A1":is_mode("policy-usb-queued")||is_mode("policy-gap-usb")||is_mode("policy-gap-usb-held")?"A1XQ!":"A1Q!";assert(length+strlen(suffix)<sizeof(want));memcpy(want,initial_name,length);strcpy(want+length,suffix);assert(!strcmp(text_observed.text,want));
  if(is_mode("policy-usb-queued"))assert(queued_policy_key&&checkpoint_pause_busy&&policy_activity_polls>policy_activity_before_queue);
 }
 if(is_mode("policy-touch-queued")||is_mode("policy-request-reset"))assert(queued_policy_key&&checkpoint_pause_busy&&policy_activity_polls>policy_activity_before_queue);
 validate_edit(policy_cancel_mode()||is_mode("cancel")||is_mode("back")||is_mode("header-back")||is_mode("frozen-back"));
 if(is_mode("stale")){
  stale_event=(risc_scene_event_v1){.struct_size=sizeof(stale_event),.kind=RISC_SCENE_VALUE_EVENT,.document_revision=ui.document.revision-1,.node=1,.action=LA_KEY,.value='Z',.sequence=UINT64_MAX};inject_stale=true;phase=41;return;
 }
 native_exit_pending=true;phase=99;
}
static void text_test_setup(void){
 text_driver=lists_text_driver_get(2);assert(text_driver);text_real=text_driver->capability;
 text_checkpoint_api=risc_text_entry_checkpoint(text_real);assert(text_checkpoint_api);text_api=*text_checkpoint_api;
 risc_text_entry_api_v1*base=(risc_text_entry_api_v1*)&text_api;base->open=text_open;base->poll=text_poll;base->close=text_close;
 if(is_mode("no-checkpoint"))base->struct_size=sizeof(risc_text_entry_api_v1_long);
 if(is_mode("old-checkpoint"))base->struct_size=offsetof(risc_text_entry_api_v1_checkpoint,pause_for_focus_change);
 text_api.checkpoint_enable=checkpoint_enable;text_api.checkpoint_snapshot=checkpoint_snapshot;text_api.pause=checkpoint_pause;text_api.resume=checkpoint_resume;text_api.pause_for_focus_change=is_mode("null-focus-pause")?NULL:checkpoint_pause_focus;
 text_old=(risc_text_entry_api_v1){1,sizeof(text_old),NULL,NULL,text_poll,text_close};text_usb_connected=is_mode("hardware")||policy_usb_mode();
 risc_provider_dependency_v1 deps[]={{RISC_SCENE_CAPABILITY,1,api},{"usb.hid.keyboard",1,&text_usb}};assert(text_driver->start(deps,2));
}
static void text_test_finish(void){
 bool retained=edit_case&&(!strncmp(mode,"loss-",5)||strstr(mode,"retained")||(!strncmp(mode,"poll-",5)&&!is_mode("poll-again"))||is_mode("open-token")||is_mode("acquire-dirty")||!strncmp(mode,"close-",6)||!strncmp(mode,"release-",8)||is_mode("resume-fault")||is_mode("save-loss")||is_mode("policy-malformed")||is_mode("policy-poll-malformed")||is_mode("policy-marker-violation")||is_mode("checkpoint-enable-error")||is_mode("checkpoint-pause-error")||is_mode("checkpoint-resume-error")||is_mode("checkpoint-snapshot-size")||is_mode("checkpoint-snapshot-flags")||is_mode("checkpoint-snapshot-paused"));
 if(retained){assert(!native_alive&&terminal&&disk_revision==1);assert(!memcmp(stored,initial_stored,sizeof(stored)));return;}
 assert(native_alive&&!text_active&&!text_granted&&!text_usb_subscribed);assert(text_driver->quiesce());text_driver->stop();
 if(!edit_case)return;
 bool home=is_mode("policy-home")||is_mode("policy-sleep-home")||is_mode("policy-gap-home")||is_mode("policy-gap-native-home")||is_mode("policy-gap-usb-home")||is_mode("home")||is_mode("frozen-home")||is_mode("header-home")||is_mode("native-home")||!strncmp(mode,"home-",5);
 if(home){assert(!writes&&disk_revision==1&&!ui.draft[0]);assert(!memcmp(stored,initial_stored,sizeof(stored)));}
 if(!strncmp(mode,"frozen",6)){
  assert(text_poll_frozen>10&&text_close_again>=10);assert(text_observed.state==((is_mode("frozen")||is_mode("frozen-burst"))?RISC_TEXT_ENTRY_ACCEPTED:RISC_TEXT_ENTRY_CANCELLED));assert(!strcmp(text_observed.text,expected_name));
 }
 if(is_mode("delayed"))assert(text_close_again);
 if(is_mode("retry"))assert(text_opens==2&&text_releases==2);
 if(is_mode("reopen"))assert(text_opens==3&&text_releases==3&&writes==2);
 if(policy_mode()&&!home&&!policy_cancel_mode()&&!strstr(mode,"exit")&&!is_mode("no-checkpoint")&&!is_mode("old-checkpoint")&&!is_mode("policy-gap-done")&&!is_mode("policy-gap-usb-done")&&!is_mode("checkpoint-enable-unavailable")&&!is_mode("checkpoint-enable-busy")){assert(policy_polls>=2&&policy_dispatches>=2&&checkpoint_pauses>=2&&checkpoint_resumes>=2);}
 if(strstr(mode,"exit"))assert(!writes&&disk_revision==1);
 if(policy_cancel_mode())assert(!writes&&disk_revision==1&&gap_input_sent);
 if(is_mode("policy-reply-alternate"))assert(resume_normal&&resume_focus_changes&&checkpoint_focus_pauses);
 if(is_mode("policy-no-marker")||is_mode("policy-marker-only")||is_mode("policy-poll-busy-marker")||is_mode("policy-poll-extra-marker"))assert(checkpoint_focus_pauses==checkpoint_pauses);
 if(policy_sleep_mode()&&!home&&!strstr(mode,"exit"))assert(resume_focus_changes);
 if(gap_input_mode()&&!home&&!policy_cancel_mode())assert(gap_input_sent);
 if(edit_case&&text_opens)assert(checkpoint_enables==text_opens||is_mode("retry")||is_mode("open-unavailable")||is_mode("open-busy")||is_mode("open-invalid")||is_mode("no-checkpoint")||is_mode("old-checkpoint")||is_mode("null-focus-pause"));
 printf("Lists shared %s reason=%u %s PASS (opens=%u polls=%u close_again=%u releases=%u policy_polls=%u policy=%u pauses=%u resumes=%u)\n",mode,reason,device,text_opens,text_polls,text_close_again,text_releases,policy_polls,policy_dispatches,checkpoint_pauses,checkpoint_resumes);
}
