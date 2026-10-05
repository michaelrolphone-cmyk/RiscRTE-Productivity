/* Production Timecard facade + validator against production Runtime storage. */
#include "../Apps/timecard_appdata_bridge.h"
#include "../Apps/timecard_portable_validation.h"
#include "runtime/storage/AppDataFiles.h"
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <string>
#include <vector>
#include <unistd.h>
using RiscStorage::AppDataFiles;
static int fault=0;
static unsigned calls=0;
extern "C" ssize_t __real_write(int,const void*,size_t);
extern "C" ssize_t __wrap_write(int fd,const void*data,size_t n){
 if(fault==1){fault=0;errno=ENOSPC;return -1;}return __real_write(fd,data,n);
}
extern "C" int __real_rename(const char*,const char*);
extern "C" int __wrap_rename(const char*from,const char*to){
 if(fault==2){fault=0;errno=EIO;return -1;}int result=__real_rename(from,to);
 if(fault==3){fault=0;errno=EIO;return -1;}return result;
}
extern "C" int __real_close(int);
extern "C" int __wrap_close(int fd){int result=__real_close(fd);if(fault==4){fault=0;errno=EIO;return -1;}return result;}
static const AppDataFiles::Hooks hooks{nullptr,[](void*){return 0u;},[](void*){return true;},malloc,free};
static risc_app_data_v1 api(AppDataFiles& files){return {1,sizeof(risc_app_data_v1),&files,
 [](void*c,const char*n,uint32_t*s,uint64_t*r){++calls;return static_cast<AppDataFiles*>(c)->stat(1,n,s,r);},
 [](void*c,const char*n,uint64_t r,void*b,uint32_t z,uint32_t*s,uint64_t*v){++calls;return static_cast<AppDataFiles*>(c)->read(1,n,r,b,z,s,v);},
 [](void*c,const char*n,uint64_t r,const void*b,uint32_t z){++calls;return static_cast<AppDataFiles*>(c)->replace(1,n,r,b,z);}};}
static std::string history(){
 std::string doc="{\"days\":[";
 for(unsigned i=0;i<400;++i){char row[96];unsigned date=(2025+i/336)*10000+(i%336/28+1)*100+i%28+1;
  snprintf(row,sizeof(row),"%s{\"d\":%u,\"in\":480,\"out\":1020}",i?",":"",date);doc+=row;}
 doc+="]}";doc.resize(TCP_APPDATA_MAX,' ');assert(tcp_validate_json(doc.data(),doc.size()));return doc;
}
static std::string load(tcp_appdata& state){
 assert(tcp_appdata_exists(&state,TCP_APPDATA_PATH)&&state.ready);
 std::vector<char> data(TCP_APPDATA_MAX);size_t size=0;
 assert(tcp_appdata_read(&state,TCP_APPDATA_PATH,data.data(),data.size(),&size));
 assert(tcp_validate_json(data.data(),size));return std::string(data.data(),size);
}
int main(int argc,char**argv){
 assert(argc==2);AppDataFiles files(hooks);auto table=api(files);tcp_appdata state{};
 assert(tcp_appdata_bind(&state,&table));assert(tcp_appdata_exists(&state,TCP_APPDATA_PATH)&&!state.ready);
 assert(state.status==RISC_APP_DATA_UNAVAILABLE);assert(files.configure(argv[1]));
 assert(!tcp_appdata_exists(&state,TCP_APPDATA_PATH)&&state.ready&&state.missing);
 auto full=history();assert(tcp_appdata_write(&state,TCP_APPDATA_PATH,full.data(),full.size()));assert(load(state)==full);
 // A Spectrum write must not alter Timecard, but invalidates its old CAS token.
 assert(files.replace(2,"timecard.json",0,"separate",8)==0);
 assert(!tcp_appdata_write(&state,TCP_APPDATA_PATH,"{\"days\":[]}",11)&&state.status==RISC_APP_DATA_STALE);
 assert(load(state)==full);
 const std::string edited="{\"days\":[{\"d\":20261005,\"in\":555}]}";
 fault=1;assert(!tcp_appdata_write(&state,TCP_APPDATA_PATH,edited.data(),edited.size())&&state.status==RISC_APP_DATA_NO_SPACE);
 assert(load(state)==full);
 fault=2;assert(!tcp_appdata_write(&state,TCP_APPDATA_PATH,edited.data(),edited.size())&&state.status==RISC_APP_DATA_COMMIT_UNKNOWN);
 assert(load(state)==full);
 fault=3;assert(!tcp_appdata_write(&state,TCP_APPDATA_PATH,edited.data(),edited.size())&&state.status==RISC_APP_DATA_COMMIT_UNKNOWN);
 assert(load(state)==edited);
 // A fresh mounted service sees the selected complete file, never the stage.
 AppDataFiles remount(hooks);assert(remount.configure(argv[1]));auto fresh=api(remount);tcp_appdata reopened{};
 assert(tcp_appdata_bind(&reopened,&fresh));assert(load(reopened)==edited);
 std::string oversized(TCP_APPDATA_MAX+1,'x');
 assert(!tcp_appdata_write(&state,TCP_APPDATA_PATH,oversized.data(),oversized.size())&&state.status==RISC_APP_DATA_INVALID);
 assert(load(state)==edited);
 fault=4;assert(!tcp_appdata_write(&state,TCP_APPDATA_PATH,full.data(),full.size())&&state.retained&&files.retained());
 unsigned before=calls;assert(tcp_appdata_exists(&state,TCP_APPDATA_PATH));
 assert(!tcp_appdata_write(&state,TCP_APPDATA_PATH,edited.data(),edited.size()));assert(!tcp_appdata_bind(&state,&table));assert(calls==before);
 puts("Timecard + Runtime complete-file backend: max 400-day JSON, namespaces/CAS, full/uncertain saves, remount and retention PASS");
}
