/* Actual Timecard controller must pause publication before native storage. */
#define PORTABLE_BLE_BROADCAST 1
#define TIMECARD_APPDATA_TEST_MAIN original_timecard_appdata_main
#include "timecard_appdata_profile_test.c"
#undef TIMECARD_APPDATA_TEST_MAIN
static bool radio_live,can_pause=true;
static unsigned pause_calls,data_calls,pause_failures,retry_yields,awaiting_data;
static void fixture_broadcast_yield(uint32_t ms){assert(ms==50&&data_calls==awaiting_data);retry_yields++;assert(retry_yields<4);if(retry_yields==2)can_pause=true;}
bool portable_broadcast_stop(void){++pause_calls;if(!can_pause){pause_failures++;return false;}radio_live=false;return true;}
static int32_t checked_stat(void*c,const char*n,uint32_t*z,uint64_t*r){assert(!radio_live);++data_calls;return ad_stat(c,n,z,r);}
static int32_t checked_read(void*c,const char*n,uint64_t e,void*b,uint32_t cap,uint32_t*z,uint64_t*r){assert(!radio_live);++data_calls;return ad_read(c,n,e,b,cap,z,r);}
static int32_t checked_replace(void*c,const char*n,uint64_t e,const void*b,uint32_t z){assert(!radio_live);++data_calls;return ad_replace(c,n,e,b,z);}
static const risc_app_data_v1 checked_data={1,sizeof(checked_data),NULL,checked_stat,checked_read,checked_replace};
static void fresh(void){can_pause=true;radio_live=false;ad_init();assert(tcp_bind_data(&checked_data,&runtime));pause_calls=data_calls=0;radio_live=true;assert(tcp_reload());assert(pause_calls&&data_calls&&!radio_live);}
int main(void){
 fresh();radio_live=true;assert(tcp_mutate(20261005,0,480));assert(!radio_live&&writes==1);
 radio_live=true;assert(tcp_reload());assert(!radio_live&&get_day(20261005).punches[0]==480);
 // A native retained result cannot be followed by another radio or file call.
 ad_read_error=RISC_APP_DATA_RETAINED;radio_live=true;assert(!tcp_reload()&&tcp_storage_retained());
 unsigned p=pause_calls,d=data_calls;assert(!tcp_reload()&&pause_calls==p&&data_calls==d);
 fresh();can_pause=false;radio_live=true;p=pause_calls;d=data_calls;awaiting_data=d;retry_yields=pause_failures=0;
 assert(tcp_reload()&&!tcp_storage_retained()&&pause_calls==p+3&&data_calls==d+1);
 assert(retry_yields==2&&pause_failures==2&&!radio_live);
 // Exercise the real app entry's initial retained read, including no launch or
 // dependency release after the read has already paused advertising.
 can_pause=true;radio_live=false;ad_init();file_present=true;strcpy(persisted,"{\"days\":[]}");
 ad_read_error=RISC_APP_DATA_RETAINED;radio_live=true;poll_count=launches=grants=0;app_main();
 assert(!radio_live&&!launches&&grants==3&&tcp_storage_retained());
 puts("Timecard production app: BLE pause before stat/read/replace, pause refusal and retained no-further-I/O passed");
}
