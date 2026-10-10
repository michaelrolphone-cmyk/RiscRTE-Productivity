/* 480x800 e-paper Points design. Coordinates are baseline-aligned to the
 * supplied NOVA-7 reference; the controller and file writer are shared. */
#include "../lib/PointsCatalog/fonts/text.inc"
#include "PortableRaster.h"
static bool pcp_clip_content,pcp_replaying;
static int pcp_first,pcp_last=800;
enum {PCP_ROUND,PCP_DISK,PCP_STROKE,PCP_FADE};
typedef struct {int v[6];unsigned kind;bool black,clip;} pcp_shape_snapshot;
static void pcp_shape_replay(const void *,int,int);
static bool pcp_record_shape(unsigned kind,int x,int y,int w,int h,int r,bool black,int top,int bottom) {
    pcp_shape_snapshot s={{x,y,w,h,r,0},kind,black,pcp_clip_content};
    return portable_raster_record_draw(pcp_shape_replay,&s,sizeof(s),top,bottom);
}
static void pcp_rows(int origin,int *first,int *last) {
    if(!pcp_replaying)return;
    if(*first<pcp_first-origin)*first=pcp_first-origin;
    if(*last>pcp_last-origin)*last=pcp_last-origin;
}
static portable_touch_scroll pcp_types_scroll;
static int pcp_types_completed,pcp_types_pending,pcp_types_hit=-1;
static bool pcp_types_submitted,pcp_types_hit_edit;
static void pcp_types_configure(void) {
    portable_scroll_configure(&pcp_types_scroll,(portable_scroll_viewport){24,122,432,646},80,(int)editor.store.saved.type_count+1);
}
static void pc_edit_action(unsigned row);
static void pc_custom_action(unsigned row);
static void pc_select_type(unsigned at,bool edit);
static void pc_activate(unsigned index,bool right);
static void pcp_fill(int x,int y,int w,int h,bool black) {
    if(!pcp_replaying&&!pc_live())return;
    if(x<0){w+=x;x=0;}if(y<0){h+=y;y=0;}
    if(pcp_clip_content&&y<118){h-=118-y;y=118;}
    if(x+w>480)w=480-x;
    if(y+h>800)h=800-y;
    if(w>0&&h>0)app->fill_rect(x,y,w,h,black);
}
static void pcp_round(int x,int y,int w,int h,int r,bool black) {
    if(pcp_record_shape(PCP_ROUND,x,y,w,h,r,black,y,y+h))return;
    if(r>w/2)r=w/2;
    if(r>h/2)r=h/2;
    int first=0,last=h;pcp_rows(y,&first,&last);
    for(int row=first;row<last;row++) {
        int dy=row<r?2*r-2*row-1:row>=h-r?2*(row-h+r)+1:0,inset=0;
        if(dy)while(inset<r){int dx=2*r-2*inset-1;if(dx*dx+dy*dy<=4*r*r)break;inset++;}
        pcp_fill(x+inset,y+row,w-2*inset,1,black);
    }
}
static void pcp_box(int x,int y,int w,int h,int r,bool selected,int stroke) {
    int inside=stroke/2,outside=stroke-inside;pcp_round(x-outside,y-outside,w+2*outside,h+2*outside,r+outside,true);
    if(!selected)pcp_round(x+inside,y+inside,w-2*inside,h-2*inside,r-inside>0?r-inside:0,false);
}
static void pcp_disk(int x,int y,int radius,bool black) {
    if(pcp_record_shape(PCP_DISK,x,y,radius,0,0,black,y-radius,y+radius+1))return;
    int first=-radius,last=radius+1;pcp_rows(y,&first,&last);
    for(int row=first;row<last;row++){int span=0;while((span+1)*(span+1)+row*row<=radius*radius)span++;pcp_fill(x-span,y+row,2*span+1,1,black);}
}
static void pcp_stroke(int x1,int y1,int x2,int y2,int width,bool black) {
    if(pcp_record_shape(PCP_STROKE,x1,y1,x2,y2,width,black,(y1<y2?y1:y2)-width/2,(y1>y2?y1:y2)+width/2+1))return;
    int dx=abs(x2-x1),dy=-abs(y2-y1),sx=x1<x2?1:-1,sy=y1<y2?1:-1,error=dx+dy;
    for(;;){pcp_disk(x1,y1,width/2,black);if(x1==x2&&y1==y2)break;int twice=2*error;if(twice>=dy){error+=dy;x1+=sx;}if(twice<=dx){error+=dx;y1+=sy;}}
}
static void pcp_rule(int y,int stroke){pcp_fill(32,y-stroke/2,416,stroke,true);}
static void pcp_chevron(int x,int y,bool black){pcp_stroke(x-4,y-8,x+4,y,4,black);pcp_stroke(x+4,y,x-4,y+8,4,black);}
static void pcp_plus(int x,int y,int half,bool black){pcp_stroke(x-half,y,x+half,y,4,black);pcp_stroke(x,y-half,x,y+half,4,black);}
static unsigned pcp_shape(uint32_t id) {
    const points_catalog_type *t=pc_type(id);return t?t->symbol:0;
}
static void pcp_symbol(unsigned shape,int x,int y,bool black) {
    switch(shape%8) {
        case 0:pcp_disk(x,y,11,black);break;
        case 1:pcp_fill(x-10,y-10,20,20,black);break;
        case 2:for(int r=0;r<=22;r++){int w=r*24/22;pcp_fill(x-w/2,y-12+r,w,1,black);}break;
        case 3:for(int r=-13;r<=13;r++)pcp_fill(x-13+abs(r),y+r,26-2*abs(r),1,black);break;
        case 4:pcp_fill(x-3,y-12,6,24,black);pcp_fill(x-12,y-3,24,6,black);break;
        case 5:pcp_fill(x-4,y-12,8,24,black);break;
        case 6:pcp_disk(x,y,11,black);pcp_disk(x,y,7,!black);break;
        case 7:for(int r=-11;r<=0;r++){int span=0;while((span+1)*(span+1)+r*r<=121)span++;pcp_fill(x-span,y+2+r,2*span+1,1,black);}break;
    }
}
static bool pcp_case_sensitive;
static unsigned pcp_char(unsigned c){if(!pcp_case_sensitive&&c>='a'&&c<='z')c-=32;return c>=32&&c<=126?c:'?';}
static int pcp_kern(const pc_font *f,unsigned a,unsigned b) {
    unsigned key=a*128+b,lo=0,hi=f->kern_count;
    while(lo<hi){unsigned mid=lo+(hi-lo)/2;unsigned value=f->kern[mid].first*128u+f->kern[mid].second;if(value<key)lo=mid+1;else hi=mid;}
    return lo<f->kern_count&&f->kern[lo].first==a&&f->kern[lo].second==b?f->kern[lo].adjust:0;
}
static int pcp_measure(const pc_font *f,const char *s,int tracking) {
    int n=0;unsigned prev=0;
    for(unsigned i=0;s[i]&&i<96;i++){unsigned ch=pcp_char((unsigned char)s[i]);if(prev)n+=pcp_kern(f,prev,ch)+tracking*64;n+=f->glyphs[ch-32].advance;prev=ch;}
    return n;
}
static bool pcp_bit(const pc_glyph *g,int x,int y) {
    if(x<0||y<0||x>=g->width||y>=g->height)return false;
    unsigned bit=(unsigned)y*g->width+(unsigned)x;return !!(g->bits[bit/8]&(128u>>(bit%8)));
}
/* Exact physical-size text; width fitting affects horizontal glyphs only,
 * matching the reference's textLength rule. align: left0, center1, right2. */
