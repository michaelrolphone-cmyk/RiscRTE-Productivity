/* Application intents and copied values only. NOVA geometry, type, glyphs,
 * palette and touch targets are implemented by the shared scene components. */
std::string upper(std::string value){for(char& c:value)if(c>='a'&&c<='z')c-=32;return value;}
std::string formatRemaining(unsigned m)const{
 if(m==UINT32_MAX)return "--";
 if(!m)return "0 MIN";return "~"+std::to_string(m)+" MIN";
}
std::string remaining()const{return formatRemaining(engine->remainingMinutes());}
std::string readingStatus()const{return std::to_string((engine->progress()+5)/10)+"%  -  "+remaining()+" LEFT";}
std::string readingFooter()const{
 std::string footer=engine->footer();auto at=footer.find("CH ");if(at!=std::string::npos)footer=footer.substr(at);
 return footer;
}
std::vector<std::string> fontNames()const{
 std::vector<std::string> names={"Noto Serif"};for(const auto& f:engine->fontRegistry.getFamilies())if(f.name!="Noto Serif")names.push_back(f.name);return names;
}
void symbolRow(const std::string& label,const std::string& text,unsigned action,unsigned symbol){auto& n=node(RISC_COMPONENT_STAT_ROW,label,"",action);auto& d=doc.details[n.id-1];d.symbol=symbol;copy(d.badge,sizeof(d.badge),text);}
void primary(const std::string& label,unsigned action){auto& n=node(RISC_SCENE_ACTION,label,"",action);n.flags=RISC_SCENE_PRIMARY;}
void progress(unsigned value){auto& n=node(RISC_COMPONENT_PROGRESS,"","",0);n.value=value;n.maximum=1000;}
void subtitle(const std::string& text){copy(doc.subtitle,sizeof(doc.subtitle),upper(text));}
bool loadRows(){
 rows.clear();if(!initialized)return true;more=false;constexpr unsigned count=8;
 if(screen==Screen::Library){
  recentTitle.clear();recentSummary=BookSummary();
  std::string recent=engine->preferences.recentPath;
  if(!recent.empty()){
   if(!readBookSummary(recent,recentSummary))return false;
   recentTitle=recentSummary.title;
   if(recentTitle.empty()){auto slash=recent.find_last_of('/');recentTitle=recent.substr(slash==std::string::npos?0:slash+1);}
  }
  const auto order=sort==0?library::SortOrder::RecentDesc:sort==1?library::SortOrder::TitleAsc:library::SortOrder::AuthorAsc;
  unsigned visible=0;
  // Keep the latest actual reading session first, independent of the upstream
  // index's arrival ordering. The remaining index stays paged and bounded.
  bool recentRow=sort==0&&!recent.empty()&&!(recentSummary.flags&BookHidden)&&
   !(tab==0&&(recentSummary.flags&BookFinished))&&!(tab==2&&!(recentSummary.flags&BookFinished));
  if(recentRow){if(!first)rows.push_back({recentTitle,recentSummary.author,recent,0,false,recentSummary});++visible;}
  for(unsigned i=0;i<engine->library.bookCount();++i){
   library::ClixRecord rec{};Row r;
   if(!engine->library.readRecord(engine->library.ordinalForRow(order,i),rec)||!engine->library.readPath(rec,r.path))return false;
   if(recentRow&&r.path==recent)continue;
   if(!readBookSummary(r.path,r.summary))return false;
   if(r.summary.flags&BookHidden)continue;
   bool done=r.summary.flags&BookFinished;
   if((tab==0&&done)||(tab==2&&!done))continue;
   if(visible++<first)continue;if(rows.size()==count){more=true;break;}
   engine->library.readTitle(rec,r.label);engine->library.readAuthor(rec,r.text);
   if(engine->library.ioFailed()||failed())return false;
   if(r.label.empty()&&!engine->library.readName(rec,r.label))return false;
   if(r.path==recent)recentTitle=r.label;
   rows.push_back(std::move(r));
  }
  if(engine->library.isOpen())libraryUnavailable=false;
 }else if(screen==Screen::Contents){
  for(unsigned i=first;i<unsigned(engine->tocCount())&&i<first+count;++i){auto t=engine->toc(i);rows.push_back({std::to_string(i+1)+". "+t.title,"","",i,false,{}});}more=first+count<unsigned(engine->tocCount());
 }else if(screen==Screen::Bookmarks||screen==Screen::BookmarkTools){
  for(unsigned i=first;i<engine->state.count&&i<first+count;++i){auto& m=engine->state.marks[i];rows.push_back({"CH "+std::to_string(m.position.spine+1),m.name,"",i,false,{}});}more=first+count<engine->state.count;
 }else if(screen==Screen::Fonts){
  auto names=fontNames();for(unsigned i=first;i<names.size()&&i<first+count;++i)rows.push_back({names[i],names[i]==engine->preferences.settings.family?"SELECTED":"","",i,false,{}});more=first+count<names.size();
 }else if(screen==Screen::Files){
  auto folder=Storage.open(directory.c_str());if(!folder||!folder.isDirectory())return false;
  unsigned visible=0;while(auto f=folder.openNextFile()){
   char name[128];if(!f.getName(name,sizeof(name)))return false;
   bool dir=f.isDirectory();std::string path=directory+(directory=="/"?"":"/")+name;
   if(!f.close())return false;
   if(name[0]=='.'||(directory=="/"&&readerCasecmp(name,"System")==0))continue;
   if(!dir&&!FsHelpers::hasReflowableBookExtension(std::string_view(name)))continue;
   if(visible++<first)continue;if(rows.size()==count){more=true;break;}
   rows.push_back({name,dir?"FOLDER":"BOOK",path,0,dir,{}});
  }if(!folder.close()||failed())return false;
 }
 return alive();
}
void pageLinks(){if(first)row("PREVIOUS ENTRIES","",OlderRows);if(more)row("MORE ENTRIES","",MoreRows);}
bool declare(){
 if(!loadRows()){
  if(screen==Screen::Library){engine->library.close();if(!alive())return false;rows.clear();more=false;libraryUnavailable=true;clearError();}
  else error("The SD card could not be read");
 }
 switch(screen){
 case Screen::Starting:begin("READER");node(RISC_COMPONENT_PROGRESS,"OPENING READER","Preparing library and fonts",0);break;
 case Screen::Library:{
  begin("BOOKS");doc.flags|=RISC_COMPONENTS_ROOT_HEADER;
  auto& add=node(RISC_COMPONENT_HEADER_ACTION,"ADD BOOKS","",Browse);doc.details[add.id-1].symbol=RISC_SYMBOL_DOCUMENT_ADD;
  if(!recentTitle.empty()&&!(recentSummary.flags&(BookHidden|BookFinished))){
   auto& n=node(RISC_COMPONENT_PROGRESS_CARD,"CONTINUE READING",upper(recentTitle),Read);n.value=recentSummary.progress;n.maximum=1000;
   copy(doc.details[n.id-1].choices,sizeof(doc.details[n.id-1].choices),"CH "+std::to_string(recentSummary.chapter?recentSummary.chapter:1)+(recentSummary.chapters?" OF "+std::to_string(recentSummary.chapters):"")+(recentSummary.remaining==UINT32_MAX?"":" - "+formatRemaining(recentSummary.remaining)+" LEFT"));
  }
  {auto& n=node(RISC_COMPONENT_SEGMENTS,"LIBRARY","",LibraryTab);n.value=tab;n.maximum=2;n.step=1;copy(doc.details[n.id-1].choices,sizeof(doc.details[n.id-1].choices),"READING|ALL|FINISHED");}
  if(rows.empty()){auto& n=node(RISC_COMPONENT_EMPTY,libraryUnavailable?"Library index unavailable":"NOTHING HERE",libraryUnavailable?"Scan your SD card or browse its folders.":"Add books with the + button.",0);doc.details[n.id-1].symbol=RISC_SYMBOL_BOOK;}
  for(unsigned i=0;i<rows.size();i++){
   const auto& r=rows[i];auto& n=node(RISC_COMPONENT_MEDIA_ROW,upper(r.label),upper(r.text)+((r.summary.flags&BookStarted)&&!(r.summary.flags&BookFinished)&&r.summary.remaining!=UINT32_MAX?" - "+formatRemaining(r.summary.remaining)+" LEFT":""),OpenRow+i);auto& d=doc.details[n.id-1];d.marker=unsigned(r.summary.identity%6);d.tone=r.summary.flags&BookFinished?RISC_TONE_DEFAULT:r.summary.flags&BookStarted?RISC_TONE_MUTED:RISC_TONE_ACCENT;
   copy(d.badge,sizeof(d.badge),r.summary.flags&BookFinished?"DONE":r.summary.flags&BookStarted?std::to_string((r.summary.progress+5)/10)+"%":"NEW");
  }
  pageLinks();row("Browse SD","EPUB, TXT, Markdown",Browse);row("SCAN LIBRARY","Read book titles and authors from SD",Scan);
  {auto& n=node(RISC_COMPONENT_SEGMENTS,"ORDER","",Sort);n.value=sort;n.maximum=2;n.step=1;copy(doc.details[n.id-1].choices,sizeof(doc.details[n.id-1].choices),"RECENT|TITLE|AUTHOR");}
  row("CLOSE READER","",Exit);break;}
 case Screen::Menu:{
  begin(upper(engine->title()).c_str());subtitle(engine->author());
  auto& stats=node(RISC_COMPONENT_STATS,std::to_string((engine->progress()+5)/10)+"%",remaining(),0);copy(doc.details[stats.id-1].choices,sizeof(doc.details[stats.id-1].choices),"READ|LEFT");progress(engine->progress());
  if(engine->summary.flags&BookFinished)primary("READ AGAIN",Again);
  else primary(engine->summary.flags&BookStarted?"CONTINUE - CH "+std::to_string(engine->chapter()):"START READING",Read);
  symbolRow("CONTENTS",std::to_string(engine->tocCount())+" CHAPTERS",Contents,RISC_SYMBOL_DOCUMENT_LIST);
  symbolRow("BOOKMARKS",std::to_string(engine->state.count),Bookmarks,RISC_SYMBOL_BOOKMARK);
  {auto& n=node(RISC_COMPONENT_CHECK_ROW,"MARK AS FINISHED","",MarkFinished);n.value=(engine->summary.flags&BookFinished)!=0;n.maximum=1;n.step=1;}
  node(RISC_SCENE_TEXT_NODE,"LAST READ",engine->summary.lastSession?"SESSION "+std::to_string(engine->summary.lastSession):"NEVER",0);
  symbolRow("REMOVE BOOK","",RemoveAsk,RISC_SYMBOL_DOCUMENT_DELETE);
  row("TEXT & LAYOUT","Fonts, spacing and margins",Layout);break;}
 case Screen::Contents:{
  begin("CONTENTS");subtitle(engine->title());
  for(unsigned i=0;i<rows.size();i++){
   const auto t=engine->toc(rows[i].index);auto& n=node(RISC_COMPONENT_STAT_ROW,upper(rows[i].label),"",OpenRow+i);
   if(t.spineIndex>=0&&unsigned(t.spineIndex)+1==engine->chapter())n.flags|=RISC_SCENE_PRIMARY;
   copy(doc.details[n.id-1].badge,sizeof(doc.details[n.id-1].badge),t.spineIndex>=0&&unsigned(t.spineIndex)+1<engine->chapter()?"READ":"");
  }pageLinks();if(rows.empty())node(RISC_COMPONENT_EMPTY,"NO CONTENTS","This document has no chapter list.",0);break;}
 case Screen::Bookmarks:{
  begin("BOOKMARKS");subtitle(engine->title());
  for(unsigned i=0;i<rows.size();i++){auto& n=node(RISC_COMPONENT_DISMISS_ROW,rows[i].label,rows[i].text,OpenRow+i);doc.details[n.id-1].secondary_action=DismissRow+i;}
  if(rows.empty()){auto& n=node(RISC_COMPONENT_EMPTY,"NO BOOKMARKS","Tap the bookmark button while reading to save your place.",0);doc.details[n.id-1].symbol=RISC_SYMBOL_BOOKMARK;}
  pageLinks();if(!rows.empty())row("MANAGE NAMES","Rename saved bookmarks",BookmarkTools);break;}
 case Screen::BookmarkTools:begin("BOOKMARK NAMES");listRows();break;
 case Screen::Bookmark:begin("BOOKMARK");node(RISC_SCENE_TEXT_NODE,engine->state.marks[selected].name,"",0);row("GO TO BOOKMARK","",JumpBookmark);row("RENAME","",RenameBookmark);row("REMOVE BOOKMARK","",DeleteBookmark);break;
 case Screen::Rename:{begin("BOOKMARK NAME");auto& n=node(RISC_SCENE_KEYBOARD_NODE,"NAME",draft,Key);n.value=keyLayer;n.maximum=3;n.step=1;n.target=71;break;}
 case Screen::Fonts:begin("FONT");listRows();break;
 case Screen::Files:begin("BROWSE SD");copy(doc.subtitle,sizeof(doc.subtitle),directory);listRows();if(rows.empty())node(RISC_COMPONENT_EMPTY,"NO BOOKS HERE","EPUB, TXT and Markdown files appear here.",0);break;
 case Screen::Layout:{
  begin("TEXT & LAYOUT");subtitle("READER SETTINGS");const auto& settings=engine->preferences.settings;
  node(RISC_COMPONENT_TEXT_PREVIEW,"PREVIEW","",0);
  auto names=fontNames();auto fi=std::find(names.begin(),names.end(),settings.family);stepper("FONT",FontCycle,fi==names.end()?0:int(fi-names.begin()),int(names.size())-1,settings.family);
  auto sizes=engine->fontSizes();auto at=std::find(sizes.begin(),sizes.end(),settings.pointSize);stepper("SIZE",FontSize,at==sizes.end()?0:int(at-sizes.begin()),int(sizes.size())-1,std::to_string(settings.pointSize)+" PT");
  stepper("LINE SPACING",LineSpacing,settings.lineSpacing,3,settings.lineSpacing==0?"TIGHT":settings.lineSpacing==1?"NORMAL":settings.lineSpacing==2?"LOOSE":"EXTRA");
  stepper("MARGINS",Margin,settings.margin/5,8,std::to_string(settings.margin)+" PX");
  node(RISC_COMPONENT_SECTION,"LAYOUT","",0);
  {auto& n=node(RISC_COMPONENT_SEGMENTS,"MODE","",LayoutMode);n.value=engine->ui.scroll;n.maximum=1;n.step=1;copy(doc.details[n.id-1].choices,sizeof(doc.details[n.id-1].choices),"PAGED|SCROLL");}
  toggle("INVERT",Invert,engine->ui.invert);
  node(RISC_SCENE_TEXT_NODE,"PAGES REFLOW","AND KEEP YOUR PLACE.",0);
  row("ADVANCED","Book styles, spacing, images and rotation",Advanced);break;}
 case Screen::Advanced:{
  begin("ADVANCED LAYOUT");const auto& settings=engine->preferences.settings;
  stepper("ALIGNMENT",Alignment,settings.alignment,4,settings.alignment==0?"JUSTIFIED":settings.alignment==1?"LEFT":settings.alignment==2?"CENTER":settings.alignment==3?"RIGHT":"BOOK");
  toggle("PARAGRAPH SPACE",ParagraphSpace,settings.paragraphSpacing);stepper("INDENT",Indent,settings.indent,8,std::to_string(settings.indent));
  stepper("CHARACTER SPACE",CharacterSpace,settings.characterSpacing,4,std::to_string(settings.characterSpacing));stepper("WORD SPACE",WordSpace,(settings.wordSpacing-50)/10,15,std::to_string(settings.wordSpacing)+"%");
  toggle("HYPHENATION",Hyphens,settings.hyphenation);toggle("BOOK STYLES",Embedded,settings.embeddedStyle);toggle("IMAGES",Images,settings.images);
  stepper("TEXT ROTATION",Orientation,settings.orientation,3,std::to_string(settings.orientation*90)+" DEG");break;}
 case Screen::Remove:{
  begin("CONFIRM");doc.flags|=RISC_COMPONENTS_CONFIRM;doc.cancel_action=Back;
  node(RISC_SCENE_TEXT_NODE,"REMOVE FROM LIBRARY?",engine->title()+". The SD book and saved position are kept. Open it from + to add it again.",0);
  node(RISC_SCENE_ACTION,"KEEP","",Back);auto& n=node(RISC_SCENE_ACTION,"REMOVE","",RemoveConfirm);n.flags=RISC_SCENE_DESTRUCTIVE;break;}
 case Screen::Finished:
  begin("FINISHED");subtitle(engine->title());node(RISC_COMPONENT_EMPTY,"YOU FINISHED THE BOOK",engine->title(),0);primary("BACK TO LIBRARY",Library);node(RISC_SCENE_ACTION,"READ AGAIN","",Again);break;
 case Screen::Notice:begin("READER");node(RISC_COMPONENT_EMPTY,"Unable to continue",notice,0);if(initialized)row("BACK","",Back);else {row("RETRY SD CARD","",Retry);row("CLOSE READER","",Exit);}break;
 case Screen::Reading:begin("READING");node(RISC_COMPONENT_PROGRESS,"OPENING BOOK","Preparing this page",0);break;
 }
 return true;
}
