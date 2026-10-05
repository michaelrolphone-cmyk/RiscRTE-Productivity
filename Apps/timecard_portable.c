/* Portable NOVA profile of the authoritative Timecard. The original source is
 * compiled once below, not copied into a second model. Its legacy entry and T5
 * getters are private to this profile; the Reader build remains byte-identical. */
#define app_main tcp_source_main
#define t5_app_get_api tcp_source_app_api
#define t5_storage_get_api tcp_source_storage_api
#define t5_system_get_api tcp_source_system_api
#define t5_system_ui_get_api tcp_source_system_ui_api
#define t5_ui_get_api tcp_source_ui_api
#include "timecard.c"
#undef app_main
#undef t5_app_get_api
#undef t5_storage_get_api
#undef t5_system_get_api
#undef t5_system_ui_get_api
#undef t5_ui_get_api

#include "RiscRuntimeV1.h"
#include "PortableNovaUi.h"
#include "PortableWatchKeyboard.h"
#include "PortableRtcClock.h"
#include "PortableTime.h"
#include "PortableTimeFormat.h"
#include "timecard_portable_validation.h"
#include <stdio.h>

const t5_app_api_v1 *t5_app_get_api(uint32_t version);
/* App-local link seam, not a new Runtime ABI. No writable Watch backend exists
 * in this profile. A future real implementation must preserve this complete-file
 * contract, with exists=false ONLY for confirmed absence. Never bind a dummy
 * empty store or an installed-files/64-byte/2048-byte KV capability here. */
#ifdef TIMECARD_APP_DATA
#include "timecard_appdata_bridge.h"
#ifndef TIMECARD_APP_DATA_INSTANCE
#define TIMECARD_APP_DATA_INSTANCE 1u
#endif
static tcp_appdata tcp_data;
static bool tcp_ad_exists(const char *path) { return tcp_appdata_exists(&tcp_data,path); }
static bool tcp_ad_read(const char *path,void *buffer,size_t capacity,size_t *size) { return tcp_appdata_read(&tcp_data,path,buffer,capacity,size); }
static bool tcp_ad_write(const char *path,const void *data,size_t size) { return tcp_appdata_write(&tcp_data,path,data,size); }
static const t5_storage_api_v1 tcp_ad_files={.api_version=T5_STORAGE_API_VERSION,.struct_size=sizeof(tcp_ad_files),.exists=tcp_ad_exists,.read_file=tcp_ad_read,.write_file_atomic=tcp_ad_write};
static const t5_storage_api_v1 *timecard_portable_file_storage(void) { return tcp_data.api?&tcp_ad_files:NULL; }
static bool tcp_storage_retained(void) { return tcp_data.retained; }
#else
#ifndef TIMECARD_FILE_STORAGE_EXTERNAL
__attribute__((weak)) const t5_storage_api_v1 *timecard_portable_file_storage(void) { return NULL; }
#else
const t5_storage_api_v1 *timecard_portable_file_storage(void);
#endif
static bool tcp_storage_retained(void) { return false; }
#endif
#ifndef TIMECARD_RETURN_APP
#define TIMECARD_RETURN_APP "springboard.elf"
#endif
#ifdef PORTABLE_RETURN_APP
#error "Timecard owns nested/root return; do not use generic pre-poll return"
#endif

#define TCP_TOP 82
#define TCP_BOTTOM 180
#define TCP_ROW_HEIGHT 48
#define TCP_TABLE_HEIGHT 76
static const risc_runtime_api_v1 *tcp_runtime;
static const twatch_rtc_api_v1 *tcp_rtc;
static const t5_storage_api_v1 *tcp_files;
static t5_local_datetime_t tcp_snapshot;
static bool tcp_clock_valid;
static risc_runtime_capability_v1 tcp_grants[3];
static unsigned tcp_grant_count,tcp_time_format;
static bool tcp_home,tcp_editor,tcp_dirty,tcp_drag,tcp_armed,tcp_moved,tcp_external_exit;
static int tcp_scroll,tcp_start_scroll,tcp_start_x,tcp_start_y,tcp_last_x,tcp_last_y;
static unsigned tcp_axis,tcp_key_page,tcp_key_choice,tcp_draw_screen;
static int32_t tcp_draw_week,tcp_draw_day;
static uint64_t tcp_cookie;
static char tcp_entry[13],tcp_editor_title[32],tcp_editor_status[STATUS_CAP];
static tc_day_t tcp_before[MAX_DAYS];
static unsigned tcp_render_rows,tcp_render_height;

