#pragma once
/* Optional service negotiation for the canonical Points controller. No
 * presentation, device identity, geometry, or physical-resource operations. */
#include "RiscRuntimeV1.h"
#include "RiscRuntimeClockV1.h"
#include "PortableRtcClock.h"
#include "PortableRealtimeClient.h"
#include "PortableTimeFormat.h"
#ifndef ALARM_SERVICE_CLOCK_EDIT_V2
#define ALARM_SERVICE_CLOCK_EDIT_V2 1
#endif
#include "PointsServiceProjection.h"
#include "points_catalog_controller.h"
typedef struct {
 const risc_runtime_api_v1 *runtime;
 bool (*alive)(void *);void (*retain)(void *);bool (*prepare)(void *);void *owner;
 risc_runtime_capability_v1 grants[4];
 const risc_app_data_v1 *source;
 const risc_key_value_v1 *legacy_source,*preference_source;
 const alarm_service_v1 *alarm;
 risc_app_data_v1 files;risc_key_value_v1 legacy,preferences;
 portable_realtime_client clock;
 alarm_status_v1 status;points_catalog_projection projection;
 uint32_t domain;unsigned time_format;
 bool terminal,ready,valid,visual_only,clock_valid,catalog;
} points_scene_services;
static void pss_retain(points_scene_services *s){
 if(s&&!s->terminal){s->terminal=true;if(s->retain)s->retain(s->owner);}
}
static bool pss_live(points_scene_services *s){
 if(!s||s->terminal)return false;
 if(!s->alive||!s->alive(s->owner)||risc_runtime_get_api(1)!=s->runtime){pss_retain(s);return false;}
 return true;
}
static bool pss_prepare(points_scene_services *s){
 if(!pss_live(s)||!s->prepare)return false;
 bool prepared=s->prepare(s->owner);return pss_live(s)&&prepared;
}
static bool pss_empty(const risc_runtime_capability_v1 *g){return !g->api&&!g->slot&&!g->generation;}
static bool pss_release(points_scene_services *s,risc_runtime_capability_v1 *g){
 if(!pss_live(s))return false;
 if(pss_empty(g))return true;
 risc_runtime_capability_v1 copy=*g;bool ok=s->runtime->release(&copy);
 if(!pss_live(s))return false;
 if(!ok||copy.struct_size!=sizeof(copy)||!pss_empty(&copy)){pss_retain(s);return false;}
 *g=copy;return true;
}
static int pss_acquire(points_scene_services *s,const char *name,uint32_t version,uint64_t id,risc_runtime_capability_v1 *g){
 if(!pss_prepare(s))return -1;
 if(!pss_empty(g)){pss_retain(s);return -1;}
 risc_runtime_capability_v1 value={.struct_size=sizeof(value)};
 bool ok=s->runtime->acquire(name,version,id,&value);if(!pss_live(s))return -1;
 if(value.struct_size!=sizeof(value)||(ok?(!value.api||!value.slot||!value.generation):!pss_empty(&value))){pss_retain(s);return -1;}
 if(!ok)return 0;
 *g=value;return 1;
}
static int32_t pss_file_result(points_scene_services *s,int32_t rc){
 if(!pss_live(s))return RISC_APP_DATA_RETAINED;
 if(rc==RISC_APP_DATA_CONTEXT||rc==RISC_APP_DATA_RETAINED||rc>0||rc<RISC_APP_DATA_BUSY){pss_retain(s);return RISC_APP_DATA_RETAINED;}
 return rc;
}
static int32_t pss_stat(void *c,const char *name,uint32_t *size,uint64_t *revision){
 points_scene_services*s=c;if(!pss_prepare(s))return RISC_APP_DATA_RETAINED;
 if(!points_catalog_storage_api(s->source))return RISC_APP_DATA_UNAVAILABLE;
 return pss_file_result(s,s->source->stat(s->source->context,name,size,revision));
}
static int32_t pss_read(void *c,const char *name,uint64_t expected,void *out,uint32_t cap,uint32_t *size,uint64_t *revision){
 points_scene_services*s=c;if(!pss_prepare(s))return RISC_APP_DATA_RETAINED;
 if(!points_catalog_storage_api(s->source))return RISC_APP_DATA_UNAVAILABLE;
 return pss_file_result(s,s->source->read(s->source->context,name,expected,out,cap,size,revision));
}
static int32_t pss_replace(void *c,const char *name,uint64_t expected,const void *data,uint32_t size){
 points_scene_services*s=c;if(!pss_prepare(s))return RISC_APP_DATA_RETAINED;
 if(!points_catalog_storage_api(s->source))return RISC_APP_DATA_UNAVAILABLE;
 return pss_file_result(s,s->source->replace(s->source->context,name,expected,data,size));
}
static int32_t pss_get_source(points_scene_services*s,const risc_key_value_v1 *kv,const char *key,void*out,uint32_t cap,uint32_t*size){
 if(!pss_prepare(s))return RISC_KEY_VALUE_CONTEXT;
 if(!kv||kv->api_version!=1||kv->struct_size<sizeof(*kv)||!kv->get)return RISC_KEY_VALUE_INVALID;
 int32_t rc=kv->get(kv->context,key,out,cap,size);if(!pss_live(s))return RISC_KEY_VALUE_CONTEXT;
 if(rc!=RISC_KEY_VALUE_OK&&rc!=RISC_KEY_VALUE_NOT_FOUND&&rc!=RISC_KEY_VALUE_BUFFER_SMALL&&rc!=RISC_KEY_VALUE_INVALID&&rc!=RISC_KEY_VALUE_IO&&rc!=RISC_KEY_VALUE_BUSY){pss_retain(s);return RISC_KEY_VALUE_CONTEXT;}
 return rc;
}
static const char *pss_legacy_key(points_scene_services*s,const char*key){
 if(!key)return NULL;
 if(!strcmp(key,"points_cfg"))return s->domain==POINTS_TIME_NATIVE_UTC?"points_utc_cfg":"points_cfg";
 if(!strcmp(key,"points_meta"))return s->domain==POINTS_TIME_NATIVE_UTC?"points_utc_meta":"points_meta";
 if(s->domain==POINTS_TIME_NATIVE_UTC&&(!strcmp(key,"points_utc_cfg")||!strcmp(key,"points_utc_meta")))return key;
 return NULL;
}
static int32_t pss_legacy_get(void*c,const char*key,void*out,uint32_t cap,uint32_t*size){
 points_scene_services*s=c;key=pss_legacy_key(s,key);
 /* Only app configuration and metadata, never occurrence/scheduler state. */
 if(!key)return RISC_KEY_VALUE_INVALID;
 return pss_get_source(s,s->legacy_source,key,out,cap,size);
}
static int32_t pss_legacy_put(void*c,const char*key,const void*data,uint32_t size){
 points_scene_services*s=c;if(!pss_prepare(s))return RISC_KEY_VALUE_CONTEXT;
 if(!s->legacy_source||s->legacy_source->api_version!=1||s->legacy_source->struct_size<sizeof(*s->legacy_source)||!s->legacy_source->put)return RISC_KEY_VALUE_INVALID;
 key=pss_legacy_key(s,key);if(!key)return RISC_KEY_VALUE_INVALID;
 int32_t rc=s->legacy_source->put(s->legacy_source->context,key,data,size);
 if(!pss_live(s))return RISC_KEY_VALUE_CONTEXT;
 if(rc!=RISC_KEY_VALUE_OK&&rc!=RISC_KEY_VALUE_NOT_FOUND&&rc!=RISC_KEY_VALUE_BUFFER_SMALL&&rc!=RISC_KEY_VALUE_INVALID&&rc!=RISC_KEY_VALUE_IO&&rc!=RISC_KEY_VALUE_BUSY){pss_retain(s);return RISC_KEY_VALUE_CONTEXT;}
 return rc;
}
static int32_t pss_preference_get(void*c,const char*key,void*out,uint32_t cap,uint32_t*size){points_scene_services*s=c;return pss_get_source(s,s->preference_source,key,out,cap,size);}
static bool pss_alarm_result(points_scene_services*s,int32_t rc){
 if(!pss_live(s))return false;
 if(rc==ALARM_RETAINED||rc==ALARM_OUTPUT||rc<ALARM_RETAINED||rc>ALARM_PENDING){pss_retain(s);return false;}return true;
}
static bool pss_status(points_scene_services*s){
 s->valid=false;if(!pss_prepare(s)||!s->alarm)return false;
 alarm_status_v1 status={.struct_size=sizeof(status)};
 int32_t rc=s->alarm->status(s->alarm->context,&status);
 if(!pss_alarm_result(s,rc)||!pss_alarm_result(s,status.error))return false;
 if(rc!=ALARM_OK||status.api_version!=1||status.struct_size<sizeof(status)||status.state>ALARM_STATE_CUE)return false;
 points_catalog_projection projection={.struct_size=sizeof(projection)};
 rc=s->catalog?points_service_project(s->alarm,&projection):ALARM_OK;
 if(!pss_alarm_result(s,rc))return false;
 if(rc!=ALARM_OK||projection.struct_size!=sizeof(projection)||projection.count>POINTS_CATALOG_NEXT_COUNT)return false;
 s->status=status;s->projection=projection;s->valid=true;return true;
}
static bool pss_busy(const points_scene_services*s){return !s->valid||s->status.output_uncertain||s->status.occurrence.generation||s->status.state==ALARM_STATE_BLOCKED||s->status.state==ALARM_STATE_CUE||s->status.state==ALARM_STATE_ALERT||s->status.state==ALARM_STATE_DISMISSING;}
static int pss_clock_guard(void*c){return pss_live(c)?PORTABLE_REALTIME_GUARD_SAFE:PORTABLE_REALTIME_GUARD_RETAINED;}
static bool pss_clock_status(points_scene_services*s,int rc){
 if(!pss_live(s))return false;
 if(rc==PORTABLE_REALTIME_CONTEXT||rc==PORTABLE_REALTIME_UNCERTAIN||rc==PORTABLE_REALTIME_RETAINED||rc>PORTABLE_REALTIME_RECOVERED||rc<PORTABLE_REALTIME_VERIFY){pss_retain(s);return false;}return true;
}
static bool pss_utc_seconds(int64_t epoch,uint32_t *out){
 if(!out||epoch<INT64_C(946684800)||epoch>INT32_MAX)return false;
 *out=(uint32_t)(epoch-INT64_C(946684800));return true;
}
static bool pss_now(points_scene_services*s,uint32_t *seconds){
 s->clock_valid=false;if(!seconds||!pss_prepare(s))return false;
 if(s->domain==POINTS_TIME_NATIVE_UTC){
  int rc=portable_realtime_open(&s->clock,s->runtime,PORTABLE_REALTIME_READER,PORTABLE_REALTIME_TIMER_ONLY,pss_clock_guard,s);
  if(!pss_clock_status(s,rc)||rc!=PORTABLE_REALTIME_OK)return false;
  risc_realtime_snapshot_v1 value={0};rc=portable_realtime_read(&s->clock,&value);
  if(!pss_clock_status(s,rc))return false;
  int closed=portable_realtime_close(&s->clock);
  if(!pss_clock_status(s,closed)||closed!=PORTABLE_REALTIME_OK||rc!=PORTABLE_REALTIME_OK)return false;
  s->clock_valid=pss_utc_seconds(value.epoch_seconds,seconds);return s->clock_valid;
 }
 risc_runtime_capability_v1 grant={0};int acquired=pss_acquire(s,"rtc.clock",2,0,&grant);
 if(acquired!=1)return false;
 const twatch_rtc_api_v1 *rtc=grant.api;twatch_rtc_time_v1 time={0};bool ok=false;
 if(rtc->api_version==2&&rtc->struct_size>=sizeof(*rtc)&&rtc->read)ok=rtc->read(rtc->context,&time);
 if(!pss_live(s)||!pss_release(s,&grant))return false;
 s->clock_valid=ok&&time.weekday<=6&&alarm_calendar_seconds(time.year,time.month,time.day,time.hour,time.minute,time.second,seconds);return s->clock_valid;
}
static bool pss_kv_valid(const risc_key_value_v1*k){return k&&k->api_version==1&&k->struct_size>=sizeof(*k)&&k->get;}
static bool pss_close(points_scene_services*s){
 if(!pss_prepare(s))return false;
 int rc=portable_realtime_close(&s->clock);if(!pss_clock_status(s,rc)||rc!=PORTABLE_REALTIME_OK)return false;
 for(unsigned i=4;i;i--)if(!pss_release(s,&s->grants[i-1]))return false;
 s->source=NULL;s->legacy_source=s->preference_source=NULL;s->alarm=NULL;
 s->ready=s->valid=s->clock_valid=s->catalog=false;return true;
}
/* A recognized but incomplete catalog suffix is not permission to write the
 * unrelated legacy records. Unknown suffix families remain legacy-compatible. */
