#include "../../lib/Lists/ListsModel.h"
#include "../../lib/Lists/ListsStore.h"
#include "../../Apps/ListsScene.h"
#include "SceneKeyboardV1.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t disk[LISTS_WIRE_SIZE];static bool present,guard=true;static uint64_t generation=1;static int write_mode;static unsigned writes;
static int stat_file(void *c,const char *n,uint32_t *size,uint64_t *rev){(void)c;assert(!strcmp(n,"lists.bin"));*size=present?sizeof(disk):0;*rev=present?generation:0;return present?0:RISC_APP_DATA_NOT_FOUND;}
static int read_file(void*c,const char*n,uint64_t revision,void*out,uint32_t cap,uint32_t*size,uint64_t*rev){(void)c;(void)n;if(revision!=generation)return RISC_APP_DATA_STALE;assert(cap==sizeof(disk));memcpy(out,disk,sizeof(disk));*size=sizeof(disk);*rev=generation;return 0;}
static int replace_file(void*c,const char*n,uint64_t revision,const void*data,uint32_t size){(void)c;(void)n;++writes;assert(revision==(present?generation:0));assert(size==sizeof(disk));if(write_mode==3){guard=false;return RISC_APP_DATA_RETAINED;}if(write_mode!=1){memcpy(disk,data,size);present=true;++generation;}return write_mode==1?RISC_APP_DATA_IO:write_mode==2?RISC_APP_DATA_COMMIT_UNKNOWN:0;}
static bool is_alive(void*c){(void)c;return guard;}
static const risc_app_data_v1 api={1,sizeof(api),NULL,stat_file,read_file,replace_file};
static uint64_t sequence;
static bool action(lists_ui *u,unsigned a,int value){lists_ui_declare(u);risc_scene_event_v1 e={.struct_size=sizeof(e),.document_revision=u->revision,.action=a,.value=value,.sequence=++sequence};for(unsigned i=0;i<u->document.node_count;i++)if(u->document.nodes[i].action==a){e.node=i+1;unsigned k=u->document.nodes[i].kind;e.kind=k==RISC_SCENE_KEYBOARD_NODE||k==RISC_COMPONENT_CHECK_ROW||k==RISC_COMPONENT_STEPPER||k==RISC_COMPONENT_SEGMENTS||k==RISC_COMPONENT_SWITCH||k==RISC_COMPONENT_TIME_PICKER||k==RISC_COMPONENT_MARKERS?RISC_SCENE_VALUE_EVENT:RISC_SCENE_ACTION_EVENT;return lists_ui_event(u,&e);}assert(!"intent not declared");return false;}
int main(void){
 lists_model m;lists_init(&m);assert(lists_valid(&m));uint32_t list,task;assert(lists_add_list(&m," work ",2,&list)==0);assert(!strcmp(m.lists[0].name,"WORK"));assert(lists_add_list(&m,"WORK",0,NULL)==LISTS_INVALID);assert(lists_add_task(&m,list,"Weekly review",lists_day(2026,10,10),&task)==0);
 assert(lists_day(2024,2,29)>=0&&lists_day(2025,2,29)==-1);for(int day=0;day<=LISTS_LAST_DAY;day++){unsigned y,mo,d;assert(lists_civil(day,&y,&mo,&d));assert(lists_day(y,mo,d)==day);}
 lists_task *t=lists_find_task(&m,task);t->repeat=2;t->minute=540;t->reminder=2;
 assert(lists_reminder_deadline(t)==(uint32_t)t->due_day*1440+530);assert(lists_pending_reminder(&m,lists_reminder_deadline(t))==task);
 int32_t day=t->due_day;assert(lists_complete(&m,task,true,day+20)==0);assert(m.task_count==2&&m.tasks[1].due_day==day+21);assert(lists_complete(&m,task,false,day+20)==0);assert(lists_complete(&m,task,true,day+20)==0&&m.task_count==2);
 uint8_t wire[LISTS_WIRE_SIZE];assert(lists_encode(&m,wire));lists_model decoded;assert(lists_decode(&decoded,wire,sizeof(wire)));assert(decoded.tasks[1].due_day==day+21);
 for(unsigned i=0;i<sizeof(wire);i++){wire[i]^=1;assert(!lists_decode(&decoded,wire,sizeof(wire)));wire[i]^=1;}assert(!lists_decode(&decoded,wire,sizeof(wire)-1));
 lists_store s={.api=&api,.alive=is_alive};lists_model saved;assert(lists_store_load(&s,&saved)==0&&!saved.task_count);assert(lists_store_save(&s,&saved,&m)==0&&saved.task_count==2&&writes==1);
 lists_model candidate=saved;assert(lists_add_task(&candidate,list,"New task",-1,NULL)==0);write_mode=1;assert(lists_store_save(&s,&saved,&candidate)==LISTS_STORE_CHANGED&&saved.task_count==2&&writes==2);
 write_mode=2;assert(lists_store_save(&s,&saved,&candidate)==0&&saved.task_count==3&&writes==3);disk[100]^=1;assert(lists_store_load(&s,&saved)==LISTS_STORE_CORRUPT&&!s.ready);assert(lists_store_save(&s,&saved,&candidate)==LISTS_STORE_IO&&writes==3);disk[100]^=1;assert(lists_store_load(&s,&saved)==0);write_mode=3;assert(lists_store_save(&s,&saved,&candidate)==LISTS_STORE_RETAINED&&!s.ready);
 lists_model full;lists_init(&full);assert(lists_add_list(&full,"FULL",0,&list)==0);for(unsigned i=0;i<LISTS_MAX_TASKS;i++)assert(lists_add_task(&full,list,"REPEAT",day,NULL)==0);full.tasks[0].repeat=1;assert(lists_complete(&full,full.tasks[0].id,true,day)==LISTS_FULL&&!full.tasks[0].done);
 lists_ui u;lists_ui_init(&u);u.today=day;action(&u,LA_NEW_LIST,0);assert(u.page==LISTS_ADD_LIST);assert(action(&u,LA_SUGGEST_LIST,0)&&u.data.list_count==1);action(&u,LA_NEW_TASK,0);assert(action(&u,LA_SUGGEST_TASK,0)&&u.data.task_count==1);assert(u.page==LISTS_TASK);assert(action(&u,LA_DUE,1));action(&u,LA_TIME,0);action(&u,LA_TIME_VALUE,480);assert(action(&u,LA_SET_TIME,0));assert(action(&u,LA_REMINDER,1));assert(action(&u,LA_REPEAT,1));
 action(&u,LA_RENAME_TASK,0);action(&u,LA_KEY,RISC_SCENE_KEY_CLEAR);action(&u,LA_KEY,'N');action(&u,LA_KEY,'E');action(&u,LA_KEY,'W');assert(action(&u,LA_KEY,RISC_SCENE_KEY_DONE));assert(!strcmp(u.data.tasks[0].name,"NEW"));
 lists_ui_clock(&u,day,480);assert(u.page==LISTS_ALERT);assert(action(&u,LA_SNOOZE,0));lists_ui_clock(&u,day,489);assert(u.page!=LISTS_ALERT);lists_ui_clock(&u,day,490);assert(u.page==LISTS_ALERT);assert(action(&u,LA_DISMISS,0));lists_ui_clock(&u,day,500);assert(u.page!=LISTS_ALERT);
 assert(action(&u,LA_DONE,1));assert(u.data.task_count==2);action(&u,LA_DELETE_TASK,0);assert(u.page==LISTS_CONFIRM);action(&u,LA_KEEP,0);assert(u.data.task_count==2);action(&u,LA_DELETE_TASK,0);assert(action(&u,LA_CONFIRM,0)&&u.data.task_count==1);
 lists_ui_declare(&u);risc_scene_event_v1 undo={sizeof(undo),RISC_SCENE_ACTION_EVENT,u.revision,0,LA_UNDO,0,++sequence};assert(lists_ui_event(&u,&undo)&&u.data.task_count==2);assert(!lists_ui_event(&u,&undo));undo.sequence=++sequence;undo.document_revision--;assert(!lists_ui_event(&u,&undo));
 puts("Lists calendar, recurrence, storage recovery and intent workflows PASS");return 0;
}
