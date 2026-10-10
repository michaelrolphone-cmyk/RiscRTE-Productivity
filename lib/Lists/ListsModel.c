#include "ListsModel.h"
#include <string.h>
static unsigned month_days(unsigned y,unsigned m){static const uint8_t n[]={31,28,31,30,31,30,31,31,30,31,30,31};return m>=1&&m<=12?n[m-1]+(m==2&&y%4==0&&(y%100!=0||y%400==0)):0;}
int32_t lists_day(unsigned y,unsigned m,unsigned d){
    if(y<2000||y>2099||!d||d>month_days(y,m))return -1;
    int32_t days=0;for(unsigned i=2000;i<y;i++)days+=365+(i%4==0);for(unsigned i=1;i<m;i++)days+=(int)month_days(y,i);return days+(int)d-1;
}
bool lists_civil(int32_t day,unsigned *year,unsigned *month,unsigned *date){
    if(day<0||day>LISTS_LAST_DAY||!year||!month||!date)return false;
    unsigned y=2000,m=1;while(day>=365+(y%4==0)){day-=365+(y%4==0);++y;}while(day>=(int)month_days(y,m)){day-=(int)month_days(y,m);++m;}
    *year=y;*month=m;*date=(unsigned)day+1;return true;
}
void lists_init(lists_model *m){memset(m,0,sizeof(*m));m->next_id=1;}
int lists_name(char out[LISTS_NAME],const char *s){
    if(!s)return LISTS_INVALID;
    size_t n=0;while(*s==' ')++s;
    for(;*s;s++){unsigned char c=(unsigned char)*s;if(c<32||c>126||n>=LISTS_NAME-1)return LISTS_INVALID;out[n++]=(char)(c>='a'&&c<='z'?c-32:c);}
    while(n&&out[n-1]==' ')--n;
    out[n]=0;return n?LISTS_OK:LISTS_INVALID;
}
static bool name_valid(const char *s){char copy[LISTS_NAME];return memchr(s,0,LISTS_NAME)&&lists_name(copy,s)==LISTS_OK&&!strcmp(s,copy);}
lists_list *lists_find_list(lists_model *m,uint32_t id){for(unsigned i=0;i<m->list_count;i++)if(m->lists[i].id==id)return &m->lists[i];return NULL;}
lists_task *lists_find_task(lists_model *m,uint32_t id){for(unsigned i=0;i<m->task_count;i++)if(m->tasks[i].id==id)return &m->tasks[i];return NULL;}
bool lists_valid(const lists_model *m){
    if(!m||!m->next_id||m->list_count>LISTS_MAX_LISTS||m->task_count>LISTS_MAX_TASKS)return false;
    for(unsigned i=0;i<m->list_count;i++){
        const lists_list *l=&m->lists[i];if(!l->id||l->id>=m->next_id||!name_valid(l->name)||l->marker>5||l->show_completed>1)return false;
        for(unsigned j=0;j<i;j++)if(l->id==m->lists[j].id||!strcmp(l->name,m->lists[j].name))return false;
    }
    for(unsigned i=0;i<m->task_count;i++){
        const lists_task *t=&m->tasks[i];bool found=false;for(unsigned j=0;j<m->list_count;j++)if(m->lists[j].id==t->list_id)found=true;
        if(!found||!t->id||t->id>=m->next_id||!name_valid(t->name)||t->due_day< -1||t->due_day>LISTS_LAST_DAY||t->minute< -1||t->minute>1439||t->priority>2||t->reminder>3||t->repeat>2||t->done>1||t->spawned>1)return false;
        if((t->due_day<0&&(t->minute>=0||t->reminder))||(t->minute<0&&t->reminder))return false;
        if(t->acknowledged>(LISTS_LAST_DAY+1u)*1440u||t->snooze>(LISTS_LAST_DAY+1u)*1440u)return false;
        for(unsigned j=0;j<i;j++)if(t->id==m->tasks[j].id)return false;
        for(unsigned j=0;j<m->list_count;j++)if(t->id==m->lists[j].id)return false;
    }return true;
}
int lists_add_list(lists_model *m,const char *name,unsigned marker,uint32_t *id){
    lists_list l={0};if(marker>5||lists_name(l.name,name)!=LISTS_OK)return LISTS_INVALID;
    for(unsigned i=0;i<m->list_count;i++)if(!strcmp(l.name,m->lists[i].name))return LISTS_INVALID;
    if(m->list_count==LISTS_MAX_LISTS||m->next_id==UINT32_MAX)return LISTS_FULL;
    l.id=m->next_id++;l.marker=(uint8_t)marker;m->lists[m->list_count++]=l;if(id)*id=l.id;return LISTS_OK;
}
int lists_add_task(lists_model *m,uint32_t list,const char *name,int32_t day,uint32_t *id){
    lists_task t={.list_id=list,.due_day=day,.minute=-1};if(!lists_find_list(m,list)||day< -1||day>LISTS_LAST_DAY||lists_name(t.name,name)!=LISTS_OK)return LISTS_INVALID;
    if(m->task_count==LISTS_MAX_TASKS||m->next_id==UINT32_MAX)return LISTS_FULL;
    t.id=m->next_id++;m->tasks[m->task_count++]=t;if(id)*id=t.id;return LISTS_OK;
}
int lists_complete(lists_model *m,uint32_t id,bool done,int32_t today){
    lists_task *t=lists_find_task(m,id);if(!t)return LISTS_NOT_FOUND;
    if(done&&!t->done&&t->repeat&&!t->spawned){
        if(m->task_count==LISTS_MAX_TASKS||m->next_id==UINT32_MAX)return LISTS_FULL;
        int32_t base=t->due_day>=0?t->due_day:today,interval=t->repeat==1?1:7;
        if(base<0||base+interval>LISTS_LAST_DAY)return LISTS_INVALID;
        /* Skip missed occurrences while preserving the original weekday. */
        int32_t next=base+interval;if(today>=next)next+=((today-next)/interval+1)*interval;
        if(next>LISTS_LAST_DAY)return LISTS_INVALID;
        lists_task n=*t;n.id=m->next_id++;n.due_day=next;n.done=n.spawned=0;n.acknowledged=n.snooze=0;
        m->tasks[m->task_count++]=n;t->spawned=1;
    }
    t->done=done;if(done){t->acknowledged=lists_reminder_deadline(t);t->snooze=0;}return LISTS_OK;
}
int lists_delete_task(lists_model *m,uint32_t id){
    for(unsigned i=0;i<m->task_count;i++)if(m->tasks[i].id==id){for(unsigned j=i+1;j<m->task_count;j++)m->tasks[j-1]=m->tasks[j];memset(&m->tasks[--m->task_count],0,sizeof(m->tasks[0]));return LISTS_OK;}
    return LISTS_NOT_FOUND;
}
int lists_delete_list(lists_model *m,uint32_t id){
    for(unsigned i=0;i<m->list_count;i++)if(m->lists[i].id==id){for(unsigned j=0;j<m->task_count;)if(m->tasks[j].list_id==id)(void)lists_delete_task(m,m->tasks[j].id);else ++j;
        for(unsigned j=i+1;j<m->list_count;j++)m->lists[j-1]=m->lists[j];
        memset(&m->lists[--m->list_count],0,sizeof(m->lists[0]));return LISTS_OK;}
    return LISTS_NOT_FOUND;
}
void lists_clear_completed(lists_model *m,uint32_t id){for(unsigned i=0;i<m->task_count;)if(m->tasks[i].done&&m->tasks[i].list_id==id)(void)lists_delete_task(m,m->tasks[i].id);else ++i;}
uint32_t lists_reminder_deadline(const lists_task *t){
    if(!t->reminder||t->due_day<0||t->minute<0)return 0;
    if(t->snooze)return t->snooze;
    uint32_t due=(uint32_t)t->due_day*1440u+(unsigned)t->minute,offset=t->reminder==2?10:t->reminder==3?60:0;
    return due>offset?due-offset:1;
}
uint32_t lists_pending_reminder(const lists_model *m,uint32_t now){
    uint32_t result=0,earliest=UINT32_MAX;
    for(unsigned i=0;i<m->task_count;i++){const lists_task *t=&m->tasks[i];uint32_t at=lists_reminder_deadline(t);if(!t->done&&at&&at<=now&&t->acknowledged!=at&&at<earliest){earliest=at;result=t->id;}}return result;
}
bool lists_overdue(const lists_task *t,int32_t today,unsigned minute){return !t->done&&t->due_day>=0&&today>=0&&(t->due_day<today||(t->due_day==today&&t->minute>=0&&(unsigned)t->minute<minute));}
static void put(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(8*i));}
static uint32_t get(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint32_t crc(const uint8_t *p,size_t n){uint32_t c=UINT32_MAX;for(size_t i=0;i<n;i++){c^=p[i];for(unsigned j=0;j<8;j++)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}return ~c;}
bool lists_encode(const lists_model *m,uint8_t out[LISTS_WIRE_SIZE]){
    if(!lists_valid(m))return false;
    memset(out,0,LISTS_WIRE_SIZE);memcpy(out,"N7LS",4);put(out+4,1);put(out+8,LISTS_WIRE_SIZE);put(out+12,m->next_id);put(out+16,m->list_count);put(out+20,m->task_count);
    for(unsigned i=0;i<m->list_count;i++){const lists_list *l=&m->lists[i];uint8_t *p=out+32+i*40;put(p,l->id);memcpy(p+4,l->name,LISTS_NAME);p[29]=l->marker;p[30]=l->show_completed;}
    for(unsigned i=0;i<m->task_count;i++){const lists_task *t=&m->tasks[i];uint8_t *p=out+32+LISTS_MAX_LISTS*40+i*64;put(p,t->id);put(p+4,t->list_id);memcpy(p+8,t->name,LISTS_NAME);put(p+36,(uint32_t)(t->due_day+1));put(p+40,(uint32_t)(t->minute+1));p[44]=t->priority;p[45]=t->reminder;p[46]=t->repeat;p[47]=t->done;p[48]=t->spawned;put(p+52,t->acknowledged);put(p+56,t->snooze);}
    put(out+28,crc(out+32,LISTS_WIRE_SIZE-32)^crc(out,28));return true;
}
bool lists_decode(lists_model *out,const uint8_t *p,size_t size){
    if(!out||!p||size!=LISTS_WIRE_SIZE||memcmp(p,"N7LS",4)||get(p+4)!=1||get(p+8)!=size||get(p+24)||get(p+28)!=(crc(p+32,size-32)^crc(p,28)))return false;
    lists_model m={.next_id=get(p+12),.list_count=get(p+16),.task_count=get(p+20)};if(m.list_count>LISTS_MAX_LISTS||m.task_count>LISTS_MAX_TASKS)return false;
    for(unsigned i=0;i<m.list_count;i++){const uint8_t *q=p+32+i*40;lists_list *l=&m.lists[i];l->id=get(q);memcpy(l->name,q+4,LISTS_NAME);l->marker=q[29];l->show_completed=q[30];}
    for(unsigned i=0;i<m.task_count;i++){const uint8_t *q=p+32+LISTS_MAX_LISTS*40+i*64;lists_task *t=&m.tasks[i];if(get(q+36)>LISTS_LAST_DAY+1u||get(q+40)>1440)return false;t->id=get(q);t->list_id=get(q+4);memcpy(t->name,q+8,LISTS_NAME);t->due_day=(int32_t)get(q+36)-1;t->minute=(int16_t)get(q+40)-1;t->priority=q[44];t->reminder=q[45];t->repeat=q[46];t->done=q[47];t->spawned=q[48];t->acknowledged=get(q+52);t->snooze=get(q+56);}
    if(!lists_valid(&m))return false;
    *out=m;return true;
}
