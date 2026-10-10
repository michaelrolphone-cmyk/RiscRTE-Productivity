#include "ReaderPort.h"
#include <filesystem>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>
#include <string>
#include <chrono>
#include <unistd.h>
namespace fs=std::filesystem;
static fs::path root;
struct File {FILE* fp;uint32_t error=0;};struct Dir {std::vector<fs::directory_entry> list;size_t offset=0;uint32_t error=0;};
static std::map<uint32_t,File> files;static std::map<uint32_t,Dir> dirs;static uint32_t serial=1;static bool held=false;
static fs::path path(const char* p){return root/fs::path(p).relative_path();}
static bool refresh(void*){return true;}static bool ready(void*){return !held;}static bool label(void*,char* b,size_t n){snprintf(b,n,"Fixture SD");return true;}
static bool stat(void*,const char* p,uint64_t* size,bool* dir){std::error_code ec;auto f=path(p);if(!fs::exists(f,ec))return false;*dir=fs::is_directory(f);*size=*dir?0:fs::file_size(f,ec);return !ec;}
static uint32_t dirOpen(void*,const char* p){std::error_code ec;Dir d;for(auto& e:fs::directory_iterator(path(p),ec))d.list.push_back(e);if(ec)return 0;auto h=serial++;dirs[h]=std::move(d);return h;}
static bool dirNext(void*,uint32_t h,risc_storage_dirent_v1* e){auto& d=dirs.at(h);if(d.offset==d.list.size())return false;auto& f=d.list[d.offset++];std::string n=f.path().filename().string();snprintf(e->name,sizeof(e->name),"%s",n.c_str());e->is_directory=f.is_directory();e->size=e->is_directory?0:f.file_size();return true;}
static bool dirClose(void*,uint32_t h){return dirs.erase(h)==1;}static void dirCloseVoid(void* c,uint32_t h){dirClose(c,h);}static bool dirRewind(void*,uint32_t h){dirs.at(h).offset=0;return true;}
static uint32_t fileOpen(void*,const char* p,uint32_t flags){const auto f=path(p);if((flags&RISC_STORAGE_OPEN_EXCLUSIVE)&&fs::exists(f))return 0;const char* mode=(flags&RISC_STORAGE_OPEN_WRITE)?(flags&RISC_STORAGE_OPEN_TRUNCATE?"w+b":"r+b"):"rb";FILE* fp=fopen(f.c_str(),mode);if(!fp&&(flags&RISC_STORAGE_OPEN_CREATE))fp=fopen(f.c_str(),"w+b");if(!fp)return 0;auto h=serial++;files[h]={fp};return h;}
static uint32_t readOpen(void* c,const char* p,uint64_t* size){auto h=fileOpen(c,p,1);if(h){fseek(files[h].fp,0,SEEK_END);*size=ftell(files[h].fp);rewind(files[h].fp);}return h;}
static uint32_t writeOpen(void* c,const char* p){return fileOpen(c,p,2|4|16);}
static size_t read(void*,uint32_t h,void* b,size_t n){if(n>4096)return 0;auto& f=files.at(h);size_t got=fread(b,1,std::min(n,size_t(173)),f.fp);if(ferror(f.fp))f.error=1;return got;}
static size_t write(void*,uint32_t h,const void* b,size_t n){if(n>4096)return 0;auto& f=files.at(h);size_t got=fwrite(b,1,std::min(n,size_t(211)),f.fp);if(got==0)f.error=1;return got;}
static bool fileClose(void*,uint32_t h,bool){auto i=files.find(h);if(i==files.end())return false;int rc=fclose(i->second.fp);files.erase(i);return rc==0;}
static bool remove(void*,const char* p){std::error_code ec;return fs::remove(path(p),ec)&&!ec;}
static bool lastError(void*,char* p,size_t n){snprintf(p,n,"I/O failure");return false;}
static bool seek(void*,uint32_t h,uint64_t offset){return fseek(files.at(h).fp,offset,SEEK_SET)==0;}
static bool info(void*,uint32_t h,uint64_t* size,uint64_t* pos){auto f=files.at(h).fp;*pos=ftell(f);fseek(f,0,SEEK_END);*size=ftell(f);return fseek(f,*pos,SEEK_SET)==0;}
static bool sync(void*,uint32_t h){return fflush(files.at(h).fp)==0;}
static uint32_t error(void*,uint32_t h,bool d){return d?dirs.at(h).error:files.at(h).error;}
static bool mkdir(void*,const char* p){std::error_code ec;return fs::create_directory(path(p),ec)&&!ec;}
static bool rename(void*,const char* a,const char* b){if(fs::exists(path(b)))return false;std::error_code ec;fs::rename(path(a),path(b),ec);return !ec;}
static bool health(risc_runtime_health_v1* h){static auto start=std::chrono::steady_clock::now();h->free_heap=16*1024*1024;h->uptime_ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();return true;}
static void yield(uint32_t){}static bool log(const char* p){fprintf(stderr,"%s\n",p);return true;}static bool retain(){held=true;return true;}
static risc_runtime_api_v1 runtime={1,sizeof(runtime),health,yield,log,nullptr,nullptr,nullptr,nullptr,retain};
static const risc_storage_volume_api_v1_ext volume={{1,sizeof(volume),nullptr,refresh,ready,label,stat,dirOpen,dirNext,dirCloseVoid,readOpen,read,writeOpen,write,fileClose,remove,lastError},fileOpen,seek,info,sync,dirRewind,dirClose,error,mkdir,rename};
static bool tellDir(void*,uint32_t h,uint64_t* position){auto i=dirs.find(h);if(i==dirs.end())return false;*position=i->second.offset;return true;}
static bool seekDir(void*,uint32_t h,uint64_t position){auto i=dirs.find(h);if(i==dirs.end()||position>i->second.list.size())return false;i->second.offset=position;return true;}
static int32_t metadata(void* c,const char* p,risc_storage_metadata_v1* out){uint64_t size;bool dir;if(!stat(c,p,&size,&dir))return RISC_STORAGE_FS_NOT_FOUND;out->flags=dir?RISC_STORAGE_DIRECTORY:0;out->size=size;out->modified_seconds=0;return 0;}
static bool truncateFile(void*,uint32_t h,uint64_t length){auto i=files.find(h);if(i==files.end())return false;uint64_t size,pos;info(nullptr,h,&size,&pos);if(length>size)return false;fflush(i->second.fp);return ftruncate(fileno(i->second.fp),length)==0;}
static bool recover(void*,const char* p){std::string backup=std::string(p)+".risc-replace";if(fs::exists(path(backup.c_str()))){if(fs::exists(path(p)))return remove(nullptr,backup.c_str());return rename(nullptr,backup.c_str(),p);}return true;}
static bool failReplace;void hostFailReplace(bool value){failReplace=value;}
static bool replace(void*,const char* a,const char* b){if(failReplace)return false;if(!recover(nullptr,b))return false;std::string backup=std::string(b)+".risc-replace";bool old=fs::exists(path(b));if(old&&!rename(nullptr,b,backup.c_str()))return false;if(!rename(nullptr,a,b)){if(old)rename(nullptr,backup.c_str(),b);return false;}return !old||remove(nullptr,backup.c_str());}
static int32_t observe(void*){return held?RISC_STORAGE_STATE_RETAINED:RISC_STORAGE_STATE_READY;}
static int32_t heapSnapshot(void*,risc_memory_heap_snapshot_v1* s){s->total_bytes=32*1024*1024;s->free_bytes=s->largest_block=16*1024*1024;return 0;}
static int32_t randomFill(void*,void* b,uint32_t n){FILE* f=fopen("/dev/urandom","rb");if(!f)return RISC_RANDOM_UNAVAILABLE;bool ok=fread(b,1,n,f)==n;fclose(f);return ok?0:RISC_RANDOM_UNAVAILABLE;}
static risc_random_api_v1 randomApi={1,sizeof(randomApi),nullptr,randomFill};
static risc_memory_heap_api_v1 heap={1,sizeof(heap),nullptr,heapSnapshot};
static risc_storage_volume_api_v1_fs filesystem;
extern "C" const risc_runtime_api_v1* risc_runtime_get_api(uint32_t v){return v==1&&!held?&runtime:nullptr;}
void hostBind(const char* p){root=p;held=false;filesystem.state.prepared.base.sleep.terminal.power.volume=volume;
 filesystem.state.prepared.base.sleep.terminal.power.volume.base.struct_size=sizeof(filesystem);
 filesystem.state.state_tag=RISC_STORAGE_STATE_TAG;filesystem.state.state_version=1;filesystem.state.observe=observe;
 filesystem.fs_tag=RISC_STORAGE_FS_TAG;filesystem.fs_version=1;filesystem.dir_tell=tellDir;filesystem.dir_seek=seekDir;filesystem.metadata=metadata;filesystem.file_truncate=truncateFile;filesystem.replace_file=replace;filesystem.recover_replace=recover;
 reader::bind(&runtime,&filesystem.state.prepared.base.sleep.terminal.power.volume.base,&heap,&randomApi);}
void hostCheckClosed(){if(!files.empty()||!dirs.empty()){fprintf(stderr,"Leaked files: %zu dirs %zu\n",files.size(),dirs.size());abort();}}
