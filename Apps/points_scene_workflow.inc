/* Copied workflow payloads and shared-editor focus custody. No raster adapter. */
static bool ps_message_pending,ps_message_closing,ps_launch_pending;
static char ps_workflow_message[33],ps_workflow_launch[RISC_APP_CATALOG_ROUTE_MAX];
static uint32_t ps_message_revision;
static risc_components_document_v1 ps_message_document;
bool files_scene_background_workflow_ready(void){
 return !ps_message_pending&&!ps_launch_pending&&!ps_text_pending&&!ps_settings.phase;
}
bool files_scene_background_message(const char text[33]){
 if(!ps_alive(NULL)||!text||!memchr(text,0,33)||!files_scene_background_workflow_ready())return false;
 memcpy(ps_workflow_message,text,33);ps_message_pending=true;ps_message_revision=0;ps_message_closing=false;return true;
}
/* Caller is inside paused Context progression; catalog never owns storage or
 * physical focus. Every external callback is fenced before another call. */
bool files_scene_background_launch(const char name[33]){
 if(!ps_alive(NULL)||!name||!memchr(name,0,33)||!files_scene_background_workflow_ready())return false;
 risc_runtime_capability_v1 grant={.struct_size=sizeof(grant)};
 bool acquired=ps_runtime->acquire(RISC_APP_CATALOG_CAPABILITY,1,0,&grant);
 if(!ps_alive(NULL))return false;
 if(grant.struct_size!=sizeof(grant)||(acquired?(!grant.api||!grant.slot||!grant.generation):(grant.api||grant.slot||grant.generation))){ps_retain(NULL);return false;}
 if(!acquired)return false;
 const risc_app_catalog_v1*api=grant.api;
 if(api->api_version!=1||api->struct_size<sizeof(*api)||!api->snapshot||!api->get){ps_retain(NULL);return false;}
 risc_app_catalog_snapshot_v1 snapshot={.struct_size=sizeof(snapshot)},after={.struct_size=sizeof(after)};
 int rc=api->snapshot(api->context,&snapshot);if(!ps_alive(NULL))return false;
 bool found=false;
 if(rc==RISC_APP_CATALOG_OK){
  if(snapshot.struct_size!=sizeof(snapshot)||!snapshot.generation||snapshot.count>RISC_APP_CATALOG_MAX){ps_retain(NULL);return false;}
  for(unsigned i=0;i<snapshot.count;i++){
   risc_app_catalog_entry_v1 e={.struct_size=sizeof(e)};rc=api->get(api->context,snapshot.generation,i,&e);
   if(!ps_alive(NULL))return false;
   if(rc)break;
   if(e.struct_size!=sizeof(e)||(e.flags&~RISC_APP_CATALOG_AVAILABLE)||!memchr(e.file_name,0,sizeof(e.file_name))){ps_retain(NULL);return false;}
   if((e.flags&RISC_APP_CATALOG_AVAILABLE)&&!strcmp(name,e.file_name)){memcpy(ps_workflow_launch,e.file_name,sizeof(e.file_name));found=true;break;}
  }
  if(!rc){rc=api->snapshot(api->context,&after);if(!ps_alive(NULL))return false;
   if(!rc&&(after.struct_size!=sizeof(after)||after.generation!=snapshot.generation||after.count!=snapshot.count))found=false;}
 }
 if(rc<RISC_APP_CATALOG_STALE||rc>RISC_APP_CATALOG_NOT_FOUND){ps_retain(NULL);return false;}
 risc_runtime_capability_v1 released=grant;bool ok=ps_runtime->release(&released);
 if(!ps_alive(NULL))return false;
 if(!ok||released.struct_size!=sizeof(released)||released.api||released.slot||released.generation){ps_retain(NULL);return false;}
 ps_launch_pending=!rc&&found;return ps_launch_pending;
}
/* 0 pending, 1 finished, -1 retained. This document never mutates a Points
 * selection/draft and cannot dispatch a domain action from dismissal input. */
