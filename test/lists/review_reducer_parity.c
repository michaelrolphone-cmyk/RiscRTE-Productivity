/* Independent reviewer: the same reducer cases execute against the exact
 * predecessor and the shared path; serialized results must be identical. */
#include "ListsScene.h"
#include "SceneKeyboardV1.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
static uint64_t sequence;
static unsigned cases;
static bool action(lists_ui *u,unsigned a,int value){
    lists_ui_declare(u);
    risc_scene_event_v1 e={.struct_size=sizeof(e),.document_revision=u->revision,.kind=RISC_SCENE_ACTION_EVENT,.action=a,.value=value,.sequence=++sequence};
    if(a==LA_UNDO)return lists_ui_event(u,&e);
    for(unsigned i=0;i<u->document.node_count;i++)if(u->document.nodes[i].action==a){e.node=i+1;if(u->document.nodes[i].kind==RISC_SCENE_KEYBOARD_NODE)e.kind=RISC_SCENE_VALUE_EVENT;return lists_ui_event(u,&e);}
    assert(!"missing declared action");return false;
}
static void setup(lists_ui *u,unsigned edit,unsigned variant){
    lists_ui_init(u);u->today=lists_day(2026,10,12);u->minute=540;
    assert(!lists_add_list(&u->data,"WORK",1,&u->view));
    assert(!lists_add_list(&u->data,"HOME",2,NULL));
    assert(!lists_add_task(&u->data,u->view,"TASK",-1,&u->task));
    if(variant==1)while(u->data.list_count<LISTS_MAX_LISTS){char name[LISTS_NAME];snprintf(name,sizeof(name),"LIST %u",u->data.list_count);assert(!lists_add_list(&u->data,name,0,NULL));}
    if(variant==2)while(u->data.task_count<LISTS_MAX_TASKS)assert(!lists_add_task(&u->data,u->view,"TASK",-1,NULL));
    if(variant==3)u->data.next_id=UINT32_MAX;
    u->page=edit==LA_CUSTOM_LIST?LISTS_ADD_LIST:edit==LA_CUSTOM_TASK?LISTS_ADD_TASK:edit==LA_RENAME_LIST?LISTS_OPTIONS:LISTS_TASK;
    assert(!action(u,edit,0));assert(u->page==LISTS_NAME_EDIT);
}
static bool accept(lists_ui *u,const char *s){
#ifdef PORTABLE_LISTS_SHARED_TEXT_INPUT
    if(u->text_retry)assert(!action(u,LA_TEXT_RETRY,0));
    return lists_ui_text_accept(u,s);
#else
    assert(!action(u,LA_KEY,RISC_SCENE_KEY_CLEAR));
    for(unsigned i=0;s[i];i++)assert(!action(u,LA_KEY,(unsigned char)s[i]));
    return action(u,LA_KEY,RISC_SCENE_KEY_DONE);
#endif
}
static uint64_t digest(const lists_model *m){uint8_t wire[LISTS_WIRE_SIZE];assert(lists_encode(m,wire));uint64_t h=UINT64_C(14695981039346656037);for(unsigned i=0;i<sizeof(wire);i++)h=(h^wire[i])*UINT64_C(1099511628211);return h;}
static void snapshot(const lists_ui *u,bool changed){
    printf("%u %d %u %u %u %d %016" PRIx64 " %s\n",++cases,changed,u->page,u->view,u->task,u->undo_valid,digest(&u->data),u->notice);
}
int main(void){
    static const unsigned edits[]={LA_CUSTOM_LIST,LA_CUSTOM_TASK,LA_RENAME_LIST,LA_RENAME_TASK};
    static const char *names[]={""," ","   ","WORK","home"," work "," new name ","abcdefghijklmnopqrstuvwx","012345678901234567890123","[{}]!?_~|\\`'\"+-=()"};
    for(unsigned e=0;e<4;e++)for(unsigned v=0;v<4;v++)for(unsigned n=0;n<sizeof(names)/sizeof(names[0]);n++){
        lists_ui u;setup(&u,edits[e],v);lists_model before=u.data;bool changed=accept(&u,names[n]);snapshot(&u,changed);
        if(!changed){assert(!memcmp(&before,&u.data,sizeof(before)));assert(u.page==LISTS_NAME_EDIT);bool retried=accept(&u,"RETRY NAME");snapshot(&u,retried);if(retried){uint32_t next=u.data.next_id;assert(action(&u,LA_UNDO,0));before.next_id=next;assert(!memcmp(&before,&u.data,sizeof(before)));snapshot(&u,true);}}
        else{uint32_t next=u.data.next_id;assert(action(&u,LA_UNDO,0));before.next_id=next;assert(!memcmp(&before,&u.data,sizeof(before)));snapshot(&u,true);}
    }
    for(unsigned e=0;e<4;e++)for(unsigned c=32;c<=126;c++){
        lists_ui u;setup(&u,edits[e],0);char s[2]={(char)c,0};snapshot(&u,accept(&u,s));
    }
    for(unsigned e=0;e<4;e++){
        lists_ui u;setup(&u,edits[e],0);lists_model before=u.data;
#ifdef PORTABLE_LISTS_SHARED_TEXT_INPUT
        lists_ui_text_cancel(&u);assert(!u.draft[0]&&!u.text_retry);
#else
        assert(!action(&u,LA_KEY,RISC_SCENE_KEY_CANCEL));
#endif
        assert(u.page==u.parent&&!memcmp(&before,&u.data,sizeof(before)));snapshot(&u,false);
    }
#ifdef PORTABLE_LISTS_SHARED_TEXT_INPUT
    for(unsigned e=0;e<4;e++){
        lists_ui u;setup(&u,edits[e],0);lists_ui before=u;
        static const char *invalid[]={NULL,"0123456789012345678901234","A\nB","A\tB","\177","\200"};
        for(unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++){assert(!lists_ui_text_accept(&u,invalid[i]));assert(!memcmp(&u,&before,sizeof(u)));}
        u.readonly=true;before=u;assert(!lists_ui_text_accept(&u,"READONLY"));assert(!memcmp(&u,&before,sizeof(u)));
        u.readonly=false;u.page=LISTS_HOME;before=u;assert(!lists_ui_text_accept(&u,"WRONG ROUTE"));assert(!memcmp(&u,&before,sizeof(u)));
    }
#endif
    return 0;
}
