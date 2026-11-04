/* Production root + pinned public System helpers; only provider endpoints are fake.
 * Run each named case in a fresh process to preserve terminal custody semantics. */
#include "points_scene.c"
#include <assert.h>
#include "ContextFingerprintService.h"
static risc_runtime_api_v1 runtime,other;
static const risc_runtime_api_v1 *identity;
static unsigned calls,retains,releases,opens,updates,closes,writes,refreshes,yields;
static uint32_t now=100,scene_flags,text_flags,last_reason,host_flags;
static int format_rc=RISC_KEY_VALUE_NOT_FOUND;static unsigned format_value;static bool revoke_format;
static bool no_scene,revoke_publish,close_again,script_exit;
static unsigned pause_calls,resume_calls,host_calls;
static int pause_rc,text_close_rc,text_open_rc;
static bool api2,broken_checkpoint,no_clock,publish_busy,revoke_write,catalog_mode,background_mode;static bool emitted_setting;static unsigned settings_reports;
static unsigned file_writes,realtime_reads;static uint8_t file_blob[65536];static uint32_t file_length;static uint64_t file_revision;
static risc_components_document_v1 attempted;
static risc_scene_event_v1 injected;static unsigned alarm_v1,alarm_v2,enabled_calls;
static risc_components_document_v1 shown;
static risc_text_entry_state_v1 text_result;
static uint8_t blobs[3][512];static uint32_t lengths[3];
static void io(void){assert(identity==&runtime&&!ps_retained);calls++;}
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1?identity:NULL;}
static bool retain(void){retains++;return true;}
static bool monotonic(uint32_t *n){io();*n=now;return true;}
static bool health(risc_runtime_health_v1 *h){io();h->uptime_ms=now;return true;}
static void yield(uint32_t ms){io();now+=ms;assert(++yields<100);if(script_exit&&ps_ui.token)ps_exit=true;if(close_again)close_again=false;}
static int scene_open(void*c,const risc_components_document_v1*d,const risc_scene_navigation_v1*n,uint64_t*t){(void)c;(void)n;io();shown=*d;opens++;*t=7;if(revoke_publish)identity=&other;return 0;}
static int scene_update(void*c,uint64_t t,const risc_components_document_v1*d){(void)c;assert(t==7);io();assert(d->revision>shown.revision);attempted=*d;if(publish_busy)return RISC_SCENE_BUSY;shown=*d;updates++;return 0;}
static int scene_next(void*c,uint64_t t,risc_scene_event_v1*e){(void)c;(void)t;io();if(injected.sequence){*e=injected;memset(&injected,0,sizeof(injected));return 0;}return RISC_SCENE_IDLE;}
static int scene_snapshot(void*c,uint64_t t,risc_scene_navigation_v1*n,uint32_t*f){(void)c;assert(t==7);io();n->depth=1;n->routes[0]=POINTS_SCENE_ROUTE;*f=scene_flags;return 0;}
static int scene_close(void*c,uint64_t t){(void)c;assert(t==7);io();closes++;return close_again?RISC_SCENE_AGAIN:0;}
static risc_scene_components_api_v1 scene;
static int key_index(const char*k){return !strcmp(k,"points_cfg")?0:!strcmp(k,"points_meta")?1:!strcmp(k,"quick_radio")?2:-1;}
static int kv_get(void*c,const char*k,void*b,uint32_t cap,uint32_t*n){(void)c;io();if(!strcmp(k,PORTABLE_TIME_FORMAT_KEY)){*n=0;if(revoke_format)identity=&other;if(format_rc)return format_rc;assert(cap>=4);uint8_t bytes[]={0x54,1,(uint8_t)format_value,(uint8_t)(format_value^0xa5u)};memcpy(b,bytes,4);*n=4;return 0;}int i=key_index(k);*n=0;if(i<0||!lengths[i])return RISC_KEY_VALUE_NOT_FOUND;*n=lengths[i];if(cap<*n)return RISC_KEY_VALUE_BUFFER_SMALL;memcpy(b,blobs[i],*n);return 0;}
static int kv_put(void*c,const char*k,const void*b,uint32_t n){(void)c;io();int i=key_index(k);assert(i>=0&&n<=512);memcpy(blobs[i],b,n);lengths[i]=n;writes++;if(revoke_write)identity=&other;return 0;}
static risc_key_value_v1 kv={1,sizeof(kv),NULL,kv_get,kv_put};
static int alarm_status(void*c,alarm_status_v1*s){(void)c;io();*s=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*s),.state=ALARM_STATE_READY};return 0;}
static int alarm_refresh(void*c){(void)c;io();refreshes++;return 0;}
static alarm_service_v1 alarm_api={.api_version=1,.struct_size=sizeof(alarm_api),.status=alarm_status,.refresh=alarm_refresh};
static int alarm_ack(void*c,const alarm_token_v1*t){(void)c;(void)t;io();return 0;}
static int alarm_sleep(void*c,alarm_sleep_v1*t){(void)c;(void)t;io();return 0;}
static alarm_service_descriptor_v2 alarm2;
static int project(void*c,points_catalog_projection*p){(void)c;io();*p=(points_catalog_projection){.struct_size=sizeof(*p)};return 0;}
static points_service_v2 catalog_alarm;
static int realtime_read(void*c,risc_realtime_snapshot_v1*s){(void)c;io();realtime_reads++;*s=(risc_realtime_snapshot_v1){.struct_size=sizeof(*s),.validity=RISC_REALTIME_VALID,.epoch_seconds=1793793600,.monotonic_before_us=100000,.monotonic_after_us=100001};return 0;}
static risc_realtime_api_v1 realtime_api={1,sizeof(realtime_api),&runtime,realtime_read};
static int file_stat(void*c,const char*n,uint32_t*z,uint64_t*r){(void)c;(void)n;io();*z=file_length;*r=file_revision;return file_revision?0:RISC_APP_DATA_NOT_FOUND;}
static int file_read(void*c,const char*n,uint64_t expected,void*b,uint32_t cap,uint32_t*z,uint64_t*r){(void)c;(void)n;io();*z=0;*r=0;if(expected!=file_revision)return RISC_APP_DATA_STALE;if(!file_revision)return RISC_APP_DATA_NOT_FOUND;*z=file_length;*r=file_revision;if(cap<file_length)return RISC_APP_DATA_BUFFER_SMALL;memcpy(b,file_blob,file_length);return 0;}
static int file_replace(void*c,const char*n,uint64_t expected,const void*b,uint32_t z){(void)c;(void)n;io();if(expected!=file_revision)return RISC_APP_DATA_STALE;assert(z<=sizeof(file_blob));memcpy(file_blob,b,z);file_length=z;file_revision++;file_writes++;return 0;}
static risc_app_data_v1 file_api={1,sizeof(file_api),NULL,file_stat,file_read,file_replace};
static bool rtc_read(void*c,twatch_rtc_time_v1*t){(void)c;io();*t=(twatch_rtc_time_v1){2026,11,4,3,12,0,0};return true;}
static twatch_rtc_api_v1 rtc={.api_version=2,.struct_size=sizeof(rtc),.read=rtc_read};
static int text_open(void*c,const risc_text_entry_request_v1*q,uint64_t*t){(void)c;io();assert(q->capacity>1);if(!text_open_rc)*t=9;return text_open_rc;}
static int text_poll(void*c,uint64_t t,risc_text_entry_state_v1*s){(void)c;assert(t==9);io();*s=text_result;return 0;}
static int text_close(void*c,uint64_t t){(void)c;assert(t==9);io();return text_close_rc;}
static risc_text_entry_api_v1 text_api={1,sizeof(text_api),NULL,text_open,text_poll,text_close};
static int text_enable(void*c,uint64_t t){(void)c;assert(t==9);io();enabled_calls++;return 0;}
static int text_snapshot(void*c,uint64_t t,risc_text_entry_checkpoint_state_v1*s){(void)c;assert(t==9);io();s->flags=text_flags;return 0;}
static int text_pause(void*c,uint64_t t){(void)c;assert(t==9);io();pause_calls++;return pause_rc;}
static int text_resume(void*c,uint64_t t,uint32_t f){(void)c;assert(t==9);io();assert(f==3);resume_calls++;return 0;}
static int ext_open(void*c,const risc_text_entry_request_v1_extended*q,uint64_t*t){(void)c;(void)q;(void)t;assert(0);return -1;}
static int ext_poll(void*c,uint64_t t,risc_text_entry_state_v1_extended*q){(void)c;(void)t;(void)q;assert(0);return -1;}
static int long_open(void*c,const risc_text_entry_request_v1_long*q,uint64_t*t){(void)c;(void)q;(void)t;assert(0);return -1;}
static int long_poll(void*c,uint64_t t,risc_text_entry_state_v1_long*q){(void)c;(void)t;(void)q;assert(0);return -1;}
static risc_text_entry_api_v1_checkpoint checkpoint={.checkpoint_enable=text_enable,.checkpoint_snapshot=text_snapshot,.pause=text_pause,.resume=text_resume,.pause_for_focus_change=text_pause};
static int host_checkpoint(uint64_t t,const risc_resident_request_v1*q,risc_resident_reply_v1*r){(void)r;assert(t==1);io();assert(!ps_ui.token&&(!ps_text.active||ps_checkpoint.paused));last_reason=q->reason;host_calls++;r->flags=host_flags;return 0;}
static bool context_step(void*c,const contexts_policy_v1*p){(void)c;(void)p;io();return true;}
static bool context_pause(void*c){(void)c;io();return true;}
static bool context_status(void*c,contexts_status_v1*s){(void)c;io();*s=(contexts_status_v1){.struct_size=sizeof(*s)};return true;}
static bool context_claim(void*c,uint32_t source,uint32_t slot,const char*n,uint32_t generation){(void)c;(void)source;(void)slot;(void)n;(void)generation;assert(0);return false;}
static bool context_report(void*c,uint32_t source,uint32_t generation,uint32_t result){(void)c;(void)source;(void)generation;(void)result;assert(0);return false;}
static bool fingerprint(void*c,uint32_t op,void*q){(void)c;io();if(op==CONTEXTS_FP_EVALUATE&&!emitted_setting){contexts_evaluate_v1*e=q;e->result.changed=1u<<CR_WIFI;e->result.winners[CR_WIFI]=0;e->result.settings[CR_WIFI]=0;emitted_setting=true;}else if(op==CONTEXTS_FP_WORKFLOW){contexts_workflow_v1*w=q;if(!w->claim)settings_reports++;}return true;}
static contexts_fingerprint_service_v1 contexts={.base={.api_version=1,.struct_size=sizeof(contexts),.step=context_step,.pause=context_pause,.status=context_status,.claim_preset=context_claim,.preset_result=context_report,.capture_audio=context_pause},.fingerprint_abi=0x31504643u,.fingerprint=fingerprint};
static bool acquire(const char*n,uint32_t v,uint64_t i,risc_runtime_capability_v1*g){io();const void*a=NULL;
 if(!strcmp(n,RISC_SCENE_CAPABILITY)&&!no_scene)a=&scene;
 else if(!strcmp(n,RISC_KEY_VALUE_CAPABILITY)&&(i==1||i==5))a=&kv;
 else if(!strcmp(n,ALARM_SERVICE_CAPABILITY)){if(v==1){alarm_v1++;if(!api2)a=&alarm_api;}else if(v==2){alarm_v2++;if(api2)a=catalog_mode?(const void*)&catalog_alarm:(const void*)&alarm2;}}
 else if(!strcmp(n,"rtc.clock")&&v==2&&!no_clock)a=&rtc;
 else if(!strcmp(n,CONTEXTS_SERVICE_CAPABILITY)&&background_mode)a=&contexts;
 else if(!strcmp(n,RISC_SHARED_DATA_CAPABILITY)&&background_mode&&i==4)a=&file_api;
 else if(!strcmp(n,RISC_APP_DATA_CAPABILITY)&&catalog_mode)a=&file_api;
 else if(!strcmp(n,RISC_REALTIME_CAPABILITY)&&api2)a=&realtime_api;
 else if(!strcmp(n,RISC_TEXT_ENTRY_CAPABILITY))a=&checkpoint;
 if(!a)return false;g->api=a;g->slot=1;g->generation=1;return true;}
