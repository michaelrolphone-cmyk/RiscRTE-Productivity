#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../Apps/timecard_appdata_bridge.h"
static unsigned calls,writes;
static int stat_result,read_result,write_result;
static uint64_t version=5;
static bool present=true,post_error;
static char content[TCP_APPDATA_MAX];
static uint32_t content_size=10;
static int32_t stat_file(void *c,const char *name,uint32_t *size,uint64_t *revision){(void)c;calls++;assert(!strcmp(name,"timecard.json"));*size=0;*revision=0;if(post_error){post_error=false;return RISC_APP_DATA_IO;}if(stat_result)return stat_result;if(!present)return RISC_APP_DATA_NOT_FOUND;*size=content_size;*revision=version;return 0;}
static int32_t read_file(void *c,const char *name,uint64_t expected,void *out,uint32_t cap,uint32_t *size,uint64_t *revision){(void)c;calls++;assert(!strcmp(name,"timecard.json"));*size=0;*revision=0;if(read_result)return read_result;if(expected!=version)return RISC_APP_DATA_STALE;if(cap<content_size)return RISC_APP_DATA_BUFFER_SMALL;memcpy(out,content,content_size);*size=content_size;*revision=version;return 0;}
static int32_t replace_file(void *c,const char *name,uint64_t expected,const void *data,uint32_t size){(void)c;calls++;assert(!strcmp(name,"timecard.json"));if(write_result)return write_result;if(expected!=(present?version:0))return RISC_APP_DATA_STALE;memcpy(content,data,size);content_size=size;present=true;version++;writes++;return 0;}
static const risc_app_data_v1 api={1,sizeof(api),NULL,stat_file,read_file,replace_file};
static void reset(tcp_appdata *state){*state=(tcp_appdata){0};assert(tcp_appdata_bind(state,&api));calls=writes=0;stat_result=read_result=write_result=0;version=5;present=true;post_error=false;content_size=10;memcpy(content,"{\"days\":[]}",10);}
int main(void){
 tcp_appdata state={0};char out[TCP_APPDATA_MAX];size_t size;
 reset(&state);assert(tcp_appdata_exists(&state,TCP_APPDATA_PATH));memset(out,0xa5,sizeof(out));assert(tcp_appdata_read(&state,TCP_APPDATA_PATH,out,sizeof(out),&size)&&size==10&&!memcmp(out,content,10));
 assert(tcp_appdata_write(&state,TCP_APPDATA_PATH,"{\"days\":[]}",10)&&writes==1&&state.revision==6);
 assert(tcp_appdata_write(&state,TCP_APPDATA_PATH,"{\"days\":[]}",10)&&writes==2&&state.revision==7);
 reset(&state);present=false;assert(!tcp_appdata_exists(&state,TCP_APPDATA_PATH)&&state.ready&&state.revision==0);assert(tcp_appdata_write(&state,TCP_APPDATA_PATH,"{\"days\":[]}",10)&&present&&writes==1);
 const int errors[]={RISC_APP_DATA_UNAVAILABLE,RISC_APP_DATA_IO,RISC_APP_DATA_CONTEXT,RISC_APP_DATA_RETAINED};
 for(unsigned i=0;i<sizeof(errors)/sizeof(errors[0]);i++){reset(&state);stat_result=errors[i];assert(tcp_appdata_exists(&state,TCP_APPDATA_PATH)&&!state.ready);size=99;assert(!tcp_appdata_read(&state,TCP_APPDATA_PATH,out,sizeof(out),&size)&&size==0);assert(!tcp_appdata_write(&state,TCP_APPDATA_PATH,"x",1)&&!writes);}
 reset(&state);content_size=TCP_APPDATA_MAX+1;assert(tcp_appdata_exists(&state,TCP_APPDATA_PATH)&&!state.ready);
 reset(&state);assert(tcp_appdata_exists(&state,TCP_APPDATA_PATH));version++;assert(!tcp_appdata_read(&state,TCP_APPDATA_PATH,out,sizeof(out),&size)&&state.status==RISC_APP_DATA_STALE&&!state.ready);assert(tcp_appdata_exists(&state,TCP_APPDATA_PATH));version++;assert(!tcp_appdata_write(&state,TCP_APPDATA_PATH,"x",1)&&state.status==RISC_APP_DATA_STALE&&!writes);
 reset(&state);assert(tcp_appdata_exists(&state,TCP_APPDATA_PATH));post_error=true;assert(!tcp_appdata_write(&state,TCP_APPDATA_PATH,"x",1)&&writes==1&&state.status==RISC_APP_DATA_COMMIT_UNKNOWN&&!state.ready);assert(tcp_appdata_exists(&state,TCP_APPDATA_PATH)&&state.size==1);
 reset(&state);assert(tcp_appdata_exists(&state,TCP_APPDATA_PATH));read_result=RISC_APP_DATA_RETAINED;assert(!tcp_appdata_read(&state,TCP_APPDATA_PATH,out,sizeof(out),&size)&&state.retained);unsigned before=calls;assert(tcp_appdata_exists(&state,TCP_APPDATA_PATH));assert(!tcp_appdata_write(&state,TCP_APPDATA_PATH,"x",1));assert(!tcp_appdata_bind(&state,&api));assert(calls==before);
 reset(&state);assert(tcp_appdata_exists(&state,"../timecard.json")&&!state.ready&&!calls);assert(!tcp_appdata_write(&state,TCP_APPDATA_PATH,"x",1));assert(tcp_appdata_exists(&state,TCP_APPDATA_PATH));memset(content,'z',sizeof(content));content_size=sizeof(content);assert(tcp_appdata_exists(&state,TCP_APPDATA_PATH));assert(tcp_appdata_read(&state,TCP_APPDATA_PATH,out,sizeof(out),&size)&&size==sizeof(out));assert(tcp_appdata_write(&state,TCP_APPDATA_PATH,out,sizeof(out)));
 puts("Timecard app-data bridge: missing/error distinction, full-size bytes, revision CAS, uncertainty and retained failures passed");return 0;
}
