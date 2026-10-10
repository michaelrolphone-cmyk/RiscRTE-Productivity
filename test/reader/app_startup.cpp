#include "host_volume.cpp"
#include "T5FileOpenApi.h"
#include <cassert>
extern "C" void app_main();
extern "C" const void *reader_test_scene_start();
extern "C" void reader_test_scene_tick(unsigned);
extern "C" void reader_test_scene_finish();
extern "C" void risc_cpp_set_failure_handler(void(*)()) {}
static const void *sceneApi;
static unsigned acquired,released;
static bool readySeen;
static bool noSource(char*,size_t){return false;}
static const t5_file_open_api_v1 fileApi={.api_version=1,.struct_size=sizeof(fileApi),.source_path_get=noSource};
static bool acquireApp(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *g){
 assert(version==1&&!instance&&g->struct_size==sizeof(*g));
 g->api=!strcmp(name,"ui.scene")?sceneApi:!strcmp(name,"storage.volume")?static_cast<const void*>(&filesystem):!strcmp(name,"memory.heap")?static_cast<const void*>(&heap):!strcmp(name,"random.bytes")?static_cast<const void*>(&randomApi):!strcmp(name,"file.open")?static_cast<const void*>(&fileApi):nullptr;
 assert(g->api);g->slot=++acquired;return true;
}
static bool releaseApp(risc_runtime_capability_v1 *g){assert(g->api&&!held);g->api=nullptr;++released;return true;}
static bool appLog(const char *message){if(strstr(message,"scene ready page=480x632 sd=ready"))readySeen=true;return log(message);}
int main(int argc,char **argv){
 assert(argc==2);hostBind(argv[1]);sceneApi=reader_test_scene_start();
 runtime.acquire=acquireApp;runtime.release=releaseApp;runtime.yield_ms=reader_test_scene_tick;runtime.diagnostic=appLog;
 app_main();assert(readySeen&&!held&&acquired==5&&released==5);
 hostCheckClosed();reader_test_scene_finish();
 puts("Reader real app entry: cold scene startup, rotated geometry, library, close and grant cleanup PASS");
}
