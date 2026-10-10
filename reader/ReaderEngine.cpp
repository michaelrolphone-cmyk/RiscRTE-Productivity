#include "ReaderEngine.h"
#include "ReaderFontSizes.h"
#include "port/MemoryManager.h"
#include <FsHelpers.h>
#include <cstdio>
#include <cstring>
namespace reader {
static bool validSettings(const Settings& s){return memchr(s.family,0,sizeof(s.family))&&s.pointSize>=8&&s.pointSize<=40&&s.margin<=40&&s.lineSpacing<=3&&s.alignment<=4&&s.orientation<=3&&s.indent<=8&&s.wordSpacing>=50&&s.wordSpacing<=200&&s.characterSpacing<=4&&s.paragraphSpacing<=1&&s.hyphenation<=1&&s.embeddedStyle<=1&&s.images<=1;}

Engine::Engine():renderer(display),fontCache(renderer.getFontMap(),renderer.getSdCardFonts(),renderer.getTtfFonts()){}
Engine::~Engine(){close();sdFonts.unloadAll(renderer);renderer.unregisterTtfFont(fontId);vectorFont.reset();freeink::MemoryManager::instance().reset();}
bool Engine::init(unsigned w,unsigned h){
 if(!w||!h||w>1024||h>1024||w%8)return false;
 pixels.resize(w*h/8);if(!display.bind(pixels.data(),w,h))return false;
 renderer.begin();if(!decompressor.init())return false;
 fontCache.setFontDecompressor(&decompressor);renderer.setFontCacheManager(&fontCache);installBuiltinFonts(renderer);
 if(!Storage.mkdir(stateRoot))return false;
 if(!loadSettings())message="Saved settings could not be read";
 fontRegistry.discover();return selectFont();
}
bool Engine::loadSettings(){bool ok=readRecord(std::string(stateRoot)+"/preferences",&preferences,sizeof(preferences),settingsGeneration,settingsWritable);
 const Settings& s=preferences.settings;
 if(!validSettings(s)||!memchr(preferences.recentPath,0,sizeof(preferences.recentPath))){settingsWritable=false;preferences=StoredSettings();return false;}
 return ok;
}
bool Engine::saveSettings(){if(!writeRecord(std::string(stateRoot)+"/preferences",&preferences,sizeof(preferences),settingsGeneration,settingsWritable)){message="Settings save failed";return false;}return true;}
std::vector<uint8_t> Engine::fontSizes()const{if(!fontRegistry.findFamily(preferences.settings.family))return {14};return readerFontPointSizes(&fontRegistry,preferences.settings.family);}
bool Engine::selectFont(){
 fontCache.clearCache();sdFonts.unloadAll(renderer);if(vectorFont){renderer.unregisterTtfFont(fontId);renderer.removeFont(fontId);vectorFont.reset();}for(auto& f:vectorFiles)if(!f.close())return false;
 auto& s=preferences.settings;
 const auto* family=fontRegistry.findFamily(s.family);
 if(!family&&strcmp(s.family,"Noto Serif")==0){s.pointSize=14;fontId=builtinFontId(false,14);return true;}
 if(!family){message="Selected SD font is unavailable";return false;}
 if(!family->vector){if(!sdFonts.loadFamily(*family,renderer,s.pointSize))return false;fontId=sdFonts.getFontId(family->name);return fontId!=0;}
 vectorFont=std::make_unique<TtfEpdFont>();
 for(const auto& file:family->files){if(file.style>=4)continue;auto& source=vectorFiles[file.style];source=Storage.open(file.path.c_str());if(!source)return false;vectorFont->addStreamSource(file.style,SdCardFontRegistry::halFileRead,&source,source.size());}
 if(!vectorFont->load(s.pointSize))return false;
 uint32_t hash=2166136261u;for(const char c:family->name){hash^=uint8_t(c);hash*=16777619u;}fontId=int((hash^s.pointSize)|1u);
 renderer.insertFont(fontId,vectorFont->family());renderer.registerTtfFont(fontId,vectorFont.get());return true;
}
ReaderRenderSpec Engine::spec()const{
 const auto& s=preferences.settings;ReaderRenderSpec v;
 v.fontId=fontId;v.lineCompression=s.lineSpacing==0?0.85f:s.lineSpacing==2?1.2f:s.lineSpacing==3?1.4f:1.0f;
 v.extraParagraphSpacing=s.paragraphSpacing;v.paragraphIndentSpaces=s.indent;v.characterSpacing=int8_t(s.characterSpacing);v.wordSpacingPercent=s.wordSpacing;v.paragraphAlignment=s.alignment;
 v.viewportWidth=renderer.getScreenWidth()-2*s.margin;v.viewportHeight=renderer.getScreenHeight()-2*s.margin;v.hyphenationEnabled=s.hyphenation;v.embeddedStyle=s.embeddedStyle;v.imageRendering=s.images?0:2;return v;
}
bool Engine::openLibrary(){library.close();return Storage.recoverFile(library::libraryIndexPath())&&library.open(library::libraryIndexPath());}
bool Engine::scanLibrary(){library.close();library::BuildStats stats;clearError();bool ok=library::buildLibraryIndex("/",stats,true);if(!ok||failed()){message="Library scan failed; previous index kept";openLibrary();return false;}return openLibrary();}
bool Engine::open(const std::string& path){
 if(!FsHelpers::hasReflowableBookExtension(std::string_view(path))){message="Choose an EPUB, TXT or MD file";return false;}
 if(!close())return false;clearError();if(!selectFont())return false;book=std::make_shared<Epub>(path,stateRoot);
 if(!book->load()||failed()){message=book->getProtectionError().empty()?"Book could not be opened":book->getProtectionError();book.reset();return false;}
 state=BookState();stateGeneration=0;stateWritable=true;
 if(!readRecord(book->getCachePath()+"/position",&state,sizeof(state),stateGeneration,stateWritable)){message="Saved reading position is damaged";book.reset();return false;}
 if(state.count>64||state.position.spine>=unsigned(book->getSpineItemsCount())){message="Saved position is invalid";stateWritable=false;book.reset();return false;}
 for(unsigned i=0;i<state.count;i++)if(!memchr(state.marks[i].name,0,sizeof(state.marks[i].name))||state.marks[i].position.spine>=unsigned(book->getSpineItemsCount())){message="Saved bookmarks are damaged";book.reset();return false;}
 snprintf(preferences.recentPath,sizeof(preferences.recentPath),"%s",path.c_str());
 renderer.setOrientation(static_cast<GfxRenderer::Orientation>((3-preferences.settings.orientation)&3));
 targetOffset=state.position.offset;seeking=true;
 if(!loadSection(state.position.spine)){book.reset();return false;}
 return saveSettings();
}
bool Engine::loadSection(int index){
 section.reset();page.reset();spine=index;pageNumber=0;
 section=std::make_unique<Section>(book,index,renderer);
 auto v=spec();if(!section->loadSectionFile(v)||section->isPartial())if(!section->startBuild(v)){message="Chapter could not be paginated";return false;}
 return step();
}
bool Engine::step(){
 if(!section)return false;
 if(section->isBuilding())if(!section->buildSomeMore(1)||failed()){message="Reading stopped: storage or document error";return false;}
 if(seeking){
  if(!anchor.empty()){auto found=section->getPageForAnchor(anchor);if(found){pageNumber=*found;seeking=false;anchor.clear();}else if(!section->isBuilding()){message="Chapter anchor not found";anchor.clear();seeking=false;}}
  else if(!section->isBuilding()||section->buildReachedVisibleTextOffset(targetOffset)){auto found=section->getPageForVisibleTextOffset(targetOffset);pageNumber=found?*found:0;seeking=false;}
 }
 if(!seeking&&pageNumber<section->pageCount){page=section->loadPage(pageNumber);if(!page||failed()){message="Page cache is unreadable";return false;}}
 return alive();
}
Position Engine::position()const{return {uint32_t(spine),page?page->visibleTextOffset:targetOffset};}
bool Engine::saveBook(){if(!book)return true;return writeRecord(book->getCachePath()+"/position",&state,sizeof(state),stateGeneration,stateWritable);}
bool Engine::savePosition(){if(!book||!page||seeking)return true;state.position=position();if(!saveBook()){message="Reading position was not saved";return false;}return true;}
bool Engine::close(){bool saved=savePosition();section.reset();page.reset();book.reset();anchor.clear();seeking=false;return saved&&!failed();}
bool Engine::suspend(){if(!savePosition())return false;section.reset();page.reset();book.reset();library.close();sdFonts.unloadAll(renderer);if(vectorFont){renderer.unregisterTtfFont(fontId);renderer.removeFont(fontId);vectorFont.reset();}for(auto& f:vectorFiles)if(!f.close())return false;return alive();}
bool Engine::turn(int direction){
 if(!section||seeking)return false;
 if(direction>0){if(pageNumber+1>=section->pageCount&&section->isBuilding()){if(!section->buildSomeMore(1)||failed())return false;}if(pageNumber+1<section->pageCount)++pageNumber;else if(spine+1<book->getSpineItemsCount()){if(!savePosition())return false;targetOffset=0;return loadSection(spine+1);}else return false;}
 else if(pageNumber>0)--pageNumber;else if(spine>0){targetOffset=UINT32_MAX;seeking=true;return loadSection(spine-1);}else return false;
 page=section->loadPage(pageNumber);return bool(page)&&savePosition();
}
bool Engine::jump(Position target){if(!book||target.spine>=unsigned(book->getSpineItemsCount()))return false;targetOffset=target.offset;seeking=true;anchor.clear();return loadSection(target.spine);}
bool Engine::jumpToc(unsigned n){if(!book||n>=unsigned(book->getTocItemsCount()))return false;auto entry=book->getTocItem(n);if(entry.spineIndex<0)return false;targetOffset=0;anchor=entry.anchor;seeking=true;return loadSection(entry.spineIndex);}
bool Engine::setLayout(const Settings& settings){
 if(!validSettings(settings)){message="Invalid layout";return false;}
 Position old=position();Settings previous=preferences.settings;section.reset();page.reset();preferences.settings=settings;
 if(!selectFont()){preferences.settings=previous;selectFont();renderer.setOrientation(static_cast<GfxRenderer::Orientation>((3-previous.orientation)&3));if(book)jump(old);return false;}
 renderer.setOrientation(static_cast<GfxRenderer::Orientation>((3-settings.orientation)&3));
 if(!saveSettings()){preferences.settings=previous;selectFont();renderer.setOrientation(static_cast<GfxRenderer::Orientation>((3-previous.orientation)&3));if(book)jump(old);return false;}
 return !book||jump(old);
}
bool Engine::toggleBookmark(){if(!page||seeking)return false;Position current=position();BookState previous=state;
 for(unsigned i=0;i<state.count;i++)if(state.marks[i].position.spine==current.spine&&state.marks[i].position.offset==current.offset)return removeBookmark(i);
 if(state.count==64){message="Bookmark list is full";return false;}auto& mark=state.marks[state.count++];mark.position=current;snprintf(mark.name,sizeof(mark.name),"Chapter %d - page %d",spine+1,pageNumber+1);
 if(!saveBook()){state=previous;message="Bookmark was not saved";return false;}return true;
}
bool Engine::removeBookmark(unsigned n){if(n>=state.count)return false;BookState previous=state;for(unsigned i=n+1;i<state.count;i++)state.marks[i-1]=state.marks[i];--state.count;if(!saveBook()){state=previous;return false;}return true;}
bool Engine::renameBookmark(unsigned n,const char* name){if(n>=state.count||!name||strlen(name)>=80)return false;BookState previous=state;snprintf(state.marks[n].name,80,"%s",name);if(!saveBook()){state=previous;return false;}return true;}
bool Engine::render(){if(!page||seeking)return false;renderer.clearScreen();auto prewarm=fontCache.createPrewarmScope();int m=preferences.settings.margin;
 page->render(renderer,fontId,m,m);prewarm.endScanAndPrewarm();page->render(renderer,fontId,m,m);return !failed();}
std::string Engine::footer()const{if(!section)return "";char s[96];float fraction=section->pageCount?float(pageNumber)/section->pageCount:0;snprintf(s,sizeof(s),"%u%%  |  CH %d  |  %d / %s%u",unsigned(book->calculateProgress(spine,fraction)*100),spine+1,pageNumber+1,section->isBuilding()?"~":"",section->estimatedTotalPages());return s;}
}
