#include "ReaderEngine.h"
#include "port/ReaderPort.h"
#include <cstring>
namespace reader {
struct Header {uint32_t magic,version,generation,length,crc;};
static uint32_t crc32(const uint8_t* b,size_t n){uint32_t c=~0u;while(n--){c^=*b++;for(int i=0;i<8;i++)c=(c>>1)^((c&1)?0xedb88320u:0);}return ~c;}
static bool readSlot(const std::string& p,std::vector<uint8_t>& data,Header& h,size_t size){
 auto f=Storage.open(p.c_str());if(!f)return false;
 bool valid=f.size()==sizeof(h)+size&&f.read(&h,sizeof(h))==sizeof(h)&&h.magic==0x52445231&&h.version==1&&h.length==size&&h.generation;
 if(valid){data.resize(size);valid=f.read(data.data(),size)==int(size)&&crc32(data.data(),size)==h.crc;}
 return f.close()&&valid&&!failed();
}
bool readRecord(const std::string& p,void* out,size_t size,uint32_t& generation,bool& writable){
 std::vector<uint8_t> a,b;Header ha{},hb{};clearError();
 bool ea=Storage.exists((p+".a").c_str()),eb=Storage.exists((p+".b").c_str());
 bool va=ea&&readSlot(p+".a",a,ha,size),vb=eb&&readSlot(p+".b",b,hb,size);
 if(failed()){writable=false;return false;}
 if(!va&&!vb){generation=0;writable=!ea&&!eb;return writable;}
 bool useA=va&&(!vb||ha.generation>=hb.generation);memcpy(out,(useA?a:b).data(),size);generation=useA?ha.generation:hb.generation;writable=true;return true;
}
bool writeRecord(const std::string& p,const void* data,size_t size,uint32_t& generation,bool& writable){
 if(!writable||generation==UINT32_MAX||!alive())return false;
 Header h{0x52445231,1,generation+1,uint32_t(size),crc32((const uint8_t*)data,size)};
 const std::string slot=p+(h.generation%2?".a":".b");auto f=Storage.open(slot.c_str(),O_WRONLY|O_CREAT|O_TRUNC);if(!f)return false;
 bool written=f.write(&h,sizeof(h))==sizeof(h)&&f.write(data,size)==size;f.flush();bool closed=f.close();
 if(!closed||!written||failed()){writable=false;return false;}
 Header verified{};std::vector<uint8_t> copy;if(!readSlot(slot,copy,verified,size)||memcmp(data,copy.data(),size)||verified.generation!=h.generation){writable=false;return false;}
 generation=h.generation;return true;
}
}
