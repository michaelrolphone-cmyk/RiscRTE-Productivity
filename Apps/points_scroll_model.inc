#include "productivity_scroll.h"
static productivity_scroll pe_scroll;
static struct {
 points_config config;points_meta meta;points_item draft;points_meta custom;
 unsigned page,selected,key_page,kind,format;bool ready,loaded,uncertain,meta_uncertain,delete_confirm;
 char text[POINTS_CUSTOM_NAME_MAX+1];
} pe_scroll_state;
static void pe_scroll_prepare(void) {
 __typeof__(pe_scroll_state) state={0};
 state.config=writer.saved;state.meta=writer.meta;state.draft=draft;state.custom=custom_draft;
 state.page=page;state.selected=selected;state.key_page=pe_key_page;state.kind=custom_kind;state.format=time_format;
 state.delete_confirm=nova_delete_confirm;state.ready=ready;state.loaded=writer.loaded;state.uncertain=writer.uncertain;state.meta_uncertain=writer.meta_uncertain;
 memcpy(state.text,pe_key_text,sizeof(state.text));
 if(!pe_scroll.generation||memcmp(&state,&pe_scroll_state,sizeof(state))) {
  productivity_scroll_changed(&pe_scroll,!pe_scroll.generation||state.page!=pe_scroll_state.page);
  pe_scroll_state=state;
 }
 unsigned n=0,order[POINTS_MAX];
 if(ready&&writer.loaded&&!writer.uncertain&&!writer.meta_uncertain) {
  if(page==PAGE_LIST)n=nova_configured(order);else if(page==PAGE_EDIT)n=9;
  else if(page==PAGE_TYPE)n=nova_type_choices(order);else if(page==PAGE_MODE)n=4;else if(page==PAGE_DAYS)n=10;
 }
 portable_scroll_configure(&pe_scroll.motion,(portable_scroll_viewport){32,PE_TOP,pe_width()-64,pe_height()-168-PE_TOP},PE_ROW,(int)n);
 unsigned first=(unsigned)portable_scroll_offset(&pe_scroll.motion)/PE_ROW;
 pe_first=pe_edit_first=pe_choice_first=first;
}
static unsigned pe_render_rows(void){return (unsigned)pe_scroll.motion.view.height/PE_ROW+2;}
static int pe_row_y(unsigned shown){return PE_TOP+(int)shown*PE_ROW-portable_scroll_offset(&pe_scroll.motion)%PE_ROW;}
static void pe_clip(bool enabled){portable_paper_scroll_clip(enabled?&pe_scroll.motion.view:NULL);}
