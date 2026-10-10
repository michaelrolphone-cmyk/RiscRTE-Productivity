/* Paper is a presentation of the same Timecard controller, store and editor.
 * Shared PaperPresentation owns type, glyphs and physical monochrome output.
 * The app owns static pages; no animation or writes on navigation/redraw. */
#define TCP_PAPER_TOP 140
#define TCP_PAPER_ROW 88
#define TCP_PAPER_ROWS 5u
static unsigned tcp_paper_first;
static int tpp_x(int x){return x*app->screen_width()/480;}
static int tpp_y(int y){return y*app->screen_height()/800;}
static void tpp_rect(int x,int y,int w,int h,bool black){app->fill_rect(tpp_x(x),tpp_y(y),tpp_x(w),tpp_y(h),black);}
#ifdef PORTABLE_PRODUCTIVITY_SCROLL
#include "timecard_scroll_model.inc"
#else
static unsigned tpp_rows(void){return TCP_PAPER_ROWS;}
static int tpp_row_y(unsigned row){return TCP_PAPER_TOP+(int)row*TCP_PAPER_ROW;}
static void tpp_clip(bool enabled){(void)enabled;}
#endif
static void tpp_text(int x,int y,int w,const char *s,bool heading,bool black,bool literal){tcp_paper->text(tpp_x(x),tpp_y(y),tpp_x(w),s,(heading?2u:1u)|(literal?PAPER_TEXT_LITERAL:0u),heading,black);}
static void tpp_button(int x,int y,int w,int h,const char *label,const char *icon,bool chosen){
    tpp_rect(x,y,w,h,true);if(!chosen)tpp_rect(x+3,y+3,w-6,h-6,false);
    if(icon && app->draw_icon)(void)app->draw_icon(tpp_x(x+(w-24)/2),tpp_y(y+12),icon,24,!chosen);
    int n=tcp_paper->measure(label,false);
    tcp_paper->text(tpp_x(x)+(tpp_x(w)-n)/2,tpp_y(y+(icon?48:28)),tpp_x(w)-8,label,1,false,!chosen);
}
static void tpp_wrap(int y,const char *s,unsigned lines){
    for(unsigned row=0;row<lines && *s;row++){
        char line[97]={0};unsigned n=0;
        while(n<96 && s[n]){line[n]=s[n];line[n+1]=0;if(tcp_paper->measure(line,false)>tpp_x(416)){line[n]=0;break;}n++;}
        if(!n)break;
        tpp_text(32,y+(int)row*27,416,line,false,true,false);s+=n;
    }
}
static void tpp_header(const char *title,const char *subtitle){
    tcp_paper->begin();tpp_text(32,28,300,title,true,true,false);
    paper_battery_draw(app,tcp_paper,340,38,paper_battery_read(tcp_paper));
    tpp_text(32,88,416,subtitle,false,true,false);tpp_rect(32,126,416,3,true);
}
static void tpp_footer(const t5_ui_chrome_t *chrome,unsigned n){
    char range[48];unsigned last=tcp_paper_first+TCP_PAPER_ROWS;
#ifdef PORTABLE_PRODUCTIVITY_SCROLL
    last=(unsigned)(portable_scroll_offset(&tcs.motion)+tcs.motion.view.height+TCP_PAPER_ROW-1)/TCP_PAPER_ROW;
#endif
    if(last>n)last=n;
    snprintf(range,sizeof(range),"%u-%u OF %u",n?tcp_paper_first+1:0,last,n);tpp_text(32,590,416,range,false,true,false);
    tpp_wrap(628,chrome->status,2);
#ifdef PORTABLE_PRODUCTIVITY_SCROLL
    const char *labels[]={"BACK","UP",store_ready?chrome->confirm_label:"RETRY","DOWN"};
#else
    const char *labels[]={"BACK","PREV",store_ready?chrome->confirm_label:"RETRY","NEXT"};
#endif
#ifdef PORTABLE_PRODUCTIVITY_SCROLL
    const char *icons[]={"solid:f053",NULL,NULL,NULL};
#else
    const char *icons[]={"solid:f053","solid:f053",NULL,"solid:f054"};
#endif
    for(unsigned i=0;i<4;i++)tpp_button(32+(int)i*106,704,98,72,labels[i],icons[i],i==2);
    app->present(false);
#ifdef PORTABLE_PRODUCTIVITY_SCROLL
    tcs_record();
#endif
}
static void tpp_view(unsigned n,int32_t index){
#ifdef PORTABLE_PRODUCTIVITY_SCROLL
    (void)index;tcs_prepare();
#else
    if(tcp_draw_screen!=screen_id || tcp_draw_week!=week_offset || tcp_draw_day!=editing_ymd){
        tcp_paper_first=index>0?(unsigned)index/TCP_PAPER_ROWS*TCP_PAPER_ROWS:0;
        tcp_draw_screen=screen_id;tcp_draw_week=week_offset;tcp_draw_day=editing_ymd;tcp_reset_gesture();
    }
    if(index>=0 && ((unsigned)index<tcp_paper_first || (unsigned)index>=tcp_paper_first+TCP_PAPER_ROWS))tcp_paper_first=(unsigned)index/TCP_PAPER_ROWS*TCP_PAPER_ROWS;
 #endif
    tcp_render_rows=n;tcp_render_height=TCP_PAPER_ROW;
}
static void tcp_paper_list(const t5_ui_chrome_t *chrome,const t5_ui_list_row_t *rows,uint32_t n,int32_t index){
    tpp_view(n,index);tpp_header(chrome->title,chrome->subtitle);
    tpp_clip(true);
    for(unsigned r=0;r<tpp_rows() && tcp_paper_first+r<n;r++){
        unsigned i=tcp_paper_first+r;int y=tpp_row_y(r);bool chosen=(int32_t)i==index;
        if(chosen)tpp_rect(32,y,416,TCP_PAPER_ROW,true);
        tpp_text(48,y+12,368,rows[i].title,false,!chosen,false);
        char value[32];tcp_display_time(rows[i].value,value,sizeof(value));
        tpp_text(48,y+44,368,value[0]?value:rows[i].subtitle,false,!chosen,false);
        tpp_rect(32,y+TCP_PAPER_ROW-3,416,3,true);
    }
    tpp_clip(false);
#ifdef PORTABLE_PRODUCTIVITY_SCROLL
    if(tcs.motion.limit){int bar=440*440/(tcs.motion.count*TCP_PAPER_ROW);if(bar<24)bar=24;
        tpp_rect(456,TCP_PAPER_TOP+(440-bar)*portable_scroll_offset(&tcs.motion)/tcs.motion.limit,4,bar,true);}
#endif
    tpp_footer(chrome,n);
}
static void tcp_paper_table(const t5_ui_chrome_t *chrome,const t5_ui_table_column_t *columns,uint32_t count,const t5_ui_table_row_t *rows,uint32_t n,int32_t index){
    tpp_view(n,index);tpp_header(chrome->title,chrome->subtitle);
    tpp_clip(true);
    for(unsigned r=0;r<tpp_rows() && tcp_paper_first+r<n;r++){
        unsigned i=tcp_paper_first+r;int y=tpp_row_y(r);bool chosen=(int32_t)i==index;
        if(chosen)tpp_rect(32,y,416,TCP_PAPER_ROW,true);
        bool action=!!(rows[i].flags&T5_UI_TABLE_ROW_FULL_WIDTH);
        tpp_text(48,y+4,368,rows[i].cells[0],false,!chosen,false);
        if(action)tpp_text(48,y+42,368,"Use current local time",false,!chosen,false);
        else for(unsigned c=1;c<count && c<=4;c++){
            char value[32],line[48];tcp_display_time(rows[i].cells[c],value,sizeof(value));
            snprintf(line,sizeof(line),"%s %s",columns[c].title,value);
            tpp_text(48+(int)((c-1)%2)*198,y+29+(int)((c-1)/2)*25,194,line,false,!chosen,false);
        }
        tpp_rect(32,y+TCP_PAPER_ROW-3,416,3,true);
    }
    tpp_clip(false);
#ifdef PORTABLE_PRODUCTIVITY_SCROLL
    if(tcs.motion.limit){int bar=440*440/(tcs.motion.count*TCP_PAPER_ROW);if(bar<24)bar=24;
        tpp_rect(456,TCP_PAPER_TOP+(440-bar)*portable_scroll_offset(&tcs.motion)/tcs.motion.limit,4,bar,true);}
#endif
    tpp_footer(chrome,n);
}
/* All 95 ASCII characters remain reachable on five static 20-key pages.
 * Time validation/clear/save/cancel use the identical existing controller. */
