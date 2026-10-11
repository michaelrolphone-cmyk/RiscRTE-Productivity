#include "productivity_scroll.h"
static productivity_scroll tcs;
static struct {
 unsigned screen,key_page,format;int32_t week,day,week_start;bool editor,store,clock;
 tc_day_t rows[DAY_COUNT];char entry[13];
} tcs_state;
static void tcs_prepare(void) {
 __typeof__(tcs_state) state={0};state.screen=screen_id;state.week=week_offset;state.day=editing_ymd;
 state.week_start=sunday(week_offset);state.editor=tcp_editor;state.key_page=tcp_key_page;
 state.format=tcp_time_format;state.store=store_ready;state.clock=tcp_clock_valid;
 memcpy(state.entry,tcp_entry,sizeof(state.entry));
 if(screen_id==SCREEN_WEEK)for(unsigned i=0;i<DAY_COUNT;++i)state.rows[i]=get_day(add_days(state.week_start,(int)i));
 if(screen_id==SCREEN_DAY)state.rows[0]=get_day(editing_ymd);
 bool reset=!tcs.generation||state.screen!=tcs_state.screen||state.week!=tcs_state.week||state.day!=tcs_state.day||state.editor!=tcs_state.editor;
 if(!tcs.generation||memcmp(&state,&tcs_state,sizeof(state))){productivity_scroll_changed(&tcs,reset);tcs_state=state;}
 portable_scroll_configure(&tcs.motion,(portable_scroll_viewport){32,TCP_PAPER_TOP,416,440},TCP_PAPER_ROW,tcp_editor||!tcp_clock_valid||!store_ready?0:item_count());
 if(reset&&!tcp_editor)portable_scroll_reveal(&tcs.motion,selected);
 tcp_paper_first=(unsigned)portable_scroll_offset(&tcs.motion)/TCP_PAPER_ROW;
}
static unsigned tpp_rows(void){return (unsigned)tcs.motion.view.height/TCP_PAPER_ROW+2;}
static int tpp_row_y(unsigned row){return TCP_PAPER_TOP+(int)row*TCP_PAPER_ROW-portable_scroll_offset(&tcs.motion)%TCP_PAPER_ROW;}
static void tpp_clip(bool enabled) {
 portable_scroll_viewport view={tpp_x(32),tpp_y(TCP_PAPER_TOP),tpp_x(416),tpp_y(440)};
 portable_paper_scroll_clip(enabled?&view:NULL);
}
static void tcs_record(void){productivity_scroll_record(&tcs,selected);}
