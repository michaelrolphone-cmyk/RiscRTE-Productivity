#include "ReaderEngine.h"
#include "port/ReaderPort.h"
#include "RiscScenePageV1.h"
#include "RiscSceneResidentV1.h"
#include "SceneKeyboardV1.h"
#include "T5FileOpenApi.h"
#include "RiscCppRuntime.h"
#include <cstdio>
#include <FsHelpers.h>
#include <cstring>

namespace {
using namespace reader;
enum class Screen { Library, Reading, Menu, Contents, Bookmarks, Bookmark, Rename, Layout, Fonts, Files, Notice };
enum Action : uint32_t {
 Back=1, Exit, Read, Previous, Next, Menu, Library, Scan, Browse, Sort,
 OlderRows, MoreRows, OpenRow=100, Contents=200, Bookmarks, AddBookmark,
 Layout, Fonts, FontSize, Margin, LineSpacing, Alignment, ParagraphSpace,
 Indent, CharacterSpace, WordSpace, Hyphens, Embedded, Images, Orientation,
 JumpBookmark, RenameBookmark, DeleteBookmark, Key, Retry,
};
struct Row {std::string label,text,path;uint32_t index=0;bool directory=false;};
struct App {
 const risc_runtime_api_v1* rt=nullptr;
 const risc_scene_api_v1* scene=nullptr;
 const risc_scene_page_api_v1* presenter=nullptr;
 risc_runtime_capability_v1 grants[5]{};
 unsigned acquired=0;
 risc_scene_resident_v1 resident{};
 risc_components_document_v1 doc{};
 risc_scene_page_geometry_v1 geometry{};
 uint64_t session=0,lastSequence=0;
 uint32_t revision=0;
 Engine* engine=nullptr;
 Screen screen=Screen::Library,noticeBack=Screen::Library;
 unsigned first=0,selected=0,sort=0,keyLayer=0;
 std::vector<Row> rows;
 std::string directory="/",notice,draft;
 bool initialized=false,more=false,done=false,dirty=true,waiting=false,inService=false;
 risc_scene_event_v1 pending{};
 bool hasPending=false;

