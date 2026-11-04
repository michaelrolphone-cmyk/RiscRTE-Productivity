#include <assert.h>
#include <stdio.h>
#include "../../Apps/timecard_scene.c"
static char disk[JSON_CAPACITY];
static uint32_t disk_size;
static uint64_t disk_revision;
static unsigned writes,reads,stats,releases,closes,retains,step;
static bool lose_after_write,loss,fail_save,commit_error,absent_clock,absent_data,close_loss;
static int32_t clock_day=20761;
static risc_runtime_api_v1 fake_rt;
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1&&!loss?&fake_rt:NULL;}
static bool keep(void){retains++;loss=true;return true;}
static int32_t ds(void *c,const char *n,uint32_t *s,uint64_t *v){(void)c;assert(!loss);assert(!strcmp(n,"timecard.json"));stats++;*s=disk_size;*v=disk_revision;return disk_revision?0:RISC_APP_DATA_NOT_FOUND;}
static int32_t dr(void *c,const char *n,uint64_t v,void *p,uint32_t cap,uint32_t *s,uint64_t *out){(void)c;(void)n;assert(!loss);reads++;assert(v==disk_revision&&cap>=disk_size);memcpy(p,disk,disk_size);*s=disk_size;*out=v;return 0;}
static int32_t dw(void *c,const char *n,uint64_t v,const void *p,uint32_t s){(void)c;(void)n;assert(!loss);assert(v==disk_revision);writes++;if(fail_save)return RISC_APP_DATA_IO;memcpy(disk,p,s);disk_size=s;disk_revision++;if(lose_after_write)loss=true;if(commit_error)return RISC_APP_DATA_COMMIT_UNKNOWN;return 0;}
static const risc_app_data_v1 store_api={.api_version=1,.struct_size=sizeof(store_api),.stat=ds,.read=dr,.replace=dw};
static int32_t clock_read(void *c,risc_civil_time_v1 *out){(void)c;assert(!loss);out->day=clock_day;out->minute=570;return 0;}
static const risc_civil_clock_api_v1 civil_api={.api_version=1,.struct_size=sizeof(civil_api),.read=clock_read};
static int32_t scene_open(void *c,const risc_components_document_v1 *d,const risc_scene_navigation_v1 *n,uint64_t *s){(void)c;(void)n;assert(!loss);assert(d->revision&&d->route_count==1);*s=1;return 0;}
static int32_t scene_update(void *c,uint64_t s,const risc_components_document_v1 *d){(void)c;assert(!loss&&s==1&&d->revision);return 0;}
static unsigned find_node(unsigned action,int32_t ref){for(unsigned i=0;i<doc.node_count;i++)if(doc.nodes[i].action==action&&refs[i]==ref)return i+1;assert(0);return 0;}
static uint64_t sequence;
static void dispatch(unsigned action,int value,int32_t ref){
 risc_scene_event_v1 e={.struct_size=sizeof(e),.kind=action==TC_KEY?RISC_SCENE_VALUE_EVENT:RISC_SCENE_ACTION_EVENT,
 .document_revision=doc.revision,.node=action==TC_BACK?0:find_node(action,ref),.action=action,.value=value,.sequence=++sequence};
 handle(&e);if(!terminal)declare();
}
static int32_t scene_next(void *c,uint64_t s,risc_scene_event_v1 *e){
 (void)c;assert(!loss&&s==1);e->sequence=++sequence;e->document_revision=doc.revision;e->kind=RISC_SCENE_ACTION_EVENT;
 if(absent_clock||absent_data){assert(doc.nodes[0].kind==RISC_COMPONENT_EMPTY);e->action=TC_BACK;return 0;}
 switch(step++){
 case 0:e->action=TC_WEEK;e->node=find_node(TC_WEEK,0);break;
 case 1:e->action=TC_PUNCH;e->node=find_node(TC_PUNCH,0);break;
 default:e->kind=RISC_SCENE_SUSPEND_EVENT;break;
 }return 0;
}
static int32_t scene_snapshot(void *c,uint64_t s,risc_scene_navigation_v1 *n,uint32_t *f){(void)c;(void)s;(void)n;assert(!loss);*f=0;return 0;}
static int32_t scene_close(void *c,uint64_t s){(void)c;assert(!loss&&s==1);closes++;return close_loss?-9:0;}
static const risc_scene_components_api_v1 scene_api={
 .lifecycle={.base={.api_version=1,.struct_size=sizeof(scene_api),.next=scene_next,.snapshot=scene_snapshot,.close=scene_close}},
 .tag=RISC_COMPONENTS_TAG,.version=1,.open=scene_open,.update=scene_update
};
static bool acquire_api(const char *name,uint32_t v,uint64_t instance,risc_runtime_capability_v1 *g){
 assert(!loss&&v==1);(void)instance;
 if(!strcmp(name,RISC_SCENE_CAPABILITY))g->api=&scene_api;
 else if(!strcmp(name,RISC_APP_DATA_CAPABILITY)&&!absent_data)g->api=&store_api;
 else if(!strcmp(name,RISC_CIVIL_CLOCK_CAPABILITY)&&!absent_clock)g->api=&civil_api;
 else return false;
 return true;
}
static bool release_api(risc_runtime_capability_v1 *g){(void)g;assert(!loss);releases++;return true;}
static void yield_api(uint32_t ms){assert(!loss&&ms<=50);}
static void reset(void){
 lose_after_write=loss=terminal=fail_save=commit_error=absent_data=absent_clock=close_loss=false;
 writes=reads=stats=releases=closes=retains=step=0;disk_size=0;disk_revision=0;sequence=0;
 fake_rt=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(fake_rt),.acquire=acquire_api,.release=release_api,.retain_invocation=keep,.yield_ms=yield_api};
}
static void model_init(void){
 reset();rt=&fake_rt;data_api=&store_api;guarded_data=store_api;guarded_data.stat=stat_data;guarded_data.read=read_data;guarded_data.replace=replace_data;assert(tcp_appdata_bind(&data,&guarded_data));clock_api=&civil_api;
 day_count=0;screen_id=SCREEN_WEEK_LIST;revision=0;last_event=0;editor=false;assert(reload()&&store_ready);assert(sample_clock()&&clock_valid);declare();
}
int main(void){
 model_init();assert(doc.node_count==20);char formatted[24];time_format=1;format_time(570,formatted,sizeof(formatted));assert(!strcmp(formatted,"09:30"));time_format=0;
 dispatch(TC_WEEK,0,0);assert(screen_id==SCREEN_WEEK);
 dispatch(TC_DAY,0,today_ymd);assert(screen_id==SCREEN_DAY);
 dispatch(TC_EDIT,0,0);assert(editor);
 dispatch(TC_KEY,'9',0);dispatch(TC_KEY,':',0);dispatch(TC_KEY,'3',0);dispatch(TC_KEY,'0',0);dispatch(TC_KEY,RISC_SCENE_KEY_DONE,0);
 assert(!editor&&get_day(today_ymd).punches[0]==570&&writes==1);
 dispatch(TC_EDIT,0,3);dispatch(TC_KEY,'1',0);dispatch(TC_KEY,'7',0);dispatch(TC_KEY,RISC_SCENE_KEY_DONE,0);
 assert(worked(find_day(today_ymd))==450);
 assert(mutate(today_ymd,1,720)&&mutate(today_ymd,2,750));assert(worked(find_day(today_ymd))==420);
 unsigned saved=writes;dispatch(TC_EDIT,0,0);dispatch(TC_KEY,RISC_SCENE_KEY_CLEAR,0);dispatch(TC_KEY,'x',0);dispatch(TC_KEY,RISC_SCENE_KEY_DONE,0);assert(editor&&writes==saved);dispatch(TC_BACK,0,0);assert(!editor);
 risc_scene_event_v1 stale={.struct_size=sizeof(stale),.kind=RISC_SCENE_ACTION_EVENT,.document_revision=doc.revision-1,.node=1,.action=TC_EDIT,.sequence=++sequence};
 handle(&stale);assert(!editor);stale.document_revision=doc.revision;stale.node=99;handle(&stale);assert(!editor);
 fail_save=true;int old=get_day(today_ymd).punches[0];assert(!mutate(today_ymd,0,600)&&!store_ready&&get_day(today_ymd).punches[0]==old);
 fail_save=false;assert(reload()&&store_ready);commit_error=true;assert(!mutate(today_ymd,0,600)&&!store_ready&&get_day(today_ymd).punches[0]==old);
 commit_error=false;assert(reload()&&store_ready&&get_day(today_ymd).punches[0]==600);
 /* All 400 records survive an attempted 401st insert. */
 day_count=MAX_DAYS;for(int i=0;i<MAX_DAYS;i++){days[i]=blank_day(add_days(20200101,i));days[i].punches[0]=1;}
 saved=writes;assert(!mutate(20260101,0,1)&&writes==saved&&day_count==MAX_DAYS&&days[0].ymd==20200101);
 assert(mutate(20200101,0,-1)&&day_count==MAX_DAYS-1);assert(mutate(20260101,0,1)&&day_count==MAX_DAYS);
 memcpy(disk,"{\"days\":[{\"d\":20260230}]}",26);disk_size=26;disk_revision++;assert(reload()&&!store_ready);
 reset();app_main();assert(writes==1&&closes==1&&releases==3&&!retains);
 reset();absent_data=true;app_main();assert(!writes&&closes==1&&releases==2);
 reset();absent_clock=true;app_main();assert(!writes&&closes==1&&releases==2);
 reset();lose_after_write=true;app_main();assert(writes==1&&!closes&&!releases);
 reset();close_loss=true;app_main();assert(closes==1&&retains==1&&!releases);
 puts("Timecard shared scene model, workflow, storage and custody tests passed.");
 return 0;
}
