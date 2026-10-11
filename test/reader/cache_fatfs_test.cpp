#include "host_volume.cpp"
#include "ReaderEngine.h"
#include <cassert>
#include <fstream>
extern "C" const risc_storage_volume_api_v1* reader_fatfs_create();
extern "C" void reader_fatfs_check_closed();
extern "C" void reader_fatfs_destroy();
static void seed(const fs::path& source,const risc_storage_volume_api_v1_ext* v){
 for(const auto& e:fs::recursive_directory_iterator(source)){
  std::string p="/"+fs::relative(e.path(),source).generic_string();
  if(e.is_directory()){assert(v->mkdir(v->base.context,p.c_str()));continue;}
  std::ifstream in(e.path(),std::ios::binary);assert(in);
  auto h=v->file_open(v->base.context,p.c_str(),RISC_STORAGE_OPEN_WRITE|RISC_STORAGE_OPEN_CREATE|RISC_STORAGE_OPEN_TRUNCATE);assert(h);
  char bytes[4096];while(in){in.read(bytes,sizeof(bytes));size_t n=in.gcount();if(n)assert(v->base.file_write(v->base.context,h,bytes,n)==n);}
  assert(v->base.file_close(v->base.context,h,true));
 }
}
static void visible(reader::Engine& e){
 unsigned n=0;while(e.waiting()){assert(++n<10000);assert(e.step());}
 assert(e.render());unsigned ink=0;for(size_t i=0;i<e.bitmapSize();++i)ink+=e.bitmap()[i]!=255;assert(ink>100);
}
int main(int argc,char**argv){
 assert(argc==2);const auto* base=reader_fatfs_create();const auto* v=risc_storage_volume_extension(base);
 seed(argv[1],v);reader::bind(&runtime,base,&heap,&randomApi);
 assert(Storage.mkdir(reader::stateRoot));
 const std::string probe=std::string(reader::stateRoot)+"/probe";
 // A cache writer must support readback before closing, including sector crossings
 // and continuation at the previous write cursor.
 {HalFile f;assert(Storage.openFileForWrite("test",probe,f));
  uint8_t expected[1100],actual[1100];for(unsigned i=0;i<sizeof(expected);++i)expected[i]=uint8_t(i*7);
  assert(f.write(expected,sizeof(expected))==sizeof(expected));assert(f.seek(0));
  assert(f.read(actual,sizeof(actual))==int(sizeof(actual)));assert(!memcmp(expected,actual,sizeof(actual)));
  assert(f.seek(511));assert(f.write(uint8_t(93))==1);assert(f.seek(511));assert(f.read()==93);assert(f.close());}
 // The actual provider rejects read-on-write-only and write-on-read-only handles.
 {auto f=Storage.open(probe.c_str(),O_WRONLY);assert(f);uint8_t b=0;assert(f.read(&b,1)==-1);assert(reader::failed());assert(f.close());reader::clearError();}
 {auto f=Storage.open(probe.c_str(),O_RDONLY);assert(f);assert(f.write(uint8_t(1))==0);assert(reader::failed());assert(f.close());reader::clearError();}
 assert(Storage.remove(probe.c_str()));
 for(const char* path:{"/Books/sample.epub","/Books/sample-ncx.epub","/Books/sample.txt","/Books/sample.md"}){
  reader::Position saved;
  {reader::Engine e;assert(e.init(480,632));assert(e.open(path));visible(e);assert(e.busy());
   assert(e.turn(1));visible(e);saved=e.position();assert(saved.offset>0);assert(e.toggleBookmark());assert(e.renameBookmark(0,"Keep this page"));
   assert(e.close());}
  reader_fatfs_check_closed();
  // Reopen a suspended partial cache, then finish and reopen the committed cache.
  {reader::Engine e;assert(e.init(480,632));assert(e.open(path));visible(e);assert(e.state.count==1);assert(!strcmp(e.state.marks[0].name,"Keep this page"));
   assert(e.position().offset==saved.offset);unsigned n=0;while(e.busy()){assert(++n<10000);assert(e.step());}visible(e);
   assert(e.close());assert(e.open(path));visible(e);assert(!e.busy());
   auto layout=e.preferences.settings;layout.margin=layout.margin==15?25:15;assert(e.setLayout(layout));visible(e);assert(e.busy());assert(e.state.count==1);assert(e.close());}
  reader_fatfs_check_closed();
  // The rejected read in 0.1.2 also rejected later header writes, leaving the
  // incomplete sentinel. It must rebuild without deleting saved reader state.
  {Epub identity(path,reader::stateRoot);auto cache=Storage.open((identity.getCachePath()+"/sections/0.bin").c_str(),O_RDWR);
   assert(cache);assert(cache.write(uint8_t(0))==1);assert(cache.close());}
  {reader::Engine e;assert(e.init(480,632));assert(e.open(path));visible(e);assert(e.state.count==1);
   assert(!strcmp(e.state.marks[0].name,"Keep this page"));assert(e.close());}
  reader_fatfs_check_closed();printf("Reader production FatFs: %s fresh/partial/complete/incomplete cache, page turn, bookmark and layout PASS\n",path);
 }
 reader_fatfs_destroy();puts("Production FatFs read/write permissions and cache readback PASS");
}
