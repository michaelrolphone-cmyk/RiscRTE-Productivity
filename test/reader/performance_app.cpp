// Same production App+scene fixture as the lifecycle suite. Measurements are
// capability-call counts on a host SD fixture, NOT board latency estimates.
#define main reader_startup_reference_main
#include "app_startup.cpp"
#undef main
static HostIo startIo;static bool measuring,verifyFast,partialBuild;
static unsigned pageTotal,turnTotal,policyTotal;static risc_scene_reading_document_v1 currentReading;
static const int directions[]={1,1,1,-1,-1,-1};
static int32_t performancePage(void* c,uint64_t session,const risc_scene_reading_document_v1* d,const uint8_t* bytes,size_t n,uint32_t refresh){
 assert(bytes&&refresh==RISC_SCENE_PAGE_REFRESH_CLEAN);
 int rc=realReading->present_reading(c,session,d,bytes,n,refresh);assert(!rc);
 if(measuring){
  const HostIo now=hostIo;
  printf("TURN_METRICS {\"partial\":%s,\"turn\":%u,\"opens\":%llu,\"directories\":%llu,\"reads\":%llu,\"read_bytes\":%llu,\"writes_before_present\":%llu,\"seeks\":%llu,\"font_loads\":%llu}\n",
   partialBuild?"true":"false",turnTotal,(unsigned long long)(now.opens-startIo.opens),(unsigned long long)(now.dirs-startIo.dirs),(unsigned long long)(now.reads-startIo.reads),(unsigned long long)(now.bytes-startIo.bytes),(unsigned long long)(now.writes-startIo.writes),(unsigned long long)(now.seeks-startIo.seeks),(unsigned long long)(now.fontLoads-startIo.fontLoads));fflush(stdout);
  if(verifyFast){assert(now.fontLoads==startIo.fontLoads);assert(now.dirs==startIo.dirs);if(!partialBuild){assert(now.writes==startIo.writes);assert(now.reads-startIo.reads<250);}
   assert(now.bytes-startIo.bytes<50000);assert(now.opens-startIo.opens<=10);assert(now.seeks-startIo.seeks<20);}
  measuring=false;
 }
 currentReading=*d;++pageTotal;if(pageTotal==7)reader_test_scene_allow_exit();return rc;
}
static int32_t performanceNext(void* c,uint64_t s,risc_scene_event_v1* e){
 int rc=realScene->components.lifecycle.base.next(c,s,e);if(rc==RISC_SCENE_OK)e->sequence=++eventSequence;
 if(rc!=RISC_SCENE_IDLE||!pageTotal||pageTotal!=turnTotal+1||turnTotal==6||policyTotal<pageTotal)return rc;
 risc_scene_navigation_v1 nav{};nav.struct_size=sizeof(nav);uint32_t flags=0;assert(!realScene->components.lifecycle.base.snapshot(c,s,&nav,&flags));if(flags&RISC_SCENE_PRESENTING)return rc;
 startIo=hostIo;measuring=true;
 *e={};e->struct_size=sizeof(*e);e->kind=RISC_SCENE_ACTION_EVENT;e->sequence=++eventSequence;e->document_revision=currentReading.base.revision;
 e->node=directions[turnTotal]>0?3:1;e->action=directions[turnTotal]>0?currentReading.base.next_action:currentReading.base.previous_action;++turnTotal;return 0;
}
static int32_t performanceCheckpoint(uint64_t invocation,const risc_resident_request_v1* q,risc_resident_reply_v1* reply){
 assert(invocation==1);reply->flags=0;
 if(q->reason==RISC_RESIDENT_CHECKPOINT_POLL){if(pageTotal&&policyTotal<pageTotal)reply->flags=RISC_RESIDENT_REPLY_POLICY_REQUEST;return 0;}
 assert(q->reason==RISC_RESIDENT_CHECKPOINT_POLICY);hostCheckClosed();reader_test_scene_checkpoint();
 ++policyTotal;reply->flags=RISC_RESIDENT_REPLY_CONFIGURATION_CHANGED;return 0;
}
static bool performanceResident(risc_resident_client_v1* c){residentClient(c);c->checkpoint=performanceCheckpoint;return true;}
int main(int argc,char** argv){
 assert(argc>=2&&argc<=4);verifyFast=argc>=3&&!strcmp(argv[2],"verify");partialBuild=argc==4&&!strcmp(argv[3],"partial");
 sourcePath="/Books/sample.epub";hostBind(argv[1]);
 if(!partialBuild){reader::Engine warm;assert(warm.init(480,680)&&warm.open(sourcePath));while(warm.busy())assert(warm.step());assert(warm.close());}
 realReading=static_cast<const risc_scene_reading_api_v1*>(reader_test_scene_start());realCheckpoint=&realReading->checkpoint;realRefresh=&realCheckpoint->presentation;realScene=&realRefresh->page;
 sceneCopy=*realReading;sceneCopy.checkpoint.presentation.page.components.lifecycle.base.struct_size=sizeof(sceneCopy);sceneCopy.present_reading=performancePage;sceneCopy.checkpoint.presentation.page.components.lifecycle.base.next=performanceNext;sceneApi=&sceneCopy;
 runtime.acquire=acquireApp;runtime.release=releaseApp;runtime.yield_ms=reader_test_scene_tick;runtime.resident_shell=performanceResident;
 pthread_attr_t attr;assert(!pthread_attr_init(&attr));assert(!pthread_attr_setstacksize(&attr,32768));pthread_t thread;
 assert(!pthread_create(&thread,&attr,runApp,nullptr));assert(!pthread_attr_destroy(&attr));assert(!pthread_join(thread,nullptr));
 assert(pageTotal==7&&turnTotal==6&&policyTotal>=6);assert(!held&&acquired==5&&released==5);hostCheckClosed();reader_test_scene_finish();
 puts("PERFORMANCE PASS: six post-policy turns, full clean pages, no leaked storage");
}
