#ifdef PORTABLE_NATIVE_SCENE
#include "native_scroll_labels.h"
#else
/* Frozen source composition has no new shared-header dependency. */
#define PRODUCTIVITY_NATIVE_SCROLL_UP_LABEL "^"
#define PRODUCTIVITY_NATIVE_SCROLL_DOWN_LABEL "v"
#endif
/* Capability-sized Timecard presentation. The authoritative civil-time ledger,
 * guarded storage mutations and shared keyboard character pages stay unchanged. */
#include "PortableTimecardNative.h"
#if !defined(PORTABLE_NOVA_UI) || (!defined(PORTABLE_NATIVE_CANVAS) && !(defined(TIMECARD_CURRENT_PROFILE) && defined(PORTABLE_RF_NATIVE_CANVAS))) || !defined(PORTABLE_APP_OWNS_TOUCH_CHROME)
#error Native Timecard requires NOVA, native canvas and app-owned touch chrome
#endif
extern bool portable_paper_frame_ready(void);
typedef struct {int x,y,w,h;} tcn_rect;
typedef struct {tcn_rect r;unsigned action,arg;} tcn_control;
enum {TCN_BACK=1,TCN_SELECT,TCN_ROW,TCN_UP,TCN_DOWN,TCN_KEY};
static struct {
 int w,h,margin,header,footer,top,bottom,row,pressed,start_x,start_y,last_y;
 unsigned count,total,first,map_screen;int32_t map_week,map_day;
 uint64_t generation;bool active,down,moved,map_editor;
 tcn_control controls[40];
} tcn;
static bool tcn_inside(tcn_rect r,int x,int y){return x>=r.x&&y>=r.y&&x-r.x<r.w&&y-r.y<r.h;}
/* Ellipsis is measured in the real native face. No completed viewport/glyph is
 * stretched, and every label stays inside its own control. */
