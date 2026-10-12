/* Shared-only reducer guards. Integration tests separately drive the real
 * services; this unit suite injects malformed/stale app events deliberately. */
#include "../../Apps/ListsScene.h"
#include "SceneKeyboardV1.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint64_t sequence;
static bool send(lists_ui*u,unsigned action,int value){
 lists_ui_declare(u);risc_scene_event_v1 e={.struct_size=sizeof(e),.document_revision=u->document.revision,.sequence=++sequence,.kind=RISC_SCENE_ACTION_EVENT,.action=action,.value=value};
 for(unsigned i=0;i<u->document.node_count;i++)if(u->document.nodes[i].action==action){e.node=i+1;return lists_ui_event(u,&e);}
 return lists_ui_event(u,&e);
}
static void edit(lists_ui*u,unsigned reason){
 u->page=reason==LA_CUSTOM_LIST?LISTS_ADD_LIST:reason==LA_CUSTOM_TASK?LISTS_ADD_TASK:reason==LA_RENAME_LIST?LISTS_OPTIONS:LISTS_TASK;
 assert(!send(u,reason,0)&&u->page==LISTS_NAME_EDIT);lists_ui_declare(u);
 bool retry=false,cancel=false;for(unsigned i=0;i<u->document.node_count;i++){assert(u->document.nodes[i].kind!=RISC_SCENE_KEYBOARD_NODE);retry|=u->document.nodes[i].action==LA_TEXT_RETRY;cancel|=u->document.nodes[i].action==LA_BACK;}assert(retry&&cancel);
}
int main(void){
 lists_ui u;lists_ui_init(&u);uint32_t list,task;assert(!lists_add_list(&u.data,"WORK",1,&list));assert(!lists_add_list(&u.data,"HOME",2,NULL));assert(!lists_add_task(&u.data,list,"TASK",-1,&task));u.view=list;u.task=task;
 unsigned reasons[]={LA_CUSTOM_LIST,LA_CUSTOM_TASK,LA_RENAME_LIST,LA_RENAME_TASK};
 for(unsigned i=0;i<4;i++){
  lists_ui original=u;edit(&u,reasons[i]);lists_model before=u.data;
  char unterminated[LISTS_NAME];memset(unterminated,'A',sizeof(unterminated));assert(!lists_ui_text_accept(&u,unterminated));
  assert(!lists_ui_text_accept(&u,"BAD\nNAME"));assert(!lists_ui_text_accept(&u,"BAD\177NAME"));assert(!memcmp(&u.data,&before,sizeof(before)));
  risc_scene_event_v1 forged={.struct_size=sizeof(forged),.kind=RISC_SCENE_VALUE_EVENT,.document_revision=u.document.revision,.node=1,.sequence=++sequence,.action=LA_KEY,.value='Z'};assert(!lists_ui_event(&u,&forged));
  lists_ui_text_cancel(&u);assert(u.page==u.parent&&!u.draft[0]&&!memcmp(&u.data,&before,sizeof(before)));
  edit(&u,reasons[i]);assert(!lists_ui_text_accept(&u,"  "));assert(u.text_retry&&u.page==LISTS_NAME_EDIT);assert(!memcmp(&u.data,&before,sizeof(before)));
  assert(!send(&u,LA_TEXT_RETRY,0)&&!u.text_retry);u.readonly=true;assert(!lists_ui_text_accept(&u,"NEW NAME"));u.readonly=false;
  assert(lists_ui_text_accept(&u,"  new name  "));assert(u.undo_valid);assert(u.data.list_count==before.list_count+(i==0)&&u.data.task_count==before.task_count+(i==1));
  lists_ui_declare(&u);forged=(risc_scene_event_v1){.struct_size=sizeof(forged),.kind=RISC_SCENE_ACTION_EVENT,.document_revision=u.document.revision-1,.sequence=++sequence,.action=LA_UNDO};assert(!lists_ui_event(&u,&forged));
  forged.document_revision=u.document.revision;forged.sequence=++sequence;assert(lists_ui_event(&u,&forged));assert(!lists_ui_event(&u,&forged));assert(!u.undo_valid);
  u=original;
 }
 edit(&u,LA_RENAME_LIST);assert(!lists_ui_text_accept(&u,"HOME")&&u.text_retry);assert(!strcmp(u.data.lists[0].name,"WORK"));lists_ui_text_cancel(&u);
 edit(&u,LA_RENAME_TASK);assert(lists_ui_text_accept(&u,"ABCDEFGHIJKLMNOPQRSTUVWX"));assert(strlen(lists_find_task(&u.data,task)->name)==24);
 puts("Lists shared-only reducer: four operations, 24-byte names, validation, retry, cancel, stale events and undo PASS");return 0;
}
