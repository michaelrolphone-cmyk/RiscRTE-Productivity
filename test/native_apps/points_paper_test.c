/* Points model and production System Apps MONO1 renderer linked independently. */
#define main original_points_fixture_main
#define t5_app_get_api points_model_app_api
#define risc_runtime_get_api points_model_runtime_api
#define portable_nova_begin points_fixture_nova_begin
#define portable_nova_measure points_fixture_nova_measure
#define portable_nova_text points_fixture_nova_text
#define portable_nova_center points_fixture_nova_center
#define portable_nova_right points_fixture_nova_right
#define portable_nova_fill points_fixture_nova_fill
#define portable_nova_round points_fixture_nova_round
#define portable_nova_button points_fixture_nova_button
#define portable_nova_row points_fixture_nova_row
#define portable_nova_header points_fixture_nova_header
#define portable_nova_rule points_fixture_nova_rule
#define portable_nova_hit points_fixture_nova_hit
#define portable_nova_wrap points_fixture_nova_wrap
#include "points_in_time_test.c"
#undef portable_nova_begin
#undef portable_nova_measure
#undef portable_nova_text
#undef portable_nova_center
#undef portable_nova_right
#undef portable_nova_fill
#undef portable_nova_round
#undef portable_nova_button
#undef portable_nova_row
#undef portable_nova_header
#undef portable_nova_rule
#undef portable_nova_hit
#undef portable_nova_wrap
#undef risc_runtime_get_api
#undef t5_app_get_api
#undef main
#include "RiscDisplayOutputV1.h"
#include "RiscTouchV1.h"
const t5_app_api_v1 *t5_app_get_api(uint32_t);
int app_module_init(void);void app_module_fini(void);
#include "PortableApps.h"
const t5_app_manifest_t portable_catalog[]={{.compatible=false}};
const unsigned portable_catalog_count=0;
static uint8_t pixels[48000],first_pixels[48000];
static unsigned renderer_grants,renderer_subs,renderer_frames,renderer_presents,renderer_polls,renderer_limit=10000;
static bool renderer_script;
static risc_touch_snapshot_v1 renderer_touch={.width=480,.height=800};
static bool host_health(risc_runtime_health_v1 *h){h->uptime_ms=ticks;return renderer_polls<renderer_limit;}
static void host_yield(uint32_t n){ticks+=n;}
static bool host_diag(const char *s){(void)s;return true;}
static bool host_launch(const char *s){(void)s;return true;}
static bool host_info(void*c,risc_display_info_v1 *out){(void)c;*out=(risc_display_info_v1){.width=800,.height=480,.flags=RISC_DISPLAY_INFO_RETAINS_IMAGE|RISC_DISPLAY_INFO_PARTIAL_DAMAGE|RISC_DISPLAY_INFO_CLEAN_PRESENT,.supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_MONO1),.damage_x_alignment=8,.damage_width_alignment=8};return true;}
static bool host_frame(void*c,uint32_t format,risc_display_surface_v1*out){(void)c;assert(format==RISC_DISPLAY_FORMAT_MONO1&&!renderer_frames);renderer_frames++;*out=(risc_display_surface_v1){.frame=1,.pixels=pixels,.width=800,.height=480,.stride_bytes=100,.size_bytes=sizeof(pixels),.pixel_format=format};return true;}
static void host_frame_release(void*c,risc_display_frame_v1 f){(void)c;assert(f==1&&renderer_frames);renderer_frames--;}
static bool host_submit(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*t){(void)c;(void)r;(void)n;(void)o;assert(f==1&&renderer_frames);renderer_frames--;*t=++renderer_presents;if(renderer_presents==1)memcpy(first_pixels,pixels,sizeof(pixels));return true;}
static bool host_status(void*c,risc_display_present_token_v1 t,risc_display_present_status_v1*out){(void)c;assert(t);out->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;}
static uint64_t host_subscribe(void*c){(void)c;renderer_subs++;return 1;}
static bool host_unsubscribe(void*c,uint64_t s){(void)c;assert(s==1&&renderer_subs);renderer_subs--;return true;}
static bool host_touch_poll(void*c,size_t n){(void)c;assert(n==1);renderer_polls++;return true;}
static int32_t host_touch_next(void*c,uint64_t s,risc_touch_event_v1*e){(void)c;(void)s;(void)e;return 0;}
static bool host_touch_snapshot(void*c,risc_touch_snapshot_v1*out){(void)c;*out=renderer_touch;
 if(renderer_script){out->contact_count=0;unsigned x=0,y=0;switch(renderer_polls){case 3:x=350;y=730;break;case 7:x=100;y=240;break;case 11:x=350;y=280;break;case 15:x=350;y=730;break;case 19:x=100;y=730;break;case 23:x=100;y=730;break;}if(x){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=x,.y=y};}}
 return true;}
