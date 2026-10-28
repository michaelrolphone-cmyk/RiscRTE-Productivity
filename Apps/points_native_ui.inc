#ifdef PORTABLE_NATIVE_SCENE
#include "native_scroll_labels.h"
#else
/* Frozen source composition has no new shared-header dependency. */
#define PRODUCTIVITY_NATIVE_SCROLL_UP_LABEL "^"
#define PRODUCTIVITY_NATIVE_SCROLL_DOWN_LABEL "v"
#endif
/* Capability-sized Points presentation. The existing writer, scheduling and
 * alarm policy remain authoritative; controls invoke their existing actions. */
#include "PortablePointsNative.h"
/* Existing private shared frame gate, linked from the selected adapter. */
extern bool portable_paper_frame_ready(void);
#if !defined(PORTABLE_NOVA_UI) || !defined(PORTABLE_NATIVE_CANVAS) || !defined(PORTABLE_APP_OWNS_TOUCH_CHROME)
#error Native Points requires NOVA, native canvas and app-owned touch chrome
#endif
typedef struct {int x,y,w,h;} pn_rect;
typedef struct {pn_rect r;unsigned action,arg;char label[64];bool selected;} pn_control;
enum {PN_BACK=1,PN_SAVE,PN_NEW,PN_EDIT,PN_TYPE,PN_TIME,PN_DURATION,PN_END,PN_WARN,
 PN_MODE,PN_DAY,PN_PRESET,PN_ENABLED,PN_DELETE,PN_KIND,PN_CUSTOM,PN_COLOR,PN_NAME,
 PN_CUSTOM_SAVE,PN_KEY,PN_PICK,PN_AMPM,PN_RETRY,PN_SCROLL_UP,PN_SCROLL_DOWN};
static struct {int w,h,margin,header,footer,row,top,bottom;unsigned first,total,count,map_page;
 pn_control controls[64];uint64_t generation,map;bool active,down,moved,dirty;int pressed,start_x,start_y,last_y;
 unsigned pressed_action,pressed_arg;} pn;
