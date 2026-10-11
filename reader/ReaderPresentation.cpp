#include "ReaderEngine.h"
#include "port/ReaderPort.h"
#include <cstdio>
#include <cstring>
#include <functional>
namespace reader {
namespace {
uint64_t identity(const std::string& path){uint64_t h=UINT64_C(14695981039346656037);for(unsigned char c:path){h^=c;h*=UINT64_C(1099511628211);}return h;}
std::string summaryBase(const std::string& path){
 // CrossPoint's cache identity remains authoritative (guarded by a host test).
 return std::string(stateRoot)+"/epub_"+std::to_string(std::hash<std::string>{}(path))+"/nova-status";
}
bool validSummary(const BookSummary& s,const std::string& path){return s.version==1&&!(s.flags&~7u)&&s.progress<=1000&&(s.remaining==UINT32_MAX||s.remaining<=9999)&&s.bookmarks<=64&&s.chapter<=s.chapters&&s.identity==identity(path)&&memchr(s.title,0,sizeof(s.title))&&memchr(s.author,0,sizeof(s.author));}
}
bool readBookSummary(const std::string& path,BookSummary& s){
 s=BookSummary();s.identity=identity(path);uint32_t generation=0;bool writable=true;
 if(!readRecord(summaryBase(path),&s,sizeof(s),generation,writable)||!validSummary(s,path))return false;
 // An old position record is not overwritten or resized by the new UI. Migrate
 // its navigation state lazily, without opening/parsing the book or font files.
 if(!generation){
  std::string legacy=summaryBase(path);legacy.resize(legacy.size()-11);legacy+="position";
  if(Storage.exists((legacy+".a").c_str())||Storage.exists((legacy+".b").c_str())){
   auto previous=std::make_unique<BookState>();uint32_t oldGeneration=0;bool oldWritable=true;
   if(!readRecord(legacy,previous.get(),sizeof(*previous),oldGeneration,oldWritable)||previous->count>64||previous->position.spine>=UINT32_MAX-1)return false;
   s.flags=BookStarted;s.chapter=previous->position.spine+1;s.chapters=s.chapter;s.bookmarks=previous->count;
  }
 }
 return !failed();
}
bool Engine::loadUiSettings(){
 bool ok=readRecord(std::string(stateRoot)+"/nova-preferences",&ui,sizeof(ui),uiGeneration,uiWritable);
 if(ui.version!=1||ui.scroll>1||ui.invert>1||ui.reserved){ui=ReaderUiSettings();uiWritable=false;return false;}return ok;
}
bool Engine::setUi(const ReaderUiSettings& next){
 if(next.version!=1||next.scroll>1||next.invert>1||next.reserved)return false;
 Position anchorPosition=position();ReaderUiSettings old=ui;ui=next;
 if(!writeRecord(std::string(stateRoot)+"/nova-preferences",&ui,sizeof(ui),uiGeneration,uiWritable)){ui=old;message="Display settings were not saved";return false;}
 if(book&&old.scroll!=ui.scroll){scrollOffset=0;return jump(anchorPosition);}return true;
}
unsigned Engine::progress()const{
 if(summary.flags&BookFinished)return 1000;
 if(!book||!section||!section->pageCount||seeking)return summary.progress;
 float fraction=(float(pageNumber)+(ui.scroll?float(scrollOffset)/renderer.getScreenHeight():0))/std::max<unsigned>(1,section->estimatedTotalPages());
 float value=book->calculateProgress(spine,std::min(0.9999f,fraction));
 return std::min(999u,unsigned(std::max(0.0f,value)*1000));
}
bool Engine::saveSummary(){
 if(!book)return true;
 snprintf(summary.title,sizeof(summary.title),"%s",book->getTitle().c_str());snprintf(summary.author,sizeof(summary.author),"%s",book->getAuthor().c_str());
 summary.identity=identity(book->getPath());summary.chapter=unsigned(spine)+1;
 summary.chapters=book->getSpineItemsCount();summary.bookmarks=state.count;
 summary.progress=progress();summary.remaining=remainingMinutes();
 return writeRecord(summaryBase(book->getPath()),&summary,sizeof(summary),summaryGeneration,summaryWritable);
}
bool Engine::beginReading(bool restart){
 if(!book||ui.session==UINT64_MAX)return false;
 snprintf(preferences.recentPath,sizeof(preferences.recentPath),"%s",book->getPath().c_str());if(!saveSettings())return false;
 ReaderUiSettings next=ui;++next.session;if(!setUi(next))return false;
 BookSummary previous=summary;summary.flags=(summary.flags|BookStarted)&~(BookHidden|BookFinished);summary.lastSession=ui.session;
 if(restart){
  Position oldPosition=state.position;summary.progress=0;
  if(!jump({0,0})){summary=previous;return false;}
  state.position={0,0};if(!saveBook()){state.position=oldPosition;summary=previous;return false;}
 }
 if(!saveSummary()){summary=previous;message="Reading status was not saved";return false;}return true;
}
bool Engine::markFinished(bool finished){
 if(!book)return false;BookSummary previous=summary;
 if(finished)summary.flags|=BookStarted|BookFinished;else summary.flags&=~BookFinished;
 if(!saveSummary()){summary=previous;message="Finished status was not saved";return false;}return true;
}
bool Engine::removeFromLibrary(){
 if(!book)return false;BookSummary previous=summary;summary.flags|=BookHidden;
 if(!saveSummary()){summary=previous;message="Library removal was not saved";return false;}
 // This is a library operation, not a destructive SD filesystem delete.
 if(path()==preferences.recentPath){StoredSettings old=preferences;preferences.recentPath[0]=0;if(!saveSettings()){preferences=old;summary=previous;saveSummary();return false;}}
 return true;
}
bool Engine::atEnd()const{return book&&section&&!section->isBuilding()&&!seeking&&spine+1>=book->getSpineItemsCount()&&pageNumber+1>=section->pageCount;}
bool Engine::isBookmarked()const{
 if(!page||seeking)return false;Position p=position();
 for(unsigned i=0;i<state.count;i++)if(state.marks[i].position.spine==p.spine&&state.marks[i].position.offset==p.offset)return true;
 return false;
}
std::string Engine::chapterTitle()const{
 if(!book)return "";int index=book->getTocIndexForSpineIndex(spine);
 return index>=0?book->getTocItem(index).title:std::string("CHAPTER ")+std::to_string(spine+1);
}
unsigned Engine::remainingMinutes()const{
 if(summary.flags&BookFinished)return 0;
 if(!book||!page||!section||seeking||!section->estimatedTotalPages())return UINT32_MAX;
 unsigned words=0;for(const auto& el:page->elements)if(el->getTag()==TAG_PageLine)words+=static_cast<const PageLine*>(el.get())->getBlock()->wordCount();
 float weight=book->calculateProgress(spine,1)-book->calculateProgress(spine,0);
 if(words<20||weight<0.00001f)return UINT32_MAX;
 // A labelled estimate, not invented reading telemetry. 250 words/minute;
 // current-page word density and upstream chapter-size weighting extrapolate.
 float remaining=float(words)*section->estimatedTotalPages()/weight*(1000-progress())/1000/250;
 return remaining<1?1:remaining>9999?9999:unsigned(remaining+0.5f);
}
std::string Engine::snippet()const{
 if(!page)return "";std::string text;const uint32_t start=position().offset;
 for(const auto& el:page->elements)if(el->getTag()==TAG_PageLine){
  const auto* line=static_cast<const PageLine*>(el.get())->getBlock();
  for(unsigned i=0;i<line->wordCount();i++){
   auto range=line->wordSourceRange(i);if(range.end!=UINT32_MAX&&range.end<=start)continue;
   const char* word=line->wordText(i);size_t length=strlen(word);
   if(text.size()+length+1>72)return text;
   if(!text.empty())text+=' ';text+=word;
  }
 }
 return text;
}
bool Engine::jumpProgress(unsigned value){
 if(!book||value>1000)return false;float p=std::min(value,999u)/1000.0f;int index=0;
 while(index+1<book->getSpineItemsCount()&&book->calculateProgress(index,1)<p)++index;
 float start=book->calculateProgress(index,0),end=book->calculateProgress(index,1);
 seekFraction=end>start?int(std::max(0.0f,std::min(0.999f,(p-start)/(end-start)))*1000):0;
 targetOffset=0;seeking=true;anchor.clear();summary.flags=(summary.flags|BookStarted)&~BookFinished;return loadSection(index);
}
void Engine::alignScrollAnchor(uint32_t offset){
 if(!page||!ui.scroll||offset<=page->visibleTextOffset)return;
 for(const auto& el:page->elements)if(el->getTag()==TAG_PageLine){
  const auto* line=static_cast<const PageLine*>(el.get())->getBlock();
  for(unsigned i=0;i<line->wordCount();i++){
   const auto range=line->wordSourceRange(i);
   if(range.start<=offset&&offset<range.end){scrollOffset=std::max(0,int(el->yPos));return;}
  }
 }
}
bool Engine::prepareFollowingPage(){
 if(followingPage)return true;if(!section||seeking)return false;
 while(pageNumber+1>=section->pageCount&&section->isBuilding())if(!section->buildSomeMore(1)||failed())return false;
 if(pageNumber+1<section->pageCount){followingPage=section->loadPage(pageNumber+1);return bool(followingPage)&&!failed();}
 if(spine+1>=book->getSpineItemsCount())return true;
 followingSection=std::make_unique<Section>(book,spine+1,renderer);auto layout=spec();
 if(!followingSection->loadSectionFile(layout)||followingSection->isPartial())if(!followingSection->startBuild(layout))return false;
 while(!followingSection->pageCount&&followingSection->isBuilding())if(!followingSection->buildSomeMore(1)||failed())return false;
 if(followingSection->pageCount)followingPage=followingSection->loadPage(0);
 return bool(followingPage)&&!failed();
}
bool Engine::scrollBy(int delta){
 if(!ui.scroll||!page||seeking||!delta)return false;
 int oldSpine=spine,oldPage=pageNumber,oldScroll=scrollOffset;
 int height=renderer.getScreenHeight(),target=scrollOffset+std::max(-height,std::min(height,delta));
 if(target<0){if(!spine&&!pageNumber)target=0;else{if(!turn(-1))return false;while(waiting())if(!step())return false;target+=height;}}
 else if(target>=height){if(atEnd())return false;if(!turn(1))return false;while(waiting())if(!step())return false;target-=height;}
 if(target>0){if(!prepareFollowingPage())return false;if(!followingPage)target=0;}
 scrollOffset=std::max(0,std::min(height-1,target));
 if(spine==oldSpine&&pageNumber==oldPage&&scrollOffset==oldScroll)return false;return savePosition();
}
bool Engine::renderPreview(unsigned width,unsigned height){
 if(!width||width%8||width>display.getDisplayWidth()||height>display.getDisplayHeight())return false;
 auto orientation=renderer.getOrientation();renderer.setOrientation(static_cast<GfxRenderer::Orientation>(3));
 // Uses the selected CrossPoint font/shaper, not the scene chrome's typeface.
 const char* words[]={"The","lamp","turned","slowly,","and","the","water","kept","its","secrets."};
 renderer.clearScreen();auto warm=fontCache.createPrewarmScope();
 for(unsigned pass=0;pass<2;pass++){
  int x=0,y=0,line=renderer.getLineHeight(fontId,spec().lineCompression);std::string text;
  for(const char* word:words){std::string next=text.empty()?word:text+" "+word;
   if(!text.empty()&&renderer.getTextWidth(fontId,next.c_str())>int(width)){renderer.drawText(fontId,x,y,text.c_str());y+=line;text=word;}else text=next;
  }
  if(!text.empty())renderer.drawText(fontId,x,y,text.c_str());if(!pass)warm.endScanAndPrewarm();
 }
 unsigned sourceStride=display.getDisplayWidthBytes();previewPixels.resize(size_t(width/8u)*height);
 for(unsigned y=0;y<height;y++)memcpy(previewPixels.data()+y*(width/8u),pixels.data()+y*sourceStride,width/8u);
 renderer.setOrientation(orientation);
 return !failed();
}
}
