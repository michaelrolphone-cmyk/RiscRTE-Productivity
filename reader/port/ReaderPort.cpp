#include "HalStorage.h"
#include "Logging.h"
#include <cstdarg>
#include <climits>
namespace {const risc_runtime_api_v1* rt;const risc_storage_volume_api_v1_ext* vol;const risc_storage_volume_api_v1_fs* fs;const risc_memory_heap_api_v1* heap;const risc_random_api_v1* randomSource;bool terminal,ioError;void(*onTerminal)();void(*onService)(void*);void* serviceContext;unsigned serviceTicks;}
namespace reader {
void setTerminal(void(*f)()){onTerminal=f;}
void setService(void(*f)(void*),void* c){onService=f;serviceContext=c;}
void service(){if(onService&&++serviceTicks%16==0)onService(serviceContext);}
void bind(const risc_runtime_api_v1* r,const risc_storage_volume_api_v1* v,const risc_memory_heap_api_v1* m,const risc_random_api_v1* random){rt=r;vol=risc_storage_volume_extension(v);fs=risc_storage_volume_fs(v);heap=m;randomSource=random;terminal=false;ioError=false;}
bool alive(){if(terminal)return false;if(rt&&!risc_runtime_get_api(1)){terminal=true;if(onTerminal)onTerminal();return false;}if(fs&&fs->state.observe(vol->base.context)==RISC_STORAGE_STATE_RETAINED){retain();return false;}return true;}
void retain(){if(!terminal){terminal=true;if(rt&&rt->retain_invocation)rt->retain_invocation();if(onTerminal)onTerminal();}}
bool heapSnapshot(unsigned kind,risc_memory_heap_snapshot_v1* s){*s={};s->struct_size=sizeof(*s);s->memory_class=kind;return heap&&alive()&&heap->snapshot(heap->context,s)==RISC_MEMORY_OK;}
uint64_t heapBytes(unsigned kind,bool largest){risc_memory_heap_snapshot_v1 s{};if(!heapSnapshot(kind,&s))return 0;return largest?s.largest_block:s.free_bytes;}
bool failed(){return ioError||!alive();}void clearError(){ioError=false;}
const risc_storage_volume_api_v1_ext* volume(){return vol;}const risc_runtime_api_v1* runtime(){return rt;}
}
uint32_t millis(){risc_runtime_health_v1 h{};h.struct_size=sizeof(h);if(rt&&reader::alive()&&rt->health&&rt->health(&h))return h.uptime_ms;return 0;}
void delay(uint32_t ms){if(rt&&reader::alive())rt->yield_ms(ms);}
void yield(){reader::service();delay(1);}
ReaderHeapInfo ESP;
uint32_t ReaderHeapInfo::getFreeHeap()const{return reader::heapBytes(RISC_MEMORY_DEFAULT,false);}
uint32_t ReaderHeapInfo::getMaxAllocHeap()const{return reader::heapBytes(RISC_MEMORY_DEFAULT,true);}
void ReaderHeapInfo::restart()const{reader::retain();}
void readerLog(const char* tag,const char* fmt,...){if(!rt||!rt->diagnostic||!reader::alive())return;char buf[256];int n=snprintf(buf,sizeof(buf),"Reader/%s: ",tag);if(n<0||n>=255)return;va_list ap;va_start(ap,fmt);vsnprintf(buf+n,sizeof(buf)-n,fmt,ap);va_end(ap);rt->diagnostic(buf);}
static bool ok(bool v){if(!reader::alive())return false;if(!v)ioError=true;return v;}
static bool writable(const char* p){size_t n=strlen(reader::stateRoot);return p&&strncmp(p,reader::stateRoot,n)==0&&(p[n]=='/'||p[n]==0)&&!strstr(p,"/../");}
HalFile::HalFile(HalFile&& o)noexcept:handle_(o.handle_),directory_(o.directory_),path_(std::move(o.path_)){o.handle_=0;}
HalFile& HalFile::operator=(HalFile&& o)noexcept{if(this!=&o){if(!close())return *this;handle_=o.handle_;directory_=o.directory_;path_=std::move(o.path_);o.handle_=0;}return *this;}
bool HalFile::close(){if(!handle_)return true;if(!reader::alive())return false;bool result=directory_?vol->dir_close_checked(vol->base.context,handle_):vol->base.file_close(vol->base.context,handle_,true);if(!ok(result)){reader::retain();return false;}handle_=0;return true;}
size_t HalFile::getName(char* b,size_t n){if(!n)return 0;const auto p=path_.find_last_of('/');const char* name=path_.c_str()+(p==std::string::npos?0:p+1);size_t len=strlen(name);if(len>=n){ioError=true;b[0]=0;return 0;}memcpy(b,name,len+1);return len;}
uint64_t HalFile::fileSize64(){uint64_t size=0,pos=0;if(!directory_&&isOpen())ok(vol->file_info(vol->base.context,handle_,&size,&pos));return size;}
size_t HalFile::position()const{uint64_t size=0,pos=0;if(isOpen())ok(directory_?fs->dir_tell(vol->base.context,handle_,&pos):vol->file_info(vol->base.context,handle_,&size,&pos));return pos;}
bool HalFile::seek64(uint64_t p){return isOpen()&&ok(directory_?fs->dir_seek(vol->base.context,handle_,p):vol->file_seek(vol->base.context,handle_,p));}
uint32_t HalFile::modificationTime(){risc_storage_metadata_v1 meta{};meta.struct_size=sizeof(meta);if(!isOpen())return 0;int rc=fs->metadata(vol->base.context,path_.c_str(),&meta);if(!ok(rc==RISC_STORAGE_FS_OK))return 0;return meta.flags&RISC_STORAGE_MODIFIED_VALID?uint32_t(meta.modified_seconds):0;}
bool HalFile::seekCur(int64_t d){auto p=position();return (d>=0||uint64_t(-d)<=p)&&seek64(p+d);}
int HalFile::available()const{uint64_t size=0,pos=0;if(!isOpen()||directory_||!ok(vol->file_info(vol->base.context,handle_,&size,&pos)))return 0;return int(std::min<uint64_t>(size-pos,INT_MAX));}
int HalFile::read(void* b,size_t n){if(!isOpen()||directory_)return -1;size_t done=0;while(done<n&&reader::alive()){size_t take=std::min(n-done,size_t(RISC_STORAGE_VOLUME_IO_MAX));size_t got=vol->base.file_read(vol->base.context,handle_,(uint8_t*)b+done,take);if(!reader::alive())return -1;if(got>take||vol->handle_error(vol->base.context,handle_,false)){ioError=true;return -1;}if(!got)break;done+=got;reader::service();}return int(done);}
size_t HalFile::write(const uint8_t* b,size_t n){if(!isOpen()||directory_)return 0;size_t done=0;while(done<n&&reader::alive()){size_t take=std::min(n-done,size_t(RISC_STORAGE_VOLUME_IO_MAX));size_t got=vol->base.file_write(vol->base.context,handle_,b+done,take);if(!reader::alive())return 0;if(got==0||got>take||vol->handle_error(vol->base.context,handle_,false)){ioError=true;break;}done+=got;reader::service();}return done;}
void HalFile::flush(){if(isOpen()&&!directory_)ok(vol->file_sync(vol->base.context,handle_));}
bool HalFile::rename(const char* p){std::string old=path_;return close()&&Storage.rename(old.c_str(),p);}
bool HalFile::truncate(uint64_t length){return isOpen()&&!directory_&&ok(fs->file_truncate(vol->base.context,handle_,length));}
void HalFile::rewindDirectory(){if(isOpen()&&directory_)ok(vol->dir_rewind(vol->base.context,handle_));}
HalFile HalFile::openNextFile(){if(!isOpen()||!directory_)return {};risc_storage_dirent_v1 e{};if(!vol->base.dir_next(vol->base.context,handle_,&e)){ok(vol->handle_error(vol->base.context,handle_,true)==0);return {};}if(!reader::alive())return {};std::string path=path_+(path_=="/"?"":"/")+e.name;auto f=Storage.open(path.c_str());if(!f)ioError=true;return f;}
HalStorage& HalStorage::getInstance(){static HalStorage s;return s;}
bool HalStorage::ready()const{return fs&&reader::alive()&&vol->base.ready(vol->base.context);}
HalFile HalStorage::open(const char* path,oflag_t flags){
 HalFile f;if(!ready()||!path||strlen(path)>=512||strstr(path,"/../"))return f;
 if((flags&O_WRONLY)&&!writable(path)){ioError=true;return f;}
 risc_storage_metadata_v1 meta{};meta.struct_size=sizeof(meta);int status=fs->metadata(vol->base.context,path,&meta);
 if(!reader::alive())return f;if(status!=RISC_STORAGE_FS_OK&&status!=RISC_STORAGE_FS_NOT_FOUND){ioError=true;return f;}
 f.directory_=status==RISC_STORAGE_FS_OK&&(meta.flags&RISC_STORAGE_DIRECTORY);f.path_=path;
 f.handle_=f.directory_?vol->base.dir_open(vol->base.context,path):vol->file_open(vol->base.context,path,flags);reader::alive();return f;
}
bool HalStorage::exists(const char* p){if(!ready())return false;risc_storage_metadata_v1 m{};m.struct_size=sizeof(m);int rc=fs->metadata(vol->base.context,p,&m);if(!reader::alive())return false;if(rc!=RISC_STORAGE_FS_OK&&rc!=RISC_STORAGE_FS_NOT_FOUND)ioError=true;return rc==RISC_STORAGE_FS_OK;}
bool HalStorage::mkdir(const char* p,bool parents){if(!ready()||!writable(p))return false;std::string path=p;if(parents){for(size_t i=1;i<path.size();++i)if(path[i]=='/'){auto part=path.substr(0,i);if(!exists(part.c_str())&&!ok(vol->mkdir(vol->base.context,part.c_str())))return false;}}return exists(p)||ok(vol->mkdir(vol->base.context,p));}
bool HalStorage::remove(const char* p){return ready()&&writable(p)&&(!exists(p)||ok(vol->base.remove(vol->base.context,p)));}
bool HalStorage::rename(const char* a,const char* b){return ready()&&writable(a)&&writable(b)&&ok(vol->rename(vol->base.context,a,b));}
bool HalStorage::replaceFile(const char* a,const char* b){return ready()&&writable(a)&&writable(b)&&!reader::failed()&&ok(fs->replace_file(vol->base.context,a,b));}
bool HalStorage::recoverFile(const char* p){return ready()&&writable(p)&&ok(fs->recover_replace(vol->base.context,p));}
bool HalStorage::removeDir(const char* p){auto d=open(p);if(!d||!d.isDirectory())return false;while(auto e=d.openNextFile()){auto path=e.path_;bool dir=e.isDirectory();if(!e.close()||!(dir?removeDir(path.c_str()):remove(path.c_str())))return false;}return !reader::failed()&&d.close()&&remove(p);}
bool HalStorage::openFileForRead(const char*,const char* p,HalFile& f){f=open(p);return bool(f)&&!f.isDirectory();}
bool HalStorage::openFileForWrite(const char*,const char* p,HalFile& f){f=open(p,O_WRONLY|O_CREAT|O_TRUNC);return bool(f);}
bool HalStorage::readFileToString(const char*,const std::string& p,size_t cap,std::string& out){auto f=open(p.c_str());if(!f||f.isDirectory()||f.size()>cap)return false;out.resize(f.size());return f.read(out.data(),out.size())==int(out.size())&&f.close();}

extern "C" void readerXmlRandom(void* bytes,size_t n){if(!randomSource||n>RISC_RANDOM_MAX_BYTES||!reader::alive()||randomSource->fill(randomSource->context,bytes,n)!=RISC_RANDOM_OK)reader::retain();}
extern "C" int readerXmlDebug(const char* fmt,...){if(!rt||!reader::alive())return 0;char b[256];va_list ap;va_start(ap,fmt);int n=vsnprintf(b,sizeof(b),fmt,ap);va_end(ap);if(rt->diagnostic)rt->diagnostic(b);return n;}