static bool pn_inside(pn_rect r,int x,int y){return x>=r.x&&y>=r.y&&x-r.x<r.w&&y-r.y<r.h;}
static void pn_label(pn_rect r,const char *s,uint32_t color){
 unsigned face=r.h>=28?0:r.h>=20?2:3;int height=face==0?20:face==2?16:14;
 if(portable_nova_measure(face,s)>r.w-6){face=3;height=14;}
 char text[96];size_t n=strlen(s);if(n>sizeof(text)-4)n=sizeof(text)-4;memcpy(text,s,n);text[n]=0;
 while(n&&portable_nova_measure(face,text)>r.w-6){--n;memcpy(text+n,"...",4);}
 int width=portable_nova_measure(face,text);if(width>r.w-6||height>r.h)return;
 portable_nova_native_text(face,r.x+(r.w-width)/2,r.y+(r.h-height)/2,r.w,text,color);
}
static bool pn_open(int w,int h){
 if(w<160||h<160||w>4096||h>4096)return false;
 memset(&pn,0,sizeof(pn));pn.w=w;pn.h=h;pn.margin=w<200?3:6;
 pn.header=h<200?24:36;pn.footer=h<200?24:32;pn.row=h<200?28:36;
 pn.top=pn.margin+pn.header+16;pn.bottom=h-pn.margin-pn.footer-3;
 pn.active=true;pn.map_page=UINT32_MAX;pn.pressed=-1;portable_points_native_begin();return true;
}
static void pn_add(pn_rect r,unsigned action,unsigned arg,const char *label,bool selected){
 if(pn.count>=64||r.w<1||r.h<1)return;
 pn_control *c=&pn.controls[pn.count++];*c=(pn_control){.r=r,.action=action,.arg=arg,.selected=selected};
 snprintf(c->label,sizeof(c->label),"%s",label);
}
static unsigned pn_visible(void){unsigned n=(unsigned)((pn.bottom-pn.top)/pn.row);return n?n:1;}
static void pn_row(unsigned action,unsigned arg,const char *label,bool selected){
 unsigned row=pn.total++;
 if(row<pn.first||row-pn.first>=pn_visible())return;
 pn_add((pn_rect){pn.margin,pn.top+(int)(row-pn.first)*pn.row,pn.w-2*pn.margin,pn.row-2},action,arg,label,selected);
}
static void pn_footer(const char *left,unsigned left_action,const char *right,unsigned right_action){
 int y=pn.h-pn.margin-pn.footer,w=(pn.w-3*pn.margin)/2;
 pn_add((pn_rect){pn.margin,y,w,pn.footer},left_action,0,left,false);
 pn_add((pn_rect){2*pn.margin+w,y,w,pn.footer},right_action,0,right,false);
}
static void pn_map(void){
 uint64_t hash=1469598103934665603ULL;
 #define PN_HASH(v) do{hash^=(uint64_t)(v);hash*=1099511628211ULL;}while(0)
 PN_HASH(page);PN_HASH(selected);PN_HASH(custom_kind);PN_HASH(custom_key_page);PN_HASH(pn.first);
 PN_HASH(ready);PN_HASH(writer.loaded);PN_HASH(writer.uncertain);PN_HASH(writer.meta_uncertain);
 PN_HASH(draft.kind);PN_HASH(nova_delete_confirm);PN_HASH(service_state.occurrence.generation);PN_HASH(pn.count);
 for(unsigned i=0;i<pn.count;i++){pn_control*c=&pn.controls[i];PN_HASH(c->action);PN_HASH(c->arg);PN_HASH(c->r.x);PN_HASH(c->r.y);PN_HASH(c->r.w);PN_HASH(c->r.h);}
 #undef PN_HASH
 pn.map=hash;portable_points_native_layout(hash);
}
static void pn_build(void){
 if(pn.map_page!=page){pn.first=0;pn.map_page=page;pn.down=false;pn.pressed=-1;p7_picker_reset();}
 pn.count=pn.total=0;char text[64];
 pn_add((pn_rect){pn.margin,pn.margin,pn.header,pn.header},PN_BACK,0,"<",false);
 if(!ready||writer.uncertain||writer.meta_uncertain||!writer.loaded){pn.first=0;pn_row(PN_RETRY,0,"RETRY",false);pn_map();return;}
#ifndef PORTABLE_ALARM_CLIENT
 if(service_valid&&service_state.occurrence.generation){pn.first=0;pn_row(PN_RETRY,0,"DISMISS",false);pn_map();return;}
#endif
 switch(page){
 case PAGE_LIST:{unsigned order[POINTS_MAX],n=nova_configured(order);
  for(unsigned i=0;i<n;i++){points_item*p=&writer.saved.points[order[i]];char tm[20];format_time(p,tm,sizeof(tm));snprintf(text,sizeof(text),"%s %s%s",tm,nova_kind_name(p->kind),p->enabled?"":" OFF");pn_row(PN_EDIT,order[i],text,false);}
  if(n<POINTS_MAX)pn_row(PN_NEW,0,"+ ADD POINT",false);
  break;}
 case PAGE_EDIT:
  snprintf(text,sizeof(text),"TYPE: %s",nova_kind_name(draft.kind));pn_row(PN_TYPE,0,text,false);
  {char tm[20];format_time(&draft,tm,sizeof(tm));snprintf(text,sizeof(text),"TIME: %s",tm);pn_row(PN_TIME,0,text,false);}
  {char dur[16];nova_duration(dur,draft.duration_minutes);snprintf(text,sizeof(text),"DURATION: %s",duration_kind(draft.kind)?dur:"NOT USED");pn_row(PN_DURATION,0,text,false);}
  pn_row(PN_END,0,"NOTIFY AT END",draft.notify_end);pn_row(PN_WARN,0,"3 MIN WARNING",draft.warn3);
  snprintf(text,sizeof(text),"START: %s",point_mode_name(draft.mode,true));pn_row(PN_MODE,0,text,false);
  for(unsigned i=0;i<7;i++){snprintf(text,sizeof(text),"DAY: %s",days[i]);pn_row(PN_DAY,i,text,!!(draft.weekdays&(1u<<i)));}
  pn_row(PN_PRESET,127,"EVERY DAY",draft.weekdays==127);pn_row(PN_PRESET,62,"MON-FRI",draft.weekdays==62);pn_row(PN_PRESET,65,"WEEKEND",draft.weekdays==65);
  pn_row(PN_ENABLED,0,"ENABLED",draft.enabled);pn_row(PN_DELETE,0,nova_delete_confirm?"CONFIRM DELETE":"DELETE POINT",nova_delete_confirm);
  pn_footer("CANCEL",PN_BACK,"SAVE",PN_SAVE);break;
 case PAGE_TYPE:{unsigned choices[8],n=p7_type_count(choices);for(unsigned i=0;i<n;i++){
   unsigned k=choices[i];pn_row(PN_KIND,k,k?nova_kind_name(k):"+ CUSTOM TYPE",k==draft.kind);
   if(custom_index(k)<POINTS_CUSTOM_COUNT){snprintf(text,sizeof(text),"EDIT: %s",nova_kind_name(k));pn_row(PN_CUSTOM,k,text,false);}}
  pn_footer("BACK",PN_BACK,"",0);break;}
 case PAGE_MODE:for(unsigned i=0;i<4;i++)pn_row(PN_MODE,i+1,point_mode_name(i,true),draft.mode==i);pn_footer("BACK",PN_BACK,"",0);break;
 case PAGE_TIME:case PAGE_DURATION:{
  int gap=pn.margin,cell=(pn.w-3*gap)/2,height=(pn.bottom-pn.top)/3;
  for(unsigned col=0;col<2;col++)for(unsigned row=0;row<3;row++) {
   unsigned value=0;bool valid=p7_picker_value(col,(int)row-1,&value);
   if(page==PAGE_TIME&&!col&&time_format==PORTABLE_TIME_FORMAT_12)value=value%12?value%12:12;
   if(valid)snprintf(text,sizeof(text),"%02u %s",value,col?"MIN":"HOUR");else text[0]=0;
   pn_add((pn_rect){gap+(int)col*(cell+gap),pn.top+(int)row*height,cell,height-2},PN_PICK,col*3+row,text,row==1);
  }
  pn_footer("BACK",PN_BACK,page==PAGE_TIME&&time_format==PORTABLE_TIME_FORMAT_12?(draft.hour<12?"AM -> PM":"PM -> AM"):"",page==PAGE_TIME&&time_format==PORTABLE_TIME_FORMAT_12?PN_AMPM:0);break;}
 case PAGE_CUSTOM:{unsigned ci=custom_index(custom_kind);if(ci>=POINTS_CUSTOM_COUNT)break;
  snprintf(text,sizeof(text),"NAME: %s",custom_draft.custom[ci].name[0]?custom_draft.custom[ci].name:"TAP TO TYPE");pn_row(PN_NAME,0,text,false);
  for(unsigned i=0;i<8;i++){snprintf(text,sizeof(text),"COLOR %u",i+1);pn_row(PN_COLOR,i,text,custom_draft.custom[ci].color==i);}
  pn_footer("BACK",PN_BACK,"SAVE",PN_CUSTOM_SAVE);break;}
 case PAGE_CUSTOM_KEYBOARD:{int gap=2,x=pn.margin,y=pn.top,available=pn.bottom-pn.top,cell=(pn.w-2*pn.margin)/8;
  for(unsigned i=0;i<32;i++){unsigned ch=p7_watch_key_character(custom_key_page,i);char key[2]={(char)(ch?ch:' '),0};
   pn_add((pn_rect){x+(int)(i%8)*cell,y+(int)(i/8)*(available/4),cell-gap,available/4-gap},PN_KEY,i,ch==32?"_":key,custom_key_choice==i);}
  int w=(pn.w-2*pn.margin)/3;const char*labels[]={"ABC/#","DEL","DONE"};
  for(unsigned i=0;i<3;i++)pn_add((pn_rect){x+(int)i*w,pn.h-pn.margin-pn.footer,w-gap,pn.footer},PN_KEY,32+i,labels[i],custom_key_choice==32+i);
  break;}
 default:pn_row(PN_BACK,0,"BACK",false);break;
 }
 if(pn.total>pn_visible()){
  int size=pn.header;pn_add((pn_rect){pn.w-pn.margin-2*size,pn.margin,size-2,size},PN_SCROLL_UP,0,PRODUCTIVITY_NATIVE_SCROLL_UP_LABEL,false);
  pn_add((pn_rect){pn.w-pn.margin-size,pn.margin,size,size},PN_SCROLL_DOWN,0,PRODUCTIVITY_NATIVE_SCROLL_DOWN_LABEL,false);
 }
 unsigned maximum=pn.total>pn_visible()?pn.total-pn_visible():0;if(pn.first>maximum){pn.first=maximum;pn_build();return;}
 pn_map();
}
static void pn_paint_control(const pn_control*c) {
 if(!c->action)return;
 uint32_t edge=c->action==PN_COLOR&&c->arg<8?nova_custom_colors[c->arg]:c->selected?NOVA_CYAN:NOVA_DIM;
 portable_nova_native_round(c->r.x,c->r.y,c->r.w,c->r.h,3,edge);
 portable_nova_native_round(c->r.x+1,c->r.y+1,c->r.w-2,c->r.h-2,2,c->selected?0x06343bu:0x031115u);
 if(c->action!=PN_PICK)pn_label(c->r,c->label,c->selected?NOVA_WHITE:NOVA_TEXT);
}
static void pn_render(void) {
 pn.dirty=true;pn_build();if(!portable_paper_frame_ready())return;
 portable_nova_begin();portable_nova_native_fill(0,0,pn.w,pn.h,0x010708u);
 for(unsigned i=0;i<pn.count;i++)pn_paint_control(&pn.controls[i]);
 if(page==PAGE_TIME||page==PAGE_DURATION) {
  int row=(pn.bottom-pn.top)/3,cell=(pn.w-3*pn.margin)/2;
  for(unsigned col=0;col<2;col++)for(int j=-2;j<=2;j++) {
   unsigned value;if(!p7_picker_value(col,j,&value))continue;
   if(page==PAGE_TIME&&!col&&time_format==PORTABLE_TIME_FORMAT_12)value=value%12?value%12:12;
   char text[24];snprintf(text,sizeof(text),"%02u %s",value,col?"MIN":"HOUR");
   int offset=p7_picker.owner_page==page&&p7_picker.column==col?p7_picker.offset*row/P7_PICKER_ROW:0;
   pn_rect r={pn.margin+(int)col*(cell+pn.margin),pn.top+(j+1)*row+offset,cell,row-2};
   pn_label(r,text,j?NOVA_CAP:NOVA_CYAN);
  }
  /* Clip the moving glyphs to their native viewport, then restore fixed
   * chrome. No scroll pixel may change a header/footer action or label. */
  portable_nova_native_fill(0,0,pn.w,pn.top,0x010708u);
  portable_nova_native_fill(0,pn.bottom,pn.w,pn.h-pn.bottom,0x010708u);
  for(unsigned i=0;i<pn.count;i++)if(pn.controls[i].action!=PN_PICK)pn_paint_control(&pn.controls[i]);
 }
 int title_width=pn.w-2*pn.margin-pn.header-(pn.total>pn_visible()?2*pn.header:0);
 const char*title=page==PAGE_LIST?"POINTS":page==PAGE_EDIT?"EDIT POINT":page==PAGE_TIME?"TIME":page==PAGE_DURATION?"DURATION":page==PAGE_CUSTOM_KEYBOARD?"NAME":"POINTS";
 pn_label((pn_rect){pn.margin+pn.header,pn.margin,title_width,pn.header},title,NOVA_CYAN);
 const char*sub=notice;
 if(!ready)sub="DEPENDENCIES UNAVAILABLE";else if(writer.uncertain||writer.meta_uncertain)sub="SAVE UNCONFIRMED";else if(!writer.loaded)sub="STORAGE INVALID";else if(page==PAGE_CUSTOM_KEYBOARD)sub=p7_key_text;
 pn_label((pn_rect){pn.margin,pn.top-16,pn.w-2*pn.margin,14},sub?sub:"",NOVA_CAP);
 portable_points_native_present(false);pn.dirty=false;
}
static int pn_target(int x,int y){if(x<0||y<0||x>=pn.w||y>=pn.h)return -1;for(unsigned i=0;i<pn.count;i++)if(pn_inside(pn.controls[i].r,x,y))return (int)i;return -1;}
static bool pn_scroll(int delta){int next=(int)pn.first+delta,max=(int)pn.total-(int)pn_visible();if(max<0)max=0;if(next<0)next=0;if(next>max)next=max;if(next==(int)pn.first)return false;pn.first=(unsigned)next;return true;}
static void pn_activate(unsigned action,unsigned arg,t5_app_input_t *input){
 if(action==PN_BACK){p7_picker_reset();input->buttons|=T5_APP_BUTTON_BACK;return;}
 if(action==PN_SCROLL_UP||action==PN_SCROLL_DOWN){pn_scroll(action==PN_SCROLL_UP?-1:1);return;}
 if(action==PN_RETRY){nova7_tap(100,180);return;}
 if(!ready||writer.uncertain||writer.meta_uncertain||!writer.loaded)return;
 bool timed=duration_kind(draft.kind)&&draft.duration_minutes;
 switch(action){
 case PN_NEW:p7_new_point();break;case PN_EDIT:edit_slot(arg);nova_delete_confirm=false;break;
 case PN_SAVE:save_action();break;case PN_TYPE:page=PAGE_TYPE;break;
 case PN_TIME:p7_picker_reset();page=PAGE_TIME;break;
 case PN_DURATION:if(duration_kind(draft.kind)){p7_picker_reset();page=PAGE_DURATION;}else notice="Duration not used for this type";break;
 case PN_END:if(timed)draft.notify_end^=1;else notice="Set a duration first";break;
 case PN_WARN:if(timed&&draft.duration_minutes>=3)draft.warn3^=1;else notice="Set duration to at least 3 min";break;
 case PN_MODE:if(page==PAGE_MODE&&arg)select_mode(arg-1);else open_mode_page();break;
 case PN_DAY:if(arg<7)draft.weekdays^=(uint8_t)(1u<<arg);break;
 case PN_PRESET:draft.weekdays=(uint8_t)arg;break;case PN_ENABLED:draft.enabled^=1;nova_delete_confirm=false;break;
 case PN_DELETE:if(!nova_delete_confirm){nova_delete_confirm=true;notice="Tap delete again to confirm";}else{draft=(points_item){0};save_action();nova_delete_confirm=false;}break;
 case PN_KIND:if(!arg){for(unsigned i=0;i<POINTS_CUSTOM_COUNT;i++)if(!writer.meta.custom[i].name[0]){p7_custom_open(POINTS_CUSTOM_1+i,true);break;}}
  else{draft.kind=(uint8_t)arg;if(!duration_kind(arg)){draft.duration_minutes=0;draft.notify_end=draft.warn3=0;}page=PAGE_EDIT;}break;
 case PN_CUSTOM:p7_custom_open(arg,false);break;case PN_NAME:p7_key_begin();break;
 case PN_COLOR:{unsigned ci=custom_index(custom_kind);if(ci<POINTS_CUSTOM_COUNT&&arg<8)custom_draft.custom[ci].color=(uint8_t)arg;break;}
 case PN_CUSTOM_SAVE:p7_custom_save();break;case PN_KEY:custom_key_choice=arg;p7_key_activate(arg);break;
 case PN_PICK:p7_picker.column=arg/3;if(arg%3!=1)p7_picker_move(arg/3,arg%3?1:-1);break;
 case PN_AMPM:draft.hour=(uint8_t)((draft.hour+12)%24);break;default:break;
 }
}
/* Feed the established picker reducer a translated contact. Its wrapping,
 * duration limits, residual movement, release rounding and settling stay shared. */
