/* Actual Timecard + production System adapter + production text host.
 * Runtime, scene presentation and HID providers are deterministic fixtures. */
#include "RiscRuntimeV1.h"
static risc_runtime_api_v1 shared_runtime;
#define TIMECARD_TEST_RUNTIME (&shared_runtime)
#pragma push_macro("main")
#undef main
#define main timecard_native_preservation_main
#include "timecard_native_time_test.c"
#undef main
#pragma pop_macro("main")
#include "RiscSceneV1.h"
#include "RiscProviderV2.h"
#include "RiscUsbHidV1.h"
#include "SceneKeyboardV1.h"
#ifdef PORTABLE_TEXT_INPUT_POLICY
#include "timecard_policy_scene_fixture.h"
#else
#define timecard_policy_scene_api(s) (s)
#endif

static const risc_driver_v2 *text_driver;
static const risc_text_entry_api_v1 *text_api;
static risc_scene_document_v1 scene_document;
static risc_scene_event_v1 scene_events[64];
static unsigned scene_count,scene_at,scene_opens,scene_closes,scene_updates,scene_nexts;
static unsigned text_grants,host_calls,usb_unsubs,usb_count,usb_at;
static uint64_t scene_serial,scene_session,usb_sequence;
static risc_usb_keyboard_event_v1 usb_events[64];
static bool scene_active,scene_busy,scene_fail_close,scene_fail_poll,usb_active,usb_connected,usb_held;
static bool text_unavailable,text_release_fail,script_enabled,script_home;
static unsigned script_nav,script_text,script_iterations;
static void host_io(void){fx_io();++host_calls;}
static int32_t scene_open(void *c,const risc_scene_document_v1 *d,const risc_scene_navigation_v1 *p,uint64_t *out) {
    (void)c;(void)p;host_io();assert(!scene_active&&!subs&&!frames);
    assert(d->node_count==1&&d->nodes[0].target==12&&!(d->nodes[0].flags&~RISC_SCENE_DISABLED));
    scene_active=true;scene_document=*d;scene_session=++scene_serial;*out=scene_session;++scene_opens;
    scene_count=scene_at=0;return 0;
}
static int32_t scene_update(void *c,uint64_t session,const risc_scene_document_v1 *d) {
    (void)c;host_io();assert(scene_active&&session==scene_session&&d->revision>scene_document.revision);
    scene_document=*d;++scene_updates;return 0;
}
static int32_t scene_next(void *c,uint64_t session,risc_scene_event_v1 *e) {
    (void)c;host_io();assert(scene_active&&session==scene_session);++scene_nexts;
    if(scene_fail_poll)return RISC_SCENE_RETAINED;
    if(script_enabled) {
        static const char draft[]="8:15";assert(++script_iterations<1000);
        unsigned key=script_text<sizeof(draft)-1?(unsigned char)draft[script_text]:RISC_SCENE_KEY_DONE;
        *e=(risc_scene_event_v1){.struct_size=sizeof(*e),.kind=script_home&&script_text==sizeof(draft)-1?RISC_SCENE_SUSPEND_EVENT:RISC_SCENE_VALUE_EVENT,.document_revision=scene_document.revision,.node=1,.action=1,.value=(int32_t)key,.sequence=script_text+1};++script_text;return 0;
    }
    if(scene_at==scene_count)return RISC_SCENE_IDLE;
    *e=scene_events[scene_at++];return 0;
}
static int32_t scene_snapshot(void *c,uint64_t session,risc_scene_navigation_v1 *p,uint32_t *flags) {
    (void)c;host_io();assert(scene_active&&session==scene_session);*p=(risc_scene_navigation_v1){.struct_size=sizeof(*p)};
    *flags=scene_busy?RISC_SCENE_PRESENTING:0;return 0;
}
static int32_t scene_close(void *c,uint64_t session) {
    (void)c;host_io();assert(scene_active&&session==scene_session);++scene_closes;
    if(scene_fail_close)return RISC_SCENE_RETAINED;
    if(scene_busy)return RISC_SCENE_AGAIN;scene_active=false;return 0;
}
static const risc_scene_api_v1 scene_api={1,sizeof(scene_api),NULL,scene_open,scene_update,scene_next,NULL,scene_snapshot,scene_close};
static uint64_t usb_subscribe(void *c,uint64_t device){(void)c;host_io();assert(!device&&!usb_active);usb_active=true;return 1;}
static bool usb_unsubscribe(void *c,uint64_t token){(void)c;host_io();assert(token==1&&usb_active);usb_active=false;usb_unsubs++;return true;}
static bool usb_poll(void *c,size_t reports){(void)c;host_io();assert(usb_active&&reports<=8);return true;}
static int32_t usb_next(void *c,uint64_t token,risc_usb_keyboard_event_v1 *event){(void)c;host_io();assert(token==1&&usb_active);if(usb_at==usb_count)return 0;*event=usb_events[usb_at++];return 1;}
static bool usb_snapshot(void *c,risc_usb_keyboard_state_v1 *items,size_t *count){(void)c;host_io();assert(*count);*count=usb_connected?1:0;if(usb_connected)*items=(risc_usb_keyboard_state_v1){.device=7,.connected=1,.keys={usb_held?4:0}};return true;}
static const risc_usb_keyboard_api_v1 usb_api={1,sizeof(usb_api),NULL,usb_subscribe,usb_unsubscribe,usb_poll,usb_next,usb_snapshot};
static bool script_navigation(void *c,risc_input_navigation_frame_v1 *s) {
    if(!script_enabled)return fx_nav_poll(c,s);
    fx_io();*s=(risc_input_navigation_frame_v1){0};assert(++script_iterations<1000);
    unsigned phase=script_nav++;
    if(!(phase&1))return true;
    s->buttons=s->pressed=!scene_opens?RISC_NAV_CONFIRM:RISC_NAV_BACK;return true;
}
static const risc_input_navigation_api_v1 script_nav_api={1,sizeof(script_nav_api),NULL,script_navigation,fx_nav_foreground,fx_nav_reset};
static bool shared_acquire(const char *n,uint32_t v,uint64_t instance,risc_runtime_capability_v1 *g) {
    if(!strcmp(n,RISC_TEXT_ENTRY_CAPABILITY)) {
        fx_io();assert(v==1&&!instance);if(text_unavailable)return false;
        ++live;++text_grants;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g),.slot=90,.generation=1,.api=text_api};return true;
    }
    if(!strcmp(n,"input.navigation")) {
        fx_io();assert(v==1&&!instance);++live;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g),.slot=89,.generation=1,.api=&script_nav_api};return true;
    }
    return fx_acquire(n,v,instance,g);
}
static bool shared_release(risc_runtime_capability_v1 *g) {
    if(g->api==text_api){fx_io();assert(text_grants);if(text_release_fail)return false;--text_grants;}
    return fx_release(g);
}
static void initialize(void) {
    shared_runtime=fx_runtime;shared_runtime.acquire=shared_acquire;shared_runtime.release=shared_release;
    text_driver=t5_driver_get(2);assert(text_driver);text_api=text_driver->capability;
    risc_provider_dependency_v1 deps[]={{RISC_SCENE_CAPABILITY,1,timecard_policy_scene_api(&scene_api)},{"usb.hid.keyboard",1,&usb_api}};
    assert(text_driver->start(deps,2));zone("UTC");start();
}
static void end(void) {
    assert(!tcp_text_active&&!tcp_editor&&!scene_active&&!text_grants&&!usb_active);
    assert(text_driver->quiesce());text_driver->stop();finish();
}
static void begin(unsigned field) {
    open_day(20261004);selected=(int)field;tcp_activate();
    assert(tcp_editor&&tcp_text_active&&scene_active&&!subs&&!frames);
    assert(!strcmp(scene_document.routes[0].title,punch_name((uint8_t)field)));
}
static void queue_key(unsigned key,uint32_t revision) {
    assert(scene_count<64);scene_events[scene_count++]=(risc_scene_event_v1){.struct_size=sizeof(risc_scene_event_v1),.kind=RISC_SCENE_VALUE_EVENT,.document_revision=revision,.node=1,.action=1,.value=(int32_t)key,.sequence=scene_count+1};
}
static void key(unsigned value){queue_key(value,scene_document.revision);tcp_text_step();}
static void type(const char *s){for(;*s;s++)key((unsigned char)*s);}
static void hid(unsigned usage,unsigned mods){assert(usb_count<64);usb_events[usb_count++]=(risc_usb_keyboard_event_v1){++usb_sequence,7,3,(uint8_t)usage,(uint8_t)mods,0};}
static void terminal(void) {
    assert(retained&&portable_adapter_retained()&&tcp_retained());unsigned before=calls;
    tcp_draw();tcp_text_step();tcp_dependencies_close();app_module_fini();assert(calls==before&&!data_writes&&!launches);
}
int main(int argc,char **argv) {
    assert(argc==2);const char *kind=argv[1];
    if(kind[0]>='0'&&kind[0]<='9'){shared_runtime=fx_runtime;return timecard_native_preservation_main(argc,argv);}
    initialize();
    if(!strcmp(kind,"fields")) {
        const char *text[]={"08:15","12:00","12:45","17:30"};int values[]={495,720,765,1050};
        for(unsigned i=0;i<4;i++){begin(i);type(text[i]);key(RISC_SCENE_KEY_DONE);assert(!tcp_editor&&!tcp_text_active&&subs==1&&get_day(20261004).punches[i]==values[i]);}
        assert(data_writes==4&&tcp_reload());tc_day_t d=get_day(20261004);assert(worked(&d)==510);end();
    } else if(!strcmp(kind,"retry-frame")) {
        begin(0);type("25:99");text_unavailable=true;key(RISC_SCENE_KEY_DONE);
        assert(tcp_editor&&!tcp_text_active&&!data_writes&&!strcmp(tcp_entry,"25:99"));tcp_draw();
        const char *folder=getenv("TIMECARD_RETRY_FRAMES");if(folder){
            char path[1024];snprintf(path,sizeof(path),"%s/x4-retry.ppm",folder);FILE *file=fopen(path,"wb");assert(file);fprintf(file,"P6\n480 800\n255\n");
            for(unsigned y=0;y<800;y++)for(unsigned x=0;x<480;x++){
                unsigned value=pixels[(479-x)*100+y/8]&(0x80u>>(y&7))?0:255;unsigned char rgb[3]={(unsigned char)value,(unsigned char)value,(unsigned char)value};assert(fwrite(rgb,1,3,file)==3);
            }assert(!fclose(file));
        }
        tcp_editor_cancel();end();
    } else if(!strcmp(kind,"busy-input")) {
        begin(0);scene_busy=true;unsigned before=presents;type("08:15");
        assert(!strcmp(scene_document.nodes[0].text,"08:15")&&scene_updates==5&&presents==before&&!data_writes);
        key(RISC_SCENE_KEY_DONE);assert(tcp_text_active&&tcp_text_closing&&!data_writes&&!usb_active);
        unsigned polls=scene_nexts;for(unsigned i=0;i<5;i++)tcp_text_step();assert(scene_nexts==polls&&tcp_text_active&&!data_writes);
        scene_busy=false;tcp_text_step();assert(!tcp_editor&&data_writes==1&&get_day(20261004).punches[0]==495&&usb_unsubs==1);end();
    } else if(!strcmp(kind,"stale-burst")) {
        begin(0);uint32_t revision=scene_document.revision;queue_key('8',revision);queue_key('9',revision);queue_key(RISC_SCENE_KEY_DONE,revision);
        tcp_text_step();tcp_text_step();tcp_text_step();assert(!strcmp(scene_document.nodes[0].text,"8")&&tcp_text_active&&!data_writes);
        type(":15");key(RISC_SCENE_KEY_DONE);assert(!tcp_editor&&data_writes==1&&get_day(20261004).punches[0]==495);
        unsigned before=data_writes;begin(1);key(RISC_SCENE_KEY_CANCEL);assert(data_writes==before);end();
    } else if(!strcmp(kind,"limit")) {
        begin(0);type("12345678901234567890");assert(strlen(scene_document.nodes[0].text)==12&&!strcmp(scene_document.nodes[0].text,"123456789012"));
        key(RISC_SCENE_KEY_DONE);assert(tcp_editor&&tcp_text_active&&!data_writes&&!strcmp(scene_document.nodes[0].text,"123456789012"));
        key(RISC_SCENE_KEY_CLEAR);type("23:59");key(RISC_SCENE_KEY_DONE);assert(!tcp_editor&&get_day(20261004).punches[0]==1439);end();
    } else if(!strcmp(kind,"hid")) {
        begin(0);usb_connected=true;tcp_text_step();assert(scene_document.nodes[0].flags&RISC_SCENE_DISABLED);
        hid(37,0);hid(0x33,2);hid(30,0);hid(34,0);hid(0x28,0);hid(38,0);tcp_text_step();
        assert(!tcp_editor&&data_writes==1&&get_day(20261004).punches[0]==495&&usb_unsubs==1);
        usb_count=usb_at=0;usb_held=true;begin(1);hid(37,0);tcp_text_step();assert(!scene_document.nodes[0].text[0]);
        usb_held=false;tcp_text_step();hid(38,0);tcp_text_step();assert(!strcmp(scene_document.nodes[0].text,"9"));
        hid(0x29,0);tcp_text_step();assert(!tcp_editor&&data_writes==1);end();
    } else if(!strcmp(kind,"home")) {
        begin(0);type("08:15");scene_busy=true;
        scene_events[scene_count++]=(risc_scene_event_v1){.struct_size=sizeof(risc_scene_event_v1),.kind=RISC_SCENE_SUSPEND_EVENT,.document_revision=scene_document.revision};tcp_text_step();
        assert(tcp_text_active&&!tcp_external_exit&&!launches&&!data_writes);scene_busy=false;tcp_text_step();
        assert(!tcp_text_active&&!tcp_editor&&tcp_external_exit&&!data_writes&&!launches);end();
    } else if(!strcmp(kind,"cancel")) {
        begin(0);type("08:15");key(RISC_SCENE_KEY_CANCEL);assert(!tcp_editor&&!data_writes&&screen_id==SCREEN_DAY);
        t5_app_input_t in={0};home_requested=true;assert(app->poll(&in,20));assert(!in.exit_requested&&!in.buttons&&!launches);home_requested=false;end();
    } else if(!strcmp(kind,"unavailable")) {
        text_unavailable=true;open_day(20261004);selected=0;tcp_activate();assert(!tcp_text_active&&!tcp_editor&&!data_writes&&subs==1);end();
    } else if(!strcmp(kind,"poll-retained")) {begin(0);scene_fail_poll=true;tcp_text_step();terminal();}
    else if(!strcmp(kind,"close-retained")) {begin(0);type("08:15");scene_fail_close=true;key(RISC_SCENE_KEY_DONE);terminal();}
    else if(!strcmp(kind,"release-retained")) {begin(0);type("08:15");text_release_fail=true;key(RISC_SCENE_KEY_DONE);terminal();}
    else if(!strcmp(kind,"entrypoint")||!strcmp(kind,"entrypoint-home")) {
        tcp_dependencies_close();script_enabled=true;script_home=strstr(kind,"home")!=NULL;app_main();
        assert(!tcp_text_active&&!tcp_editor&&!scene_active&&!text_grants&&!usb_active);
        fprintf(stderr,"entrypoint diagnostic: nav=%u text=%u writes=%u launches=%u screen=%u selected=%d opens=%u\n",script_nav,script_text,data_writes,launches,screen_id,selected,scene_opens);
        assert(data_writes==(script_home?0u:1u)&&launches==(script_home?0u:1u));
        assert(text_driver->quiesce());text_driver->stop();app_module_fini();assert(!live&&!subs&&!frames&&!retained);
    } else assert(!"Unknown shared host scenario");
    printf("Timecard real text-host/adapter %s PASS (opens=%u closes=%u updates=%u writes=%u)\n",kind,scene_opens,scene_closes,scene_updates,data_writes);return 0;
}
