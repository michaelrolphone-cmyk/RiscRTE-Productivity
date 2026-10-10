#ifndef PRODUCTIVITY_POINTS_NATIVE_TIME_H
#define PRODUCTIVITY_POINTS_NATIVE_TIME_H
/* Opt-in app-local custody and read-only native UTC sampler. No RTC recovery,
 * time-control grant, timezone writes or service occurrence access. */
#include "PortableRealtimeClient.h"
#include "AlarmServiceV2.h"
#include "PortableTimeZonePreference.h"
#ifdef PORTABLE_NATIVE_TIME_TOOLBAR
#include "PortableNativeTimeToolbar.h"
#else
/* Controller-only hosts provide the same hidden adapter fence. */
__attribute__((visibility("hidden"))) void portable_adapter_retain(void);
#endif
#include "PointsUtcSchedule.h"
#ifdef PORTABLE_BLE_BROADCAST
bool portable_broadcast_stop(void);
#endif
static bool points_retained;
static portable_realtime_client points_clock;
static portable_timezone_rule points_zone_rule;
static char points_zone[PORTABLE_TIMEZONE_ID_BYTES];
static unsigned points_zone_index;
static int points_clock_status,points_zone_status;
static points_projection points_preview;
static const risc_key_value_v1 *points_storage_source,*points_preferences_source;
static risc_key_value_v1 points_storage_guarded,points_preferences_guarded;
static bool points_live(void) {
    if(points_retained)return false;
#ifdef PORTABLE_NATIVE_TIME_TOOLBAR
    if(portable_adapter_retained()){points_retained=true;return false;}
#endif
#ifdef PORTABLE_ALARM_CLIENT
    if(portable_app_sleep_retained()){points_retained=true;return false;}
#endif
    return true;
}
static void points_retain(void) {
    if(points_retained)return;
    points_retained=true;portable_realtime_stop(&points_clock,true);
    portable_adapter_retain();
}
static int points_phase(void *context) {
    (void)context;return points_live()?PORTABLE_REALTIME_GUARD_SAFE:PORTABLE_REALTIME_GUARD_RETAINED;
}
static bool points_native_status(int rc) {
    if(rc==PORTABLE_REALTIME_CONTEXT||rc==PORTABLE_REALTIME_UNCERTAIN||rc==PORTABLE_REALTIME_RETAINED)points_retain();
    return points_live();
}
static bool points_kv_status(int32_t rc) {
    if(rc!=RISC_KEY_VALUE_OK&&rc!=RISC_KEY_VALUE_NOT_FOUND&&rc!=RISC_KEY_VALUE_BUFFER_SMALL&&
       rc!=RISC_KEY_VALUE_INVALID&&rc!=RISC_KEY_VALUE_IO)points_retain();
    return points_live();
}
#ifdef PORTABLE_BLE_BROADCAST
static bool points_storage_ready(void) {
    if(!points_live())return false;
    if(!portable_broadcast_stop()){points_retain();return false;}
    return points_live();
}
#else
#define points_storage_ready points_live
#endif
static int32_t points_guarded_get(void *context,const char *key,void *out,uint32_t cap,uint32_t *size) {
    if(!points_storage_ready())return RISC_KEY_VALUE_CONTEXT;
    const risc_key_value_v1 *kv=context;int32_t rc=kv->get(kv->context,key,out,cap,size);
    if(!points_kv_status(rc))return RISC_KEY_VALUE_CONTEXT;
    return rc;
}
static int32_t points_guarded_put(void *context,const char *key,const void *bytes,uint32_t size) {
    if(!points_storage_ready())return RISC_KEY_VALUE_CONTEXT;
    const risc_key_value_v1 *kv=context;int32_t rc=kv->put(kv->context,key,bytes,size);
    if(!points_kv_status(rc))return RISC_KEY_VALUE_CONTEXT;
    return rc;
}
static bool points_service_result(int32_t rc) {
    if(rc==ALARM_RETAINED||rc==ALARM_OUTPUT||rc<ALARM_RETAINED||rc>ALARM_PENDING)points_retain();
    return points_live();
}
static bool points_native_sample(const risc_runtime_api_v1 *rt,const risc_key_value_v1 *prefs,uint32_t *seconds,
                                 portable_timezone_civil *local) {
    if(!points_live()||!seconds)return false;
    points_clock_status=portable_realtime_open(&points_clock,rt,PORTABLE_REALTIME_READER,
        PORTABLE_REALTIME_TIMER_ONLY,points_phase,NULL);
    if(!points_native_status(points_clock_status)||points_clock_status!=PORTABLE_REALTIME_OK)return false;
    risc_realtime_snapshot_v1 sample={0};
    points_clock_status=portable_realtime_read(&points_clock,&sample);
    if(!points_native_status(points_clock_status))return false;
    int closed=portable_realtime_close(&points_clock);
    if(!points_native_status(closed)||closed!=PORTABLE_REALTIME_OK)return false;
    if(points_clock_status!=PORTABLE_REALTIME_OK||!points_utc_from_unix(sample.epoch_seconds,seconds))return false;
    char zone[PORTABLE_TIMEZONE_ID_BYTES];
    points_zone_status=portable_timezone_preference_load(prefs,zone);
    if(!points_live()||(points_zone_status!=PORTABLE_TIMEZONE_LOADED&&points_zone_status!=PORTABLE_TIMEZONE_MISSING))return false;
    portable_timezone_rule rule;int index=portable_timezone_find(zone,sizeof(zone));
    if(index<0||portable_timezone_resolve(zone,sizeof(zone),&rule)!=PORTABLE_TIMEZONE_OK)return false;
    portable_timezone_civil civil;
    if(portable_timezone_utc_to_local(&rule,sample.epoch_seconds,&civil,NULL)!=PORTABLE_TIMEZONE_OK)return false;
    points_zone_rule=rule;points_zone_index=(unsigned)index;memcpy(points_zone,zone,sizeof(points_zone));
    if(local)*local=civil;
    return true;
}
#endif
