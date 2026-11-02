#include "host_volume.cpp"
#include "ReaderEngine.h"
#include <cassert>
#include <fstream>
int main(int argc,char** argv){
 assert(argc==2);hostBind(argv[1]);
 fs::create_directories(root/"Apps/Example/docs");
 fs::create_directories(root/"Bookshelf");
 fs::create_directories(root/"Books/Nested");
 for(unsigned i=0;i<200;++i)std::ofstream(root/("Apps/Example/docs/guide"+std::to_string(i)+".md"))<<"# Application documentation\n";
 fs::copy_file(root/"Books/sample.txt",root/"outside.txt");
 fs::copy_file(root/"Books/sample.md",root/"Bookshelf/not-a-library-book.md");
 fs::copy_file(root/"Books/sample.txt",root/"Books/Nested/nested.txt");
 {
  reader::Engine e;assert(e.init(480,632)&&e.scanLibrary());
  const auto opens=hostIo.opens;auto folder=Storage.open("/Apps/Example/docs");
  assert(folder&&folder.isDirectory());unsigned listed=0;risc_storage_dirent_v1 entry{};
  while(folder.nextEntry(entry)){assert(!entry.is_directory);++listed;}
  assert(folder.close()&&listed==200&&hostIo.opens==opens&&"Listing must not open the directory's files");
  assert(e.library.bookCount()==5&&"Library must include only /Books and its subfolders");
  for(unsigned i=0;i<e.library.bookCount();++i){library::ClixRecord record{};std::string path;
   assert(e.library.readRecord(i,record)&&e.library.readPath(record,path));
   assert(path.rfind("/Books/",0)==0);
  }
  assert(e.open("/outside.txt")&&"Explicit file-open outside /Books must keep working");
  unsigned ticks=0;while(e.busy()){assert(++ticks<10000);assert(e.step());}
  assert(e.render()&&e.close());
 }
 hostCheckClosed();puts("Reader scoped library: 200 application docs excluded, nested books retained, explicit outside file opens PASS");
}