typedef struct {const pc_font *font;int16_t x,baseline,tracking,align,limit;bool black,outline,clip,literal;char text[97];} pcp_text_snapshot;
_Static_assert(sizeof(pcp_text_snapshot)<=129,"Raster text payload must fit its copied command");
static void pcp_text_replay(const void *,int,int);
static void pcp_text(const pc_font *f,int x,int baseline,const char *s,int tracking,int align,int limit,bool black,bool outline) {
    if(!pcp_replaying) {
        pcp_text_snapshot snapshot={.font=f,.x=x,.baseline=baseline,.tracking=tracking,.align=align,.limit=limit,
            .black=black,.outline=outline,.clip=pcp_clip_content,.literal=pcp_case_sensitive};
        int top=baseline,bottom=baseline;
        for(unsigned i=0;i<96&&s[i];i++) {
            snapshot.text[i]=s[i];const pc_glyph *g=&f->glyphs[pcp_char((unsigned char)s[i])-32];
            if(baseline+g->top-2<top)top=baseline+g->top-2;
            if(baseline+g->top+g->height+2>bottom)bottom=baseline+g->top+g->height+2;
        }
        if(portable_raster_record_draw(pcp_text_replay,&snapshot,sizeof(snapshot),top,bottom))return;
    }
    if(!pc_live())return;
    int total=pcp_measure(f,s,tracking);if(total<=0)return;
    int width=(total+32)/64;if(limit&&width>limit)width=limit;
    int origin=x-(align==1?width/2:align==2?width:0),pen=0;unsigned prev=0;
    for(unsigned i=0;s[i]&&i<96;i++) {
        unsigned ch=pcp_char((unsigned char)s[i]);const pc_glyph *g=&f->glyphs[ch-32];
        if(prev)pen+=pcp_kern(f,prev,ch)+tracking*64;
        int border=outline?2:0;
        int first=-border,last=g->height+border;pcp_rows(baseline+g->top,&first,&last);
        for(int row=first;row<last;row++) {
            for(int col=-border;col<g->width+border;) {
                bool ink=pcp_bit(g,col,row);
                if(outline){
                    /* Center a three-pixel one-bit contour on the glyph edge,
                     * including its inner boundary like SVG stroke-width2.5. */
                    bool near=false,inside=true;
                    for(int dy=-2;dy<=2;dy++)for(int dx=-2;dx<=2;dx++){
                        bool bit=pcp_bit(g,col+dx,row+dy);if(dx*dx+dy*dy<=4)near|=bit;
                        if(abs(dx)<=1&&abs(dy)<=1)inside&=bit;
                    }
                    ink=near&&!inside;
                }
                if(!ink){col++;continue;}
                int begin=col++;
                if(!outline)while(col<g->width&&pcp_bit(g,col,row))col++;
                int left=((pen+(g->left+begin)*64)*width+total/2)/total;
                int right=((pen+(g->left+col)*64)*width+total/2)/total;
                if(right>left)pcp_fill(origin+left,baseline+g->top+row,right-left,1,black);
            }
        }
        pen+=g->advance;prev=ch;
    }
}
static void pcp_fade(int h) {
    if(pcp_record_shape(PCP_FADE,h,0,0,0,0,false,800-h,800))return;
    bool old=pcp_clip_content;pcp_clip_content=false;
    int start=800-h;
    int first=start,last=800;pcp_rows(0,&first,&last);
    for(int y=first;y<last;y++) {
        unsigned band=(unsigned)(y-start)*4u/(unsigned)h;
        if(band>=3){pcp_fill(0,y,480,1,false);continue;}
        if(band==0){if(y%2==0)for(int x=0;x<480;x+=2)pcp_fill(x,y,1,1,false);}
        else if(band==1){for(int x=y%2;x<480;x+=2)pcp_fill(x,y,1,1,false);}
        else if(y%2==0)pcp_fill(0,y,480,1,false);
        else for(int x=0;x<480;x+=2)pcp_fill(x,y,1,1,false);
    }
    pcp_clip_content=old;
}
/* Payloads are copied when recording: text, clip and case decisions cannot
 * follow later editor changes. Only immutable font tables are referenced. */
