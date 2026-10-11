#include "ReaderEngine.h"
#include "ReaderPort.h"
#include <cassert>
#include <cstdio>
#include <cstring>
void hostBind(const char*);void hostCheckClosed();void hostFailWrites(bool);
int main(int argc,char** argv){
 assert(argc==2);hostBind(argv[1]);
 for(unsigned mode=0;mode<6;++mode){
  auto e=std::make_unique<reader::Engine>();assert(e->init(480,632));assert(e->open("/Books/sample.epub"));
  unsigned steps=0;while(e->busy()){assert(++steps<10000);assert(e->step());}
  e->state.count=3;
  for(unsigned i=0;i<3;++i){e->state.marks[i].position={0,12345+i};snprintf(e->state.marks[i].name,80,"Bookmark %u",i);}
  if(mode==1)e->state.marks[1].position=e->position();
  assert(e->savePosition());auto previous=std::make_unique<reader::BookState>(e->state);
  hostFailWrites(true);
  bool result=mode<2?e->toggleBookmark():mode<5?e->removeBookmark(mode-2):e->renameBookmark(1,"Replacement");
  assert(!result);assert(!memcmp(&e->state,previous.get(),sizeof(e->state)));
  hostFailWrites(false);reader::clearError();assert(!e->close());e.reset();hostCheckClosed();
 }
 puts("Failed bookmark add/toggle/remove first-middle-last/rename preserve the complete in-memory state PASS");
}