static bool release(risc_runtime_capability_v1*g){io();releases++;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
static void setup(void){runtime=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(runtime),.acquire=acquire,.release=release,.yield_ms=yield,.monotonic_ms=monotonic,.health=health,.retain_invocation=retain};identity=ps_runtime=&runtime;
 scene=(risc_scene_components_api_v1){.tag=RISC_COMPONENTS_TAG,.version=1,.open=scene_open,.update=scene_update};scene.lifecycle.base=(risc_scene_api_v1){.api_version=1,.struct_size=sizeof(scene),.next=scene_next,.snapshot=scene_snapshot,.close=scene_close};
 alarm2=(alarm_service_descriptor_v2){.base={.api_version=2,.struct_size=sizeof(alarm2),.status=alarm_status,.step=alarm_refresh,.refresh=alarm_refresh,.acknowledge=alarm_ack,.prepare_sleep=alarm_sleep,.stop_only=alarm_refresh},.tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=1,.output_modes=ALARM_MODE_BOTH};
 catalog_alarm=(points_service_v2){.base=alarm2,.points={POINTS_SERVICE_PROJECTION_TAG,1,project}};catalog_alarm.base.base.struct_size=sizeof(catalog_alarm);
 checkpoint.base.base.base.base.base=text_api;checkpoint.base.base.base.base.base.struct_size=sizeof(checkpoint);
 checkpoint.base.base.base.base.home_reason_tag=RISC_TEXT_ENTRY_HOME_REASON_TAG;checkpoint.base.base.base.base.home_reason_version=1;
 checkpoint.base.base.base.masked_tag=RISC_TEXT_ENTRY_MASKED_TAG;checkpoint.base.base.base.masked_version=1;
 checkpoint.base.base.extended_tag=RISC_TEXT_ENTRY_EXTENDED_TAG;checkpoint.base.base.extended_version=1;checkpoint.base.base.open_extended=ext_open;checkpoint.base.base.poll_extended=ext_poll;
 checkpoint.base.long_tag=RISC_TEXT_ENTRY_LONG_TAG;checkpoint.base.long_version=1;checkpoint.base.open_long=long_open;checkpoint.base.poll_long=long_poll;
 checkpoint.checkpoint_tag=RISC_TEXT_ENTRY_CHECKPOINT_TAG;checkpoint.checkpoint_version=1;
 text_result=(risc_text_entry_state_v1){.struct_size=sizeof(text_result),.state=RISC_TEXT_ENTRY_EDITING,.revision=1};}
