#include "ReaderEngine.h"
#include "port/ReaderPort.h"
#include "RiscScenePageV1.h"
#include "RiscSceneReadingV1.h"
#include "RiscSceneResidentV1.h"
#include "SceneKeyboardV1.h"
#include "T5FileOpenApi.h"
#include "RiscCppRuntime.h"
#include <cstdio>
#include <FsHelpers.h>
#include <cstring>
#include "port/Logging.h"

namespace {
using namespace reader;
enum class Screen { Library, Reading, Menu, Contents, Bookmarks, Bookmark, Rename, Layout, Fonts, Files, Notice, Starting, Advanced, Finished, Remove, BookmarkTools };
enum Action : uint32_t {
 Back=1, Exit, Read, Previous, Next, Menu, Library, Scan, Browse, Sort,
 OlderRows, MoreRows, OpenRow=100, Contents=200, Bookmarks, AddBookmark,
 Layout, Fonts, FontSize, Margin, LineSpacing, Alignment, ParagraphSpace,
 Indent, CharacterSpace, WordSpace, Hyphens, Embedded, Images, Orientation,
 JumpBookmark, RenameBookmark, DeleteBookmark, Key, Retry,
 LibraryTab=300, Again, MarkFinished, RemoveAsk, RemoveConfirm, ToggleChrome,
 Scrub, Scroll, LayoutMode, Invert, Advanced, FontCycle, BookmarkTools,
 DismissRow=400,

};
struct Row {std::string label,text,path;uint32_t index=0;bool directory=false;BookSummary summary;};
struct App {
 const risc_runtime_api_v1* rt=nullptr;
 const risc_scene_api_v1* scene=nullptr;
 const risc_scene_page_api_v1* presenter=nullptr;
 const risc_scene_page_refresh_api_v1* refreshPresenter=nullptr;
 const risc_scene_checkpoint_api_v1* checkpointScene=nullptr;
 const risc_scene_reading_api_v1* readingPresenter=nullptr;
 risc_scene_reading_document_v1 readingDocument{};
 risc_runtime_capability_v1 grants[5]{};
 unsigned acquired=0;
 risc_scene_resident_v1 resident{};
 risc_components_document_v1 doc{};
 risc_scene_page_geometry_v1 geometry{};
 risc_scene_page_document_v1 pageDocument{};
 uint64_t session=0,lastSequence=0;
 uint32_t revision=0;
 Engine* engine=nullptr;
 Screen screen=Screen::Starting,noticeBack=Screen::Library;
 unsigned first=0,selected=0,sort=0,keyLayer=0,tab=0;
 bool chrome=false,metadataOnly=false;
 Screen listBack=Screen::Menu,layoutBack=Screen::Menu;
 std::string recentTitle;
 BookSummary recentSummary;

 std::vector<Row> rows;
 std::string directory="/",notice,draft;
 bool initialized=false,more=false,done=false,dirty=true,waiting=false,inService=false,libraryUnavailable=false;
 bool scenePaused=false,restoringReading=false,engineSuspended=false;
 std::string suspendedPath;
 risc_scene_event_v1 pending{};
 bool hasPending=false;

