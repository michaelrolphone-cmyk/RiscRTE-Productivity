/* Exercise the real Points controller and renderer with the existing fixtures. */
#ifndef PORTABLE_NOVA_UI
#error The Watch keyboard fixture requires PORTABLE_NOVA_UI
#endif
#define main points_existing_fixture_main
#include "points_in_time_test.c"
#undef main

static void keyboard_setup(const char *name) {
    setup();app=&fake_app;writer=(points_writer){.loaded=true};
    ready=service_valid=clock_valid=true;custom_kind=POINTS_CUSTOM_1;
    custom_draft=(points_meta){.revision=7};custom_draft.custom[0].color=6;
    snprintf(custom_draft.custom[0].name,sizeof(custom_draft.custom[0].name),"%s",name);
    strcpy(custom_draft.custom[1].name,"Other");custom_draft.custom[1].color=2;
    page=PAGE_CUSTOM;p7_key_begin();
}
static void type_character(unsigned ch) {
    assert(ch>=32 && ch<=126);
    custom_key_page=(ch-32)/P7_WATCH_KEY_CHARACTERS;
    p7_key_activate((ch-32)%P7_WATCH_KEY_CHARACTERS);
}
static void keyboard_storage_test(void) {
    setup();
    points_config original={.revision=3,.created=1000};
    for(unsigned i=0;i<POINTS_MAX;i++)original.points[i]=(points_item){
        .kind=POINTS_CUSTOM_1,.enabled=(uint8_t)(i&1),.mode=(uint8_t)(i%4),
        .weekdays=(uint8_t)(1u<<(i%7)),.hour=(uint8_t)(i*3),.minute=(uint8_t)(i*7),
        .duration_minutes=(uint16_t)(3+i*37),.notify_end=(uint8_t)(i&1),.warn3=(uint8_t)((i>>1)&1)};
    points_config_encode(&original,stored);stored_size=POINTS_RECORD_SIZE;
    points_meta metadata={.revision=2};
    strcpy(metadata.custom[0].name,"Original");metadata.custom[0].color=6;
    strcpy(metadata.custom[1].name,"Other");metadata.custom[1].color=2;
    points_meta_encode(&metadata,stored_meta);stored_meta_size=POINTS_RECORD_SIZE;
    uint8_t initial_points[POINTS_RECORD_SIZE],initial_meta[POINTS_RECORD_SIZE];
    memcpy(initial_points,stored,sizeof(initial_points));memcpy(initial_meta,stored_meta,sizeof(initial_meta));
    set_direct();edit_slot(0);p7_custom_open(POINTS_CUSTOM_1,false);p7_key_begin();
    for(unsigned i=0;i<POINTS_CUSTOM_NAME_MAX;i++)p7_key_activate(P7_WATCH_KEY_DELETE);
    type_character('N');type_character('e');type_character('w');p7_key_activate(P7_WATCH_KEY_DONE);
    custom_draft.custom[0].color=4;
    assert(!puts_count && !meta_puts_count);
    nova7_tap(65,220); /* Custom Back discards name/color without writing. */
    assert(page==PAGE_TYPE && !memcmp(stored,initial_points,sizeof(initial_points)));
    assert(!memcmp(stored_meta,initial_meta,sizeof(initial_meta)) && !meta_puts_count);
    p7_custom_open(POINTS_CUSTOM_1,false);assert(!strcmp(custom_draft.custom[0].name,"Original"));
    assert(custom_draft.custom[0].color==6);p7_key_begin();
    for(unsigned i=0;i<POINTS_CUSTOM_NAME_MAX;i++)p7_key_activate(P7_WATCH_KEY_DELETE);
    type_character('N');type_character('e');type_character('w');p7_key_activate(P7_WATCH_KEY_DONE);
    custom_draft.custom[0].color=4;nova7_tap(175,220); /* Explicit custom SAVE. */
    assert(page==PAGE_EDIT && meta_puts_count==1 && !puts_count);
    points_meta persisted;assert(points_meta_decode(&persisted,stored_meta,stored_meta_size));
    assert(persisted.revision==3 && !strcmp(persisted.custom[0].name,"New") && persisted.custom[0].color==4);
    assert(!memcmp(&persisted.custom[1],&metadata.custom[1],sizeof(metadata.custom[1])));
    assert(!memcmp(stored,initial_points,sizeof(initial_points))); /* All eight records/flags untouched. */
    assert(!memcmp(&writer.saved,&original,sizeof(original)));
    draft=(points_item){.kind=POINTS_CUSTOM_1,.enabled=1,.mode=3,.weekdays=127,
        .hour=23,.minute=59,.duration_minutes=720,.notify_end=1,.warn3=1};
    nova7_tap(65,220);assert(page==PAGE_LIST && !puts_count); /* Explicit editor Cancel. */
    assert(!memcmp(stored,initial_points,sizeof(initial_points)));
    edit_slot(0);draft=(points_item){.kind=POINTS_CUSTOM_1,.enabled=1,.mode=3,.weekdays=127,
        .hour=23,.minute=59,.duration_minutes=720,.notify_end=1,.warn3=1};
    points_item desired=draft;nova7_tap(175,220);assert(page==PAGE_LIST && puts_count==1 && meta_puts_count==1);
    points_config decoded;assert(points_config_decode(&decoded,stored,stored_size));
    assert(!memcmp(&decoded.points[0],&desired,sizeof(desired)));
    for(unsigned i=1;i<POINTS_MAX;i++)assert(!memcmp(&decoded.points[i],&original.points[i],sizeof(points_item)));
    assert(points_meta_decode(&persisted,stored_meta,stored_meta_size) && !strcmp(persisted.custom[0].name,"New"));
    close_dependencies();
}
int main(void) {
    unsigned seen[127]={0},count=0;
    for(unsigned key_page=0;key_page<P7_WATCH_KEY_PAGES;key_page++)
        for(unsigned key=0;key<P7_WATCH_KEY_CHARACTERS;key++) {
            unsigned ch=p7_watch_key_character(key_page,key);
            if(ch){assert(ch>=32 && ch<=126);seen[ch]++;count++;}
        }
    assert(count==95);
    for(unsigned ch=32;ch<=126;ch++)assert(seen[ch]==1);
    assert(!p7_watch_key_character(2,31)); /* ASCII DEL stays blank and inert. */
    assert(!p7_watch_key_character(3,0) && !p7_watch_key_character(0,32));
    for(unsigned key=0;key<32;key++) {
        int x=12+(int)(key%8)*27,y=76+(int)(key/8)*24;
        assert(p7_watch_key_hit(x,y)==(int)key);
        assert(p7_watch_key_hit(x+26,y+23)==(int)key);
    }
    assert(p7_watch_key_hit(11,76)==-1 && p7_watch_key_hit(228,76)==-1);
    assert(p7_watch_key_hit(12,75)==-1 && p7_watch_key_hit(12,172)==-1);
    assert(p7_watch_key_hit(12,178)==32 && p7_watch_key_hit(83,206)==32);
    assert(p7_watch_key_hit(84,178)==33 && p7_watch_key_hit(155,206)==33);
    assert(p7_watch_key_hit(156,178)==34 && p7_watch_key_hit(227,206)==34);
    assert(p7_watch_key_hit(156,207)==-1 && p7_watch_key_hit(30,20)==-1);
    assert(p7_watch_key_hit(-1,100)==-1 && p7_watch_key_hit(240,100)==-1);

    keyboard_setup("Seed");
    assert(page==PAGE_CUSTOM_KEYBOARD && custom_key_page==2 && custom_key_choice==0);
    assert(!strcmp(p7_key_text,"Seed"));
    type_character(' ');type_character('a');type_character('A');type_character('!');
    assert(!strcmp(p7_key_text,"Seed aA!"));
    assert(!strcmp(custom_draft.custom[0].name,"Seed") && !meta_puts_count && !puts_count);
    p7_key_activate(P7_WATCH_KEY_DELETE);assert(!strcmp(p7_key_text,"Seed aA"));
    p7_key_cancel();assert(page==PAGE_CUSTOM && !strcmp(custom_draft.custom[0].name,"Seed"));
    p7_key_begin();assert(!strcmp(p7_key_text,"Seed"));
    type_character(' ');type_character('z');p7_key_activate(P7_WATCH_KEY_DONE);
    assert(page==PAGE_CUSTOM && !strcmp(custom_draft.custom[0].name,"Seed z"));
    assert(custom_draft.revision==7 && custom_draft.custom[0].color==6);
    assert(!strcmp(custom_draft.custom[1].name,"Other") && custom_draft.custom[1].color==2);
    assert(!meta_puts_count && !puts_count); /* DONE only updates the custom-type draft. */

    keyboard_setup("");
    for(unsigned i=0;i<POINTS_CUSTOM_NAME_MAX;i++)type_character('A'+i);
    assert(!strcmp(p7_key_text,"ABCDEFGHIJKL") && !p7_key_text[POINTS_CUSTOM_NAME_MAX]);
    for(unsigned i=0;i<50;i++)type_character('x');
    assert(!strcmp(p7_key_text,"ABCDEFGHIJKL"));
    p7_key_activate(P7_WATCH_KEY_DELETE);type_character('z');
    assert(!strcmp(p7_key_text,"ABCDEFGHIJKz"));
    p7_key_activate(P7_WATCH_KEY_DONE);assert(!strcmp(custom_draft.custom[0].name,"ABCDEFGHIJKz"));
    p7_key_begin();for(unsigned i=0;i<50;i++)p7_key_activate(P7_WATCH_KEY_DELETE);
    assert(!p7_key_text[0]);p7_key_activate(P7_WATCH_KEY_DONE);assert(!custom_draft.custom[0].name[0]);

    keyboard_setup("");
    p7_key_activate(31);assert(!p7_key_text[0]);
    p7_key_activate(35);assert(!p7_key_text[0] && page==PAGE_CUSTOM_KEYBOARD);
    p7_key_activate(P7_WATCH_KEY_PAGE);assert(custom_key_page==0);
    p7_key_activate(0);assert(!strcmp(p7_key_text," "));
    p7_key_activate(P7_WATCH_KEY_PAGE);assert(custom_key_page==1);
    p7_key_activate(1);assert(!strcmp(p7_key_text," A"));
    p7_key_activate(P7_WATCH_KEY_PAGE);assert(custom_key_page==2);
    p7_key_activate(1);assert(!strcmp(p7_key_text," Aa"));
    p7_text_calls=p7_fill_calls=p7_round_calls=p7_button_calls=0;p7_keyboard();
    assert(strstr(rendered,"CUSTOM TYPE") && strstr(rendered," Aa"));
    assert(strstr(rendered,"ABC/#") && strstr(rendered,"DELETE") && strstr(rendered,"DONE"));
    assert(strstr(rendered,"3 / 12 CHARACTERS") && !strstr(rendered,"PREV") && !strstr(rendered,"NEXT"));
    assert(p7_fill_calls==71 && p7_text_calls==39 && !p7_round_calls && !p7_button_calls);

    /* Production tap, button and Back wiring, including reopening after cancel. */
    keyboard_setup("Keep");p7_key_cancel();nova7_tap(60,110);
    assert(page==PAGE_CUSTOM_KEYBOARD && !strcmp(p7_key_text,"Keep"));
    nova7_tap(40,80);assert(!strcmp(p7_key_text,"Keepa"));
    assert(!on_back() && page==PAGE_CUSTOM && !strcmp(custom_draft.custom[0].name,"Keep"));
    nova7_tap(60,110);assert(!strcmp(p7_key_text,"Keep"));
    custom_key_choice=0;assert(nova7_buttons(T5_APP_BUTTON_LEFT) && custom_key_choice==34);
    assert(nova7_buttons(T5_APP_BUTTON_RIGHT) && custom_key_choice==0);
    assert(nova7_buttons(T5_APP_BUTTON_UP) && custom_key_choice==34);
    assert(nova7_buttons(T5_APP_BUTTON_DOWN) && custom_key_choice==0);
    custom_key_choice=1;assert(nova7_buttons(T5_APP_BUTTON_CONFIRM));
    assert(!strcmp(p7_key_text,"Keepa"));
    nova7_tap(190,190);assert(page==PAGE_CUSTOM && !strcmp(custom_draft.custom[0].name,"Keepa"));
    assert(!meta_puts_count && !puts_count);
    keyboard_storage_test();
    puts("Points Watch keyboard: 95 ASCII keys, original grid/hits, 12-byte bound, draft/Done/Back and input wiring passed");
    return 0;
}
