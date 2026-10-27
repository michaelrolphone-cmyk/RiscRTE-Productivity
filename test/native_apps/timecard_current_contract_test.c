/* Actual authoritative controller + current checked facades; media are doubles. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define PORTABLE_NOVA_UI
#define PORTABLE_RF_NATIVE_CANVAS
#define PORTABLE_APP_OWNS_TOUCH_CHROME
#define TIMECARD_CURRENT_PROFILE
#define TIMECARD_APP_DATA
#define TIMECARD_PAPER
#define TIMECARD_NATIVE_TIME
#define PORTABLE_NATIVE_TIME_TOOLBAR
#define PORTABLE_NATIVE_CUSTODY_FENCE
#define ALARM_SERVICE_TAGGED_V2
#define PORTABLE_TIMECARD_NATIVE_UI
#define PORTABLE_NATIVE_SCENE
#include "../../Apps/timecard_portable.c"
static bool retained;static unsigned calls,pref_writes,replaces,reads,stats,grants;
static uint32_t tick;static int kv_result,stat_result,read_result,replace_result,post_stat_result;
static uint64_t revision=1;static char disk[JSON_CAPACITY]="{\"days\":[{\"d\":20261025,\"in\":540}]}";
static unsigned preference=PORTABLE_TIME_FORMAT_24;
static void io(void){assert(!retained);++calls;}
bool portable_adapter_retained(void){return retained;}
void portable_adapter_retain(void){assert(!retained);retained=true;}
static uint32_t millis(void){io();return tick;}
static const t5_app_api_v1 fake_app={.abi_version=T5_APP_ABI_VERSION,.struct_size=sizeof(fake_app),.millis=millis};
static int32_t get(void*c,const char*k,void*d,uint32_t cap,uint32_t*n){(void)c;io();assert(!strcmp(k,PORTABLE_TIME_FORMAT_KEY));*n=0;if(kv_result)return kv_result;assert(cap>=4);uint8_t b[]={0x54,1,preference,preference^0xa5};memcpy(d,b,4);*n=4;return 0;}
static int32_t put(void*c,const char*k,const void*d,uint32_t n){(void)c;(void)k;(void)d;(void)n;io();++pref_writes;assert(!"No implicit preference writes");return RISC_KEY_VALUE_IO;}
static const risc_key_value_v1 prefs={1,sizeof(prefs),0,get,put};
static int32_t stat_file(void*c,const char*n,uint32_t*size,uint64_t*r){(void)c;io();++stats;assert(!strcmp(n,"timecard.json"));*size=0;*r=0;int result=replaces&&post_stat_result?post_stat_result:stat_result;if(result)return result;*size=strlen(disk);*r=revision;return 0;}
static int32_t read_file(void*c,const char*n,uint64_t r,void*d,uint32_t cap,uint32_t*used,uint64_t*actual){(void)c;io();++reads;assert(!strcmp(n,"timecard.json"));*used=0;*actual=0;if(read_result)return read_result;if(r!=revision)return RISC_APP_DATA_STALE;assert(cap>=strlen(disk));memcpy(d,disk,strlen(disk));*used=strlen(disk);*actual=revision;return 0;}
static int32_t replace_file(void*c,const char*n,uint64_t r,const void*d,uint32_t size){(void)c;io();assert(!strcmp(n,"timecard.json"));if(replace_result)return replace_result;if(r!=revision)return RISC_APP_DATA_STALE;assert(size<sizeof(disk));memcpy(disk,d,size);disk[size]=0;++revision;++replaces;return 0;}
static const risc_app_data_v1 files={1,sizeof(files),0,stat_file,read_file,replace_file};
static bool acquire(const char*n,uint32_t v,uint64_t i,risc_runtime_capability_v1*g){io();assert(v==1&&i==1);*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g),.slot=++grants,.generation=1};if(!strcmp(n,RISC_KEY_VALUE_CAPABILITY))g->api=&prefs;else {assert(!strcmp(n,RISC_APP_DATA_CAPABILITY));g->api=&files;}return true;}
static bool release(risc_runtime_capability_v1*g){io();assert(grants);--grants;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
static bool launch(const char*p){io();assert(!strcmp(p,"springboard.elf"));return true;}
static bool retain(void){portable_adapter_retain();return true;}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.acquire=acquire,.release=release,.request_launch=launch,.retain_invocation=retain};
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){return v==1?&runtime:0;}
int main(int argc,char**argv){
 assert(argc==2);unsigned test=(unsigned)atoi(argv[1]);app=&fake_app;
 if(test==0)kv_result=RISC_KEY_VALUE_BUSY;
 tcp_native_open();assert(!retained&&grants==2&&!pref_writes);tcp_files=timecard_portable_file_storage();storage=&tcp_store;
 if(test==0){
  assert(!tcp_current_format_known&&tcp_current_format_pending);unsigned n=calls;tcp_current_format_refresh(false);assert(calls==n+1); /* timestamp only */
  tick=249;tcp_current_format_refresh(false);assert(!tcp_current_format_known);tick=250;kv_result=0;tcp_current_format_refresh(false);assert(tcp_current_format_known&&tcp_time_format==PORTABLE_TIME_FORMAT_24);
  kv_result=RISC_KEY_VALUE_BUSY;tcp_current_format_refresh(true);assert(tcp_current_format_known&&tcp_current_format_pending&&tcp_time_format==PORTABLE_TIME_FORMAT_24);
  kv_result=RISC_KEY_VALUE_IO;tick=500;tcp_current_format_refresh(false);assert(tcp_time_format==PORTABLE_TIME_FORMAT_24&&!retained);
  kv_result=0;preference=PORTABLE_TIME_FORMAT_12;tick=750;tcp_current_format_refresh(false);assert(tcp_time_format==PORTABLE_TIME_FORMAT_12&&!tcp_current_format_pending&&!pref_writes);
 } else {
  assert(tcp_reload()&&day_count==1&&days[0].punches[0]==540);tc_day_t before=days[0];
  if(test==1)stat_result=RISC_APP_DATA_BUSY;
  if(test==2)read_result=RISC_APP_DATA_BUSY;
  if(test==1||test==2){assert(!tcp_reload()&&!store_ready&&day_count==1&&!memcmp(&before,&days[0],sizeof(before))&&!replaces&&!retained&&strstr(status_text,"busy"));}
  if(test==3){replace_result=RISC_APP_DATA_BUSY;assert(!tcp_mutate(20261025,0,600)&&!replaces&&!retained&&!store_ready&&!memcmp(&before,&days[0],sizeof(before))&&strstr(status_text,"busy"));}
  if(test==4){post_stat_result=RISC_APP_DATA_BUSY;assert(!tcp_mutate(20261025,0,600)&&replaces==1&&!retained&&tcp_data.status==RISC_APP_DATA_COMMIT_UNKNOWN&&!memcmp(&before,&days[0],sizeof(before))&&strstr(status_text,"unconfirmed"));post_stat_result=0;assert(tcp_reload()&&days[0].punches[0]==600&&replaces==1);}
  if(test==5){replace_result=RISC_APP_DATA_STALE;assert(!tcp_mutate(20261025,0,600)&&!replaces&&!retained&&tcp_data.status==RISC_APP_DATA_STALE);}
  if(test==7){tcp_editor=true;tcp_cookie=make_cookie(20261025,0,0);strcpy(tcp_entry,"10:00");replace_result=RISC_APP_DATA_BUSY;tcp_editor_done();assert(tcp_editor&&!strcmp(tcp_entry,"10:00")&&!replaces&&!retained&&!memcmp(&before,&days[0],sizeof(before)));replace_result=0;tcp_editor_done();assert(!tcp_editor&&replaces==1&&get_day(20261025).punches[0]==600);}
  if(test==8){post_stat_result=RISC_APP_DATA_RETAINED;assert(!tcp_mutate(20261025,0,600)&&retained&&replaces==1);unsigned n=calls;tcp_dependencies_close();assert(calls==n);puts("PASS retained post-replace verification preserves committed uncertainty without calls");return 0;}
  if(test==9){kv_result=RISC_KEY_VALUE_CONTEXT;tcp_current_format_refresh(true);assert(retained);unsigned n=calls;tcp_current_format_refresh(true);tcp_dependencies_close();assert(calls==n);puts("PASS unknown preference custody remains terminal");return 0;}
  if(test==10){day_count=MAX_DAYS;for(int i=0;i<MAX_DAYS;++i){days[i]=blank_day(add_days(20250101,i));days[i].punches[0]=540;}assert(!tcp_mutate(20270101,0,600)&&day_count==MAX_DAYS&&!replaces&&strstr(status_text,"400"));for(int i=0;i<MAX_DAYS;++i)assert(days[i].punches[0]==540);}
  if(test==11){replace_result=RISC_APP_DATA_NO_SPACE;assert(!tcp_mutate(20261025,0,600)&&!replaces&&!retained&&!memcmp(&before,&days[0],sizeof(before)));}
  if(test==12){strcpy(disk,"{broken");assert(!tcp_reload()&&!store_ready&&day_count==1&&!memcmp(&before,&days[0],sizeof(before))&&!replaces);}
  if(test==13){tick=UINT32_MAX-100;kv_result=RISC_KEY_VALUE_BUSY;tcp_current_format_refresh(true);unsigned n=calls;tick=148;tcp_current_format_refresh(false);assert(calls==n+1&&tcp_time_format==PORTABLE_TIME_FORMAT_24);tick=149;kv_result=0;preference=PORTABLE_TIME_FORMAT_12;tcp_current_format_refresh(false);assert(tcp_time_format==PORTABLE_TIME_FORMAT_12&&!tcp_current_format_pending);}
  if(test==14){stat_result=RISC_APP_DATA_NOT_FOUND;assert(tcp_reload()&&store_ready&&day_count==0&&!replaces);}
  if(test==15){kv_result=RISC_KEY_VALUE_NOT_FOUND;tcp_current_format_refresh(true);assert(tcp_current_format_known&&!tcp_current_format_pending&&tcp_time_format==PORTABLE_TIME_FORMAT_12&&!pref_writes);}
  if(test==16){post_stat_result=RISC_APP_DATA_IO;assert(!tcp_mutate(20261025,0,600)&&replaces==1&&!retained&&tcp_data.status==RISC_APP_DATA_COMMIT_UNKNOWN&&!memcmp(&before,&days[0],sizeof(before)));post_stat_result=0;assert(tcp_reload()&&days[0].punches[0]==600&&replaces==1);}
  if(test==19){kv_result=RISC_KEY_VALUE_BUSY;tick=10;tcp_current_format_refresh(true);tick=0x90000000u;kv_result=0;preference=PORTABLE_TIME_FORMAT_12;tcp_current_format_refresh(false);assert(tcp_time_format==PORTABLE_TIME_FORMAT_12&&!tcp_current_format_pending);}
  if(test==18){preference=42;tcp_current_format_refresh(true);assert(tcp_current_format_known&&!tcp_current_format_pending&&tcp_time_format==PORTABLE_TIME_FORMAT_12&&!pref_writes);}
  if(test==17){stat_result=-12345;assert(!tcp_reload()&&retained);unsigned n=calls;tcp_dependencies_close();assert(calls==n);puts("PASS unknown AppData custody terminal without later calls");return 0;}
  if(test==6){replace_result=RISC_APP_DATA_RETAINED;assert(!tcp_mutate(20261025,0,600)&&retained&&!replaces);unsigned n=calls;tcp_dependencies_close();assert(calls==n);puts("PASS current Timecard retained operation has no later calls");return 0;}
 }
 tcp_dependencies_close();assert(!grants&&!pref_writes);printf("PASS current Timecard contract %u; authoritative ledger/private namespace/BUSY boundaries\n",test);
}
