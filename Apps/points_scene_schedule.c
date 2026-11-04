/* This translation unit implements the UTC record-domain projection. Both
 * domains are always linked into the same ELF; runtime service negotiation
 * selects the domain, never a product/compiler variant. The canonical pure
 * UTC helpers require their explicit record profile. No record keys are used. */
#define ALARM_NATIVE_UTC 1
#define ALARM_SERVICE_CLOCK_EDIT_V2 1
#include "points_scene_schedule.h"
#include "PointsUtcSchedule.h"
#include "PointsServiceContext.h"
#include "PortableTimeZonePreference.h"
#include "PortableTimeFormat.h"
#include <stdio.h>
bool points_scene_native_schedule(const alarm_service_v1 *alarm,const points_config *config,
 const risc_key_value_v1 *preferences,uint32_t seconds,unsigned format,
 bool(*alive)(void*),void *context,points_scene_schedule *out){
 if(!alive||!alive(context)||!out)return false;
 points_scene_schedule value={0};char zone[PORTABLE_TIMEZONE_ID_BYTES];
 int rc=portable_timezone_preference_load(preferences,zone);
 if(!alive(context))return false;
 if(rc!=PORTABLE_TIMEZONE_LOADED&&rc!=PORTABLE_TIMEZONE_MISSING){
  snprintf(value.status,sizeof(value.status),"Timezone unavailable: retry");*out=value;return true;
 }
 portable_timezone_rule rule;portable_timezone_civil local;
 int index=portable_timezone_find(zone,sizeof(zone));
 if(index<0||portable_timezone_resolve(zone,sizeof(zone),&rule)!=PORTABLE_TIMEZONE_OK||
    portable_timezone_utc_to_local(&rule,INT64_C(946684800)+seconds,&local,NULL)!=PORTABLE_TIMEZONE_OK){
  snprintf(value.status,sizeof(value.status),"Time/zone invalid: retry");*out=value;return true;
 }
 unsigned hour=local.hour;const char *suffix="";
 if(format==PORTABLE_TIME_FORMAT_12){suffix=hour<12?" AM":" PM";hour=hour%12;if(!hour)hour=12;}
 snprintf(value.clock_text,sizeof(value.clock_text),"%02u:%02u%s %.39s",hour,local.minute,suffix,zone);
 value.valid=true;
 if(config){
  points_config projected=*config;uint32_t sample=seconds;
  const points_service_context_suffix *p=alarm_service_extension_v2(alarm,sizeof(*p),POINTS_SERVICE_CONTEXT_TAG,POINTS_SERVICE_CONTEXT_VERSION);
  const alarm_service_descriptor_v2 *descriptor=alarm_service_descriptor(alarm);
  if(descriptor){
   size_t offset=(descriptor->features&ALARM_DESCRIPTOR_CLOCK_EDIT)?sizeof(alarm_clock_edit_descriptor_v2):sizeof(*descriptor);
   if(alarm->struct_size>=offset+sizeof(uint32_t)){
    uint32_t tag;memcpy(&tag,(const unsigned char*)alarm+offset,sizeof(tag));
    if(tag==POINTS_SERVICE_CONTEXT_TAG&&(!p||!p->context)){
     snprintf(value.status,sizeof(value.status),"Schedule service incompatible");*out=value;return true;
    }
   }
  }
  if(p){
   points_service_context snapshot={.struct_size=sizeof(snapshot)};
   rc=points_service_read_context(alarm,config,zone,(uint32_t)index,&snapshot);
   if(!alive(context))return false;
   if(rc!=ALARM_OK){
    snprintf(value.status,sizeof(value.status),"%s",rc==ALARM_PENDING||rc==ALARM_BUSY?"Schedule syncing":rc==ALARM_STALE?"Schedule changed: retry":"Schedule unavailable: retry");
    *out=value;return rc!=ALARM_RETAINED&&rc!=ALARM_OUTPUT&&rc>=ALARM_RETAINED&&rc<=ALARM_PENDING;
   }
   projected.created=snapshot.effective_created;sample=snapshot.sampled_utc;
  }
  points_projection preview={0};
  if(!points_utc_project(&rule,&projected,sample,&preview))snprintf(value.status,sizeof(value.status),"Schedule unavailable: retry");
  else if(preview.flags&POINTS_FLAG_GAP)snprintf(value.status,sizeof(value.status),"DST gap: start skipped");
  else if(preview.flags&POINTS_FLAG_FOLD)snprintf(value.status,sizeof(value.status),"DST fold: start skipped");
  else if(preview.flags&POINTS_FLAG_RANGE)snprintf(value.status,sizeof(value.status),"Time range limit");
  if(preview.count)value.next_event_id=preview.next[0].slot+1;
 }
 *out=value;return true;
}
