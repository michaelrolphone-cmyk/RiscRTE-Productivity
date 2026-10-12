/* Actual Timecard controller and PortableTextInputClient. Only provider I/O is fake. */
#include "RiscRuntimeV1.h"
static risc_runtime_api_v1 shared_runtime;
static bool runtime_lost;
#define TIMECARD_TEST_RUNTIME (runtime_lost?NULL:&shared_runtime)
#include "timecard_portable_test.c"

static bool modal_retained, modal_suspended, unavailable, fail_release, fail_resume;
static int open_result, poll_result, close_result;
static unsigned opens, closes, releases, modal_polls, pending, modal_calls;
static risc_text_entry_request_v1 request;
static risc_text_entry_state_v1 result;
static void modal_io(void) { assert(!modal_retained); ++modal_calls; }
int portable_text_adapter_suspend(void) {
    modal_io(); assert(!modal_suspended); modal_suspended=true; return RISC_TEXT_ENTRY_OK;
}
int portable_text_adapter_resume(void) {
    modal_io(); assert(modal_suspended); if(fail_resume)return RISC_TEXT_ENTRY_RETAINED;
    modal_suspended=false; return RISC_TEXT_ENTRY_OK;
}
int portable_text_adapter_attention(void) { modal_io(); return 0; }
void portable_text_adapter_retain(void) { modal_retained=true; }
bool portable_text_adapter_retained(void) { return modal_retained; }
static int32_t text_open(void *context,const risc_text_entry_request_v1 *r,uint64_t *session) {
    (void)context; modal_io(); assert(modal_suspended); ++opens;
    assert(r->capacity==13 && !(r->reserved&RISC_TEXT_ENTRY_REQUEST_MASKED));
    request=*r; if(open_result)return open_result; *session=opens; return RISC_TEXT_ENTRY_OK;
}
static int32_t text_poll(void *context,uint64_t session,risc_text_entry_state_v1 *s) {
    (void)context; modal_io(); assert(session && modal_suspended); ++modal_polls;
    if(poll_result)return poll_result; *s=result; return RISC_TEXT_ENTRY_OK;
}
static int32_t text_close(void *context,uint64_t session) {
    (void)context; modal_io(); assert(session && modal_suspended); ++closes;
    if(close_result)return close_result; if(pending){--pending;return RISC_TEXT_ENTRY_AGAIN;}
    return RISC_TEXT_ENTRY_OK;
}
static const risc_text_entry_api_v1_home_reason text_api={
    {1,sizeof(text_api),NULL,text_open,text_poll,text_close},
    RISC_TEXT_ENTRY_HOME_REASON_TAG,RISC_TEXT_ENTRY_HOME_REASON_VERSION
};
static bool shared_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *grant) {
    modal_io(); if(strcmp(name,RISC_TEXT_ENTRY_CAPABILITY))return acquire(name,version,instance,grant);
    assert(version==1&&!instance); if(unavailable)return false;
    *grant=(risc_runtime_capability_v1){.struct_size=sizeof(*grant),.slot=90,.generation=1,.api=&text_api}; return true;
}
static bool shared_release(risc_runtime_capability_v1 *grant) {
    modal_io(); if(grant->api!=&text_api)return release(grant);
    ++releases; if(fail_release)return false;
    *grant=(risc_runtime_capability_v1){.struct_size=sizeof(*grant)}; return true;
}
static void initialize(void) {
    runtime_lost=false;
    modal_retained=modal_suspended=unavailable=fail_release=fail_resume=false;
    opens=closes=releases=modal_polls=pending=modal_calls=0; open_result=poll_result=close_result=0;
    shared_runtime=runtime;shared_runtime.acquire=shared_acquire;shared_runtime.release=shared_release;
    tcp_text_client=(portable_text_client){0};tcp_text_state=(risc_text_entry_state_v1){0};
    tcp_text_active=tcp_text_closing=false;tcp_runtime=&shared_runtime;
    init();tcp_runtime=&shared_runtime;
    result=(risc_text_entry_state_v1){.struct_size=sizeof(result),.revision=1,.state=RISC_TEXT_ENTRY_EDITING};
}
static void begin(unsigned field) {
    open_day(20261005);selected=(int)field;tcp_activate();
    assert(tcp_editor&&tcp_text_active&&modal_suspended);
    assert(!strcmp(request.label,punch_name((uint8_t)field)));
}
static void complete(unsigned state,const char *text) {
    result.state=state;result.revision++;result.flags=0;
    snprintf(result.text,sizeof(result.text),"%s",text?text:"");tcp_text_step();
}
static void unchanged(const char *saved,unsigned count) {
    assert(writes==count&&!strcmp(persisted,saved));
}
static void inert(void) {
    assert(tcp_retained());unsigned before=modal_calls,presented=presentations,count=writes;
    tcp_draw();tcp_text_step();tcp_dependencies_close();
    t5_app_input_t in={.buttons=T5_APP_BUTTON_CONFIRM,.tapped=true,.touch_x=130,.touch_y=220};tcp_input(&in);
    assert(modal_calls==before&&presentations==presented&&writes==count);
}
int main(int argc,char **argv) {
    assert(argc==2);const char *kind=argv[1];initialize();
    if(!strcmp(kind,"fields")) {
        const char *times[]={"8:15 AM","12:00","12:45 PM","17:30"};const int expected[]={495,720,765,1050};
        for(unsigned field=0;field<4;field++) {
            begin(field);assert(!request.text[0]);complete(RISC_TEXT_ENTRY_ACCEPTED,times[field]);
            assert(!tcp_editor&&!tcp_text_active&&!modal_suspended&&get_day(20261005).punches[field]==expected[field]);
            assert(writes==field+1&&screen_id==SCREEN_DAY&&selected==(int)field);
        }
        assert(tcp_reload());tc_day_t day=get_day(20261005);assert(worked(&day)==510);
        for(unsigned field=0;field<4;field++)assert(day.punches[field]==expected[field]);
        begin(0);assert(!strcmp(request.text,"8:15 AM"));complete(RISC_TEXT_ENTRY_ACCEPTED,"");assert(get_day(20261005).punches[0]==-1&&writes==5);
        tcp_time_format=PORTABLE_TIME_FORMAT_24;begin(3);assert(!strcmp(request.text,"17:30"));complete(RISC_TEXT_ENTRY_CANCELLED,"9:00");
    } else if(!strcmp(kind,"validation")) {
        begin(0);const char *invalid[]={"25:99","12:345","12:30 junk","123456789012"};
        for(unsigned i=0;i<4;i++) {
            unsigned prior=opens;complete(RISC_TEXT_ENTRY_ACCEPTED,invalid[i]);
            assert(tcp_editor&&tcp_text_active&&!writes&&opens==prior+1&&!strcmp(request.text,invalid[i]));
        }
        complete(RISC_TEXT_ENTRY_ACCEPTED,"23:59");assert(!tcp_editor&&writes==1&&get_day(20261005).punches[0]==1439);
    } else if(!strcmp(kind,"invalid-cookie")) {
        begin(0);tcp_cookie=0;complete(RISC_TEXT_ENTRY_ACCEPTED,"08:15");
        assert(tcp_editor&&tcp_text_active&&!writes&&strstr(tcp_editor_status,"Invalid edit"));
        complete(RISC_TEXT_ENTRY_CANCELLED,NULL);assert(!tcp_editor&&!writes);
    } else if(!strcmp(kind,"input-fence")) {
        begin(0);unsigned rendered=presentations,prior_opens=opens,prior_calls=modal_calls;
        t5_app_input_t input={.buttons=T5_APP_BUTTON_CONFIRM|T5_APP_BUTTON_BACK,.tapped=true,.touch_x=130,.touch_y=220};
        for(unsigned i=0;i<50;i++){tcp_input(&input);tcp_draw();}
        assert(tcp_text_active&&tcp_editor&&opens==prior_opens&&modal_calls==prior_calls&&presentations==rendered&&!writes);
        complete(RISC_TEXT_ENTRY_CANCELLED,NULL);assert(!tcp_editor&&!writes);
    } else if(!strcmp(kind,"cancel")) {
        assert(tcp_mutate(20261005,0,480));char saved[JSON_CAPACITY];strcpy(saved,persisted);
        for(unsigned field=0;field<4;field++) {
            begin(field);complete(RISC_TEXT_ENTRY_CANCELLED,"17:59");unchanged(saved,1);
            assert(!tcp_editor&&!tcp_text_active&&!modal_suspended&&screen_id==SCREEN_DAY);
        }
    } else if(!strcmp(kind,"reload")) {
        assert(tcp_mutate(20261005,0,480));begin(1);
        strcpy(persisted,"{\"days\":[{\"d\":20261005,\"in\":500,\"ls\":-1,\"le\":-1,\"out\":-1},{\"d\":20261006,\"in\":600,\"ls\":-1,\"le\":-1,\"out\":-1}]}");
        complete(RISC_TEXT_ENTRY_ACCEPTED,"12:00");assert(!tcp_editor&&get_day(20261005).punches[0]==500&&get_day(20261005).punches[1]==720&&get_day(20261006).punches[0]==600);
    } else if(!strcmp(kind,"storage-retry")) {
        assert(tcp_mutate(20261005,0,480));begin(0);write_ok=false;
        complete(RISC_TEXT_ENTRY_ACCEPTED,"9:15");assert(tcp_editor&&tcp_text_active&&!store_ready&&get_day(20261005).punches[0]==480&&strstr(tcp_editor_status,"unconfirmed"));
        write_ok=true;complete(RISC_TEXT_ENTRY_ACCEPTED,"9:15");assert(!tcp_editor&&get_day(20261005).punches[0]==555&&writes==3);
        begin(3);read_ok=false;complete(RISC_TEXT_ENTRY_ACCEPTED,"17:00");assert(tcp_editor&&tcp_text_active&&writes==3);
        read_ok=true;complete(RISC_TEXT_ENTRY_ACCEPTED,"17:00");assert(!tcp_editor&&writes==4);
    } else if(!strcmp(kind,"pending")) {
        begin(0);pending=3;complete(RISC_TEXT_ENTRY_ACCEPTED,"8:00");assert(tcp_text_active&&tcp_text_closing&&!writes);
        unsigned old_polls=modal_polls,old_frames=presentations;
        for(unsigned i=0;i<2;i++){tcp_draw();tcp_text_step();assert(tcp_text_active&&!writes&&modal_polls==old_polls&&presentations==old_frames);}
        tcp_text_step();assert(!tcp_editor&&!tcp_text_active&&writes==1&&closes==4&&releases==1);
    } else if(!strcmp(kind,"poll-again")) {
        begin(0);poll_result=RISC_TEXT_ENTRY_AGAIN;for(unsigned i=0;i<10;i++)tcp_text_step();
        assert(tcp_text_active&&!writes&&!closes);poll_result=0;complete(RISC_TEXT_ENTRY_CANCELLED,"");assert(!tcp_editor);
    } else if(!strcmp(kind,"home")) {
        begin(0);pending=2;result.state=RISC_TEXT_ENTRY_CANCELLED;result.flags=RISC_TEXT_ENTRY_HOME_CANCEL;strcpy(result.text,"10:00");tcp_text_step();
        assert(tcp_text_active&&!tcp_home&&!writes);tcp_text_step();tcp_text_step();
        assert(!tcp_text_active&&!tcp_editor&&!writes&&tcp_external_exit&&!modal_suspended&&launches==1);
    } else if(!strcmp(kind,"unavailable")||!strcmp(kind,"busy")) {
        unavailable=!strcmp(kind,"unavailable");open_result=unavailable?0:RISC_TEXT_ENTRY_BUSY;
        open_day(20261005);selected=0;tcp_activate();assert(!tcp_text_active&&!modal_suspended&&!writes);
        assert(!tcp_text_client.acquired);tcp_editor_cancel();
    } else if(!strcmp(kind,"malformed")) {
        begin(0);memset(result.text,'1',13);result.text[13]=0;result.state=RISC_TEXT_ENTRY_ACCEPTED;tcp_text_step();assert(!writes);inert();
    } else if(!strcmp(kind,"poll-failure")) {
        begin(0);poll_result=RISC_TEXT_ENTRY_UNAVAILABLE;tcp_text_step();assert(!writes);inert();
    } else if(!strcmp(kind,"close-retained")) {
        begin(0);close_result=RISC_TEXT_ENTRY_RETAINED;complete(RISC_TEXT_ENTRY_ACCEPTED,"8:00");assert(!writes);inert();
    } else if(!strcmp(kind,"release-failure")) {
        begin(0);fail_release=true;complete(RISC_TEXT_ENTRY_ACCEPTED,"8:00");assert(!writes);inert();
    } else if(!strcmp(kind,"resume-failure")) {
        begin(0);fail_resume=true;complete(RISC_TEXT_ENTRY_ACCEPTED,"8:00");assert(!writes);inert();
    } else if(!strcmp(kind,"storage-preservation")) {storage_tests();}
    else assert(!"Unknown shared Timecard scenario");
    printf("Timecard shared controller %s PASS (opens=%u closes=%u writes=%u retained=%u)\n",kind,opens,closes,writes,modal_retained);return 0;
}
