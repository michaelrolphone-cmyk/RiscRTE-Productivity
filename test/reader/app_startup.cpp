#include "host_volume.cpp"
#include "T5FileOpenApi.h"
#include "RiscScenePageV1.h"
#include "RiscResidentShellV1.h"
#include "ReaderEngine.h"
#include <cassert>
#include <pthread.h>
extern "C" void app_main();
extern "C" const void *reader_test_scene_start();
extern "C" void reader_test_scene_tick(unsigned);
extern "C" void reader_test_scene_finish();
extern "C" void reader_test_scene_allow_exit();
extern "C" uint64_t reader_test_scene_now();
extern "C" void reader_test_scene_checkpoint();
extern "C" unsigned reader_test_scene_clean_count();
extern "C" void risc_cpp_set_failure_handler(void(*)()) {}
static const void *sceneApi;
static unsigned acquired,released;
static bool readySeen;
static const char *sourcePath;
static unsigned pages;
static bool residentTest,restorePending,policyPending;
static bool catalogTest,sawBrowse,sawLibraryRows;
static bool badCatalog,sawUnavailable,sawBrowser,browseSent;
static risc_scene_event_v1 browseEvent;
static bool turnsTest;
static unsigned turnEvents,cleanRequests;
static uint64_t eventSequence;
static risc_scene_page_document_v1 shownPage;
static unsigned policyCalls,restores;
static uint64_t lastPolicy;
static std::vector<uint8_t> firstPage;
static risc_scene_page_refresh_api_v1 sceneCopy;
static const risc_scene_page_api_v1 *realScene;
static const risc_scene_page_refresh_api_v1 *realRefresh;
static int32_t presentPage(void* c,uint64_t session,const risc_scene_page_document_v1* p,const uint8_t* bytes,size_t size,uint32_t refresh){
 assert(refresh==(restorePending?RISC_SCENE_PAGE_REFRESH_DEFAULT:RISC_SCENE_PAGE_REFRESH_CLEAN));
 cleanRequests+=refresh==RISC_SCENE_PAGE_REFRESH_CLEAN;
 int32_t result=realRefresh->present_page_with_refresh(c,session,p,bytes,size,refresh);
 if(!result){unsigned ink=0;for(size_t i=0;i<size;++i)ink+=bytes[i]!=255;assert(ink>100);
  shownPage=*p;
  if(!pages)firstPage.assign(bytes,bytes+size);
  if(turnsTest&&pages==1)assert(firstPage.size()==size&&memcmp(firstPage.data(),bytes,size));
  if(turnsTest&&pages==2)assert(firstPage.size()==size&&!memcmp(firstPage.data(),bytes,size));
  if(restorePending){assert(firstPage.size()==size&&!memcmp(firstPage.data(),bytes,size));restorePending=false;++restores;}
  ++pages;if(turnsTest?pages==3:!residentTest||restores==4)reader_test_scene_allow_exit();}
 return result;
}
static void checkDocument(const risc_components_document_v1* d){
 if(catalogTest){
  unsigned bookRows=0,emptyAuthors=0;
  for(unsigned i=0;i<d->node_count;++i){
   const auto& n=d->nodes[i];assert(strcmp(n.label,"Unable to continue"));
   if(!strcmp(n.label,"Browse SD"))sawBrowse=true;
   if(badCatalog&&!strcmp(n.label,"Browse SD")){browseEvent.kind=RISC_SCENE_ACTION_EVENT;browseEvent.document_revision=d->revision;browseEvent.node=n.id;browseEvent.action=n.action;}
   if(!strcmp(n.label,"Library index unavailable"))sawUnavailable=true;
   if(n.action>=100&&n.action<108){++bookRows;emptyAuthors+=n.text[0]==0;assert(n.label[0]);}
  }
  if(bookRows==4&&emptyAuthors)sawLibraryRows=true;
  if(badCatalog&&!strcmp(d->routes[0].title,"BROWSE SD")){sawBrowser=true;reader_test_scene_allow_exit();}
 }
 if(!residentTest||!pages)return;
 for(unsigned i=0;i<d->node_count;++i)assert(strcmp(d->nodes[i].label,"Opening book")&&"Loading screen replaced the visible page during resident restore");
}
static int32_t openComponents(void* c,const risc_components_document_v1* d,const risc_scene_navigation_v1* n,uint64_t* s){checkDocument(d);return realScene->components.open(c,d,n,s);}
static int32_t updateComponents(void* c,uint64_t s,const risc_components_document_v1* d){checkDocument(d);return realScene->components.update(c,s,d);}
static int32_t nextScene(void* c,uint64_t s,risc_scene_event_v1* e){
 assert(!restorePending&&"Scene serviced before restoring the page");
 int rc=realScene->components.lifecycle.base.next(c,s,e);
 if(rc==RISC_SCENE_OK)e->sequence=++eventSequence;
 if(badCatalog&&sawUnavailable&&!browseSent&&rc==RISC_SCENE_IDLE){*e=browseEvent;e->struct_size=sizeof(*e);e->sequence=++eventSequence;browseSent=true;return RISC_SCENE_OK;}
 if(turnsTest&&rc==RISC_SCENE_IDLE&&pages==turnEvents+1&&turnEvents<2){
  risc_scene_navigation_v1 n{};n.struct_size=sizeof(n);uint32_t flags=0;
  assert(!realScene->components.lifecycle.base.snapshot(c,s,&n,&flags));
  if(!(flags&RISC_SCENE_PRESENTING)){e->kind=RISC_SCENE_ACTION_EVENT;e->document_revision=shownPage.revision;e->sequence=++eventSequence;e->action=turnEvents?shownPage.previous_action:shownPage.next_action;++turnEvents;return RISC_SCENE_OK;}
 }
 return rc;
}
static int32_t checkpoint(uint64_t invocation,const risc_resident_request_v1* request,risc_resident_reply_v1* reply){
 assert(invocation==1);reply->flags=0;
 if(request->reason==RISC_RESIDENT_CHECKPOINT_POLL){
  if(pages&&policyCalls<4&&reader_test_scene_now()-lastPolicy>=5000){reply->flags=RISC_RESIDENT_REPLY_POLICY_REQUEST;policyPending=true;}
  return RISC_RESIDENT_OK;
 }
 assert(request->reason==RISC_RESIDENT_CHECKPOINT_POLICY&&policyPending);
 hostCheckClosed();reader_test_scene_checkpoint();assert(!restorePending);
 ++policyCalls;restorePending=true;lastPolicy=reader_test_scene_now();
 // BUSY keeps the request pending and must restore just like OK.
 if(policyCalls==2)return RISC_RESIDENT_BUSY;
 policyPending=false;reply->flags=RISC_RESIDENT_REPLY_CONFIGURATION_CHANGED;
 return RISC_RESIDENT_OK;
}
static bool residentClient(risc_resident_client_v1* out){*out={};out->api_version=1;out->struct_size=sizeof(*out);out->invocation=1;out->role=RISC_RESIDENT_ROLE_FOREGROUND;out->checkpoint=checkpoint;return true;}
static bool source(char* out,size_t n){if(!sourcePath)return false;return snprintf(out,n,"/sd%s",sourcePath)>0;}
static const t5_file_open_api_v1 fileApi={.api_version=1,.struct_size=sizeof(fileApi),.source_path_get=source};
static bool acquireApp(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *g){
 assert(version==1&&!instance&&g->struct_size==sizeof(*g));
 g->api=!strcmp(name,"ui.scene")?sceneApi:!strcmp(name,"storage.volume")?static_cast<const void*>(&filesystem):!strcmp(name,"memory.heap")?static_cast<const void*>(&heap):!strcmp(name,"random.bytes")?static_cast<const void*>(&randomApi):!strcmp(name,"file.open")?static_cast<const void*>(&fileApi):nullptr;
 assert(g->api);g->slot=++acquired;return true;
}
static bool releaseApp(risc_runtime_capability_v1 *g){assert(g->api&&!held);g->api=nullptr;++released;return true;}
static bool appLog(const char *message){if(strstr(message,"scene ready page=480x632 sd=ready")){readySeen=true;
 if(badCatalog){for(auto& f:files)f.second.error=1;} // Fail the already-open index's next read.
 else if(!sourcePath)reader_test_scene_allow_exit();}return log(message);}
