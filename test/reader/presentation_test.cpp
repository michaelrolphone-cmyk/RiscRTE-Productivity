#include "ReaderEngine.h"
#include "ReaderPort.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
void hostBind(const char*);void hostCheckClosed();void hostFailWrites(bool);
static void settle(reader::Engine& e){unsigned n=0;while(e.busy()){assert(++n<10000);assert(e.step());}assert(e.render());}
static bool equal(reader::Position a,reader::Position b){return a.spine==b.spine&&a.offset==b.offset;}
int main(int argc,char**argv){assert(argc==2);hostBind(argv[1]);
 const std::string path="/Books/multi.epub";reader::Position anchor;
 {
  reader::Engine e;assert(e.init(480,680)&&e.open(path,false));settle(e);assert(e.chapterCount()==3);assert(!e.preferences.recentPath[0]);assert(e.beginReading());assert(e.ui.session==1);
  auto ui=e.ui;ui.scroll=1;assert(e.setUi(ui));settle(e);assert(e.scrollBy(220));auto a=e.position();assert(a.offset>0);assert(e.render());
  assert(e.scrollBy(-680));assert(!e.position().spine&&!e.position().offset);assert(!e.scrollBy(-50));
  assert(e.jump({1,0}));settle(e);assert(e.scrollBy(-180));assert(e.position().spine==0);assert(e.render());
  assert(e.scrollBy(400));assert(e.position().spine==1);assert(e.render());anchor=e.position();assert(anchor.offset>0);
  assert(e.toggleBookmark()&&e.isBookmarked());assert(e.state.count==1&&strlen(e.state.marks[0].name)>10);assert(e.savePosition());assert(e.suspend());hostCheckClosed();
  assert(e.open(path));settle(e);assert(e.position().spine==anchor.spine);assert(e.position().offset<=anchor.offset);assert(anchor.offset-e.position().offset<300);assert(e.state.count==1);
  auto settings=e.preferences.settings;settings.pointSize=18;settings.margin=20;assert(e.setLayout(settings));settle(e);assert(e.position().spine==anchor.spine&&e.position().offset<=anchor.offset);assert(e.renderPreview(368,120));assert(e.previewSize()==5520);
  assert(e.jumpProgress(550));settle(e);assert(e.position().spine==1&&e.progress()>450&&e.progress()<650);
  assert(e.jumpProgress(999));settle(e);assert(e.atEnd());assert(e.markFinished(true)&&e.progress()==1000);
  assert(e.beginReading(true));settle(e);assert(!e.position().spine&&!e.position().offset&&e.progress()==0);
  assert(e.markFinished(true));assert(e.removeFromLibrary());assert(!e.preferences.recentPath[0]);assert(e.close());
 }
 reader::BookSummary status;assert(reader::readBookSummary(path,status));assert((status.flags&(reader::BookHidden|reader::BookFinished))==(reader::BookHidden|reader::BookFinished)&&status.bookmarks==1);
 assert(std::filesystem::exists(std::string(argv[1])+path));
 {reader::Engine e;assert(e.init(480,680)&&e.open(path)&&e.beginReading());settle(e);assert(!(e.summary.flags&(reader::BookHidden|reader::BookFinished)));assert(e.state.count==1);assert(e.savePosition());}
 // Existing bookmark/position records keep their on-disk layout. Remove only
 // the new UI sidecars and prove lazy migration doesn't erase that state.
 auto state=std::filesystem::path(argv[1])/"System/State/Applications/ebook-reader";
 std::filesystem::path cache;
 for(auto const& item:std::filesystem::recursive_directory_iterator(state))if(item.path().filename()=="nova-status.a"||item.path().filename()=="nova-status.b"){cache=item.path().parent_path();std::filesystem::remove(item.path());}
 assert(reader::readBookSummary(path,status)&&status.bookmarks==1&&(status.flags&reader::BookStarted));
 {reader::Engine e;assert(e.init(480,680)&&e.open(path));settle(e);assert(e.state.count==1);assert(e.close());}
 // A failed status write restores the in-memory flags. The surviving CRC slot
 // is readable after a fresh engine open.
 {reader::Engine e;assert(e.init(480,680)&&e.open(path));settle(e);auto flags=e.summary.flags;hostFailWrites(true);assert(!e.markFinished(true));assert(e.summary.flags==flags);hostFailWrites(false);reader::clearError();}
 {reader::Engine e;assert(e.init(480,680)&&e.open(path));settle(e);assert(!(e.summary.flags&reader::BookFinished));}
 for(const char* name:{"nova-status.a","nova-status.b"})std::ofstream(cache/name,std::ios::binary)<<"corrupt";
 {reader::Engine e;assert(e.init(480,680));assert(!e.open(path));}
 for(const char* name:{"nova-status.a","nova-status.b"})assert(std::filesystem::file_size(cache/name)==7);
 hostCheckClosed();std::cout<<"Reader NOVA state: 3-spine scroll, reflow/resume anchor, preview, scrub, restart, hide/re-add, migration and status-write/corruption safety PASS\n";
}
