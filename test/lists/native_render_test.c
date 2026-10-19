/* Native-sized qualification of the shipped Lists reducer and scene host.
 * Compile once as a read-only host probe and once as the transport fixture.
 * Geometry and hit maps always come from the actual presenter. */
#ifdef LISTS_NATIVE_HOST_PROBE
#include SCENE_HOST_SOURCE

bool lists_native_target(uint32_t action, int value, uint32_t ref,
                         const uint32_t *refs, int *x, int *y) {
    for (unsigned i=0;i<visible.count;i++) {
        const hit *h=&visible.items[i]; int at=node_index(&document,h->node);
        uint32_t actual=h->kind==HIT_BACK?document.cancel_action:0;
        if (h->kind==HIT_BACK&&!actual) actual=document.routes[0].back_action;
        if (at>=0) actual=h->kind==HIT_SECONDARY?document.details[at].secondary_action:document.nodes[at].action;
        if (actual!=action || (value!=-999&&h->value!=value) ||
            (ref && (at<0||refs[at]!=ref))) continue;
        int left=h->x<0?0:h->x,top=h->y<0?0:h->y;
        int right=h->x+h->w>(int)width?(int)width:h->x+h->w;
        int bottom=h->y+h->h>(int)height?(int)height:h->y+h->h;
        if (right-left<6||bottom-top<6) continue;
        for (int py=top+3;py<bottom-2;py+=3) for (int px=left+3;px<right-2;px+=3) {
            bool covered=false;
            for (unsigned j=i+1;j<visible.count;j++) {
                const hit *b=&visible.items[j];
                if (px>=b->x&&py>=b->y&&px<b->x+b->w&&py<b->y+b->h) covered=true;
            }
            if (!covered) {*x=px;*y=py;return true;}
        }
    }
    return false;
}
int lists_native_scroll(void) {return logical_model.scroll;}
unsigned lists_native_bounds(void) {
    unsigned invalid=0;
    for(unsigned i=0;i<visible.count;i++) {
        const hit *h=&visible.items[i];
        invalid+=h->x<0||h->y<0||h->x+h->w>(int)width||h->y+h->h>(int)height||h->w<=0||h->h<=0;
    }
    return invalid;
}
#else
#define main scene_baseline_main
#include "host_test.c"
#undef main
#include "../../Apps/ListsScene.h"
#include "SceneKeyboardV1.h"
#include "RiscSceneCheckpointV1.h"
#include "RiscSceneLifecycleV1.h"
#include <limits.h>

bool lists_native_target(uint32_t,int,uint32_t,const uint32_t*,int*,int*);
int lists_native_scroll(void);
unsigned lists_native_bounds(void);
static lists_ui ui;
static const risc_scene_components_api_v1 *components;
static uint32_t work,first_task,second_task;
static const char *folder;
static unsigned actions;

