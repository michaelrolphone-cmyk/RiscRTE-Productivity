/* Actual Points controller with a copied, bounded modal provider boundary. */
#define WATCH_POINTS_SHARED_INPUT
#define PORTABLE_TEXT_INPUT_CLIENT
#define main legacy_points_fixture_main
#include "points_in_time_test.c"
#undef main
static bool modal_retained,modal_suspended,modal_unavailable;
static unsigned modal_opens,modal_closes,modal_releases,modal_pending;
static risc_text_entry_request_v1 modal_request;
static risc_text_entry_state_v1 modal_result;
int portable_text_adapter_suspend(void){assert(!modal_suspended&&!modal_retained);modal_suspended=true;return RISC_TEXT_ENTRY_OK;}
int portable_text_adapter_resume(void){assert(modal_suspended&&!modal_retained);modal_suspended=false;return RISC_TEXT_ENTRY_OK;}
int portable_text_adapter_attention(void){return 0;}
void portable_text_adapter_retain(void){modal_retained=true;}
bool portable_text_adapter_retained(void){return modal_retained;}
static int32_t modal_open(void *context,const risc_text_entry_request_v1 *request,uint64_t *session){
 (void)context;assert(modal_suspended&&!modal_retained);assert(request->capacity==POINTS_CUSTOM_NAME_MAX+1);
 assert(!(request->reserved&RISC_TEXT_ENTRY_REQUEST_MASKED));assert(!strcmp(request->label,"Custom type"));
 modal_request=*request;modal_opens++;*session=1;return RISC_TEXT_ENTRY_OK;
}
static int32_t modal_poll(void *context,uint64_t session,risc_text_entry_state_v1 *state){
 (void)context;assert(session==1&&modal_suspended&&!modal_retained);*state=modal_result;return RISC_TEXT_ENTRY_OK;
}
static int32_t modal_close(void *context,uint64_t session){
 (void)context;assert(session==1&&modal_suspended&&!modal_retained);modal_closes++;
 if(modal_pending){modal_pending--;return RISC_TEXT_ENTRY_AGAIN;}return RISC_TEXT_ENTRY_OK;
}
static const risc_text_entry_api_v1_home_reason modal_api={{1,sizeof(modal_api),NULL,modal_open,modal_poll,modal_close},RISC_TEXT_ENTRY_HOME_REASON_TAG,RISC_TEXT_ENTRY_HOME_REASON_VERSION};
static bool modal_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *grant){
 assert(!modal_retained);
 if(!strcmp(name,RISC_TEXT_ENTRY_CAPABILITY)){
  assert(version==1&&!instance);if(modal_unavailable)return false;grant->api=&modal_api;
 }else if(!acquire(name,version,instance,grant))return false;
 grant->slot=1;grant->generation=1;return true;
}
static bool modal_release(risc_runtime_capability_v1 *grant){
 assert(!modal_retained);if(grant->api==&modal_api)modal_releases++;
 *grant=(risc_runtime_capability_v1){.struct_size=sizeof(*grant)};return true;
}
static void initialize(void){
 setup();modal_retained=modal_suspended=modal_unavailable=false;modal_opens=modal_closes=modal_releases=modal_pending=0;
 points_text_client=(portable_text_client){0};points_text_state=(risc_text_entry_state_v1){0};points_text_closing=points_text_home=false;
 runtime_api.acquire=modal_acquire;runtime_api.release=modal_release;
 points_config config={.revision=3,.created=1000};
 for(unsigned i=0;i<POINTS_MAX;i++)config.points[i]=(points_item){.kind=POINTS_CUSTOM_1,.enabled=(uint8_t)(i&1),.mode=(uint8_t)(i%4),.weekdays=(uint8_t)(1u<<(i%7)),.hour=(uint8_t)(i*3),.minute=(uint8_t)(i*7),.duration_minutes=(uint16_t)(3+i*37),.notify_end=(uint8_t)(i&1),.warn3=(uint8_t)((i>>1)&1)};
 points_config_encode(&config,stored);stored_size=POINTS_RECORD_SIZE;
 points_meta meta={.revision=2,.custom={{.color=6,.name="Original"},{.color=2,.name="Other"}}};
 points_meta_encode(&meta,stored_meta);stored_meta_size=POINTS_RECORD_SIZE;
 set_direct();edit_slot(0);p7_custom_open(POINTS_CUSTOM_1,false);
 draft=(points_item){.kind=POINTS_CUSTOM_1,.enabled=1,.mode=3,.weekdays=127,.hour=23,.minute=59,.duration_minutes=720,.notify_end=1,.warn3=1};
 modal_result=(risc_text_entry_state_v1){.struct_size=sizeof(modal_result),.revision=1,.state=RISC_TEXT_ENTRY_EDITING};
}
static void accepted(const char *text){modal_result.state=RISC_TEXT_ENTRY_ACCEPTED;snprintf(modal_result.text,sizeof(modal_result.text),"%s",text);p7_key_step();}
int main(void){
 initialize();uint8_t original[64],metadata[64];memcpy(original,stored,64);memcpy(metadata,stored_meta,64);
 points_item draft_before=draft;points_meta custom_before=custom_draft;
 custom_draft.custom[0].color=4;p7_key_begin();assert(page==PAGE_CUSTOM_KEYBOARD&&modal_suspended);
 assert(!strcmp(modal_request.text,"Original"));modal_pending=3;accepted("New !abc");
 assert(page==PAGE_CUSTOM_KEYBOARD&&!strcmp(custom_draft.custom[0].name,"Original")&&!meta_puts_count&&!puts_count);
 for(unsigned i=0;i<3;i++)p7_key_step();
 assert(page==PAGE_CUSTOM&&!modal_suspended&&modal_releases==1&&modal_closes==4);
 assert(!strcmp(custom_draft.custom[0].name,"New !abc")&&custom_draft.custom[0].color==4);
 assert(!memcmp(&draft,&draft_before,sizeof(draft))&&!memcmp(stored,original,64)&&!memcmp(stored_meta,metadata,64));
 p7_key_begin();assert(!strcmp(modal_request.text,"New !abc"));modal_result.state=RISC_TEXT_ENTRY_CANCELLED;p7_key_step();
 assert(!strcmp(custom_draft.custom[0].name,"New !abc")&&custom_draft.custom[0].color==4);
 p7_custom_save();assert(page==PAGE_EDIT&&meta_puts_count==1&&!puts_count);
 points_meta saved_meta;assert(points_meta_decode(&saved_meta,stored_meta,stored_meta_size));
 assert(saved_meta.revision==3&&!strcmp(saved_meta.custom[0].name,"New !abc")&&saved_meta.custom[0].color==4);
 assert(!memcmp(&saved_meta.custom[1],&custom_before.custom[1],sizeof(points_custom_type))&&!memcmp(stored,original,64));
 save_action();points_config saved_points;assert(points_config_decode(&saved_points,stored,stored_size));
 assert(!memcmp(&saved_points.points[0],&draft_before,sizeof(draft_before))&&puts_count==1);
 points_config original_points;assert(points_config_decode(&original_points,original,64));
 for(unsigned i=1;i<POINTS_MAX;i++)assert(!memcmp(&saved_points.points[i],&original_points.points[i],sizeof(points_item)));
 close_dependencies();
 initialize();custom_before=custom_draft;draft_before=draft;p7_key_begin();p7_key_cancel();
 assert(page==PAGE_CUSTOM&&!modal_suspended&&!memcmp(&custom_before,&custom_draft,sizeof(custom_before))&&!memcmp(&draft_before,&draft,sizeof(draft)));
 initialize();p7_key_begin();accepted("");p7_custom_save();assert(page==PAGE_CUSTOM&&!meta_puts_count&&!strcmp(notice,"Custom name cannot be empty"));
 initialize();p7_key_begin();accepted("ABCDEFGHIJKLM");assert(!strcmp(custom_draft.custom[0].name,"ABCDEFGHIJKLM"));
 initialize();p7_key_begin();modal_result.state=RISC_TEXT_ENTRY_CANCELLED;modal_result.flags=RISC_TEXT_ENTRY_HOME_CANCEL;p7_key_step();
 assert(points_text_home&&page==PAGE_CUSTOM&&!meta_puts_count&&!puts_count&&!modal_suspended);
 initialize();modal_unavailable=true;custom_before=custom_draft;p7_key_begin();
 assert(page==PAGE_CUSTOM&&!modal_suspended&&!strcmp(notice,"Text input unavailable")&&!memcmp(&custom_before,&custom_draft,sizeof(custom_before)));
 initialize();p7_key_begin();modal_result.state=RISC_TEXT_ENTRY_ACCEPTED;memcpy(modal_result.text,"ABCDEFGHIJKLMN",15);p7_key_step();
 assert(modal_retained&&points_text_client.retained&&page==PAGE_CUSTOM_KEYBOARD&&!meta_puts_count&&!puts_count);
 puts("Watch Points shared input: exact 13-byte capacity, plaintext, pending close, draft-only Done, repeated reopen/Cancel, separate Save, colors, all eight schedules and flags, Home, unavailable and malformed result retention passed");
 return 0;
}
