#pragma once
#ifdef PORTABLE_BLE_BROADCAST
#include "TelemetryBroadcastV1.h"
static bool bt_active,bt_pause_fail;
static unsigned bt_pauses,bt_steps;
static bool bt_step(void *ctx,bool allow,const telemetry_broadcast_policy_v1 *policy) {
 (void)ctx;fx_io();assert(policy&&policy->struct_size==sizeof(*policy));bt_steps++;
 bt_active=allow&&policy->enabled&&policy->settings_valid&&policy->radios_allowed;return true;
}
static bool bt_pause(void *ctx) {(void)ctx;fx_io();bt_pauses++;if(bt_pause_fail)return false;bt_active=false;return true;}
static bool bt_status(void *ctx,telemetry_broadcast_status_v1 *out) {
 (void)ctx;fx_io();assert(out&&out->struct_size==sizeof(*out));
 *out=(telemetry_broadcast_status_v1){.struct_size=sizeof(*out),.state=bt_active?TELEMETRY_BROADCAST_LIVE:TELEMETRY_BROADCAST_OFF};return true;
}
static int32_t bt_enumerate(void *ctx,uint32_t i,risc_telemetry_field_v1 *out) {(void)ctx;(void)i;(void)out;fx_io();return 0;}
static int32_t bt_read(void *ctx,uint32_t i,int32_t *out) {(void)ctx;(void)i;(void)out;fx_io();return 0;}
static const telemetry_broadcast_v1 fx_broadcast={1,sizeof(fx_broadcast),NULL,bt_step,bt_pause,bt_status,bt_enumerate,bt_read};
#endif
