/* Production Points picker + production PortableNovaUi renderer and glyphs.
 * No fake text API: assertions compare actual RGB565 glyph pixels to the font
 * bitmaps and exercise the same contact handler used by app_main. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../Apps/points_in_time.c"
static uint16_t picker_pixels[244*240];
static struct {unsigned frame,stride_bytes;void *pixels;} surface={1,488,picker_pixels};
static bool list_mode;
static int width(void){return 240;}
static int height(void){return 240;}
static void clear_color(uint32_t color){assert(!color);for(unsigned y=0;y<240;y++)for(unsigned x=0;x<240;x++)picker_pixels[y*244+x]=0;}
#include "nova_ui.inc"
static uint32_t test_ticks;
static uint32_t test_millis(void){return test_ticks;}
static const t5_app_api_v1 test_api={.millis=test_millis};
const t5_app_api_v1 *t5_app_get_api(uint32_t version){assert(version==1);return &test_api;}
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){assert(version==1);return NULL;}
static void init(unsigned p) {
    memset(picker_pixels,0xa5,sizeof(picker_pixels));
    app=&test_api;page=p;ready=true;writer=(points_writer){.loaded=true};
    draft=(points_item){.kind=POINTS_LUNCH,.enabled=1};time_format=PORTABLE_TIME_FORMAT_24;
    p7_picker_reset();test_ticks=0;
}
static void guard(void){for(unsigned y=0;y<240;y++)for(unsigned x=240;x<244;x++)assert(picker_pixels[y*244+x]==0xa5a5);}
static unsigned measure_digits(unsigned value) {
    return (rps_text[4*95+'0'+value/10-32].advance_q4+rps_text[4*95+'0'+value%10-32].advance_q4+15)/16;
}
static uint16_t blend(uint16_t old,uint32_t rgb,unsigned alpha) {
    unsigned r=(((old>>11)&31)*255/31*(255-alpha)+((rgb>>16)&255)*alpha)/255;
    unsigned g=(((old>>5)&63)*255/63*(255-alpha)+((rgb>>8)&255)*alpha)/255;
    unsigned b=((old&31)*255/31*(255-alpha)+(rgb&255)*alpha)/255;
    return (uint16_t)((r>>3)<<11|(g>>2)<<5|(b>>3));
}
static void expected_digits(unsigned value,unsigned column,uint16_t expected[36][80]) {
    memset(expected,0,36*80*sizeof(uint16_t));
    int pen=(int)((80-measure_digits(value))/2)*16;
    unsigned digits[2]={value/10,value%10};
    for(unsigned digit=0;digit<2;digit++) {
        const rps_glyph *g=&rps_text[4*95+'0'+digits[digit]-32];unsigned ink=0;
        for(unsigned y=0;y<g->height;y++)for(unsigned x=0;x<g->width;x++) {
            unsigned n=y*g->width+x,a=(g->bits[n/4]>>(6-2*(n%4)))&3;
            int px=pen/16+g->left+(int)x,py=P7_PICKER_TEXT_Y+g->top+(int)y-129;
            assert(px>=0&&px<80&&py>=0&&py<36);
            if(a){expected[py][px]=blend(expected[py][px],NOVA_CYAN,a*85);ink++;}
        }
        assert(ink>15);pen+=g->advance_q4;
    }
    (void)column;
}
static void assert_selected(unsigned left,unsigned right) {
    for(unsigned column=0;column<2;column++) {
        uint16_t expected[36][80];expected_digits(column?right:left,column,expected);
        /* Selected glyphs end at156; the boundary rule is164. */
        for(unsigned y=0;y<35;y++)for(unsigned x=0;x<80;x++) {
            uint16_t actual=picker_pixels[(129+y)*244+(column?130:30)+x];
            if(actual!=expected[y][x]) {
                fprintf(stderr,"glyph mismatch page=%u value=%u column=%u at%u,%u: actual=%x expected=%x\n",page,column?right:left,column,x,y,actual,expected[y][x]);abort();
            }
        }
    }
    guard();
}
static void save_frame(const char *directory,const char *name) {
    if(!directory)return;
    char path[1024];snprintf(path,sizeof(path),"%s/%s.ppm",directory,name);
    FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n240 240\n255\n");
    for(unsigned y=0;y<240;y++)for(unsigned x=0;x<240;x++) {
        uint16_t p=picker_pixels[y*244+x];unsigned char rgb[]={(unsigned char)((p>>11)*255/31),(unsigned char)(((p>>5)&63)*255/63),(unsigned char)((p&31)*255/31)};
        assert(fwrite(rgb,1,3,f)==3);
    }
    assert(!fclose(f));
}
static springboard_contact contact(int x,int y,bool began,bool released) {
    return (springboard_contact){.valid=true,.down=!released,.began=began,.released=released,.tap_eligible=!released||!p7_picker_dragged(),.x=(int16_t)x,.y=(int16_t)y};
}
static void settle(void) {
    for(unsigned n=0;n<20&&p7_picker.settling;n++){test_ticks+=12;assert(p7_picker_tick(test_ticks));p7_picker_draw(page==PAGE_DURATION);}
    assert(!p7_picker.settling&&!p7_picker.offset);
}
static void mask_column(unsigned column,unsigned char mask[96][88]) {
    for(unsigned y=0;y<96;y++)for(unsigned x=0;x<88;x++)mask[y][x]=picker_pixels[(100+y)*244+(column?128:24)+x]!=0;
}
static void gesture(unsigned column,int pixels,bool cross) {
    int x=column?170:70;unsigned old_hour=draft.hour,old_minute=draft.minute;
    springboard_contact c=contact(x,146,true,false);assert(p7_picker_contact(&c));assert(p7_picker_active());
    /* Smaller than a row still has visible pixel movement. */
    c=contact(cross?(column?70:170):x,146+pixels,false,false);assert(p7_picker_contact(&c));
    p7_picker_draw(page==PAGE_DURATION);assert(p7_picker_dragged());
    unsigned char before[96][88],after[96][88];mask_column(column,before);
    c=contact(c.x,c.y,false,true);assert(p7_picker_contact(&c));assert(!p7_picker_active());
    p7_picker_draw(page==PAGE_DURATION);mask_column(column,after);
    assert(!memcmp(before,after,sizeof(before))); /* No positional jump at release. */
    if(page==PAGE_TIME){if(column)assert(draft.hour==old_hour);else assert(draft.minute==old_minute);}
    settle();
}
int main(int argc,char **argv) {
    const char *directory=argc>1?argv[1]:NULL;
    init(PAGE_TIME);
    /* Every hour x every minute in both display modes, including 00,10..19,
     * 23,59, midnight/noon and variable-width numeral1. */
    unsigned frames=0;
    for(unsigned format=0;format<2;format++)for(unsigned hour=0;hour<24;hour++)for(unsigned minute=0;minute<60;minute++) {
        time_format=format;draft.hour=(uint8_t)hour;draft.minute=(uint8_t)minute;p7_time();
        assert_selected(format?hour:hour%12?hour%12:12,minute);frames++;
    }
    time_format=PORTABLE_TIME_FORMAT_24;draft.hour=23;draft.minute=59;p7_time();save_frame(directory,"time-23-59");
    time_format=PORTABLE_TIME_FORMAT_12;draft.hour=12;draft.minute=11;p7_time();save_frame(directory,"time-12-11-pm");
    /* Each valid value survives real draw, captured cross-column drag, release
     * rebasing and settling. Forward/backward time wheels wrap independently. */
    for(unsigned format=0;format<2;format++) {
    time_format=format;
    for(unsigned hour=0;hour<24;hour++)for(unsigned column=0;column<2;column++) {
        draft.hour=(uint8_t)hour;draft.minute=(uint8_t)((hour*7)%60);p7_picker_reset();gesture(column,-20,true);
        assert_selected(format?draft.hour:draft.hour%12?draft.hour%12:12,draft.minute);gesture(column,20,true);assert(draft.hour==hour&&draft.minute==(hour*7)%60);
    }
    for(unsigned minute=0;minute<60;minute++) {
        draft.hour=11;draft.minute=(uint8_t)minute;p7_picker_reset();gesture(1,-36,true);assert(draft.hour==11&&draft.minute==(minute+1)%60);
        assert_selected(format?draft.hour:draft.hour%12?draft.hour%12:12,draft.minute);gesture(1,36,false);assert(draft.hour==11&&draft.minute==minute);
    }
    }
    draft.hour=10;draft.minute=25;p7_picker_reset();
    assert(!p7_picker_tap(170,146));assert(draft.hour==10&&draft.minute==25);
    assert(!p7_picker_tap(170,80));assert(!p7_picker_tap(120,110));
    assert(p7_picker_tap(170,180)&&draft.minute==26&&draft.hour==10);
    assert(p7_picker_tap(70,110)&&draft.hour==9&&draft.minute==26);
    p7_picker_select(170);assert(p7_picker_step(1)&&draft.minute==27&&draft.hour==9);
    springboard_contact c=contact(170,146,true,false);p7_picker_contact(&c);c=contact(70,130,false,false);p7_picker_contact(&c);
    c.cancelled=true;p7_picker_contact(&c);assert(!p7_picker_active()&&!p7_picker.offset);
    c=contact(170,80,true,false);assert(!p7_picker_contact(&c)&&!p7_picker_active());
    /* Sub-threshold samples accumulate. A new column must not inherit the
     * previous column's interrupted settling offset. */
    c=contact(170,146,true,false);p7_picker_contact(&c);
    c=contact(170,144,false,false);assert(!p7_picker_contact(&c));
    c=contact(170,139,false,false);assert(p7_picker_contact(&c)&&p7_picker.offset==-7);
    c=contact(170,139,false,true);p7_picker_contact(&c);assert(p7_picker.settling);
    c=contact(70,146,true,false);p7_picker_contact(&c);assert(!p7_picker.offset&&!p7_picker.settling);
    c=contact(70,146,false,true);p7_picker_contact(&c);
    time_format=PORTABLE_TIME_FORMAT_12;
    for(unsigned h=0;h<24;h++){draft.hour=(uint8_t)h;draft.minute=59;assert(p7_picker_tap(65,89));assert(draft.hour==(h+12)%24&&draft.minute==59);}
    init(PAGE_DURATION);
    for(unsigned duration=0;duration<=720;duration++) {
        draft.duration_minutes=(uint16_t)duration;p7_duration();assert_selected(duration/60,duration%60);frames++;
    }
    draft.duration_minutes=420;p7_duration();save_frame(directory,"duration-07-00");
    draft.duration_minutes=720;p7_duration();save_frame(directory,"duration-12-00");
    for(unsigned hours=0;hours<=12;hours++) {
        draft.duration_minutes=(uint16_t)(hours*60);p7_picker_reset();
        if(hours<12){gesture(0,-36,true);assert(draft.duration_minutes==(hours+1)*60);}
        if(hours>0){draft.duration_minutes=(uint16_t)(hours*60);gesture(0,36,true);assert(draft.duration_minutes==(hours-1)*60);}
    }
    for(unsigned minute=0;minute<60;minute++) {
        draft.duration_minutes=(uint16_t)(420+minute);p7_picker_reset();p7_picker_select(170);
        if(minute<59){assert(p7_picker_step(1));assert(draft.duration_minutes==421+minute);}
        else assert(!p7_picker_step(1)&&draft.duration_minutes==479);
    }
    draft.duration_minutes=719;p7_picker_select(70);assert(!p7_picker_step(1)&&draft.duration_minutes==719);
    draft.duration_minutes=720;p7_picker_select(170);assert(!p7_picker_step(1)&&draft.duration_minutes==720);
    draft.duration_minutes=1;draft.notify_end=draft.warn3=1;assert(p7_picker_step(-1)&&!draft.duration_minutes&&!draft.notify_end&&!draft.warn3);
    printf("Points picker: %u actual-renderer all-value frames, independent columns, smooth drag/release, 12/24-hour display and 0..720min passed\n",frames);
    return 0;
}
