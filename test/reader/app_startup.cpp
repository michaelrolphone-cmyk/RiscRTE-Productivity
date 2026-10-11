#include "host_volume.cpp"
#include "T5FileOpenApi.h"
#include "RiscScenePageV1.h"
#include <cassert>
#include <pthread.h>
extern "C" void app_main();
extern "C" const void *reader_test_scene_start();
extern "C" void reader_test_scene_tick(unsigned);
extern "C" void reader_test_scene_finish();
extern "C" void reader_test_scene_allow_exit();
extern "C" void risc_cpp_set_failure_handler(void(*)()) {}
static const void *sceneApi;
static unsigned acquired,released;
static bool readySeen;
static const char *sourcePath;
static unsigned pages;
static risc_scene_page_api_v1 sceneCopy;
static const risc_scene_page_api_v1 *realScene;
static int32_t presentPage(void* c,uint64_t session,const risc_scene_page_document_v1* p,const uint8_t* bytes,size_t size){
 int32_t result=realScene->present_page(c,session,p,bytes,size);
 if(!result){unsigned ink=0;for(size_t i=0;i<size;++i)ink+=bytes[i]!=255;assert(ink>100);++pages;reader_test_scene_allow_exit();}
 return result;
}
static bool source(char* out,size_t n){if(!sourcePath)return false;return snprintf(out,n,"/sd%s",sourcePath)>0;}
static const t5_file_open_api_v1 fileApi={.api_version=1,.struct_size=sizeof(fileApi),.source_path_get=source};
static bool acquireApp(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *g){
 assert(version==1&&!instance&&g->struct_size==sizeof(*g));
 g->api=!strcmp(name,"ui.scene")?sceneApi:!strcmp(name,"storage.volume")?static_cast<const void*>(&filesystem):!strcmp(name,"memory.heap")?static_cast<const void*>(&heap):!strcmp(name,"random.bytes")?static_cast<const void*>(&randomApi):!strcmp(name,"file.open")?static_cast<const void*>(&fileApi):nullptr;
 assert(g->api);g->slot=++acquired;return true;
}
static bool releaseApp(risc_runtime_capability_v1 *g){assert(g->api&&!held);g->api=nullptr;++released;return true;}
static bool appLog(const char *message){if(strstr(message,"scene ready page=480x632 sd=ready")){readySeen=true;if(!sourcePath)reader_test_scene_allow_exit();}return log(message);}
static void *runApp(void*){app_main();return nullptr;}
int main(int argc,char **argv){
 assert(argc==2||argc==3);sourcePath=argc==3?argv[2]:nullptr;hostBind(argv[1]);
 realScene=static_cast<const risc_scene_page_api_v1*>(reader_test_scene_start());sceneCopy=*realScene;sceneCopy.present_page=presentPage;sceneApi=&sceneCopy;
 runtime.acquire=acquireApp;runtime.release=releaseApp;runtime.yield_ms=reader_test_scene_tick;runtime.diagnostic=appLog;
 // Host libc/64-bit call frames differ from Xtensa. This bounded-stack run
 // complements the target compiler frame budget; it is not hardware emulation.
 pthread_attr_t attr;assert(!pthread_attr_init(&attr));assert(!pthread_attr_setstacksize(&attr,32768));
 pthread_t thread;assert(!pthread_create(&thread,&attr,runApp,nullptr));assert(!pthread_attr_destroy(&attr));assert(!pthread_join(thread,nullptr));
 assert(readySeen&&!held&&acquired==5&&released==5);if(sourcePath)assert(pages);
 hostCheckClosed();reader_test_scene_finish();
 printf("Reader real app entry: %s, 32 KiB host stack, rotated page, close and grant cleanup PASS\n",sourcePath?sourcePath:"library");
}
