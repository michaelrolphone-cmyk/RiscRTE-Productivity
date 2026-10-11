#include "host_volume.cpp"
#include "HalStorage.h"
#include <cassert>
#include <utility>
int main(int argc,char** argv){
 assert(argc==3);hostBind(argv[1]);assert(Storage.mkdir(reader::stateRoot));
 const std::string name=std::string(reader::stateRoot)+"/checkpoint-data";
 std::vector<uint8_t> data(10007);for(size_t i=0;i<data.size();++i)data[i]=uint8_t(i*17+3);
 {auto f=Storage.open(name.c_str(),O_RDWR|O_CREAT|O_TRUNC);assert(f&&f.write(data.data(),data.size())==data.size());assert(f.close());}
 if(strcmp(argv[2],"normal")){
  auto f=Storage.open(name.c_str());uint8_t b[20];assert(f.read(b,sizeof(b))==20);assert(HalFile::parkAll());hostCheckClosed();
  const auto before=hostIo;
  if(!strcmp(argv[2],"changed")){FILE* raw=fopen(path(name.c_str()).c_str(),"ab");assert(raw);fputc('x',raw);fclose(raw);}
  else if(!strcmp(argv[2],"missing"))fs::remove(path(name.c_str()));
  else if(!strcmp(argv[2],"revoked"))held=true;
  else assert(false);
  assert(!HalFile::unparkAll()&&held);assert(hostIo.writes==before.writes);
  assert(hostIo.opens==before.opens);puts("Changed/missing/revoked checkpoint stops before reopen or write PASS");return 0;
 }
 auto f=Storage.open(name.c_str());assert(f);const auto before=hostIo;
 for(size_t i=0;i<data.size();++i){assert(f.position()==i);assert(f.read()==data[i]);}
 assert(!f.available()&&f.read()==-1);assert(hostIo.reads-before.reads<100);
 assert(f.seek(9990));uint8_t tail[30]{};assert(f.read(tail,sizeof(tail))==17);assert(!memcmp(tail,data.data()+9990,17));
 assert(f.seek(100));assert(f.read()==data[100]);
 auto writer=Storage.open(name.c_str(),O_RDWR);assert(writer&&writer.seek(8000));
 auto directory=Storage.open(reader::stateRoot);assert(directory&&directory.isDirectory());
 auto entry=directory.openNextFile();assert(entry);entry.close();auto directoryPos=directory.position();
 assert(HalFile::parkAll());hostCheckClosed();const auto parkedIo=hostIo;
 assert(HalFile::parkAll());assert(hostIo.opens==parkedIo.opens&&hostIo.writes==parkedIo.writes);
 HalFile moved=std::move(writer);assert(!writer);
 assert(HalFile::unparkAll());assert(f.position()==101&&moved.position()==8000&&directory.position()==directoryPos);
 assert(f.read()==data[101]);const uint8_t replacement=42;assert(moved.write(replacement)==1);moved.flush();
 assert(moved.size()==data.size());assert(moved.seek(8000)&&moved.read()==42);
 assert(f.close()&&moved.close()&&directory.close());hostCheckClosed();
 auto verify=Storage.open(name.c_str());assert(verify.size()==data.size());assert(verify.seek(7999)&&verify.read()==data[7999]&&verify.read()==42&&verify.read()==data[8001]);verify.close();hostCheckClosed();
 puts("Buffered short reads/cursors/EOF and parked read-write/directory/move/idempotence PASS");
}