static void tcp_reset_gesture(void) { tcp_drag=tcp_moved=false;tcp_axis=0;tcp_armed=false; }
static bool tcp_read_datetime(t5_local_datetime_t *out) {
    twatch_rtc_time_v1 raw,local;
    if(!out || !tcp_rtc || !tcp_rtc->read(tcp_rtc->context,&raw) || !portable_time_forward(&raw,&local))return false;
    civil_t c={local.year,local.month,local.day};
    unsigned yd=local.day-1;for(unsigned m=1;m<local.month;m++)yd+=(unsigned)month_days(local.year,(int)m);
    *out=(t5_local_datetime_t){(int16_t)c.year,local.month,local.day,local.hour,local.minute,local.second,local.weekday,(uint16_t)yd};
    return true;
}
static bool tcp_local_datetime(t5_local_datetime_t *out) {
    if(!out || !tcp_clock_valid)return false;
    *out=tcp_snapshot;return true;
}
static bool tcp_store_exists(const char *path) { return tcp_files && tcp_files->exists(path); }
static bool tcp_store_read(const char *path,void *buffer,size_t capacity,size_t *size) {
    if(size)*size=0;
    if(!tcp_files || !buffer || !size)return false;
    size_t got=0;
    if(!tcp_files->read_file(path,buffer,capacity,&got) || got>capacity || !tcp_validate_json(buffer,got))return false;
    *size=got;return true;
}
static bool tcp_store_write(const char *path,const void *data,size_t size) {
    return tcp_files && size<JSON_CAPACITY && tcp_validate_json(data,size) && tcp_files->write_file_atomic(path,data,size);
}
static const t5_storage_api_v1 tcp_store={.api_version=T5_STORAGE_API_VERSION,.struct_size=sizeof(tcp_store),.exists=tcp_store_exists,.read_file=tcp_store_read,.write_file_atomic=tcp_store_write};
static const t5_system_api_v1 tcp_clock={T5_SYSTEM_API_VERSION,sizeof(tcp_clock),tcp_local_datetime};
static void tcp_home_request(void) { tcp_home=true; }
static bool tcp_keyboard_request(const char *title,const char *initial,size_t maximum,uint8_t type,uint64_t cookie) {
    if(tcp_editor || type!=T5_SYSTEM_KEYBOARD_TEXT || !maximum || maximum>12 || !title || !initial || slen(initial)>maximum)return false;
    scopy(tcp_editor_title,sizeof(tcp_editor_title),title);scopy(tcp_entry,sizeof(tcp_entry),initial);
    tcp_cookie=cookie;tcp_key_page=PWK_INITIAL_PAGE;tcp_key_choice=0;tcp_editor=true;
    scopy(tcp_editor_status,sizeof(tcp_editor_status),"DONE saves; Back cancels");tcp_reset_gesture();tcp_dirty=true;return true;
}
/* The portable controller owns the same-invocation keyboard workflow. There is
 * deliberately no fake firmware relaunch/mailbox, nor a pending result shared
 * with another app. The original legacy entry below is not the profile entry. */
static bool tcp_no_keyboard_result(char *text,size_t capacity,bool *cancelled,uint64_t *cookie) {
    (void)text;(void)capacity;(void)cancelled;(void)cookie;return false;
}
static const t5_system_ui_api_v1 tcp_system_ui={.api_version=T5_SYSTEM_UI_API_VERSION,.struct_size=sizeof(tcp_system_ui),.keyboard_request=tcp_keyboard_request,.keyboard_take_result=tcp_no_keyboard_result,.navigate_home=tcp_home_request};

