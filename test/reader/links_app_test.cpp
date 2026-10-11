// Real touch edges through the production scene hit tester, then real Reader
// intent validation, URI resolution, pagination and page publication.
#define main reader_startup_reference_main
#include "app_startup.cpp"
#undef main
#include "RiscSceneReadingLinksV1.h"
#include <filesystem>
extern "C" void reader_test_scene_tap(unsigned,unsigned);
extern "C" void reader_test_scene_snapshot(const char*);
static risc_scene_reading_links_api_v1 linkCopy;
static const risc_scene_reading_links_api_v1* realLinks;
static risc_scene_reading_document_v1 reading;
static risc_scene_reading_links_v1 regions;
static risc_components_document_v1 components;
static unsigned stepNumber,viewRevision,lastIssued,linkEvents,chapterEvents,warmChecks;
static bool readingView,initialPolicy;
static std::vector<uint8_t> sourceBitmap,currentBitmap;
static std::string captures;
static int32_t linked(void*c,uint64_t s,const risc_scene_reading_document_v1*r,const risc_scene_reading_links_v1*l,const uint8_t*b,size_t n,uint32_t refresh){
 int rc=realLinks->present_linked(c,s,r,l,b,n,refresh);
 if(!rc){reading=*r;regions=*l;viewRevision=r->base.revision;readingView=true;
  if(b){assert(refresh==RISC_SCENE_PAGE_REFRESH_CLEAN);currentBitmap.assign(b,b+n);if(sourceBitmap.empty())sourceBitmap=currentBitmap;}
 }
 return rc;
}
static int32_t componentUpdate(void*c,uint64_t s,const risc_components_document_v1*d){int rc=realScene->components.update(c,s,d);if(!rc){components=*d;viewRevision=d->revision;readingView=false;}return rc;}
static int32_t linkCheckpoint(uint64_t invocation,const risc_resident_request_v1*q,risc_resident_reply_v1*r){
 assert(invocation==1);r->flags=0;
 if(q->reason==RISC_RESIDENT_CHECKPOINT_POLL){if(readingView&&!initialPolicy)r->flags=RISC_RESIDENT_REPLY_POLICY_REQUEST;return 0;}
 assert(q->reason==RISC_RESIDENT_CHECKPOINT_POLICY);hostCheckClosed();reader_test_scene_checkpoint();initialPolicy=true;++warmChecks;return 0;
}
static bool linkResident(risc_resident_client_v1*c){residentClient(c);c->checkpoint=linkCheckpoint;return true;}
static int32_t linkNext(void*c,uint64_t s,risc_scene_event_v1*e){
 int rc=realScene->components.lifecycle.base.next(c,s,e);
 if(rc==RISC_SCENE_OK){
  if(e->kind==RISC_SCENE_VALUE_EVENT&&e->node==RISC_SCENE_READING_LINK_NODE)++linkEvents;
  if(!readingView&&e->kind==RISC_SCENE_ACTION_EVENT&&e->action>=100&&e->action<108)++chapterEvents;
  return rc;
 }
 if(rc!=RISC_SCENE_IDLE||!viewRevision||viewRevision==lastIssued||!initialPolicy||stepNumber>=14)return rc;
 risc_scene_navigation_v1 nav{};nav.struct_size=sizeof(nav);uint32_t flags=0;assert(!realScene->components.lifecycle.base.snapshot(c,s,&nav,&flags));if(flags&RISC_SCENE_PRESENTING)return rc;
 if(!readingView&&strcmp(components.routes[0].title,"CONTENTS"))return rc;
 const bool chapter3=stepNumber==1||stepNumber==2||stepNumber==6;
 if(readingView)assert(strstr(reading.base.footer,stepNumber==13?"CH 4":chapter3?"CH 3":"CH 1"));
 unsigned x=240,y=500;
 switch(stepNumber){
 case 0:case 6:assert(readingView&&regions.count);x=regions.items[0].x+regions.items[0].width/2;y=48+regions.items[0].y+regions.items[0].height/2;break;
 case 1:case 3:case 7:case 10:assert(readingView&&!(reading.flags&RISC_SCENE_READING_CHROME));if(stepNumber==3)assert(currentBitmap==sourceBitmap);if(stepNumber==10)assert(currentBitmap!=sourceBitmap);break;
 case 2:assert(reading.flags&RISC_SCENE_READING_CHROME);x=50;y=56;break;
 case 4:case 8:case 11:assert(reading.flags&RISC_SCENE_READING_CHROME);x=120;y=740;break;
 case 5:case 9:case 12:assert(!readingView);x=200;y=120+84*(stepNumber==5?2:stepNumber==9?3:4)+42;break;
 case 13:assert(readingView);reader_test_scene_exit_in(1);break;
 }
 char name[64];snprintf(name,sizeof(name),"/links-%02u.pbm",stepNumber);reader_test_scene_snapshot((captures+name).c_str());
 fprintf(stderr,"LINK_TOUCH stage=%u revision=%u x=%u y=%u view=%s\n",stepNumber,viewRevision,x,y,readingView?reading.base.footer:components.routes[0].title);
 lastIssued=viewRevision;++stepNumber;if(stepNumber<14)reader_test_scene_tap(x,y);return rc;
}
int main(int argc,char**argv){
 assert(argc==3);hostBind(argv[1]);sourcePath="/Books/links.epub";captures=argv[2];std::filesystem::create_directories(captures);
 realLinks=static_cast<const risc_scene_reading_links_api_v1*>(reader_test_scene_start());assert(risc_scene_reading_links_get_v1(reinterpret_cast<const risc_scene_api_v1*>(realLinks)));
 realReading=&realLinks->reading;realCheckpoint=&realReading->checkpoint;realRefresh=&realCheckpoint->presentation;realScene=&realRefresh->page;
 linkCopy=*realLinks;linkCopy.present_linked=linked;linkCopy.reading.checkpoint.presentation.page.components.update=componentUpdate;linkCopy.reading.checkpoint.presentation.page.components.lifecycle.base.next=linkNext;sceneApi=&linkCopy;
 runtime.acquire=acquireApp;runtime.release=releaseApp;runtime.yield_ms=reader_test_scene_tick;runtime.resident_shell=linkResident;
 pthread_attr_t attr;assert(!pthread_attr_init(&attr));assert(!pthread_attr_setstacksize(&attr,32768));pthread_t thread;assert(!pthread_create(&thread,&attr,runApp,nullptr));assert(!pthread_attr_destroy(&attr));assert(!pthread_join(thread,nullptr));
 assert(stepNumber==14&&linkEvents==2&&chapterEvents==3&&warmChecks==1);assert(acquired==5&&released==5);hostCheckClosed();reader_test_scene_finish();
 puts("Reader navigation: 13 physical taps, warm checkpoint, relative inline chapter, Back to exact source, contents rows, late anchor, encoded path/anchor and 14 framebuffer captures PASS");
}