static int ps_message_step(void){
 if(!ps_message_pending)return 1;
 if(!ps_alive(NULL))return -1;
 if(!files_scene_background_pause())return ps_alive(NULL)?0:-1;
 if(!ps_message_revision){
  if(!ps_close_scene())return ps_alive(NULL)?0:-1;
  if(ps_model.revision==UINT32_MAX){ps_retain(NULL);return -1;}
  ps_message_revision=++ps_model.revision;
  ps_message_document=(risc_components_document_v1){.api_version=1,.struct_size=sizeof(ps_message_document),.revision=ps_message_revision,.root=1,.route_count=1,.node_count=2,.screen_key=UINT32_C(0x464d0001)};
  ps_message_document.routes[0]=(risc_scene_route_v1){.id=1,.back_action=1};strcpy(ps_message_document.routes[0].title,"CONTEXT MESSAGE");
  ps_message_document.nodes[0]=(risc_scene_node_v1){.id=1,.route=1,.kind=RISC_SCENE_TEXT_NODE};memcpy(ps_message_document.nodes[0].text,ps_workflow_message,33);
  ps_message_document.nodes[1]=(risc_scene_node_v1){.id=2,.route=1,.kind=RISC_SCENE_ACTION,.action=1};strcpy(ps_message_document.nodes[1].label,"Dismiss");
 }
 if(!ps_message_closing){
  if(!ps_ui.token){int rc=files_ui_publish(&ps_ui,&ps_message_document,ps_host.enabled);if(rc!=1)return rc;}
  /* next advances the provider's presentation; snapshot alone never pumps it. */
  risc_scene_event_v1 event={0};int rc=files_ui_next(&ps_ui,&event);if(rc<0)return -1;
  if(!files_ui_snapshot(&ps_ui))return -1;
  /* A delivered current-revision action is already accepted by the provider.
   * Do not discard it because the physical key is still held; checked close
   * owns the remaining input/presentation drain. */
  if(!rc)return 0;
  if(event.document_revision!=ps_message_revision)return 0;
  if(event.kind==RISC_SCENE_CONTROLS_EVENT&&!event.node&&!event.action&&!event.value){(void)files_resident_manual(&ps_host,RISC_RESIDENT_CHECKPOINT_CONTROLS);ps_message_closing=true;}
  else if(event.kind==RISC_SCENE_SUSPEND_EVENT&&!event.node&&!event.action&&!event.value){ps_exit=true;ps_message_closing=true;}
  else if(event.kind==RISC_SCENE_ACTION_EVENT&&!event.value&&event.action==1&&(event.node==0||event.node==2))ps_message_closing=true;
  if(!ps_message_closing)return 0;
 }
 if(!ps_close_scene())return ps_alive(NULL)?0:-1;
 ps_message_pending=false;ps_message_revision=0;points_scene_begin_session(&ps_model);return 1;
}
void portable_text_adapter_retain(void){ps_retain(NULL);}
bool portable_text_adapter_retained(void){return !ps_alive(NULL);}
int portable_text_adapter_suspend(void){
 if(!ps_alive(NULL))return RISC_TEXT_ENTRY_RETAINED;
 if(ps_text_suspended||ps_ui.token){ps_retain(NULL);return RISC_TEXT_ENTRY_RETAINED;}
 if(!files_scene_background_pause())return RISC_TEXT_ENTRY_AGAIN;
 ps_text_suspended=true;return RISC_TEXT_ENTRY_OK;
}
int portable_text_adapter_resume(void){
 if(!ps_alive(NULL))return RISC_TEXT_ENTRY_RETAINED;
 if(!ps_text_suspended||ps_ui.token){ps_retain(NULL);return RISC_TEXT_ENTRY_RETAINED;}
 ps_text_suspended=false;points_scene_changed(&ps_model);return RISC_TEXT_ENTRY_OK;
}
int portable_text_adapter_exit_pending(void){
 if(!ps_alive(NULL))return -1;
 return ps_exit||ps_launch_pending||!!(ps_host.pending_attention&RISC_RESIDENT_ATTENTION_EXIT);
}
int portable_text_adapter_attention(void){
 if(!ps_alive(NULL)||!ps_text_suspended)return -1;
 if(!files_resident_capture(&ps_host,ps_millis()))return -1;
 return portable_text_adapter_exit_pending()||(!ps_checkpoint.enabled&&!!(ps_host.pending_attention&(RISC_RESIDENT_ATTENTION_ALARM|RISC_RESIDENT_ATTENTION_CONTROLS)));
}