static bool tcp_reload(void) {
    store_ready=false;
    if(!tcp_files || !load_store()) {set_status("History unavailable; Retry");tcp_dirty=true;return false;}
    set_status("History loaded");tcp_dirty=true;return true;
}
/* Every portable mutation passes this preflight. Preserve all 400 existing days
 * rather than entering the legacy ensure_day eviction path. Unconfirmed saves
 * restore the displayed snapshot and lock edits until a complete durable reload;
 * the backing write may still have committed, so no rollback claim is made. */
static void tcp_compact_empty_days(void) {
    int32_t kept=0;
    for(int32_t i=0;i<day_count;i++)if(any_punch(&days[i]))days[kept++]=days[i];
    day_count=kept;
}
static bool tcp_mutate(int32_t date,uint8_t punch,int16_t minutes) {
    if(!store_ready){set_status("History unavailable; Retry");return false;}
    if(!tcpv_date(date) || punch>=PUNCH_COUNT || minutes< -1 || minutes>1439){set_status("Invalid punch");return false;}
    int32_t before_count=day_count;memcpy(tcp_before,days,(size_t)day_count*sizeof(days[0]));
    if(!find_day(date) && day_count>=MAX_DAYS) {
        /* Serialization already omits blank days. Reclaim only those empty
         * records, never a day containing a punch; failure restores the snapshot. */
        tcp_compact_empty_days();
        if(day_count>=MAX_DAYS){set_status("History full: 400 days");return false;}
    }
    if(!set_punch(date,punch,minutes)) {
        memcpy(days,tcp_before,(size_t)before_count*sizeof(days[0]));day_count=before_count;store_ready=false;
        set_status("Save unconfirmed; Retry history");return false;
    }
    /* Keep RAM consistent with the confirmed serializer, including clearing the
     * final punch of a day. This is not eviction of recorded history. */
    tcp_compact_empty_days();
    punch_status(punch,minutes);
    if(tcp_time_format==PORTABLE_TIME_FORMAT_24 && minutes>=0)snprintf(status_text,sizeof(status_text),"%s %02u:%02u",punch_name(punch),(unsigned)minutes/60,(unsigned)minutes%60);
    return true;
}
static void tcp_punch(uint8_t punch) {
    t5_local_datetime_t now;
    if(!store_ready){set_status("History unavailable; Retry");return;}
    if(!tcp_read_datetime(&now)){set_status("Clock unavailable");return;}
    tcp_snapshot=now;tcp_clock_valid=true;
    int32_t date=make_ymd(now.year,now.month,now.day);
    if(tcp_mutate(date,punch,(int16_t)(now.hour*60+now.minute))) {
        screen_id=SCREEN_WEEK;week_offset=0;editing_ymd=0;selected=now.weekday;
    }
}
static void tcp_display_time(const char *source,char *out,size_t cap) {
    int16_t minutes;
    if(tcp_time_format==PORTABLE_TIME_FORMAT_24 && tcp_parse_time(source,&minutes) && minutes>=0)
        snprintf(out,cap,"%02u:%02u",(unsigned)minutes/60,(unsigned)minutes%60);
    else scopy(out,cap,source);
}
static void tcp_chrome(const t5_ui_chrome_t *chrome) {
    portable_nova_fill(0,0,240,TCP_TOP,0);portable_nova_fill(0,TCP_BOTTOM,240,240-TCP_BOTTOM,0);
    portable_nova_text(0,20,49,200,chrome->title,NOVA_CYAN);
    if(app->draw_icon)(void)app->draw_icon(194,18,"solid:f017",20,false);
    portable_nova_text(3,20,68,200,chrome->subtitle,NOVA_CAP);
    portable_nova_fill(20,80,200,1,NOVA_DIM);
    (void)portable_nova_wrap(3,20,184,200,12,2,chrome->status,NOVA_CAP);
    portable_nova_button(12,211,76,29,chrome->back_label,false);
    portable_nova_button(96,211,132,29,store_ready?chrome->confirm_label:"Retry",selected>=0);
}
static void tcp_adjust_view(uint32_t n,unsigned row_height,int32_t index) {
    bool changed=tcp_draw_screen!=screen_id || tcp_draw_week!=week_offset || tcp_draw_day!=editing_ymd;
    if(changed){tcp_scroll=index>0?index*(int)row_height:0;tcp_draw_screen=screen_id;tcp_draw_week=week_offset;tcp_draw_day=editing_ymd;tcp_reset_gesture();}
    tcp_render_rows=n;tcp_render_height=row_height;
    int max=(int)(n*row_height)-(TCP_BOTTOM-TCP_TOP);if(max<0)max=0;
    if(tcp_scroll<0)tcp_scroll=0;
    if(tcp_scroll>max)tcp_scroll=max;
    (void)index;
}
static void tcp_list(const t5_ui_chrome_t *chrome,const t5_ui_list_row_t *rows,uint32_t n,int32_t index) {
    tcp_adjust_view(n,TCP_ROW_HEIGHT,index);portable_nova_begin();
    for(unsigned i=0;i<n;i++) {
        int y=TCP_TOP+(int)i*TCP_ROW_HEIGHT-tcp_scroll;
        if(y+TCP_ROW_HEIGHT<=TCP_TOP || y>=TCP_BOTTOM)continue;
        if((int32_t)i==index)portable_nova_fill(20,y,200,TCP_ROW_HEIGHT,NOVA_LINE);
        portable_nova_fill(20,y,2,TCP_ROW_HEIGHT,(int32_t)i==index?NOVA_CYAN:NOVA_DIM);
        portable_nova_text(1,30,y+5,184,rows[i].title,NOVA_TEXT);
        char value[32];tcp_display_time(rows[i].value,value,sizeof(value));
        portable_nova_text(3,30,y+26,184,value[0]?value:rows[i].subtitle,NOVA_CYAN);
        portable_nova_fill(20,y+TCP_ROW_HEIGHT-1,200,1,NOVA_DIM);
    }
    tcp_chrome(chrome);app->present(false);
}
static void tcp_table(const t5_ui_chrome_t *chrome,const t5_ui_table_column_t *columns,uint32_t count,const t5_ui_table_row_t *rows,uint32_t n,int32_t index) {
    tcp_adjust_view(n,TCP_TABLE_HEIGHT,index);portable_nova_begin();
    for(unsigned i=0;i<n;i++) {
        int y=TCP_TOP+(int)i*TCP_TABLE_HEIGHT-tcp_scroll;
        if(y+TCP_TABLE_HEIGHT<=TCP_TOP || y>=TCP_BOTTOM)continue;
        bool action=!!(rows[i].flags&T5_UI_TABLE_ROW_FULL_WIDTH);
        if((int32_t)i==index)portable_nova_fill(20,y,200,TCP_TABLE_HEIGHT,NOVA_LINE);
        portable_nova_fill(20,y,2,TCP_TABLE_HEIGHT,(int32_t)i==index?NOVA_CYAN:NOVA_DIM);
        portable_nova_text(1,30,y+5,184,rows[i].cells[0],action?NOVA_CYAN:NOVA_TEXT);
        if(!action)for(unsigned c=1;c<count && c<=4;c++) {
            int x=30+(int)((c-1)%2)*98,cy=y+27+(int)((c-1)/2)*22;char value[32],line[48];
            tcp_display_time(rows[i].cells[c],value,sizeof(value));snprintf(line,sizeof(line),"%s %s",columns[c].title,value);
            portable_nova_text(3,x,cy,94,line,NOVA_CYAN);
        }
        else portable_nova_text(3,30,y+31,180,"Use current local time",NOVA_CAP);
        portable_nova_fill(20,y+TCP_TABLE_HEIGHT-1,200,1,NOVA_DIM);
    }
    tcp_chrome(chrome);app->present(false);
}
static int32_t tcp_hit(int16_t x,int16_t y) {
    if(x<20 || x>=220 || y<TCP_TOP || y>=TCP_BOTTOM || !tcp_render_height)return T5_UI_HIT_NONE;
    unsigned index=(unsigned)(y-TCP_TOP+tcp_scroll)/tcp_render_height;
    return index<tcp_render_rows?(int32_t)index:T5_UI_HIT_NONE;
}
static int32_t tcp_next(int32_t index,uint32_t n) {return n?(index+1)%(int32_t)n:0;}
static int32_t tcp_previous(int32_t index,uint32_t n) {return n?(index+(int32_t)n-1)%(int32_t)n:0;}
static bool tcp_no_poll(t5_ui_event_t *event,uint32_t wait) {(void)event;(void)wait;return false;}
static const t5_ui_api_v1 tcp_ui={.api_version=T5_UI_API_VERSION,.struct_size=sizeof(tcp_ui),.render_list=tcp_list,.render_table=tcp_table,.hit_test=tcp_hit,.poll_event=tcp_no_poll,.next_index=tcp_next,.previous_index=tcp_previous};
const t5_app_api_v1 *tcp_source_app_api(uint32_t version) {return t5_app_get_api(version);}
const t5_storage_api_v1 *tcp_source_storage_api(uint32_t version) {return version==1 && tcp_files?&tcp_store:NULL;}
const t5_system_api_v1 *tcp_source_system_api(uint32_t version) {return version==1?&tcp_clock:NULL;}
const t5_system_ui_api_v1 *tcp_source_system_ui_api(uint32_t version) {return version==1?&tcp_system_ui:NULL;}
const t5_ui_api_v1 *tcp_source_ui_api(uint32_t version) {return version==1?&tcp_ui:NULL;}