static void tcp_paper_editor(void){
    tpp_header(tcp_editor_title,"DONE SAVES / CANCEL KEEPS ORIGINAL");
    tpp_text(32,144,416,tcp_entry,false,true,true);tpp_wrap(180,tcp_editor_status,2);
    for(unsigned k=0;k<20;k++){
        unsigned ch=32+tcp_key_page*20+k;if(ch>126)break;
        int x=32+(int)(k%5)*84,y=242+(int)(k/5)*82;char s[2]={(char)ch,0};
        tpp_button(x,y,80,78,"",NULL,false);tpp_text(x+28,y+25,48,ch==32?"_":s,false,true,true);
    }
    char page[32];snprintf(page,sizeof(page),"KEYS %u/5",tcp_key_page+1);
    tpp_button(32,590,200,82,page,NULL,false);tpp_button(248,590,200,82,"DELETE",NULL,false);
    tpp_button(32,704,200,72,"CANCEL",NULL,false);tpp_button(248,704,200,72,"DONE",NULL,true);app->present(false);
}
static void tcp_paper_unavailable(void){
    tpp_header("TIME CARD","");
    tpp_wrap(160,tcp_files?"Clock unavailable. Retry when the clock is ready.":"Writable history storage is unavailable. No history is created or changed.",5);
    tpp_button(32,704,200,72,"BACK",NULL,false);
    if(tcp_files)tpp_button(248,704,200,72,"RETRY",NULL,true);
    app->present(false);
}
static void tpp_page(int direction){
#ifdef PORTABLE_PRODUCTIVITY_SCROLL
    tcp_move(direction);tcs_prepare();portable_scroll_reveal(&tcs.motion,selected);tcs.dirty=true;
#else
    unsigned total=(unsigned)item_count();
    unsigned pages=(total+TCP_PAPER_ROWS-1)/TCP_PAPER_ROWS;
    if(!pages)return;
    unsigned page=tcp_paper_first/TCP_PAPER_ROWS;
    page=direction>0?(page+1)%pages:(page+pages-1)%pages;
    tcp_paper_first=page*TCP_PAPER_ROWS;selected=(int32_t)tcp_paper_first;tcp_dirty=true;
#endif
}
static void tpp_tap(int x,int y){
    if(tcp_editor){
        if(y>=704 && y<776){if(x>=32 && x<232)tcp_editor_cancel();else if(x>=248 && x<448)tcp_editor_done();}
        else if(y>=590 && y<672){if(x>=32 && x<232)tcp_key_page=(tcp_key_page+1)%5;else if(x>=248 && x<448){size_t n=slen(tcp_entry);if(n)tcp_entry[n-1]=0;}}
        else if(x>=32 && x<448 && y>=242 && y<570){
            unsigned col=(unsigned)(x-32)/84,row=(unsigned)(y-242)/82;
            if((x-32)%84<80 && (y-242)%82<78 && col<5 && row<4){
                unsigned ch=32+tcp_key_page*20+row*5+col;size_t n=slen(tcp_entry);
                if(ch<=126 && n<12){tcp_entry[n]=(char)ch;tcp_entry[n+1]=0;}
                else if(ch<=126)scopy(tcp_editor_status,sizeof(tcp_editor_status),"Time entry is limited to 12 characters");
            }
        }
        tcp_dirty=true;return;
    }
    if(y>=704 && y<776){
        if(!tcp_files || !tcp_clock_valid){if(x>=32 && x<232)tcp_back();else if(x>=248 && x<448)tcp_dirty=true;return;}
        for(unsigned i=0;i<4;i++)if(x>=32+(int)i*106 && x<130+(int)i*106){
            if(i==0)tcp_back();else if(i==1)tpp_page(-1);else if(i==2){
#ifdef PORTABLE_PRODUCTIVITY_SCROLL
                if(!store_ready||productivity_scroll_confirm(&tcs,selected))tcp_activate();else tcp_dirty=true;
#else
                tcp_activate();
#endif
            }else tpp_page(1);return;
        }
    }
    if(tcp_files && tcp_clock_valid && x>=32 && x<448 && y>=TCP_PAPER_TOP && y<TCP_PAPER_TOP+5*TCP_PAPER_ROW){
        unsigned i=tcp_paper_first+(unsigned)(y-TCP_PAPER_TOP)/TCP_PAPER_ROW;
        if(i<tcp_render_rows){selected=(int32_t)i;tcp_activate();}
    }
}
#ifndef PORTABLE_PRODUCTIVITY_SCROLL
static void tcp_paper_input(const t5_app_input_t *input){
    if(input->exit_requested && !(input->buttons&T5_APP_BUTTON_BACK)){tcp_external_exit=true;return;}
    if(input->buttons&T5_APP_BUTTON_BACK){tcp_reset_gesture();tcp_back();return;}
    /* Adapter delivers only complete eligible taps. Lost contact, movement and
     * QuickActions captures never synthesize a key or a save. */
    springboard_contact contact={0};tcp_paper->contact(&contact);
    if(contact.valid && !contact.cancelled && contact.released && contact.tap_eligible)
        tpp_tap(contact.x*480/app->screen_width(),contact.y*800/app->screen_height());
    if(input->buttons&(T5_APP_BUTTON_UP|T5_APP_BUTTON_LEFT)){
        if(tcp_editor){tcp_key_page=(tcp_key_page+4)%5;tcp_dirty=true;}else tcp_move(-1);
    }else if(input->buttons&(T5_APP_BUTTON_DOWN|T5_APP_BUTTON_RIGHT)){
        if(tcp_editor){tcp_key_page=(tcp_key_page+1)%5;tcp_dirty=true;}else tcp_move(1);
    }else if(input->buttons&T5_APP_BUTTON_CONFIRM){if(tcp_editor)tcp_editor_done();else tcp_activate();tcp_dirty=true;}
}

#else
#include "timecard_scroll_input.inc"
#endif
