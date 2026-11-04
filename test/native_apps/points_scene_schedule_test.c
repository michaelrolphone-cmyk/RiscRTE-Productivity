#define ALARM_NATIVE_UTC 1
#define ALARM_SERVICE_CLOCK_EDIT_V2 1
#include "points_scene_schedule.h"
#include "PointsServiceContext.h"
#include "PortableTimeZonePreference.h"
#include "PortableTimeFormat.h"
#include <assert.h>
#include <stdio.h>
static bool live=true,missing=true,revoke_get,revoke_context;
static unsigned reads,writes,contexts;
static uint8_t record[44];
static points_config config;
static points_context_service_v2 service;
static int context_rc,damage;
static uint32_t now;
static bool alive(void*c){(void)c;return live;}
static int32_t get(void*c,const char*k,void*b,uint32_t cap,uint32_t*n){
 (void)c;assert(live);assert(!strcmp(k,"time_zone"));reads++;if(revoke_get)live=false;
 *n=0;if(missing)return RISC_KEY_VALUE_NOT_FOUND;assert(cap>=44);memcpy(b,record,44);*n=44;return 0;
}
static int32_t put(void*c,const char*k,const void*b,uint32_t n){(void)c;assert(!strcmp(k,"time_zone"));assert(n==44);memcpy(record,b,n);missing=false;writes++;return 0;}
static risc_key_value_v1 kv={1,sizeof(kv),NULL,get,put};
static int32_t generic(void*c){(void)c;return 0;}
static int32_t status(void*c,alarm_status_v1*s){(void)c;(void)s;return 0;}
static int32_t ack(void*c,const alarm_token_v1*t){(void)t;return generic(c);}
static int32_t sleep_cb(void*c,alarm_sleep_v1*t){(void)t;return generic(c);}
static int32_t edit_begin(void*c,uint32_t n,alarm_clock_edit_permit_v2*p){(void)n;(void)p;return generic(c);}
static int32_t edit_finish(void*c,const alarm_clock_edit_permit_v2*p){(void)p;return generic(c);}
static int32_t edit_status(void*c,alarm_clock_edit_status_v2*s){(void)s;return generic(c);}
static int32_t context(void*c,points_service_context*s){
 (void)c;assert(live);contexts++;if(revoke_context)live=false;
 if(context_rc)return context_rc;
 s->struct_size=sizeof(*s);points_config_encode(&config,s->config);
 memset(s->timezone_id,0,sizeof(s->timezone_id));strcpy(s->timezone_id,"UTC");
 s->timezone_index=(uint32_t)portable_timezone_find("UTC",4);s->sampled_utc=now;s->effective_created=0;s->snapshot=1;
 if(damage==1)s->config[8]^=1;if(damage==2)s->snapshot=0;if(damage==3)s->timezone_index++;return 0;
}
static void reset(void){
 live=missing=true;revoke_get=revoke_context=false;reads=writes=contexts=0;context_rc=damage=0;
 config=points_default_config();assert(alarm_calendar_seconds(2026,1,2,13,5,0,&now));
 alarm_service_v1 base={.api_version=2,.struct_size=sizeof(service),.status=status,.step=generic,.refresh=generic,.stop_only=generic,.acknowledge=ack,.prepare_sleep=sleep_cb};
 service=(points_context_service_v2){.base={.base=base,.tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=1,.output_modes=ALARM_MODE_BOTH},.points={POINTS_SERVICE_CONTEXT_TAG,1,context}};
}
int main(void){
 reset();points_scene_schedule out={0};
 assert(points_scene_native_schedule(NULL,NULL,&kv,now,0,alive,NULL,&out));assert(out.valid);assert(strstr(out.clock_text,"01:05 PM UTC"));assert(writes==0&&reads==1);
 assert(points_scene_native_schedule(NULL,NULL,&kv,now,1,alive,NULL,&out));assert(strstr(out.clock_text,"13:05 UTC"));
 assert(points_scene_native_schedule(&service.base.base,&config,&kv,now,1,alive,NULL,&out));assert(out.valid&&contexts==1&&out.next_event_id);
 points_clock_edit_context_service_v2 mixed={.base={.service=service.base,.tag=ALARM_CLOCK_EDIT_TAG,.version=1,.begin=edit_begin,.finish=edit_finish,.cancel=edit_finish,.edit_status=edit_status},.points=service.points};
 mixed.base.service.base.struct_size=sizeof(mixed);mixed.base.service.features=ALARM_DESCRIPTOR_CLOCK_EDIT;
 assert(points_scene_native_schedule(&mixed.base.service.base,&config,&kv,now,1,alive,NULL,&out));assert(out.valid&&contexts==2);
 for(damage=1;damage<=3;damage++){assert(points_scene_native_schedule(&service.base.base,&config,&kv,now,1,alive,NULL,&out));assert(out.status[0]);}
 damage=0;context_rc=ALARM_PENDING;assert(points_scene_native_schedule(&service.base.base,&config,&kv,now,1,alive,NULL,&out));assert(!strcmp(out.status,"Schedule syncing"));
 context_rc=ALARM_RETAINED;assert(!points_scene_native_schedule(&service.base.base,&config,&kv,now,1,alive,NULL,&out));
 reset();service.points.version=99;assert(points_scene_native_schedule(&service.base.base,&config,&kv,now,1,alive,NULL,&out));assert(!strcmp(out.status,"Schedule service incompatible")&&contexts==0);
 reset();revoke_get=true;memset(&out,0x5a,sizeof(out));points_scene_schedule before=out;assert(!points_scene_native_schedule(&service.base.base,&config,&kv,now,1,alive,NULL,&out));assert(!memcmp(&before,&out,sizeof(out))&&contexts==0);
 reset();revoke_context=true;memset(&out,0x5a,sizeof(out));before=out;assert(!points_scene_native_schedule(&service.base.base,&config,&kv,now,1,alive,NULL,&out));assert(!memcmp(&before,&out,sizeof(out))&&reads==1&&contexts==1);
 reset();assert(portable_timezone_preference_save(&kv,"America/New_York",17)==PORTABLE_TIMEZONE_SAVED);unsigned old_writes=writes;
 assert(points_scene_native_schedule(NULL,NULL,&kv,now,1,alive,NULL,&out));assert(strstr(out.clock_text,"08:05 America/New_York")&&writes==old_writes);
 record[3]^=1;assert(points_scene_native_schedule(NULL,NULL,&kv,now,1,alive,NULL,&out));assert(!out.valid&&strstr(out.status,"Timezone unavailable"));
 reset();assert(portable_timezone_preference_save(&kv,"America/New_York",17)==PORTABLE_TIMEZONE_SAVED);
 memset(&config,0,sizeof(config));config.revision=1;config.points[0]=(points_item){.kind=POINTS_BREAK,.enabled=1,.mode=1,.weekdays=127,.hour=2,.minute=30,.duration_minutes=15};
 assert(alarm_calendar_seconds(2026,3,8,5,0,0,&now));assert(points_scene_native_schedule(NULL,&config,&kv,now,1,alive,NULL,&out));assert(strstr(out.status,"DST gap"));
 config.points[0].hour=1;assert(alarm_calendar_seconds(2026,11,1,4,0,0,&now));assert(points_scene_native_schedule(NULL,&config,&kv,now,1,alive,NULL,&out));assert(strstr(out.status,"DST fold"));
 puts("points_scene_schedule: timezone, DST, context, malformed providers and custody PASS");return 0;
}
