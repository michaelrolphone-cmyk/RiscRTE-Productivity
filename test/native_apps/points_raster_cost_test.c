/* Actual Points screens, selected adapter, immutable replay versus immediate
 * pixels. Count producer work so a fast host cannot hide recorder overflow. */
#define main points_legacy_main
#include "points_catalog_app_test.c"
#undef main
#include <time.h>
static bool recording;
static unsigned rectangles,deferred;
#ifdef PORTABLE_RASTER_SNAPSHOT
bool __real_portable_raster_defer(portable_raster_draw,const void *,size_t,int,int);
#endif
bool __wrap_portable_raster_defer(portable_raster_draw draw,const void *p,size_t n,int top,int bottom) {
#ifdef PORTABLE_RASTER_SNAPSHOT
    bool stored=__real_portable_raster_defer(draw,p,n,top,bottom);
    if(recording&&stored)deferred++;
    return stored;
#else
    (void)draw;(void)p;(void)n;(void)top;(void)bottom;return false;
#endif
}
static void count_fill(int x,int y,int w,int h,bool ink){if(recording)rectangles++;render_app->fill_rect(x,y,w,h,ink);}
static void scene(const char *name) {
    settle();rectangles=deferred=0;dirty=true;recording=true;
    clock_t before=clock();pc_draw();double record_ms=1000.0*(clock()-before)/CLOCKS_PER_SEC;recording=false;
    /* Later logical/model state must not alter the sealed image. */
    bool clip=pcp_clip_content,literal=pcp_case_sensitive;
    pcp_clip_content=!clip;pcp_case_sensitive=!literal;
    assert(render_app->frame_drain());pcp_clip_content=clip;pcp_case_sensitive=literal;
    frame(name);
#ifdef PORTABLE_RASTER_SNAPSHOT
    assert(deferred>0&&rectangles+deferred<1024);
#endif
    printf("{\"scene\":\"%s\",\"recorded_rectangles\":%u,\"semantic_commands\":%u,\"host_record_ms\":%.3f}\n",name,rectangles,deferred,record_ms);
}
int main(void) {
    start();fixture_app.fill_rect=count_fill;scene("list");
    assert(points_editor_begin_event(&editor,0));pc_page(PC_EDIT);scene("edit");
    pcp_edit_offset=248;scene("edit-scrolled");pc_page(PC_DAYS);scene("days");
    pc_page(PC_TIME);scene("time");pc_page(PC_TYPES);scene("types");
    finish();return 0;
}
