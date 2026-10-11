#include "host_volume.cpp"
#include "ReaderEngine.h"
#include <cassert>
static void ready(reader::Engine& e){unsigned steps=0;while(e.waiting()){assert(++steps<2000);assert(e.step());}assert(e.render());}
static reader::DocumentLink find(reader::Engine& e,const char* href){for(const auto& l:e.visibleLinks())if(l.href==href)return l;fprintf(stderr,"Missing link %s (%zu)\n",href,e.visibleLinks().size());abort();}
int main(int argc,char** argv){
 assert(argc==2);hostBind(argv[1]);
 {reader::Engine e;assert(e.init(480,680)&&e.open("/Books/links.epub"));ready(e);assert(e.tocCount()==5);
  auto origin=e.position();auto target=find(e,"../two/chapter.xhtml#target%20spot");
  assert(e.park());hostCheckClosed();assert(e.unpark()&&e.followLink(target));ready(e);assert(e.chapter()==3&&e.busy());
  assert(e.followLink(find(e,"../one/chapter.xhtml#contents")));ready(e);assert(e.chapter()==1);
  assert(e.followLink(find(e,"../two/part%23two.xhtml#literal%2520id")));ready(e);assert(e.chapter()==4);
  assert(e.jumpToc(4));ready(e);assert(e.chapter()==4); // cached TOC fragments are already decoded
  assert(e.jumpToc(3));ready(e);assert(e.chapter()==1&&e.position().offset>origin.offset);
  assert(e.jump(origin));ready(e);
  assert(e.followLink(find(e,"#late")));ready(e);assert(e.chapter()==1&&e.position().offset>origin.offset);
  assert(e.savePosition());auto late=e.position();assert(e.suspend()&&e.open("/Books/links.epub"));ready(e);assert(e.position().offset==late.offset);
  for(unsigned rotation=0;rotation<4;rotation++){
   auto settings=e.preferences.settings;settings.orientation=rotation;assert(e.setLayout(settings)&&e.jump(origin));ready(e);
   auto links=e.visibleLinks();assert(!links.empty()&&links.size()<=64);
   for(const auto& l:links){assert(l.width&&l.height&&unsigned(l.x)+l.width<=480&&unsigned(l.y)+l.height<=680);bool ink=false;
    for(unsigned y=l.y;y<l.y+l.height;y++)for(unsigned x=l.x;x<l.x+l.width;x++)ink|=!(e.bitmap()[y*60+x/8]&(0x80u>>(x%8)));
    assert(ink&&"Link bounds must rotate with actual rendered text");}
  }
  auto settings=e.preferences.settings;settings.orientation=0;assert(e.setLayout(settings)&&e.jump(origin));ready(e);
  auto before=e.position();assert(!e.followLink(find(e,"missing.xhtml#bad")));assert(e.position().spine==before.spine&&e.position().offset==before.offset);e.message.clear();
  for(const char* href:{"https://example.invalid/#note","//example.invalid/note","../../../../outside.xhtml","x.xhtml?query#frag","x%00.xhtml"}){reader::DocumentLink l;l.href=href;assert(!e.followLink(l));assert(e.position().spine==before.spine&&e.position().offset==before.offset);e.message.clear();}
  assert(e.followLink(find(e,"../two/chapter.xhtml#absent")));bool rejected=false;for(unsigned n=0;e.waiting()&&n<2000;n++)if(!e.step()){rejected=true;break;}assert(rejected&&e.message=="Chapter anchor not found");e.message.clear();assert(e.jump(origin));ready(e);
  auto ui=e.ui;ui.scroll=1;assert(e.setUi(ui)&&e.scrollBy(75));ready(e);for(const auto& l:e.visibleLinks())assert(l.y+l.height<=680&&l.x+l.width<=480);
  assert(e.savePosition());
 }
 hostCheckClosed();puts("Reader links: relative and same-file links, duplicate basenames, escaped path/fragment, incremental anchors, TOC, warm/cold resume, all orientations, clipped scrolling, invalid targets PASS");
}
