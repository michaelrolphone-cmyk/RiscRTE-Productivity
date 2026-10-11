#include "metadata.h"
#include "RiscNetHttpClientV1.h"
#include "RiscProviderV2.h"
#include "RiscRuntimeV1.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern const risc_driver_v2 *t5_driver_get(uint32_t);
static risc_runtime_api_v1 runtime;
static unsigned closes,opens;
static bool revoked,retain_close,fail_open;
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1&&!revoked?&runtime:NULL;}
static int32_t open_(void*c,const risc_http_request_v1*r,uint64_t*t){(void)c;assert(r);++opens;*t=123;return fail_open?RISC_HTTP_TRANSPORT:RISC_HTTP_OK;}
static int32_t read_(void*c,uint64_t t,void*b,uint32_t n,uint32_t*out){(void)c;assert(t==123&&b&&n);*out=0;return RISC_HTTP_EOF;}
static int32_t info_(void*c,uint64_t t,risc_http_response_v1*r){(void)c;assert(t==123);r->status_code=200;return 0;}
static int32_t close_(void*c,uint64_t t){(void)c;assert(t==123);++closes;return retain_close?RISC_HTTP_RETAINED:0;}
int main(int argc,char**argv){
 assert(argc==2);const char*name=argv[1];
 risc_http_client_v1 native={1,sizeof(native),NULL,open_,read_,info_,close_};
 risc_provider_dependency_v1 dependency={RISC_HTTP_CLIENT_CAPABILITY,1,&native};
 const risc_driver_v2*d=t5_driver_get(2);assert(d&&!t5_driver_get(1));
 assert(!d->start(NULL,0));assert(d->start(&dependency,1));assert(!d->start(&dependency,1));
 const risc_http_client_v1*api=d->capability;
 risc_http_request_v1 r={sizeof(r),DAILY_LATEST,8192,30000,1791684000};uint64_t token=0;uint32_t count=0;char buf[32];
 fail_open=!strcmp(name,"failed-open");
 assert(api->open(NULL,&r,&token)==(fail_open?RISC_HTTP_TRANSPORT:0));assert(token==123);
 assert(api->read(NULL,124,buf,sizeof(buf),&count)==RISC_HTTP_CLOSED);
 assert(api->close(NULL,124)==RISC_HTTP_CLOSED&&closes==0);
 if(!strcmp(name,"normal")){
  risc_http_response_v1 response={.struct_size=sizeof(response)};assert(api->info(NULL,token,&response)==0&&response.status_code==200);
  assert(api->read(NULL,token,buf,sizeof(buf),&count)==RISC_HTTP_EOF);
  assert(api->close(NULL,token)==0&&api->close(NULL,token)==0&&closes==1);assert(d->quiesce());d->stop();
 }else if(!strcmp(name,"busy")){
  uint64_t other=99;assert(api->open(NULL,&r,&other)==RISC_HTTP_BUSY&&other==0&&opens==1);assert(api->close(NULL,token)==0);assert(d->quiesce());
 }else if(!strcmp(name,"retained")){
  retain_close=true;assert(api->close(NULL,token)==RISC_HTTP_RETAINED);assert(!d->quiesce());d->stop();assert(!d->start(&dependency,1));assert(api->read(NULL,token,buf,sizeof(buf),&count)==RISC_HTTP_RETAINED);assert(closes==1);
 }else if(!strcmp(name,"failed-open")){
  assert(api->close(NULL,token)==0&&closes==1);assert(d->quiesce());
 }else if(!strcmp(name,"shutdown")){
  assert(d->quiesce()&&closes==1);assert(d->quiesce()&&closes==1);d->stop();
 }else if(!strcmp(name,"revoked")){
  revoked=true;assert(api->read(NULL,token,buf,sizeof(buf),&count)==RISC_HTTP_RETAINED);assert(!d->quiesce()&&closes==0);
 }else assert(false);
 printf("PASS HTTP provider %s\n",name);
}
