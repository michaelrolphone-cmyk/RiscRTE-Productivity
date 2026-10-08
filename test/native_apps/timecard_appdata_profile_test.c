/* Actual Timecard source/controller/file facade over the versioned prototype. */
#define TIMECARD_APP_DATA
#define main timecard_base_fixture_main
#include "timecard_portable_test.c"
#undef main
static uint64_t ad_version=1;
static int32_t ad_stat_error,ad_read_error,ad_write_error;
static bool ad_commit_error;
static int32_t ad_stat(void *context,const char *name,uint32_t *size,uint64_t *revision){
 (void)context;assert(!strcmp(name,TCP_APPDATA_NAME));*size=0;*revision=0;
 if(ad_stat_error)return ad_stat_error;
 if(!file_present)return RISC_APP_DATA_NOT_FOUND;
 *size=(uint32_t)strlen(persisted);*revision=ad_version;return 0;
}
static int32_t ad_read(void *context,const char *name,uint64_t expected,void *buffer,uint32_t capacity,uint32_t *size,uint64_t *revision){
 (void)context;assert(!strcmp(name,TCP_APPDATA_NAME));*size=0;*revision=0;
 if(ad_read_error)return ad_read_error;
 if(expected!=ad_version)return RISC_APP_DATA_STALE;
 size_t got=0;if(!read_file(STORE_PATH,buffer,capacity,&got))return RISC_APP_DATA_IO;
 *size=(uint32_t)got;*revision=ad_version;return 0;
}
static int32_t ad_replace(void *context,const char *name,uint64_t expected,const void *data,uint32_t size){
 (void)context;assert(!strcmp(name,TCP_APPDATA_NAME));
 if(ad_write_error)return ad_write_error;
 if(expected!=(file_present?ad_version:0))return RISC_APP_DATA_STALE;
 if(!write_file(STORE_PATH,data,size))return RISC_APP_DATA_IO;
 ad_version++;return ad_commit_error?RISC_APP_DATA_COMMIT_UNKNOWN:0;
}
static const risc_app_data_v1 ad_api={1,sizeof(ad_api),NULL,ad_stat,ad_read,ad_replace};
static const risc_app_data_v1 *fixture_appdata_api(void){return &ad_api;}
static void ad_init(void){init();tcp_data=(tcp_appdata){0};assert(tcp_appdata_bind(&tcp_data,&ad_api));tcp_files=&tcp_ad_files;ad_stat_error=ad_read_error=ad_write_error=0;ad_commit_error=false;ad_version=1;assert(tcp_reload());}
#ifndef TIMECARD_APPDATA_TEST_MAIN
#define TIMECARD_APPDATA_TEST_MAIN main
#endif
int TIMECARD_APPDATA_TEST_MAIN(void){
 ad_init();assert(day_count==0&&store_ready);assert(tcp_mutate(20261005,0,480));assert(writes==1&&tcp_data.revision==2);assert(tcp_mutate(20261005,3,1020)&&tcp_data.revision==3);
 ad_stat_error=RISC_APP_DATA_UNAVAILABLE;assert(!tcp_reload()&&day_count==1&&!store_ready);assert(!tcp_mutate(20261006,0,500)&&writes==2);ad_stat_error=0;assert(tcp_reload());
 ad_version++;assert(!tcp_mutate(20261005,0,600)&&tcp_data.status==RISC_APP_DATA_STALE&&!store_ready&&writes==2);assert(tcp_reload()&&get_day(20261005).punches[0]==480);
 ad_commit_error=true;assert(!tcp_mutate(20261005,0,615)&&!store_ready&&get_day(20261005).punches[0]==480);ad_commit_error=false;assert(tcp_reload()&&get_day(20261005).punches[0]==615);
 ad_write_error=RISC_APP_DATA_RETAINED;assert(!tcp_mutate(20261005,0,630)&&tcp_storage_retained());unsigned count=writes;assert(!tcp_reload()&&writes==count);tcp_runtime=&runtime;tcp_grant_count=1;grants=1;tcp_dependencies_close();assert(grants==1); /* Consumer never tears down retention. */
 ad_init();poll_count=launches=grants=0;app_main();assert(launches==1&&!grants&&back_exits);
 /* A retained initial read returns without finalization/revocation; the paired
  * Runtime's separately tested pre-fini barrier owns the retained invocation. */
 ad_init();file_present=true;strcpy(persisted,"{\"days\":[]}");ad_read_error=RISC_APP_DATA_RETAINED;poll_count=launches=grants=0;app_main();assert(!launches&&grants==3&&tcp_storage_retained());
 puts("Timecard source + app-data facade: CAS creation/read/write, stale/uncertain reconciliation and retained cleanup guard passed");return 0;
}