 bool checked(int rc){if(rc==RISC_SCENE_RETAINED){retain();return false;}return alive();}
 void show(Screen s){metadataOnly=false;restoringReading=false;screen=s;first=0;dirty=true;}
 void error(const char* fallback){notice=engine&&!engine->message.empty()?engine->message:fallback;noticeBack=engine&&engine->isOpen()?Screen::Menu:Screen::Library;show(Screen::Notice);}
 void copy(char* out,size_t n,const std::string& s){snprintf(out,n,"%s",s.c_str());}
 risc_scene_node_v1& node(unsigned kind,const std::string& label,const std::string& text,unsigned action){
  const unsigned i=doc.node_count++;auto& n=doc.nodes[i];n.id=i+1;n.route=1;n.kind=kind;n.action=action;doc.details[i].marker=6;copy(n.label,sizeof(n.label),label);copy(n.text,sizeof(n.text),text);return n;
 }
 void row(const std::string& label,const std::string& text,unsigned action){node(RISC_COMPONENT_ROW,label,text,action);}
 void stepper(const char* label,unsigned action,int value,int maximum,const std::string& text=""){
  auto& n=node(RISC_COMPONENT_STEPPER,label,text,action);n.value=value;n.minimum=0;n.maximum=maximum;n.step=1;if(!maximum)n.flags|=RISC_SCENE_DISABLED;
 }
 void toggle(const char* label,unsigned action,bool value){auto& n=node(RISC_COMPONENT_SWITCH,label,"",action);n.value=value;n.maximum=1;n.step=1;}
 void begin(const char* title){
  memset(&doc,0,sizeof(doc));doc.api_version=1;doc.struct_size=sizeof(doc);doc.revision=++revision;doc.root=1;doc.screen_key=1+unsigned(screen);if(readingPresenter)doc.flags=RISC_COMPONENTS_DOCUMENT;doc.route_count=1;doc.routes[0].id=1;doc.routes[0].back_action=Back;copy(doc.routes[0].title,sizeof(doc.routes[0].title),title);
 }
 void listRows(){
  for(unsigned i=0;i<rows.size();++i)row(rows[i].label,rows[i].text,OpenRow+i);
  if(first)row("Previous entries","",OlderRows);
  if(more)row("More entries","",MoreRows);
 }
 #include "ReaderGui.inc"
 bool openScene(bool restoringPage=false){
  if(restoringPage)begin("READING");else if(!declare())return false;
  int rc=presenter->components.open(scene->context,&doc,nullptr,&session);if(!checked(rc)||rc){LOG_ERR("startup","scene open failed status=%d",rc);return false;}lastSequence=0;
  rc=presenter->components.lifecycle.configure(scene->context,session,resident.enabled?RISC_SCENE_FEATURE_SHARED_CONTROLS:0);if(rc)LOG_ERR("startup","scene configure failed status=%d",rc);return checked(rc)&&rc==0;
 }
 bool closeScene(){
  if(!session)return true;for(unsigned i=0;i<1000&&alive();++i){int rc=scene->close(scene->context,session);if(!checked(rc))return false;if(!rc){session=0;return true;}if(rc!=RISC_SCENE_AGAIN){retain();return false;}rt->yield_ms(5);}retain();return false;
 }
 bool pauseScene(){
  for(unsigned i=0;i<1000&&alive();++i){int rc=checkpointScene->pause(scene->context,session);if(!checked(rc))return false;if(!rc){scenePaused=true;return true;}if(rc!=RISC_SCENE_AGAIN){retain();return false;}rt->yield_ms(5);}retain();return false;
 }
 bool presentPage(bool clean=false,bool controlsOnly=false){
  pageDocument.revision=++revision;
  if(readingPresenter){
   readingDocument.struct_size=sizeof(readingDocument);readingDocument.base=pageDocument;
   readingDocument.flags=(chrome?RISC_SCENE_READING_CHROME:0)|(engine->ui.invert?RISC_SCENE_READING_INVERT:0)|(engine->ui.scroll?RISC_SCENE_READING_SCROLL:0);
   if(!engineSuspended){
    readingDocument.progress=engine->progress();copy(readingDocument.progress_text,sizeof(readingDocument.progress_text),readingStatus());
    if(engine->isBookmarked())readingDocument.flags|=RISC_SCENE_READING_BOOKMARKED;
   }else readingDocument.flags|=retainedBookmark?RISC_SCENE_READING_BOOKMARKED:0;
   readingDocument.bookmark_action=AddBookmark;readingDocument.layout_action=Layout;readingDocument.contents_action=Contents;readingDocument.scrub_action=Scrub;readingDocument.scroll_action=Scroll;
   int rc=readingPresenter->present_reading(scene->context,session,&readingDocument,controlsOnly?nullptr:engine->bitmap(),controlsOnly?0:engine->bitmapSize(),clean?RISC_SCENE_PAGE_REFRESH_CLEAN:RISC_SCENE_PAGE_REFRESH_DEFAULT);
   return checked(rc)&&rc==0;
  }
  return false;
 }
 bool retainedBookmark=false;
 bool publish(){
  if(screen==Screen::Reading&&!waiting){
   if(metadataOnly){metadataOnly=false;return presentPage(false,true);}
   if(!engine->render()){error("Page could not be rendered");return publish();}
   pageDocument={};pageDocument.struct_size=sizeof(pageDocument);pageDocument.previous_action=Previous;pageDocument.next_action=Next;pageDocument.menu_action=ToggleChrome;pageDocument.back_action=Menu;
   copy(pageDocument.title,sizeof(pageDocument.title),engine->title());copy(pageDocument.footer,sizeof(pageDocument.footer),readingFooter());
   retainedBookmark=engine->isBookmarked();return presentPage(true);
  }
  if(!declare())return false;
  int rc;
  if(screen==Screen::Layout){
   risc_scene_page_geometry_v1 preview{sizeof(preview),0,0,0};
   rc=readingPresenter->preview_geometry(scene->context,&preview);if(!checked(rc)||rc)return false;
   if(!engine->renderPreview(preview.width,preview.height)){error("Font preview unavailable");return publish();}
   rc=readingPresenter->present_preview(scene->context,session,&doc,1,engine->previewBitmap(),engine->previewSize());
  }else rc=presenter->components.update(scene->context,session,&doc);
  return checked(rc)&&rc==0;
 }
 void openBook(const std::string& path){
  restoringReading=false;
  if(!engine->open(path)||!engine->beginReading()){error("Book could not be opened");return;}chrome=false;screen=Screen::Reading;waiting=engine->waiting();dirty=true;
 }
 void back(){
  if(screen==Screen::Reading){show(Screen::Menu);return;}
  if(screen==Screen::Library){done=true;return;}
  if(screen==Screen::Files){if(directory!="/"){size_t n=directory.find_last_of('/');directory=n?directory.substr(0,n):"/";first=0;dirty=true;}else show(Screen::Library);return;}
  if(screen==Screen::Notice){show(noticeBack);return;}
  if(screen==Screen::Fonts||screen==Screen::Advanced){show(Screen::Layout);return;}
  if(screen==Screen::Layout){show(layoutBack);waiting=engine->waiting();return;}
  if(screen==Screen::Contents||screen==Screen::Bookmarks){show(listBack);waiting=engine->waiting();return;}
  if(screen==Screen::Bookmark||screen==Screen::Rename||screen==Screen::BookmarkTools){show(Screen::Bookmarks);return;}
  if(screen==Screen::Remove){show(Screen::Menu);return;}
  if(screen==Screen::Menu||screen==Screen::Finished){action(Library,0);return;}
  show(engine->isOpen()?Screen::Menu:Screen::Library);
 }
 void action(unsigned a,int value){
  if(a>=DismissRow&&a<DismissRow+rows.size()&&screen==Screen::Bookmarks){
   if(!engine->removeBookmark(rows[a-DismissRow].index))error("Bookmark was not removed");else dirty=true;return;
  }
  if(a>=OpenRow&&a<OpenRow+rows.size()){
   Row r=rows[a-OpenRow];
   switch(screen){
   case Screen::Library:if(!engine->open(r.path,false))error("Book could not be opened");else show(Screen::Menu);break;
   case Screen::Files:if(r.directory){directory=r.path;first=0;dirty=true;}else openBook(r.path);break;
   case Screen::Contents:if(engine->jumpToc(r.index)&&engine->markFinished(false)){chrome=false;show(Screen::Reading);waiting=engine->waiting();}else error("Contents location is unavailable");break;
   case Screen::Bookmarks:if(engine->jump(engine->state.marks[r.index].position)&&engine->markFinished(false)){chrome=false;show(Screen::Reading);waiting=engine->waiting();}else error("Bookmark is unavailable");break;
   case Screen::BookmarkTools:selected=r.index;show(Screen::Bookmark);break;
   case Screen::Fonts:{auto settings=engine->preferences.settings;copy(settings.family,sizeof(settings.family),r.label);if(!engine->setLayout(settings))error("Font is unavailable");else show(Screen::Layout);break;}
   default:break;
   }return;
  }
  if(a>=FontSize&&a<=Orientation){
   auto settings=engine->preferences.settings;switch(a){
   case FontSize:{auto sizes=engine->fontSizes();if(value>=0&&unsigned(value)<sizes.size())settings.pointSize=sizes[value];break;}
   case Margin:settings.margin=value*5;break;case LineSpacing:settings.lineSpacing=value;break;case Alignment:settings.alignment=value;break;
   case ParagraphSpace:settings.paragraphSpacing=value;break;case Indent:settings.indent=value;break;case CharacterSpace:settings.characterSpacing=value;break;case WordSpace:settings.wordSpacing=50+value*10;break;
   case Hyphens:settings.hyphenation=value;break;case Embedded:settings.embeddedStyle=value;break;case Images:settings.images=value;break;case Orientation:settings.orientation=value;break;
   }if(!engine->setLayout(settings))error("Layout could not be saved");dirty=true;return;
  }
  switch(a){
  case Retry:delete engine;engine=new Engine;initialized=engine->init(geometry.width,geometry.height);if(initialized){engine->openLibrary();show(Screen::Library);}else error("SD card unavailable");break;
  case Back:if(!initialized)done=true;else back();break;case Exit:done=true;break;
  case Read:
   if(engine->isOpen()){if(!engine->beginReading())error("Reading status was not saved");else {chrome=false;show(Screen::Reading);waiting=engine->waiting();}}
   else openBook(engine->preferences.recentPath);break;
  case Again:if(!engine->beginReading(true))error("Book could not be restarted");else{chrome=false;show(Screen::Reading);waiting=engine->waiting();}break;
  case Previous:case Next:case Scroll:{
   restoringReading=false;
   bool forward=a==Next||(a==Scroll&&value>1024);
   bool changed=engine->ui.scroll?engine->scrollBy(a==Scroll?value-1024:(a==Next?1:-1)*int(geometry.height)*2/3):engine->turn(a==Next?1:-1);
   if(!changed){
    if(!engine->message.empty()||failed())error("Page is unavailable");
    else if(forward&&engine->atEnd()){if(engine->markFinished(true))show(Screen::Finished);else error("Finished status was not saved");}
   }else{chrome=false;waiting=engine->waiting();dirty=true;}
   break;}
  case Scrub:if(!engine->jumpProgress(unsigned(value)))error("Location is unavailable");else{waiting=engine->waiting();dirty=true;}break;
  case ToggleChrome:chrome=!chrome;metadataOnly=true;dirty=true;break;
  case Menu:show(Screen::Menu);break;
  case Library:if(!engine->close())error("Reading position could not be saved");else{engine->openLibrary();show(Screen::Library);}break;
  case Scan:if(!engine->scanLibrary())error("Library scan failed");else show(Screen::Library);break;
  case Browse:directory="/";show(Screen::Files);break;
  case Sort:sort=value;first=0;dirty=true;break;
  case LibraryTab:tab=value;first=0;dirty=true;break;
  case MoreRows:first+=8;dirty=true;break;case OlderRows:first=first>=8?first-8:0;dirty=true;break;
  case Contents:listBack=screen;show(Screen::Contents);break;
  case Bookmarks:listBack=screen;show(Screen::Bookmarks);break;
  case BookmarkTools:show(Screen::BookmarkTools);break;
  case AddBookmark:if(!engine->toggleBookmark())error("Bookmark was not saved");else{retainedBookmark=engine->isBookmarked();metadataOnly=true;dirty=true;}break;
  case Layout:layoutBack=screen;show(Screen::Layout);break;case Fonts:show(Screen::Fonts);break;
  case Advanced:show(Screen::Advanced);break;
  case FontCycle:{auto names=fontNames();if(value>=0&&unsigned(value)<names.size()){auto settings=engine->preferences.settings;copy(settings.family,sizeof(settings.family),names[value]);if(!engine->setLayout(settings))error("Font is unavailable");dirty=true;}break;}
  case LayoutMode:case Invert:{auto ui=engine->ui;if(a==LayoutMode)ui.scroll=value;else ui.invert=value;if(!engine->setUi(ui))error("Display setting was not saved");dirty=true;break;}
  case MarkFinished:if(!engine->markFinished(value!=0))error("Finished status was not saved");else dirty=true;break;
  case RemoveAsk:show(Screen::Remove);break;
  case RemoveConfirm:if(!engine->removeFromLibrary())error("Book was not removed");else action(Library,0);break;
  case JumpBookmark:if(engine->jump(engine->state.marks[selected].position)){chrome=false;show(Screen::Reading);waiting=engine->waiting();}else error("Bookmark is unavailable");break;
  case RenameBookmark:draft=engine->state.marks[selected].name;keyLayer=0;show(Screen::Rename);break;
  case DeleteBookmark:if(!engine->removeBookmark(selected))error("Bookmark was not removed");else show(Screen::Bookmarks);break;
  case Key:
   if(value==RISC_SCENE_KEY_DONE){if(draft.empty())break;if(!engine->renameBookmark(selected,draft.c_str()))error("Name was not saved");else show(Screen::Bookmark);}
   else if(value==RISC_SCENE_KEY_CANCEL)show(Screen::Bookmark);
   else if(value==RISC_SCENE_KEY_LAYER)keyLayer=(keyLayer+1)%4;
   else if(value==RISC_SCENE_KEY_BACKSPACE){if(!draft.empty()){size_t at=draft.size()-1;while(at&&(static_cast<unsigned char>(draft[at])&0xc0)==0x80)--at;draft.resize(at);}}
   else if(value==RISC_SCENE_KEY_CLEAR)draft.clear();
   else {if(value==RISC_SCENE_KEY_SPACE)value=' ';if(value>=32&&value<=126&&draft.size()<71)draft+=char(value);}dirty=true;break;
  default:break;
  }
 }
 bool valid(const risc_scene_event_v1& e){
  if(e.document_revision!=revision||e.sequence<=lastSequence)return false;
  lastSequence=e.sequence;
  if(e.kind==RISC_SCENE_CONTROLS_EVENT||e.kind==RISC_SCENE_SUSPEND_EVENT)return true;
  if(screen==Screen::Reading&&!waiting){
   if(e.kind==RISC_SCENE_VALUE_EVENT)return (e.action==Scrub&&e.node==7&&chrome&&e.value>=0&&e.value<=1000)||(e.action==Scroll&&e.node==8&&engine->ui.scroll&&!chrome&&e.value>=0&&e.value<=2048);
   if(e.kind!=RISC_SCENE_ACTION_EVENT)return false;
   return (e.action==Previous&&e.node==1)||(e.action==Next&&e.node==3)||(e.action==ToggleChrome&&e.node==2)||(e.action==Menu&&e.node==0)||(chrome&&((e.action==AddBookmark&&e.node==4)||(e.action==Layout&&e.node==5)||(e.action==Contents&&e.node==6)));
  }
  if(e.action==Back&&e.node==0)return e.kind==RISC_SCENE_ACTION_EVENT;
  if(!e.node||e.node>doc.node_count)return false;
  const auto& n=doc.nodes[e.node-1];if(n.flags&(RISC_SCENE_DISABLED|RISC_SCENE_HIDDEN))return false;
  if(n.kind==RISC_COMPONENT_DISMISS_ROW&&doc.details[e.node-1].secondary_action==e.action)return e.kind==RISC_SCENE_ACTION_EVENT;
  if(n.action!=e.action)return false;
  bool value=n.kind==RISC_COMPONENT_STEPPER||n.kind==RISC_COMPONENT_SWITCH||n.kind==RISC_COMPONENT_CHECK_ROW||n.kind==RISC_COMPONENT_SEGMENTS||n.kind==RISC_SCENE_KEYBOARD_NODE;
  return value?e.kind==RISC_SCENE_VALUE_EVENT&&(n.kind==RISC_SCENE_KEYBOARD_NODE||(e.value>=n.minimum&&e.value<=n.maximum)):e.kind==RISC_SCENE_ACTION_EVENT;
 }
 bool restoreEngine(){
  if(!engineSuspended)return true;
  // The scene owns the visible page. Reopen storage only for an actual book
  // interaction, not for each periodic power check while the page is idle.
  delete engine;engine=new Engine;engineSuspended=false;
  initialized=engine->init(geometry.width,geometry.height);
  if(!initialized){error("SD card unavailable");return false;}
  engine->openLibrary();
  if(!engine->open(suspendedPath)){error("Book unavailable after resume");return false;}
  while(engine->waiting()&&alive())if(!engine->step()){error("Book processing failed");return false;}
  suspendedPath.clear();waiting=false;return alive();
 }
 bool shell(unsigned reason){
  std::string reopen=engineSuspended?suspendedPath:engine->path();
  const bool restorePage=screen==Screen::Reading&&!waiting&&pageDocument.struct_size==sizeof(pageDocument);
  const bool preserveScene=restorePage&&checkpointScene;
  if(!(preserveScene?pauseScene():closeScene())||(!engineSuspended&&!engine->suspend()))return false;
  uint32_t reply=0;
  int rc=risc_scene_resident_dispatch_reply_v1(&resident,reason,&reply);if(rc==RISC_RESIDENT_RETAINED||!alive()){retain();return false;}if(rc!=RISC_RESIDENT_OK&&rc!=RISC_RESIDENT_BUSY)return false;
  hasPending=false;
  if(preserveScene){
   uint32_t flags=reply&RISC_RESIDENT_REPLY_REDRAW?RISC_SCENE_RESUME_REDRAW|RISC_SCENE_RESUME_CLEAN:0;
   rc=checkpointScene->resume(scene->context,session,flags);if(!checked(rc)||rc)return false;
   scenePaused=false;dirty=false;engineSuspended=true;suspendedPath=reopen;
   return true;
  }else if(restorePage){
   // suspend closes storage but retains the bitmap. Copy it into the new scene
   // before any service callback can paint, then rebuild handles behind it.
   if(!openScene(true)||!presentPage())return false;
   dirty=false;
  }
  // Remount/font discovery and all book handles are recreated after shared UI.
  delete engine;engine=new Engine;
  initialized=engine->init(geometry.width,geometry.height);
  if(!initialized){error("SD card unavailable");}
  else {engine->openLibrary();if(!reopen.empty()){if(!engine->open(reopen))error("Book unavailable after resume");else waiting=engine->waiting();}}
  if(restorePage){
   // The scene already owns the visible pixels. Rebuild the replacement
   // engine's bitmap behind them for subsequent checkpoints, without another
   // publication or clean refresh of this unchanged page.
   restoringReading=initialized&&screen==Screen::Reading&&waiting;
   if(initialized&&screen==Screen::Reading&&!waiting&&!engine->render())error("Page could not be restored");
   return true;
  }
  hasPending=false;dirty=true;return openScene();
 }
 static void service(void* p){
  auto& a=*static_cast<App*>(p);if(!a.session||a.scenePaused||a.inService)return;a.inService=true;
  if(!a.hasPending){risc_scene_event_v1 e{};e.struct_size=sizeof(e);int rc=a.scene->next(a.scene->context,a.session,&e);if(a.checked(rc)&&rc==0){a.pending=e;a.hasPending=true;}}
  a.rt->yield_ms(1);a.inService=false;
 }
};
static intptr_t retainedJump[5];
static void terminalReturn(){__builtin_longjmp(retainedJump,1);}
}
extern "C" __attribute__((visibility("default"))) const risc_resident_app_descriptor_v1_t risc_resident_app_descriptor_v1={1,sizeof(risc_resident_app_descriptor_v1_t),RISC_RESIDENT_ROLE_FOREGROUND,0};
extern "C" __attribute__((visibility("default"))) void app_main(){
 if(__builtin_setjmp(retainedJump))return; // A retained invocation must bypass C++ destructors.
 const auto* rt=risc_runtime_get_api(1);
 if(!rt||rt->struct_size<RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE||!rt->retain_invocation||!rt->acquire||!rt->release||!rt->yield_ms)return;
 bind(rt,nullptr);setTerminal(terminalReturn);risc_cpp_set_failure_handler(reader::retain);
 auto* a=new App; a->rt=rt;
 for(const char* name:{"ui.scene","storage.volume","memory.heap","random.bytes","file.open"}){
  auto& g=a->grants[a->acquired];g.struct_size=sizeof(g);bool ok=a->rt->acquire(name,1,0,&g);if(!alive())return;if(!ok){LOG_ERR("startup","capability unavailable: %s",name);break;}++a->acquired;
 }
 if(a->acquired==5){
  a->scene=static_cast<const risc_scene_api_v1*>(a->grants[0].api);a->presenter=risc_scene_page_get_v1(a->scene);
  a->refreshPresenter=risc_scene_page_refresh_get_v1(a->scene);
  a->checkpointScene=risc_scene_checkpoint_get_v1(a->scene);
  a->readingPresenter=risc_scene_reading_get_v1(a->scene);
  const auto* v=static_cast<const risc_storage_volume_api_v1*>(a->grants[1].api);const auto* ext=risc_storage_volume_extension(v);const auto* heap=static_cast<const risc_memory_heap_api_v1*>(a->grants[2].api);
  const auto* random=static_cast<const risc_random_api_v1*>(a->grants[3].api);bind(a->rt,v,heap,random);
  if(random&&random->api_version==1&&random->struct_size>=sizeof(*random)&&random->fill&&a->presenter&&risc_storage_volume_fs(v)&&heap&&heap->api_version==1&&heap->struct_size>=sizeof(*heap)&&heap->snapshot&&ext&&ext->file_open&&ext->file_seek&&ext->file_info&&ext->file_sync&&ext->dir_rewind&&ext->dir_close_checked&&ext->handle_error&&ext->mkdir&&ext->rename&&risc_scene_resident_bind_v1(a->rt,&a->resident)){
   // Opening the scene establishes the selected display/profile geometry.
   // Querying it before open fails on a fresh scene provider.
   if(a->openScene()){
   setService(App::service,a);
   a->geometry.struct_size=sizeof(a->geometry);
   if(!a->readingPresenter){
    LOG_ERR("startup","Reader requires matching NOVA scene provider");
    a->begin("READER UPDATE");
    a->node(RISC_COMPONENT_EMPTY,"UPDATE FIRMWARE","Install the matching NOVA scene provider with this Reader.",0);
    a->node(RISC_SCENE_ACTION,"CLOSE READER","",Exit);
    int rc=a->presenter->components.update(a->scene->context,a->session,&a->doc);
    while(a->checked(rc)&&rc==RISC_SCENE_OK&&alive()){
     risc_scene_event_v1 event{};event.struct_size=sizeof(event);
     rc=a->scene->next(a->scene->context,a->session,&event);
     if(rc==RISC_SCENE_OK&&(event.kind==RISC_SCENE_SUSPEND_EVENT||event.action==Exit||event.action==Back))break;
     if(rc==RISC_SCENE_IDLE)rc=RISC_SCENE_OK;
     a->rt->yield_ms(5);
    }
   }
   int geometryResult=a->readingPresenter?a->readingPresenter->reading_geometry(a->scene->context,&a->geometry):RISC_SCENE_UNAVAILABLE;
   if(geometryResult)LOG_ERR("startup","page geometry failed status=%d",geometryResult);
   if(a->checked(geometryResult)&&geometryResult==0){
    a->engine=new Engine;
    a->initialized=a->engine->init(a->geometry.width,a->geometry.height);
    {
     if(a->initialized){a->engine->openLibrary();a->show(Screen::Library);}else a->error("SD card unavailable");
     {
      LOG_INF("startup","scene ready page=%ux%u sd=%s",a->geometry.width,a->geometry.height,a->initialized?"ready":"unavailable");
      const auto* files=static_cast<const t5_file_open_api_v1*>(a->grants[4].api);char source[512]{};
      if(a->initialized&&files&&files->struct_size>=sizeof(*files)&&files->source_path_get&&files->source_path_get(source,sizeof(source))){if(!strncmp(source,"/sd/",4))a->openBook(source+3);else a->error("Unsupported book location");}
      unsigned ticks=0;
      while(!a->done&&alive()){
       if(a->waiting&&a->screen==Screen::Reading){if(!a->engine->step())a->error("Book processing failed");else if(!a->engine->waiting()){
        a->waiting=false;
        if(a->restoringReading){a->restoringReading=false;if(!a->engine->render())a->error("Page could not be restored");}
        else a->dirty=true;
       }}
       if(a->dirty){a->dirty=false;if(!a->publish())break;}
       risc_scene_event_v1 e{};e.struct_size=sizeof(e);int rc;
       if(a->hasPending){e=a->pending;a->hasPending=false;rc=0;}else rc=a->scene->next(a->scene->context,a->session,&e);
       if(!a->checked(rc)|| (rc!=RISC_SCENE_OK&&rc!=RISC_SCENE_IDLE))break;
       if(!rc&&a->valid(e)){
        if(e.kind==RISC_SCENE_SUSPEND_EVENT)break;
        if(e.kind==RISC_SCENE_CONTROLS_EVENT){if(a->resident.enabled&&!a->shell(RISC_RESIDENT_CHECKPOINT_CONTROLS))break;}
        else if(e.action==ToggleChrome&&a->screen==Screen::Reading){a->action(e.action,e.value);}
        else if(a->restoreEngine()){a->engine->message.clear();a->action(e.action,e.value);}
       }
       if(++ticks>=20){ticks=0;risc_scene_navigation_v1 nav{};nav.struct_size=sizeof(nav);uint32_t flags=0;rc=a->scene->snapshot(a->scene->context,a->session,&nav,&flags);if(!a->checked(rc)||rc)break;
        int status=risc_scene_resident_poll_work_v1(&a->resident,flags,a->waiting?RISC_RESIDENT_POLL_INHIBIT_IDLE|RISC_RESIDENT_POLL_INHIBIT_POLICY:0);if(status==RISC_RESIDENT_RETAINED){retain();return;}if(status==RISC_RESIDENT_EXIT)break;
        if(a->resident.policy&&!a->waiting&&!(flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY))&&!a->shell(RISC_RESIDENT_CHECKPOINT_POLICY))break;
       }
       a->rt->yield_ms(5);
      }
     }
    }
   }
   }
  }else LOG_ERR("startup","required scene/storage/memory/random interface or resident binding unavailable");
 }
 setService(nullptr,nullptr);
 if(a->closeScene()){
  delete a->engine;
  while(a->acquired){bool ok=a->rt->release(&a->grants[--a->acquired]);if(!alive())return;if(!ok){retain();return;}}
 }
 delete a;setTerminal(nullptr);risc_cpp_set_failure_handler(nullptr);
}
