/* Exercise the actual shared provider, touch queue, renderer and Lists reducer.
 * The transport doubles are the same ones used by the presenter's regression
 * suite. Only fixture data (never shipped user data) is seeded here. */
#define main scene_baseline_main
#include "host_test.c"
#undef main
#include "../../Apps/ListsScene.h"
#include "SceneKeyboardV1.h"
static lists_ui ui;
static const risc_scene_components_api_v1 *components;
static void update_ui(void){lists_ui_declare(&ui);assert(components->update(NULL,session,&ui.document)==0);}
static unsigned task_rows(void){unsigned count=0;for(unsigned i=0;i<ui.document.node_count;i++)count+=ui.document.nodes[i].kind==RISC_COMPONENT_CHECK_ROW;return count;}
static void fixture(void){
 lists_ui_init(&ui);ui.today=lists_day(2026,10,10);ui.minute=660;uint32_t work,grocery,home,id;
 assert(!lists_add_list(&ui.data,"GROCERIES",0,&grocery));assert(!lists_add_list(&ui.data,"WORK",1,&work));assert(!lists_add_list(&ui.data,"HOME",2,&home));
 assert(!lists_add_task(&ui.data,work,"SEND WEEKLY REPORT",ui.today,&id));ui.data.tasks[0].minute=900;ui.data.tasks[0].priority=2;ui.data.tasks[0].repeat=2;
 assert(!lists_add_task(&ui.data,work,"REVIEW PR #42",ui.today,&id));ui.data.tasks[1].minute=600;
 assert(!lists_add_task(&ui.data,work,"BOOK ROOM FOR DEMO",ui.today+1,&id));
 assert(!lists_add_task(&ui.data,grocery,"MILK",ui.today,&id));assert(!lists_add_task(&ui.data,grocery,"EGGS",-1,&id));assert(!lists_add_task(&ui.data,home,"WATER THE PLANTS",ui.today,&id));
 ui.data.tasks[4].done=1;ui.view=work;ui.task=ui.data.tasks[0].id;
}
static const char *device;
static void frame(const char *folder,const char *name){
 settle();unsigned lw=w==800?h:w,lh=w==800?w:h;char path[1024];snprintf(path,sizeof(path),"%s/%s-%s.ppm",folder,device,name);
 FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%u %u\n255\n",lw,lh);
 for(unsigned y=0;y<lh;y++)for(unsigned x=0;x<lw;x++){
  unsigned px=x,py=y;if(prof.display_rotation==270){px=y;py=h-1-x;}
  uint8_t rgb[3];if(format==5){unsigned at=16+(py*w+px)*2;uint16_t c=pixels[at]|(uint16_t)pixels[at+1]<<8;rgb[0]=(uint8_t)((c>>11)*255/31);rgb[1]=(uint8_t)(((c>>5)&63)*255/63);rgb[2]=(uint8_t)((c&31)*255/31);}
  else{unsigned bit=py*w+px;rgb[0]=rgb[1]=rgb[2]=pixels[16+bit/8]&(128>>(bit%8))?0:255;}
  assert(fwrite(rgb,1,3,f)==3);
 }assert(!fclose(f));
}
int main(int argc,char **argv){
 assert(argc==3);device=argv[1];if(!strcmp(device,"paper")){w=800;h=480;format=1;prof=(risc_scene_profile_v1){1,sizeof(prof),1,3,88,20,0,0xffff,0,270,0,0};}
 startup();assert(api->close(NULL,session)==0);components=risc_scene_components_get_v1(api);assert(components);fixture();lists_ui_declare(&ui);
 expected_intent=RISC_DISPLAY_PRESENT_LOW_LATENCY;assert(!components->open(NULL,&ui.document,NULL,&session));frame(argv[2],"home");
 ui.page=LISTS_VIEW;update_ui();frame(argv[2],"list");
 unsigned scale=w==800?2:1,header=scale==2?56:78,first_y=(header+46+38+(scale==2?21:20))*scale;
 touch_tap(25*scale,first_y);risc_scene_event_v1 e;assert(tick(&e)==RISC_SCENE_OK&&e.action==LA_DONE&&e.kind==RISC_SCENE_VALUE_EVENT);assert(lists_ui_event(&ui,&e));update_ui();
 /* OPEN/ALL swaps this list in place while a refresh is outstanding. */
 unsigned open_rows=task_rows();allow_complete=false;begin_frame();
 touch_tap(172*scale,(header+46+21)*scale);assert(tick(&e)==RISC_SCENE_OK&&e.action==LA_FILTER&&e.value==1);assert(!lists_ui_event(&ui,&e));update_ui();assert(task_rows()==open_rows+1&&ui.page==LISTS_VIEW);
 allow_complete=true;ui.notice[0]=0;update_ui();frame(argv[2],"list-all");
 touch_tap(68*scale,(header+46+21)*scale);assert(tick(&e)==RISC_SCENE_OK&&e.action==LA_FILTER&&e.value==0);assert(!lists_ui_event(&ui,&e));update_ui();assert(task_rows()==open_rows);
 /* Details target remains distinct after the completed row leaves OPEN. */
 touch_tap(90*scale,first_y);assert(tick(&e)==RISC_SCENE_OK&&e.action==LA_TASK);assert(!lists_ui_event(&ui,&e)&&ui.page==LISTS_TASK);update_ui();frame(argv[2],"task");
 ui.page=LISTS_TIME;ui.time_value=905;update_ui();frame(argv[2],"time");
 ui.page=LISTS_NAME_EDIT;ui.edit_kind=LA_RENAME_TASK;strcpy(ui.draft,"WEEKLY REPORT");update_ui();frame(argv[2],"keyboard");
 if(scale==1){touch_tap(30,24);assert(tick(&e)==RISC_SCENE_OK&&e.action==LA_KEY&&e.value==RISC_SCENE_KEY_CANCEL);}
 ui.page=LISTS_CONFIRM;ui.confirm_kind=LA_DELETE_TASK;update_ui();frame(argv[2],"confirm");
 risc_scene_navigation_v1 focus={.struct_size=sizeof(focus)};uint32_t flags;assert(!api->snapshot(NULL,session,&focus,&flags));assert(focus.focus[0]&&ui.document.nodes[focus.focus[0]-1].action==LA_KEEP);
 unsigned middle=scale==2?200:120;
 touch_tap(120*scale,middle*scale);assert(tick(&e)==RISC_SCENE_IDLE);
 touch_tap(75*scale,(middle+44)*scale);assert(tick(&e)==RISC_SCENE_OK&&e.action==LA_KEEP);
 touch_tap(5*scale,5*scale);assert(tick(&e)==RISC_SCENE_OK&&e.action==LA_KEEP);
 ui.page=LISTS_ALERT;ui.alert=ui.task;update_ui();frame(argv[2],"alert");
 ui.page=LISTS_OPTIONS;update_ui();frame(argv[2],"options");
 ui.page=LISTS_ADD_TASK;update_ui();frame(argv[2],"new-task");
 ui.page=LISTS_ADD_LIST;update_ui();frame(argv[2],"new-list");
 touch_tap(48*scale,((scale==2?60:84)+73)*scale);assert(tick(&e)==RISC_SCENE_OK&&e.action==LA_SUGGEST_LIST);
 /* Slow refresh never blocks a fresh control target. */
 allow_complete=false;update_ui();begin_frame();touch_tap(60*scale,((scale==2?60:84)+25)*scale);assert(tick(&e)==RISC_SCENE_OK&&e.action==LA_MARKER&&e.value==1);assert(!lists_ui_event(&ui,&e));update_ui();
 /* Invalid component documents are rejected without losing the prior route. */
 risc_components_document_v1 bad=ui.document;bad.revision++;bad.nodes[0].maximum=7;assert(components->update(NULL,session,&bad)==RISC_SCENE_INVALID);
 allow_complete=true;settle();
 /* Capture the completed state that was missing from the original review. */
 for(unsigned i=0;i<ui.data.task_count;i++)ui.data.tasks[i].done=1;
 ui.page=LISTS_VIEW;ui.filter=0;ui.notice[0]=0;update_ui();frame(argv[2],"completed");
 finish();printf("Lists actual renderer + separated touch targets + pending frame %s PASS\n",device);return 0;
}