static const risc_display_output_api_v1 host_display={.api_version=1,.struct_size=sizeof(host_display),.get_info=host_info,.acquire=host_frame,.release=host_frame_release,.submit=host_submit,.present_status=host_status};
static const risc_touch_api_v1 host_touch={1,sizeof(host_touch),NULL,host_subscribe,host_unsubscribe,host_touch_poll,host_touch_next,host_touch_snapshot};
static bool host_acquire(const char*name,uint32_t v,uint64_t instance,risc_runtime_capability_v1*g){assert(v==1&&!instance);if(!strcmp(name,"display.output"))g->api=&host_display;else if(!strcmp(name,"input.touch.raw"))g->api=&host_touch;else return false;renderer_grants++;return true;}
static bool host_release(risc_runtime_capability_v1*g){assert(renderer_grants&&g->api);renderer_grants--;g->api=NULL;return true;}
static const risc_runtime_api_v1 host_runtime={1,sizeof(host_runtime),host_health,host_yield,host_diag,host_launch,host_acquire,host_release};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1?&host_runtime:NULL;}
static void start(void){renderer_polls=0;renderer_limit=10000;renderer_script=false;renderer_touch.contact_count=0;setup();set_direct();assert(app_module_init()==0);fake_app=*t5_app_get_api(1);app=&fake_app;paper=paper_presentation_get();assert(paper&&pe_width()==480&&pe_height()==800);pe_first=pe_edit_first=pe_choice_first=pe_focus=pe_key_page=0;pe_focus_visible=pe_exit=pe_down=false;pe_clean=true;}
static void finish(void){close_dependencies();app_module_fini();assert(!renderer_grants&&!renderer_subs&&!renderer_frames);paper=NULL;}
static void frame(const char *name){const char *dir=getenv("POINTS_PAPER_FRAMES");if(!dir)return;char path[512];snprintf(path,sizeof(path),"%s/%s.pbm",dir,name);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P4\n800 480\n");assert(fwrite(pixels,1,sizeof(pixels),f)==sizeof(pixels));fclose(f);}
static void touch(unsigned contacts,int x,int y){renderer_touch.contact_count=contacts;renderer_touch.contacts[0]=(risc_touch_contact_v1){.id=1,.x=(uint16_t)x,.y=(uint16_t)y};t5_app_input_t in;assert(app->poll(&in,20));(void)pe_input(&in);}
int main(void){
 start();paper->begin();paper->text(32,32,100,"A",1,false,true);memcpy(first_pixels,pixels,sizeof(pixels));paper->begin();paper->text(32,32,100,"a",1|PAPER_TEXT_LITERAL,false,true);assert(memcmp(first_pixels,pixels,sizeof(pixels)));writer.saved=points_default_config();writer.meta=points_default_meta();time_format=PORTABLE_TIME_FORMAT_24;draw();frame("points-list");unsigned drawn=renderer_presents;pe_clean=false;draw();assert(renderer_presents==drawn);assert(!puts_count&&!meta_puts_count);
 touch(0,0,0);touch(1,150,600);touch(1,150,180);touch(0,0,0);assert(page==PAGE_LIST&&pe_first==6&&!puts_count);draw();frame("points-list-page-2");
 pe_first=0;pe_tap(100,150);assert(page==PAGE_EDIT&&!puts_count);draw();frame("points-edit");points_item original=writer.saved.points[selected];pe_tap(350,730);assert(page==PAGE_LIST);assert(puts_count==1);assert(!memcmp(&writer.saved.points[selected],&original,sizeof(original)));finish();
 start();draw();pe_tap(350,730);assert(page==PAGE_EDIT);pe_tap(240,730);assert(page==PAGE_EDIT&&!puts_count);pe_tap(150,240);assert(page==PAGE_TIME);uint8_t hour=draft.hour;pe_tap(350,280);assert(draft.hour==(hour+1)%24&&!puts_count);draw();frame("points-time");pe_tap(350,730);assert(page==PAGE_EDIT);pe_tap(100,730);assert(page==PAGE_LIST&&!puts_count);finish();
 start();nova_new_point();pe_edit_first=6;pe_tap(100,150);assert(page==PAGE_DAYS);draft.weekdays=0;for(unsigned i=0;i<7;i++){pe_choice_first=i/pe_rows()*pe_rows();pe_tap(100,PE_TOP+(int)(i%pe_rows())*PE_ROW+40);}assert(draft.weekdays==127);draw();frame("points-days");pe_tap(350,730);pe_tap(350,730);assert(page==PAGE_LIST&&puts_count==1);finish();
 start();nova_new_point();draft.duration_minutes=5;draft.notify_end=draft.warn3=1;page=PAGE_DURATION;pe_tap(100,280);assert(!draft.duration_minutes&&!draft.notify_end&&!draft.warn3);draft.duration_minutes=720;pe_tap(350,280);assert(draft.duration_minutes==720);finish();
 start();nova_new_point();draft.weekdays=127;put_error=RISC_KEY_VALUE_IO;pe_tap(350,730);assert(writer.uncertain&&puts_count==1);uint8_t exact[64];memcpy(exact,writer.pending,64);pe_tap(100,150);assert(writer.uncertain&&!memcmp(exact,writer.pending,64));put_error=0;pe_tap(350,730);assert(!writer.uncertain&&puts_count==2&&!memcmp(exact,last_put,64));finish();
 start();nova_new_point();custom_kind=POINTS_CUSTOM_1;custom_draft=writer.meta;memset(custom_draft.custom[0].name,0,sizeof(custom_draft.custom[0].name));page=PAGE_CUSTOM;pe_tap(100,170);assert(page==PAGE_CUSTOM_KEYBOARD);unsigned seen=0;
 for(unsigned key_page=0;key_page<(95+pe_key_count()-1)/pe_key_count();key_page++)for(unsigned key=0;key<pe_key_count()&&32+key_page*pe_key_count()+key<=126;key++){pe_key_text[0]=0;pe_key_page=key_page;pe_key(key);assert((unsigned char)pe_key_text[0]==32+key_page*pe_key_count()+key);seen++;}
 assert(seen==95);strcpy(pe_key_text,"ABCDEFGHIJKLM");pe_key_page=1;pe_key(13);assert(strlen(pe_key_text)==13);draw();frame("points-keyboard");pe_tap(100,730);assert(page==PAGE_CUSTOM&&!custom_draft.custom[0].name[0]&&!meta_puts_count);
 pe_tap(100,170);strcpy(pe_key_text,"WALK");pe_tap(350,730);assert(page==PAGE_CUSTOM&&!strcmp(custom_draft.custom[0].name,"WALK")&&!meta_puts_count);pe_tap(350,730);assert(page==PAGE_EDIT&&meta_puts_count==1&&!strcmp(writer.meta.custom[0].name,"WALK"));finish();
 start();nova_new_point();draft.weekdays=127;pe_tap(350,730);assert(puts_count==1);edit_slot(0);pe_edit_first=6;pe_tap(100,330);assert(nova_delete_confirm&&puts_count==1);pe_edit_action(1);assert(!nova_delete_confirm&&page==PAGE_TIME);page=PAGE_EDIT;pe_tap(100,330);assert(nova_delete_confirm&&puts_count==1);pe_tap(100,330);assert(page==PAGE_LIST&&puts_count==2&&!writer.saved.points[0].kind);finish();
 start();nova_new_point();draft.enabled=true;draft.weekdays=0;pe_tap(350,730);assert(!puts_count&&!writer.uncertain);draw();frame("points-invalid-save");finish();
 start();close_dependencies();unsigned before=renderer_presents;renderer_limit=100;app_main();assert(renderer_presents==before+1&&!puts_count&&!meta_puts_count);finish();
 start();close_dependencies();renderer_script=true;renderer_limit=100;app_main();assert(!puts_count&&!meta_puts_count&&page==PAGE_LIST&&pe_exit);finish();
 puts("Paper Points: real native renderer, paging, draft/Save/Cancel, 95 keyboard keys, exact retry, delete confirmation and cleanup pass");
}
