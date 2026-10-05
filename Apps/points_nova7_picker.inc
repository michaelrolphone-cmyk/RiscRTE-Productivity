/* Pixel-scrolling, independently captured time/duration wheels. Keep the
 * selected row inside the content viewport, below the header and above Back.
 * Face 4 is the real large Orbitron numeric face; both digits fit in 80px. */
#define P7_PICKER_TOP 100
#define P7_PICKER_BOTTOM 196
#define P7_PICKER_ROW 36
#define P7_PICKER_TEXT_Y 126
#define P7_PICKER_WIDTH 80
static struct {
    bool active,dragged,settling;
    unsigned owner_page,column;
    int start_y,last_y,offset;
    uint32_t last_tick;
} p7_picker;

static void p7_picker_reset(void) { memset(&p7_picker,0,sizeof(p7_picker)); }
static bool p7_picker_active(void) { return p7_picker.active; }
static bool p7_picker_dragged(void) { return p7_picker.dragged; }
static bool p7_picker_page(void) { return page==PAGE_TIME||page==PAGE_DURATION; }
static int p7_picker_column_at(int x,int y) {
    if(y<P7_PICKER_TOP||y>=P7_PICKER_BOTTOM)return -1;
    if(x>=24&&x<112)return 0;
    if(x>=128&&x<216)return 1;
    return -1;
}
static void p7_picker_select(int x) { p7_picker.column=x<120?0u:1u; }
static bool p7_picker_value(unsigned column,int steps,unsigned *out) {
    int value;
    if(page==PAGE_TIME) {
        int modulus=column?60:24;
        value=(column?(int)draft.minute:(int)draft.hour)+steps;
        value%=modulus;if(value<0)value+=modulus;
    } else {
        int hours=draft.duration_minutes/60,minutes=draft.duration_minutes%60;
        value=(column?minutes:hours)+steps;
        int maximum=column?(hours==12?0:59):(minutes?11:12);
        if(value<0||value>maximum)return false;
    }
    *out=(unsigned)value;return true;
}
static bool p7_picker_move(unsigned column,int steps) {
    unsigned value;
    if(!p7_picker_page()||!steps||!p7_picker_value(column,steps,&value))return false;
    if(page==PAGE_TIME) {
        if(column)draft.minute=(uint8_t)value;else draft.hour=(uint8_t)value;
    } else {
        unsigned old=draft.duration_minutes;
        draft.duration_minutes=(uint16_t)(column?(old/60)*60+value:value*60+old%60);
        if(!draft.duration_minutes)draft.notify_end=draft.warn3=0;
    }
    return true;
}
static bool p7_picker_step(int direction) {
    p7_picker.offset=0;p7_picker.settling=false;
    return p7_picker_move(p7_picker.column,direction>0?1:-1);
}
static bool p7_picker_tap(int x,int y) {
    if(page==PAGE_TIME&&time_format==PORTABLE_TIME_FORMAT_12&&x>=24&&x<112&&y>=82&&y<100){draft.hour=(uint8_t)((draft.hour+12)%24);return true;}
    int column=p7_picker_column_at(x,y);if(column<0)return false;
    p7_picker.column=(unsigned)column;p7_picker.offset=0;p7_picker.settling=false;
    /* Glyphs occupy 135..156 in the selected 128..163 touch row. */
    int steps=(y-(P7_PICKER_TEXT_Y+20)+P7_PICKER_ROW/2);
    steps=steps>=0?steps/P7_PICKER_ROW:-((-steps+P7_PICKER_ROW-1)/P7_PICKER_ROW);
    return p7_picker_move((unsigned)column,steps);
}
static bool p7_picker_contact(const springboard_contact *c) {
    if(!p7_picker_page()) { bool changed=p7_picker.active||p7_picker.offset;p7_picker_reset();return changed; }
    if(!c||c->cancelled||!c->valid) {
        bool changed=p7_picker.active||p7_picker.offset;
        p7_picker.active=false;p7_picker.offset=0;p7_picker.settling=false;return changed;
    }
    if(c->began) {
        p7_picker.active=false;p7_picker.dragged=false;
        int column=p7_picker_column_at(c->x,c->y);if(column<0)return false;
        if(p7_picker.owner_page!=page||p7_picker.column!=(unsigned)column)p7_picker.offset=0;
        p7_picker.column=(unsigned)column;p7_picker.owner_page=page;
        p7_picker.start_y=p7_picker.last_y=c->y;p7_picker.active=true;
        p7_picker.settling=false;return true;
    }
    if(!p7_picker.active||p7_picker.owner_page!=page)return false;
    bool changed=false;
    if(c->down||c->released) {
        bool was_dragged=p7_picker.dragged;
        if(c->y-p7_picker.start_y>5||c->y-p7_picker.start_y< -5)p7_picker.dragged=true;
        int dy=c->y-(was_dragged?p7_picker.last_y:p7_picker.start_y);p7_picker.last_y=c->y;
        if(p7_picker.dragged&&dy) {
            p7_picker.offset+=dy;changed=true;
            while(p7_picker.offset>=P7_PICKER_ROW||p7_picker.offset<=-P7_PICKER_ROW) {
                int direction=p7_picker.offset<0?1:-1;
                if(!p7_picker_move(p7_picker.column,direction)){p7_picker.offset=0;break;}
                p7_picker.offset+=direction*P7_PICKER_ROW;
            }
            unsigned ignored;
            if(p7_picker.offset&&!p7_picker_value(p7_picker.column,p7_picker.offset<0?1:-1,&ignored))p7_picker.offset=0;
        }
    }
    if(c->released) {
        p7_picker.active=false;
        /* Rebase the value and residual together: the digits remain at exactly
         * the same pixels on release. Only the short settling motion recenters. */
        if(p7_picker.dragged&&(p7_picker.offset>=P7_PICKER_ROW/2||p7_picker.offset<=-P7_PICKER_ROW/2)) {
            int direction=p7_picker.offset<0?1:-1;
            if(p7_picker_move(p7_picker.column,direction))p7_picker.offset+=direction*P7_PICKER_ROW;
        }
        p7_picker.settling=p7_picker.offset!=0;p7_picker.last_tick=app->millis();changed=true;
    }
    return changed;
}
static bool p7_picker_tick(uint32_t now) {
    if(!p7_picker.settling)return false;
    if(!p7_picker_page()||p7_picker.owner_page!=page){p7_picker_reset();return false;}
    unsigned elapsed=(uint32_t)(now-p7_picker.last_tick);if(elapsed<12)return false;
    int distance=(int)(elapsed/12)*3;if(distance>18)distance=18;
    p7_picker.last_tick=now;
    if(p7_picker.offset>0)p7_picker.offset=p7_picker.offset>distance?p7_picker.offset-distance:0;
    else p7_picker.offset=p7_picker.offset< -distance?p7_picker.offset+distance:0;
    p7_picker.settling=p7_picker.offset!=0;return true;
}
static void p7_picker_draw(bool duration) {
    portable_nova_begin();
    for(unsigned column=0;column<2;column++) {
        int offset=p7_picker.owner_page==page&&p7_picker.column==column?p7_picker.offset:0;
        for(int j=-2;j<=2;j++) {
            unsigned value;if(!p7_picker_value(column,j,&value))continue;
            if(!duration&&!column&&time_format!=PORTABLE_TIME_FORMAT_24)value=value%12?value%12:12;
            char text[4];snprintf(text,sizeof(text),"%02u",value);
            int y=P7_PICKER_TEXT_Y+j*P7_PICKER_ROW+offset;
            if(y+31<=P7_PICKER_TOP||y+9>=P7_PICKER_BOTTOM)continue;
            portable_nova_center(4,column?130:30,y,P7_PICKER_WIDTH,text,j?NOVA_CAP:NOVA_CYAN);
        }
    }
    /* The renderer has a horizontal text clip, but no viewport API. Paint
     * outside the wheel viewport before drawing the stationary chrome. */
    portable_nova_fill(0,0,240,P7_PICKER_TOP,0);
    portable_nova_fill(0,P7_PICKER_BOTTOM,240,240-P7_PICKER_BOTTOM,0);
    p7_header(duration?"DURATION":"TIME",duration?"END OF THIS POINT":"DRAG OR TAP A COLUMN",0);
    const char *left=duration?"HOURS":time_format==PORTABLE_TIME_FORMAT_24?"HOUR 24H":draft.hour<12?"HOUR AM":"HOUR PM";
    portable_nova_center(3,24,85,88,left,NOVA_CAP);
    portable_nova_center(3,128,85,88,duration?"MINUTES":"MINUTE",NOVA_CAP);
    portable_nova_center(4,110,P7_PICKER_TEXT_Y,20,":",NOVA_CYAN);
    portable_nova_fill(24,128,192,1,NOVA_DIM);
    portable_nova_fill(24,164,192,1,NOVA_DIM);
    p7_back();
}
static void p7_time(void) { p7_picker_draw(false); }
static void p7_duration(void) { p7_picker_draw(true); }