 bool checked(int rc){if(rc==RISC_SCENE_RETAINED){retain();return false;}return alive();}
 void show(Screen s){screen=s;first=0;dirty=true;}
 void error(const char* fallback){notice=engine&&!engine->message.empty()?engine->message:fallback;noticeBack=engine&&engine->isOpen()?Screen::Menu:Screen::Library;show(Screen::Notice);}
 void copy(char* out,size_t n,const std::string& s){snprintf(out,n,"%s",s.c_str());}
 risc_scene_node_v1& node(unsigned kind,const std::string& label,const std::string& text,unsigned action){
  const unsigned i=doc.node_count++;auto& n=doc.nodes[i];n.id=i+1;n.route=1;n.kind=kind;n.action=action;copy(n.label,sizeof(n.label),label);copy(n.text,sizeof(n.text),text);return n;
 }
 void row(const std::string& label,const std::string& text,unsigned action){node(RISC_COMPONENT_ROW,label,text,action);}
 void stepper(const char* label,unsigned action,int value,int maximum,const std::string& text=""){
  auto& n=node(RISC_COMPONENT_STEPPER,label,text,action);n.value=value;n.minimum=0;n.maximum=maximum;n.step=1;if(!maximum)n.flags|=RISC_SCENE_DISABLED;
 }
 void toggle(const char* label,unsigned action,bool value){auto& n=node(RISC_COMPONENT_SWITCH,label,"",action);n.value=value;n.maximum=1;n.step=1;}
 void begin(const char* title){
  memset(&doc,0,sizeof(doc));doc.api_version=1;doc.struct_size=sizeof(doc);doc.revision=++revision;doc.root=1;doc.screen_key=1+unsigned(screen);doc.route_count=1;doc.routes[0].id=1;doc.routes[0].back_action=Back;copy(doc.routes[0].title,sizeof(doc.routes[0].title),title);
 }
 void listRows(){
  for(unsigned i=0;i<rows.size();++i)row(rows[i].label,rows[i].text,OpenRow+i);
  if(first)row("Previous entries","",OlderRows);
  if(more)row("More entries","",MoreRows);
 }
 bool loadRows(){
  rows.clear();if(!initialized)return true;more=false;constexpr unsigned count=8;
  if(screen==Screen::Library){
   const auto order=sort==0?library::SortOrder::RecentDesc:sort==1?library::SortOrder::TitleAsc:library::SortOrder::AuthorAsc;
   for(unsigned i=first;i<engine->library.bookCount()&&i<first+count;++i){
    library::ClixRecord rec{};Row r;
    if(!engine->library.readRecord(engine->library.ordinalForRow(order,i),rec)||!engine->library.readTitle(rec,r.label)||!engine->library.readAuthor(rec,r.text)||!engine->library.readPath(rec,r.path))return false;
    if(r.label.empty()&&!engine->library.readName(rec,r.label))return false;
    rows.push_back(std::move(r));
   }more=first+count<engine->library.bookCount();
  }else if(screen==Screen::Contents){
   for(unsigned i=first;i<unsigned(engine->tocCount())&&i<first+count;++i){auto t=engine->toc(i);rows.push_back({t.title,"","",i,false});}more=first+count<unsigned(engine->tocCount());
  }else if(screen==Screen::Bookmarks){
   for(unsigned i=first;i<engine->state.count&&i<first+count;++i)rows.push_back({engine->state.marks[i].name,"Saved location","",i,false});more=first+count<engine->state.count;
  }else if(screen==Screen::Fonts){
   std::vector<std::string> names={"Noto Serif"};for(const auto& f:engine->fontRegistry.getFamilies())if(f.name!="Noto Serif")names.push_back(f.name);
   for(unsigned i=first;i<names.size()&&i<first+count;++i)rows.push_back({names[i],names[i]==engine->preferences.settings.family?"Selected":"","",i,false});more=first+count<names.size();
  }else if(screen==Screen::Files){
   auto folder=Storage.open(directory.c_str());if(!folder||!folder.isDirectory())return false;
   unsigned visible=0;while(auto f=folder.openNextFile()){
    char name[128];if(!f.getName(name,sizeof(name)))return false;
    bool dir=f.isDirectory();std::string path=directory+(directory=="/"?"":"/")+name;
    if(!f.close())return false;
    if(name[0]=='.'||(directory=="/"&&readerCasecmp(name,"System")==0))continue;
    if(!dir&&!FsHelpers::hasReflowableBookExtension(std::string_view(name)))continue;
    if(visible++<first)continue;if(rows.size()==count){more=true;break;}
    rows.push_back({name,dir?"Folder":"Book",path,0,dir});
   }if(!folder.close()||failed())return false;
  }return alive();
 }
 bool declare(){
  if(!loadRows()){error("The SD card could not be read");}
  switch(screen){
  case Screen::Library:
   begin("READER");
   if(engine->preferences.recentPath[0])row("Continue reading",engine->preferences.recentPath,Read);
   {auto& n=node(RISC_COMPONENT_SEGMENTS,"SORT","",Sort);n.value=sort;n.maximum=2;n.step=1;copy(doc.details[n.id-1].choices,sizeof(doc.details[n.id-1].choices),"RECENT|TITLE|AUTHOR");}
   if(rows.empty())node(RISC_COMPONENT_EMPTY,"No indexed books","Scan your SD card or browse its folders.",0);
   listRows();row("Browse SD","EPUB, TXT, Markdown",Browse);row("Scan library","Read book titles and authors from SD",Scan);row("Close reader","",Exit);break;
  case Screen::Menu:
   begin("BOOK");row("Continue reading",engine->title(),Read);row("Contents","",Contents);row("Bookmarks","",Bookmarks);row("Bookmark this page","Toggle saved location",AddBookmark);row("Layout","Fonts, spacing and margins",Layout);row("Library","",Library);break;
  case Screen::Contents:begin("CONTENTS");listRows();if(rows.empty())node(RISC_COMPONENT_EMPTY,"No contents","",0);break;
  case Screen::Bookmarks:begin("BOOKMARKS");listRows();if(rows.empty())node(RISC_COMPONENT_EMPTY,"No bookmarks","Add one from the book menu.",0);break;
  case Screen::Bookmark:begin("BOOKMARK");node(RISC_SCENE_TEXT_NODE,engine->state.marks[selected].name,"",0);row("Go to bookmark","",JumpBookmark);row("Rename","",RenameBookmark);row("Remove bookmark","",DeleteBookmark);break;
  case Screen::Rename:{begin("BOOKMARK NAME");auto& n=node(RISC_SCENE_KEYBOARD_NODE,"NAME",draft,Key);n.value=keyLayer;n.maximum=3;n.step=1;n.target=71;break;}
  case Screen::Fonts:begin("FONT");listRows();break;
  case Screen::Files:begin("BROWSE SD");copy(doc.subtitle,sizeof(doc.subtitle),directory);listRows();if(rows.empty())node(RISC_COMPONENT_EMPTY,"No books here","",0);break;
  case Screen::Layout:{
   begin("LAYOUT");const auto& s=engine->preferences.settings;row("Font",s.family,Fonts);
   auto sizes=engine->fontSizes();auto at=std::find(sizes.begin(),sizes.end(),s.pointSize);stepper("Size",FontSize,at==sizes.end()?0:at-sizes.begin(),sizes.size()-1,std::to_string(s.pointSize)+" pt");
   stepper("Margins",Margin,s.margin/5,8,std::to_string(s.margin));stepper("Line spacing",LineSpacing,s.lineSpacing,3,s.lineSpacing==0?"Compact":s.lineSpacing==1?"Normal":s.lineSpacing==2?"Wide":"Extra wide");
   stepper("Alignment",Alignment,s.alignment,4,s.alignment==0?"Justified":s.alignment==1?"Left":s.alignment==2?"Center":s.alignment==3?"Right":"Book style");
   toggle("Paragraph spacing",ParagraphSpace,s.paragraphSpacing);stepper("Paragraph indent",Indent,s.indent,8);
   stepper("Character spacing",CharacterSpace,s.characterSpacing,4);stepper("Word spacing",WordSpace,(s.wordSpacing-50)/10,15,std::to_string(s.wordSpacing)+"%");
   toggle("Hyphenation",Hyphens,s.hyphenation);toggle("Book styles",Embedded,s.embeddedStyle);toggle("Images",Images,s.images);
   stepper("Text rotation",Orientation,s.orientation,3,std::to_string(s.orientation*90)+" degrees");break;}
  case Screen::Notice:begin("READER");node(RISC_COMPONENT_EMPTY,"Unable to continue",notice,0);if(initialized)row("Back","",Back);else {row("Retry SD card","",Retry);row("Close reader","",Exit);}break;
  case Screen::Reading:begin("READING");node(RISC_COMPONENT_PROGRESS,"Opening book","Building this page",0);break;
  }
  return true;
 }
 bool openScene(){
  declare();int rc=presenter->components.open(scene->context,&doc,nullptr,&session);if(!checked(rc)||rc)return false;lastSequence=0;
  rc=presenter->components.lifecycle.configure(scene->context,session,resident.enabled?RISC_SCENE_FEATURE_SHARED_CONTROLS:0);return checked(rc)&&rc==0;
 }
 bool closeScene(){
  if(!session)return true;for(unsigned i=0;i<1000&&alive();++i){int rc=scene->close(scene->context,session);if(!checked(rc))return false;if(!rc){session=0;return true;}if(rc!=RISC_SCENE_AGAIN){retain();return false;}rt->yield_ms(5);}retain();return false;
 }
 bool publish(){
  if(screen==Screen::Reading&&!waiting){
   if(!engine->render()){error("Page could not be rendered");return publish();}
   risc_scene_page_document_v1 p{};p.struct_size=sizeof(p);p.revision=++revision;p.previous_action=Previous;p.next_action=Next;p.menu_action=Menu;p.back_action=Menu;copy(p.title,sizeof(p.title),engine->title());copy(p.footer,sizeof(p.footer),engine->footer());
   int rc=presenter->present_page(scene->context,session,&p,engine->bitmap(),engine->bitmapSize());return checked(rc)&&rc==0;
  }
  declare();int rc=presenter->components.update(scene->context,session,&doc);return checked(rc)&&rc==0;
 }
 void openBook(const std::string& path){
  if(!engine->open(path)){error("Book could not be opened");return;}screen=Screen::Reading;waiting=engine->waiting();dirty=true;
 }
 void back(){
  if(screen==Screen::Reading){show(Screen::Menu);return;}
  if(screen==Screen::Library){done=true;return;}
  if(screen==Screen::Files){if(directory!="/"){size_t n=directory.find_last_of('/');directory=n?directory.substr(0,n):"/";first=0;dirty=true;}else show(Screen::Library);return;}
  if(screen==Screen::Notice){show(noticeBack);return;}
  if(screen==Screen::Fonts){show(Screen::Layout);return;}
  if(screen==Screen::Bookmark||screen==Screen::Rename){show(Screen::Bookmarks);return;}
  if(screen==Screen::Menu){show(Screen::Reading);waiting=engine->waiting();return;}
  show(engine->isOpen()?Screen::Menu:Screen::Library);
 }
 void action(unsigned a,int value){
  if(a>=OpenRow&&a<OpenRow+rows.size()){
   Row r=rows[a-OpenRow];
   switch(screen){
   case Screen::Library:openBook(r.path);break;
   case Screen::Files:if(r.directory){directory=r.path;first=0;dirty=true;}else openBook(r.path);break;
   case Screen::Contents:if(engine->jumpToc(r.index)){show(Screen::Reading);waiting=engine->waiting();}else error("Contents location is unavailable");break;
   case Screen::Bookmarks:selected=r.index;show(Screen::Bookmark);break;
   case Screen::Fonts:{auto s=engine->preferences.settings;copy(s.family,sizeof(s.family),r.label);if(!engine->setLayout(s))error("Font is unavailable");else show(Screen::Layout);break;}
   default:break;
   }return;
  }
  if(a>=FontSize&&a<=Orientation){
   auto s=engine->preferences.settings;switch(a){
   case FontSize:{auto sizes=engine->fontSizes();if(value>=0&&unsigned(value)<sizes.size())s.pointSize=sizes[value];break;}
   case Margin:s.margin=value*5;break;case LineSpacing:s.lineSpacing=value;break;case Alignment:s.alignment=value;break;
   case ParagraphSpace:s.paragraphSpacing=value;break;case Indent:s.indent=value;break;case CharacterSpace:s.characterSpacing=value;break;case WordSpace:s.wordSpacing=50+value*10;break;
   case Hyphens:s.hyphenation=value;break;case Embedded:s.embeddedStyle=value;break;case Images:s.images=value;break;case Orientation:s.orientation=value;break;
   }if(!engine->setLayout(s))error("Layout could not be saved");dirty=true;return;
  }
  switch(a){
  case Retry:delete engine;engine=new Engine;initialized=engine->init(geometry.width,geometry.height);if(initialized){engine->openLibrary();show(Screen::Library);}else error("SD card unavailable");break;
  case Back:if(!initialized)done=true;else back();break;case Exit:done=true;break;
  case Read:if(engine->isOpen()){show(Screen::Reading);waiting=engine->waiting();}else openBook(engine->preferences.recentPath);break;
  case Previous:case Next:if(!engine->turn(a==Next?1:-1)&&!engine->message.empty())error("Page is unavailable");else {waiting=engine->waiting();dirty=true;}break;
  case Menu:show(Screen::Menu);break;
  case Library:if(!engine->close())error("Reading position could not be saved");else {engine->openLibrary();show(Screen::Library);}break;
  case Scan:if(!engine->scanLibrary())error("Library scan failed");else show(Screen::Library);break;
  case Browse:directory="/";show(Screen::Files);break;
  case Sort:sort=value;first=0;dirty=true;break;
  case MoreRows:first+=8;dirty=true;break;case OlderRows:first=first>=8?first-8:0;dirty=true;break;
  case Contents:show(Screen::Contents);break;case Bookmarks:show(Screen::Bookmarks);break;
  case AddBookmark:if(!engine->toggleBookmark())error("Bookmark was not saved");else show(Screen::Bookmarks);break;
  case Layout:show(Screen::Layout);break;case Fonts:show(Screen::Fonts);break;
  case JumpBookmark:if(engine->jump(engine->state.marks[selected].position)){show(Screen::Reading);waiting=engine->waiting();}else error("Bookmark is unavailable");break;
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
  if(screen==Screen::Reading&&!waiting)return e.kind==RISC_SCENE_ACTION_EVENT&&(e.action==Previous||e.action==Next||e.action==Menu);
  if(e.action==Back&&e.node==0)return e.kind==RISC_SCENE_ACTION_EVENT;
  if(!e.node||e.node>doc.node_count)return false;
  const auto& n=doc.nodes[e.node-1];if(n.action!=e.action||n.flags&RISC_SCENE_DISABLED)return false;
  bool value=n.kind==RISC_COMPONENT_STEPPER||n.kind==RISC_COMPONENT_SWITCH||n.kind==RISC_COMPONENT_SEGMENTS||n.kind==RISC_SCENE_KEYBOARD_NODE;
  return value?e.kind==RISC_SCENE_VALUE_EVENT&&(n.kind==RISC_SCENE_KEYBOARD_NODE||(e.value>=n.minimum&&e.value<=n.maximum)):e.kind==RISC_SCENE_ACTION_EVENT;
 }
 bool shell(unsigned reason){
  std::string reopen=engine->path();
  if(!closeScene()||!engine->suspend())return false;
  int rc=risc_scene_resident_dispatch_v1(&resident,reason);if(rc==RISC_RESIDENT_RETAINED||!alive()){retain();return false;}if(rc!=RISC_RESIDENT_OK&&rc!=RISC_RESIDENT_BUSY)return false;
  // Remount/font discovery and all book handles are recreated after shared UI.
  delete engine;engine=new Engine;
  initialized=engine->init(geometry.width,geometry.height);
  if(!initialized){error("SD card unavailable");}
  else {engine->openLibrary();if(!reopen.empty()){if(!engine->open(reopen))error("Book unavailable after resume");else waiting=engine->waiting();}}
  hasPending=false;dirty=true;return openScene();
 }
 static void service(void* p){
  auto& a=*static_cast<App*>(p);if(!a.session||a.inService)return;a.inService=true;
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
  auto& g=a->grants[a->acquired];g.struct_size=sizeof(g);bool ok=a->rt->acquire(name,1,0,&g);if(!alive())return;if(!ok)break;++a->acquired;
 }
 if(a->acquired==5){
  a->scene=static_cast<const risc_scene_api_v1*>(a->grants[0].api);a->presenter=risc_scene_page_get_v1(a->scene);
  const auto* v=static_cast<const risc_storage_volume_api_v1*>(a->grants[1].api);const auto* ext=risc_storage_volume_extension(v);const auto* heap=static_cast<const risc_memory_heap_api_v1*>(a->grants[2].api);
  const auto* random=static_cast<const risc_random_api_v1*>(a->grants[3].api);bind(a->rt,v,heap,random);
  if(random&&random->api_version==1&&random->struct_size>=sizeof(*random)&&random->fill&&a->presenter&&risc_storage_volume_fs(v)&&heap&&heap->api_version==1&&heap->struct_size>=sizeof(*heap)&&heap->snapshot&&ext&&ext->file_open&&ext->file_seek&&ext->file_info&&ext->file_sync&&ext->dir_rewind&&ext->dir_close_checked&&ext->handle_error&&ext->mkdir&&ext->rename&&risc_scene_resident_bind_v1(a->rt,&a->resident)){
   a->geometry.struct_size=sizeof(a->geometry);
   int geometryResult=a->presenter->geometry(a->scene->context,&a->geometry);
   if(a->checked(geometryResult)&&geometryResult==0){
    a->engine=new Engine;
    a->initialized=a->engine->init(a->geometry.width,a->geometry.height);
    {
     if(a->initialized)a->engine->openLibrary();else a->error("SD card unavailable");
     if(a->openScene()){
      setService(App::service,a);
      const auto* files=static_cast<const t5_file_open_api_v1*>(a->grants[4].api);char source[512]{};
      if(a->initialized&&files&&files->struct_size>=sizeof(*files)&&files->source_path_get&&files->source_path_get(source,sizeof(source))){if(!strncmp(source,"/sd/",4))a->openBook(source+3);else a->error("Unsupported book location");}
      unsigned ticks=0;
      while(!a->done&&alive()){
       if(a->waiting&&a->screen==Screen::Reading){if(!a->engine->step())a->error("Book processing failed");else if(!a->engine->waiting()){a->waiting=false;a->dirty=true;}}
       if(a->dirty){a->dirty=false;if(!a->publish())break;}
       risc_scene_event_v1 e{};e.struct_size=sizeof(e);int rc;
       if(a->hasPending){e=a->pending;a->hasPending=false;rc=0;}else rc=a->scene->next(a->scene->context,a->session,&e);
       if(!a->checked(rc)|| (rc!=RISC_SCENE_OK&&rc!=RISC_SCENE_IDLE))break;
       if(!rc&&a->valid(e)){
        if(e.kind==RISC_SCENE_SUSPEND_EVENT)break;
        if(e.kind==RISC_SCENE_CONTROLS_EVENT){if(a->resident.enabled&&!a->shell(RISC_RESIDENT_CHECKPOINT_CONTROLS))break;}
        else {a->engine->message.clear();a->action(e.action,e.value);}
       }
       if(++ticks>=20){ticks=0;risc_scene_navigation_v1 nav{};nav.struct_size=sizeof(nav);uint32_t flags=0;rc=a->scene->snapshot(a->scene->context,a->session,&nav,&flags);if(!a->checked(rc)||rc)break;
        int status=risc_scene_resident_poll_v1(&a->resident,flags);if(status==RISC_RESIDENT_RETAINED){retain();return;}if(status==RISC_RESIDENT_EXIT)break;
        if(a->resident.policy&&!(flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY))&&!a->shell(RISC_RESIDENT_CHECKPOINT_POLICY))break;
       }
       a->rt->yield_ms(5);
      }
     }
    }
   }
  }
 }
 setService(nullptr,nullptr);
 if(a->closeScene()){
  delete a->engine;
  while(a->acquired){bool ok=a->rt->release(&a->grants[--a->acquired]);if(!alive())return;if(!ok){retain();return;}}
 }
 delete a;setTerminal(nullptr);risc_cpp_set_failure_handler(nullptr);
}