static void tcp_editor_draw(void) {
    portable_nova_begin();portable_nova_text(0,12,24,216,tcp_editor_title,NOVA_CYAN);
    portable_nova_text(1,12,49,216,tcp_entry,NOVA_TEXT);
    for(unsigned key=0;key<PWK_COUNT;key++) {
        portable_watch_key_rect r;portable_watch_key_bounds(key,&r);
        char caption[8]={0};unsigned ch=portable_watch_key_character(tcp_key_page,key);
        if(key<PWK_CHARACTERS) {if(ch==32)strcpy(caption,"SP");else if(ch){caption[0]=(char)ch;caption[1]=0;}}
        else scopy(caption,sizeof(caption),key==PWK_PAGE?"ABC/#":key==PWK_DELETE?"DELETE":"DONE");
        portable_nova_fill(r.x,r.y,r.w,r.h,key==tcp_key_choice?NOVA_DIM:NOVA_LINE);
        portable_nova_fill(r.x,r.y,r.w,1,NOVA_CYAN);
        portable_nova_text(2,r.x+5,r.y+7,r.w-10,caption,NOVA_CYAN);
    }
    (void)portable_nova_wrap(3,12,210,216,12,2,tcp_editor_status,NOVA_CAP);app->present(false);
}
static void tcp_editor_cancel(void) {tcp_editor=false;memset(tcp_entry,0,sizeof(tcp_entry));tcp_reset_gesture();tcp_dirty=true;}
static void tcp_editor_done(void) {
    int16_t minutes;int32_t date,offset;uint8_t punch;
    if(!decode_cookie(tcp_cookie,&date,&punch,&offset) || punch>=PUNCH_COUNT || date<19700101) {
        scopy(tcp_editor_status,sizeof(tcp_editor_status),"Invalid edit; Back to cancel");return;
    }
    if(!tcp_parse_time(tcp_entry,&minutes)) {scopy(tcp_editor_status,sizeof(tcp_editor_status),"Use H:MM AM/PM or 24-hour");return;}
    /* Re-read before writing, as the authoritative keyboard handoff does. */
    if(!tcp_reload() || !tcp_mutate(date,punch,minutes)) {
        scopy(tcp_editor_status,sizeof(tcp_editor_status),status_text);return;
    }
    week_offset=offset;editing_ymd=date;selected=punch;screen_id=SCREEN_DAY;tcp_editor_cancel();
}
static void tcp_key(unsigned key) {
    tcp_key_choice=key;
    if(key==PWK_PAGE)tcp_key_page=(tcp_key_page+1)%PWK_PAGES;
    else if(key==PWK_DELETE) {size_t n=slen(tcp_entry);if(n)tcp_entry[n-1]=0;}
    else if(key==PWK_DONE)tcp_editor_done();
    else {unsigned ch=portable_watch_key_character(tcp_key_page,key);size_t n=slen(tcp_entry);if(ch && n<12){tcp_entry[n]=(char)ch;tcp_entry[n+1]=0;}else if(ch)scopy(tcp_editor_status,sizeof(tcp_editor_status),"Time entry is limited to 12 characters");}
    tcp_dirty=true;
}
static void tcp_activate(void) {
    if(!store_ready){tcp_reload();return;}
    if(!tcp_clock_valid){tcp_dirty=true;return;}
    if(screen_id==SCREEN_WEEK_LIST)open_week(-selected);
    else if(screen_id==SCREEN_DAY) {
        tc_day_t day=get_day(editing_ymd);char initial[24];format_ampm(day.punches[selected],initial,sizeof(initial));
        if(day.punches[selected]<0)initial[0]=0;
        else if(tcp_time_format==PORTABLE_TIME_FORMAT_24)snprintf(initial,sizeof(initial),"%02u:%02u",(unsigned)day.punches[selected]/60,(unsigned)day.punches[selected]%60);
        if(!tcp_keyboard_request(punch_name((uint8_t)selected),initial,12,T5_SYSTEM_KEYBOARD_TEXT,make_cookie(editing_ymd,(uint8_t)selected,week_offset)))set_status("Keyboard unavailable");
    } else if(selected<DAY_COUNT)open_day(add_days(sunday(week_offset),selected));
    else tcp_punch((uint8_t)(selected-DAY_COUNT));
    tcp_dirty=true;
}
static void tcp_back(void) {if(tcp_editor)tcp_editor_cancel();else go_back();tcp_dirty=true;}
static void tcp_move(int direction) {
    if(tcp_editor){tcp_key_choice=(tcp_key_choice+PWK_COUNT+(direction>0?1:PWK_COUNT-1))%PWK_COUNT;}
    else {
        selected=direction>0?tcp_next(selected,(uint32_t)item_count()):tcp_previous(selected,(uint32_t)item_count());
        int y=selected*(int)tcp_render_height;
        if(y<tcp_scroll)tcp_scroll=y;
        else if(y+(int)tcp_render_height>tcp_scroll+TCP_BOTTOM-TCP_TOP)tcp_scroll=y+(int)tcp_render_height-(TCP_BOTTOM-TCP_TOP);
    }
    tcp_dirty=true;
}
static void tcp_tap(int x,int y) {
    if(tcp_editor){int key=portable_watch_key_hit(x,y);if(key>=0)tcp_key((unsigned)key);return;}
    if(portable_nova_hit(x,y,12,211,76,29)){tcp_back();return;}
    if(portable_nova_hit(x,y,96,211,132,29)){tcp_activate();return;}
    int32_t hit=tcp_hit((int16_t)x,(int16_t)y);if(hit>=0){selected=hit;tcp_activate();}
}
static void tcp_input(const t5_app_input_t *input) {
    if(input->exit_requested){tcp_external_exit=true;return;}
    if(input->buttons&T5_APP_BUTTON_BACK){tcp_reset_gesture();tcp_back();return;}
    t5_app_contact_t contact={0};bool down=app->touch_contact && app->touch_contact(&contact) && contact.down;
    if(down) {
        if(!tcp_armed)return;
        if(!tcp_drag){tcp_drag=true;tcp_moved=false;tcp_axis=0;tcp_start_x=tcp_last_x=contact.x;tcp_start_y=tcp_last_y=contact.y;tcp_start_scroll=tcp_scroll;}
        int dx=contact.x-tcp_start_x,dy=contact.y-tcp_start_y;tcp_last_x=contact.x;tcp_last_y=contact.y;
        int ax=dx<0?-dx:dx,ay=dy<0?-dy:dy;
        if(ax+ay>=6){tcp_moved=true;if(!tcp_axis)tcp_axis=ay>ax?1:2;}
        /* Contact loss cannot distinguish release from cancellation. Act on a
         * directional swipe only while a valid held sample is still present. */
        if(tcp_axis==2 && dx>=36 && dx>ay*2){tcp_back();tcp_reset_gesture();return;}
        if(tcp_axis==1 && !tcp_editor && tcp_start_y>=TCP_TOP && tcp_start_y<TCP_BOTTOM) {
            int max=(int)(tcp_render_rows*tcp_render_height)-(TCP_BOTTOM-TCP_TOP);if(max<0)max=0;
            int next=tcp_start_scroll-dy;if(next<0)next=0;if(next>max)next=max;
            if(next!=tcp_scroll){tcp_scroll=next;tcp_dirty=true;}
        }
        return;
    }
    bool moved=tcp_drag && tcp_moved;
    tcp_drag=false;
    bool was_armed=tcp_armed;tcp_armed=true;
    if(!was_armed && input->tapped)return;
    if(input->tapped && !moved){tcp_tap(input->touch_x,input->touch_y);return;}
    if(input->buttons&(T5_APP_BUTTON_UP|T5_APP_BUTTON_LEFT))tcp_move(-1);
    else if(input->buttons&(T5_APP_BUTTON_DOWN|T5_APP_BUTTON_RIGHT))tcp_move(1);
    else if(input->buttons&T5_APP_BUTTON_CONFIRM){if(tcp_editor)tcp_key(tcp_key_choice);else tcp_activate();}
}
static void tcp_dependencies_close(void) {
    if(tcp_storage_retained())return;
    if(tcp_runtime)while(tcp_grant_count)tcp_runtime->release(&tcp_grants[--tcp_grant_count]);
    tcp_rtc=NULL;tcp_runtime=NULL;tcp_files=NULL;
}
static void tcp_dependencies_open(void) {
    tcp_runtime=risc_runtime_get_api(1);tcp_grant_count=0;tcp_time_format=PORTABLE_TIME_FORMAT_12;tcp_rtc=NULL;
#ifdef TIMECARD_APP_DATA
    tcp_data=(tcp_appdata){0};
#endif
    if(!tcp_runtime || tcp_runtime->api_version!=1 || tcp_runtime->struct_size<RISC_RUNTIME_CAPABILITIES_V1_SIZE || !tcp_runtime->acquire || !tcp_runtime->release || !tcp_runtime->request_launch){tcp_runtime=NULL;return;}
    tcp_grants[0]=(risc_runtime_capability_v1){.struct_size=sizeof(tcp_grants[0])};
    if(tcp_runtime->acquire("rtc.clock",2,0,&tcp_grants[0])) {
        tcp_grant_count=1;const twatch_rtc_api_v1 *p=tcp_grants[0].api;
        if(p && p->api_version==2 && p->struct_size>=sizeof(*p) && p->read)tcp_rtc=p;
    }
    unsigned slot=tcp_grant_count;tcp_grants[slot]=(risc_runtime_capability_v1){.struct_size=sizeof(tcp_grants[slot])};
    if(tcp_runtime->acquire("storage.key-value",1,1,&tcp_grants[slot])) {
        tcp_grant_count++;portable_time_format_load(tcp_grants[slot].api,&tcp_time_format);
    }
#ifdef TIMECARD_APP_DATA
    slot=tcp_grant_count;tcp_grants[slot]=(risc_runtime_capability_v1){.struct_size=sizeof(tcp_grants[slot])};
    if(tcp_runtime->acquire(RISC_APP_DATA_CAPABILITY,RISC_APP_DATA_API_V1,TIMECARD_APP_DATA_INSTANCE,&tcp_grants[slot])) {
        tcp_grant_count++;(void)tcp_appdata_bind(&tcp_data,tcp_grants[slot].api);
    }
#endif
}
static void tcp_draw(void) {
    if(tcp_editor)tcp_editor_draw();
    else if(!tcp_files) {
        portable_nova_begin();portable_nova_text(0,20,49,200,"TIME CARD",NOVA_CYAN);
        portable_nova_wrap(1,20,88,200,22,4,"Writable history storage is unavailable.",NOVA_TEXT);
        portable_nova_wrap(3,20,180,200,12,2,"No history is created or changed.",NOVA_CAP);
        portable_nova_button(82,211,76,29,"BACK",false);app->present(false);
    } else {
        tcp_clock_valid=tcp_read_datetime(&tcp_snapshot);
        if(!tcp_clock_valid) {
            portable_nova_begin();portable_nova_text(0,20,49,200,"TIME CARD",NOVA_CYAN);
            portable_nova_wrap(1,20,88,200,22,3,"Clock unavailable. Retry when the clock is ready.",NOVA_TEXT);
            portable_nova_button(12,211,76,29,"BACK",false);portable_nova_button(96,211,132,29,"Retry",false);app->present(false);
        } else render();
    }
    tcp_dirty=false;
}
__attribute__((visibility("default"))) void app_main(void) {
    app=t5_app_get_api(T5_APP_ABI_VERSION);
    if(!app || app->abi_version!=T5_APP_ABI_VERSION || app->struct_size<offsetof(t5_app_api_v1,touch_contact)+sizeof(app->touch_contact) || !app->poll || !app->present || !app->set_back_exits_app || !app->touch_contact)return;
    tcp_home=tcp_editor=tcp_external_exit=false;tcp_dirty=true;tcp_scroll=0;tcp_draw_screen=UINT32_MAX;tcp_reset_gesture();
    screen_id=SCREEN_WEEK_LIST;week_offset=selected=editing_ymd=day_count=0;store_ready=false;status_text[0]=0;
    tcp_dependencies_open();tcp_files=timecard_portable_file_storage();
    if(tcp_files && (tcp_files->api_version!=T5_STORAGE_API_VERSION || tcp_files->struct_size<offsetof(t5_storage_api_v1,write_file_atomic)+sizeof(tcp_files->write_file_atomic) || !tcp_files->exists || !tcp_files->read_file || !tcp_files->write_file_atomic))tcp_files=NULL;
    storage=&tcp_store;system_api=&tcp_clock;system_ui=&tcp_system_ui;fwui=&tcp_ui;
    app->set_back_exits_app(false);if(tcp_files)tcp_reload();
    /* Returning a retained invocation is safe only because the paired Runtime
     * checks storage safety BEFORE app_module_fini, revocation or ELF unload.
     * Do not touch display, launch, or release any grant after this status. */
    if(tcp_storage_retained())return;
    tcp_draw();
    t5_app_input_t input;
    while(!tcp_home && !tcp_external_exit && app->poll(&input,20)) {
        if(!tcp_files) {
            if(input.exit_requested)tcp_external_exit=true;
            else if((input.buttons&T5_APP_BUTTON_BACK) || (input.tapped && portable_nova_hit(input.touch_x,input.touch_y,82,211,76,29)))tcp_home=true;
        } else tcp_input(&input);
        if(tcp_storage_retained())return;
        if(tcp_dirty && !tcp_home && !tcp_external_exit)tcp_draw();
    }
    if(tcp_storage_retained())return;
    if(tcp_home && tcp_runtime && !tcp_runtime->request_launch(TIMECARD_RETURN_APP) && tcp_runtime->diagnostic)tcp_runtime->diagnostic("TIMECARD error=return-request");
    tcp_editor=false;memset(tcp_entry,0,sizeof(tcp_entry));app->set_back_exits_app(true);tcp_dependencies_close();
}
