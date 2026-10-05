/* Exercise the production UI and contact state with the real font renderer. */
#define main picker_fixture_main
#include "points_picker_renderer_test.c"
#undef main
static void gui_drag(int x,int start,int end) {
    springboard_contact c=contact(x,start,true,false);nova7_contact_update(&c);
    for(int y=start;y>end;y--){c=contact(x,y,false,false);nova7_contact_update(&c);}
    c=contact(x,end,false,false);nova7_contact_update(&c);
    unsigned *v=p7_scroll_value(),before=v?*v:0;
    c=contact(x,end,false,true);nova7_contact_update(&c);
    assert(!v||*v==before);assert(p7_contact_moved);
}
static void check_chrome(unsigned p) {
    init(p);draft.kind=POINTS_LUNCH;draft.duration_minutes=45;selected=0;
    for(unsigned i=0;i<8;i++)writer.saved.points[i]=(points_item){.kind=POINTS_LUNCH,.enabled=1,.hour=(uint8_t)i,.minute=11};
    uint16_t top[82][240],bottom[38][240];
    unsigned *v=p7_scroll_value();assert(v);*v=0;
    if(p==PAGE_LIST)p7_list();else if(p==PAGE_EDIT)p7_edit();else if(p==PAGE_TYPE)p7_type();else p7_mode();
    for(unsigned y=0;y<82;y++)memcpy(top[y],picker_pixels+y*244,sizeof(top[y]));
    for(unsigned y=202;y<240;y++)memcpy(bottom[y-202],picker_pixels+y*244,sizeof(bottom[0]));
    for(unsigned offset=1;offset<=p7_scroll_max();offset++) {
        *v=offset;if(p==PAGE_LIST)p7_list();else if(p==PAGE_EDIT)p7_edit();else if(p==PAGE_TYPE)p7_type();else p7_mode();
        for(unsigned y=0;y<82;y++)assert(!memcmp(top[y],picker_pixels+y*244,sizeof(top[y])));
        if(p!=PAGE_LIST)for(unsigned y=202;y<240;y++)assert(!memcmp(bottom[y-202],picker_pixels+y*244,sizeof(bottom[0])));
        guard();
    }
    *v=0;gui_drag(110,180,153);assert(*v==27);
    unsigned before=*v;springboard_contact c=contact(110,150,false,true);nova7_contact_update(&c);assert(*v==before);
}
int main(int argc,char **argv) {
    const char *directory=argc>1?argv[1]:NULL;
    assert(portable_nova_measure(3,"OFF")<=20&&portable_nova_measure(3,"ON")<=20);
    assert(portable_nova_measure(3,"EVERY DAY")<=54&&portable_nova_measure(3,"WEEKEND")<=54);
    check_chrome(PAGE_LIST);check_chrome(PAGE_EDIT);check_chrome(PAGE_TYPE);check_chrome(PAGE_MODE);
    init(PAGE_CUSTOM);custom_kind=POINTS_CUSTOM_1;custom_draft=(points_meta){0};strcpy(custom_draft.custom[0].name,"WALK");
    for(unsigned i=0;i<8;i++){page=PAGE_CUSTOM;unsigned old_page=page;nova7_tap(43+(int)(i%4)*42+10,141+(int)(i/4)*32+10);assert(page==old_page&&custom_draft.custom[0].color==i);}
    p7_custom();save_frame(directory,"custom-colors-separated");nova7_tap(60,220);assert(page==PAGE_TYPE);
    init(PAGE_EDIT);draft.kind=POINTS_LUNCH;draft.duration_minutes=45;nova_edit_scroll=120;
    nova7_tap(200,100);assert(draft.notify_end&&!draft.warn3);nova7_tap(200,140);assert(draft.notify_end&&draft.warn3);
    nova7_tap(200,100);assert(!draft.notify_end&&draft.warn3);
    nova_edit_scroll=0;p7_edit();save_frame(directory,"add-point");
    time_format=PORTABLE_TIME_FORMAT_12;draft.hour=0;p7_edit();save_frame(directory,"add-point-midnight-12h");
    draft.hour=12;p7_edit();save_frame(directory,"add-point-noon-12h");
    /* Scroll until the entire day block is visible, then hit every day/preset. */
    nova_edit_scroll=240;draft.weekdays=0;
    for(unsigned i=0;i<7;i++){nova7_tap(24+(int)i*28+10,110);assert(draft.weekdays==((1u<<(i+1))-1));}
    nova7_tap(110,140);assert(draft.weekdays==62);nova7_tap(177,140);assert(draft.weekdays==65);nova7_tap(40,140);assert(draft.weekdays==127);
    p7_edit();save_frame(directory,"days-fully-visible");
    nova_edit_scroll=175;p7_edit();save_frame(directory,"edit-scrolled-notifications");
    nova7_tap(60,220);assert(page==PAGE_LIST); /* Explicit Cancel never saves. */
    init(PAGE_LIST);time_format=PORTABLE_TIME_FORMAT_12;
    for(unsigned i=0;i<8;i++)writer.saved.points[i]=(points_item){.kind=(uint8_t)(i%5+1),.enabled=1,.hour=(uint8_t)(i+8),.minute=11,.weekdays=62};
    nova_list_scroll=0;p7_list();save_frame(directory,"list-12h");
    nova_list_scroll=37;p7_list();save_frame(directory,"list-drag-37px");
    page=PAGE_TYPE;nova_type_scroll=53;p7_type();save_frame(directory,"types-drag-53px");
    puts("Points GUI: every scroll pixel preserves header/footer; drag release continuity; custom colors and Back separation; independent notification switches and Cancel passed");
    return 0;
}
