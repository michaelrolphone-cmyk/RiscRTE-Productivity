/* Provider-only extension for the production-profile idle controller tests. */
#pragma once
#include "PortableBluetoothControl.h"
#include "PortableRadioPolicy.h"
#include "WifiApi.h"
static unsigned idle_navigation_resets,idle_broadcast_grants;
static unsigned idle_radio_enables,idle_radio_disables,idle_wifi_disconnects;
static bool idle_bluetooth;
static bool idle_ble_set(void *context,bool enabled) {
 (void)context;fx_io();idle_bluetooth=enabled;
 if(enabled)idle_radio_enables++;else idle_radio_disables++;
 return true;
}
static bool idle_ble_status(void *context,uint8_t *state) {
 (void)context;fx_io();*state=idle_bluetooth?PORTABLE_BLUETOOTH_ON:PORTABLE_BLUETOOTH_OFF;return true;
}
static wifi_link_t idle_wifi_status(void *context) {(void)context;fx_io();return WIFI_LINK_DOWN;}
static bool idle_wifi_disconnect(void *context) {(void)context;fx_io();idle_wifi_disconnects++;return true;}
static bool idle_wifi_connect(void *context,const char *ssid,const char *password) {
 (void)context;(void)ssid;(void)password;fx_io();assert(!"Foreground resume must not start a Wi-Fi connection");return false;
}
static const wifi_api_v1 idle_wifi={.api_version=1,.struct_size=sizeof(idle_wifi),.connect=idle_wifi_connect,.status=idle_wifi_status,.disconnect_checked=idle_wifi_disconnect};
static const portable_bluetooth_control_v1 idle_ble={.api_version=1,.struct_size=sizeof(idle_ble),.set_enabled=idle_ble_set,.status=idle_ble_status};
static bool idle_radio_acquire(const char *name,uint32_t version,uint64_t instance,const void **api) {
 if(!strcmp(name,"net.wifi")){assert(version==1&&instance==15);*api=&idle_wifi;return true;}
 if(!strcmp(name,"bluetooth.hci")){assert(version==1&&instance==16);*api=&idle_ble;return true;}
 return false;
}
