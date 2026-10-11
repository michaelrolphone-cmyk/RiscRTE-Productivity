/* Exercise the real app and native scene, not a JavaScript reimplementation. */
#define main original_startup_main
#include "app_startup.cpp"
#undef main
extern "C" void reader_test_scene_snapshot(const char*);
static risc_components_document_v1 guiDocument;
static risc_scene_reading_document_v1 guiReading;
static bool isReading;
static uint32_t guiRevision,lastActionRevision;
static unsigned stage,metadataPages,previewCount;
static std::string captureRoot;
static int32_t guiComponents(void*c,uint64_t s,const risc_components_document_v1*d){
 int rc=realScene->components.update(c,s,d);if(!rc){guiDocument=*d;guiRevision=d->revision;isReading=false;}return rc;
}
static int32_t guiPage(void*c,uint64_t s,const risc_scene_reading_document_v1*d,const uint8_t*p,size_t n,uint32_t refresh){
 if(!p){assert(!n&&refresh==RISC_SCENE_PAGE_REFRESH_DEFAULT);++metadataPages;}
 else {assert(n==480*680/8&&refresh==RISC_SCENE_PAGE_REFRESH_CLEAN);++pages;}
 int rc=realReading->present_reading(c,s,d,p,n,refresh);if(!rc){guiReading=*d;guiRevision=d->base.revision;isReading=true;}return rc;
}
static int32_t guiPreview(void*c,uint64_t s,const risc_components_document_v1*d,uint32_t node,const uint8_t*p,size_t n){
 assert(p&&n==368*120/8);unsigned ink=0;for(size_t i=0;i<n;i++)ink+=p[i]!=255;assert(ink>50);++previewCount;
 int rc=realReading->present_preview(c,s,d,node,p,n);if(!rc){guiDocument=*d;guiRevision=d->revision;isReading=false;}return rc;
}
struct Command{bool reading;const char* capture;const char* label;unsigned kind;int value;unsigned readingNode;};
static const Command commands[]={
 {false,"library","",RISC_COMPONENT_MEDIA_ROW,-1,0},
 {false,"book","CONTENTS",0,-1,0},
 {false,"contents","BACK",0,-1,0},
 {false,nullptr,"BOOKMARKS",0,-1,0},
 {false,"bookmarks","",RISC_COMPONENT_DISMISS_ROW,-1,0},
 {true,"reading",nullptr,0,-1,2},
 {true,"controls",nullptr,0,-1,4},
 {true,"bookmark-removed",nullptr,0,-1,4},
 {true,"bookmark-saved",nullptr,0,-1,5},
 {false,"settings","FONT",0,1,0},
 {false,"settings-sans","SIZE",0,-2,0},
 {false,"settings-larger","MODE",0,1,0},
 {false,nullptr,"INVERT",0,1,0},
 {false,"settings-invert","BACK",0,-1,0},
 {true,"inverted-controls",nullptr,0,-1,2},
 {true,"reading-invert",nullptr,0,1244,8},
 {true,"scrolled",nullptr,0,-1,2},
 {true,"scroll-controls",nullptr,0,550,7},
 {true,"scrubbed",nullptr,0,-1,6},
 {false,"contents-after-scrub","BACK",0,-1,0},
 {true,nullptr,nullptr,0,-1,0},
 {false,nullptr,"MARK AS FINISHED",0,1,0},
 {false,"finished-details","REMOVE BOOK",0,-1,0},
 {false,"remove-confirm","KEEP",0,-1,0},
 {false,nullptr,"REMOVE BOOK",0,-1,0},
 {false,nullptr,"REMOVE",0,-1,0},
 {false,"library-after-remove","LIBRARY",0,1,0},
 {false,"library-all","CLOSE READER",0,-1,0},
};
static int32_t guiNext(void*c,uint64_t s,risc_scene_event_v1*e){
 int rc=realScene->components.lifecycle.base.next(c,s,e);if(rc==RISC_SCENE_OK)e->sequence=++eventSequence;
 if(rc!=RISC_SCENE_IDLE||stage>=sizeof(commands)/sizeof(*commands)||guiRevision==lastActionRevision)return rc;
 const auto& cmd=commands[stage];if(isReading!=cmd.reading)return rc;
 risc_scene_navigation_v1 nav{};nav.struct_size=sizeof(nav);uint32_t flags=0;assert(!realScene->components.lifecycle.base.snapshot(c,s,&nav,&flags));if(flags&RISC_SCENE_PRESENTING)return rc;
 if(!isReading&&strcmp(guiDocument.routes[0].title,"READING")==0)return rc;
 if(cmd.capture){std::string path=captureRoot+"/"+cmd.capture+".pbm";reader_test_scene_snapshot(path.c_str());}
 *e={};e->struct_size=sizeof(*e);e->document_revision=guiRevision;e->sequence=++eventSequence;e->kind=RISC_SCENE_ACTION_EVENT;
 if(isReading){e->node=cmd.readingNode;const uint32_t actions[]={guiReading.base.back_action,guiReading.base.previous_action,guiReading.base.menu_action,guiReading.base.next_action,guiReading.bookmark_action,guiReading.layout_action,guiReading.contents_action,guiReading.scrub_action,guiReading.scroll_action};e->action=actions[e->node];if(e->node>=7){e->kind=RISC_SCENE_VALUE_EVENT;e->value=cmd.value;}}
 else if(!strcmp(cmd.label,"BACK")){e->node=0;e->action=guiDocument.routes[0].back_action;}
 else{
  const risc_scene_node_v1* node=nullptr;
  for(unsigned i=0;i<guiDocument.node_count;i++){
   const auto& n=guiDocument.nodes[i];
   if(cmd.kind?n.kind==cmd.kind:!strcmp(n.label,cmd.label)){
    if(cmd.kind==RISC_COMPONENT_MEDIA_ROW&&guiDocument.details[i].tone!=RISC_TONE_MUTED)continue;
    node=&n;break;
   }
  }
  if(!node){fprintf(stderr,"Missing stage %u label %s on %s\n",stage,cmd.label,guiDocument.routes[0].title);abort();}
  e->node=node->id;e->action=node->action;
  if(cmd.value!=-1){e->kind=RISC_SCENE_VALUE_EVENT;e->value=cmd.value==-2?node->value+1:cmd.value;assert(e->value<=node->maximum);}
 }
 fprintf(stderr,"GUI stage %u revision %u action %u node %u value %d\n",stage,guiRevision,e->action,e->node,e->value);
 lastActionRevision=guiRevision;++stage;return RISC_SCENE_OK;
}
static bool guiLog(const char*m){if(strstr(m,"scene ready page=480x680 sd=ready"))readySeen=true;return log(m);}
int main(int argc,char**argv){
 assert(argc==3);hostBind(argv[1]);captureRoot=argv[2];std::filesystem::create_directories(captureRoot);
 {reader::Engine e;assert(e.init(480,680)&&e.scanLibrary()&&e.open("/Books/sample.epub")&&e.beginReading());while(e.busy())assert(e.step());assert(e.turn(1));assert(e.toggleBookmark());assert(e.savePosition());}
 realReading=static_cast<const risc_scene_reading_api_v1*>(reader_test_scene_start());realCheckpoint=&realReading->checkpoint;realRefresh=&realCheckpoint->presentation;realScene=&realRefresh->page;
 sceneCopy=*realReading;sceneCopy.present_reading=guiPage;sceneCopy.present_preview=guiPreview;
 sceneCopy.checkpoint.presentation.page.components.update=guiComponents;sceneCopy.checkpoint.presentation.page.components.lifecycle.base.next=guiNext;sceneApi=&sceneCopy;
 runtime.acquire=acquireApp;runtime.release=releaseApp;runtime.yield_ms=reader_test_scene_tick;runtime.diagnostic=guiLog;
 pthread_attr_t attr;assert(!pthread_attr_init(&attr));assert(!pthread_attr_setstacksize(&attr,32768));pthread_t thread;assert(!pthread_create(&thread,&attr,runApp,nullptr));assert(!pthread_attr_destroy(&attr));assert(!pthread_join(thread,nullptr));
 assert(readySeen&&stage==sizeof(commands)/sizeof(*commands)&&metadataPages>=4&&previewCount>=5&&pages>=4);
 assert(acquired==5&&released==5);hostCheckClosed();reader_test_scene_finish();
 assert(std::filesystem::exists(std::string(argv[1])+"/Books/sample.epub"));
 reader::BookSummary status;assert(reader::readBookSummary("/Books/sample.epub",status));assert((status.flags&reader::BookHidden)&&status.bookmarks==1);
 puts("Reader GUI: 28 real-app intents, native frame captures, font preview, bookmarks, layout, scrolling, scrub, finished and non-destructive remove PASS");
}