static void tcn_label(tcn_rect r,const char *s,uint32_t color,unsigned face){
 int height=face==0?20:face==2?16:14;
 if(height>r.h||portable_nova_measure(face,s)>r.w-4){face=3;height=14;}
 char text[128];size_t n=strlen(s);if(n>sizeof(text)-4)n=sizeof(text)-4;
 memcpy(text,s,n);text[n]=0;
 while(n&&portable_nova_measure(face,text)>r.w-4){--n;memcpy(text+n,"...",4);}
 int width=portable_nova_measure(face,text);if(width>r.w-4||height>r.h)return;
 portable_nova_native_text(face,r.x+(r.w-width)/2,r.y+(r.h-height)/2,r.w,text,color);
}
static void tcn_add(tcn_rect r,unsigned action,unsigned arg){
 if(tcn.count<40&&r.w>0&&r.h>0)tcn.controls[tcn.count++]=(tcn_control){r,action,arg};
}
static void tcn_button(tcn_rect r,unsigned action,unsigned arg,const char *label,bool chosen){
 tcn_add(r,action,arg);
 portable_nova_native_round(r.x,r.y,r.w,r.h,3,chosen?NOVA_CYAN:NOVA_DIM);
 portable_nova_native_round(r.x+1,r.y+1,r.w-2,r.h-2,2,chosen?0x06343bu:0x031115u);
 tcn_label(r,label,chosen?NOVA_TEXT:NOVA_CYAN,2);
}
static bool tcn_open(int w,int h){
 memset(&tcn,0,sizeof(tcn));if(w<160||h<160||w>4096||h>4096)return false;
 tcn.w=w;tcn.h=h;tcn.margin=w<200?3:6;tcn.header=h<200?24:32;tcn.footer=h<200?24:32;
 tcn.top=tcn.margin+tcn.header+16;tcn.bottom=h-tcn.margin-tcn.footer-30;
 tcn.map_screen=UINT32_MAX;tcn.pressed=-1;tcn.active=true;portable_timecard_native_begin();return true;
}
static unsigned tcn_visible(void){unsigned rows=(unsigned)((tcn.bottom-tcn.top)/tcn.row);return rows?rows:1;}
static void tcn_adjust(unsigned total,int row){
 bool changed=tcn.map_screen!=screen_id||tcn.map_week!=week_offset||tcn.map_day!=editing_ymd||tcn.map_editor!=tcp_editor;
 tcn.row=row;tcn.total=total;
 if(changed){tcn.first=selected>0?(unsigned)selected:0;tcn.down=false;tcn.pressed=-1;}
 tcn.map_screen=screen_id;tcn.map_week=week_offset;tcn.map_day=editing_ymd;tcn.map_editor=tcp_editor;
 unsigned maximum=total>tcn_visible()?total-tcn_visible():0;if(tcn.first>maximum)tcn.first=maximum;
}
static void tcn_map(void){
 uint64_t hash=1469598103934665603ULL;
 #define TCN_HASH(v) do{hash^=(uint64_t)(v);hash*=1099511628211ULL;}while(0)
 TCN_HASH(screen_id);TCN_HASH(week_offset);TCN_HASH(editing_ymd);TCN_HASH(tcp_editor);
 TCN_HASH(tcp_key_page);TCN_HASH(tcp_cookie);TCN_HASH(tcn.first);TCN_HASH(store_ready);
#ifdef TIMECARD_CURRENT_PROFILE
 TCN_HASH(tcp_current_format_known);
#endif
 TCN_HASH(tcp_clock_valid);TCN_HASH(tcp_files!=NULL);TCN_HASH(tcn.count);
 for(unsigned i=0;i<tcn.count;i++){tcn_control*c=&tcn.controls[i];TCN_HASH(c->action);TCN_HASH(c->arg);TCN_HASH(c->r.x);TCN_HASH(c->r.y);TCN_HASH(c->r.w);TCN_HASH(c->r.h);}
 #undef TCN_HASH
 portable_timecard_native_layout(hash);
}
static bool tcn_begin(void){
 if(tcp_retained()||!portable_paper_frame_ready())return false;
 tcn.count=0;portable_nova_begin();portable_nova_native_fill(0,0,tcn.w,tcn.h,0x010708u);return true;
}
static void tcn_finish(void){tcn_map();portable_timecard_native_present(false);tcp_dirty=false;}
static void tcn_header(const char *title,const char *subtitle,bool scroll){
#ifdef TIMECARD_CURRENT_PROFILE
 int size=tcn.header,space=scroll?2*size:0,icon=0; /* Current canvas has no decorative glyph dependency. */
#else
 int size=tcn.header,space=scroll?2*size:0,icon=!tcp_editor&&tcn.w>=240?24:0;
 if(icon)(void)portable_nova_native_icon(tcn.margin,tcn.margin+(size-20)/2,20,"solid:f274",NOVA_CYAN);
#endif
 tcn_label((tcn_rect){tcn.margin+icon,tcn.margin,tcn.w-2*tcn.margin-space-icon,size},title,NOVA_CYAN,0);
 if(scroll){tcn_button((tcn_rect){tcn.w-tcn.margin-2*size,tcn.margin,size-2,size},TCN_UP,0,PRODUCTIVITY_NATIVE_SCROLL_UP_LABEL,false);
  tcn_button((tcn_rect){tcn.w-tcn.margin-size,tcn.margin,size,size},TCN_DOWN,0,PRODUCTIVITY_NATIVE_SCROLL_DOWN_LABEL,false);}
 tcn_label((tcn_rect){tcn.margin,tcn.top-16,tcn.w-2*tcn.margin,14},subtitle,NOVA_CAP,3);
}
static void tcn_footer(const t5_ui_chrome_t *chrome){
 int w=(tcn.w-3*tcn.margin)/2,y=tcn.h-tcn.margin-tcn.footer;
 tcn_button((tcn_rect){tcn.margin,y,w,tcn.footer},TCN_BACK,0,chrome->back_label,false);
 tcn_button((tcn_rect){2*tcn.margin+w,y,w,tcn.footer},TCN_SELECT,0,store_ready?chrome->confirm_label:"Retry",selected>=0);
 /* Two real lines preserve the complete failure message on small displays. */
 const char *s=chrome->status;char line[128];size_t used=0,n=strlen(s);
 for(unsigned row=0;row<2&&used<n;row++){
  size_t end=used;while(end<n&&end-used<sizeof(line)-1){memcpy(line,s+used,end-used+1);line[end-used+1]=0;if(portable_nova_measure(3,line)>tcn.w-2*tcn.margin-4)break;end++;}
  if(end==used)break;
  size_t split=end;if(end<n){while(split>used&&s[split]!=' ')split--;if(split==used)split=end;}
  memcpy(line,s+used,split-used);line[split-used]=0;
  tcn_label((tcn_rect){tcn.margin,tcn.bottom+(int)row*14,tcn.w-2*tcn.margin,14},line,NOVA_CAP,3);
  used=split;while(s[used]==' ')used++;
 }
}
static void tcn_list(const t5_ui_chrome_t *chrome,const t5_ui_list_row_t *rows,uint32_t n,int32_t index){
 tcn_adjust(n,tcn.h<200?38:48);if(!tcn_begin())return;
 for(unsigned j=0;j<tcn_visible()&&tcn.first+j<n;j++){
  unsigned i=tcn.first+j;int y=tcn.top+(int)j*tcn.row;tcn_rect r={tcn.margin,y,tcn.w-2*tcn.margin,tcn.row-2};
  tcn_add(r,TCN_ROW,i);portable_nova_native_round(r.x,r.y,r.w,r.h,3,(int32_t)i==index?0x06343bu:0x031115u);
  tcn_label((tcn_rect){r.x+2,y,r.w-4,tcn.row/2},rows[i].title,NOVA_TEXT,2);
  char value[32];tcp_display_time(rows[i].value,value,sizeof(value));
  tcn_label((tcn_rect){r.x+2,y+tcn.row/2,r.w-4,tcn.row/2-2},value[0]?value:rows[i].subtitle,NOVA_CYAN,3);
 }
 tcn_header(chrome->title,chrome->subtitle,n>tcn_visible());tcn_footer(chrome);tcn_finish();
}
static void tcn_table(const t5_ui_chrome_t *chrome,const t5_ui_table_column_t *columns,uint32_t count,const t5_ui_table_row_t *rows,uint32_t n,int32_t index){
 tcn_adjust(n,tcn.h<200?48:64);if(!tcn_begin())return;
 for(unsigned j=0;j<tcn_visible()&&tcn.first+j<n;j++){
  unsigned i=tcn.first+j;int y=tcn.top+(int)j*tcn.row;tcn_rect r={tcn.margin,y,tcn.w-2*tcn.margin,tcn.row-2};
  bool action=!!(rows[i].flags&T5_UI_TABLE_ROW_FULL_WIDTH);tcn_add(r,TCN_ROW,i);
  portable_nova_native_round(r.x,r.y,r.w,r.h,3,(int32_t)i==index?0x06343bu:0x031115u);
  tcn_label((tcn_rect){r.x+2,y,r.w-4,18},rows[i].cells[0],action?NOVA_CYAN:NOVA_TEXT,3);
  if(action)tcn_label((tcn_rect){r.x+2,y+20,r.w-4,20},"Use current local time",NOVA_CAP,3);
  else for(unsigned c=1;c<count&&c<=4;c++){
   char value[32],line[48];tcp_display_time(rows[i].cells[c],value,sizeof(value));
   const char *short_names[]={"","I","LS","LE","O"};
   snprintf(line,sizeof(line),"%s %s",tcn.w<200?short_names[c]:columns[c].title,value);
   int cw=(r.w-4)/2,cy=y+18+(int)((c-1)/2)*(tcn.row<60?14:20);
   tcn_label((tcn_rect){r.x+2+(int)((c-1)%2)*cw,cy,cw,16},line,NOVA_CYAN,3);
  }
 }
 tcn_header(chrome->title,chrome->subtitle,n>tcn_visible());tcn_footer(chrome);tcn_finish();
}
static void tcn_editor(void){
 tcn_adjust(0,1);if(!tcn_begin())return;
 tcn_header(tcp_editor_title,tcp_entry,false);
 int x=tcn.margin,top=tcn.top,cell=(tcn.w-2*x)/8,rows=(tcn.bottom-top)/4;
 for(unsigned key=0;key<PWK_CHARACTERS;key++){
  unsigned ch=portable_watch_key_character(tcp_key_page,key);char text[2]={(char)(ch?ch:' '),0};
  tcn_button((tcn_rect){x+(int)(key%8)*cell,top+(int)(key/8)*rows,cell-1,rows-1},TCN_KEY,key,ch==32?"_":text,key==tcp_key_choice);
 }
 /* Back is a distinct control, alongside all three shared keyboard actions. */
 const char *labels[]={"<","ABC/#","DEL","DONE"};int cellw=(tcn.w-2*x)/4,y=tcn.h-tcn.margin-tcn.footer;
 for(unsigned i=0;i<4;i++)tcn_button((tcn_rect){x+(int)i*cellw,y,cellw-1,tcn.footer},i?TCN_KEY:TCN_BACK,i?PWK_PAGE+i-1:0,labels[i],i&&tcp_key_choice==PWK_PAGE+i-1);
 tcn_label((tcn_rect){x,tcn.bottom,tcn.w-2*x,28},tcp_editor_status,NOVA_CAP,3);tcn_finish();
}
static void tcn_unavailable(bool clock){
 tcn_adjust(0,1);if(!tcn_begin())return;tcn_header("TIME CARD","",false);
 tcn_label((tcn_rect){tcn.margin,tcn.top,tcn.w-2*tcn.margin,20},clock?"Clock unavailable":"History unavailable",NOVA_TEXT,3);
 tcn_label((tcn_rect){tcn.margin,tcn.top+22,tcn.w-2*tcn.margin,20},clock?"Retry when ready":"Writable storage required",NOVA_CAP,3);
 t5_ui_chrome_t chrome={.back_label="BACK",.confirm_label="Retry",.status="No history is created or changed."};
 tcn_footer(&chrome);tcn_finish();
}
static void tcn_draw(void){
 if(tcp_retained())return;
#ifdef TIMECARD_CURRENT_PROFILE
 if(!tcp_current_format_known){
  tcn_adjust(0,1);if(!tcn_begin())return;tcn_header("TIME CARD","",false);
  tcn_label((tcn_rect){tcn.margin,tcn.top,tcn.w-2*tcn.margin,20},"Display preference pending",NOVA_TEXT,3);
  tcn_label((tcn_rect){tcn.margin,tcn.top+22,tcn.w-2*tcn.margin,20},"Retrying; history unchanged",NOVA_CAP,3);
  t5_ui_chrome_t chrome={.back_label="BACK",.confirm_label="Retry",.status="No preference defaults are written."};
  tcn_footer(&chrome);tcn_finish();return;
 }
#endif
 if(tcp_editor){tcn_editor();return;}
 if(!tcp_files){tcp_clock_valid=false;tcn_unavailable(false);return;}
 tcp_clock_valid=tcp_read_datetime(&tcp_snapshot);if(tcp_retained())return;
 if(!tcp_clock_valid)tcn_unavailable(true);else render();
}
static int tcn_target(int x,int y){if(x<0||y<0||x>=tcn.w||y>=tcn.h)return -1;for(unsigned i=0;i<tcn.count;i++)if(tcn_inside(tcn.controls[i].r,x,y))return (int)i;return -1;}
static void tcn_scroll(int delta){int next=(int)tcn.first+delta,maximum=(int)tcn.total-(int)tcn_visible();if(maximum<0)maximum=0;if(next<0)next=0;if(next>maximum)next=maximum;if(next!=(int)tcn.first){tcn.first=(unsigned)next;tcp_dirty=true;}}
static void tcn_action(unsigned action,unsigned arg){
 if(action==TCN_BACK){if(!tcp_files)tcp_home=true;else tcp_back();return;}
 if(action==TCN_UP||action==TCN_DOWN){tcn_scroll(action==TCN_UP?-1:1);return;}
 if(action==TCN_KEY){if(tcp_editor)tcp_key(arg);return;}
 if(!tcp_files)return;
 if(action==TCN_ROW)selected=(int32_t)arg;
 if(action==TCN_ROW||action==TCN_SELECT)tcp_activate();
 tcp_dirty=true;
}
static void tcn_input(const t5_app_input_t *input){
 if(input->exit_requested){tcp_external_exit=true;return;}
 uint64_t generation=0;bool visible=portable_timecard_native_ready(&generation,NULL);
 portable_timecard_contact c={0};bool ok=portable_timecard_native_contact(&c);
 if(generation!=tcn.generation){tcn.generation=generation;tcn.down=false;tcn.pressed=-1;}
 if(!visible||!ok||!c.valid||c.cancelled){tcn.down=false;tcn.pressed=-1;}
 else {
  if(c.down){
   int target=tcn_target(c.x,c.y);
   if(!tcn.down){tcn.down=true;tcn.moved=false;tcn.pressed=target;tcn.start_x=c.x;tcn.start_y=tcn.last_y=c.y;}
   else {
    int dx=c.x-tcn.start_x,dy=c.y-tcn.start_y;if(dx>5||dx< -5||dy>5||dy< -5)tcn.moved=true;
    int ax=dx<0?-dx:dx,ay=dy<0?-dy:dy;
    if(dx>=36&&ax>ay*2){tcn.down=false;tcn.pressed=-1;tcp_back();return;}
    if(!tcp_editor&&tcn.moved&&tcn.start_y>=tcn.top&&tcn.start_y<tcn.bottom){
     int delta=tcn.last_y-c.y;if(delta>=tcn.row||delta<=-tcn.row){tcn_scroll(delta>0?1:-1);tcn.last_y=c.y;}
    }else if(target!=tcn.pressed)tcn.pressed=-1;
   }
  }
  if(tcn.down&&(c.down||c.released)&&(c.x-tcn.start_x>5||tcn.start_x-c.x>5||c.y-tcn.start_y>5||tcn.start_y-c.y>5))tcn.moved=true;
  if(input->tapped){int target=tcn_target(input->touch_x,input->touch_y);if(tcn.down&&!tcn.moved&&target>=0&&target==tcn.pressed)tcn_action(tcn.controls[target].action,tcn.controls[target].arg);tcn.down=false;tcn.pressed=-1;}
  else if(!c.down){tcn.down=false;tcn.pressed=-1;}
 }
 if(input->buttons&T5_APP_BUTTON_BACK){tcn.down=false;tcn.pressed=-1;tcp_back();return;}
 if(!visible||!tcp_files)return;
 if(input->buttons&(T5_APP_BUTTON_UP|T5_APP_BUTTON_LEFT))tcp_move(-1);
 else if(input->buttons&(T5_APP_BUTTON_DOWN|T5_APP_BUTTON_RIGHT))tcp_move(1);
 else if(input->buttons&T5_APP_BUTTON_CONFIRM){if(tcp_editor)tcp_key(tcp_key_choice);else tcp_activate();tcp_dirty=true;}
 if(!tcp_editor&&(input->buttons&(T5_APP_BUTTON_UP|T5_APP_BUTTON_DOWN|T5_APP_BUTTON_LEFT|T5_APP_BUTTON_RIGHT))){if((unsigned)selected<tcn.first)tcn.first=(unsigned)selected;else if((unsigned)selected>=tcn.first+tcn_visible())tcn.first=(unsigned)selected-tcn_visible()+1;}
}
