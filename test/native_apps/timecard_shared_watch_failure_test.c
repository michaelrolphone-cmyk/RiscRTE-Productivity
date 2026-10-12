/* Watch's real app-data facade and shared-input liveness fences. */
#define PORTABLE_BLE_BROADCAST
#define PORTABLE_CONTEXTS_CLIENT
#define TIMECARD_APP_DATA
#define main shared_controller_main
#include "timecard_shared_input_test.c"
#undef main
static const char *loss_site;
static unsigned boundary_calls,background_calls;
static bool background_false;
static int pref_result;
static uint64_t revision=1;
static void boundary(const char *site){assert(!runtime_lost);boundary_calls++;if(loss_site&&!strcmp(site,loss_site))runtime_lost=true;}
static void fixture_broadcast_yield(uint32_t ms){(void)ms;assert(!runtime_lost&&!modal_retained);}
bool portable_contexts_stop(void){boundary("contexts-stop");background_calls++;return !(background_false&&loss_site&&!strcmp(loss_site,"contexts-stop"));}
bool portable_broadcast_stop(void){boundary("broadcast-stop");background_calls++;return !(background_false&&loss_site&&!strcmp(loss_site,"broadcast-stop"));}
static int32_t fault_get(void *c,const char *key,void *out,uint32_t cap,uint32_t *size){boundary("pref-get");if(pref_result){*size=0;return pref_result;}return kv_get(c,key,out,cap,size);}
static const risc_key_value_v1 fault_prefs={1,sizeof(fault_prefs),NULL,fault_get,NULL};
static int32_t ad_stat(void *c,const char *name,uint32_t *size,uint64_t *rev){
    (void)c;assert(!strcmp(name,TCP_APPDATA_NAME));*size=file_present?(uint32_t)strlen(persisted):0;*rev=file_present?revision:0;boundary("stat");return file_present?0:RISC_APP_DATA_NOT_FOUND;
}
static int32_t ad_read(void *c,const char *name,uint64_t expected,void *data,uint32_t cap,uint32_t *size,uint64_t *rev){
    (void)c;assert(!strcmp(name,TCP_APPDATA_NAME)&&expected==revision&&cap>=strlen(persisted));*size=(uint32_t)strlen(persisted);*rev=revision;memcpy(data,persisted,*size);boundary("read");return 0;
}
static int32_t ad_replace(void *c,const char *name,uint64_t expected,const void *data,uint32_t size){
    (void)c;assert(!strcmp(name,TCP_APPDATA_NAME)&&expected==(file_present?revision:0)&&size<sizeof(persisted));memcpy(persisted,data,size);persisted[size]=0;revision++;file_present=true;writes++;boundary("replace");return 0;
}
static const risc_app_data_v1 ad_api={1,sizeof(ad_api),NULL,ad_stat,ad_read,ad_replace};
static const risc_app_data_v1 *fixture_appdata_api(void){return &ad_api;}
static bool loss_rtc(void *c,twatch_rtc_time_v1 *out){bool rc=rtc_read(c,out);boundary("rtc");return rc;}
static const twatch_rtc_api_v1 loss_rtc_api={.api_version=2,.struct_size=sizeof(loss_rtc_api),.read=loss_rtc};
static bool loss_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *g){
    bool rc=shared_acquire(name,version,instance,g);if(rc){g->slot=++boundary_calls;g->generation=1;if(!strcmp(name,"storage.key-value"))g->api=&fault_prefs;}boundary(name);return rc;
}
static bool loss_release(risc_runtime_capability_v1 *g){
    bool rc=shared_release(g);if(rc)*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};boundary("release");return rc;
}
int main(int argc,char **argv){
    assert(argc==2);initialize();shared_runtime.acquire=loss_acquire;shared_runtime.release=loss_release;
    tcp_text_client.runtime=&shared_runtime;assert(tcp_bind_data(&ad_api,&shared_runtime));tcp_files=&tcp_ad_files;assert(tcp_reload());
    const char *kind=argv[1];unsigned before_present=presentations;
    if(!strcmp(kind,"stat")||!strcmp(kind,"read")||!strcmp(kind,"replace")){
        assert(tcp_mutate(20261005,0,480));loss_site=kind;
        if(!strcmp(kind,"replace")){begin(0);complete(RISC_TEXT_ENTRY_ACCEPTED,"9:15");}
        else assert(!tcp_reload());
    }else if(!strncmp(kind,"background-",11)){
        loss_site=strstr(kind,"contexts")?"contexts-stop":"broadcast-stop";
        background_false=strstr(kind,"false")!=NULL;assert(!tcp_reload());
    }else if(!strncmp(kind,"prefs-",6)){
        pref_result=!strcmp(kind,"prefs-context")?RISC_KEY_VALUE_CONTEXT:!strcmp(kind,"prefs-unknown")?-333:RISC_KEY_VALUE_BUSY;
        tcp_dependencies_open();
        if(pref_result==RISC_KEY_VALUE_BUSY){assert(!tcp_retained()&&tcp_grant_count==3&&tcp_time_format==PORTABLE_TIME_FORMAT_12);tcp_dependencies_close();puts("Watch clean preference Busy defaults without retention PASS");return 0;}
    }else if(!strcmp(kind,"rtc")){
        tcp_rtc=&loss_rtc_api;loss_site=kind;tcp_draw();assert(presentations==before_present);
    }else if(!strcmp(kind,"release")){
        tcp_dependencies_open();assert(tcp_grant_count==3);loss_site=kind;tcp_dependencies_close();
    }else {
        loss_site=kind;tcp_dependencies_open();
    }
    assert((runtime_lost||modal_retained)&&tcp_retained());unsigned io=boundary_calls,modal=modal_calls,draws=presentations,launch_count=launches;
    tcp_draw();tcp_reload();tcp_mutate(20261005,0,600);tcp_punch(0);tcp_text_step();tcp_dependencies_close();app_main();
    assert(boundary_calls==io&&modal_calls==modal&&presentations==draws&&launches==launch_count);
    printf("Watch callback retention %s PASS (calls=%u writes=%u)\n",kind,boundary_calls,writes);return 0;
}
