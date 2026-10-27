/* Ledger-only fixture: any accidental presentation execution is fatal. */
#define PORTABLE_RF_NATIVE_CANVAS
#define PORTABLE_NATIVE_SCENE
#include "PortableNovaUi.h"
#include "PortableNativeScene.h"
#include <stdlib.h>
#include <stdio.h>
#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
_Noreturn static void forbidden_render(void) { fputs("Unexpected renderer/scene call in ledger-only fixture\n",stderr);abort(); }
void portable_nova_begin(void) { forbidden_render(); }
void portable_nova_text(unsigned face,int x,int y,int width,const char *text,uint32_t color) { forbidden_render(); }
void portable_nova_center(unsigned face,int x,int y,int width,const char *text,uint32_t color) { forbidden_render(); }
void portable_nova_right(unsigned face,int x,int y,int width,const char *text,uint32_t color) { forbidden_render(); }
int portable_nova_measure(unsigned face,const char *text) { forbidden_render(); }
void portable_nova_fill(int x,int y,int width,int height,uint32_t color) { forbidden_render(); }
void portable_nova_round(int x,int y,int width,int height,int radius,uint32_t color) { forbidden_render(); }
void portable_nova_button(int x,int y,int width,int height,const char *text,bool selected) { forbidden_render(); }
void portable_nova_row(int x,int y,int width,int height,const char *label,const char *value,bool selected) { forbidden_render(); }
void portable_nova_header(const char *title) { forbidden_render(); }
void portable_nova_rule(int x,int y,int width) { forbidden_render(); }
bool portable_nova_hit(int x,int y,int left,int top,int width,int height) { forbidden_render(); }
bool portable_nova_wrap(unsigned face,int x,int y,int width,int line_height,unsigned max_lines,const char *text,uint32_t color) { forbidden_render(); }
void portable_nova_native_fill(int x,int y,int width,int height,uint32_t color) { forbidden_render(); }
void portable_nova_native_round(int x,int y,int width,int height,int radius,uint32_t color) { forbidden_render(); }
void portable_nova_native_text(unsigned face,int x,int y,int width,const char *text,uint32_t color) { forbidden_render(); }
bool portable_native_scene_available(void) { forbidden_render(); }
void portable_native_scene_begin(void) { forbidden_render(); }
void portable_native_scene_layout(uint64_t identity) { forbidden_render(); }
void portable_native_scene_present(bool full) { forbidden_render(); }
bool portable_native_scene_ready(uint64_t *generation,uint64_t *sequence) { forbidden_render(); }
bool portable_native_scene_repaint(void) { forbidden_render(); }
bool portable_native_scene_contact_read(portable_native_scene_contact *out) { forbidden_render(); }
void portable_native_scene_back(void) { forbidden_render(); }

bool portable_paper_frame_ready(void) { forbidden_render(); }
