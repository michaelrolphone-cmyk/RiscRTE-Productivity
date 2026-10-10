#include "ReaderEngine.h"
#include "ReaderPort.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <filesystem>
void hostBind(const char*);void hostCheckClosed();void hostFailReplace(bool);
static void settle(reader::Engine& e){unsigned n=0;while(e.busy()){assert(++n<10000);assert(e.step());}assert(e.render());unsigned ink=0;for(size_t i=0;i<e.bitmapSize();++i)ink+=e.bitmap()[i]!=255;assert(ink>100);}
int main(int argc,char** argv){assert(argc==2);hostBind(argv[1]);
 {reader::Engine e;assert(e.init(480,632));assert(e.scanLibrary());assert(e.library.bookCount()==4);
 hostFailReplace(true);assert(!e.scanLibrary());hostFailReplace(false);assert(e.openLibrary());assert(e.library.bookCount()==4);reader::clearError();
 for(const char* path:{"/Books/sample.epub","/Books/sample-ncx.epub","/Books/sample.txt","/Books/sample.md"}){
  assert(e.open(path));settle(e);if(std::string(path).find(".epub")!=std::string::npos){std::cerr<<path<<" toc "<<e.tocCount()<<" spine "<<e.toc(0).spineIndex<<" anchor "<<e.toc(0).anchor<<"\n";assert(e.tocCount()==1);assert(e.jumpToc(0));settle(e);assert(e.position().spine==0);}std::cout<<path<<" "<<e.title()<<" "<<e.footer()<<"\n";
  unsigned black=0;for(size_t i=0;i<e.bitmapSize();i++)black+=e.bitmap()[i]!=255;assert(black>100);
  std::ofstream image(std::string(argv[1])+"/"+std::filesystem::path(path).filename().string()+".pbm",std::ios::binary);image<<"P4\n480 632\n";for(size_t i=0;i<e.bitmapSize();i++){char c=~e.bitmap()[i];image.write(&c,1);}
  assert(e.turn(1));settle(e);auto position=e.position();assert(position.offset>0);assert(e.toggleBookmark());assert(e.state.count==1);assert(e.renameBookmark(0,"Saved page"));
  auto settings=e.preferences.settings;settings.pointSize=18;settings.margin=25;settings.lineSpacing=2;assert(e.setLayout(settings));settle(e);assert(e.state.count==1);assert(e.jump(e.state.marks[0].position));settle(e);assert(e.position().offset<=position.offset);assert(e.savePosition());assert(e.close());assert(e.open(path));settle(e);assert(e.state.count==1);assert(!strcmp(e.state.marks[0].name,"Saved page"));assert(e.close());
 }
 }
 hostCheckClosed();
 if(std::filesystem::exists(std::string(argv[1])+"/fonts/TestFont.ttf")){
  reader::Engine e;assert(e.init(480,632));assert(!e.fontRegistry.getFamilies().empty());
  auto settings=e.preferences.settings;auto family=e.fontRegistry.getFamilies().front();assert(family.vector);
  snprintf(settings.family,sizeof(settings.family),"%s",family.name.c_str());settings.pointSize=16;
  assert(e.setLayout(settings));assert(e.open("/Books/sample.epub"));settle(e);assert(e.turn(1));settle(e);auto position=e.position();
  assert(e.suspend());hostCheckClosed();assert(e.open("/Books/sample.epub"));settle(e);assert(e.position().offset<=position.offset);assert(e.close());
 }
 hostCheckClosed();
 // Corrupt settings must not be silently overwritten with defaults.
 auto state=std::filesystem::path(argv[1])/"System/State/Applications/ebook-reader";
 for(const char* slot:{"preferences.a","preferences.b"})std::ofstream(state/slot,std::ios::binary)<<"corrupt";
 {reader::Engine e;assert(e.init(480,632));assert(!e.saveSettings());}
 for(const char* slot:{"preferences.a","preferences.b"})assert(std::filesystem::file_size(state/slot)==7);
 hostCheckClosed();std::cout<<"Reader open, pagination, layout anchors, bookmark persistence, library and short I/O tests passed\n";
}
