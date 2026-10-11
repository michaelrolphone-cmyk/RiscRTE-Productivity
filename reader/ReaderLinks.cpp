#include "ReaderEngine.h"
#include <FsHelpers.h>
#include <algorithm>
namespace reader {
std::vector<DocumentLink> Engine::visibleLinks()const{
 std::vector<DocumentLink> result;if(!page||seeking)return result;
 const int w=renderer.getScreenWidth(),h=renderer.getScreenHeight(),m=preferences.settings.margin;
 const int bw=display.getDisplayWidth(),bh=display.getDisplayHeight(),dy=ui.scroll?scrollOffset:0;
 auto append=[&](const Page* source,int yOffset,unsigned origin){
  if(!source)return;
  for(const auto& link:source->links){
   int x=std::max(0,int(link.x)+m),y=std::max(0,int(link.y)+yOffset);
   int right=std::min(w,int(link.x)+m+link.width),bottom=std::min(h,int(link.y)+yOffset+link.height);
   if(x>=right||y>=bottom||result.size()==64)continue;
   int pw=right-x,ph=bottom-y,px=x,py=y;
   switch(renderer.getOrientation()){
    case GfxRenderer::Portrait:px=y;py=bh-right;std::swap(pw,ph);break;
    case GfxRenderer::LandscapeClockwise:px=bw-right;py=bh-bottom;break;
    case GfxRenderer::PortraitInverted:px=bw-bottom;py=x;std::swap(pw,ph);break;
    case GfxRenderer::LandscapeCounterClockwise:break;
   }
   if(px<0||py<0||px+pw>bw||py+ph>bh)continue;
   DocumentLink v;v.x=uint16_t(px);v.y=uint16_t(py);v.width=uint16_t(pw);v.height=uint16_t(ph);v.sourceSpine=origin;v.href=link.href;result.push_back(std::move(v));
  }
 };
 append(page.get(),m-dy,unsigned(spine));
 if(dy)append(followingPage.get(),h-dy+m,unsigned(spine)+(followingSection?1u:0u));
 return result;
}
/* Link URIs resolve from the source content document, never by basename.
 * Keeping the complete ZIP path avoids collisions between chapters in distinct
 * directories. Fragments split before decoding, so %23 can occur in a filename. */
static bool localPath(const std::string& raw,std::string& out){
 if(raw.find('\\')!=std::string::npos||raw.find(':')!=std::string::npos||raw.rfind("//",0)==0)return false;
 std::vector<std::string> parts;size_t at=0;
 while(at<=raw.size()){
  size_t end=raw.find('/',at);if(end==std::string::npos)end=raw.size();auto part=raw.substr(at,end-at);
  if(part==".."){if(parts.empty())return false;parts.pop_back();}
  else if(!part.empty()&&part!=".")parts.push_back(part);
  if(end==raw.size())break;at=end+1;
 }
 out.clear();for(const auto& p:parts){if(!out.empty())out+='/';out+=p;}return !out.empty();
}
bool Engine::followLink(const DocumentLink& link){
 if(!book||link.sourceSpine>=unsigned(book->getSpineItemsCount())||link.href.empty()){message="Link destination is unavailable";return false;}
 const auto hash=link.href.find('#');const auto query=link.href.find('?');
 auto raw=link.href.substr(0,hash);if(query!=std::string::npos&&query<hash){message="This link type is not supported";return false;}
 std::string fragment=hash==std::string::npos?"":FsHelpers::decodeUriEscapes(link.href.substr(hash+1));
 raw=FsHelpers::decodeUriEscapes(raw);
 if(raw.find('\0')!=std::string::npos||fragment.find('\0')!=std::string::npos){message="Invalid link destination";return false;}
 int target=int(link.sourceSpine);
 if(!raw.empty()){
  if(raw.rfind("//",0)==0||raw.find(':')!=std::string::npos||raw.find('\\')!=std::string::npos){message="External link is not supported";return false;}
  const auto source=book->getSpineItem(target).href;const auto slash=source.find_last_of('/');
  const auto relative=raw[0]=='/'?raw.substr(1):(slash==std::string::npos?"":source.substr(0,slash+1))+raw;
  std::string resolved;if(!localPath(relative,resolved)){message="External or invalid link is not supported";return false;}
  target=-1;
  for(int i=0;i<book->getSpineItemsCount();i++){
   std::string candidate;if(localPath(book->getSpineItem(i).href,candidate)&&candidate==resolved){target=i;break;}
  }
  if(target<0){message="Link destination is not a readable chapter";return false;}
 }
 // Destination validated before discarding the current page or changing state.
 targetOffset=0;seekFraction=-1;anchor=std::move(fragment);seeking=true;
 summary.flags=(summary.flags|BookStarted)&~BookFinished;
 return loadSection(target);
}
}
