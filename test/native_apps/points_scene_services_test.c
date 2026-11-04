#include <assert.h>
#include <stdio.h>
#include "../../Apps/points_scene_services.h"
static points_scene_services services;
static risc_runtime_api_v1 runtime;
static const risc_runtime_api_v1 *current;
static risc_app_data_v1 files;
static risc_key_value_v1 legacy,prefs;
static points_service_v1 alarm1;
static points_service_v2 alarm2;
static points_clock_edit_service_v2 mixed;
static twatch_rtc_api_v1 rtc;
static risc_realtime_api_v1 native;
static alarm_status_v1 status;
static points_catalog_projection projection;
static risc_realtime_snapshot_v1 snapshot;
static twatch_rtc_time_v1 calendar;
static unsigned calls,acquires,releases,retains,raw_reads,native_reads,projections,kv_reads,kv_writes,file_reads,file_writes;
static unsigned revoke_at,fail_acquire_at,fail_release_at,grant_damage,release_damage;
static uint32_t live_slots;
static bool owner_live,prepared,has_files,has_legacy,has_prefs,has_v1,has_v2,has_rtc,has_native,use_mixed,rtc_ok;
static int file_rc,kv_rc,alarm_rc,projection_rc,native_rc;
static char last_key[64];
static unsigned preference_mode;
static void external(void){calls++;if(revoke_at&&calls==revoke_at)current=NULL;}
static bool alive(void*c){assert(c==&services);return owner_live;}
static void retain(void*c){assert(c==&services);retains++;owner_live=false;}
static bool prepare(void*c){assert(c==&services);return prepared;}
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){assert(version==1);return current;}
static int32_t stat_cb(void*c,const char*name,uint32_t*size,uint64_t*revision){
 (void)c;assert(!strcmp(name,POINTS_CATALOG_FILE));external();file_reads++;*size=0;*revision=0;return file_rc;
}
static int32_t read_cb(void*c,const char*name,uint64_t revision,void*out,uint32_t cap,uint32_t*size,uint64_t*actual){
 (void)c;(void)revision;(void)out;(void)cap;return stat_cb(c,name,size,actual);
}
static int32_t replace_cb(void*c,const char*name,uint64_t revision,const void*data,uint32_t size){
 (void)c;(void)revision;(void)data;(void)size;assert(!strcmp(name,POINTS_CATALOG_FILE));external();file_writes++;return file_rc;
}
static int32_t get_cb(void*c,const char*key,void*out,uint32_t cap,uint32_t*size){
 external();kv_reads++;snprintf(last_key,sizeof(last_key),"%s",key);*size=0;
 if(c==&prefs&&kv_rc==0){assert(!strcmp(key,"time_format"));if(cap<4)return RISC_KEY_VALUE_BUFFER_SMALL;
  const uint8_t bytes[]={0x54,1,(uint8_t)preference_mode,(uint8_t)(preference_mode^0xa5u)};memcpy(out,bytes,4);*size=4;}
 return kv_rc;
}
static int32_t put_cb(void*c,const char*key,const void*data,uint32_t size){
 (void)c;(void)data;(void)size;external();kv_writes++;snprintf(last_key,sizeof(last_key),"%s",key);return kv_rc;
}
static int32_t status_cb(void*c,alarm_status_v1*out){(void)c;assert(out->struct_size==sizeof(*out));external();*out=status;return alarm_rc;}
static int32_t generic_cb(void*c){(void)c;external();return ALARM_OK;}
static int32_t ack_cb(void*c,const alarm_token_v1*t){(void)t;return generic_cb(c);}
static int32_t sleep_cb(void*c,alarm_sleep_v1*t){(void)t;return generic_cb(c);}
static int32_t projection_cb(void*c,points_catalog_projection*out){(void)c;external();projections++;assert(out->struct_size==sizeof(*out));*out=projection;return projection_rc;}
static int32_t begin_cb(void*c,uint32_t requested,alarm_clock_edit_permit_v2*out){(void)requested;(void)out;return generic_cb(c);}
static int32_t finish_cb(void*c,const alarm_clock_edit_permit_v2*permit){(void)permit;return generic_cb(c);}
static int32_t edit_status_cb(void*c,alarm_clock_edit_status_v2*out){(void)out;return generic_cb(c);}
static bool rtc_read(void*c,twatch_rtc_time_v1*out){(void)c;external();raw_reads++;*out=calendar;return rtc_ok;}
static int32_t native_read(void*c,risc_realtime_snapshot_v1*out){assert(c==&native);external();native_reads++;*out=snapshot;return native_rc;}
static bool acquire(const char*name,uint32_t version,uint64_t id,risc_runtime_capability_v1*g){
 assert(g->struct_size==sizeof(*g)&&!g->api&&!g->slot&&!g->generation);external();acquires++;
 const void *api=NULL;
 if(!strcmp(name,RISC_APP_DATA_CAPABILITY)){assert(version==1&&id==5);if(has_files)api=&files;}
 else if(!strcmp(name,"storage.key-value")){assert(version==1&&(id==1||id==5));if(id==5&&has_legacy)api=&legacy;else if(id==1&&has_prefs)api=&prefs;}
 else if(!strcmp(name,ALARM_SERVICE_CAPABILITY)){assert(id==0&&(version==1||version==2));if(version==2&&has_v2)api=use_mixed?(const void*)&mixed:(const void*)&alarm2;else if(version==1&&has_v1)api=&alarm1;}
 else if(!strcmp(name,"rtc.clock")){assert(version==2&&id==0);if(has_rtc)api=&rtc;}
 else {assert(!strcmp(name,RISC_REALTIME_CAPABILITY)&&version==1&&id==0);if(has_native)api=&native;}
 if(fail_acquire_at==acquires){if(grant_damage){g->api=api;g->slot=31;g->generation=1;}return false;}
 if(!api)return false;
 unsigned slot=1;while(live_slots&(1u<<slot))slot++;assert(slot<31);live_slots|=1u<<slot;
 *g=(risc_runtime_capability_v1){.struct_size=sizeof(*g),.slot=slot,.generation=1,.api=api};
 if(grant_damage==2)g->slot=0;else if(grant_damage==3)g->struct_size--;
 return true;
}
static bool release(risc_runtime_capability_v1*g){
 assert(g->api&&g->slot&&g->generation);external();releases++;
 if(fail_release_at==releases)return false;
 assert(live_slots&(1u<<g->slot));live_slots&=~(1u<<g->slot);
 if(!release_damage)*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};
 return true;
}
static void fixture(void){
 calls=acquires=releases=retains=raw_reads=native_reads=projections=kv_reads=kv_writes=file_reads=file_writes=0;
 revoke_at=fail_acquire_at=fail_release_at=grant_damage=release_damage=live_slots=0;
 owner_live=prepared=has_files=has_legacy=has_prefs=has_v1=has_rtc=has_native=rtc_ok=true;has_v2=use_mixed=false;
 file_rc=kv_rc=alarm_rc=projection_rc=native_rc=0;preference_mode=1;last_key[0]=0;
 runtime=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(runtime),.acquire=acquire,.release=release};current=&runtime;
 files=(risc_app_data_v1){1,sizeof(files),NULL,stat_cb,read_cb,replace_cb};
 legacy=(risc_key_value_v1){1,sizeof(legacy),&legacy,get_cb,put_cb};prefs=(risc_key_value_v1){1,sizeof(prefs),&prefs,get_cb,NULL};
 alarm_service_v1 base={.api_version=1,.struct_size=sizeof(alarm_service_v1),.status=status_cb,.step=generic_cb,.refresh=generic_cb,.acknowledge=ack_cb,.prepare_sleep=sleep_cb,.stop_only=generic_cb};
 alarm1=(points_service_v1){.base={.base=base}};
 base.api_version=2;base.struct_size=sizeof(alarm_service_descriptor_v2);
 alarm2=(points_service_v2){.base={.base=base,.tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=1,.output_modes=ALARM_MODE_BOTH}};
 mixed=(points_clock_edit_service_v2){.base={.service=alarm2.base,.tag=ALARM_CLOCK_EDIT_TAG,.version=1,.begin=begin_cb,.finish=finish_cb,.cancel=finish_cb,.edit_status=edit_status_cb}};
 mixed.base.service.features=ALARM_DESCRIPTOR_CLOCK_EDIT;mixed.base.service.base.struct_size=sizeof(mixed.base);
 status=(alarm_status_v1){.api_version=1,.struct_size=sizeof(status),.state=ALARM_STATE_READY};
 projection=(points_catalog_projection){.struct_size=sizeof(projection)};
 rtc=(twatch_rtc_api_v1){.api_version=2,.struct_size=sizeof(rtc),.read=rtc_read};
 calendar=(twatch_rtc_time_v1){2026,11,4,3,12,30,0};
 native=(risc_realtime_api_v1){.api_version=1,.struct_size=sizeof(native),.context=&native,.read=native_read};
 snapshot=(risc_realtime_snapshot_v1){.struct_size=sizeof(snapshot),.validity=RISC_REALTIME_VALID,.epoch_seconds=INT64_C(1793795400)};
 pss_init(&services,&runtime,alive,retain,prepare,&services);
}
static void catalog(bool v2){
 has_v2=v2;
 if(v2){alarm2.base.base.struct_size=sizeof(alarm2);alarm2.points=(points_service_projection_suffix){POINTS_SERVICE_PROJECTION_TAG,1,projection_cb};}
 else {alarm1.base.base.struct_size=sizeof(alarm1);alarm1.points=(points_service_projection_suffix){POINTS_SERVICE_PROJECTION_TAG,1,projection_cb};}
}
static void closed(void){assert(pss_close(&services)&&!services.ready&&!services.valid&&!services.clock_valid&&!live_slots);}
static void terminal(void){
 assert(services.terminal&&retains==1);unsigned before=calls;
 assert(!pss_status(&services));uint32_t now=77;assert(!pss_now(&services,&now));assert(!pss_open(&services));assert(!pss_close(&services));
 uint32_t n;uint64_t revision;assert(pss_stat(&services,POINTS_CATALOG_FILE,&n,&revision)==RISC_APP_DATA_RETAINED);
 assert(!pss_alarm_result(&services,ALARM_OK)&&now==77&&calls==before&&retains==1);
}
static void negotiations(void){
 fixture();assert(pss_open(&services)&&services.domain==POINTS_TIME_RAW_RTC&&!services.catalog&&services.time_format==1&&services.valid&&!pss_busy(&services));assert(!projections&&!kv_writes);closed();
 fixture();has_v2=true;alarm2.base.output_modes=ALARM_MODE_VISUAL;assert(pss_open(&services)&&services.domain==POINTS_TIME_NATIVE_UTC&&!services.catalog&&services.visual_only);closed();
 for(unsigned v=0;v<2;v++){fixture();catalog(v);assert(pss_open(&services)&&services.catalog&&projections==1&&services.domain==v);closed();}
 fixture();has_v2=use_mixed=true;assert(pss_open(&services)&&!services.catalog);closed();
 fixture();has_v2=use_mixed=true;mixed.base.service.base.struct_size=sizeof(mixed);mixed.points=(points_service_projection_suffix){POINTS_SERVICE_PROJECTION_TAG,1,projection_cb};assert(pss_open(&services)&&services.catalog&&projections==1);closed();
 fixture();has_files=false;assert(pss_open(&services)&&!services.catalog);uint32_t n;uint64_t r;assert(pss_stat(&services,POINTS_CATALOG_FILE,&n,&r)==RISC_APP_DATA_UNAVAILABLE);closed();
 fixture();catalog(true);has_files=false;assert(!pss_open(&services)&&!services.terminal);closed();
 fixture();has_prefs=false;assert(pss_open(&services)&&services.time_format==0);closed();
 fixture();prefs.struct_size=8;assert(pss_open(&services)&&!services.preference_source&&services.time_format==0);closed();
 fixture();preference_mode=99;assert(pss_open(&services)&&services.time_format==0&&!kv_writes);closed();
 fixture();legacy.put=NULL;assert(!pss_open(&services)&&!services.terminal);closed();
 fixture();catalog(true);legacy.put=NULL;assert(pss_open(&services));closed();
 fixture();has_legacy=false;assert(!pss_open(&services));closed();
 fixture();has_v1=false;assert(!pss_open(&services));closed();
 fixture();runtime.struct_size=8;assert(!pss_open(&services)&&!acquires);
 fixture();runtime.acquire=NULL;assert(!pss_open(&services)&&!acquires);
}
static void malformed(void){
 for(unsigned v=0;v<2;v++)for(unsigned fault=0;fault<4;fault++){
  fixture();catalog(v);points_service_projection_suffix*p=v?&alarm2.points:&alarm1.points;
  alarm_service_v1*a=v?&alarm2.base.base:&alarm1.base.base;
  if(fault==0)p->version=2;else if(fault==1)p->projection=NULL;
  else if(fault==2)a->struct_size-=(uint32_t)sizeof(p->projection);
  else p->tag=0x12345678;
  assert(pss_open(&services)==(fault==3));assert(!services.catalog&&!projections&&!kv_writes);closed();
 }
 fixture();has_v2=true;alarm2.base.descriptor_version=9;assert(!pss_open(&services)&&!projections);closed();
 fixture();has_v2=true;alarm2.base.base.status=NULL;assert(!pss_open(&services));closed();
 fixture();has_v2=use_mixed=true;mixed.base.finish=NULL;assert(!pss_open(&services));closed();
 fixture();catalog(true);files.replace=NULL;assert(!pss_open(&services));closed();
 fixture();legacy.get=NULL;assert(!pss_open(&services));closed();
 fixture();status.struct_size=8;assert(pss_open(&services)&&!services.valid&&pss_busy(&services));closed();
 fixture();status.state=99;assert(pss_open(&services)&&!services.valid);closed();
 fixture();catalog(true);projection.count=POINTS_CATALOG_NEXT_COUNT+1;assert(pss_open(&services)&&!services.valid);closed();
 fixture();catalog(true);projection.struct_size=0;assert(pss_open(&services)&&!services.valid);closed();
}
static void wrappers(void){
 uint8_t bytes[64]={0};uint32_t n;uint64_t revision;
 for(unsigned v=0;v<2;v++){
  fixture();has_v2=v;assert(pss_open(&services));
  assert(pss_legacy_get(&services,"points_cfg",bytes,sizeof(bytes),&n)==0&&!strcmp(last_key,v?"points_utc_cfg":"points_cfg"));
  assert(pss_legacy_put(&services,"points_meta",bytes,sizeof(bytes))==0&&!strcmp(last_key,v?"points_utc_meta":"points_meta"));
  unsigned before=calls;assert(pss_legacy_get(&services,"points_occ",bytes,sizeof(bytes),&n)==RISC_KEY_VALUE_INVALID);
  assert(pss_legacy_put(&services,"points_utc_occ",bytes,sizeof(bytes))==RISC_KEY_VALUE_INVALID&&calls==before);
  assert(pss_read(&services,POINTS_CATALOG_FILE,0,bytes,sizeof(bytes),&n,&revision)==0);
  assert(pss_replace(&services,POINTS_CATALOG_FILE,0,bytes,sizeof(bytes))==0);
  kv_rc=RISC_KEY_VALUE_BUSY;unsigned kv_before=kv_reads;
  assert(pss_legacy_get(&services,"points_cfg",bytes,sizeof(bytes),&n)==RISC_KEY_VALUE_BUSY&&kv_reads==kv_before+1&&!services.terminal);
  assert(pss_legacy_put(&services,"points_cfg",bytes,sizeof(bytes))==RISC_KEY_VALUE_BUSY&&!services.terminal);kv_rc=0;
  file_rc=RISC_APP_DATA_BUSY;assert(pss_stat(&services,POINTS_CATALOG_FILE,&n,&revision)==RISC_APP_DATA_BUSY&&!services.terminal);
  closed();before=calls;assert(pss_stat(&services,POINTS_CATALOG_FILE,&n,&revision)==RISC_APP_DATA_UNAVAILABLE&&calls==before);
 }
 fixture();assert(pss_open(&services));file_rc=RISC_APP_DATA_CONTEXT;assert(pss_stat(&services,POINTS_CATALOG_FILE,&n,&revision)==RISC_APP_DATA_RETAINED);terminal();
 fixture();assert(pss_open(&services));file_rc=99;pss_read(&services,POINTS_CATALOG_FILE,0,bytes,sizeof(bytes),&n,&revision);terminal();
 fixture();assert(pss_open(&services));kv_rc=RISC_KEY_VALUE_CONTEXT;assert(pss_legacy_get(&services,"points_cfg",bytes,sizeof(bytes),&n)==RISC_KEY_VALUE_CONTEXT);terminal();
 fixture();assert(pss_open(&services));kv_rc=55;assert(pss_legacy_put(&services,"points_cfg",bytes,sizeof(bytes))==RISC_KEY_VALUE_CONTEXT);terminal();
}
static void clock_paths(void){
 uint32_t now,expected;
 fixture();assert(pss_open(&services));assert(alarm_calendar_seconds(2026,11,4,12,30,0,&expected));
 assert(pss_now(&services,&now)&&now==expected&&raw_reads==1&&native_reads==0);assert(!pss_now(&services,NULL));closed();
 fixture();assert(pss_open(&services));rtc_ok=false;now=77;assert(!pss_now(&services,&now)&&now==77&&raw_reads==1&&!services.terminal);closed();
 fixture();assert(pss_open(&services));calendar.month=19;now=77;assert(!pss_now(&services,&now)&&now==77);closed();
 fixture();assert(pss_open(&services));calendar.weekday=7;assert(!pss_now(&services,&now));closed();
 fixture();assert(pss_open(&services));rtc.struct_size=8;assert(!pss_now(&services,&now)&&!raw_reads);closed();
 fixture();assert(pss_open(&services));has_rtc=false;assert(!pss_now(&services,&now)&&!services.terminal);closed();
 fixture();has_v2=true;assert(pss_open(&services));assert(pss_now(&services,&now)&&now==(uint32_t)(snapshot.epoch_seconds-946684800)&&native_reads==1&&!raw_reads);closed();
 fixture();has_v2=true;assert(pss_open(&services));snapshot.validity=RISC_REALTIME_UNSET;snapshot.epoch_seconds=0;now=77;assert(!pss_now(&services,&now)&&now==77&&!services.terminal);closed();
 fixture();has_v2=true;assert(pss_open(&services));native_rc=RISC_REALTIME_IO;assert(!pss_now(&services,&now)&&!services.terminal);closed();
 fixture();has_v2=true;assert(pss_open(&services));snapshot.struct_size=8;assert(!pss_now(&services,&now)&&!services.terminal);closed();
 fixture();has_v2=true;assert(pss_open(&services));has_native=false;assert(!pss_now(&services,&now)&&!raw_reads&&!services.terminal);closed();
 fixture();has_v2=true;assert(pss_open(&services));native.read=NULL;assert(!pss_now(&services,&now)&&!native_reads&&!services.terminal);closed();
 fixture();has_v2=true;assert(pss_open(&services));snapshot.epoch_seconds=946684799;assert(!pss_now(&services,&now));closed();
 now=77;assert(!pss_utc_seconds(946684799,&now)&&now==77);assert(pss_utc_seconds(946684800,&now)&&!now);assert(pss_utc_seconds(INT32_MAX,&now));assert(!pss_utc_seconds(INT64_C(2147483648),&now));
 fixture();has_v2=true;assert(pss_open(&services));native_rc=RISC_REALTIME_CONTEXT;assert(!pss_now(&services,&now));terminal();
 fixture();has_v2=true;assert(pss_open(&services));fail_release_at=releases+1;assert(!pss_now(&services,&now));terminal();
 fixture();assert(pss_open(&services));fail_release_at=releases+1;assert(!pss_now(&services,&now));terminal();
}
static void custody(void){
 /* Revoke at every operation in startup, including preference and projection reads. */
 fixture();catalog(true);assert(pss_open(&services));unsigned startup=calls;closed();
 for(unsigned i=1;i<=startup;i++){fixture();catalog(true);revoke_at=i;assert(!pss_open(&services));terminal();}
 fixture();assert(pss_open(&services));owner_live=false;assert(!pss_live(&services));terminal();
 fixture();assert(pss_open(&services));prepared=false;unsigned before=calls;assert(!pss_status(&services)&&!services.terminal&&calls==before);prepared=true;assert(pss_status(&services));closed();
 fixture();fail_acquire_at=1;grant_damage=1;assert(!pss_open(&services));terminal();
 fixture();grant_damage=2;assert(!pss_open(&services));terminal();
 fixture();grant_damage=3;assert(!pss_open(&services));terminal();
 fixture();assert(pss_open(&services));release_damage=1;assert(!pss_close(&services));terminal();
 fixture();assert(pss_open(&services));alarm_rc=ALARM_OUTPUT;assert(!pss_status(&services));terminal();
 fixture();catalog(true);assert(pss_open(&services));projection_rc=ALARM_RETAINED;assert(!pss_status(&services));terminal();
 for(unsigned native_clock=0;native_clock<2;native_clock++)for(unsigned offset=1;offset<=3;offset++){
  fixture();has_v2=native_clock;assert(pss_open(&services));revoke_at=calls+offset;uint32_t now=77;
  assert(!pss_now(&services,&now));terminal();assert(now==77);
 }
}
int main(void){
 negotiations();malformed();wrappers();clock_paths();custody();
 puts("Points scene services: ABI/domain negotiation, optional providers, malformed catalog fencing, scoped keys, clocks, cleanup and adversarial revocation passed");return 0;
}
