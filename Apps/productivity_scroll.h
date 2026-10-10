#pragma once
#if (!defined(ALARM_NATIVE_UTC) && !defined(TIMECARD_NATIVE_TIME)) || !defined(PORTABLE_APP_TOUCH_SCROLL) || !defined(PORTABLE_TOUCH_SCROLL)
#error "Productivity touch scrolling requires an explicit native X4 profile"
#endif
#include "PortableTouchScroll.h"
#include "PaperFrame.h"
#include "PortablePaperScroll.h"
typedef struct {
 portable_touch_scroll motion;
 unsigned generation,submitted_generation,completed_generation,hit_generation;
 int submitted_q8,completed_q8,hit_q8,hit_row,hit_x,hit_y;
 int submitted_selected,completed_selected;
 bool hit_ready,dirty;
} productivity_scroll;
static inline void productivity_scroll_visible(productivity_scroll *s) {
 if(portable_paper_scroll_settled()) {
  s->completed_generation=s->submitted_generation;s->completed_q8=s->submitted_q8;
  s->completed_selected=s->submitted_selected;
 }
}
static inline void productivity_scroll_interrupt(productivity_scroll *s) {
 portable_scroll_cancel(&s->motion);s->hit_ready=false;s->dirty=true;
}
static inline void productivity_scroll_changed(productivity_scroll *s,bool reset) {
 productivity_scroll_interrupt(s);++s->generation;
 if(reset)s->motion.position_q8=0;
}
static inline void productivity_scroll_record(productivity_scroll *s,int selected) {
 s->submitted_generation=s->generation;s->submitted_q8=s->motion.position_q8;s->submitted_selected=selected;
 s->dirty=false;productivity_scroll_visible(s);
}
static inline unsigned productivity_scroll_input(productivity_scroll *s,const portable_touch_sample *sample,uint32_t now) {
 productivity_scroll_visible(s);
 if(!sample->valid||sample->cancelled) {
  if(!s->motion.blocked){productivity_scroll_changed(s,false);}
  return 0;
 }
 if(sample->began) {
  s->hit_ready=s->generation==s->completed_generation && s->generation!=0;
  s->hit_generation=s->completed_generation;s->hit_q8=s->completed_q8;
  s->hit_x=sample->x;s->hit_y=sample->y;
  portable_touch_scroll visible=s->motion;visible.position_q8=s->hit_q8;
  s->hit_row=portable_scroll_row(&visible,sample->x,sample->y);
 }
 unsigned result=portable_scroll_touch(&s->motion,sample,now);
 if(result&PORTABLE_SCROLL_CHANGED)s->dirty=true;
 if(result&PORTABLE_SCROLL_TAP) {
  portable_touch_scroll visible=s->motion;visible.position_q8=s->hit_q8;
  int row=portable_scroll_row(&visible,sample->x,sample->y);
  if(!s->hit_ready||s->hit_generation!=s->generation||row!=s->hit_row)result&=~PORTABLE_SCROLL_TAP;
 }
 return result;
}
static inline bool productivity_scroll_confirm(productivity_scroll *s,int selected) {
 productivity_scroll_visible(s);portable_touch_scroll visible=s->motion;visible.position_q8=s->completed_q8;
 int y=portable_scroll_row_y(&visible,selected);
 if(s->generation==s->completed_generation && selected==s->completed_selected &&
    y>=visible.view.y && y+visible.extent<=visible.view.y+visible.view.height)return true;
 portable_scroll_reveal(&s->motion,selected);s->dirty=true;return false;
}
static inline void productivity_scroll_bar(productivity_scroll *s,const t5_app_api_v1 *api) {
 if(!s->motion.limit)return;
 int height=s->motion.view.height,bar=height*height/(s->motion.count*s->motion.extent);
 if(bar<24)bar=24;
 int y=s->motion.view.y+(height-bar)*portable_scroll_offset(&s->motion)/s->motion.limit;
 api->fill_rect(s->motion.view.x+s->motion.view.width+8,y,4,bar,true);
}
