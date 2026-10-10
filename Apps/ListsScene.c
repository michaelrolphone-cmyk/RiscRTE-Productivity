/* Application semantics only. No display API, dimensions, glyphs or hit boxes. */
#include "ListsScene.h"
#include "SceneKeyboardV1.h"
#include <stdio.h>
#include <string.h>
static const char *const names[]={"GROCERIES","WORK","HOME","ERRANDS","READING","FITNESS","TRAVEL"};
static const char *const suggestions[][4]={{"CALL MOM","PAY BILLS","STRETCH","EMAIL TEAM"},{"BREAD","FRUIT","RICE","TEA"},{"PREP MEETING","REVIEW DOC","CALL CLIENT","EMAIL TEAM"},{"LAUNDRY","VACUUM","DISHES","GARBAGE"},{"NEW BOOK","READ A CHAPTER","TAKE NOTES","RETURN BOOK"}};
static const char *const dues[]={"NONE","TODAY","TOMORROW","THIS WEEK","SAVED DATE"};
static const char *const reminders[]={"NONE","AT TIME","10 MIN BEFORE","1 H BEFORE"};
static const char *const repeats[]={"NONE","DAILY","WEEKLY"};
static void copy(char *out,size_t size,const char *s){snprintf(out,size,"%s",s?s:"");}
void lists_ui_notice(lists_ui *u,const char *s){copy(u->notice,sizeof(u->notice),s);if(++u->toast_token==0)++u->toast_token;}
void lists_ui_init(lists_ui *u){memset(u,0,sizeof(*u));lists_init(&u->data);u->page=LISTS_HOME;u->view=LISTS_VIEW_ALL;u->today=-1;}
static const char *view_name(lists_ui *u){lists_list *l=lists_find_list(&u->data,u->view);return l?l->name:u->view==LISTS_VIEW_TODAY?"TODAY":"ALL TASKS";}
static bool in_view(const lists_ui *u,const lists_task *t,uint32_t view){return view==LISTS_VIEW_ALL||(view==LISTS_VIEW_TODAY?u->today>=0&&t->due_day==u->today:t->list_id==view);}
static void counts(lists_ui *u,uint32_t view,unsigned *done,unsigned *total){*done=*total=0;for(unsigned i=0;i<u->data.task_count;i++)if(in_view(u,&u->data.tasks[i],view)){++*total;*done+=u->data.tasks[i].done;}}
static unsigned suggestion_set(lists_ui *u){lists_list *l=lists_find_list(&u->data,u->view);if(!l)return 0;return !strcmp(l->name,"GROCERIES")?1:!strcmp(l->name,"WORK")?2:!strcmp(l->name,"HOME")?3:!strcmp(l->name,"READING")?4:0;}
static risc_scene_node_v1 *node(lists_ui *u,unsigned kind,const char *label,const char *text,uint32_t action,uint32_t ref){
    risc_components_document_v1 *d=&u->document;if(d->node_count==RISC_COMPONENTS_MAX_NODES)return NULL;
    unsigned i=d->node_count++;risc_scene_node_v1 *n=&d->nodes[i];*n=(risc_scene_node_v1){.id=i+1,.route=1,.kind=kind,.action=action};
    copy(n->label,sizeof(n->label),label);copy(n->text,sizeof(n->text),text);d->details[i].marker=6;u->refs[i]=ref;return n;
}
static risc_component_detail_v1 *detail(lists_ui *u,risc_scene_node_v1 *n){return &u->document.details[n->id-1];}
static void header(lists_ui *u,const char *title,const char *sub,bool back){copy(u->document.routes[0].title,RISC_SCENE_LABEL,title);copy(u->document.subtitle,RISC_SCENE_TEXT,sub);u->document.routes[0].back_action=back?LA_BACK:0;}
static risc_scene_node_v1 *button(lists_ui *u,const char *label,unsigned action,unsigned flags){risc_scene_node_v1 *n=node(u,RISC_SCENE_ACTION,label,"",action,0);n->flags=flags;return n;}
static void bounds(risc_scene_node_v1 *n,int value,int maximum){n->value=value;n->minimum=0;n->maximum=maximum;n->step=1;}
static risc_scene_node_v1 *stepper(lists_ui *u,const char *label,const char *value,unsigned action,int selected,int maximum){risc_scene_node_v1 *n=node(u,RISC_COMPONENT_STEPPER,label,value,action,0);bounds(n,selected,maximum);return n;}
static void segments(lists_ui *u,const char *choices,unsigned action,unsigned selected,unsigned count){risc_scene_node_v1 *n=node(u,RISC_COMPONENT_SEGMENTS,"","",action,0);bounds(n,(int)selected,(int)count-1);copy(detail(u,n)->choices,sizeof(detail(u,n)->choices),choices);}
static void progress(lists_ui *u,const char *label,uint32_t view){unsigned done,total;counts(u,view,&done,&total);char sub[40];snprintf(sub,sizeof(sub),"%u OF %u DONE",done,total);risc_scene_node_v1 *n=node(u,RISC_COMPONENT_PROGRESS,label,sub,label[0]?LA_TODAY:0,0);n->value=(int)done;n->maximum=(int)total;}
static int32_t week_end(const lists_ui *u){return u->today+(7-(u->today+6)%7)%7;}
static unsigned due_index(const lists_ui *u,const lists_task *t){if(t->due_day<0)return 0;if(u->today<0)return 4;return t->due_day==u->today?1:t->due_day==u->today+1?2:t->due_day==week_end(u)?3:4;}
static void due_text(lists_ui *u,const lists_task *t,char *out,size_t size){
    if(t->due_day<0){copy(out,size,"NO DATE");return;}unsigned index=due_index(u,t);char date[24];
    if(index==1||index==2)copy(date,sizeof(date),dues[index]);else{unsigned y,m,d;if(!lists_civil(t->due_day,&y,&m,&d)){copy(out,size,"DATE UNAVAILABLE");return;}snprintf(date,sizeof(date),"%04u-%02u-%02u",y,m,d);}
    if(t->minute<0)copy(out,size,date);else snprintf(out,size,"%s %02u:%02u",date,(unsigned)t->minute/60,(unsigned)t->minute%60);
}
static bool task_before(const lists_task *a,const lists_task *b){if(a->done!=b->done)return !a->done;int da=a->due_day<0?INT32_MAX:a->due_day,db=b->due_day<0?INT32_MAX:b->due_day;if(da!=db)return da<db;if(a->minute!=b->minute)return (a->minute<0?1440:a->minute)<(b->minute<0?1440:b->minute);return a->id<b->id;}
void lists_ui_declare(lists_ui *u){
    if(u->revision==UINT32_MAX)return;
    risc_components_document_v1 *d=&u->document;memset(d,0,sizeof(*d));memset(u->refs,0,sizeof(u->refs));
    *d=(risc_components_document_v1){.api_version=1,.struct_size=sizeof(*d),.revision=++u->revision,.root=1,.route_count=1,.screen_key=u->page*65536u+(u->page==LISTS_TASK?u->task:u->page==LISTS_VIEW?u->view:0)};
    if(!d->screen_key)d->screen_key=1;
    d->routes[0].id=1;
    d->toast_token=u->toast_token;d->toast_action=u->undo_valid?LA_UNDO:0;copy(d->toast,sizeof(d->toast),u->notice);
    if(u->readonly){header(u,"LISTS","SAVED DATA UNAVAILABLE",false);node(u,RISC_COMPONENT_EMPTY,"RETRY STORAGE","Your saved lists have not been replaced.",0,0);button(u,"RETRY",LA_RETRY,RISC_SCENE_PRIMARY);return;}
    lists_task *t=lists_find_task(&u->data,u->task);lists_list *l=lists_find_list(&u->data,u->view);
    if((u->page==LISTS_TASK||u->page==LISTS_TIME)&&!t)u->page=LISTS_VIEW;
    switch(u->page){
    case LISTS_HOME:{unsigned done,total;char sub[40];counts(u,LISTS_VIEW_ALL,&done,&total);snprintf(sub,sizeof(sub),"%u OPEN / %u DONE",total-done,done);header(u,"LISTS",sub,false);progress(u,"TODAY",LISTS_VIEW_TODAY);
        risc_scene_node_v1 *n=node(u,RISC_COMPONENT_ROW,"TODAY",u->today<0?"Set the device clock":"Due today",LA_TODAY,0);detail(u,n)->symbol=RISC_SYMBOL_TODAY;
        n=node(u,RISC_COMPONENT_ROW,"ALL TASKS","Every list",LA_ALL,0);detail(u,n)->symbol=RISC_SYMBOL_LIST;
        node(u,RISC_COMPONENT_SECTION,"MY LISTS","",0,0);
        for(unsigned i=0;i<u->data.list_count;i++){lists_list *item=&u->data.lists[i];counts(u,item->id,&done,&total);snprintf(sub,sizeof(sub),"%u OF %u DONE",done,total);n=node(u,RISC_COMPONENT_ROW,item->name,sub,LA_LIST,item->id);detail(u,n)->marker=item->marker;}
        if(!u->data.list_count)node(u,RISC_COMPONENT_EMPTY,"YOUR FIRST LIST","Create a list, then add tasks.",0,0);
        n=node(u,RISC_COMPONENT_ROW,"NEW LIST","",LA_NEW_LIST,0);detail(u,n)->symbol=RISC_SYMBOL_PLUS;break;}
    case LISTS_VIEW:{header(u,view_name(u),"",true);risc_scene_node_v1 *n;
        if(l){n=node(u,RISC_COMPONENT_HEADER_ACTION,"OPTIONS","",LA_OPTIONS,0);detail(u,n)->symbol=RISC_SYMBOL_MORE;}
        n=node(u,RISC_COMPONENT_HEADER_ACTION,"ADD TASK","",LA_NEW_TASK,0);detail(u,n)->symbol=RISC_SYMBOL_PLUS;
        unsigned done,total;counts(u,u->view,&done,&total);snprintf(d->subtitle,sizeof(d->subtitle),"%u OF %u DONE",done,total);progress(u,"",u->view);segments(u,"OPEN|ALL",LA_FILTER,u->filter,2);
        unsigned order[LISTS_MAX_TASKS],count=0;
        for(unsigned i=0;i<u->data.task_count;i++)if(in_view(u,&u->data.tasks[i],u->view)&&(!u->data.tasks[i].done||u->filter||(l&&l->show_completed))){unsigned at=count++;while(at&&task_before(&u->data.tasks[i],&u->data.tasks[order[at-1]])){order[at]=order[at-1];--at;}order[at]=i;}
        for(unsigned i=0;i<count;i++){lists_task *item=&u->data.tasks[order[i]];char date[40],sub[72];due_text(u,item,date,sizeof(date));lists_list *owner=lists_find_list(&u->data,item->list_id);snprintf(sub,sizeof(sub),"%s%s%s%s%s",l?"":owner->name,l?"":" / ",date,item->repeat?" / ":"",item->repeat?repeats[item->repeat]:"");n=node(u,RISC_COMPONENT_CHECK_ROW,item->name,sub,LA_DONE,item->id);bounds(n,item->done,1);risc_component_detail_v1 *a=detail(u,n);a->secondary_action=LA_TASK;
            if(lists_overdue(item,u->today,u->minute)){copy(a->badge,sizeof(a->badge),"OVERDUE");a->tone=RISC_TONE_WARNING;}else if(item->priority==2&&!item->done){copy(a->badge,sizeof(a->badge),"HIGH");a->tone=RISC_TONE_WARNING;}}
        if(!count)node(u,RISC_COMPONENT_EMPTY,total?"ALL DONE":"NOTHING HERE YET",total?"Switch to ALL to see completed tasks.":"Tap + to add the first task.",0,0);
        n=node(u,RISC_COMPONENT_ROW,"ADD TASK","",LA_NEW_TASK,0);detail(u,n)->symbol=RISC_SYMBOL_PLUS;break;}
    case LISTS_TASK:{header(u,t->name,"TASK",true);risc_scene_node_v1 *n=node(u,RISC_COMPONENT_CHECK_ROW,"DONE","",LA_DONE,t->id);bounds(n,t->done,1);
        n=node(u,RISC_COMPONENT_ROW,"RENAME","",LA_RENAME_TASK,0);detail(u,n)->symbol=RISC_SYMBOL_EDIT;
        unsigned at=0;for(unsigned i=0;i<u->data.list_count;i++)if(u->data.lists[i].id==t->list_id)at=i;
        stepper(u,"LIST",u->data.lists[at].name,LA_TASK_LIST,(int)at,(int)u->data.list_count-1);
        unsigned due=due_index(u,t);char date[40];due_text(u,t,date,sizeof(date));n=stepper(u,"DUE",due==4?date:dues[due],LA_DUE,(int)due,due==4?4:3);if(u->today<0)n->flags|=RISC_SCENE_DISABLED;
        char time[12];if(t->minute<0)copy(time,sizeof(time),"NONE");else snprintf(time,sizeof(time),"%02u:%02u",(unsigned)t->minute/60,(unsigned)t->minute%60);
        n=node(u,RISC_COMPONENT_ROW,"TIME",time,LA_TIME,0);detail(u,n)->symbol=RISC_SYMBOL_CLOCK;if(u->today<0&&t->due_day<0)n->flags|=RISC_SCENE_DISABLED;
        node(u,RISC_COMPONENT_SECTION,"PRIORITY","",0,0);segments(u,"LOW|MED|HIGH",LA_PRIORITY,t->priority,3);
        n=stepper(u,"REMINDER",reminders[t->reminder],LA_REMINDER,t->reminder,3);if(t->due_day<0||t->minute<0)n->flags|=RISC_SCENE_DISABLED;
        stepper(u,"REPEAT",repeats[t->repeat],LA_REPEAT,t->repeat,2);
        node(u,RISC_SCENE_TEXT_NODE,"REMINDERS","Shown while Lists is open; missed reminders appear on return.",0,0);
        n=node(u,RISC_COMPONENT_ROW,"DELETE TASK","",LA_DELETE_TASK,0);detail(u,n)->symbol=RISC_SYMBOL_DELETE;detail(u,n)->tone=RISC_TONE_DANGER;break;}
    case LISTS_ADD_TASK:{header(u,"ADD TASK",view_name(u),true);unsigned set=suggestion_set(u);for(unsigned i=0;i<4;i++)button(u,suggestions[set][i],LA_SUGGEST_TASK,0),u->refs[d->node_count-1]=i;button(u,"CUSTOM...",LA_CUSTOM_TASK,RISC_SCENE_PRIMARY);break;}
    case LISTS_ADD_LIST:{header(u,"NEW LIST","PICK A SHAPE, THEN A NAME",true);risc_scene_node_v1 *n=node(u,RISC_COMPONENT_MARKERS,"SHAPE","",LA_MARKER,0);bounds(n,(int)u->marker,5);
        for(unsigned i=0;i<sizeof(names)/sizeof(names[0]);i++){bool found=false;for(unsigned j=0;j<u->data.list_count;j++)if(!strcmp(names[i],u->data.lists[j].name))found=true;if(!found){button(u,names[i],LA_SUGGEST_LIST,0);u->refs[d->node_count-1]=i;}}
        button(u,"CUSTOM...",LA_CUSTOM_LIST,RISC_SCENE_PRIMARY);break;}
    case LISTS_OPTIONS:{if(!l){u->page=LISTS_HOME;lists_ui_declare(u);return;}header(u,l->name,"LIST OPTIONS",true);node(u,RISC_COMPONENT_ROW,"RENAME","",LA_RENAME_LIST,0);
        risc_scene_node_v1 *n=node(u,RISC_COMPONENT_SWITCH,"SHOW COMPLETED","",LA_SHOW_DONE,0);bounds(n,l->show_completed,1);button(u,"CLEAR COMPLETED",LA_CLEAR_DONE,0);button(u,"DELETE LIST",LA_DELETE_LIST,RISC_SCENE_DESTRUCTIVE);break;}
    case LISTS_NAME_EDIT:{header(u,"NAME",u->edit_kind==LA_CUSTOM_LIST||u->edit_kind==LA_RENAME_LIST?"LIST NAME":"TASK NAME",true);risc_scene_node_v1 *n=node(u,RISC_SCENE_KEYBOARD_NODE,"NAME",u->draft,LA_KEY,0);bounds(n,(int)u->key_layer,3);n->target=LISTS_NAME-1;break;}
    case LISTS_TIME:{header(u,"SET TIME",t->name,true);risc_scene_node_v1 *n=node(u,RISC_COMPONENT_TIME_PICKER,"TIME","",LA_TIME_VALUE,0);bounds(n,(int)u->time_value,1439);button(u,"SET TIME",LA_SET_TIME,RISC_SCENE_PRIMARY);button(u,"NO TIME",LA_NO_TIME,0);break;}
    case LISTS_CONFIRM:{header(u,"ARE YOU SURE?","",true);d->flags=RISC_COMPONENTS_CONFIRM;d->cancel_action=LA_KEEP;const char *title=u->confirm_kind==LA_DELETE_TASK?"DELETE TASK?":u->confirm_kind==LA_DELETE_LIST?"DELETE LIST?":"CLEAR COMPLETED?";
        node(u,RISC_COMPONENT_EMPTY,title,u->confirm_kind==LA_DELETE_LIST?"The list and all its tasks will be removed.":"This removes the selected saved items.",0,0);button(u,"KEEP",LA_KEEP,0);button(u,"DELETE",LA_CONFIRM,RISC_SCENE_DESTRUCTIVE);break;}
    case LISTS_ALERT:{lists_task *a=lists_find_task(&u->data,u->alert);if(!a){u->page=u->parent;lists_ui_declare(u);return;}header(u,"REMINDER","",true);d->flags=RISC_COMPONENTS_ALERT;d->cancel_action=LA_DISMISS;char date[40];due_text(u,a,date,sizeof(date));node(u,RISC_COMPONENT_EMPTY,a->name,date,0,0);button(u,"DONE",LA_ALERT_DONE,RISC_SCENE_PRIMARY);button(u,"+10 MIN",LA_SNOOZE,0);button(u,"DISMISS",LA_DISMISS,0);break;}
    default:u->page=LISTS_HOME;lists_ui_declare(u);break;
    }
}
static void back_page(lists_ui *u){switch(u->page){case LISTS_HOME:break;case LISTS_VIEW:u->page=LISTS_HOME;break;case LISTS_TASK:case LISTS_ADD_TASK:case LISTS_OPTIONS:u->page=LISTS_VIEW;break;case LISTS_TIME:u->page=LISTS_TASK;break;case LISTS_NAME_EDIT:u->page=u->parent;break;case LISTS_CONFIRM:u->page=u->confirm_kind==LA_DELETE_TASK?LISTS_TASK:LISTS_OPTIONS;break;default:u->page=LISTS_HOME;break;}}
static bool result(lists_ui *u,int rc,const char *message){if(rc!=LISTS_OK){lists_ui_notice(u,rc==LISTS_FULL?"STORAGE LIMIT REACHED":"CHECK NAME OR DATE");return false;}lists_ui_notice(u,message);return true;}
static bool add_task(lists_ui *u,const char *name){uint32_t list=lists_find_list(&u->data,u->view)?u->view:u->data.list_count?u->data.lists[0].id:0;if(!result(u,lists_add_task(&u->data,list,name,u->view==LISTS_VIEW_TODAY?u->today:-1,&u->task),"TASK ADDED"))return false;u->page=LISTS_TASK;return true;}
static bool add_list(lists_ui *u,const char *name){if(!result(u,lists_add_list(&u->data,name,u->marker,&u->view),"LIST ADDED"))return false;u->page=LISTS_VIEW;return true;}
static bool name_done(lists_ui *u){char name[LISTS_NAME];if(lists_name(name,u->draft)!=LISTS_OK){lists_ui_notice(u,"ENTER A NAME");return false;}
    if(u->edit_kind==LA_CUSTOM_LIST)return add_list(u,name);
    if(u->edit_kind==LA_CUSTOM_TASK)return add_task(u,name);
    if(u->edit_kind==LA_RENAME_TASK){lists_task *t=lists_find_task(&u->data,u->task);if(!t)return false;copy(t->name,sizeof(t->name),name);u->page=LISTS_TASK;}
    else {lists_list *l=lists_find_list(&u->data,u->view);if(!l)return false;for(unsigned i=0;i<u->data.list_count;i++)if(u->data.lists[i].id!=l->id&&!strcmp(u->data.lists[i].name,name)){lists_ui_notice(u,"NAME ALREADY EXISTS");return false;}copy(l->name,sizeof(l->name),name);u->page=LISTS_OPTIONS;}
    lists_ui_notice(u,"NAME SAVED");return true;
}
static bool apply(lists_ui *u,unsigned action,int value,uint32_t ref){
    lists_task *t=lists_find_task(&u->data,u->task);lists_list *l=lists_find_list(&u->data,u->view);
    switch(action){
    case LA_BACK:back_page(u);break;
    case LA_TODAY:u->view=LISTS_VIEW_TODAY;u->page=LISTS_VIEW;u->filter=0;break;
    case LA_ALL:u->view=LISTS_VIEW_ALL;u->page=LISTS_VIEW;u->filter=0;break;
    case LA_LIST:if(lists_find_list(&u->data,ref)){u->view=ref;u->page=LISTS_VIEW;u->filter=0;}break;
    case LA_NEW_LIST:u->page=LISTS_ADD_LIST;break;
    case LA_NEW_TASK:u->page=u->data.list_count?LISTS_ADD_TASK:LISTS_ADD_LIST;if(!u->data.list_count)lists_ui_notice(u,"CREATE A LIST FIRST");break;
    case LA_OPTIONS:u->page=LISTS_OPTIONS;break;
    case LA_TASK:if(lists_find_task(&u->data,ref)){u->task=ref;u->page=LISTS_TASK;}break;
    case LA_DONE:return result(u,lists_complete(&u->data,ref,value!=0,u->today),value?"TASK COMPLETED":"TASK REOPENED");
    case LA_FILTER:u->filter=(unsigned)value;break;
    case LA_MARKER:u->marker=(unsigned)value;break;
    case LA_SUGGEST_LIST:if(ref<sizeof(names)/sizeof(names[0]))return add_list(u,names[ref]);break;
    case LA_SUGGEST_TASK:if(ref<4)return add_task(u,suggestions[suggestion_set(u)][ref]);break;
    case LA_CUSTOM_LIST:case LA_CUSTOM_TASK:case LA_RENAME_LIST:case LA_RENAME_TASK:u->parent=u->page;u->edit_kind=action;u->key_layer=0;copy(u->draft,sizeof(u->draft),action==LA_RENAME_LIST&&l?l->name:action==LA_RENAME_TASK&&t?t->name:"");u->page=LISTS_NAME_EDIT;break;
    case LA_KEY:{size_t length=strlen(u->draft);if(value==RISC_SCENE_KEY_DONE)return name_done(u);if(value==RISC_SCENE_KEY_CANCEL){u->page=u->parent;break;}if(value==RISC_SCENE_KEY_LAYER)u->key_layer=(u->key_layer+1)%4;else if(value==RISC_SCENE_KEY_BACKSPACE){if(length)u->draft[length-1]=0;}else if(value==RISC_SCENE_KEY_CLEAR)u->draft[0]=0;else{if(value==RISC_SCENE_KEY_SPACE)value=' ';if(value>=32&&value<=126&&length<LISTS_NAME-1){u->draft[length]=(char)value;u->draft[length+1]=0;}}break;}
    case LA_TASK_LIST:if(t&&(unsigned)value<u->data.list_count){t->list_id=u->data.lists[value].id;return true;}break;
    case LA_DUE:if(t&&u->today>=0&&value<4){int32_t day=value==0?-1:value==3?week_end(u):u->today+(value==1?0:1);if(day>LISTS_LAST_DAY)return result(u,LISTS_INVALID,"");t->due_day=day;t->acknowledged=t->snooze=0;if(value==0){t->minute=-1;t->reminder=0;}return true;}break;
    case LA_TIME:if(t){u->time_value=t->minute<0?540:(unsigned)t->minute;u->page=LISTS_TIME;}break;
    case LA_TIME_VALUE:u->time_value=(unsigned)value;break;
    case LA_SET_TIME:if(t){if(t->due_day<0)t->due_day=u->today;t->minute=(int16_t)u->time_value;t->acknowledged=t->snooze=0;u->page=LISTS_TASK;return true;}break;
    case LA_NO_TIME:if(t){t->minute=-1;t->reminder=0;t->acknowledged=t->snooze=0;u->page=LISTS_TASK;return true;}break;
    case LA_PRIORITY:if(t){t->priority=(uint8_t)value;return true;}break;
    case LA_REMINDER:if(t){t->reminder=(uint8_t)value;t->acknowledged=t->snooze=0;return true;}break;
    case LA_REPEAT:if(t){t->repeat=(uint8_t)value;return true;}break;
    case LA_SHOW_DONE:if(l){l->show_completed=(uint8_t)value;return true;}break;
    case LA_DELETE_TASK:case LA_DELETE_LIST:case LA_CLEAR_DONE:u->confirm_kind=action;u->page=LISTS_CONFIRM;break;
    case LA_KEEP:back_page(u);break;
    case LA_CONFIRM:if(u->confirm_kind==LA_DELETE_TASK){if(!t)return false;lists_delete_task(&u->data,t->id);u->page=LISTS_VIEW;}else if(u->confirm_kind==LA_DELETE_LIST){if(!l)return false;lists_delete_list(&u->data,l->id);u->page=LISTS_HOME;}else{lists_clear_completed(&u->data,u->view);u->page=LISTS_OPTIONS;}lists_ui_notice(u,"DELETED");return true;
    case LA_ALERT_DONE:case LA_SNOOZE:case LA_DISMISS:{lists_task *a=lists_find_task(&u->data,u->alert);if(!a)return false;if(action==LA_ALERT_DONE){int rc=lists_complete(&u->data,a->id,true,u->today);if(rc!=LISTS_OK)return result(u,rc,"");}else if(action==LA_SNOOZE){if(u->today<0)return false;a->snooze=(uint32_t)u->today*1440u+u->minute+10;a->acknowledged=0;}else a->acknowledged=lists_reminder_deadline(a);u->alert=0;u->page=u->parent;return true;}
    default:break;
    }return false;
}
bool lists_ui_event(lists_ui *u,const risc_scene_event_v1 *e){
    if(!e||e->struct_size!=sizeof(*e)||e->document_revision!=u->document.revision||!e->sequence||e->sequence<=u->last_event)return false;
    unsigned action=e->action;uint32_t ref=0;bool valid=false;
    if(!e->node){valid=e->kind==RISC_SCENE_ACTION_EVENT&&(action==u->document.routes[0].back_action||(u->document.flags&&action==u->document.cancel_action)||(u->undo_valid&&action==LA_UNDO));}
    else if(e->node<=u->document.node_count){const risc_scene_node_v1 *n=&u->document.nodes[e->node-1];const risc_component_detail_v1 *d=&u->document.details[e->node-1];if(!(n->flags&(RISC_SCENE_DISABLED|RISC_SCENE_HIDDEN))){
        bool value_node=n->kind==RISC_COMPONENT_CHECK_ROW||n->kind==RISC_COMPONENT_STEPPER||n->kind==RISC_COMPONENT_SEGMENTS||n->kind==RISC_COMPONENT_MARKERS||n->kind==RISC_COMPONENT_TIME_PICKER||n->kind==RISC_COMPONENT_SWITCH;
        valid=(e->kind==RISC_SCENE_ACTION_EVENT&&((!value_node&&action==n->action)||(d->secondary_action&&action==d->secondary_action)))||(e->kind==RISC_SCENE_VALUE_EVENT&&action==n->action&&((value_node&&e->value>=n->minimum&&e->value<=n->maximum)||n->kind==RISC_SCENE_KEYBOARD_NODE));ref=u->refs[e->node-1];}}
    if(!valid||!action)return false;
    u->last_event=e->sequence;
    if(u->readonly)return false;
    if(action==LA_UNDO){uint32_t next=u->data.next_id;u->data=u->undo;if(u->data.next_id<next)u->data.next_id=next;u->undo_valid=false;u->page=LISTS_HOME;lists_ui_notice(u,"RESTORED");return true;}
    lists_model previous=u->data;uint32_t token=u->toast_token;bool changed=apply(u,action,e->value,ref);
    if(changed){u->undo=previous;u->undo_valid=true;if(token==u->toast_token)lists_ui_notice(u,"SAVED");}return changed;
}
void lists_ui_clock(lists_ui *u,int32_t day,unsigned minute){
    u->today=day;u->minute=minute;
    if(day<0||u->readonly||u->page==LISTS_ALERT||u->page==LISTS_CONFIRM||u->page==LISTS_NAME_EDIT||u->page==LISTS_TIME)return;
    uint32_t id=lists_pending_reminder(&u->data,(uint32_t)day*1440+minute);if(id){u->parent=u->page;u->page=LISTS_ALERT;u->alert=id;}
}
