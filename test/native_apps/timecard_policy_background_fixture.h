#pragma once
#include "ContextsServiceV1.h"
#include "ContextFingerprintService.h"
static bool lifecycle_background,lifecycle_message,lifecycle_message_sent,lifecycle_background_fail;
static bool lifecycle_observing,lifecycle_busy_frame;
static unsigned lifecycle_context_steps,lifecycle_capture_calls,lifecycle_capture_busy,lifecycle_broadcast_steps,lifecycle_message_dismissals;
static bool lc_pause(void *c){(void)c;fx_io();lifecycle_observing=false;return true;}
static bool lc_step(void *c,const contexts_policy_v1 *p){(void)c;fx_io();assert(p->struct_size==sizeof(*p));if(tcp_text_active)lifecycle_context_steps++;lifecycle_observing=p->enabled&&p->audio_allowed;return true;}
static bool lc_capture(void *c){(void)c;fx_io();if(tcp_text_active){lifecycle_capture_calls++;if(host_busy)lifecycle_capture_busy++;if(lifecycle_background_fail&&lifecycle_capture_calls>3)return false;}return true;}
static bool lc_status(void *c,contexts_status_v1 *s){(void)c;fx_io();*s=(contexts_status_v1){.struct_size=sizeof(*s)};return true;}
static bool lc_source(void*c,uint32_t s){(void)c;(void)s;fx_io();return true;}
static bool lc_record(void*c,uint32_t a,uint32_t b,uint32_t d,const void*e,uint32_t f){(void)c;(void)a;(void)b;(void)d;(void)e;(void)f;fx_io();return true;}
static bool lc_finish(void*c,uint32_t a,uint32_t b){(void)c;(void)a;(void)b;fx_io();return true;}
static int32_t lc_label(void*c,uint32_t a,uint32_t b,contexts_label_v1*s){(void)c;(void)a;(void)b;(void)s;fx_io();return 0;}
static bool lc_claim(void*c,uint32_t a,uint32_t b,const char*s,uint32_t d){(void)c;(void)a;(void)b;(void)s;(void)d;fx_io();return false;}
static bool lc_result(void*c,uint32_t a,uint32_t b,uint32_t d){(void)c;(void)a;(void)b;(void)d;fx_io();return true;}
static bool lc_fingerprint(void*c,uint32_t op,void*q){
 (void)c;fx_io();
 if(op==CONTEXTS_FP_STATUS){contexts_fingerprint_status_v1 *s=q;s->sources=s->dirty_sources=0;}
 else if(op==CONTEXTS_FP_WORKFLOW){contexts_workflow_v1 *w=q;if(w->claim&&lifecycle_message&&!lifecycle_message_sent&&tcp_text_active&&text_at==6){w->available=true;w->step.kind=CR_MESSAGE;strcpy(w->step.text,"Lifecycle test");lifecycle_message_sent=true;}}
 return true;
}
static contexts_fingerprint_service_v1 lc_contexts={.base={.api_version=1,.struct_size=sizeof(lc_contexts),.step=lc_step,.pause=lc_pause,.status=lc_status,.request_export=lc_source,.begin_export=lc_source,.export_record=lc_record,.finish_export=lc_finish,.label=lc_label,.claim_preset=lc_claim,.preset_result=lc_result,.capture_audio=lc_capture},.fingerprint_abi=0x31504643u,.fingerprint=lc_fingerprint};
static int32_t lc_stat(void*c,const char*n,uint32_t*s,uint64_t*r){(void)c;(void)n;fx_io();*s=0;*r=0;return RISC_APP_DATA_NOT_FOUND;}
static int32_t lc_read(void*c,const char*n,uint64_t r,void*b,uint32_t z,uint32_t*s,uint64_t*a){(void)c;(void)n;(void)r;(void)b;(void)z;(void)s;(void)a;assert(0);return -1;}
static int32_t lc_replace(void*c,const char*n,uint64_t r,const void*b,uint32_t z){(void)c;(void)n;(void)r;(void)b;(void)z;assert(0);return -1;}
static const risc_app_data_v1 lc_files={1,sizeof(lc_files),NULL,lc_stat,lc_read,lc_replace};
static bool lc_broadcast(void*c,bool allow,const telemetry_broadcast_policy_v1*p){if(tcp_text_active)lifecycle_broadcast_steps++;return bt_step(c,allow,p);}
static telemetry_broadcast_v1 lc_bt;
static bool lifecycle_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1*g){
 if(lifecycle_background&&!strcmp(name,CONTEXTS_SERVICE_CAPABILITY)){assert(version==1&&!instance);grants++;g->slot=grants;g->generation=1;g->api=&lc_contexts;return true;}
 if(lifecycle_background&&!strcmp(name,RISC_SHARED_DATA_CAPABILITY)){assert(version==1&&instance==4);grants++;g->slot=grants;g->generation=1;g->api=&lc_files;return true;}
 bool ok=watch_acquire(name,version,instance,g);if(ok&&!strcmp(name,TELEMETRY_BROADCAST_CAPABILITY))g->api=&lc_bt;return ok;
}
static bool lifecycle_navigation(void*c,risc_input_navigation_frame_v1*s){
 (void)c;*s=(risc_input_navigation_frame_v1){0};
 if(lifecycle_message&&policy_paused&&subs){lifecycle_message_dismissals++;if(!(lifecycle_message_dismissals&1))s->buttons=s->pressed=T5_APP_BUTTON_BACK;}
 return true;
}
