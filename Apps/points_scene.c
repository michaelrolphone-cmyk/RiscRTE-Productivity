/* Canonical semantic Points application. All rendering, input interpretation,
 * physical lifecycle and device behavior are supplied through public services. */
#include "points_scene_model.h"
#include "points_scene_services.h"
#include "points_scene_storage.h"
#include "points_scene_schedule.h"
#include "PointsSchedule.h"
#include "files_scene_ui.h"
#include "files_scene_resident.h"
#include "files_scene_effects.h"
#include "files_scene_background.h"
#include "PortableTextInputCheckpointClient.h"
#include "RiscAppCatalogV1.h"
static const risc_runtime_api_v1 *ps_runtime;
static bool ps_retained,ps_invoked,ps_clean,ps_exit,ps_background,ps_text_suspended;
static bool ps_text_pending,ps_text_finishing,ps_publish_pending;
static enum points_scene_effect ps_deferred_effect;
static uint32_t ps_idle_at,ps_idle_ms,ps_sample_at;
static bool ps_sampled;
static points_editor ps_editor;
static points_scene_model ps_model;
static points_scene_services ps_services;
static points_scene_legacy ps_legacy;
static files_scene_ui ps_ui;
static files_resident ps_host;
static files_effects ps_settings;
static portable_text_client ps_text;
static portable_text_checkpoint_client ps_checkpoint;
static risc_text_entry_state_v1 ps_text_result;
static risc_components_document_v1 ps_document;
static void ps_retain(void *unused){
 (void)unused;if(ps_retained)return;
 ps_retained=true;ps_editor.store.retained=true;ps_services.terminal=true;
 ps_legacy.terminal=true;ps_ui.terminal=true;ps_host.terminal=true;
 ps_settings.retained=true;ps_text.retained=true;ps_model.accepting=false;
 /* A changed/absent owner table is already revoked. Never call a cached
  * callback through a different invocation, even to request retention. */
 if(risc_runtime_get_api(1)==ps_runtime&&ps_runtime&&
    ps_runtime->struct_size>=RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE&&ps_runtime->retain_invocation)
  (void)ps_runtime->retain_invocation();
}
static bool ps_alive(void *unused){
 (void)unused;if(ps_retained)return false;
 if(risc_runtime_get_api(1)!=ps_runtime){ps_retain(NULL);return false;}
 return true;
}
bool portable_adapter_retained(void){return !ps_alive(NULL);}
void portable_adapter_retain(void){ps_retain(NULL);}
void portable_adapter_retain_silent(void){ps_retain(NULL);}
static uint32_t ps_millis(void){uint32_t n=0;if(!ps_alive(NULL))return 0;bool ok=ps_runtime->monotonic_ms(&n);if(!ps_alive(NULL))return 0;if(!ok)ps_retain(NULL);return n;}
static bool ps_prepare(void *unused){(void)unused;return ps_alive(NULL)&&(!ps_background||files_scene_background_pause())&&ps_alive(NULL);}
static bool ps_close_scene(void){
 bool existed=ps_ui.token!=0;if(!files_ui_close(&ps_ui))return false;
 if(existed||ps_publish_pending){points_scene_begin_session(&ps_model);ps_publish_pending=false;}
 return ps_alive(NULL);
}
static int ps_settle(void *unused){
 (void)unused;if(!ps_alive(NULL))return -1;
 if(ps_text.active&&!ps_checkpoint.paused)return 0;
 if(!ps_prepare(NULL))return ps_alive(NULL)?0:-1;
 return ps_close_scene()?1:ps_alive(NULL)?0:-1;
}
void files_scene_background_idle_update(uint32_t ms){ps_idle_ms=ms;}
static int ps_settings_call(void*unused,uint32_t reason){
 (void)unused;if(!ps_host.enabled)return RISC_RESIDENT_BUSY;
 uint32_t flags=0;int rc=files_resident_call(&ps_host,reason,&flags);
 if(flags){ps_retain(NULL);return RISC_RESIDENT_RETAINED;}return rc;
}
static bool ps_settings_done(void*unused,unsigned effect,unsigned outcome){(void)unused;return files_scene_background_effect_done(effect,outcome);}
static bool ps_settings_step(bool settled){
 int rc=files_effects_step(&ps_settings,files_scene_background_effects(),settled);
 if(rc<0)return false;
 if(rc==2)points_scene_notice(&ps_model,"Host settings effects unavailable");
 if(rc==1)points_scene_changed(&ps_model);
 return ps_alive(NULL);
}
static bool ps_host_step(bool settled){
 uint32_t now=ps_millis();if(!ps_alive(NULL))return false;
 if(ps_ui.flags&(RISC_SCENE_ACTIVITY|RISC_SCENE_INPUT_BUSY))ps_idle_at=now;
 bool due=ps_idle_ms&&!files_scene_background_capture_active()&&!files_scene_background_effects()&&(uint32_t)(now-ps_idle_at)>=ps_idle_ms;
 int rc=files_resident_step(&ps_host,settled,false,due);if(rc<0)return false;
 if(rc==1){ps_idle_at=now;points_scene_changed(&ps_model);ps_sampled=false;
  if(ps_host.exit_requested)ps_exit=true;
  if(ps_host.reply_flags&RISC_RESIDENT_REPLY_CONFIGURATION_CHANGED){
   if(!files_scene_background_idle_timeout(&ps_idle_ms))ps_idle_ms=0;
   if(!ps_alive(NULL))return false;
   if(ps_services.ready){
    unsigned format=ps_services.time_format;
    int loaded=portable_time_format_load(&ps_services.preferences,&format);
    if(!ps_alive(NULL))return false;
    /* Transient/unavailable reads cannot replace the last observed setting.
     * Missing and invalid records retain the public preference's 12h default. */
    if(loaded==PORTABLE_TIME_FORMAT_LOADED||loaded==PORTABLE_TIME_FORMAT_MISSING||loaded==PORTABLE_TIME_FORMAT_INVALID){
     ps_services.time_format=format;ps_model.time_format=format;points_scene_changed(&ps_model);
    }
   }
  }
  if(ps_host.unsupported)points_scene_notice(&ps_model,"Host lifecycle service unavailable");
 }return ps_alive(NULL);
}
#include "points_scene_workflow.inc"
static bool ps_load(void){
 int rc=ps_services.catalog?points_editor_load_guarded(&ps_editor,&ps_services.files,&ps_services.legacy,ps_services.domain,ps_alive,NULL):psl_load(&ps_legacy,&ps_editor,&ps_services.legacy,ps_services.domain);
 if(!ps_alive(NULL))return false;
 if(rc!=RISC_APP_DATA_OK)points_scene_notice(&ps_model,"Saved data unavailable: retry");
 return true;
}
static void ps_refresh(void){
 if(!ps_services.ready||!ps_prepare(NULL))return;
 int rc=ps_services.alarm->refresh(ps_services.alarm->context);
 if(!pss_alarm_result(&ps_services,rc))return;
 if(rc!=ALARM_OK&&rc!=ALARM_PENDING)points_scene_notice(&ps_model,"Saved; scheduler refresh pending");
 ps_sampled=false;
}
static void ps_capabilities(void){
 ps_model.ready=ps_services.ready;ps_model.service_valid=ps_services.ready&&ps_services.valid;
 ps_model.service_busy=pss_busy(&ps_services);ps_model.clock_valid=ps_services.ready&&ps_services.clock_valid;
 ps_model.visual_only=ps_services.visual_only;ps_model.time_format=ps_services.time_format;
 ps_model.event_limit=ps_services.catalog?0:POINTS_MAX;
 ps_model.custom_type_limit=ps_services.catalog?0:POINTS_CUSTOM_COUNT;
 ps_model.type_name_limit=ps_services.catalog?POINTS_CATALOG_NAME_MAX:POINTS_CUSTOM_NAME_MAX;
 ps_model.legacy_type_metadata_only=!ps_services.catalog;ps_model.retry_writes=!ps_services.catalog;
}
static bool ps_schedule(uint32_t seconds){
 points_scene_schedule schedule={0};
 if(ps_services.ready&&ps_services.clock_valid){
  if(ps_services.domain==POINTS_TIME_NATIVE_UTC){
   const points_config *config=!ps_services.catalog&&ps_legacy.writer.loaded?&ps_legacy.writer.saved:NULL;
   if(!points_scene_native_schedule(ps_services.alarm,config,&ps_services.preferences,seconds,
      ps_services.time_format,ps_alive,NULL,&schedule)){ps_retain(NULL);return false;}
   ps_services.clock_valid=schedule.valid;
  }else{
   twatch_rtc_time_v1 local={0};schedule.valid=points_calendar(seconds,&local);
   if(schedule.valid){
    points_catalog_item time={.hour=local.hour,.minute=local.minute};
    points_editor_time(&time,ps_services.time_format,schedule.clock_text,sizeof(schedule.clock_text));
   }
   ps_services.clock_valid=schedule.valid;
  }
 }
 if(ps_services.catalog&&ps_services.valid){
  if(ps_services.projection.count)schedule.next_event_id=ps_services.projection.next[0].event_id;
  if(!schedule.status[0]){
   const char *notice=ps_services.projection.flags&POINTS_FLAG_GAP?"DST gap: start skipped":
    ps_services.projection.flags&POINTS_FLAG_FOLD?"DST fold: start skipped":
    ps_services.projection.flags&POINTS_FLAG_RANGE?"Time range limit":"";
   snprintf(schedule.status,sizeof(schedule.status),"%s",notice);
  }
 }
 if(strcmp(ps_model.clock_text,schedule.clock_text)||strcmp(ps_model.schedule_status,schedule.status)||ps_model.next_event_id!=schedule.next_event_id)
  points_scene_changed(&ps_model);
 memcpy(ps_model.clock_text,schedule.clock_text,sizeof(ps_model.clock_text));
 memcpy(ps_model.schedule_status,schedule.status,sizeof(ps_model.schedule_status));
 ps_model.next_event_id=schedule.next_event_id;return ps_alive(NULL);
}
static bool ps_sample(void){
 /* Preserve a pending document and its bindings until publication completes.
  * Any focus boundary closes/discards the pending candidate explicitly. */
 if(ps_publish_pending)return true;
 uint32_t now=ps_millis();if(!ps_alive(NULL))return false;
 if(ps_sampled&&(uint32_t)(now-ps_sample_at)<1000)return true;
 ps_sample_at=now;ps_sampled=true;
 bool ready=ps_model.ready,valid=ps_model.service_valid,busy=ps_model.service_busy,clock=ps_model.clock_valid;
 bool visual=ps_model.visual_only;unsigned format=ps_model.time_format;
 uint32_t seconds=0;
 if(ps_services.ready){(void)pss_status(&ps_services);if(!ps_alive(NULL))return false;(void)pss_now(&ps_services,&seconds);}
 if(!ps_alive(NULL)||!ps_schedule(seconds))return false;
 ps_capabilities();
 if(ready!=ps_model.ready||valid!=ps_model.service_valid||busy!=ps_model.service_busy||clock!=ps_model.clock_valid||visual!=ps_model.visual_only||format!=ps_model.time_format)points_scene_changed(&ps_model);
 return true;
}
static bool ps_retry(void){
 if(!ps_services.ready){
  if(!pss_close(&ps_services))return false;
  (void)pss_open(&ps_services);if(!ps_alive(NULL))return false;
  ps_capabilities();points_scene_changed(&ps_model);
  if(ps_services.ready&&!ps_load())return false;
 }else if(ps_editor.store.uncertain){
  unsigned action=ps_services.catalog?ps_editor.pending_action:ps_legacy.pending_action;
  int rc=ps_services.catalog?points_editor_resolve(&ps_editor,&ps_services.files):psl_retry(&ps_legacy,&ps_editor,&ps_services.legacy);
  if(!ps_alive(NULL))return false;
  points_scene_saved(&ps_model,rc,action);if(rc==RISC_APP_DATA_OK)ps_refresh();
 }else if(ps_editor.conflict||!ps_editor.store.loaded){if(!ps_load())return false;}
 else if(!ps_editor.index_ready)(void)points_editor_reindex(&ps_editor);
 if(!ps_alive(NULL))return false;
 points_scene_retry_done(&ps_model);ps_sampled=false;return true;
}
static bool ps_effect(enum points_scene_effect effect){
 if(effect==POINTS_SCENE_EFFECT_NONE)return true;
 if(effect==POINTS_SCENE_EFFECT_EXIT){ps_exit=true;return true;}
 if(effect==POINTS_SCENE_EFFECT_NAME){ps_text_pending=true;return true;}
 if(effect==POINTS_SCENE_EFFECT_RETRY)return ps_retry();
 unsigned action=effect==POINTS_SCENE_EFFECT_SAVE_EVENT?POINTS_EDITOR_EVENT:effect==POINTS_SCENE_EFFECT_DELETE_EVENT?POINTS_EDITOR_DELETE:effect==POINTS_SCENE_EFFECT_SAVE_TYPE?POINTS_EDITOR_TYPE:POINTS_EDITOR_DELETE_TYPE;
 uint32_t now=0;int rc=RISC_APP_DATA_UNAVAILABLE;
 bool event_action=action==POINTS_EDITOR_EVENT||action==POINTS_EDITOR_DELETE;
 if(ps_services.ready&&pss_status(&ps_services)&&!pss_busy(&ps_services)&&(!event_action||(pss_now(&ps_services,&now)&&ps_schedule(now)&&ps_services.clock_valid))){
  if(!ps_alive(NULL))return false;
  if(ps_services.catalog)rc=(action==POINTS_EDITOR_EVENT||action==POINTS_EDITOR_DELETE)?points_editor_save_event(&ps_editor,&ps_services.files,now,action==POINTS_EDITOR_DELETE):points_editor_save_type(&ps_editor,&ps_services.files,action==POINTS_EDITOR_DELETE_TYPE);
  else rc=psl_save(&ps_legacy,&ps_editor,&ps_services.legacy,now,action);
 }
 if(!ps_alive(NULL))return false;
 points_scene_saved(&ps_model,rc,action);if(rc==RISC_APP_DATA_OK)ps_refresh();ps_sampled=false;return ps_alive(NULL);
}
static bool ps_publish(void){
 if(!ps_model.dirty&&ps_ui.token&&!ps_publish_pending)return true;
 if(!ps_publish_pending){
  if(ps_ui.token){
   if(!files_ui_snapshot(&ps_ui))return false;
   if(ps_ui.flags&(RISC_SCENE_PRESENTING|RISC_SCENE_CLOSING|RISC_SCENE_INPUT_BUSY))return false;
  }
  if(!points_scene_build(&ps_model,&ps_document)){ps_retain(NULL);return false;}
  ps_publish_pending=true;
 }
 int rc=files_ui_publish(&ps_ui,&ps_document,ps_host.enabled);
 if(rc==1){
  if(!points_scene_commit(&ps_model,ps_document.revision)){ps_retain(NULL);return false;}
  ps_publish_pending=false;
 }
 return rc==1;
}
static bool ps_text_step(void){
 if(ps_text_pending){
  if(!ps_prepare(NULL)||!ps_close_scene())return ps_alive(NULL);
  /* A busy optional editor must not starve explicit host attention. No text
   * session owns focus yet, so the normal settled host path is safe. */
  if(!ps_host_step(true))return false;
  if(ps_exit||ps_launch_pending||ps_host.phase)return true;
  int rc=portable_text_client_begin(&ps_text,ps_runtime,"Type name",ps_editor.type.name,ps_model.type_name_limit+1);
  if(!ps_alive(NULL))return false;
  if(rc==RISC_TEXT_ENTRY_AGAIN||rc==RISC_TEXT_ENTRY_BUSY)return true;
  ps_text_pending=false;
  if(rc!=RISC_TEXT_ENTRY_OK){(void)points_scene_name_result(&ps_model,false,NULL);points_scene_notice(&ps_model,"Text entry unavailable");return true;}
  ps_checkpoint=(portable_text_checkpoint_client){0};(void)portable_text_checkpoint_enable(&ps_text,&ps_checkpoint);return ps_alive(NULL);
 }
 if(ps_text_finishing){
  int rc=portable_text_client_close(&ps_text);if(!ps_alive(NULL))return false;if(rc==RISC_TEXT_ENTRY_AGAIN)return true;
  if(rc!=RISC_TEXT_ENTRY_OK){ps_retain(NULL);return false;}
  ps_text_finishing=false;ps_checkpoint=(portable_text_checkpoint_client){0};
  if(ps_text_result.flags&RISC_TEXT_ENTRY_HOME_CANCEL)ps_exit=true;
  (void)points_scene_name_result(&ps_model,ps_text_result.state==RISC_TEXT_ENTRY_ACCEPTED,ps_text_result.text);
  memset(&ps_text_result,0,sizeof(ps_text_result));ps_sampled=false;return true;
 }
 if(ps_checkpoint.enabled){
  if(ps_checkpoint.paused){
   if(ps_exit||ps_launch_pending){ps_text_finishing=true;return true;}
   int message=ps_message_step();if(message<0)return false;if(!message)return true;
   if(!ps_settings_step(true))return false;if(ps_settings.phase)return true;
   if(!ps_host_step(true))return false;if(ps_host.phase)return true;
   if(ps_exit||ps_launch_pending){ps_text_finishing=true;return true;}
   int rc=portable_text_checkpoint_resume(&ps_text,&ps_checkpoint,RISC_TEXT_ENTRY_RESUME_REDRAW|RISC_TEXT_ENTRY_RESUME_FOCUS_CHANGED);
   return ps_alive(NULL)&&rc==RISC_TEXT_ENTRY_OK;
  }
  if(ps_checkpoint.pausing){(void)portable_text_checkpoint_pause_mode(&ps_text,&ps_checkpoint,false);return ps_alive(NULL);}
  risc_text_entry_checkpoint_state_v1 state={0};int rc=portable_text_checkpoint_snapshot(&ps_text,&ps_checkpoint,&state);
  if(!ps_alive(NULL))return false;
  if(rc==RISC_TEXT_ENTRY_OK){
   if(state.flags&(RISC_TEXT_ENTRY_CHECKPOINT_ACTIVITY|RISC_TEXT_ENTRY_CHECKPOINT_INPUT_BUSY))ps_idle_at=ps_millis();
   if(!(state.flags&RISC_TEXT_ENTRY_CHECKPOINT_PRESENTING)){
    if(!files_scene_background_step(true))return false;
    bool due=ps_idle_ms&&!files_scene_background_capture_active()&&(uint32_t)(ps_millis()-ps_idle_at)>=ps_idle_ms;
    if(!(state.flags&RISC_TEXT_ENTRY_CHECKPOINT_INPUT_BUSY)&&(ps_message_pending||ps_launch_pending||files_scene_background_effects()||ps_host.pending_attention||ps_host.phase||due)){
     (void)portable_text_checkpoint_pause_mode(&ps_text,&ps_checkpoint,false);return ps_alive(NULL);
    }
   }
  }
 }
 risc_text_entry_state_v1 state={0};int rc=portable_text_client_poll(&ps_text,&state);
 if(!ps_alive(NULL))return false;
 if(rc==RISC_TEXT_ENTRY_OK&&state.state!=RISC_TEXT_ENTRY_EDITING){ps_text_result=state;ps_text_finishing=true;}
 return true;
}
static bool ps_open(void){
 if(ps_invoked&&!ps_clean){ps_retain(NULL);return false;}ps_invoked=true;ps_clean=false;
 memset(&ps_editor,0,sizeof(ps_editor));memset(&ps_legacy,0,sizeof(ps_legacy));memset(&ps_text,0,sizeof(ps_text));
 ps_exit=ps_sampled=ps_text_pending=ps_text_finishing=ps_text_suspended=ps_publish_pending=ps_background=false;
 ps_checkpoint=(portable_text_checkpoint_client){0};ps_text_result=(risc_text_entry_state_v1){0};
 ps_idle_at=ps_idle_ms=ps_sample_at=0;ps_deferred_effect=POINTS_SCENE_EFFECT_NONE;
 ps_message_pending=ps_message_closing=ps_launch_pending=false;ps_message_revision=0;
 points_scene_init(&ps_model,&ps_editor);
 files_ui_init(&ps_ui,ps_runtime,ps_alive,ps_retain,NULL);
 pss_init(&ps_services,ps_runtime,ps_alive,ps_retain,ps_prepare,NULL);psl_set_alive(&ps_legacy,ps_alive,NULL);
 memset(&ps_host,0,sizeof(ps_host));memset(&ps_settings,0,sizeof(ps_settings));
 int available=files_ui_bind(&ps_ui);
 if(available!=1){if(available==0&&files_ui_release(&ps_ui))ps_clean=true;return false;}
 if(!files_scene_background_init(ps_runtime,ps_alive,ps_retain,NULL)){ps_retain(NULL);return false;}ps_background=true;
 if(!files_resident_init(&ps_host,ps_runtime,ps_alive,ps_retain,ps_settle,NULL))return false;
 files_effects_init(&ps_settings,ps_alive,ps_retain,ps_settle,ps_settings_call,ps_settings_done,NULL);
 (void)pss_open(&ps_services);if(!ps_alive(NULL))return false;
 ps_capabilities();
 if(ps_services.ready&&!ps_load())return false;
 ps_idle_at=ps_millis();ps_idle_ms=0;(void)files_scene_background_idle_timeout(&ps_idle_ms);return ps_alive(NULL);
}
static bool ps_close(void){
 if(!ps_prepare(NULL))return false;
 /* A paused text session can coexist with a workflow scene. Drain that
  * scene first so the public text handoff can return focus cleanly. */
 if(!ps_close_scene())return false;
 if(ps_text.active||ps_text.acquired||ps_text.suspended){int rc=portable_text_client_close(&ps_text);if(!ps_alive(NULL)||rc!=RISC_TEXT_ENTRY_OK)return false;}
 if(!pss_close(&ps_services))return false;
 if(ps_background&&!files_scene_background_close())return false;
 ps_background=false;
 if(!files_ui_release(&ps_ui))return false;
 points_editor_dispose(&ps_editor);if(!ps_alive(NULL))return false;
 ps_checkpoint=(portable_text_checkpoint_client){0};ps_clean=true;return true;
}
__attribute__((visibility("default"))) const risc_resident_app_descriptor_v1_t risc_resident_app_descriptor_v1={1,sizeof(risc_resident_app_descriptor_v1_t),RISC_RESIDENT_ROLE_FOREGROUND,0};
__attribute__((visibility("default"))) void app_main(void){
 const risc_runtime_api_v1 *rt=risc_runtime_get_api(1);
 if(!rt||rt->api_version!=1||rt->struct_size<RISC_RUNTIME_MONOTONIC_V1_SIZE||!rt->acquire||!rt->release||!rt->yield_ms||!rt->monotonic_ms||!rt->retain_invocation)return;
 if(ps_retained||(ps_invoked&&!ps_clean)){ps_retain(NULL);return;}ps_runtime=rt;
 if(!ps_open()){
  while(ps_alive(NULL)&&!ps_clean&&!ps_close())rt->yield_ms(1);
  return;
 }
 while(ps_alive(NULL)&&!ps_exit&&!ps_launch_pending){
  if(!files_scene_background_capture()||!files_resident_capture(&ps_host,ps_millis()))break;
  if(ps_text_pending||ps_text.active||ps_text_finishing){if(!ps_text_step())break;rt->yield_ms(1);continue;}
  if(ps_message_pending){if(ps_message_step()<0)break;rt->yield_ms(1);continue;}
  risc_scene_event_v1 event={0};int next=files_ui_next(&ps_ui,&event);if(next<0)break;
  if(next==1&&event.document_revision==ps_model.committed_revision){
   ps_idle_at=ps_millis();
   if(event.kind==RISC_SCENE_CONTROLS_EVENT&&!event.node&&!event.action&&!event.value)(void)files_resident_manual(&ps_host,RISC_RESIDENT_CHECKPOINT_CONTROLS);
   else if(event.kind==RISC_SCENE_SUSPEND_EVENT&&!event.node&&!event.action&&!event.value)ps_exit=true;
   else {
    enum points_scene_effect effect=points_scene_reduce(&ps_model,&event);
    if(effect==POINTS_SCENE_EFFECT_RETRY||(effect>=POINTS_SCENE_EFFECT_SAVE_EVENT&&effect<=POINTS_SCENE_EFFECT_DELETE_TYPE))ps_deferred_effect=effect;
    else if(!ps_effect(effect))break;
   }
  }
  if(ps_exit||ps_text_pending)continue;
  if(ps_ui.token&&!files_ui_snapshot(&ps_ui))break;
  bool settled=!(ps_ui.flags&(RISC_SCENE_PRESENTING|RISC_SCENE_CLOSING|RISC_SCENE_INPUT_BUSY));
  if(ps_host.phase){if(!ps_host_step(settled))break;rt->yield_ms(1);continue;}
  /* Semantic intent is accepted once, but storage/service effects wait for
   * the presenter's confirmed settled boundary. Held input is not replayed. */
  if(ps_deferred_effect){
   if(settled){
    if(ps_effect(ps_deferred_effect))ps_deferred_effect=POINTS_SCENE_EFFECT_NONE;
    else if(!ps_alive(NULL))break;
   }
   if(ps_deferred_effect){rt->yield_ms(1);continue;}
  }
  if(!files_scene_background_step(settled)||!ps_settings_step(settled)||!ps_host_step(settled))break;
  if(ps_settings.phase||ps_host.phase||ps_message_pending||ps_exit||ps_launch_pending){rt->yield_ms(1);continue;}
  if(settled&&!ps_sample())break;
  (void)ps_publish();if(ps_alive(NULL))rt->yield_ms(1);
 }
 if(!ps_alive(NULL))return;
 while(ps_alive(NULL)&&!ps_close())rt->yield_ms(1);
 if(ps_alive(NULL)&&ps_clean&&ps_launch_pending){ps_launch_pending=false;bool ok=rt->request_launch&&rt->request_launch(ps_workflow_launch);if(ps_alive(NULL)&&!ok&&rt->diagnostic)rt->diagnostic("Points workflow launch refused after cleanup");}
}