static int pss_catalog_kind(const alarm_service_v1 *a){
 size_t offset=sizeof(alarm_service_sleep_v1);
 if(a->api_version==2){
  const alarm_service_descriptor_v2*d=alarm_service_descriptor(a);if(!d)return -1;
  offset=sizeof(*d);
  if(d->features&ALARM_DESCRIPTOR_CLOCK_EDIT){
   if(!alarm_clock_edit_descriptor(a))return -1;
   offset=sizeof(alarm_clock_edit_descriptor_v2);
  }
 }
 if(a->struct_size<offset+sizeof(uint32_t))return 0;
 uint32_t tag;memcpy(&tag,(const uint8_t*)a+offset,sizeof(tag));
 if(tag!=POINTS_SERVICE_PROJECTION_TAG)return 0;
 if(a->struct_size<offset+sizeof(points_service_projection_suffix))return -1;
 const points_service_projection_suffix*p=(const points_service_projection_suffix*)((const uint8_t*)a+offset);
 return p->version==POINTS_SERVICE_PROJECTION_VERSION&&p->projection?1:-1;
}
static bool pss_open(points_scene_services*s){
 if(!pss_prepare(s))return false;
 if(!s->runtime||s->runtime->api_version!=1||s->runtime->struct_size<RISC_RUNTIME_CAPABILITIES_V1_SIZE||!s->runtime->acquire||!s->runtime->release)return false;
 s->ready=false;
 if(pss_acquire(s,RISC_APP_DATA_CAPABILITY,1,5,&s->grants[0])<0)return false;
 if(pss_acquire(s,"storage.key-value",1,5,&s->grants[1])!=1)return false;
 if(pss_acquire(s,"storage.key-value",1,1,&s->grants[2])<0)return false;
 int found=pss_acquire(s,ALARM_SERVICE_CAPABILITY,2,0,&s->grants[3]);if(found<0)return false;
 s->domain=POINTS_TIME_NATIVE_UTC;
 if(!found){s->domain=POINTS_TIME_RAW_RTC;if(pss_acquire(s,ALARM_SERVICE_CAPABILITY,1,0,&s->grants[3])!=1)return false;}
 s->source=s->grants[0].api;s->legacy_source=s->grants[1].api;s->preference_source=s->grants[2].api;s->alarm=s->grants[3].api;
 if(!pss_kv_valid(s->legacy_source))return false;
 if(!pss_kv_valid(s->preference_source)){
  if(!pss_release(s,&s->grants[2]))return false;
  s->preference_source=NULL;
 }
 const alarm_service_v1*a=s->alarm;
 if(a->struct_size<sizeof(*a)||a->api_version!=(s->domain?2u:1u)||!a->status||!a->refresh)return false;
 const alarm_service_descriptor_v2*d=s->domain?alarm_service_descriptor(a):NULL;
 if(s->domain&&!d)return false;
 s->visual_only=d&&d->output_modes==ALARM_MODE_VISUAL;
 s->files=(risc_app_data_v1){1,sizeof(s->files),s,pss_stat,pss_read,pss_replace};
 s->legacy=(risc_key_value_v1){1,sizeof(s->legacy),s,pss_legacy_get,pss_legacy_put};
 s->preferences=(risc_key_value_v1){1,sizeof(s->preferences),s,pss_preference_get,NULL};
 /* A tagged catalog extension selects catalog storage. A legacy scheduler
  * keeps the established fixed records; absence never triggers migration. */
 int catalog=pss_catalog_kind(a);if(catalog<0)return false;s->catalog=catalog==1;
 if(s->catalog?!points_catalog_storage_api(s->source):!s->legacy_source->put)return false;
 (void)pss_status(s);if(!pss_live(s))return false;s->ready=true;
 s->time_format=PORTABLE_TIME_FORMAT_12;
 if(s->preference_source)(void)portable_time_format_load(&s->preferences,&s->time_format);
 return pss_live(s);
}
static void pss_init(points_scene_services*s,const risc_runtime_api_v1*rt,bool(*alive)(void*),void(*retain)(void*),bool(*prepare)(void*),void*owner){memset(s,0,sizeof(*s));s->runtime=rt;s->alive=alive;s->retain=retain;s->prepare=prepare;s->owner=owner;}
