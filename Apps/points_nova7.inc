#ifdef PORTABLE_NOVA_UI
/* NOVA-7 Points presentation follows the supplied 240x240 mockup literally:
 * left-aligned Orbitron/Rajdhani hierarchy, 40px single-line rows, per-type
 * left rails, 1px row rules, inline day controls and the shared Nova keyboard. */
#include "PortableNovaUi.h"
#include "PortableNovaKeyboard.h"

#define P7_ROW_TOP 82
#define P7_ROW_H 40
#define P7_VIEW_BOTTOM 202
#define P7_BACK_Y 204
#define P7_EDIT_STEP 36
#define P7_DARK 0x001418u
#define P7_KEY_BG 0x14191cu

static void p7_header(const char *title,const char *sub,uint32_t dot) {
    if(dot)portable_nova_round(20,52,8,8,4,dot);
    portable_nova_text(0,dot?35:20,49,dot?185:200,title,NOVA_CYAN);
    if(sub&&*sub)portable_nova_text(3,20,68,200,sub,NOVA_CAP);
    portable_nova_fill(20,80,200,1,NOVA_DIM);
}
static void p7_back(void){portable_nova_button(82,P7_BACK_Y,76,30,"< BACK",false);}
static void p7_divider(int y){portable_nova_fill(20,y,200,1,NOVA_LINE);}
static void p7_rail(int y,uint32_t color){portable_nova_fill(20,y,2,P7_ROW_H,color);}
static void p7_chevron(int y){portable_nova_right(0,202,y+9,18,">",NOVA_DIM);}
static void p7_toggle(int x,int y,bool on) {
    portable_nova_round(x,y,48,20,10,NOVA_DIM);
    portable_nova_round(x+2,y+2,44,16,8,0x000000u);
    if(on) {
        portable_nova_round(x+24,y+2,22,16,7,NOVA_CYAN);
        portable_nova_center(NOVA_FACE_ORBITRON_10,x+25,y+3,20,"ON",P7_DARK);
        portable_nova_center(NOVA_FACE_ORBITRON_10,x+3,y+3,20,"OFF",0x4a666bu);
    } else {
        portable_nova_round(x+2,y+2,22,16,7,0x1c2a2eu);
        portable_nova_center(NOVA_FACE_ORBITRON_10,x+3,y+3,20,"OFF",NOVA_TEXT);
        portable_nova_center(NOVA_FACE_ORBITRON_10,x+25,y+3,20,"ON",0x4a666bu);
    }
}
static void p7_checkbox(int x,int y,bool on,bool enabled) {
    uint32_t edge=enabled?NOVA_DIM:NOVA_LINE;
    portable_nova_round(x,y,16,16,5,on&&enabled?NOVA_CYAN:edge);
    if(!on||!enabled)portable_nova_round(x+1,y+1,14,14,4,0x000000u);
    if(on&&enabled) {
        portable_nova_fill(x+4,y+8,2,4,P7_DARK);
        portable_nova_fill(x+6,y+10,2,2,P7_DARK);
        portable_nova_fill(x+8,y+7,2,5,P7_DARK);
        portable_nova_fill(x+10,y+5,2,4,P7_DARK);
    }
}
static void p7_outline_pill(int x,int y,int w,int h,const char *text) {
    portable_nova_round(x,y,w,h,h/2,NOVA_DIM);
    portable_nova_round(x+1,y+1,w-2,h-2,(h-2)/2,0x000000u);
    portable_nova_center(NOVA_FACE_ORBITRON_10,x+3,y+3,w-6,text,NOVA_CYAN);
}
static void p7_list(void) {
    unsigned order[POINTS_MAX],count=nova_configured(order),active=nova_active_count();
    char sub[32];snprintf(sub,sizeof(sub),"%u ACTIVE  %u TOTAL",active,count);
    portable_nova_begin();p7_header("POINTS IN TIME",sub,0);
    unsigned rows=count+(count<POINTS_MAX?1u:0u),max=rows>4?rows-4:0;
    if(nova_list_scroll>max)nova_list_scroll=max;
    for(unsigned r=0;r<4;r++) {
        unsigned index=nova_list_scroll+r;if(index>=rows)break;int y=P7_ROW_TOP+(int)r*P7_ROW_H;
        if(index==count) {
            p7_rail(y,NOVA_DIM);p7_divider(y+P7_ROW_H-1);
            portable_nova_text(1,32,y+10,180,"+ ADD POINT",NOVA_CYAN);continue;
        }
        const points_item *p=&writer.saved.points[order[index]];
        char tm[20],days_text[16],dur[16]={0};format_time(p,tm,sizeof(tm));nova_short_days(p->weekdays,days_text);
        if(p->duration_minutes)nova_duration(dur,p->duration_minutes);
        uint32_t rail=p->enabled?nova_kind_color(p->kind):NOVA_LINE;
        uint32_t time_col=p->enabled?NOVA_CYAN:NOVA_DIM,name_col=p->enabled?NOVA_WHITE:NOVA_CAP,meta=p->enabled?NOVA_CAP:NOVA_DIM;
        p7_rail(y,rail);p7_divider(y+P7_ROW_H-1);
        portable_nova_text(NOVA_FACE_ORBITRON_10,32,y+10,50,tm,time_col);
        portable_nova_text(1,84,y+8,68,nova_kind_name(p->kind),name_col);
        if(dur[0])portable_nova_text(3,151,y+11,31,dur,meta);
        portable_nova_right(3,179,y+11,41,days_text,meta);
    }
}
static int p7_edit_offset(void){return (int)nova_edit_scroll*P7_EDIT_STEP;}
static int p7_edit_y(int content_y){return content_y-p7_edit_offset();}
static void p7_edit_row(int cy,const char *label_text,const char *value,uint32_t value_color,bool chevron) {
    int y=p7_edit_y(cy);if(y>=P7_VIEW_BOTTOM||y+P7_ROW_H<=81)return;
    p7_rail(y,NOVA_LINE);p7_divider(y+P7_ROW_H-1);
    portable_nova_text(1,32,y+9,108,label_text,NOVA_WHITE);
    if(value&&*value)portable_nova_right(2,137,y+10,65,value,value_color);
    if(chevron)p7_chevron(y);
}
static void p7_edit_check(int cy,const char *label_text,bool on,bool enabled) {
    int y=p7_edit_y(cy);if(y>=P7_VIEW_BOTTOM||y+P7_ROW_H<=81)return;
    p7_rail(y,NOVA_LINE);p7_divider(y+P7_ROW_H-1);
    portable_nova_text(1,32,y+9,150,label_text,enabled?NOVA_WHITE:NOVA_CAP);
    p7_checkbox(198,y+12,on,enabled);
}
static void p7_days(int cy) {
    int y=p7_edit_y(cy);if(y>=P7_VIEW_BOTTOM||y+72<=81)return;
    if(y>=81&&y<P7_VIEW_BOTTOM)portable_nova_text(3,30,y+2,100,"DAYS",NOVA_CAP);
    static const char letters[]="SMTWTFS";
    for(unsigned i=0;i<7;i++) {
        int by=y+17;if(by+24<=81||by>=P7_VIEW_BOTTOM)continue;int x=24+(int)i*28;bool on=!!(draft.weekdays&(1u<<i));
        portable_nova_round(x,by,24,24,8,on?NOVA_CYAN:P7_KEY_BG);
        char s[2]={letters[i],0};portable_nova_center(NOVA_FACE_ORBITRON_10,x,by+5,24,s,on?P7_DARK:NOVA_CAP);
    }
    int py=y+48;const char *presets[]={"EVERY DAY","MON-FRI","WEEKEND"};int xs[]={24,88,152};
    for(unsigned i=0;i<3;i++)if(py+18>81&&py<P7_VIEW_BOTTOM)p7_outline_pill(xs[i],py,60,18,presets[i]);
    if(y+70>81&&y+70<P7_VIEW_BOTTOM)p7_divider(y+70);
}
static void p7_edit(void) {
    portable_nova_begin();p7_header("EDIT POINT",nova_kind_name(draft.kind),nova_kind_color(draft.kind));
    char value[32];bool timed=duration_kind(draft.kind)&&draft.duration_minutes;
    int cy=82;
    p7_edit_row(cy,"TYPE",nova_kind_name(draft.kind),NOVA_CYAN,true);cy+=40;
    format_time(&draft,value,sizeof(value));p7_edit_row(cy,"TIME",value,NOVA_CYAN,true);cy+=40;
    nova_duration(value,draft.duration_minutes);p7_edit_row(cy,"DURATION",duration_kind(draft.kind)?value:"NOT USED",duration_kind(draft.kind)?NOVA_CYAN:NOVA_CAP,true);cy+=40;
    p7_edit_check(cy,"NOTIFY AT END",draft.notify_end,timed);cy+=40;
    p7_edit_check(cy,"3 MIN WARNING",draft.warn3,timed&&draft.duration_minutes>=3);cy+=40;
    p7_edit_row(cy,"NOTIFY",nova_modes[draft.mode],NOVA_CYAN,true);cy+=40;
    p7_days(cy);cy+=72;
    int y=p7_edit_y(cy);
    if(y<P7_VIEW_BOTTOM&&y+40>81){p7_rail(y,NOVA_LINE);p7_divider(y+39);portable_nova_text(1,32,y+9,120,"ENABLED",NOVA_WHITE);p7_toggle(164,y+10,draft.enabled);}cy+=40;
    y=p7_edit_y(cy);
    if(y<P7_VIEW_BOTTOM&&y+40>81){p7_rail(y,NOVA_LINE);p7_divider(y+39);portable_nova_text(1,32,y+9,170,nova_delete_confirm?"TAP AGAIN TO DELETE":"DELETE POINT",NOVA_RED);}
    p7_back();
}
static void p7_simple_header(const char *title,const char *sub){portable_nova_begin();p7_header(title,sub,0);}
static unsigned p7_type_count(unsigned choice[8]){return nova_type_choices(choice);}
static void p7_type(void) {
    p7_simple_header("POINT TYPE","CHOOSE OR CREATE");
    unsigned choice[8],count=p7_type_count(choice),max=count>3?count-3:0;if(nova_type_scroll>max)nova_type_scroll=max;
    for(unsigned r=0;r<3;r++) {
        unsigned at=nova_type_scroll+r;if(at>=count)break;unsigned kind=choice[at];int y=82+(int)r*40;
        uint32_t col=kind?nova_kind_color(kind):NOVA_CYAN;p7_rail(y,col);p7_divider(y+39);
        if(!kind){portable_nova_text(1,32,y+9,180,"+ CUSTOM TYPE",NOVA_CYAN);continue;}
        portable_nova_text(1,32,y+9,145,nova_kind_name(kind),kind==draft.kind?col:NOVA_WHITE);
        if(custom_index(kind)<POINTS_CUSTOM_COUNT)portable_nova_right(3,178,y+11,42,"EDIT",NOVA_CAP);
        else if(kind==draft.kind)portable_nova_right(3,184,y+11,36,"ON",NOVA_CYAN);
    }
    p7_back();
}
static void p7_mode(void) {
    p7_simple_header("NOTIFY","HOW TO ALERT YOU");
    for(unsigned i=0;i<3;i++){unsigned at=nova_type_scroll+i;if(at>=4)break;int y=82+(int)i*40;p7_rail(y,at==draft.mode?NOVA_CYAN:NOVA_LINE);p7_divider(y+39);
        portable_nova_text(1,32,y+9,165,nova_modes[at],at==draft.mode?NOVA_CYAN:NOVA_WHITE);
        if(at==draft.mode)p7_checkbox(198,y+12,true,true);}
    p7_back();
}
static void p7_time(void) {
    p7_simple_header("TIME","DRAG OR TAP A COLUMN");
    for(int j=-2;j<=2;j++) {
        int h=(int)draft.hour+j;while(h<0)h+=24;h%=24;int m=(int)draft.minute+j*5;while(m<0)m+=60;m%=60;
        char left[4],right[4];snprintf(left,sizeof(left),"%02d",h);snprintf(right,sizeof(right),"%02d",m);
        int y=59+(j+2)*28;uint32_t col=j?NOVA_CAP:NOVA_CYAN;
        portable_nova_right(j?2:0,48,y,54,left,col);portable_nova_center(j?2:0,110,y,20,":",col);portable_nova_text(j?2:0,139,y,54,right,col);
        if(!j){portable_nova_fill(34,y-4,172,1,NOVA_DIM);portable_nova_fill(34,y+28,172,1,NOVA_DIM);}
    }
    p7_back();
}
static void p7_duration(void) {
    p7_simple_header("DURATION","END OF THIS POINT");
    unsigned cur=draft.duration_minutes;
    for(int j=-2;j<=2;j++) {
        unsigned v=cur;if(j<0)for(int n=0;n<-j;n++)v=nova_duration_next(v,-1);else for(int n=0;n<j;n++)v=nova_duration_next(v,1);
        char s[20];nova_duration(s,v);int y=59+(j+2)*28;uint32_t col=j?NOVA_CAP:NOVA_CYAN;
        portable_nova_center(j?2:0,36,y,168,s,col);if(!j){portable_nova_fill(34,y-4,172,1,NOVA_DIM);portable_nova_fill(34,y+28,172,1,NOVA_DIM);}
    }
    p7_back();
}
static void p7_custom(void) {
    unsigned ci=custom_index(custom_kind);if(ci>=POINTS_CUSTOM_COUNT)return;
    p7_simple_header("CUSTOM TYPE","NAME AND COLOR");
    portable_nova_round(20,91,200,38,12,NOVA_DIM);portable_nova_round(21,92,198,36,11,0);
    portable_nova_text(0,32,101,176,custom_draft.custom[ci].name[0]?custom_draft.custom[ci].name:"TAP TO TYPE",custom_draft.custom[ci].name[0]?NOVA_WHITE:NOVA_CAP);
    portable_nova_text(3,30,139,90,"COLOR",NOVA_CAP);
    for(unsigned i=0;i<8;i++){int x=43+(int)(i%4)*42,y=153+(int)(i/4)*35;portable_nova_round(x,y,24,24,12,nova_custom_colors[i]);
        if(custom_draft.custom[ci].color==i){portable_nova_round(x-3,y-3,30,30,15,NOVA_WHITE);portable_nova_round(x-1,y-1,26,26,13,0);portable_nova_round(x,y,24,24,12,nova_custom_colors[i]);}}
    portable_nova_button(24,207,82,28,"< BACK",false);portable_nova_button(134,207,82,28,"SAVE",false);
}
static void p7_keyboard(void) {
    unsigned ci=custom_index(custom_kind);if(ci>=POINTS_CUSTOM_COUNT)return;
    portable_nova_begin();portable_nova_text(0,18,18,204,"CUSTOM TYPE",NOVA_CYAN);
    portable_nova_text(0,18,52,204,custom_draft.custom[ci].name,NOVA_WHITE);
    char status[32];snprintf(status,sizeof(status),"CHARACTERS %u / 12",custom_key_page+1);
    portable_nova_center(3,18,69,204,status,NOVA_CAP);
    for(unsigned i=0;i<8;i++){unsigned ch=portable_nova_key_character(custom_key_page,i);if(!ch)continue;char key[]={(char)ch,0};
        portable_nova_button(16+(int)(i%4)*53,84+(int)(i/4)*48,49,44,ch==32?"SPACE":key,custom_key_choice==i);}
    const char *labels[]={"PREV","NEXT","DEL","DONE"};
    for(unsigned i=0;i<4;i++)portable_nova_button(16+(int)i*53,188,49,44,labels[i],custom_key_choice==8+i);
}
static void p7_problem(const char *title,const char *detail) {
    portable_nova_begin();p7_header("POINTS IN TIME",NULL,0);portable_nova_center(0,20,103,200,title,NOVA_RED);portable_nova_center(2,20,126,200,detail,NOVA_CAP);
    portable_nova_button(60,174,120,44,"RETRY",false);
}
static void draw_nova7(void) {
    if(!ready)p7_problem("DEPENDENCIES UNAVAILABLE","SERVICE / RTC / STORAGE");
#ifndef PORTABLE_ALARM_CLIENT
    else if(service_valid&&service_state.occurrence.generation)p7_problem(service_state.label,"TAP TO DISMISS");
#endif
    else if(writer.uncertain||writer.meta_uncertain)p7_problem("SAVE UNCONFIRMED","RETRY WRITES SAME DATA");
    else if(!writer.loaded)p7_problem("STORAGE INVALID","NO DEFAULTS SAVED");
    else if(page==PAGE_LIST)p7_list();
    else if(page==PAGE_EDIT)p7_edit();
    else if(page==PAGE_TYPE)p7_type();
    else if(page==PAGE_TIME)p7_time();
    else if(page==PAGE_MODE)p7_mode();
    else if(page==PAGE_DURATION)p7_duration();
    else if(page==PAGE_CUSTOM)p7_custom();
    else if(page==PAGE_CUSTOM_KEYBOARD)p7_keyboard();
    else p7_edit();
    app->present(false);
}
static void p7_new_point(void) {
    unsigned slot=nova_first_empty();if(slot>=POINTS_MAX){notice="All 8 point slots are in use";return;}
    twatch_rtc_time_v1 t={0};unsigned h=8,m=0;
    if(rtc&&rtc->read(rtc->context,&t)){h=t.hour;m=((unsigned)t.minute+4u)/5u*5u;if(m>=60){m=0;h=(h+1)%24;}}
    selected=slot;draft=(points_item){.kind=POINTS_BREAK,.enabled=1,.mode=1,.weekdays=62,.hour=(uint8_t)h,.minute=(uint8_t)m,.duration_minutes=15};
    page=PAGE_EDIT;nova_edit_scroll=0;nova_delete_confirm=false;notice="Draft only - not saved";
}
static void p7_custom_open(unsigned kind,bool fresh) {
    custom_kind=kind;custom_draft=writer.meta;unsigned ci=custom_index(kind);if(ci>=POINTS_CUSTOM_COUNT)return;
    if(fresh){memset(custom_draft.custom[ci].name,0,sizeof(custom_draft.custom[ci].name));custom_draft.custom[ci].color=6;}
    custom_key_page=custom_key_choice=0;page=PAGE_CUSTOM;
}
static void p7_custom_save(void) {
    unsigned ci=custom_index(custom_kind);if(ci>=POINTS_CUSTOM_COUNT)return;custom_trim(custom_draft.custom[ci].name);
    if(!custom_draft.custom[ci].name[0]){notice="Custom name cannot be empty";return;}
    custom_draft.revision=writer.meta.revision+1;int32_t rc=points_writer_save_meta(&writer,storage,&custom_draft);
    if(rc!=ALARM_OK){notice="Custom type save failed";return;}draft.kind=(uint8_t)custom_kind;page=PAGE_EDIT;notice="Custom type saved";
}
static void p7_key_activate(unsigned key) {
    unsigned ci=custom_index(custom_kind);if(ci>=POINTS_CUSTOM_COUNT)return;char *name=custom_draft.custom[ci].name;size_t n=strlen(name);
    if(key<8){unsigned ch=portable_nova_key_character(custom_key_page,key);if(ch&&n<POINTS_CUSTOM_NAME_MAX){if(ch>='a'&&ch<='z')ch-=32;name[n]=(char)ch;name[n+1]=0;}}
    else if(key==8){custom_key_page=(custom_key_page+PORTABLE_NOVA_KEY_PAGES-1)%PORTABLE_NOVA_KEY_PAGES;custom_key_choice=0;}
    else if(key==9){custom_key_page=(custom_key_page+1)%PORTABLE_NOVA_KEY_PAGES;custom_key_choice=0;}
    else if(key==10){if(n)name[n-1]=0;}
    else if(key==11){page=PAGE_CUSTOM;custom_key_choice=0;}
}
static bool p7_edit_content_hit(int x,int y) {
    if(y<81||y>=P7_VIEW_BOTTOM)return false;int cy=y+p7_edit_offset(),row=82;
    bool timed=duration_kind(draft.kind)&&draft.duration_minutes;
    if(cy>=row&&cy<row+40){page=PAGE_TYPE;nova_type_scroll=0;return true;}row+=40;
    if(cy>=row&&cy<row+40){page=PAGE_TIME;return true;}row+=40;
    if(cy>=row&&cy<row+40){if(duration_kind(draft.kind))page=PAGE_DURATION;else notice="Duration not used for this type";return true;}row+=40;
    if(cy>=row&&cy<row+40){if(timed)draft.notify_end^=1;else notice="Set a duration first";return true;}row+=40;
    if(cy>=row&&cy<row+40){if(timed&&draft.duration_minutes>=3)draft.warn3^=1;else notice="Set duration to at least 3 min";return true;}row+=40;
    if(cy>=row&&cy<row+40){page=PAGE_MODE;nova_type_scroll=draft.mode>2?1:0;return true;}row+=40;
    if(cy>=row&&cy<row+72){
        int dy=cy-row;if(dy>=17&&dy<41&&x>=24&&x<220){unsigned d=(unsigned)(x-24)/28u;if(d<7)draft.weekdays^=(uint8_t)(1u<<d);}
        else if(dy>=48&&dy<68){if(x>=24&&x<84)draft.weekdays=127;else if(x>=88&&x<148)draft.weekdays=62;else if(x>=152&&x<212)draft.weekdays=65;}
        return true;
    }row+=72;
    if(cy>=row&&cy<row+40){draft.enabled^=1;nova_delete_confirm=false;return true;}row+=40;
    if(cy>=row&&cy<row+40){if(!nova_delete_confirm){nova_delete_confirm=true;notice="Tap delete again to confirm";}else{draft=(points_item){0};save_action();nova_delete_confirm=false;nova_edit_scroll=0;}return true;}
    return false;
}
static void nova7_tap(int x,int y) {
    if(!ready){if(y>=150)retry_action();return;}
#ifndef PORTABLE_ALARM_CLIENT
    if(service_valid&&service_state.occurrence.generation){if(y>=150)(void)service->acknowledge(service->context,&service_state.occurrence);return;}
#endif
    if(writer.uncertain||writer.meta_uncertain||!writer.loaded){if(y>=150)retry_action();return;}
    if(page==PAGE_CUSTOM_KEYBOARD) {
        for(unsigned i=0;i<8;i++)if(portable_nova_hit(x,y,16+(int)(i%4)*53,84+(int)(i/4)*48,49,44)){custom_key_choice=i;p7_key_activate(i);return;}
        for(unsigned i=0;i<4;i++)if(portable_nova_hit(x,y,16+(int)i*53,188,49,44)){custom_key_choice=8+i;p7_key_activate(8+i);return;}
        return;
    }
    if(page!=PAGE_LIST)notice="Draft only - not saved";
    if(page==PAGE_LIST) {
        if(y<P7_ROW_TOP)return;unsigned order[POINTS_MAX],count=nova_configured(order),index=nova_list_scroll+(unsigned)(y-P7_ROW_TOP)/P7_ROW_H;
        if(index<count){edit_slot(order[index]);nova_edit_scroll=0;nova_delete_confirm=false;}else if(index==count&&count<POINTS_MAX)p7_new_point();
    } else if(page==PAGE_EDIT) {
        if(portable_nova_hit(x,y,72,196,96,44)){save_action();return;}p7_edit_content_hit(x,y);
    } else if(page==PAGE_TYPE) {
        if(portable_nova_hit(x,y,72,196,96,44)){page=PAGE_EDIT;return;}
        if(y>=82&&y<202){unsigned choices[8],count=p7_type_count(choices),at=nova_type_scroll+(unsigned)(y-82)/40u;if(at<count){unsigned kind=choices[at];
            if(!kind){for(unsigned i=0;i<POINTS_CUSTOM_COUNT;i++)if(!writer.meta.custom[i].name[0]){p7_custom_open(POINTS_CUSTOM_1+i,true);return;}}
            else if(custom_index(kind)<POINTS_CUSTOM_COUNT&&x>=170)p7_custom_open(kind,false);
            else {draft.kind=(uint8_t)kind;if(!duration_kind(kind)){draft.duration_minutes=0;draft.notify_end=draft.warn3=0;}page=PAGE_EDIT;}}}
    } else if(page==PAGE_MODE) {
        if(portable_nova_hit(x,y,72,196,96,44)){page=PAGE_EDIT;return;}if(y>=82&&y<202){unsigned at=nova_type_scroll+(unsigned)(y-82)/40u;if(at<4){draft.mode=(uint8_t)at;page=PAGE_EDIT;}}
    } else if(page==PAGE_TIME) {
        if(portable_nova_hit(x,y,72,196,96,44)){page=PAGE_EDIT;return;}if(y>=55&&y<195){int row=(y-55)/28,j=row-2;if(j){if(x<120){int h=(int)draft.hour+j;while(h<0)h+=24;draft.hour=(uint8_t)(h%24);}else{int m=(int)draft.minute+j*5;while(m<0)m+=60;draft.minute=(uint8_t)(m%60);}}}
    } else if(page==PAGE_DURATION) {
        if(portable_nova_hit(x,y,72,196,96,44)){page=PAGE_EDIT;return;}if(y>=55&&y<195){int row=(y-55)/28,j=row-2;if(j)nova_duration_move(j);if(!draft.duration_minutes)draft.notify_end=draft.warn3=0;}
    } else if(page==PAGE_CUSTOM) {
        unsigned ci=custom_index(custom_kind);if(ci>=POINTS_CUSTOM_COUNT)return;
        if(portable_nova_hit(x,y,20,88,200,46)){page=PAGE_CUSTOM_KEYBOARD;custom_key_choice=0;return;}
        for(unsigned i=0;i<8;i++){int bx=40+(int)(i%4)*42,by=150+(int)(i/4)*35;if(portable_nova_hit(x,y,bx,by,30,30)){custom_draft.custom[ci].color=(uint8_t)i;return;}}
        if(portable_nova_hit(x,y,16,198,104,42)){page=PAGE_TYPE;return;}if(portable_nova_hit(x,y,122,198,110,42))p7_custom_save();
    }
}
static bool nova7_scroll_step(int direction) {
    if(page==PAGE_LIST){unsigned order[POINTS_MAX],count=nova_configured(order),rows=count+(count<POINTS_MAX?1u:0u),max=rows>4?rows-4:0,old=nova_list_scroll;
        if(direction>0&&nova_list_scroll<max)nova_list_scroll++;else if(direction<0&&nova_list_scroll)nova_list_scroll--;return old!=nova_list_scroll;}
    if(page==PAGE_EDIT){unsigned max=9,old=nova_edit_scroll;if(direction>0&&nova_edit_scroll<max)nova_edit_scroll++;else if(direction<0&&nova_edit_scroll)nova_edit_scroll--;return old!=nova_edit_scroll;}
    if(page==PAGE_TYPE||page==PAGE_MODE){unsigned max=page==PAGE_MODE?1:4,old=nova_type_scroll;if(direction>0&&nova_type_scroll<max)nova_type_scroll++;else if(direction<0&&nova_type_scroll)nova_type_scroll--;return old!=nova_type_scroll;}
    if(page==PAGE_TIME){if(nova_drag_x<120)draft.hour=(uint8_t)((draft.hour+(direction>0?1:23))%24);else{int m=(int)draft.minute+(direction>0?5:-5);while(m<0)m+=60;draft.minute=(uint8_t)(m%60);}return true;}
    if(page==PAGE_DURATION){nova_duration_move(direction);return true;}return false;
}
static bool nova7_buttons(uint32_t buttons) {
    if(page==PAGE_CUSTOM_KEYBOARD) {
        if(buttons&T5_APP_BUTTON_LEFT)custom_key_choice=custom_key_choice?custom_key_choice-1:11;
        else if(buttons&T5_APP_BUTTON_RIGHT)custom_key_choice=(custom_key_choice+1)%12;
        else if(buttons&T5_APP_BUTTON_UP)custom_key_choice=(custom_key_choice+8)%12;
        else if(buttons&T5_APP_BUTTON_DOWN)custom_key_choice=(custom_key_choice+4)%12;
        else if(buttons&T5_APP_BUTTON_CONFIRM)p7_key_activate(custom_key_choice);else return false;return true;
    }
    if(buttons&T5_APP_BUTTON_UP){nova_drag_x=60;return nova7_scroll_step(-1);}
    if(buttons&T5_APP_BUTTON_DOWN){nova_drag_x=60;return nova7_scroll_step(1);}
    if(page==PAGE_TIME&&buttons&T5_APP_BUTTON_LEFT){nova_drag_x=60;return nova7_scroll_step(-1);}
    if(page==PAGE_TIME&&buttons&T5_APP_BUTTON_RIGHT){nova_drag_x=180;return nova7_scroll_step(1);}
    return false;
}
#endif