static bool pn_picker_contact(int y,bool began,bool down,bool released) {
 int height=(pn.bottom-pn.top)/3;
 springboard_contact c={.valid=true,.began=began,.down=down,.released=released,
  .x=pn.pressed_arg/3?172:68,
  .y=146+(y-pn.start_y)*P7_PICKER_ROW/height};
 return p7_picker_contact(&c);
}
static bool pn_input(t5_app_input_t*input) {
 uint64_t gen=0;bool visible=portable_points_native_ready(&gen,NULL);
 portable_points_contact c={0};bool ok=portable_points_native_contact(&c),changed=false;
 if(gen!=pn.generation){pn.generation=gen;pn.down=false;pn.pressed=-1;p7_picker_reset();}
 if(!visible||!ok||c.cancelled||!c.valid){pn.down=false;pn.pressed=-1;p7_picker_reset();input->tapped=false;goto navigation;}
 if(c.down) {
  int target=pn_target(c.x,c.y);
  if(!pn.down) {
   pn.down=true;pn.moved=false;pn.pressed=target;pn.start_x=c.x;pn.start_y=pn.last_y=c.y;
   pn.pressed_action=target>=0?pn.controls[target].action:0;
   pn.pressed_arg=target>=0?pn.controls[target].arg:0;
   if(pn.pressed_action==PN_PICK)changed|=pn_picker_contact(c.y,true,true,false);
  } else {
   if(c.x-pn.start_x>5||pn.start_x-c.x>5||c.y-pn.start_y>5||pn.start_y-c.y>5)pn.moved=true;
   int delta=pn.last_y-c.y;
   if(pn.pressed_action==PN_PICK)changed|=pn_picker_contact(c.y,false,true,false);
   else if(pn.moved&&pn.total>pn_visible()&&pn.start_y>=pn.top&&pn.start_y<pn.bottom) {
    while(delta>=pn.row||delta<=-pn.row) {
     int step=delta>0?1:-1;changed|=pn_scroll(step);pn.last_y-=step*pn.row;delta=pn.last_y-c.y;
    }
   } else if(target!=pn.pressed)pn.pressed=-1;
  }
 }
 if(pn.down&&(c.down||c.released)&&(c.x-pn.start_x>5||pn.start_x-c.x>5||c.y-pn.start_y>5||pn.start_y-c.y>5))pn.moved=true;
 if(pn.down&&c.released&&pn.pressed_action==PN_PICK)
  changed|=pn_picker_contact(c.y,false,false,true);
 if(input->tapped) {
  int target=pn_target(input->touch_x,input->touch_y);
  if(pn.down&&!pn.moved&&target>=0&&target==pn.pressed) {
   pn_activate(pn.controls[target].action,pn.controls[target].arg,input);changed=true;
  }
  input->tapped=false;pn.down=false;pn.pressed=-1;
 } else if(!c.down){pn.down=false;pn.pressed=-1;}
navigation:
 if(page==PAGE_CUSTOM_KEYBOARD)changed|=nova7_buttons(input->buttons);
 else {
  if(input->buttons&T5_APP_BUTTON_UP)changed|=p7_picker_page()?p7_picker_step(-1):pn_scroll(-1);
  if(input->buttons&T5_APP_BUTTON_DOWN)changed|=p7_picker_page()?p7_picker_step(1):pn_scroll(1);
  if(p7_picker_page()&&(input->buttons&(T5_APP_BUTTON_LEFT|T5_APP_BUTTON_RIGHT))) {
   p7_picker.column=(input->buttons&T5_APP_BUTTON_RIGHT)?1:0;
   changed|=p7_picker_step((input->buttons&T5_APP_BUTTON_RIGHT)?1:-1);
  }
 }
 changed|=p7_picker_tick(app->millis());
 return changed;
}
