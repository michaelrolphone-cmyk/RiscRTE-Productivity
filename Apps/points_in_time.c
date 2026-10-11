/* Original shared Points in Time application. The ordinary alarm service owns
 * recurrence, durable occurrences, arbitration and all sound/vibration output. */
#ifdef PORTABLE_RETURN_APP
#error Points requires app-owned POINTS_RETURN_APP, never generic pre-poll return
#endif
#include "T5AppApi.h"
#include "SpringboardPresentation.h"
#include "PaperPresentation.h"
#include "RiscRuntimeV1.h"
#include "PortableRtcClock.h"
#include "PortableTime.h"
#include "PortableTimeFormat.h"
#include "points_writer.h"
#ifdef PORTABLE_NOVA_UI
#include "PortableNovaUi.h"
#include "PortableNovaKeyboard.h"
#endif
#ifdef PORTABLE_ALARM_CLIENT
#include "PortableAppSleep.h"
#endif
#include <stddef.h>
#include <stdio.h>
#ifdef ALARM_NATIVE_UTC
#include "points_native_time.h"
#ifndef ALARM_SERVICE_TAGGED_V2
#error Native Points requires explicitly negotiated alarm.service API2
#endif
#else
#define points_live() true
#define points_service_result(rc) ((void)(rc),true)
#endif

