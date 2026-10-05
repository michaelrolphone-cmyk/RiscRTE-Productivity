#pragma once
/* Reuse the portable Watch Wi-Fi keyboard, not the NOVA eight-key pager.
 * Source: RiscRTE-System-Apps 7d6cdc052e5bc286ff017baa409049471d0540e7,
 * lib/PortableApps/src/wifi_view.inc and Apps/wifi_settings_portable.inc
 * (the non-PORTABLE_NOVA_UI branches). The same 8x4 ASCII grid, three pages,
 * ABC/#, DELETE, DONE, typography and hit cells are rendered through NOVA's
 * app-local drawing functions. This adaptation does not add a Runtime import
 * or depend on the Reader-only firmware KeyboardEntryActivity/relaunch API.
 * Keep the entry separate until DONE, as the Wi-Fi editor does. */
#define P7_WATCH_KEY_CHARACTERS 32u
#define P7_WATCH_KEY_PAGES 3u
#define P7_WATCH_KEY_PAGE 32u
#define P7_WATCH_KEY_DELETE 33u
#define P7_WATCH_KEY_DONE 34u
#define P7_WATCH_KEY_COUNT 35u
static char p7_key_text[POINTS_CUSTOM_NAME_MAX+1];

static inline unsigned p7_watch_key_character(unsigned key_page,unsigned key) {
    if(key_page>=P7_WATCH_KEY_PAGES || key>=P7_WATCH_KEY_CHARACTERS)return 0;
    unsigned ch=32+key_page*P7_WATCH_KEY_CHARACTERS+key;
    return ch<=126?ch:0;
}
static inline int p7_watch_key_hit(int x,int y) {
    if(x<0 || x>=240 || y<0 || y>=240)return -1;
    if(x>=12 && x<228 && y>=76 && y<172)return (y-76)/24*8+(x-12)/27;
    if(y>=178 && y<207 && x>=12 && x<228)return 32+(x-12)/72;
    return -1;
}
static inline void p7_watch_key_button(int x,int y,int w,int h,const char *text,bool selected) {
    portable_nova_fill(x,y,w,h,selected?NOVA_DIM:NOVA_LINE);
    portable_nova_fill(x,y,w,1,selected?NOVA_CYAN:NOVA_DIM);
    portable_nova_text(2,x+5,y+7,w-10,text,NOVA_CYAN);
}
static inline void p7_key_begin(void) {
    unsigned ci=custom_index(custom_kind);if(ci>=POINTS_CUSTOM_COUNT)return;
    memcpy(p7_key_text,custom_draft.custom[ci].name,sizeof(p7_key_text));
    p7_key_text[POINTS_CUSTOM_NAME_MAX]=0;
    custom_key_page=2;custom_key_choice=0;page=PAGE_CUSTOM_KEYBOARD;
    notice="Name draft only - DONE to keep";
}
static inline void p7_key_cancel(void) {
    memset(p7_key_text,0,sizeof(p7_key_text));
    custom_key_choice=0;page=PAGE_CUSTOM;notice="Name edit discarded";
}