static void update_ui(void) {
    lists_ui_declare(&ui);
    assert(components->update(NULL,session,&ui.document)==RISC_SCENE_OK);
}
static void fixture(void) {
    lists_ui_init(&ui);ui.today=lists_day(2026,10,10);ui.minute=660;
    uint32_t groceries,home,id;
    assert(lists_add_list(&ui.data,"GROCERIES",0,&groceries)==LISTS_OK);
    assert(lists_add_list(&ui.data,"WORK",1,&work)==LISTS_OK);
    assert(lists_add_list(&ui.data,"HOME",2,&home)==LISTS_OK);
    assert(lists_add_task(&ui.data,work,"SEND WEEKLY REPORT",ui.today,&first_task)==LISTS_OK);
    assert(lists_add_task(&ui.data,work,"REVIEW PR #42",ui.today,&second_task)==LISTS_OK);
    assert(lists_add_task(&ui.data,work,"BOOK ROOM FOR DEMO",ui.today+1,&id)==LISTS_OK);
    ui.data.tasks[0].minute=900;ui.data.tasks[0].priority=2;ui.data.tasks[0].repeat=2;
    ui.data.tasks[1].minute=600;
    assert(lists_add_task(&ui.data,groceries,"MILK",ui.today,&id)==LISTS_OK);
    assert(lists_add_task(&ui.data,groceries,"EGGS",-1,&id)==LISTS_OK);
    ui.data.tasks[4].done=1;
    assert(lists_add_task(&ui.data,home,"WATER THE PLANTS",ui.today,&id)==LISTS_OK);
    ui.view=work;ui.task=first_task;
}
static void capture(const char *name) {
    settle();check_guards();
    char path[1024];assert(snprintf(path,sizeof(path),"%s/%s.ppm",folder,name)<(int)sizeof(path));
    FILE *file=fopen(path,"wb");assert(file);fprintf(file,"P6\n%u %u\n255\n",w,h);
    for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++) {
        unsigned at=16+(y*w+x)*2;uint16_t c=pixels[at]|(uint16_t)pixels[at+1]<<8;
        uint8_t rgb[3]={(uint8_t)((c>>11)*255/31),(uint8_t)(((c>>5)&63)*255/63),(uint8_t)((c&31)*255/31)};
        assert(fwrite(rgb,1,3,file)==3);
    }
    assert(!fclose(file));
}
static void page_to(unsigned page) {ui.page=page;ui.notice[0]=0;update_ui();settle();}
static void drag(bool down) {
    unsigned x=2,from=down?h/4:3*h/4,to=down?3*h/4:h/4;
    event_at=0;event_count=3;
    events[0]=(risc_touch_event_v1){++seq,ms,1,0,(uint16_t)x,(uint16_t)from};
    events[1]=(risc_touch_event_v1){++seq,ms+40,2,0,(uint16_t)x,(uint16_t)to};
    events[2]=(risc_touch_event_v1){++seq,ms+80,3,0,(uint16_t)x,(uint16_t)to};
    risc_scene_event_v1 e;assert(tick(&e)==RISC_SCENE_IDLE);
}
static void target(unsigned action,int value,uint32_t ref,int *x,int *y) {
    if(lists_native_target(action,value,ref,ui.refs,x,y))return;
    for(unsigned i=0;i<20;i++) {
        int previous=lists_native_scroll();drag(false);
        if(lists_native_target(action,value,ref,ui.refs,x,y))return;
        if(previous==lists_native_scroll())break;
    }
    for(unsigned i=0;i<20;i++) {
        int previous=lists_native_scroll();drag(true);
        if(lists_native_target(action,value,ref,ui.refs,x,y))return;
        if(previous==lists_native_scroll())break;
    }
    fprintf(stderr,"Missing target action=%u value=%d ref=%u page=%u scroll=%d\n",action,value,ref,ui.page,lists_native_scroll());
    assert(!"actual visible control missing");
}
static risc_scene_event_v1 tap_action(unsigned action,int value,uint32_t ref) {
    int x,y;target(action,value,ref,&x,&y);touch_tap((unsigned)x,(unsigned)y);
    risc_scene_event_v1 e;int result=tick(&e);
    if(result!=RISC_SCENE_OK||e.action!=action||(value!=-999&&e.value!=value))
        fprintf(stderr,"Tap (%d,%d) action %u value %d: result %d action %u value %d\n",x,y,action,value,result,e.action,e.value);
    assert(result==RISC_SCENE_OK&&e.action==action&&(value==-999||e.value==value));
    assert(e.document_revision==ui.document.revision&&e.sequence>ui.last_event);
    actions++;return e;
}
static bool apply_action(unsigned action,int value,uint32_t ref) {
    risc_scene_event_v1 e=tap_action(action,value,ref);bool changed=lists_ui_event(&ui,&e);update_ui();return changed;
}
static void keyboard(void) {
    page_to(LISTS_TASK);assert(!apply_action(LA_RENAME_TASK,-999,0));
    assert(ui.page==LISTS_NAME_EDIT&&!strcmp(ui.draft,"SEND WEEKLY REPORT"));
    capture("keyboard");
    for(unsigned i=0;i<18;i++)assert(!apply_action(LA_KEY,RISC_SCENE_KEY_BACKSPACE,0));
    assert(!ui.draft[0]);
    assert(!apply_action(LA_KEY,'A',0));assert(!apply_action(LA_KEY,'B',0));
    assert(!apply_action(LA_KEY,RISC_SCENE_KEY_LAYER,0));assert(ui.key_layer==1);
    assert(!apply_action(LA_KEY,'1',0));
    assert(!apply_action(LA_KEY,RISC_SCENE_KEY_LAYER,0));assert(ui.key_layer==2);
    assert(!apply_action(LA_KEY,'c',0));
    assert(!apply_action(LA_KEY,RISC_SCENE_KEY_LAYER,0));assert(ui.key_layer==3);
    assert(!apply_action(LA_KEY,'^',0));
    assert(!apply_action(LA_KEY,RISC_SCENE_KEY_LAYER,0));assert(ui.key_layer==0);
    assert(!strcmp(ui.draft,"AB1c^"));
    assert(!apply_action(LA_KEY,RISC_SCENE_KEY_SPACE,0));
    for(unsigned i=0;i<30;i++)assert(!apply_action(LA_KEY,'Z',0));
    assert(strlen(ui.draft)==LISTS_NAME-1);
    assert(apply_action(LA_KEY,RISC_SCENE_KEY_DONE,0));
    assert(ui.page==LISTS_TASK&&strlen(lists_find_task(&ui.data,first_task)->name)==LISTS_NAME-1);
    ui.notice[0]=0;update_ui();
    assert(!apply_action(LA_RENAME_TASK,-999,0));
    assert(!apply_action(LA_KEY,RISC_SCENE_KEY_CANCEL,0));assert(ui.page==LISTS_TASK);
    assert(!apply_action(LA_RENAME_TASK,-999,0));
    nav_pressed=RISC_NAV_BACK;risc_scene_event_v1 e;assert(tick(&e)==RISC_SCENE_OK&&e.action==LA_KEY&&e.value==RISC_SCENE_KEY_CANCEL);
    assert(!lists_ui_event(&ui,&e));update_ui();assert(ui.page==LISTS_TASK);
}
static void pending_keyboard(void) {
    page_to(LISTS_TASK);assert(!apply_action(LA_RENAME_TASK,-999,0));settle();
    strcpy(ui.draft,"");update_ui();allow_complete=false;begin_frame();unsigned before=submits;
    assert(!apply_action(LA_KEY,'A',0));assert(!apply_action(LA_KEY,'B',0));assert(!apply_action(LA_KEY,'C',0));
    assert(!strcmp(ui.draft,"ABC")&&submits==before&&presenting);
    assert(!apply_action(LA_KEY,RISC_SCENE_KEY_LAYER,0));assert(!apply_action(LA_KEY,'1',0));
    assert(!strcmp(ui.draft,"ABC1")&&submits==before);
    assert(apply_action(LA_KEY,RISC_SCENE_KEY_DONE,0));assert(ui.page==LISTS_TASK);
    allow_complete=true;capture("pending-keyboard-result");
    assert(!strcmp(lists_find_task(&ui.data,first_task)->name,"ABC1"));
}
static void controls(void) {
    capture("home");page_to(LISTS_VIEW);capture("list");
    unsigned count=ui.data.task_count;
    assert(apply_action(LA_DONE,1,second_task));assert(lists_find_task(&ui.data,second_task)->done);
    assert(ui.data.task_count==count&&ui.page==LISTS_VIEW);
    ui.notice[0]=0;update_ui();settle();allow_complete=false;update_ui();begin_frame();unsigned before=submits;
    assert(!apply_action(LA_FILTER,1,0));assert(ui.filter==1&&ui.page==LISTS_VIEW&&submits==before);
    assert(apply_action(LA_DONE,0,second_task));assert(!lists_find_task(&ui.data,second_task)->done);
    ui.notice[0]=0;update_ui();assert(!apply_action(LA_FILTER,0,0));
    assert(!apply_action(LA_TASK,-999,first_task));assert(ui.page==LISTS_TASK&&ui.task==first_task&&submits==before);
    allow_complete=true;capture("task");
    assert(apply_action(LA_PRIORITY,1,0));assert(lists_find_task(&ui.data,first_task)->priority==1);
    ui.notice[0]=0;update_ui();assert(apply_action(LA_REPEAT,1,0));assert(lists_find_task(&ui.data,first_task)->repeat==1);
    ui.notice[0]=0;update_ui();assert(!apply_action(LA_TIME,-999,0));assert(ui.page==LISTS_TIME);
    capture("time");unsigned value=ui.time_value;
    assert(!apply_action(LA_TIME_VALUE,-999,0));assert(ui.time_value!=value&&ui.time_value<1440);
    assert(apply_action(LA_SET_TIME,-999,0));assert(ui.page==LISTS_TASK);
    page_to(LISTS_OPTIONS);capture("options");assert(apply_action(LA_SHOW_DONE,1,0));assert(lists_find_list(&ui.data,work)->show_completed);
    ui.notice[0]=0;update_ui();assert(!apply_action(LA_RENAME_LIST,-999,0));assert(ui.page==LISTS_NAME_EDIT);
    assert(!apply_action(LA_KEY,RISC_SCENE_KEY_CANCEL,0));
    page_to(LISTS_ADD_LIST);capture("new-list");assert(!apply_action(LA_MARKER,5,0));assert(ui.marker==5);
    assert(apply_action(LA_SUGGEST_LIST,-999,0));assert(ui.page==LISTS_VIEW&&ui.data.list_count==4);
    ui.view=work;page_to(LISTS_ADD_TASK);capture("new-task");assert(apply_action(LA_SUGGEST_TASK,-999,0));assert(ui.page==LISTS_TASK&&ui.data.task_count==count+1);
    for(unsigned i=0;i<ui.data.task_count;i++)ui.data.tasks[i].done=1;
    lists_find_list(&ui.data,work)->show_completed=0;ui.filter=0;page_to(LISTS_VIEW);capture("completed");
}
static void modal(void) {
    ui.confirm_kind=LA_DELETE_TASK;page_to(LISTS_CONFIRM);capture("confirm");
    assert(!lists_native_bounds()&&"modal targets must fit the display");
    risc_scene_navigation_v1 n={.struct_size=sizeof(n)};uint32_t flags;
    assert(!api->snapshot(NULL,session,&n,&flags));assert(ui.document.nodes[n.focus[0]-1].action==LA_KEEP);
    touch_tap(w/2,h/2);risc_scene_event_v1 e;assert(tick(&e)==RISC_SCENE_IDLE);
    assert(!apply_action(LA_KEEP,-999,0));assert(ui.page==LISTS_TASK);
    page_to(LISTS_CONFIRM);touch_tap(1,1);assert(tick(&e)==RISC_SCENE_OK&&e.action==LA_KEEP);
    assert(!lists_ui_event(&ui,&e));update_ui();
    ui.alert=first_task;ui.parent=LISTS_TASK;page_to(LISTS_ALERT);capture("alert");
    assert(!lists_native_bounds()&&"alert targets must fit the display");
    assert(apply_action(LA_SNOOZE,-999,0));assert(lists_find_task(&ui.data,first_task)->snooze==(uint32_t)ui.today*1440+ui.minute+10);
}
static void scrolling(void) {
    for(unsigned i=0;i<20;i++){char title[LISTS_NAME];uint32_t id;snprintf(title,sizeof(title),"SCROLL TASK %u",i);assert(lists_add_task(&ui.data,work,title,-1,&id)==LISTS_OK);}
    page_to(LISTS_VIEW);int initial=lists_native_scroll();drag(false);assert(lists_native_scroll()>initial);
    uint32_t last=ui.data.tasks[ui.data.task_count-1].id;
    assert(!apply_action(LA_TASK,-999,last));assert(ui.task==last&&ui.page==LISTS_TASK);
    page_to(LISTS_VIEW);assert(!lists_native_scroll());
    risc_scene_event_v1 e;for(unsigned i=0;i<ui.document.node_count+2;i++){nav_pressed=RISC_NAV_DOWN;assert(tick(&e)==RISC_SCENE_IDLE);}
    capture("focus-scroll");
    assert(lists_native_scroll()>=0);
    ui.today=-1;page_to(LISTS_TASK);int x,y;
    assert(!lists_native_target(LA_DUE,-999,0,ui.refs,&x,&y));
    assert(!lists_native_target(LA_REMINDER,-999,0,ui.refs,&x,&y));
}
static void checkpoint(void) {
    page_to(LISTS_TASK);assert(!apply_action(LA_RENAME_TASK,-999,0));assert(!apply_action(LA_KEY,'X',0));settle();
    char draft[LISTS_NAME];strcpy(draft,ui.draft);
    const risc_scene_checkpoint_api_v1 *checkpoint=risc_scene_checkpoint_get_v1(api);
    const risc_scene_lifecycle_api_v1 *lifecycle=risc_scene_lifecycle_get_v1(api);assert(checkpoint&&lifecycle);
    assert(!lifecycle->configure(NULL,session,0));settle();
    assert(!checkpoint->pause(NULL,session));assert(!subscribed&&!presenting&&!held);
    assert(components->update(NULL,session,&ui.document)==RISC_SCENE_BUSY);
    assert(!checkpoint->resume(NULL,session,RISC_SCENE_RESUME_REDRAW));capture("resumed-keyboard");assert(!strcmp(draft,ui.draft));
    assert(!apply_action(LA_KEY,'Y',0));assert(strlen(ui.draft)==strlen(draft)+1);
    nav_pressed=RISC_NAV_HOME;risc_scene_event_v1 e;assert(tick(&e)==RISC_SCENE_OK&&e.kind==RISC_SCENE_SUSPEND_EVENT);
    risc_scene_navigation_v1 saved={.struct_size=sizeof(saved)};uint32_t flags;assert(!api->snapshot(NULL,session,&saved,&flags));
    uint64_t old=session;allow_complete=true;while(api->close(NULL,session)==RISC_SCENE_AGAIN)ms+=10;
    assert(!held&&!presenting&&!subscribed);assert(!components->open(NULL,&ui.document,&saved,&session));assert(session!=old);
    assert(components->update(NULL,old,&ui.document)==RISC_SCENE_STALE);capture("reopened-keyboard");
    ui.last_event=0;assert(!apply_action(LA_KEY,RISC_SCENE_KEY_CANCEL,0));assert(ui.page==LISTS_TASK);
}
static void retained_case(const char *callback) {
    page_to(LISTS_TASK);
    if(!strcmp(callback,"status")){allow_complete=false;update_ui();begin_frame();}
    if(!strcmp(callback,"acquire")||!strcmp(callback,"submit"))update_ui();
    loss_callback=callback;
    if(!strcmp(callback,"before"))native_alive=false;
    risc_scene_event_v1 e;int result=RISC_SCENE_IDLE;
    if(!strcmp(callback,"unsubscribe"))result=api->close(NULL,session);
    else for(unsigned i=0;result==RISC_SCENE_IDLE&&i<1000;i++)result=tick(&e);
    assert(result==RISC_SCENE_RETAINED&&!native_alive);
    unsigned before=calls;native_alive=true;
    assert(components->update(NULL,session,&ui.document)==RISC_SCENE_RETAINED);
    assert(tick(&e)==RISC_SCENE_RETAINED&&api->close(NULL,session)==RISC_SCENE_RETAINED);
    assert(!driver->quiesce());driver->stop();assert(calls==before);
}
int main(int argc,char **argv) {
    assert(argc==4&&sscanf(argv[1],"%ux%u",&w,&h)==2);assert(w>=160&&h>=160&&w*h<=800*480);
    folder=argv[2];const char *mode=argv[3];
    startup();assert(!api->close(NULL,session));components=risc_scene_components_get_v1(api);assert(components);
    fixture();lists_ui_declare(&ui);expected_intent=RISC_DISPLAY_PRESENT_LOW_LATENCY;
    assert(!components->open(NULL,&ui.document,NULL,&session));
    if(!strncmp(mode,"retained-",9)) {retained_case(mode+9);}
    else {
        if(!strcmp(mode,"controls"))controls();
        else if(!strcmp(mode,"keyboard"))keyboard();
        else if(!strcmp(mode,"pending-keyboard"))pending_keyboard();
        else if(!strcmp(mode,"modal"))modal();
        else if(!strcmp(mode,"scroll"))scrolling();
        else if(!strcmp(mode,"checkpoint"))checkpoint();
        else assert(!"unknown case");
        finish();
    }
    printf("Lists native %s %s PASS (%u actions, %u frames, %u provider calls)\n",argv[1],mode,actions,submits,calls);return 0;
}
#endif