enum { PAGE_LIST, PAGE_EDIT, PAGE_TYPE, PAGE_TIME, PAGE_DAYS, PAGE_MODE, PAGE_DURATION, PAGE_SAVE, PAGE_CUSTOM, PAGE_CUSTOM_KEYBOARD };
static const t5_app_api_v1 *app;
static const risc_runtime_api_v1 *runtime;
static const risc_key_value_v1 *storage,*preferences;
#ifndef ALARM_NATIVE_UTC
static const twatch_rtc_api_v1 *rtc;
#endif
static const alarm_service_v1 *service;
static risc_runtime_capability_v1 grants[4];
static unsigned acquired,page,list_page,selected,time_format;
static const springboard_presentation *nova;
static const paper_presentation *paper;
static void pe_draw(void);
static unsigned nova_list_scroll,nova_edit_scroll,nova_type_scroll,custom_kind,custom_pos,custom_key_page,custom_key_choice;
static int nova_drag_x,nova_drag_y;
static bool nova_drag_active,nova_delete_confirm;
static points_meta custom_draft;
static points_writer writer;
static points_item draft;
static alarm_status_v1 service_state;
static bool ready,service_valid,clock_valid;
static const char *notice;
static const char *const kinds[]={"Empty","Work","Work End","Lunch","Break","Bedtime"};
static const char *const modes[]={"System default","Vibrate","Sound","Sound and vibrate"};
static const char *const days[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
static const char *const nova_kinds[]={"","Work","Work End","LUNCH","BREAK","WIND DOWN","CUSTOM 1","CUSTOM 2"};
static const char *const nova_modes[]={"SYSTEM","VIBRATE","SOUND","VIBRATE + SOUND"};
static bool points_visual_only(void) {
#ifdef ALARM_NATIVE_UTC
    const alarm_service_descriptor_v2 *descriptor=alarm_service_descriptor(service);
    return descriptor && descriptor->output_modes==ALARM_MODE_VISUAL;
#elif defined(ALARM_SERVICE_SLEEP_RESUME_SUPPORTED)
    /* Current Watch API1 appends the sleep callback at the offset occupied by
     * the retired untagged output descriptor. Never reinterpret that pointer. */
    return false;
#else
    return service && alarm_service_output_modes(service)==ALARM_MODE_VISUAL;
#endif
}
static const char *point_mode_name(unsigned mode,bool compact) {
    if(points_visual_only())return "VISUAL ONLY";
    return compact?nova_modes[mode]:modes[mode];
}
static void open_mode_page(void) {
    if(points_visual_only()){notice="Visual only - saved mode kept";return;}
    page=PAGE_MODE;
}
static void select_mode(unsigned mode) {
    if(!points_visual_only())draft.mode=(uint8_t)mode;
    else notice="Visual only - saved mode kept";
    page=PAGE_EDIT;
}
#define NOVA_CYAN 0x19e3ffu
#define NOVA_WHITE 0xffffffu
#define NOVA_MUTED 0x6b8288u
#ifndef PORTABLE_NOVA_UI
#define NOVA_DIM 0x34484du
#endif
#define NOVA_RED 0xff6a5fu
static const uint32_t nova_kind_colors[]={NOVA_MUTED,0x3d9bffu,0xff3d71u,0xffb020u,0x3dff9au,0x6d7bffu,NOVA_CYAN,NOVA_CYAN};
static const uint32_t nova_custom_colors[]={0xffd24au,0xff7a1au,0xff3d71u,0xb24dffu,0x6d7bffu,0x3d9bffu,0x19e3ffu,0x3dff9au};
static const char custom_chars[]=" ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_";
/* The shared adapter provides this private presentation on the 240x240 watch.
 * Host fixtures and non-NOVA deployments intentionally fall back to the
 * original monochrome UI. A strong adapter definition overrides this weak
 * null implementation in the target ELF. */
__attribute__((weak)) const springboard_presentation *springboard_presentation_get(void) { return NULL; }
__attribute__((weak)) const paper_presentation *paper_presentation_get(void) { return NULL; }
static bool duration_kind(unsigned kind) { return points_duration_kind(kind); }
static unsigned custom_index(unsigned kind){return kind>=POINTS_CUSTOM_1&&kind<=POINTS_CUSTOM_2?kind-POINTS_CUSTOM_1:POINTS_CUSTOM_COUNT;}
static const char *nova_kind_name(unsigned kind) {
    unsigned i=custom_index(kind);if(i<POINTS_CUSTOM_COUNT&&writer.meta.custom[i].name[0])return writer.meta.custom[i].name;
    return kind<=POINTS_CUSTOM_2?nova_kinds[kind]:"POINT";
}
static uint32_t nova_kind_color(unsigned kind) {
    unsigned i=custom_index(kind);if(i<POINTS_CUSTOM_COUNT&&writer.meta.custom[i].name[0])return nova_custom_colors[writer.meta.custom[i].color%POINTS_COLOR_COUNT];
    return kind<=POINTS_CUSTOM_2?nova_kind_colors[kind]:NOVA_MUTED;
}
static void custom_trim(char *s){int n=POINTS_CUSTOM_NAME_MAX;while(n>0&&(s[n-1]==' '||!s[n-1]))s[--n]=0;}
static void custom_cycle(int direction){
    unsigned i=custom_index(custom_kind);if(i>=POINTS_CUSTOM_COUNT)return;char *ch=&custom_draft.custom[i].name[custom_pos];
    unsigned n=0;while(custom_chars[n]&&custom_chars[n]!=*ch)n++;if(!custom_chars[n])n=1;
    unsigned count=(unsigned)strlen(custom_chars);n=(n+count+(direction>0?1:count-1))%count;*ch=custom_chars[n];
    custom_draft.custom[i].name[POINTS_CUSTOM_NAME_MAX]=0;
}
static bool read_clock(uint32_t *seconds) {
#ifdef ALARM_NATIVE_UTC
    bool ok=points_native_sample(runtime,preferences,seconds,NULL);
    memset(&points_preview,0,sizeof(points_preview));
    if(ok&&writer.loaded)(void)points_utc_project(&points_zone_rule,&writer.saved,*seconds,&points_preview);
    return ok;
#else
    twatch_rtc_time_v1 t;
    return rtc && rtc->read(rtc->context,&t) && t.weekday<=6 &&
        alarm_calendar_seconds(t.year,t.month,t.day,t.hour,t.minute,t.second,seconds);
#endif
}
#ifdef ALARM_NATIVE_UTC
/* Optional adapter toolbar contract: already-local civil values, no RTC grant. */
__attribute__((visibility("hidden"))) bool portable_app_native_local_time(twatch_rtc_time_v1 *out) {
    uint32_t seconds;portable_timezone_civil local;
    if(!out||!ready||!points_native_sample(runtime,preferences,&seconds,&local))return false;
    *out=(twatch_rtc_time_v1){(uint16_t)local.year,local.month,local.day,local.weekday,local.hour,local.minute,local.second};
    return true;
}
static const char *points_time_zone(void) {return clock_valid&&points_zone[0]?points_zone:"Time/zone unavailable";}
#else
#define points_time_zone portable_time_zone
#endif
static void refresh_status(void) {
    if(!points_live())return;
    service_state=(alarm_status_v1){.struct_size=sizeof(service_state)};
#ifdef ALARM_NATIVE_UTC
    int32_t rc=service?service->status(service->context,&service_state):ALARM_INVALID;
    if(!points_service_result(rc)||!points_service_result(service_state.error))return;
    service_valid=rc==ALARM_OK&&service_state.api_version==1&&service_state.struct_size>=sizeof(service_state)&&
        service_state.state<=ALARM_STATE_CUE;
#else
    service_valid=service && service->status(service->context,&service_state)==ALARM_OK;
#endif
}
static void close_dependencies(void) {
    if(!points_live())return;
#ifdef ALARM_NATIVE_UTC
    if(runtime)while(acquired){
        risc_runtime_capability_v1 released=grants[acquired-1];
        bool ok=runtime->release(&released);
        if(!points_live())return;
        if(!ok||released.api||released.slot||released.generation){points_retain();return;}
        grants[--acquired]=released;
    }
    storage=preferences=NULL;service=NULL;runtime=NULL;ready=false;
#else
    if(runtime)while(acquired)runtime->release(&grants[--acquired]);
    storage=preferences=NULL;rtc=NULL;service=NULL;runtime=NULL;ready=false;
#endif
}
static bool open_dependencies(void) {
    if(!points_live())return false;
    runtime=risc_runtime_get_api(1);acquired=0;
    if(!runtime || runtime->api_version!=1 || runtime->struct_size<RISC_RUNTIME_CAPABILITIES_V1_SIZE ||
       !runtime->acquire || !runtime->release || !runtime->yield_ms)return false;
#ifdef ALARM_NATIVE_UTC
    const char *caps[]={"storage.key-value",ALARM_SERVICE_CAPABILITY,"storage.key-value"};
    const unsigned apis[]={1,ALARM_SERVICE_API_V2,1};const uint64_t instances[]={5,0,1};
#else
    const char *caps[]={"storage.key-value","rtc.clock",ALARM_SERVICE_CAPABILITY,"storage.key-value"};
    const unsigned apis[]={1,2,1,1};const uint64_t instances[]={5,0,0,1};
#endif
    for(unsigned i=0;i<sizeof(apis)/sizeof(apis[0]);i++) {
        grants[i]=(risc_runtime_capability_v1){.struct_size=sizeof(grants[i])};
#ifdef ALARM_NATIVE_UTC
        bool ok=runtime->acquire(caps[i],apis[i],instances[i],&grants[i]);
        if(!points_live())return false;
        if(!ok){points_retain();return false;}
        if(!grants[i].api||!grants[i].slot||!grants[i].generation){points_retain();return false;}
#else
        if(!runtime->acquire(caps[i],apis[i],instances[i],&grants[i]))return false;
#endif
        acquired++;
    }
#ifdef ALARM_NATIVE_UTC
    points_storage_source=grants[0].api;service=grants[1].api;points_preferences_source=grants[2].api;
    if(!points_writer_storage_valid(points_storage_source)||!portable_time_format_api_valid(points_preferences_source))return false;
    points_storage_guarded=(risc_key_value_v1){1,sizeof(points_storage_guarded),(void*)points_storage_source,points_guarded_get,points_guarded_put};
    points_preferences_guarded=(risc_key_value_v1){1,sizeof(points_preferences_guarded),(void*)points_preferences_source,points_guarded_get,NULL};
    storage=&points_storage_guarded;preferences=&points_preferences_guarded;
#else
    storage=grants[0].api;rtc=grants[1].api;service=grants[2].api;preferences=grants[3].api;
#endif
    return points_writer_storage_valid(storage) && portable_time_format_api_valid(preferences) &&
#ifndef ALARM_NATIVE_UTC
        rtc && rtc->api_version==2 && rtc->struct_size>=sizeof(*rtc) && rtc->read &&
#endif
#ifdef ALARM_NATIVE_UTC
        alarm_service_descriptor(service) &&
#else
        service && service->api_version==1 && service->struct_size>=sizeof(*service) && service->status &&
#endif
        service->step && service->refresh && service->acknowledge && service->stop_only;
}
static void load_catalog(void) {
    if(!points_live())return;
    (void)points_writer_load(&writer,storage);if(!points_live())return;
    (void)portable_time_format_load(preferences,&time_format);if(!points_live())return;
    uint32_t now;clock_valid=read_clock(&now);if(!points_live())return;
    refresh_status();
    notice=writer.loaded?"Tap a point to edit":"Storage invalid: retry";
}
static void format_time(const points_item *p,char *out,size_t cap) {
    if(time_format==PORTABLE_TIME_FORMAT_24)snprintf(out,cap,"%u:%02u",p->hour,p->minute);
    else snprintf(out,cap,"%u:%02u %s",p->hour%12?p->hour%12:12,p->minute,p->hour<12?"AM":"PM");
}
static void format_days(unsigned mask,char *out,size_t cap) {
    if(mask==127){snprintf(out,cap,"Every day");return;}
    if(mask==62){snprintf(out,cap,"Weekdays");return;}
    if(mask==65){snprintf(out,cap,"Weekends");return;}
    if(!mask){snprintf(out,cap,"Choose days");return;}
    /* Seven unambiguous single-letter positions: Sunday first, '-' means off. */
    snprintf(out,cap,"%c%c%c%c%c%c%c  Sun-Sat",mask&1?'S':'-',mask&2?'M':'-',mask&4?'T':'-',
        mask&8?'W':'-',mask&16?'T':'-',mask&32?'F':'-',mask&64?'S':'-');
}
static const char *status_message(void) {
    if(!ready)return "Dependencies unavailable";
    if(writer.uncertain||writer.meta_uncertain)return "Save unconfirmed: retry";
    if(!writer.loaded)return "Storage invalid: retry";
    if(!clock_valid)return
#ifdef ALARM_NATIVE_UTC
        points_clock_status==PORTABLE_REALTIME_UNSET?"Time unset: use Settings":"Time/zone invalid: retry";
#else
        "RTC invalid: retry";
#endif
    if(!service_valid)return "Service unavailable: retry";
    if(service_state.state==ALARM_STATE_BLOCKED)return service_state.error==ALARM_RTC?
#ifdef ALARM_NATIVE_UTC
        "Native time: retry":
#else
        "RTC changed: retry":
#endif
        "Service error: retry";
#ifdef ALARM_NATIVE_UTC
    if(points_preview.flags&POINTS_FLAG_GAP)return "DST gap: start skipped";
    if(points_preview.flags&POINTS_FLAG_FOLD)return "DST fold: start skipped";
    if(points_preview.flags&POINTS_FLAG_RANGE)return "Time range limit";
#endif
    if(service_state.state==ALARM_STATE_DISMISSING)return "Dismiss saving";
    if(service_state.state==ALARM_STATE_LOADING)return "Service loading";
    return notice?notice:"Service ready";
}
static void label(int x,int y,int w,const char *value) { app->draw_label(x,y,w,value); }
static void line(int y) { app->fill_rect(8,y,224,1,true); }
static void button(int x,int y,int w,const char *value) {
    app->fill_rect(x,y,w,1,true);app->fill_rect(x,y+27,w,1,true);
    label(x,y+7,w,value);
}
static bool hit(int x,int y,int left,int top,int width,int height) {
    return x>=left && x<left+width && y>=top && y<top+height;
}
static __attribute__((unused)) void draw_legacy(void) {
    char text[64],value[40];app->clear();label(4,10,44,"Back");
    if(page==PAGE_LIST)label(50,10,182,"POINTS IN TIME");
    else {snprintf(text,sizeof(text),"POINT %u",selected+1);label(50,10,182,text);}
    line(31);
    if(!ready) {
        label(8,76,224,"Service / RTC / storage");label(8,101,224,"could not be opened.");
        button(8,180,224,"Retry");
    }
#ifndef PORTABLE_ALARM_CLIENT
    else if(service_valid && service_state.occurrence.generation) {
        label(8,65,224,service_state.label);label(8,104,224,"Dismiss to continue");button(8,176,224,"Dismiss");
    }
#endif
    else if(writer.uncertain) {
        label(8,54,224,"Save may have committed.");label(8,79,224,"Edits locked until retry.");
        label(8,111,224,"Retry writes the same data.");button(8,177,224,"Retry save");
    } else if(!writer.loaded) {
        label(8,69,224,"Saved data is unavailable");label(8,96,224,"or invalid. No defaults saved.");button(8,177,224,"Retry storage");
    } else if(page==PAGE_LIST) {
        for(unsigned row=0;row<4;row++) {
            unsigned slot=list_page*4+row;const points_item *p=&writer.saved.points[slot];int y=38+(int)row*35;
            snprintf(text,sizeof(text),"%u  %s",slot+1,kinds[p->kind]);label(8,y,224,text);
            if(!p->kind)snprintf(text,sizeof(text),"Empty - tap to set");
            else {format_time(p,value,sizeof(value));snprintf(text,sizeof(text),"%s  %s",value,p->enabled?"On":"Off");}
            label(8,y+15,224,text);line(y+32);
        }
        button(8,181,68,"Prev");snprintf(text,sizeof(text),"%u of 2",list_page+1);button(84,181,72,text);button(164,181,68,"Next");
    } else if(page==PAGE_EDIT) {
        snprintf(text,sizeof(text),"Type: %s",kinds[draft.kind]);label(8,42,224,text);line(63);
        format_time(&draft,value,sizeof(value));snprintf(text,sizeof(text),"Time: %s",value);label(8,72,224,text);line(93);
        format_days(draft.weekdays,value,sizeof(value));snprintf(text,sizeof(text),"Days: %s",value);label(8,102,224,text);line(123);
        snprintf(text,sizeof(text),"Alert: %s",point_mode_name(draft.mode,false));label(8,132,224,text);line(153);
        if(duration_kind(draft.kind))snprintf(text,sizeof(text),"Duration: %u min",draft.duration_minutes);
        else snprintf(text,sizeof(text),"Duration: not used");
        label(8,162,224,text);line(183);button(8,188,106,draft.enabled?"On":"Off");button(124,188,108,"Save...");
    } else if(page==PAGE_TYPE) {
        for(unsigned i=1;i<=5;i++){snprintf(text,sizeof(text),"%s%s",draft.kind==i?"X ":"",kinds[i]);button(8,39+(int)(i-1)*32,224,text);}
    } else if(page==PAGE_MODE) {
        for(unsigned i=0;i<4;i++){snprintf(text,sizeof(text),"%s%s",draft.mode==i?"X ":"",point_mode_name(i,false));button(8,47+(int)i*37,224,text);}
    } else if(page==PAGE_TIME) {
        label(8,42,224,"24-hour editor (HH:MM)");button(24,70,80,"Hour up");button(136,70,80,"Min up");
        snprintf(text,sizeof(text),"%u : %02u",draft.hour,draft.minute);label(24,114,192,text);
        button(24,142,80,"Hour down");button(136,142,80,"Min down");label(8,174,224,points_time_zone());button(8,185,224,"Done");
    } else if(page==PAGE_DAYS) {
        for(unsigned i=0;i<7;i++){snprintf(text,sizeof(text),"%s%s",draft.weekdays&(1u<<i)?"ON ":"OFF ",days[i]);button(8+(int)(i%3)*76,43+(int)(i/3)*45,72,text);}
        button(84,133,72,"All");button(160,133,72,"M-F");button(8,184,106,"Clear");button(124,184,108,"Done");
    } else if(page==PAGE_DURATION) {
        label(8,43,224,"Lunch / break end alert");snprintf(text,sizeof(text),"%u minutes",draft.duration_minutes);label(8,78,224,text);
        button(8,100,106,"Less 1 hour");button(124,100,108,"Add 1 hour");
        button(8,132,106,"Less 5 min");button(124,132,108,"Add 5 min");
        button(8,164,106,"Less 1 min");button(124,164,108,"Add 1 min");button(8,196,106,"None 0");button(124,196,108,"Done");
    } else if(page==PAGE_SAVE) {
        label(8,41,224,"Save changes this catalog.");label(8,68,224,"Old point alerts and pending");label(8,86,224,"lunch / break ends cancel.");
        label(8,115,224,"Only future starts will run.");label(8,140,224,"All 8 points are retained.");
        label(8,154,224,"Missing or repeated local");label(8,166,224,"times skip that date.");
        button(8,181,106,"Back");button(124,181,108,"Save now");
    }
    label(4,225,232,status_message());app->present(false);
}

static void nova_cap(int y,const char *text,bool clock,uint32_t color) {
    if(nova && nova->caption)nova->caption(y,text,clock,color);
}
static void nova_dot(int x,int y,int radius,uint32_t color,unsigned opacity) {
    if(nova && nova->circle)nova->circle(x,y,radius,color,(uint8_t)(opacity>255?255:opacity));
}
static unsigned nova_configured(unsigned order[POINTS_MAX]) {
    unsigned n=0;
    for(unsigned i=0;i<POINTS_MAX;i++)if(writer.saved.points[i].kind)order[n++]=i;
    for(unsigned i=1;i<n;i++) {
        unsigned slot=order[i],j=i;
        unsigned key=writer.saved.points[slot].hour*60u+writer.saved.points[slot].minute;
        while(j) {
            unsigned prev=order[j-1],pkey=writer.saved.points[prev].hour*60u+writer.saved.points[prev].minute;
            if(pkey<key || (pkey==key && prev<slot))break;
            order[j]=prev;j--;
        }
        order[j]=slot;
    }
    return n;
}
static unsigned nova_active_count(void) {
    unsigned n=0;for(unsigned i=0;i<POINTS_MAX;i++)if(writer.saved.points[i].kind&&writer.saved.points[i].enabled)n++;return n;
}
static unsigned nova_first_empty(void) {
    for(unsigned i=0;i<POINTS_MAX;i++)if(!writer.saved.points[i].kind)return i;
    return POINTS_MAX;
}
static void nova_short_days(unsigned mask,char out[16]) {
    if(mask==127){snprintf(out,16,"DAILY");return;}
    if(mask==62){snprintf(out,16,"MON-FRI");return;}
    if(mask==65){snprintf(out,16,"WKND");return;}
    unsigned k=0;static const char d[]="SMTWTFS";
    for(unsigned i=0;i<7&&k<14;i++){if(mask&(1u<<i))out[k++]=d[i];if(i<6&&k<14)out[k++]='.';}
    if(!mask){snprintf(out,16,"NEVER");return;}out[k]=0;
}
static void nova_duration(char out[16],unsigned minutes) {
    if(!minutes){snprintf(out,16,"NONE");return;}
    if(minutes>=60 && !(minutes%60))snprintf(out,16,"%uH",minutes/60);
    else if(minutes>=60)snprintf(out,16,"%uH %uM",minutes/60,minutes%60);
    else snprintf(out,16,"%u MIN",minutes);
}
static unsigned nova_duration_next(unsigned value,int direction) {
    static const uint16_t values[]={0,5,10,15,20,30,45,60,90,120,180,240,360,480,600,720};
    if(direction>0){for(unsigned i=0;i<sizeof(values)/sizeof(values[0]);i++)if(values[i]>value)return values[i];return 720;}
    for(int i=(int)(sizeof(values)/sizeof(values[0]))-1;i>=0;i--)if(values[i]<value)return values[i];
    return 0;
}
static void nova_duration_move(int steps) {
    while(steps>0){draft.duration_minutes=(uint16_t)nova_duration_next(draft.duration_minutes,1);steps--;}
    while(steps<0){draft.duration_minutes=(uint16_t)nova_duration_next(draft.duration_minutes,-1);steps++;}
}
static void nova_header(const char *title,const char *sub) {
    nova_cap(31,title,true,NOVA_CYAN);if(sub&&*sub)nova_cap(48,sub,false,NOVA_MUTED);
}
static void nova_problem(const char *title,const char *detail) {
    nova_header("POINTS IN TIME",NULL);nova_cap(93,title,true,NOVA_RED);nova_cap(119,detail,false,NOVA_MUTED);
    nova_cap(171,"TAP TO RETRY",false,NOVA_CYAN);
}
static void nova_draw_list(void) {
    unsigned order[POINTS_MAX],count=nova_configured(order),active=nova_active_count();
    char a[40];snprintf(a,sizeof(a),"%u ACTIVE - %u TOTAL",active,count);nova_header("POINTS IN TIME",a);
    unsigned rows=count+(count<POINTS_MAX?1u:0u),max_scroll=rows>4?rows-4:0;
    if(nova_list_scroll>max_scroll)nova_list_scroll=max_scroll;
    for(unsigned r=0;r<4;r++) {
        unsigned index=nova_list_scroll+r;if(index>=rows)break;int y=75+(int)r*38;
        if(index==count) {nova_dot(20,y+3,3,NOVA_CYAN,180);nova_cap(y,"+ ADD POINT",false,NOVA_CYAN);continue;}
        unsigned slot=order[index];const points_item *p=&writer.saved.points[slot];
        char time[24],line[52],sub[24],dur[16];format_time(p,time,sizeof(time));nova_short_days(p->weekdays,sub);
        if(p->duration_minutes){nova_duration(dur,p->duration_minutes);snprintf(line,sizeof(line),"%s  %s  %s",time,nova_kind_name(p->kind),dur);}
        else snprintf(line,sizeof(line),"%s  %s",time,nova_kind_name(p->kind));
        uint32_t col=p->enabled?nova_kind_color(p->kind):NOVA_DIM;
        nova_dot(20,y+3,3,col,p->enabled?255:110);nova_cap(y,line,true,col);nova_cap(y+14,sub,false,p->enabled?NOVA_MUTED:NOVA_DIM);
    }
    if(rows>4){char p[20];snprintf(p,sizeof(p),"%u-%u / %u",nova_list_scroll+1,
        nova_list_scroll+4<rows?nova_list_scroll+4:rows,rows);nova_cap(222,p,false,NOVA_MUTED);}
}
static void nova_draw_edit(void) {
    char sub[32];snprintf(sub,sizeof(sub),"%s",nova_kind_name(draft.kind));nova_header("EDIT POINT",sub);
    if(nova_edit_scroll>6)nova_edit_scroll=6;
    for(unsigned r=0;r<4;r++) {
        unsigned row=nova_edit_scroll+r;char line[64],v[32];uint32_t col=NOVA_WHITE;int y=76+(int)r*38;
        bool timed=duration_kind(draft.kind)&&draft.duration_minutes;
        if(row==0)snprintf(line,sizeof(line),"TYPE  %s",nova_kind_name(draft.kind));
        else if(row==1){format_time(&draft,v,sizeof(v));snprintf(line,sizeof(line),"TIME  %s",v);}
        else if(row==2){nova_duration(v,draft.duration_minutes);snprintf(line,sizeof(line),"DURATION  %s",duration_kind(draft.kind)?v:"NOT USED");if(!duration_kind(draft.kind))col=NOVA_DIM;}
        else if(row==3){snprintf(line,sizeof(line),"NOTIFY AT END  %s",draft.notify_end?"ON":"OFF");if(!timed)col=NOVA_DIM;}
        else if(row==4){snprintf(line,sizeof(line),"3 MIN WARNING  %s",draft.warn3?"ON":"OFF");if(!timed||draft.duration_minutes<3)col=NOVA_DIM;}
        else if(row==5)snprintf(line,sizeof(line),"NOTIFY  %s",point_mode_name(draft.mode,true));
        else if(row==6){nova_short_days(draft.weekdays,v);snprintf(line,sizeof(line),"DAYS  %s",v);}
        else if(row==7)snprintf(line,sizeof(line),"ENABLED  %s",draft.enabled?"ON":"OFF");
        else if(row==8){snprintf(line,sizeof(line),"SAVE CHANGES");col=NOVA_CYAN;}
        else {snprintf(line,sizeof(line),"%s",nova_delete_confirm?"TAP AGAIN TO DELETE":"DELETE POINT");col=NOVA_RED;}
        nova_dot(20,y+2,2,col,row==9?230:160);nova_cap(y,line,false,col);
    }
    nova_cap(222,"SWIPE FOR MORE",false,NOVA_MUTED);
}
static unsigned nova_type_choices(unsigned out[8]) {
    unsigned n=0;for(unsigned i=1;i<=5;i++)out[n++]=i;
    for(unsigned i=0;i<POINTS_CUSTOM_COUNT;i++)if(writer.meta.custom[i].name[0])out[n++]=POINTS_CUSTOM_1+i;
    for(unsigned i=0;i<POINTS_CUSTOM_COUNT;i++)if(!writer.meta.custom[i].name[0]){out[n++]=0;break;}
    return n;
}
static void nova_draw_type(void) {
    nova_header("POINT TYPE","CHOOSE OR CREATE");unsigned choice[8],count=nova_type_choices(choice),max=count>4?count-4:0;if(nova_type_scroll>max)nova_type_scroll=max;
    for(unsigned r=0;r<4;r++){unsigned at=nova_type_scroll+r;if(at>=count)break;unsigned kind=choice[at];int y=76+(int)r*38;
        if(!kind){nova_dot(25,y+2,3,NOVA_CYAN,200);nova_cap(y,"+ CUSTOM TYPE",false,NOVA_CYAN);continue;}
        uint32_t col=nova_kind_color(kind);nova_dot(25,y+2,draft.kind==kind?5:2,col,255);nova_cap(y,nova_kind_name(kind),false,draft.kind==kind?col:NOVA_WHITE);
    }
}
static void nova_draw_custom(void) {
    unsigned i=custom_index(custom_kind);if(i>=POINTS_CUSTOM_COUNT)return;char line[48],shown[POINTS_CUSTOM_NAME_MAX+1];
    memcpy(shown,custom_draft.custom[i].name,sizeof(shown));shown[POINTS_CUSTOM_NAME_MAX]=0;
    nova_header("CUSTOM TYPE","LEFT/RIGHT POSITION - UP/DOWN CHAR");snprintf(line,sizeof(line),"NAME  %s",shown);nova_cap(78,line,true,NOVA_CYAN);
    char ch=shown[custom_pos]?shown[custom_pos]:' ';snprintf(line,sizeof(line),"POS %u/12   CHAR %c",custom_pos+1,ch);nova_cap(111,line,false,NOVA_WHITE);
    nova_cap(137,"COLOR",false,NOVA_MUTED);for(unsigned n=0;n<8;n++)nova_dot(29+(int)n*26,160,custom_draft.custom[i].color==n?7:5,nova_custom_colors[n],255);
    nova_cap(205,"BACK                 SAVE",false,NOVA_CYAN);
}
static void nova_draw_mode(void) {
    nova_header("NOTIFY","HOW TO ALERT YOU");
    for(unsigned i=0;i<4;i++){int y=82+(int)i*36;uint32_t col=i==draft.mode?NOVA_CYAN:NOVA_WHITE;nova_dot(25,y+2,i==draft.mode?5:2,col,220);nova_cap(y,point_mode_name(i,true),false,col);}
}
static void nova_draw_time(void) {
    nova_header("TIME","DRAG OR TAP A COLUMN");
    for(int j=-2;j<=2;j++) {
        int h=(int)draft.hour+j;while(h<0)h+=24;h%=24;
        int m=(int)draft.minute+j*5;while(m<0)m+=60;m%=60;
        char line[24];snprintf(line,sizeof(line),"%d       %02d",h,m);
        nova_cap(72+(j+2)*32,line,j==0,j==0?NOVA_CYAN:(j==1||j==-1?NOVA_MUTED:NOVA_DIM));
    }
    nova_cap(138,":",true,NOVA_CYAN);
}
static void nova_draw_duration(void) {
    nova_header("DURATION","LUNCH / BREAK END ALERT");
    unsigned cur=draft.duration_minutes;
    for(int j=-2;j<=2;j++) {
        unsigned v=cur;if(j<0)for(int n=0;n<-j;n++)v=nova_duration_next(v,-1);else for(int n=0;n<j;n++)v=nova_duration_next(v,1);
        char line[20];nova_duration(line,v);nova_cap(72+(j+2)*32,line,j==0,j==0?NOVA_CYAN:(j==1||j==-1?NOVA_MUTED:NOVA_DIM));
    }
}
static void nova_draw_days(void) {
    char mask[24];unsigned k=0;static const char letter[]="SMTWTFS";
    for(unsigned i=0;i<7;i++){mask[k++]=(draft.weekdays&(1u<<i))?letter[i]:'-';if(i<6)mask[k++]=' ';}mask[k]=0;
    nova_header("DAYS","TAP A DAY OR PRESET");nova_cap(82,mask,true,NOVA_CYAN);
    nova_cap(124,"EVERY DAY",false,draft.weekdays==127?NOVA_CYAN:NOVA_WHITE);
    nova_cap(155,"MON-FRI",false,draft.weekdays==62?NOVA_CYAN:NOVA_WHITE);
    nova_cap(186,"WEEKEND",false,draft.weekdays==65?NOVA_CYAN:NOVA_WHITE);
    nova_cap(217,"NONE",false,!draft.weekdays?NOVA_RED:NOVA_MUTED);
}
static void nova_draw_save(void) {
    nova_header("SAVE POINT","CONFIRM CATALOG CHANGE");
    nova_cap(82,"OLD PENDING POINT ALERTS",false,NOVA_WHITE);nova_cap(103,"ARE CANCELLED",false,NOVA_WHITE);
    nova_cap(132,"ONLY FUTURE STARTS RUN",false,NOVA_MUTED);nova_cap(154,"AFTER THIS SAVE",false,NOVA_MUTED);
    nova_cap(194,"BACK              SAVE NOW",false,NOVA_CYAN);
}
static __attribute__((unused)) void draw_nova(void) {
    if(!nova||!nova->begin||!nova->caption){draw_legacy();return;}
    nova->begin();
    if(!ready)nova_problem("DEPENDENCIES UNAVAILABLE","SERVICE / RTC / STORAGE");
#ifndef PORTABLE_ALARM_CLIENT
    else if(service_valid&&service_state.occurrence.generation){nova_problem(service_state.label,"TAP TO DISMISS");}
#endif
    else if(writer.uncertain||writer.meta_uncertain)nova_problem("SAVE UNCONFIRMED","RETRY WRITES SAME DATA");
    else if(!writer.loaded)nova_problem("STORAGE INVALID","NO DEFAULTS WERE SAVED");
    else if(page==PAGE_LIST)nova_draw_list();
    else if(page==PAGE_EDIT)nova_draw_edit();
    else if(page==PAGE_TYPE)nova_draw_type();
    else if(page==PAGE_TIME)nova_draw_time();
    else if(page==PAGE_DAYS)nova_draw_days();
    else if(page==PAGE_MODE)nova_draw_mode();
    else if(page==PAGE_DURATION)nova_draw_duration();
    else if(page==PAGE_CUSTOM)nova_draw_custom();
    else nova_draw_save();
    if(ready&&writer.loaded&&!writer.uncertain)nova_cap(232,status_message(),false,NOVA_MUTED);
    app->present(false);
}
static void edit_slot(unsigned slot);
static void save_action(void);
static void retry_action(void);
#include "points_nova7.inc"
static void draw(void) {
    if(!points_live())return;
    if(paper){pe_draw();return;}
#ifdef PORTABLE_NOVA_UI
    draw_nova7();
#else
    if(nova)draw_nova();else draw_legacy();
#endif
}

static void edit_slot(unsigned slot) {
    selected=slot;draft=writer.saved.points[slot];
    if(!draft.kind)draft=(points_item){.kind=POINTS_WORK_START};
    page=PAGE_EDIT;notice="Draft only - not saved";
}
#ifdef ALARM_NATIVE_UTC
static void save_action(void) {
    if(!points_live())return;
    int32_t rc;
    if(writer.meta_uncertain){rc=points_writer_retry_meta(&writer,storage);
        if(points_live()&&rc==ALARM_OK&&writer.uncertain)rc=points_writer_retry(&writer,storage);
    }
    else if(writer.uncertain)rc=points_writer_retry(&writer,storage);
    else {
        if(!writer.loaded){notice="Storage invalid: retry";return;}
        if(writer.saved.revision==UINT32_MAX){notice="Revision limit reached";return;}
        if(draft.enabled && !draft.weekdays){notice="Choose at least one day";
#ifdef PORTABLE_NOVA_UI
            page=PAGE_EDIT;
#else
            page=PAGE_DAYS;
#endif
            return;}
        if(!service_valid || service_state.state==ALARM_STATE_BLOCKED){notice="Service blocked: retry";return;}
        uint32_t now;clock_valid=read_clock(&now);if(!points_live())return;if(!clock_valid){notice="Time/zone invalid: retry";return;}
        if(now>ALARM_RTC_MAX-ALARM_RECOVERY_SECONDS){notice="Native time range limit";return;}
        points_config desired=writer.saved;desired.revision++;desired.created=now;desired.points[selected]=draft;
        rc=points_writer_save(&writer,storage,&desired);
    }
    if(!points_live())return;
    if(rc!=ALARM_OK){notice=writer.uncertain?"Save unconfirmed: retry":"Point or revision invalid";return;}
    page=PAGE_LIST;notice="Catalog saved";(void)points_service_result(service->refresh(service->context));
}
#else
static void save_action(void) {
    int32_t rc;
    if(writer.meta_uncertain){rc=points_writer_retry_meta(&writer,storage);
        if(rc==ALARM_OK&&writer.uncertain)rc=points_writer_retry(&writer,storage);
    }
    else if(writer.uncertain)rc=points_writer_retry(&writer,storage);
    else {
        if(!writer.loaded){notice="Storage invalid: retry";return;}
        if(writer.saved.revision==UINT32_MAX){notice="Revision limit reached";return;}
        if(draft.enabled && !draft.weekdays){notice="Choose at least one day";
#ifdef PORTABLE_NOVA_UI
            page=PAGE_EDIT;
#else
            page=PAGE_DAYS;
#endif
            return;}
        if(!service_valid || service_state.state==ALARM_STATE_BLOCKED){notice="Service blocked: retry";return;}
        uint32_t now;clock_valid=read_clock(&now);if(!clock_valid){notice="RTC invalid: retry";return;}
        if(now>ALARM_RTC_MAX-ALARM_RECOVERY_SECONDS){notice="RTC range limit";return;}
        points_config desired=writer.saved;desired.revision++;desired.created=now;desired.points[selected]=draft;
        rc=points_writer_save(&writer,storage,&desired);
    }
    if(rc!=ALARM_OK){notice=writer.uncertain?"Save unconfirmed: retry":"Point or revision invalid";return;}
    page=PAGE_LIST;notice="Catalog saved";(void)service->refresh(service->context);
}
#endif
static void retry_action(void) {
    if(!points_live())return;
    if(!ready) {
        close_dependencies();if(!points_live())return;ready=open_dependencies();
        if(ready)load_catalog();
        return;
    }
    if(writer.meta_uncertain){if(writer.uncertain)save_action();else (void)points_writer_retry_meta(&writer,storage);return;}
    if(writer.uncertain){save_action();return;}
    if(page==PAGE_LIST || !writer.loaded)load_catalog();
    uint32_t now;clock_valid=read_clock(&now);if(!points_live())return;(void)points_service_result(service->refresh(service->context));refresh_status();notice="Refreshed";
}
static __attribute__((unused)) void on_tap_legacy(int x,int y) {
    if(!ready) {
        if(hit(x,y,8,176,224,34))retry_action();
        return;
    }
#ifndef PORTABLE_ALARM_CLIENT
    if(service_valid && service_state.occurrence.generation) {
        if(hit(x,y,8,176,224,34))(void)points_service_result(service->acknowledge(service->context,&service_state.occurrence));
        return;
    }
#endif
    if(writer.uncertain || writer.meta_uncertain || !writer.loaded) {
        if(hit(x,y,8,176,224,34))retry_action();
        return;
    }
    if(hit(x,y,4,224,232,16)){retry_action();return;}
    if(page!=PAGE_LIST && page!=PAGE_SAVE)notice="Draft only - not saved";
    if(page==PAGE_LIST) {
        if(hit(x,y,8,35,224,140)){edit_slot(list_page*4+(unsigned)(y-35)/35);return;}
        if(hit(x,y,8,181,68,28) || hit(x,y,164,181,68,28)){list_page^=1u;return;}
    } else if(page==PAGE_EDIT) {
        if(hit(x,y,8,35,224,149)) {
            unsigned row=(unsigned)(y-35)/30;
            page=row==0?PAGE_TYPE:row==1?PAGE_TIME:row==2?PAGE_DAYS:row==3?(points_visual_only()?PAGE_EDIT:PAGE_MODE):duration_kind(draft.kind)?PAGE_DURATION:PAGE_EDIT;
        } else if(hit(x,y,8,188,106,28))draft.enabled^=1;
        else if(hit(x,y,124,188,108,28))page=PAGE_SAVE;
    } else if(page==PAGE_TYPE) {
        for(unsigned i=1;i<=5;i++)if(hit(x,y,8,39+(int)(i-1)*32,224,28)) {
            draft.kind=(uint8_t)i;if(!duration_kind(i))draft.duration_minutes=0;page=PAGE_EDIT;break;
        }
    } else if(page==PAGE_MODE) {
        for(unsigned i=0;i<4;i++)if(hit(x,y,8,47+(int)i*37,224,28)){select_mode(i);break;}
    } else if(page==PAGE_TIME) {
        if(hit(x,y,24,70,80,28))draft.hour=(uint8_t)((draft.hour+1)%24);
        else if(hit(x,y,136,70,80,28))draft.minute=(uint8_t)((draft.minute+1)%60);
        else if(hit(x,y,24,142,80,28))draft.hour=(uint8_t)((draft.hour+23)%24);
        else if(hit(x,y,136,142,80,28))draft.minute=(uint8_t)((draft.minute+59)%60);
        else if(hit(x,y,8,185,224,28))page=PAGE_EDIT;
    } else if(page==PAGE_DAYS) {
        for(unsigned i=0;i<7;i++)if(hit(x,y,8+(int)(i%3)*76,43+(int)(i/3)*45,72,28))draft.weekdays^=(uint8_t)(1u<<i);
        if(hit(x,y,84,133,72,28))draft.weekdays=127;
        else if(hit(x,y,160,133,72,28))draft.weekdays=62;
        else if(hit(x,y,8,184,106,28))draft.weekdays=0;
        else if(hit(x,y,124,184,108,28))page=PAGE_EDIT;
    } else if(page==PAGE_DURATION) {
        unsigned n=draft.duration_minutes;
        if(hit(x,y,8,100,106,28))n=n>=60?n-60:0;
        else if(hit(x,y,124,100,108,28))n=n<=660?n+60:720;
        else if(hit(x,y,8,132,106,28))n=n>=5?n-5:0;
        else if(hit(x,y,124,132,108,28))n=n<=715?n+5:720;
        else if(hit(x,y,8,164,106,28))n=n?n-1:0;
        else if(hit(x,y,124,164,108,28))n=n<720?n+1:720;
        else if(hit(x,y,8,196,106,28))n=0;
        else if(hit(x,y,124,196,108,28))page=PAGE_EDIT;
        draft.duration_minutes=(uint16_t)n;
    } else if(page==PAGE_SAVE) {
        if(hit(x,y,8,181,106,28))page=PAGE_EDIT;
        else if(hit(x,y,124,181,108,28))save_action();
    }
}

static void nova_new_point(void) {
    unsigned slot=nova_first_empty();if(slot>=POINTS_MAX){notice="All 8 point slots are in use";return;}
    twatch_rtc_time_v1 t={0};unsigned h=8,m=0;
#ifdef ALARM_NATIVE_UTC
    uint32_t seconds;portable_timezone_civil local;
    if(points_native_sample(runtime,preferences,&seconds,&local)){t.hour=local.hour;t.minute=local.minute;
        h=t.hour;m=((unsigned)t.minute+4u)/5u*5u;if(m>=60){m=0;h=(h+1)%24;}}
    if(!points_live())return;
#else
    if(rtc&&rtc->read(rtc->context,&t)){h=t.hour;m=((unsigned)t.minute+4u)/5u*5u;if(m>=60){m=0;h=(h+1)%24;}}
#endif
    selected=slot;draft=(points_item){.kind=POINTS_BREAK,.enabled=1,.mode=1,.weekdays=62,.hour=(uint8_t)h,.minute=(uint8_t)m,.duration_minutes=15};
    page=PAGE_EDIT;nova_edit_scroll=0;nova_delete_confirm=false;notice="Draft only - not saved";
}
static __attribute__((unused)) void nova_tap(int x,int y) {
    if(!ready){if(y>=135)retry_action();return;}
#ifndef PORTABLE_ALARM_CLIENT
    if(service_valid&&service_state.occurrence.generation){if(y>=130)(void)points_service_result(service->acknowledge(service->context,&service_state.occurrence));return;}
#endif
    if(writer.uncertain||writer.meta_uncertain||!writer.loaded){if(y>=130)retry_action();return;}
    if(page!=PAGE_LIST&&page!=PAGE_SAVE)notice="Draft only - not saved";
    if(page==PAGE_LIST) {
        if(y<60||y>=216)return;
        unsigned order[POINTS_MAX],count=nova_configured(order);
        unsigned index=nova_list_scroll+(unsigned)(y-60)/38u;
        if(index<count){edit_slot(order[index]);nova_edit_scroll=0;nova_delete_confirm=false;}
        else if(index==count&&count<POINTS_MAX)nova_new_point();
    } else if(page==PAGE_EDIT) {
        if(y<60||y>=216)return;
        unsigned row=nova_edit_scroll+(unsigned)(y-60)/38u;bool timed=duration_kind(draft.kind)&&draft.duration_minutes;
        if(row==0){page=PAGE_TYPE;nova_type_scroll=0;}
        else if(row==1)page=PAGE_TIME;
        else if(row==2){if(duration_kind(draft.kind))page=PAGE_DURATION;else notice="Duration not used for this type";}
        else if(row==3){if(timed)draft.notify_end^=1;else notice="Set a duration first";}
        else if(row==4){if(timed&&draft.duration_minutes>=3)draft.warn3^=1;else notice="Set duration to at least 3 min";}
        else if(row==5)open_mode_page();
        else if(row==6)page=PAGE_DAYS;
        else if(row==7){draft.enabled^=1;nova_delete_confirm=false;}
        else if(row==8){page=PAGE_SAVE;nova_delete_confirm=false;}
        else if(row==9){if(!nova_delete_confirm){nova_delete_confirm=true;notice="Tap delete again to confirm";}else{draft=(points_item){0};save_action();nova_delete_confirm=false;nova_edit_scroll=0;}}
    } else if(page==PAGE_TYPE) {
        if(y>=60&&y<216){unsigned choices[8],count=nova_type_choices(choices),at=nova_type_scroll+(unsigned)(y-60)/38u;if(at<count){unsigned kind=choices[at];
            if(kind&&kind<POINTS_CUSTOM_1){draft.kind=(uint8_t)kind;if(!duration_kind(kind)){draft.duration_minutes=0;draft.notify_end=draft.warn3=0;}page=PAGE_EDIT;}
            else {if(!kind){for(unsigned i=0;i<POINTS_CUSTOM_COUNT;i++)if(!writer.meta.custom[i].name[0]){kind=POINTS_CUSTOM_1+i;break;}}custom_kind=kind;custom_draft=writer.meta;unsigned ci=custom_index(kind);
                if(ci<POINTS_CUSTOM_COUNT&&!custom_draft.custom[ci].name[0])snprintf(custom_draft.custom[ci].name,sizeof(custom_draft.custom[ci].name),"CUSTOM %u",ci+1);
                custom_pos=0;page=PAGE_CUSTOM;}
        }}
    } else if(page==PAGE_MODE) {
        if(y>=64&&y<220){unsigned row=(unsigned)(y-64)/36u;if(row<4){select_mode(row);}}
    } else if(page==PAGE_TIME) {
        if(y>=56&&y<216){int row=(y-56)/32,j=row-2;if(j){if(x<120){int h=(int)draft.hour+j;while(h<0)h+=24;draft.hour=(uint8_t)(h%24);}else{int m=(int)draft.minute+j*5;while(m<0)m+=60;draft.minute=(uint8_t)(m%60);}}}
    } else if(page==PAGE_DURATION) {
        if(y>=56&&y<216){int row=(y-56)/32,j=row-2;if(j)nova_duration_move(j);if(!draft.duration_minutes)draft.notify_end=draft.warn3=0;}
    } else if(page==PAGE_CUSTOM) {
        unsigned ci=custom_index(custom_kind);
        if(ci>=POINTS_CUSTOM_COUNT)return;
        if(y>=62&&y<126){if(x<80)custom_cycle(-1);else if(x>160)custom_cycle(1);else custom_pos=(custom_pos+1)%POINTS_CUSTOM_NAME_MAX;}
        else if(y>=140&&y<184){unsigned col=(unsigned)x*8u/240u;if(col<8)custom_draft.custom[ci].color=(uint8_t)col;}
        else if(y>=184){if(x<120)page=PAGE_TYPE;else{custom_trim(custom_draft.custom[ci].name);if(!custom_draft.custom[ci].name[0]){notice="Custom name cannot be empty";return;}
            custom_draft.revision=writer.meta.revision+1;int32_t rc=points_writer_save_meta(&writer,storage,&custom_draft);if(rc!=ALARM_OK){notice="Custom type save failed";return;}
            draft.kind=(uint8_t)custom_kind;page=PAGE_EDIT;notice="Custom type saved";}}
    } else if(page==PAGE_DAYS) {
        if(y>=62&&y<106&&x>=15&&x<225){unsigned d=(unsigned)(x-15)*7u/210u;if(d<7)draft.weekdays^=(uint8_t)(1u<<d);}
        else if(y>=108&&y<140)draft.weekdays=127;
        else if(y>=140&&y<172)draft.weekdays=62;
        else if(y>=172&&y<204)draft.weekdays=65;
        else if(y>=204&&y<236)draft.weekdays=0;
    } else if(page==PAGE_SAVE) {
        if(y>=170){if(x<120)page=PAGE_EDIT;else save_action();}
    }
}
static bool nova_scroll_step(int direction) {
    if(page==PAGE_LIST) {
        unsigned order[POINTS_MAX],count=nova_configured(order),rows=count+(count<POINTS_MAX?1u:0u),max=rows>4?rows-4:0,old=nova_list_scroll;
        if(direction>0&&nova_list_scroll<max)nova_list_scroll++;else if(direction<0&&nova_list_scroll)nova_list_scroll--;
        return old!=nova_list_scroll;
    }
    if(page==PAGE_EDIT){unsigned old=nova_edit_scroll;if(direction>0&&nova_edit_scroll<6)nova_edit_scroll++;else if(direction<0&&nova_edit_scroll)nova_edit_scroll--;return old!=nova_edit_scroll;}
    if(page==PAGE_TYPE){unsigned choices[8],count=nova_type_choices(choices),max=count>4?count-4:0,old=nova_type_scroll;if(direction>0&&nova_type_scroll<max)nova_type_scroll++;else if(direction<0&&nova_type_scroll)nova_type_scroll--;return old!=nova_type_scroll;}
    if(page==PAGE_CUSTOM){custom_cycle(direction);return true;}
    if(page==PAGE_TIME){if(nova_drag_x<120){draft.hour=(uint8_t)((draft.hour+(direction>0?1:23))%24);}else{int m=(int)draft.minute+(direction>0?5:-5);while(m<0)m+=60;draft.minute=(uint8_t)(m%60);}return true;}
    if(page==PAGE_DURATION){nova_duration_move(direction);return true;}
    return false;
}
static __attribute__((unused)) bool nova_contact_update(const springboard_contact *c) {
    if(!c||c->cancelled||!c->valid){nova_drag_active=false;return false;}
    if(c->began){nova_drag_active=true;nova_drag_x=c->x;nova_drag_y=c->y;return false;}
    bool changed=false;
    if(c->down&&nova_drag_active&&(page==PAGE_LIST||page==PAGE_EDIT||page==PAGE_TYPE||page==PAGE_TIME||page==PAGE_DURATION||page==PAGE_CUSTOM)) {
        int d=c->y-nova_drag_y;
        while(d>=24||d<=-24){int direction=d<0?1:-1;changed|=nova_scroll_step(direction);nova_drag_y+=d<0?-24:24;d=c->y-nova_drag_y;}
    }
    if(c->released)nova_drag_active=false;
    return changed;
}
static __attribute__((unused)) bool nova_buttons(uint32_t buttons) {
    if(buttons&T5_APP_BUTTON_UP){nova_drag_x=60;return nova_scroll_step(-1);}
    if(buttons&T5_APP_BUTTON_DOWN){nova_drag_x=60;return nova_scroll_step(1);}
    if(page==PAGE_TIME&&buttons&T5_APP_BUTTON_LEFT){nova_drag_x=60;return nova_scroll_step(-1);}
    if(page==PAGE_TIME&&buttons&T5_APP_BUTTON_RIGHT){nova_drag_x=180;return nova_scroll_step(1);}
    return false;
}
static __attribute__((unused)) void on_tap(int x,int y) {
#ifdef PORTABLE_NOVA_UI
    nova7_tap(x,y);
#else
    if(nova)nova_tap(x,y);else on_tap_legacy(x,y);
#endif
}

static bool on_back(void);
#include "points_paper.inc"

static bool on_back(void) {
    if(!points_live())return false;
    if(writer.uncertain||writer.meta_uncertain){notice="Retry save before leaving";return false;}
    if(service_valid && service_state.occurrence.generation){notice="Dismiss before leaving";return false;}
    if(page==PAGE_LIST) {
#ifdef POINTS_RETURN_APP
        /* Deployment selects a destination; only confirmed root Back queues it.
           The generic adapter must NOT receive PORTABLE_RETURN_APP for Points. */
        if(!runtime || !runtime->request_launch || !runtime->request_launch(POINTS_RETURN_APP)) {
            notice="Return unavailable: retry";return false;
        }
#endif
        return true;
    }
#ifdef PORTABLE_NOVA_UI
    if(page==PAGE_EDIT){page=PAGE_LIST;notice="Draft discarded";nova_edit_scroll=0;nova_delete_confirm=false;}
    else if(page==PAGE_CUSTOM_KEYBOARD){p7_key_cancel();notice="Name edit discarded";}
    else if(page==PAGE_CUSTOM){page=PAGE_TYPE;notice="Custom edit discarded";}
    else {page=PAGE_EDIT;notice="Draft only - not saved";nova_delete_confirm=false;}
#else
    if(page==PAGE_EDIT){page=PAGE_LIST;notice="Draft discarded";nova_edit_scroll=0;nova_delete_confirm=false;}
    else if(page==PAGE_CUSTOM){page=PAGE_TYPE;notice="Custom edit discarded";}
    else {page=PAGE_EDIT;notice="Draft only - not saved";nova_delete_confirm=false;}
#endif
    return false;
}
void app_main(void) {
#ifdef ALARM_NATIVE_UTC
    if(points_retained)return;
    points_clock=(portable_realtime_client){0};points_zone[0]=0;points_zone_index=0;
    points_clock_status=PORTABLE_REALTIME_UNSET;points_zone_status=PORTABLE_TIMEZONE_UNAVAILABLE;
#endif
    app=t5_app_get_api(1);writer=(points_writer){0};draft=(points_item){0};page=PAGE_LIST;list_page=selected=0;
#ifdef PORTABLE_PRODUCTIVITY_SCROLL
    pe_scroll=(productivity_scroll){0};memset(&pe_scroll_state,0,sizeof(pe_scroll_state));
#endif
    nova=NULL;paper=NULL;pe_first=pe_edit_first=pe_choice_first=pe_focus=pe_key_page=0;pe_focus_visible=pe_exit=pe_down=false;pe_clean=true;memset(pe_key_text,0,sizeof(pe_key_text));
    nova_list_scroll=nova_edit_scroll=nova_type_scroll=custom_kind=custom_pos=custom_key_page=custom_key_choice=0;nova_drag_x=nova_drag_y=0;nova_drag_active=nova_delete_confirm=false;custom_draft=(points_meta){0};
    time_format=PORTABLE_TIME_FORMAT_12;service_state=(alarm_status_v1){0};service_valid=clock_valid=ready=false;notice="";
    if(!app || app->abi_version!=1 || app->struct_size<offsetof(t5_app_api_v1,draw_label)+sizeof(app->draw_label) ||
       !app->poll || !app->millis || !app->screen_width || !app->screen_height || !app->clear || !app->draw_label ||
       !app->fill_rect || !app->present)return;
    paper=paper_presentation_get();
    if(paper&&(paper->struct_size<sizeof(*paper)||!paper->begin||!paper->text||!paper->measure||!paper->circle||!paper->contact))paper=NULL;
    if(!paper&&(app->screen_width()!=240||app->screen_height()!=240))return;
    if(app->set_back_exits_app)app->set_back_exits_app(false);
#ifdef PORTABLE_NOVA_UI
    p7_picker_reset();p7_contact_active=p7_contact_moved=false;
#endif
    nova=paper?NULL:springboard_presentation_get();
    if(nova&&(!nova->begin||!nova->caption||!nova->contact))nova=NULL;
    ready=open_dependencies();if(!points_live())return;if(ready)load_catalog();if(!points_live())return;draw();if(!points_live())return;uint32_t last_draw=app->millis();
#ifdef ALARM_NATIVE_UTC
    uint32_t last_time_sample=last_draw;
#endif
    for(;;) {
        t5_app_input_t input={0};bool poll_ok=app->poll(&input,20);if(!points_live())return;
        if(!poll_ok) {
#ifdef PORTABLE_ALARM_CLIENT
            /* Adapter owns settled/error barriers and bounded output cleanup. */
            if(portable_app_sleep_retained())return;
#else
            if(ready) {
                int32_t stopped=ALARM_PENDING;
                for(unsigned i=0;i<3 && stopped==ALARM_PENDING;i++) {
                    stopped=service->stop_only(service->context);
                    if(stopped==ALARM_PENDING)runtime->yield_ms(1);
                }
                if(stopped!=ALARM_OK) {
                    if(runtime->diagnostic)runtime->diagnostic("POINTS foreground-failed output-stop-unconfirmed; invocation retained");
                    for(;;)runtime->yield_ms(50);
                }
            }
#endif
            break;
        }
        /* Accepted global Home consumes Back before reporting its handoff.
         * Legacy Watch header taps also set exit_requested, but carry Back:
         * keep those app-owned so nested navigation and save guards still run. */
        if(input.exit_requested && !(input.buttons&T5_APP_BUTTON_BACK))break;
#ifndef PORTABLE_ALARM_CLIENT
        if(ready)(void)points_service_result(service->step(service->context));
        if(!points_live())return;
#endif
        alarm_status_v1 previous_status=service_state;bool previous_valid=service_valid;
        if(ready)refresh_status();
        if(!points_live())return;
#ifdef ALARM_NATIVE_UTC
        bool old_clock_valid=clock_valid;unsigned old_zone_index=points_zone_index;
        if(ready&&(uint32_t)(app->millis()-last_time_sample)>=500){
            last_time_sample=app->millis();uint32_t now;clock_valid=read_clock(&now);
        }
        if(!points_live())return;
#endif
        bool status_changed=previous_valid!=service_valid||previous_status.state!=service_state.state||previous_status.error!=service_state.error||previous_status.occurrence.generation!=service_state.occurrence.generation;
#ifdef ALARM_NATIVE_UTC
        status_changed|=old_clock_valid!=clock_valid||old_zone_index!=points_zone_index;
#endif
        bool back=(input.buttons&T5_APP_BUTTON_BACK) || (input.tapped && hit(input.touch_x,input.touch_y,0,0,48,31));
        if(back){if(on_back())break;if(paper)pe_reset_page();draw();if(!points_live())return;last_draw=app->millis();continue;}
        bool interacted=false;
        if(paper){interacted=pe_input(&input);if(pe_exit)break;}
        else {
#ifdef PORTABLE_NOVA_UI
        if(!p7_picker_active()&&!p7_contact_active)interacted|=nova7_buttons(input.buttons);
        if(nova&&nova->contact) {
            springboard_contact contact={0};nova->contact(&contact);
            interacted|=nova7_contact_update(&contact);
            if(contact.released&&contact.tap_eligible&&!p7_contact_moved&&!p7_picker_dragged()&&!contact.cancelled){nova7_tap(contact.x,contact.y);interacted=true;}
        } else if(input.tapped){nova7_tap(input.touch_x,input.touch_y);interacted=true;}
        interacted|=p7_picker_tick(app->millis());
#else
        if(nova) {
            springboard_contact contact={0};nova->contact(&contact);
            interacted|=nova_buttons(input.buttons);
            interacted|=nova_contact_update(&contact);
            if(contact.released&&contact.tap_eligible){nova_tap(contact.x,contact.y);interacted=true;}
        } else if(input.tapped){on_tap(input.touch_x,input.touch_y);interacted=true;}
#endif
        }
        if(!points_live())return;
        if(interacted || (paper?status_changed:(uint32_t)(app->millis()-last_draw)>=500)) {
            if(ready){uint32_t now;clock_valid=read_clock(&now);}
            if(!points_live())return;
            draw();if(!points_live())return;last_draw=app->millis();
        }
    }
    close_dependencies();
}
