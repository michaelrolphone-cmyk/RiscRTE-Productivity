/* Original shared Points in Time application. The ordinary alarm service owns
 * recurrence, durable occurrences, arbitration and all sound/vibration output. */
#ifdef PORTABLE_RETURN_APP
#error Points requires app-owned POINTS_RETURN_APP, never generic pre-poll return
#endif
#include "T5AppApi.h"
#include "RiscRuntimeV1.h"
#include "PortableRtcClock.h"
#include "PortableTime.h"
#include "PortableTimeFormat.h"
#include "points_writer.h"
#ifdef PORTABLE_ALARM_CLIENT
#include "PortableAppSleep.h"
#endif
#include <stddef.h>
#include <stdio.h>

enum { PAGE_LIST, PAGE_EDIT, PAGE_TYPE, PAGE_TIME, PAGE_DAYS, PAGE_MODE, PAGE_DURATION, PAGE_SAVE };
static const t5_app_api_v1 *app;
static const risc_runtime_api_v1 *runtime;
static const risc_key_value_v1 *storage,*preferences;
static const twatch_rtc_api_v1 *rtc;
static const alarm_service_v1 *service;
static risc_runtime_capability_v1 grants[4];
static unsigned acquired,page,list_page,selected,time_format;
static points_writer writer;
static points_item draft;
static alarm_status_v1 service_state;
static bool ready,service_valid,clock_valid;
static const char *notice;
static const char *const kinds[]={"Empty","Work start","Work end","Lunch","Break","Bedtime"};
static const char *const modes[]={"System default","Vibrate","Sound","Sound and vibrate"};
static const char *const days[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
static bool duration_kind(unsigned kind) { return kind==POINTS_LUNCH || kind==POINTS_BREAK; }
static bool read_clock(uint32_t *seconds) {
    twatch_rtc_time_v1 t;
    return rtc && rtc->read(rtc->context,&t) && t.weekday<=6 &&
        alarm_calendar_seconds(t.year,t.month,t.day,t.hour,t.minute,t.second,seconds);
}
static void refresh_status(void) {
    service_state=(alarm_status_v1){.struct_size=sizeof(service_state)};
    service_valid=service && service->status(service->context,&service_state)==ALARM_OK;
}
static void close_dependencies(void) {
    if(runtime)while(acquired)runtime->release(&grants[--acquired]);
    storage=preferences=NULL;rtc=NULL;service=NULL;runtime=NULL;ready=false;
}
static bool open_dependencies(void) {
    runtime=risc_runtime_get_api(1);acquired=0;
    if(!runtime || runtime->api_version!=1 || runtime->struct_size<RISC_RUNTIME_CAPABILITIES_V1_SIZE ||
       !runtime->acquire || !runtime->release || !runtime->yield_ms)return false;
    const char *caps[]={"storage.key-value","rtc.clock",ALARM_SERVICE_CAPABILITY,"storage.key-value"};
    const unsigned apis[]={1,2,1,1};const uint64_t instances[]={5,0,0,1};
    for(unsigned i=0;i<4;i++) {
        grants[i]=(risc_runtime_capability_v1){.struct_size=sizeof(grants[i])};
        if(!runtime->acquire(caps[i],apis[i],instances[i],&grants[i]))return false;
        acquired++;
    }
    storage=grants[0].api;rtc=grants[1].api;service=grants[2].api;preferences=grants[3].api;
    return points_writer_storage_valid(storage) && portable_time_format_api_valid(preferences) &&
        rtc && rtc->api_version==2 && rtc->struct_size>=sizeof(*rtc) && rtc->read &&
        service && service->api_version==1 && service->struct_size>=sizeof(*service) && service->status &&
        service->step && service->refresh && service->acknowledge && service->stop_only;
}
static void load_catalog(void) {
    (void)points_writer_load(&writer,storage);
    (void)portable_time_format_load(preferences,&time_format);
    uint32_t now;clock_valid=read_clock(&now);refresh_status();
    notice=writer.loaded?"Tap a point to edit":"Storage invalid: retry";
}
static void format_time(const points_item *p,char *out,size_t cap) {
    if(time_format==PORTABLE_TIME_FORMAT_24)snprintf(out,cap,"%02u:%02u",p->hour,p->minute);
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
    if(writer.uncertain)return "Save unconfirmed: retry";
    if(!writer.loaded)return "Storage invalid: retry";
    if(!clock_valid)return "RTC invalid: retry";
    if(!service_valid)return "Service unavailable: retry";
    if(service_state.state==ALARM_STATE_BLOCKED)return service_state.error==ALARM_RTC?"RTC changed: retry":"Service error: retry";
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
static void draw(void) {
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
        snprintf(text,sizeof(text),"Alert: %s",modes[draft.mode]);label(8,132,224,text);line(153);
        if(duration_kind(draft.kind))snprintf(text,sizeof(text),"Duration: %u min",draft.duration_minutes);
        else snprintf(text,sizeof(text),"Duration: not used");
        label(8,162,224,text);line(183);button(8,188,106,draft.enabled?"On":"Off");button(124,188,108,"Save...");
    } else if(page==PAGE_TYPE) {
        for(unsigned i=1;i<=5;i++){snprintf(text,sizeof(text),"%s%s",draft.kind==i?"X ":"",kinds[i]);button(8,39+(int)(i-1)*32,224,text);}
    } else if(page==PAGE_MODE) {
        for(unsigned i=0;i<4;i++){snprintf(text,sizeof(text),"%s%s",draft.mode==i?"X ":"",modes[i]);button(8,47+(int)i*37,224,text);}
    } else if(page==PAGE_TIME) {
        label(8,42,224,"24-hour editor (HH:MM)");button(24,70,80,"Hour up");button(136,70,80,"Min up");
        snprintf(text,sizeof(text),"%02u : %02u",draft.hour,draft.minute);label(24,114,192,text);
        button(24,142,80,"Hour down");button(136,142,80,"Min down");label(8,174,224,portable_time_zone());button(8,185,224,"Done");
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
static void edit_slot(unsigned slot) {
    selected=slot;draft=writer.saved.points[slot];
    if(!draft.kind)draft=(points_item){.kind=POINTS_WORK_START};
    page=PAGE_EDIT;notice="Draft only - not saved";
}
static void save_action(void) {
    int32_t rc;
    if(writer.uncertain)rc=points_writer_retry(&writer,storage);
    else {
        if(!writer.loaded){notice="Storage invalid: retry";return;}
        if(writer.saved.revision==UINT32_MAX){notice="Revision limit reached";return;}
        if(draft.enabled && !draft.weekdays){notice="Choose at least one day";page=PAGE_DAYS;return;}
        if(!service_valid || service_state.state==ALARM_STATE_BLOCKED){notice="Service blocked: retry";return;}
        uint32_t now;clock_valid=read_clock(&now);if(!clock_valid){notice="RTC invalid: retry";return;}
        if(now>ALARM_RTC_MAX-ALARM_RECOVERY_SECONDS){notice="RTC range limit";return;}
        points_config desired=writer.saved;desired.revision++;desired.created=now;desired.points[selected]=draft;
        rc=points_writer_save(&writer,storage,&desired);
    }
    if(rc!=ALARM_OK){notice=writer.uncertain?"Save unconfirmed: retry":"Point or revision invalid";return;}
    page=PAGE_LIST;notice="Catalog saved";(void)service->refresh(service->context);
}
static void retry_action(void) {
    if(!ready) {
        close_dependencies();ready=open_dependencies();
        if(ready)load_catalog();
        return;
    }
    if(writer.uncertain){save_action();return;}
    if(page==PAGE_LIST || !writer.loaded)load_catalog();
    uint32_t now;clock_valid=read_clock(&now);(void)service->refresh(service->context);refresh_status();notice="Refreshed";
}
static void on_tap(int x,int y) {
    if(!ready) {
        if(hit(x,y,8,176,224,34))retry_action();
        return;
    }
#ifndef PORTABLE_ALARM_CLIENT
    if(service_valid && service_state.occurrence.generation) {
        if(hit(x,y,8,176,224,34))(void)service->acknowledge(service->context,&service_state.occurrence);
        return;
    }
#endif
    if(writer.uncertain || !writer.loaded) {
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
            page=row==0?PAGE_TYPE:row==1?PAGE_TIME:row==2?PAGE_DAYS:row==3?PAGE_MODE:duration_kind(draft.kind)?PAGE_DURATION:PAGE_EDIT;
        } else if(hit(x,y,8,188,106,28))draft.enabled^=1;
        else if(hit(x,y,124,188,108,28))page=PAGE_SAVE;
    } else if(page==PAGE_TYPE) {
        for(unsigned i=1;i<=5;i++)if(hit(x,y,8,39+(int)(i-1)*32,224,28)) {
            draft.kind=(uint8_t)i;if(!duration_kind(i))draft.duration_minutes=0;page=PAGE_EDIT;break;
        }
    } else if(page==PAGE_MODE) {
        for(unsigned i=0;i<4;i++)if(hit(x,y,8,47+(int)i*37,224,28)){draft.mode=(uint8_t)i;page=PAGE_EDIT;break;}
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
static bool on_back(void) {
    if(writer.uncertain){notice="Retry save before leaving";return false;}
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
    if(page==PAGE_EDIT){page=PAGE_LIST;notice="Draft discarded";}
    else {page=PAGE_EDIT;notice="Draft only - not saved";}
    return false;
}
void app_main(void) {
    app=t5_app_get_api(1);writer=(points_writer){0};draft=(points_item){0};page=PAGE_LIST;list_page=selected=0;
    time_format=PORTABLE_TIME_FORMAT_12;service_state=(alarm_status_v1){0};service_valid=clock_valid=ready=false;notice="";
    if(!app || app->abi_version!=1 || app->struct_size<offsetof(t5_app_api_v1,draw_label)+sizeof(app->draw_label) ||
       !app->poll || !app->millis || !app->screen_width || !app->screen_height || !app->clear || !app->draw_label ||
       !app->fill_rect || !app->present || app->screen_width()!=240 || app->screen_height()!=240)return;
    if(app->set_back_exits_app)app->set_back_exits_app(false);
    ready=open_dependencies();if(ready)load_catalog();draw();uint32_t last_draw=app->millis();
    for(;;) {
        t5_app_input_t input={0};bool poll_ok=app->poll(&input,20);
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
#ifndef PORTABLE_ALARM_CLIENT
        if(ready)(void)service->step(service->context);
#endif
        if(ready)refresh_status();
        bool back=input.exit_requested || (input.buttons&T5_APP_BUTTON_BACK) || (input.tapped && hit(input.touch_x,input.touch_y,0,0,48,31));
        if(back){if(on_back())break;draw();last_draw=app->millis();continue;}
        if(input.tapped)on_tap(input.touch_x,input.touch_y);
        if(input.tapped || (uint32_t)(app->millis()-last_draw)>=500) {
            if(ready){uint32_t now;clock_valid=read_clock(&now);}
            draw();last_draw=app->millis();
        }
    }
    close_dependencies();
}