static void open_root(void){assert(ps_open());assert(ps_services.ready&&ps_editor.store.loaded&&!writes);assert(ps_sample());assert(ps_publish());assert(opens==1&&shown.node_count&&ps_model.committed_revision==shown.revision);}
static void host_setup(void){ps_host.enabled=true;ps_host.client.base=(risc_resident_client_v1){.api_version=1,.struct_size=sizeof(risc_resident_client_v1),.role=RISC_RESIDENT_ROLE_FOREGROUND,.invocation=1,.checkpoint=host_checkpoint};}
static void name_setup(void){assert(points_editor_begin_type(&ps_editor,0));points_scene_page_to(&ps_model,POINTS_SCENE_NAME);ps_model.pending_effect=POINTS_SCENE_EFFECT_NAME;ps_text_pending=true;assert(ps_text_step());assert(ps_text.active&&!ps_ui.token);assert(ps_checkpoint.enabled==!broken_checkpoint);assert(enabled_calls==!broken_checkpoint);}
int main(int argc,char**argv){assert(argc==2);setup();const char*c=argv[1];
 if(!strcmp(c,"api2")){api2=true;assert(ps_open()&&ps_services.ready&&ps_services.domain==POINTS_TIME_NATIVE_UTC&&alarm_v2==1&&!alarm_v1);assert(ps_close());}
 else if(!strcmp(c,"api1")){assert(ps_open()&&ps_services.ready&&ps_services.domain==POINTS_TIME_RAW_RTC&&alarm_v2==1&&alarm_v1==1);assert(ps_close());}
 else if(!strcmp(c,"no-scene")){no_scene=true;app_main();assert(ps_clean&&!ps_retained&&!opens&&!writes);}
 else if(!strcmp(c,"loop-reentry")){script_exit=true;app_main();assert(ps_clean&&!ps_retained&&opens==1&&closes==1);app_main();assert(ps_clean&&!ps_retained&&opens==2&&closes==2);}
 else if(!strcmp(c,"publish-revoked")){assert(ps_open());revoke_publish=true;assert(!ps_publish()&&ps_retained);unsigned n=calls,r=releases;app_main();assert(calls==n&&!closes&&releases==r);}
 else {if(!strncmp(c,"format-",7)){format_rc=0;format_value=PORTABLE_TIME_FORMAT_24;}if(!strcmp(c,"background-settings"))background_mode=true;if(!strcmp(c,"catalog-save-native")){api2=catalog_mode=true;}open_root();
 if(!strncmp(c,"format-",7)){assert(ps_services.time_format==PORTABLE_TIME_FORMAT_24);host_setup();host_flags=RISC_RESIDENT_REPLY_CONFIGURATION_CHANGED;format_value=PORTABLE_TIME_FORMAT_12;
 if(!strcmp(c,"format-io"))format_rc=RISC_KEY_VALUE_IO;
 else if(!strcmp(c,"format-missing"))format_rc=RISC_KEY_VALUE_NOT_FOUND;
 else if(!strcmp(c,"format-revoked"))revoke_format=true;
 assert(files_resident_manual(&ps_host,RISC_RESIDENT_CHECKPOINT_CONTROLS));bool ok=ps_host_step(true);assert(!writes&&host_calls==1);
 if(revoke_format){assert(!ok&&ps_retained&&ps_services.time_format==PORTABLE_TIME_FORMAT_24);unsigned n=calls;assert(!ps_close()&&calls==n);printf("%s PASS\n",c);return 0;}
 assert(ok&&!ps_retained&&ps_services.time_format==(!strcmp(c,"format-io")?PORTABLE_TIME_FORMAT_24:PORTABLE_TIME_FORMAT_12));assert(ps_model.time_format==ps_services.time_format);
 }
 else if(!strcmp(c,"background-settings")){host_setup();assert(files_scene_background_step(true));assert(emitted_setting&&settings_reports==1&&writes==1&&files_scene_background_effects()==(1u<<RISC_SETTINGS_WIFI));for(unsigned i=0;i<4;i++)assert(ps_settings_step(true));assert(!files_scene_background_effects()&&host_calls==3&&last_reason==RISC_SETTINGS_RESULT(RISC_SETTINGS_WIFI,RISC_SETTINGS_APPLIED)&&!ps_ui.token);}
 else if(!strcmp(c,"catalog-save-native")){assert(ps_services.catalog&&ps_services.clock_valid&&realtime_reads);assert(points_editor_begin_event(&ps_editor,0));ps_editor.event.hour=14;points_scene_page_to(&ps_model,POINTS_SCENE_EDIT);assert(ps_effect(POINTS_SCENE_EFFECT_SAVE_EVENT));assert(file_writes&&refreshes==1&&!writes);uint32_t id=ps_editor.event.id;assert(ps_load());assert(points_catalog_find_event(&ps_editor.store.saved,id)->hour==14);}
 else
 if(!strcmp(c,"publish-state")){uint32_t revision=shown.revision;assert(ps_publish()&&!updates);ps_model.dirty=true;scene_flags=RISC_SCENE_PRESENTING;assert(!ps_publish()&&shown.revision==revision);scene_flags=0;assert(ps_publish()&&updates==1&&shown.revision>revision);}
 else if(!strcmp(c,"publish-busy-candidate")){ps_model.dirty=true;publish_busy=true;assert(!ps_publish());risc_components_document_v1 candidate=attempted;points_scene_notice(&ps_model,"Changed while busy");publish_busy=false;assert(ps_publish());assert(!memcmp(&candidate,&shown,sizeof(shown)));assert(ps_model.committed_revision==candidate.revision&&ps_model.dirty);assert(ps_publish());}
 else if(!strcmp(c,"save-refresh")){assert(points_editor_begin_event(&ps_editor,0));points_scene_page_to(&ps_model,POINTS_SCENE_EDIT);assert(ps_effect(POINTS_SCENE_EFFECT_SAVE_EVENT));assert(writes&&refreshes==1&&ps_editor.store.saved.event_count==8);unsigned count=writes;assert(ps_load()&&ps_editor.store.saved.event_count==8&&writes==count);}
 else if(!strcmp(c,"type-save-no-clock")){assert(points_editor_begin_type(&ps_editor,POINTS_CUSTOM_1));strcpy(ps_editor.type.name,"Custom");points_scene_page_to(&ps_model,POINTS_SCENE_CUSTOM);no_clock=true;assert(ps_effect(POINTS_SCENE_EFFECT_SAVE_TYPE));assert(writes&&refreshes==1&&!ps_editor.editing_type);}
 else if(!strcmp(c,"workflow-invalid-events")){char message[33]="Reminder";assert(files_scene_background_message(message));assert(ps_message_step()==0);uint32_t rev=ps_message_revision;
 injected=(risc_scene_event_v1){.struct_size=sizeof(injected),.sequence=1,.document_revision=rev,.kind=RISC_SCENE_VALUE_EVENT,.node=2,.action=1};assert(ps_message_step()==0&&ps_message_pending);
 injected=(risc_scene_event_v1){.struct_size=sizeof(injected),.sequence=2,.document_revision=rev,.kind=RISC_SCENE_SUSPEND_EVENT,.node=1};assert(ps_message_step()==0&&!ps_exit);
 injected=(risc_scene_event_v1){.struct_size=sizeof(injected),.sequence=3,.document_revision=rev,.kind=RISC_SCENE_ACTION_EVENT,.node=2,.action=1};assert(ps_message_step()==1&&!ps_message_pending);}
 else if(!strcmp(c,"close-again")){close_again=true;assert(!ps_close()&&!ps_clean);close_again=false;assert(ps_close()&&ps_clean);return puts("close-again PASS")<0;}
 else if(!strcmp(c,"host-controls")){host_setup();assert(files_resident_manual(&ps_host,RISC_RESIDENT_CHECKPOINT_CONTROLS));assert(ps_host_step(true)&&host_calls==1&&last_reason==RISC_RESIDENT_CHECKPOINT_CONTROLS&&!ps_ui.token);}
 else if(!strcmp(c,"host-alarm")){host_setup();ps_host.pending_attention=RISC_RESIDENT_ATTENTION_ALARM;assert(ps_host_step(true)&&last_reason==RISC_RESIDENT_QUERY_ALARM_ONLY);assert(ps_host_step(true)&&last_reason==RISC_RESIDENT_CHECKPOINT_ALARM&&host_calls==2);}
 else if(!strcmp(c,"text-accept")||!strcmp(c,"text-cancel")){name_setup();text_result.state=!strcmp(c,"text-accept")?RISC_TEXT_ENTRY_ACCEPTED:RISC_TEXT_ENTRY_CANCELLED;strcpy(text_result.text,"Meeting");assert(ps_text_step()&&ps_text_finishing);text_close_rc=RISC_TEXT_ENTRY_AGAIN;assert(ps_text_step()&&ps_text_finishing&&ps_text.active);text_close_rc=0;assert(ps_text_step()&&!ps_text.active&&!writes);if(text_result.state==RISC_TEXT_ENTRY_ACCEPTED)assert(!strcmp(ps_editor.type.name,"Meeting"));else assert(!ps_editor.type.name[0]);}
 else if(!strcmp(c,"text-host-pause")){host_setup();name_setup();ps_host.pending_attention=RISC_RESIDENT_ATTENTION_ALARM;pause_rc=RISC_TEXT_ENTRY_AGAIN;assert(ps_text_step()&&ps_checkpoint.pausing&&!host_calls);pause_rc=0;assert(ps_text_step()&&ps_checkpoint.paused);assert(ps_text_step()&&host_calls==1&&!resume_calls);assert(ps_text_step()&&host_calls==2&&resume_calls==1&&!ps_checkpoint.paused&&!writes);}
 else if(!strcmp(c,"text-suffix-invalid")){broken_checkpoint=true;checkpoint.checkpoint_version=99;name_setup();assert(!ps_checkpoint.enabled&&!enabled_calls);}
 else if(!strcmp(c,"text-workflow-serialized")){host_setup();name_setup();char message[33]="While typing";assert(files_scene_background_message(message));ps_host.pending_attention=RISC_RESIDENT_ATTENTION_ALARM;
 assert(ps_text_step()&&ps_checkpoint.paused);assert(ps_text_step()&&ps_message_pending&&ps_ui.token&&!host_calls&&!resume_calls);
 injected=(risc_scene_event_v1){.struct_size=sizeof(injected),.sequence=1,.document_revision=ps_message_revision,.kind=RISC_SCENE_ACTION_EVENT,.node=2,.action=1};assert(ps_text_step()&&!ps_message_pending&&host_calls==1&&!resume_calls);assert(ps_text_step()&&host_calls==2&&resume_calls==1);}
 else if(!strcmp(c,"text-open-busy")){host_setup();assert(points_editor_begin_type(&ps_editor,POINTS_CUSTOM_1));points_scene_page_to(&ps_model,POINTS_SCENE_NAME);ps_model.pending_effect=POINTS_SCENE_EFFECT_NAME;ps_text_pending=true;text_open_rc=RISC_TEXT_ENTRY_BUSY;ps_host.pending_attention=RISC_RESIDENT_ATTENTION_ALARM;for(unsigned i=0;i<4;i++)assert(ps_text_step());assert(host_calls>=2&&!ps_text.active);}
 else if(!strcmp(c,"text-held")){host_setup();name_setup();ps_host.pending_attention=RISC_RESIDENT_ATTENTION_ALARM;text_flags=RISC_TEXT_ENTRY_CHECKPOINT_INPUT_BUSY;assert(ps_text_step()&&!pause_calls&&!host_calls);}
 else if(!strcmp(c,"save-revoked")){assert(points_editor_begin_event(&ps_editor,0));points_scene_page_to(&ps_model,POINTS_SCENE_EDIT);revoke_write=true;assert(!ps_effect(POINTS_SCENE_EFFECT_SAVE_EVENT)&&ps_retained&&writes==1&&!refreshes);unsigned n=calls,r=releases;assert(!ps_close());app_main();assert(calls==n&&releases==r);return puts("save-revoked PASS")<0;}
 else if(!strcmp(c,"custody-revoked")){identity=&other;unsigned n=calls;assert(!ps_close()&&ps_retained&&calls==n);app_main();assert(calls==n);return puts("custody-revoked PASS")<0;}
 else assert(!"unknown case");
 assert(ps_close()&&ps_clean&&!ps_retained);}
 printf("%s PASS\n",c);return 0;}
