/* Actual app_main, including unload/reopen around shared shell controls. */
#include "RiscRuntimeV1.h"
#include "RiscSceneComponentsV1.h"
#include "RiscSceneResidentV1.h"
#include "RiscCivilClockV1.h"
#include "RiscAppDataV1.h"
#include "../../Apps/ListsScene.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern void app_main(void);
static bool live=true,opened,closed,controls,lose_on_close,loss_on_store;
static unsigned opens,closes,releases,checkpoints,events,reads;
static risc_components_document_v1 latest;
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version);
static int open_scene(void*c,const risc_components_document_v1*d,const risc_scene_navigation_v1*p,uint64_t*out){(void)c;(void)p;assert(live&&!opened);latest=*d;opened=true;closed=false;*out=++opens;return 0;}
static int update_scene(void*c,uint64_t s,const risc_components_document_v1*d){(void)c;(void)s;assert(live&&opened);assert(d->revision>latest.revision);latest=*d;return 0;}
static int next(void*c,uint64_t s,risc_scene_event_v1*e){(void)c;(void)s;assert(live&&opened);*e=(risc_scene_event_v1){.struct_size=sizeof(*e),.document_revision=latest.revision,.sequence=++events};
 if(!controls){controls=true;e->kind=RISC_SCENE_CONTROLS_EVENT;return 0;}
 e->kind=RISC_SCENE_SUSPEND_EVENT;return 0;
}
static int snap(void*c,uint64_t s,risc_scene_navigation_v1*n,uint32_t*f){(void)c;(void)s;assert(live&&opened);*n=(risc_scene_navigation_v1){.api_version=1,.struct_size=sizeof(*n),.depth=1,.routes={1}};*f=0;return 0;}
static int close_scene(void*c,uint64_t s){(void)c;(void)s;assert(live&&opened);closes++;if(lose_on_close){live=false;return -9;}opened=false;closed=true;return 0;}
static int configure(void*c,uint64_t s,uint32_t flags){(void)c;(void)s;assert(live&&opened&&flags==RISC_SCENE_FEATURE_SHARED_CONTROLS);return 0;}
static const risc_scene_components_api_v1 scene={{{1,sizeof(scene),NULL,NULL,NULL,next,NULL,snap,close_scene},configure},RISC_COMPONENTS_TAG,1,open_scene,update_scene};
static int stat_file(void*c,const char*n,uint32_t*size,uint64_t*revision){(void)c;(void)n;assert(live);++reads;*size=0;*revision=0;if(loss_on_store){live=false;return -9;}return RISC_APP_DATA_NOT_FOUND;}
static int read_file(void*c,const char*n,uint64_t r,void*p,uint32_t cap,uint32_t*sz,uint64_t*rev){(void)c;(void)n;(void)r;(void)p;(void)cap;(void)sz;(void)rev;assert(0);return -9;}
static int save_file(void*c,const char*n,uint64_t r,const void*p,uint32_t sz){(void)c;(void)n;(void)r;(void)p;(void)sz;assert(0);return -9;}
static const risc_app_data_v1 storage={1,sizeof(storage),NULL,stat_file,read_file,save_file};
static int civil(void*c,risc_civil_time_v1*t){(void)c;assert(live);*t=(risc_civil_time_v1){sizeof(*t),9000,600,"UTC"};return 0;}
static const risc_civil_clock_api_v1 clock_api={1,sizeof(clock_api),NULL,civil};
static bool acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){(void)id;assert(live&&v==1);g->slot=1;g->generation=1;g->api=!strcmp(n,"ui.scene")?(void*)&scene:!strcmp(n,"storage.app-data")?(void*)&storage:(void*)&clock_api;return true;}
static bool release(risc_runtime_capability_v1*g){(void)g;assert(live&&!opened);++releases;return true;}
static void yield(uint32_t ms){(void)ms;assert(live);}
static bool retain(void){live=false;return true;}
static int checkpoint(uint64_t invocation,const risc_resident_request_v1*q,risc_resident_reply_v1*r){assert(invocation==1&&live&&closed&&!opened);assert(q->reason==RISC_RESIDENT_CHECKPOINT_CONTROLS);++checkpoints;r->flags=RISC_RESIDENT_REPLY_REDRAW;return 0;}
static bool resident(risc_resident_client_v1*s){*s=(risc_resident_client_v1){.api_version=1,.struct_size=sizeof(*s),.invocation=1,.role=RISC_RESIDENT_ROLE_FOREGROUND,.checkpoint=checkpoint};return true;}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.yield_ms=yield,.acquire=acquire,.release=release,.retain_invocation=retain,.resident_shell=resident};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1&&live?&runtime:NULL;}
int main(int argc,char**argv){assert(argc==2);lose_on_close=!strcmp(argv[1],"close-loss");loss_on_store=!strcmp(argv[1],"store-loss");app_main();
 if(loss_on_store)assert(reads==1&&!opens&&!releases&&!checkpoints);
 else if(lose_on_close)assert(opens==1&&closes==1&&!releases&&!checkpoints);
 else assert(opens==2&&closes==2&&checkpoints==1&&releases==3&&!opened);
 puts("Lists app lifecycle / shell handoff / terminal custody PASS");return 0;
}
