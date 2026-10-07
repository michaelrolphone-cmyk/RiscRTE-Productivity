/* Static Nova7 e-paper layout. The existing writer, record format, RTC and
 * alarm service own state. Only explicit Save reaches those writers. */
#include <stdlib.h>
#define PE_TOP 112
#define PE_ROW 88
static unsigned pe_first,pe_edit_first,pe_choice_first,pe_focus,pe_key_page;
static bool pe_focus_visible,pe_exit,pe_down,pe_clean;
static int pe_start_x,pe_start_y,pe_last_x,pe_last_y;
static char pe_key_text[POINTS_CUSTOM_NAME_MAX+1];
static int pe_width(void){return app->screen_width();}
static int pe_height(void){return app->screen_height();}
static unsigned pe_rows(void){unsigned n=(unsigned)(pe_height()-PE_TOP-144)/PE_ROW;return n?n:1;}
static void pe_text(int x,int y,int w,const char *s,bool head,bool black){paper->text(x,y,w,s,head?2:1,head,black);}
static void pe_center(int y,const char *s,bool head,bool black){int n=paper->measure(s,head);pe_text((pe_width()-n)/2,y,pe_width()-64,s,head,black);}
static void pe_button(int x,int y,int w,const char *s,bool selected) {
 app->fill_rect(x,y,w,88,true);if(!selected)app->fill_rect(x+3,y+3,w-6,82,false);
 int n=paper->measure(s,false);pe_text(x+(w-n)/2,y+30,w-12,s,false,!selected);
}
static void pe_control(int x,int y,int w,const char *caption,bool selected) {
 const char *glyph=NULL,*text=caption;
 if(!strcmp(caption,"< BACK")){glyph="solid:f053";text="BACK";}
 else if(!strcmp(caption,"MORE >")){glyph="solid:f054";text="MORE";}
 else if(!strcmp(caption,"+ ADD POINT")){glyph="solid:f067";text="ADD POINT";}
 if(!glyph){pe_button(x,y,w,text,selected);return;}
 pe_button(x,y,w,"",selected);int n=paper->measure(text,false),left=x+(w-n-36)/2;
 if(app->draw_icon)(void)app->draw_icon(left,y+31,glyph,24,!selected);
 pe_text(left+36,y+30,w-48,text,false,!selected);
}
static void pe_footer(const char *left,const char *right) {
 int w=(pe_width()-80)/2;pe_control(32,pe_height()-112,w,left,false);pe_control(pe_width()/2+8,pe_height()-112,w,right,true);
}
static void pe_header(const char *s,const char *sub) {
 pe_text(32,24,pe_width()-64,s,true,true);
 if(sub&&*sub)pe_text(32,66,pe_width()-64,sub,false,true);
 app->fill_rect(32,106,pe_width()-64,3,true);
}
static void pe_shape(unsigned kind,int x,int y,bool black) {
 static const char *const glyphs[]={"regular:f111","solid:f0c8","solid:f0d8","solid:f219","solid:f067","solid:f7a5","solid:f111"};
 if(!app->draw_icon||!app->draw_icon(x-13,y-13,glyphs[kind%7],26,black))notice="FONT AWESOME ICON UNAVAILABLE";
}
static void pe_row(unsigned shown,const char *title,const char *value,bool selected) {
 int y=PE_TOP+(int)shown*PE_ROW;
 if(selected)app->fill_rect(32,y,pe_width()-64,PE_ROW,true);
 pe_text(48,y+10,pe_width()-112,title,false,!selected);
 if(value&&*value)pe_text(48,y+42,pe_width()-112,value,false,!selected);
 app->fill_rect(32,y+PE_ROW-3,pe_width()-64,3,true);
}
static void pe_list(void) {
 unsigned order[POINTS_MAX],count=nova_configured(order),rows=pe_rows();char sub[40];
 snprintf(sub,sizeof(sub),"%u ACTIVE  %u TOTAL",nova_active_count(),count);pe_header("POINTS IN TIME",sub);
 if(pe_first>=count)pe_first=0;
 for(unsigned at=0;at<rows&&pe_first+at<count;at++) {
  unsigned index=pe_first+at;const points_item *p=&writer.saved.points[order[index]];
  int y=PE_TOP+(int)at*PE_ROW;bool selected=pe_focus_visible&&pe_focus==index;
  if(selected)app->fill_rect(32,y,pe_width()-64,PE_ROW,true);
  pe_shape(p->kind,56,y+37,!selected);
  char tm[24],days_text[16],duration[16],meta[48];
  snprintf(tm,sizeof(tm),"%u:%02u",time_format==PORTABLE_TIME_FORMAT_24?p->hour:p->hour%12?p->hour%12:12,p->minute);
  pe_text(88,y+8,126,tm,true,!selected);
  if(time_format==PORTABLE_TIME_FORMAT_12)pe_text(90,y+47,110,p->hour<12?"AM":"PM",false,!selected);
  const char *name=nova_kind_name(p->kind);bool head=paper->measure(name,true)<=pe_width()-248;
  pe_text(222,y+6,pe_width()-254,name,head,!selected);
  nova_short_days(p->weekdays,days_text);nova_duration(duration,p->duration_minutes);
  if(!p->enabled)snprintf(meta,sizeof(meta),"OFF  %s",days_text);
  else if(p->duration_minutes)snprintf(meta,sizeof(meta),"%s  %s",duration,days_text);
  else snprintf(meta,sizeof(meta),"%s",days_text);
  pe_text(222,y+44,pe_width()-254,meta,false,!selected);
  app->fill_rect(32,y+PE_ROW-3,pe_width()-64,3,true);
 }
 if(!count)pe_text(32,150,pe_width()-64,"NO POINTS YET",true,true);
 pe_text(32,pe_height()-146,pe_width()-64,count>rows?"SWIPE TO SEE MORE POINTS":status_message(),false,true);
 pe_footer("< BACK",count<POINTS_MAX?"+ ADD POINT":"MORE >");
}
static void pe_edit(void) {
 pe_header("EDIT POINT",nova_kind_name(draft.kind));
 static const char *const names[]={"TYPE","TIME","DURATION","NOTIFY AT END","3 MIN WARNING","NOTIFY START","DAYS","ENABLED","DELETE POINT"};
 for(unsigned at=0;at<pe_rows()&&pe_edit_first+at<9;at++) {
  unsigned row=pe_edit_first+at;char value[48]={0};
  if(row==0)snprintf(value,sizeof(value),"%s",nova_kind_name(draft.kind));
  else if(row==1)format_time(&draft,value,sizeof(value));
  else if(row==2)nova_duration(value,draft.duration_minutes);
  else if(row==3||row==4||row==7)snprintf(value,sizeof(value),"%s",(row==3?draft.notify_end:row==4?draft.warn3:draft.enabled)?"ON":"OFF");
  else if(row==5)snprintf(value,sizeof(value),"%s",point_mode_name(draft.mode,true));
  else if(row==6)format_days(draft.weekdays,value,sizeof(value));
  else if(row==8)snprintf(value,sizeof(value),"%s",nova_delete_confirm?"TAP AGAIN TO DELETE":"EXPLICIT CONFIRMATION REQUIRED");
  pe_row(at,names[row],value,pe_focus_visible&&pe_focus==row);
  if((row==0||row==1||row==2||(row==5&&!points_visual_only())||row==6)&&app->draw_icon)(void)app->draw_icon(pe_width()-72,PE_TOP+(int)at*PE_ROW+30,"solid:f054",24,!(pe_focus_visible&&pe_focus==row));
 }
 pe_text(32,pe_height()-146,pe_width()-64,notice&&*notice?notice:"SWIPE FOR MORE OPTIONS",false,true);pe_footer("CANCEL","SAVE");
}
static unsigned pe_key_count(void){unsigned rows=(unsigned)(pe_height()-448)/88;return (rows?rows:1)*5;}
static void pe_keyboard(void) {
 pe_header("CUSTOM NAME","DONE KEEPS THE DRAFT");pe_text(32,132,pe_width()-64,pe_key_text,false,true);
 unsigned keys=pe_key_count();int w=(pe_width()-64)/5;
 for(unsigned i=0;i<keys;i++){unsigned c=32+pe_key_page*keys+i;if(c>126)break;char text[2]={(char)c,0};int x=32+(int)(i%5)*w,y=208+(int)(i/5)*88;bool selected=pe_focus_visible&&pe_focus==i;
  pe_button(x,y,w-3,"",selected);paper->text(x+(w-20)/2,y+30,w-12,c==32?"_":text,1|PAPER_TEXT_LITERAL,false,!selected);}
 int w2=(pe_width()-80)/2;pe_button(32,pe_height()-216,w2,"NEXT KEYS",false);pe_button(pe_width()/2+8,pe_height()-216,w2,"DELETE",false);pe_footer("CANCEL","DONE");
}
static void pe_draw(void) {
 paper->begin();
 if(!ready||writer.uncertain||writer.meta_uncertain||!writer.loaded) {
  pe_header("POINTS IN TIME",NULL);pe_text(32,150,pe_width()-64,status_message(),false,true);
  pe_text(32,198,pe_width()-64,"RETRY KEEPS EXACT PENDING DATA",false,true);pe_footer("< BACK","RETRY");
 } else if(page==PAGE_LIST)pe_list();
 else if(page==PAGE_EDIT)pe_edit();
 else if(page==PAGE_CUSTOM_KEYBOARD)pe_keyboard();
 else if(page==PAGE_TYPE||page==PAGE_MODE) {
  pe_header(page==PAGE_TYPE?"POINT TYPE":"NOTIFY START",NULL);
  unsigned choices[8],n=page==PAGE_TYPE?nova_type_choices(choices):4;
  for(unsigned at=0;at<pe_rows()&&pe_choice_first+at<n;at++){unsigned i=pe_choice_first+at;unsigned kind=page==PAGE_TYPE?choices[i]:0;
   pe_row(at,page==PAGE_TYPE?(kind?nova_kind_name(kind):"+ CUSTOM TYPE"):point_mode_name(i,true),page==PAGE_TYPE&&custom_index(kind)<POINTS_CUSTOM_COUNT?"TAP RIGHT TO EDIT":"",pe_focus_visible&&pe_focus==i);}
  pe_footer("< BACK","MORE >");
 } else if(page==PAGE_TIME||page==PAGE_DURATION) {
  pe_header(page==PAGE_TIME?"TIME":"DURATION","DRAFT ONLY - USE SAVE");char value[32];
  if(page==PAGE_TIME)format_time(&draft,value,sizeof(value));else nova_duration(value,draft.duration_minutes);
  pe_center(150,value,true,true);int w=(pe_width()-80)/2;
  pe_button(32,256,w,page==PAGE_TIME?"HOUR -":"- 5 MIN",false);pe_button(pe_width()/2+8,256,w,page==PAGE_TIME?"HOUR +":"+ 5 MIN",false);
  pe_button(32,368,w,page==PAGE_TIME?"MINUTE -":"- 1 MIN",false);pe_button(pe_width()/2+8,368,w,page==PAGE_TIME?"MINUTE +":"+ 1 MIN",false);
  pe_footer("< BACK","DONE");
 } else if(page==PAGE_DAYS) {
  pe_header("DAYS","SWIPE FOR DAYS AND PRESETS");
  for(unsigned at=0;at<pe_rows()&&pe_choice_first+at<10;at++){unsigned i=pe_choice_first+at;const char *name=i<7?days[i]:i==7?"DAILY":i==8?"MON-FRI":"WEEKEND";pe_row(at,name,i<7?(draft.weekdays&(1u<<i)?"ON":"OFF"):"APPLY PRESET",pe_focus_visible&&pe_focus==i);}
  pe_footer("< BACK","DONE");
 } else if(page==PAGE_CUSTOM) {
  pe_header("CUSTOM TYPE",notice&&*notice?notice:"SHARED NAME AND COLOR");unsigned ci=custom_index(custom_kind);
  if(ci<POINTS_CUSTOM_COUNT){pe_button(32,144,pe_width()-64,custom_draft.custom[ci].name[0]?custom_draft.custom[ci].name:"EDIT NAME",false);
   for(unsigned i=0;i<8;i++){char name[16];snprintf(name,sizeof(name),"COLOR %u",i+1);int w=(pe_width()-88)/4;pe_button(32+(int)(i%4)*(w+8),288+(int)(i/4)*104,w,name,custom_draft.custom[ci].color==i);}}
  pe_footer("CANCEL","SAVE");
 } else {pe_header("SAVE POINT",NULL);pe_text(32,150,pe_width()-64,"ONLY FUTURE STARTS RUN AFTER SAVE",false,true);pe_footer("CANCEL","SAVE");}
 app->present(pe_clean);pe_clean=false;
}
static void pe_reset_page(void){pe_focus=0;pe_focus_visible=false;pe_choice_first=0;pe_down=false;pe_clean=true;}
static void pe_back(void){if(page==PAGE_CUSTOM_KEYBOARD){page=PAGE_CUSTOM;notice="Name edit discarded";}else pe_exit=on_back();pe_reset_page();}
static void pe_key(unsigned index){unsigned c=32+pe_key_page*pe_key_count()+index;size_t n=strlen(pe_key_text);if(c<=126&&n<POINTS_CUSTOM_NAME_MAX){pe_key_text[n]=(char)c;pe_key_text[n+1]=0;}}
static void pe_choose_type(unsigned index,bool edit) {
 unsigned choices[8],n=nova_type_choices(choices);if(index>=n)return;unsigned kind=choices[index];
 if(kind&&(!edit||custom_index(kind)>=POINTS_CUSTOM_COUNT)){draft.kind=(uint8_t)kind;if(!duration_kind(kind)){draft.duration_minutes=0;draft.notify_end=draft.warn3=0;}page=PAGE_EDIT;return;}
 if(!kind){for(unsigned i=0;i<POINTS_CUSTOM_COUNT;i++)if(!writer.meta.custom[i].name[0]){kind=POINTS_CUSTOM_1+i;break;}}
 custom_kind=kind;custom_draft=writer.meta;unsigned ci=custom_index(kind);if(ci<POINTS_CUSTOM_COUNT){page=PAGE_CUSTOM;custom_pos=0;}
}
static void pe_edit_action(unsigned row) {
 if(row!=8)nova_delete_confirm=false;
 bool timed=duration_kind(draft.kind)&&draft.duration_minutes;
 if(row==0)page=PAGE_TYPE;else if(row==1)page=PAGE_TIME;
 else if(row==2){if(duration_kind(draft.kind))page=PAGE_DURATION;else notice="Duration not used for this type";}
 else if(row==3){if(timed)draft.notify_end^=1;else notice="Set a duration first";}
 else if(row==4){if(timed&&draft.duration_minutes>=3)draft.warn3^=1;else notice="Set duration to at least 3 min";}
 else if(row==5)open_mode_page();else if(row==6)page=PAGE_DAYS;
 else if(row==7){draft.enabled^=1;nova_delete_confirm=false;}
 else if(row==8){if(!nova_delete_confirm){nova_delete_confirm=true;notice="Tap delete again to confirm";}else{draft=(points_item){0};save_action();nova_delete_confirm=false;pe_edit_first=0;}}
}
static void pe_advance(void) {
 unsigned count=0,order[POINTS_MAX],*first=&pe_choice_first;
 if(page==PAGE_LIST){count=nova_configured(order);first=&pe_first;}
 else if(page==PAGE_EDIT){count=9;first=&pe_edit_first;}
 else if(page==PAGE_TYPE)count=nova_type_choices(order);else if(page==PAGE_MODE)count=4;else if(page==PAGE_DAYS)count=10;
 *first=*first+pe_rows()<count?*first+pe_rows():0;
}
static void pe_tap(int x,int y) {
 if(x<32||x>=pe_width()-32)return;
 unsigned old_page=page;
 if(y>=pe_height()-112&&y<pe_height()-24) {
  if(x>=pe_width()/2-8&&x<pe_width()/2+8)return;
  if(x<pe_width()/2){pe_back();return;}
  if(!ready||writer.uncertain||writer.meta_uncertain||!writer.loaded)retry_action();
  else if(page==PAGE_LIST){if(nova_first_empty()<POINTS_MAX){nova_new_point();pe_edit_first=0;}else pe_advance();}
  else if(page==PAGE_EDIT||page==PAGE_SAVE)save_action();
  else if(page==PAGE_CUSTOM){nova_tap(180,205);}
  else if(page==PAGE_CUSTOM_KEYBOARD){unsigned ci=custom_index(custom_kind);if(ci<POINTS_CUSTOM_COUNT){memcpy(custom_draft.custom[ci].name,pe_key_text,sizeof(pe_key_text));page=PAGE_CUSTOM;notice="Name draft updated";}}
  else if(page==PAGE_TYPE||page==PAGE_MODE)pe_advance();else page=PAGE_EDIT;
 } else if(!ready||writer.uncertain||writer.meta_uncertain||!writer.loaded)return;
 else if(page==PAGE_LIST&&y>=PE_TOP&&y<PE_TOP+(int)pe_rows()*PE_ROW) {
  unsigned order[POINTS_MAX],n=nova_configured(order),at=pe_first+(unsigned)(y-PE_TOP)/PE_ROW;
  if(at<n){edit_slot(order[at]);pe_edit_first=0;nova_delete_confirm=false;}
 } else if(page==PAGE_EDIT&&y>=PE_TOP&&y<PE_TOP+(int)pe_rows()*PE_ROW)pe_edit_action(pe_edit_first+(unsigned)(y-PE_TOP)/PE_ROW);
 else if((page==PAGE_TYPE||page==PAGE_MODE)&&y>=PE_TOP&&y<PE_TOP+(int)pe_rows()*PE_ROW) {
  unsigned at=pe_choice_first+(unsigned)(y-PE_TOP)/PE_ROW;
  if(page==PAGE_TYPE)pe_choose_type(at,x>=pe_width()-112);else if(at<4){select_mode(at);}
 } else if(page==PAGE_TIME||page==PAGE_DURATION) {
  if(x>=pe_width()/2-8&&x<pe_width()/2+8)return;
  int direction=x<pe_width()/2?-1:1;
  if(y>=256&&y<344){if(page==PAGE_TIME)draft.hour=(uint8_t)((draft.hour+24+direction)%24);else{int n=(int)draft.duration_minutes+direction*5;draft.duration_minutes=(uint16_t)(n<0?0:n>720?720:n);}}
  else if(y>=368&&y<456){if(page==PAGE_TIME)draft.minute=(uint8_t)((draft.minute+60+direction)%60);else{int n=(int)draft.duration_minutes+direction;draft.duration_minutes=(uint16_t)(n<0?0:n>720?720:n);}}
  if(page==PAGE_DURATION&&!draft.duration_minutes)draft.notify_end=draft.warn3=0;
 } else if(page==PAGE_DAYS) {
  if(y>=PE_TOP&&y<PE_TOP+(int)pe_rows()*PE_ROW){unsigned at=pe_choice_first+(unsigned)(y-PE_TOP)/PE_ROW;if(at<7)draft.weekdays^=(uint8_t)(1u<<at);else if(at<10)draft.weekdays=at==7?127:at==8?62:65;}
 } else if(page==PAGE_CUSTOM) {
  unsigned ci=custom_index(custom_kind);if(ci>=POINTS_CUSTOM_COUNT)return;
  if(y>=144&&y<232){memcpy(pe_key_text,custom_draft.custom[ci].name,sizeof(pe_key_text));pe_key_page=0;page=PAGE_CUSTOM_KEYBOARD;}
  else if(y>=288&&y<496){int w=(pe_width()-88)/4;unsigned at=(unsigned)(y-288)/104*4+(unsigned)(x-32)/(unsigned)(w+8);if(at<8&&(y-288)%104<88&&(x-32)%(w+8)<w)custom_draft.custom[ci].color=(uint8_t)at;}
 } else if(page==PAGE_CUSTOM_KEYBOARD) {
  if(y>=pe_height()-216&&y<pe_height()-128){if(x>=pe_width()/2-8&&x<pe_width()/2+8)return;if(x<pe_width()/2)pe_key_page=(pe_key_page+1)%((95+pe_key_count()-1)/pe_key_count());else{size_t n=strlen(pe_key_text);if(n)pe_key_text[n-1]=0;}}
  else if(y>=208&&y<208+(int)(pe_key_count()/5)*88){int w=(pe_width()-64)/5;unsigned col=(unsigned)(x-32)/(unsigned)w,at=(unsigned)(y-208)/88*5+col;if(col<5&&(x-32)%w<w-3&&at<pe_key_count())pe_key(at);}
 }
 if(page!=old_page)pe_reset_page();
}
static bool pe_input(const t5_app_input_t *input) {
 bool changed=false;unsigned old_page=page;
 if(input->buttons&(T5_APP_BUTTON_UP|T5_APP_BUTTON_LEFT|T5_APP_BUTTON_DOWN|T5_APP_BUTTON_RIGHT)) {
  bool next=!!(input->buttons&(T5_APP_BUTTON_DOWN|T5_APP_BUTTON_RIGHT));unsigned n=0,items[POINTS_MAX],*first=&pe_choice_first;
  if(page==PAGE_LIST){n=nova_configured(items);first=&pe_first;}else if(page==PAGE_EDIT){n=9;first=&pe_edit_first;}else if(page==PAGE_TYPE)n=nova_type_choices(items);else if(page==PAGE_MODE)n=4;else if(page==PAGE_DAYS)n=10;
  if(n){pe_focus=next?(pe_focus+1)%n:(pe_focus+n-1)%n;*first=pe_focus/pe_rows()*pe_rows();pe_focus_visible=true;changed=true;}
  else if(page==PAGE_TIME||page==PAGE_DURATION){pe_tap(next?pe_width()-80:80,input->buttons&(T5_APP_BUTTON_UP|T5_APP_BUTTON_DOWN)?400:280);changed=true;}
 }
 if(input->buttons&T5_APP_BUTTON_CONFIRM) {
  if(page==PAGE_LIST){unsigned order[POINTS_MAX],n=nova_configured(order);if(n){edit_slot(order[pe_focus<n?pe_focus:0]);pe_edit_first=0;}else nova_new_point();}
  else if(page==PAGE_EDIT)pe_edit_action(pe_focus);
  else if(page==PAGE_TYPE)pe_choose_type(pe_focus,false);
  else if(page==PAGE_MODE){select_mode(pe_focus%4);}
  else if(page==PAGE_DAYS){if(pe_focus<7)draft.weekdays^=(uint8_t)(1u<<pe_focus);else draft.weekdays=pe_focus==7?127:pe_focus==8?62:65;}
  changed=true;
 }
 springboard_contact c={0};paper->contact(&c);
 if(!c.valid||c.cancelled)pe_down=false;
 if(c.valid&&c.began){pe_down=true;pe_start_x=pe_last_x=c.x;pe_start_y=pe_last_y=c.y;}
 if(c.valid&&c.down&&pe_down){pe_last_x=c.x;pe_last_y=c.y;}
 if(c.valid&&c.released&&!c.cancelled) {
  int dy=pe_last_y-pe_start_y,dx=pe_last_x-pe_start_x;
  if(pe_down&&!c.tap_eligible&&abs(dy)>=44&&abs(dy)>abs(dx)*2&&(page==PAGE_LIST||page==PAGE_EDIT||page==PAGE_TYPE||page==PAGE_MODE||page==PAGE_DAYS)){if(dy<0)pe_advance();else{pe_first=pe_edit_first=pe_choice_first=0;}changed=true;}
  else if(c.tap_eligible){pe_tap(c.x,c.y);changed=true;}
  pe_down=false;
 } else if(input->tapped){pe_tap(input->touch_x,input->touch_y);changed=true;}
 if(page!=old_page)pe_reset_page();
 return changed;
}