static void *runApp(void*){app_main();return nullptr;}
int main(int argc,char **argv){
 assert(argc>=2&&argc<=4);sourcePath=argc>=3&&strcmp(argv[2],"-")?argv[2]:nullptr;
 residentTest=argc==4&&!strcmp(argv[3],"resident");badCatalog=argc==4&&!strcmp(argv[3],"bad-catalog");catalogTest=badCatalog||(argc==4&&!strcmp(argv[3],"catalog"));turnsTest=argc==4&&!strcmp(argv[3],"turns");hostBind(argv[1]);
 if(catalogTest){reader::Engine e;assert(e.init(480,632));assert(e.scanLibrary());assert(e.library.bookCount()==4);}
 realRefresh=static_cast<const risc_scene_page_refresh_api_v1*>(reader_test_scene_start());realScene=&realRefresh->page;sceneCopy=*realRefresh;sceneCopy.present_page_with_refresh=presentPage;sceneApi=&sceneCopy;
 runtime.acquire=acquireApp;runtime.release=releaseApp;runtime.yield_ms=reader_test_scene_tick;runtime.diagnostic=appLog;
 sceneCopy.page.components.open=openComponents;sceneCopy.page.components.update=updateComponents;
 if(residentTest)runtime.resident_shell=residentClient;
 sceneCopy.page.components.lifecycle.base.next=nextScene;
 // Host libc/64-bit call frames differ from Xtensa. This bounded-stack run
 // complements the target compiler frame budget; it is not hardware emulation.
 pthread_attr_t attr;assert(!pthread_attr_init(&attr));assert(!pthread_attr_setstacksize(&attr,32768));
 pthread_t thread;assert(!pthread_create(&thread,&attr,runApp,nullptr));assert(!pthread_attr_destroy(&attr));assert(!pthread_join(thread,nullptr));
 assert(readySeen&&!held&&acquired==5&&released==5);if(sourcePath)assert(pages);
 if(residentTest)assert(policyCalls==4&&restores==4&&!restorePending);
 if(residentTest)assert(cleanRequests==1&&reader_test_scene_clean_count()==1);
 if(turnsTest)assert(turnEvents==2&&pages==3&&cleanRequests==3&&reader_test_scene_clean_count()==3);
 if(catalogTest)assert(sawBrowse&&(badCatalog?sawUnavailable&&sawBrowser:sawLibraryRows));
 hostCheckClosed();reader_test_scene_finish();
 printf("Reader real app entry: %s, 32 KiB host stack, rotated page, close and grant cleanup%s PASS\n",sourcePath?sourcePath:"library",residentTest?", four resident policy restores including BUSY, stable page":badCatalog?", failed index read with working SD browser":catalogTest?", indexed books without author metadata":turnsTest?", next/previous clean refreshes":"");
}