static void pcp_shape_replay(const void *payload,int first,int last) {
    pcp_shape_snapshot s;memcpy(&s,payload,sizeof(s));
    bool clip=pcp_clip_content;pcp_clip_content=s.clip;pcp_replaying=true;pcp_first=first;pcp_last=last;
    switch(s.kind) {
        case PCP_ROUND:pcp_round(s.v[0],s.v[1],s.v[2],s.v[3],s.v[4],s.black);break;
        case PCP_DISK:pcp_disk(s.v[0],s.v[1],s.v[2],s.black);break;
        case PCP_STROKE:pcp_stroke(s.v[0],s.v[1],s.v[2],s.v[3],s.v[4],s.black);break;
        case PCP_FADE:pcp_fade(s.v[0]);break;
    }
    pcp_replaying=false;pcp_clip_content=clip;
}
static void pcp_text_replay(const void *payload,int first,int last) {
    pcp_text_snapshot s;memcpy(&s,payload,sizeof(s));
    bool clip=pcp_clip_content,literal=pcp_case_sensitive;
    pcp_clip_content=s.clip;pcp_case_sensitive=s.literal;pcp_replaying=true;pcp_first=first;pcp_last=last;
    pcp_text(s.font,s.x,s.baseline,s.text,s.tracking,s.align,s.limit,s.black,s.outline);
    pcp_replaying=false;pcp_clip_content=clip;pcp_case_sensitive=literal;
}
static void pcp_back_button(void) {
    pcp_disk(56,46,28,true);pcp_disk(56,46,24,false);pcp_stroke(58,37,50,46,4,true);pcp_stroke(50,46,58,55,4,true);
}
static void pcp_header(const char *title,const char *sub,bool symbol) {
    if(page==PC_EDIT||page==PC_TYPES||page==PC_CUSTOM)pcp_back_button();
    pcp_text(&PCF_O900_26,(page==PC_EDIT||page==PC_TYPES||page==PC_CUSTOM)?96:32,56,title,page==PC_TIME?4:2,0,(page==PC_EDIT||page==PC_TYPES||page==PC_CUSTOM)?352:380,true,false);
    if(symbol){pcp_symbol(pcp_shape(editor.event.type_id),44,88,true);pcp_text(&PCF_R700_22,66,96,sub,3,0,382,true,false);pcp_rule(112,4);}
    else {pcp_text(&PCF_R700_21,32,92,sub,3,0,416,true,false);pcp_rule(108,4);}
}
static const char *pcp_type_name(uint32_t id){return id==POINTS_WORK_START?"START WORK":id==POINTS_WORK_END?"END WORK":id==POINTS_BEDTIME?"WIND DOWN":pc_name(id);}
static void pcp_time_text(const points_catalog_item *e,char *s,size_t n) {
    snprintf(s,n,"%u:%02u",time_format==PORTABLE_TIME_FORMAT_24?e->hour:e->hour%12?e->hour%12:12,e->minute);
}
static void pcp_list_view(void) {
    char sub[100],time[24];unsigned active=0;
    for(uint32_t i=0;i<editor.store.saved.event_count;i++)active+=editor.store.saved.events[i].enabled;
    const points_catalog_item *next=catalog_projection.count?points_catalog_find_event(&editor.store.saved,catalog_projection.next[0].event_id):NULL;
    if(next){pcp_time_text(next,time,sizeof(time));snprintf(sub,sizeof(sub),"%u ACTIVE - NEXT %s %s",active,pcp_type_name(next->type_id),time);}
    else snprintf(sub,sizeof(sub),"%u ACTIVE - %u POINTS",active,editor.store.saved.event_count);
    pcp_text(&PCF_O900_26,32,56,"POINTS IN TIME",1,0,330,true,false);
    pcp_disk(420,46,26,true);pcp_disk(420,46,22,false);pcp_plus(420,46,8,true);
    pcp_text(&PCF_R700_21,32,92,sub,2,0,416,true,false);pcp_rule(108,4);
    pcp_clip_content=true;
    for(unsigned row=0;row<9;row++) {
        const points_catalog_item *e=points_editor_at(&editor,first+row);if(!e)break;
        int y=122+(int)row*80;bool selected=next&&next->id==e->id,black=!selected;
        if(selected)pcp_round(24,y,432,72,12,true);
        else if(!e->enabled) {
            /* Dashed rounded outline is a one-bit disabled state. */
            pcp_box(28,y+2,424,68,12,false,3);
            for(int xx=28;xx<452;xx+=14){pcp_fill(xx+8,y,6,5,false);pcp_fill(xx+8,y+68,6,5,false);}
            for(int yy=y+14;yy<y+62;yy+=14){pcp_fill(26,yy+8,5,6,false);pcp_fill(450,yy+8,5,6,false);}
        }
        pcp_symbol(pcp_shape(e->type_id),56,y+36,black);
        pcp_time_text(e,time,sizeof(time));pcp_text(&PCF_O700_28,92,y+46,time,0,0,124,black,false);
        if(time_format==PORTABLE_TIME_FORMAT_12)pcp_text(&PCF_R700_14,92,y+64,e->hour<12?"AM":"PM",1,0,100,black,false);
        pcp_text(&PCF_R700_28,222,y+34,pcp_type_name(e->type_id),1,0,170,black,false);
        char days[32],detail[64];points_editor_days(e->weekdays,days,sizeof(days));
        if(e->weekdays==127)strcpy(days,"DAILY");
        if(e->duration_minutes)snprintf(detail,sizeof(detail),"%u MIN - %s",e->duration_minutes,days);else snprintf(detail,sizeof(detail),"%s",days);
        pcp_text(&PCF_R600_19,222,y+60,detail,2,0,170,black,false);pcp_chevron(430,y+36,black);
        if(!selected&&e->enabled)pcp_rule(y+76,2);
    }
    pcp_clip_content=false;
    if(!editor.order_count)pcp_text(&PCF_R700_23,32,180,"NO POINTS YET - TAP +",2,0,416,true,false);
    pcp_fade(130);
}
static void pcp_types_view(void) {
    pcp_types_configure();pcp_header("POINT TYPE","CHOOSE TYPE - RIGHT EDGE EDITS",false);
    pcp_clip_content=true;int offset=portable_scroll_offset(&pcp_types_scroll);
    unsigned start=(unsigned)offset/80;
    for(unsigned i=start;i<pc_count()&&i<start+10;i++) {
        int y=122+(int)i*80-offset;bool custom=i==editor.store.saved.type_count;
        const points_catalog_type *t=custom?NULL:&editor.store.saved.types[i];
        bool selected=t&&editor.editing_event&&editor.event.type_id==t->id,black=!selected;
        if(selected)pcp_round(24,y,432,72,12,true);
        if(custom)pcp_plus(56,y+36,10,black);else pcp_symbol(t->symbol,56,y+36,black);
        pcp_text(&PCF_R700_28,92,y+34,custom?"NEW CUSTOM TYPE":t->name,1,0,310,black,false);
        char detail[64];
        if(custom)strcpy(detail,"NAME - SYMBOL - DEFAULTS");
        else if(t->flags&POINTS_TYPE_DURATION)snprintf(detail,sizeof(detail),"%u MIN - %s",t->duration_minutes,mode_names[t->mode]);
        else snprintf(detail,sizeof(detail),"%s",mode_names[t->mode]);
        pcp_text(&PCF_R600_19,92,y+60,detail,2,0,310,black,false);pcp_chevron(430,y+36,black);
        if(!selected)pcp_rule(y+76,2);
    }
    pcp_clip_content=false;pcp_fade(64);
}
static void pcp_check(int y,bool checked) {
    pcp_box(396,y+11,40,40,8,checked,4);
    if(checked){pcp_stroke(405,y+32,413,y+39,4,false);pcp_stroke(413,y+39,430,y+23,4,false);}
}
static void pcp_edit_row(unsigned row,const char *name,const char *value,int checked) {
    int y=122+(int)row*62-pcp_edit_offset;pcp_rule(y+62,2);
    pcp_text(&PCF_R700_23,32,y+40,name,2,0,230,true,false);
    if(value){pcp_text(&PCF_O700_23,404,y+40,value,0,2,150,true,false);pcp_chevron(436,y+32,true);}
    if(checked>=0)pcp_check(y,checked!=0);
}
static void pcp_action_button(int x,int y,int w,const char *s,bool selected) {
    pcp_box(x,y,w,48,24,selected,3);pcp_text(&PCF_R700_20,x+w/2,y+32,s,3,1,w-24,!selected,false);
}
static void pcp_days_inline(int dy) {
    pcp_text(&PCF_R700_20,32,dy+34,"DAYS",4,0,200,true,false);
    static const char *const letters[]={"S","M","T","W","T","F","S"};
    for(unsigned i=0;i<7;i++){int x=32+(int)i*60;bool on=!!(editor.event.weekdays&(1u<<i));pcp_box(x,dy+44,52,52,10,on,4);pcp_text(&PCF_O700_22,x+26,dy+79,letters[i],0,1,44,!on,false);}
    const char *const labels[]={"EVERY DAY","MON-FRI","WEEKEND"};
    for(unsigned i=0;i<3;i++){int x=32+(int)i*146;pcp_box(x,dy+106,124,38,19,false,3);pcp_text(&PCF_R700_16,x+62,dy+131,labels[i],1,1,116,true,false);}
}
static void pcp_edit_view(void) {
    const char *subtitle=editor.error?pc_error():pcp_type_name(editor.event.type_id);pcp_header(pc_edit_title(),subtitle,!editor.error);
    pcp_clip_content=true;char value[48];
    pcp_edit_row(0,"TYPE",pcp_type_name(editor.event.type_id),-1);pcp_time_text(&editor.event,value,sizeof(value));pcp_edit_row(1,"TIME",value,-1);
    if(!pc_duration())strcpy(value,"NONE");else if(editor.event.duration_minutes&&editor.event.duration_minutes%60==0)snprintf(value,sizeof(value),"%u H",editor.event.duration_minutes/60);else snprintf(value,sizeof(value),"%u MIN",editor.event.duration_minutes);
    pcp_edit_row(2,"DURATION",value,-1);pcp_edit_row(3,"NOTIFY AT END",NULL,editor.event.notify_end);pcp_edit_row(4,"3 MIN WARNING",NULL,editor.event.warn3);
    const char *const compact_modes[]={"SYSTEM","VIBRATE","SOUND","BOTH"};pcp_edit_row(5,"NOTIFY",pc_visual_only()?"VISUAL":compact_modes[editor.event.mode],-1);
    int dy=494-pcp_edit_offset;pcp_days_inline(dy);pcp_rule(dy+160,2);
    pcp_text(&PCF_R700_23,32,dy+204,"ENABLED",2,0,230,true,false);pcp_box(354,dy+172,92,44,22,false,4);
    pcp_box(360,dy+178,38,32,16,!editor.event.enabled,2);pcp_box(402,dy+178,38,32,16,editor.event.enabled,2);
    pcp_text(&PCF_O700_14,379,dy+200,"OFF",0,1,34,!!editor.event.enabled,false);pcp_text(&PCF_O700_14,421,dy+200,"ON",0,1,32,!editor.event.enabled,false);
    pcp_text(&PCF_R700_23,32,dy+262,delete_confirm?"TAP AGAIN TO DELETE":"DELETE POINT",2,0,416,true,false);
    pcp_action_button(32,810-pcp_edit_offset,200,"CANCEL",false);pcp_action_button(248,810-pcp_edit_offset,200,"SAVE",true);
    pcp_clip_content=false;pcp_fade(84);
}
static void pcp_custom_view(void) {
    pcp_header("TYPE DEFAULTS",editor.type.name[0]?editor.type.name:"NAME AND SYMBOL",false);
    const char *const labels[]={"NAME","STYLE","USE DURATION","DURATION","NOTIFY","NOTIFY AT END","3 MIN WARNING"};
    for(unsigned row=0;row<7;row++) {
        int y=122+(int)row*62;char value[48];const char *text=value;int checked=-1;
        if(row==0)snprintf(value,sizeof(value),"%s",editor.type.name[0]?editor.type.name:"SET NAME");
        else if(row==1)snprintf(value,sizeof(value),"#%06X",(unsigned)editor.type.color);
        else if(row==2){text=NULL;checked=!!(editor.type.flags&POINTS_TYPE_DURATION);}
        else if(row==3)snprintf(value,sizeof(value),"%u MIN",editor.type.duration_minutes);
        else if(row==4)snprintf(value,sizeof(value),"%s",mode_names[editor.type.mode]);
        else {text=NULL;checked=!!(editor.type.flags&(row==5?POINTS_TYPE_NOTIFY_END:POINTS_TYPE_WARN3));}
        pcp_rule(y+62,2);pcp_text(&PCF_R700_23,32,y+40,labels[row],2,0,230,true,false);
        if(text){pcp_text(&PCF_O700_23,404,y+40,text,0,2,150,true,false);pcp_chevron(436,y+32,true);}
        if(checked>=0)pcp_check(y,checked!=0);
    }
    pcp_text(&PCF_R700_23,32,624,delete_confirm?"TAP AGAIN TO DELETE":"DELETE TYPE",2,0,416,true,false);
    if(editor.error||(notice&&*notice))pcp_text(&PCF_R600_19,32,682,pc_error(),1,0,416,true,false);
    pcp_action_button(32,716,200,"CANCEL",false);pcp_action_button(248,716,200,"SAVE TYPE",true);
}
static void pcp_time_view(void) {
    pcp_header(page==PC_TIME?"TIME":"DURATION",return_page==PC_CUSTOM?editor.type.name:pcp_type_name(editor.event.type_id),false);
    unsigned duration=return_page==PC_CUSTOM?editor.type.duration_minutes:editor.event.duration_minutes;
    for(unsigned col=0;col<2;col++) {
        int x=col?350:130;unsigned value=page==PC_TIME?(col?editor.event.minute:editor.event.hour):(col?duration%60:duration/60);
        unsigned mod=col?60:page==PC_TIME?24:13,step=col?5:1;char current[8],before[8],after[8];
        snprintf(current,sizeof(current),col?"%02u":"%u",value);
        snprintf(before,sizeof(before),col?"%02u":"%u",(value+mod-step)%mod);snprintf(after,sizeof(after),col?"%02u":"%u",(value+step)%mod);
        pcp_box(x-76,128,152,92,18,false,4);pcp_plus(x,174,14,true);
        pcp_text(&PCF_R700_18,x,254,col?"MINUTES":"HOURS",3,1,160,true,false);
        pcp_text(&PCF_O700_52,x,332,before,0,1,162,true,true);
        pcp_round(x-84,358,168,150,18,true);pcp_text(&PCF_O900_88,x,462,current,0,1,156,false,false);
        pcp_text(&PCF_O700_52,x,568,after,0,1,162,true,true);
        pcp_box(x-76,596,152,92,18,false,4);pcp_stroke(x-14,642,x+14,642,4,true);
    }
    pcp_text(&PCF_O700_80,240,462,":",0,1,44,true,false);
    pcp_action_button(32,716,200,"CANCEL",false);pcp_action_button(248,716,200,"DONE",true);
}
static void pc_paper_page_reset(void){
    pcp_types_scroll=(portable_touch_scroll){0};pcp_types_completed=pcp_types_pending=0;pcp_types_hit=-1;pcp_types_submitted=false;
}
static void pcp_draw(void) {
    paper->begin();if(!pc_live())return;pcp_clip_content=false;
    if(!ready||!editor.store.loaded||!editor.index_ready||editor.store.uncertain||editor.conflict) {
        pcp_header("POINTS IN TIME",pc_error(),false);pcp_text(&PCF_R700_23,32,180,editor.store.uncertain?"CHECK READS THE SAVED FILE":"SAVED POINTS ARE PRESERVED",1,0,416,true,false);
        pcp_action_button(32,716,200,"BACK",false);pcp_action_button(248,716,200,editor.store.uncertain?"CHECK SAVE":"RETRY",true);
    } else if(page==PC_LIST)pcp_list_view();
    else if(page==PC_EDIT)pcp_edit_view();
    else if(page==PC_TIME||page==PC_DURATION)pcp_time_view();
    else if(page==PC_DAYS){pcp_header("WEEKDAYS","SUNDAY THROUGH SATURDAY",false);pcp_days_inline(160);pcp_action_button(32,716,200,"CANCEL",false);pcp_action_button(248,716,200,"DONE",true);}
    else if(page==PC_TYPES)pcp_types_view();
    else if(page==PC_CUSTOM)pcp_custom_view();
    else if(page==PC_MODE){pcp_header("ALERT MODE","CHOOSE THE SAVED MODE",false);for(unsigned i=0;i<4;i++){bool on=(return_page==PC_CUSTOM?editor.type.mode:editor.event.mode)==i;pcp_box(32,140+(int)i*108,416,88,16,on,4);pcp_text(&PCF_R700_23,240,195+(int)i*108,mode_names[i],2,1,384,!on,false);}pcp_action_button(32,716,200,"CANCEL",false);pcp_action_button(248,716,200,"DONE",true);}
    else if(page==PC_COLOR){pcp_header("TYPE STYLE","COLOR AND SYMBOL",false);for(unsigned i=0;i<8;i++){int x=32+(int)(i%4)*108,y=160+(int)(i/4)*120;bool on=editor.type.color==palette[i];pcp_box(x,y,92,96,16,on,4);char color[8];snprintf(color,sizeof(color),"%06X",(unsigned)palette[i]);pcp_text(&PCF_R700_18,x+46,y+54,color,0,1,84,!on,false);}for(unsigned i=0;i<8;i++){int x=32+(int)(i%4)*108,y=424+(int)(i/4)*108;bool on=editor.type.symbol==i;pcp_box(x,y,92,88,12,on,4);pcp_symbol(i,x+46,y+44,!on);}pcp_action_button(32,716,200,"CANCEL",false);pcp_action_button(248,716,200,"DONE",true);}
    if(!pc_live())return;
    app->present(clean_frame);if(!pc_live())return;if(page==PC_TYPES){pcp_types_pending=portable_scroll_offset(&pcp_types_scroll);pcp_types_submitted=true;}dirty=false;clean_frame=false;
}
static void pcp_done(void){pc_page(return_page==PC_CUSTOM?PC_CUSTOM:PC_EDIT);dirty=true;}
static void pcp_days_tap(int x,int y,int dy) {
    if(y>=dy+44&&y<dy+96&&x>=32&&x<444){unsigned at=(unsigned)(x-32)/60;if(at<7&&(x-32)%60<52)editor.event.weekdays^=(uint8_t)(1u<<at);}
    else if(y>=dy+106&&y<dy+144&&x>=32&&x<448){unsigned at=(unsigned)(x-32)/146;if(at<3&&(x-32)%146<124)editor.event.weekdays=at==0?127:at==1?62:65;}
    dirty=true;
}
static void pcp_types_input(const springboard_contact *c) {
    pcp_types_configure();
    if(pcp_types_submitted&&paper_frame_idle()){pcp_types_completed=pcp_types_pending;pcp_types_submitted=false;}
    portable_touch_sample sample={.valid=c->valid,.down=c->down,.began=c->began,.released=c->released,.cancelled=c->cancelled,.tap_eligible=c->tap_eligible,.x=(uint16_t)c->x,.y=(uint16_t)c->y};
    if(c->began){portable_touch_scroll visible=pcp_types_scroll;visible.position_q8=pcp_types_completed*256;pcp_types_hit=portable_scroll_row(&visible,c->x,c->y);pcp_types_hit_edit=c->x>=400;}
    unsigned action=portable_scroll_touch(&pcp_types_scroll,&sample,app->millis());
    if(action&PORTABLE_SCROLL_CHANGED)dirty=true;
    if(action&PORTABLE_SCROLL_BACK){exit_app=pc_back();return;}
    if(action&PORTABLE_SCROLL_TAP) {
        if(pcp_types_hit>=0)pc_select_type((unsigned)pcp_types_hit,pcp_types_hit_edit);
        else if(c->x>=28&&c->x<84&&c->y>=18&&c->y<74)exit_app=pc_back();
        pcp_types_hit=-1;dirty=true;
    }
}
static void pcp_tap(int x,int y) {
    if(!pc_live())return;
    if(!ready||!editor.store.loaded||!editor.index_ready||editor.store.uncertain||editor.conflict) {
        if(y>=716&&y<764){if(x>=32&&x<232)exit_app=pc_back();else if(x>=248&&x<448)pc_retry();}return;
    }
    if(!points_editor_editable(&editor))return;
    if(page==PC_LIST) {
        if(x>=392&&x<448&&y>=18&&y<74){if(points_editor_begin_event(&editor,0)){pcp_edit_offset=0;notice="";pc_page(PC_EDIT);}return;}
        if(x>=24&&x<456&&y>=122&&y<800){const points_catalog_item *e=points_editor_at(&editor,first+(unsigned)(y-122)/80);if(e&&points_editor_begin_event(&editor,e->id)){pcp_edit_offset=0;notice="";pc_page(PC_EDIT);}}return;
    }
    if(page==PC_TYPES) {
        if(x>=28&&x<84&&y>=18&&y<74){exit_app=pc_back();return;}
        pcp_types_configure();int row=portable_scroll_row(&pcp_types_scroll,x,y);if(row>=0)pc_select_type((unsigned)row,x>=400);return;
    }
    if(page==PC_EDIT) {
        if(x>=28&&x<84&&y>=18&&y<74){exit_app=pc_back();return;}
        int content=y+pcp_edit_offset;if(y<118||x<28||x>=452)return;
        if(content>=122&&content<494){static const unsigned rows[]={0,1,3,4,5,6};pc_edit_action(rows[(unsigned)(content-122)/62]);}
        else if(content>=494&&content<654)pcp_days_tap(x,content,494);
        else if(content>=666&&content<712)pc_edit_action(7);
        else if(content>=730&&content<794)pc_edit_action(9);
        else if(content>=810&&content<858){if(x<232)exit_app=pc_back();else if(x>=248)pc_save(false);}
        dirty=true;return;
    }
    if(page==PC_CUSTOM) {
        if(x>=28&&x<84&&y>=18&&y<74){exit_app=pc_back();return;}
        if(y>=716&&y<764){if(x>=32&&x<232)exit_app=pc_back();else if(x>=248&&x<448)pc_save(false);return;}
        if(x>=28&&x<452&&y>=122&&y<556)pc_custom_action((unsigned)(y-122)/62);
        else if(x>=28&&x<452&&y>=588&&y<652)pc_custom_action(8);
        dirty=true;return;
    }
    if(page==PC_TIME||page==PC_DURATION) {
        if(y>=716&&y<764){if(x>=32&&x<232)exit_app=pc_back();else if(x>=248&&x<448)pcp_done();return;}
        int col=x>=54&&x<206?0:x>=274&&x<426?1:-1,direction=y>=128&&y<220?1:y>=596&&y<688?-1:0;
        if(col<0||!direction)return;
        if(page==PC_TIME){if(!col)editor.event.hour=(uint8_t)((editor.event.hour+24+direction)%24);else editor.event.minute=(uint8_t)((editor.event.minute+60+direction)%60);}
        else {
            uint16_t *minutes=return_page==PC_CUSTOM?&editor.type.duration_minutes:&editor.event.duration_minutes;
            int n=*minutes+direction*(col?1:60);*minutes=(uint16_t)(n<0?0:n>720?720:n);
            if(return_page==PC_CUSTOM){if(!*minutes)editor.type.flags&=~POINTS_TYPE_NOTIFY_END;if(*minutes<3)editor.type.flags&=~POINTS_TYPE_WARN3;}
            else {if(!*minutes)editor.event.notify_end=0;if(*minutes<3)editor.event.warn3=0;}
        }
        dirty=true;return;
    }
    if(page==PC_DAYS) {
        if(y>=716&&y<764){if(x>=32&&x<232)exit_app=pc_back();else if(x>=248&&x<448)pcp_done();}else pcp_days_tap(x,y,160);return;
    }
    if(page==PC_MODE) {
        if(y>=716&&y<764){if(x>=32&&x<232)exit_app=pc_back();else if(x>=248&&x<448)pcp_done();return;}
        if(x>=32&&x<448&&y>=140&&y<552&&(y-140)%108<88){unsigned i=(unsigned)(y-140)/108;if(i<4){if(return_page==PC_CUSTOM)editor.type.mode=(uint8_t)i;else if(!pc_visual_only())editor.event.mode=(uint8_t)i;dirty=true;}}return;
    }
    if(page==PC_COLOR) {
        if(y>=716&&y<764){if(x>=32&&x<232)exit_app=pc_back();else if(x>=248&&x<448)pcp_done();return;}
        if(x>=32&&x<448&&y>=160&&y<376){unsigned col=(unsigned)(x-32)/108,row=(unsigned)(y-160)/120;if(col<4&&row<2&&(x-32)%108<92&&(y-160)%120<96)editor.type.color=palette[row*4+col];dirty=true;}else if(x>=32&&x<448&&y>=424&&y<620){unsigned col=(unsigned)(x-32)/108,row=(unsigned)(y-424)/108;if(col<4&&row<2&&(x-32)%108<92&&(y-424)%108<88){editor.type.symbol=(uint8_t)(row*4+col);dirty=true;}}return;
    }
    /* Additional type/default editors use the existing shared row controller. */
    if(y>=pc_footer_top()&&y<pc_footer_top()+88&&x>=32&&x<448) {
        if(x<232){exit_app=pc_back();return;}
        if(x<248)return;
        if(page==PC_CUSTOM)pc_save(false);else if(page==PC_TYPES){unsigned max=pc_count()>pc_rows()?pc_count()-pc_rows():0;if(first==max)first=0;else pc_scroll(1);}dirty=true;return;
    }
    if(x>=32&&x<448&&y>=pc_top()&&y<pc_top()+(int)pc_rows()*pc_row_height())pc_activate(first+(unsigned)(y-pc_top())/(unsigned)pc_row_height(),x>=368);
    dirty=true;
}
