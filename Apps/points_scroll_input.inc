static void pe_scroll_activate(unsigned row,int x) {
 if(row>=(unsigned)pe_scroll.motion.count)return;
 if(page==PAGE_LIST){unsigned order[POINTS_MAX],n=nova_configured(order);if(row<n){edit_slot(order[row]);nova_delete_confirm=false;}}
 else if(page==PAGE_EDIT)pe_edit_action(row);
 else if(page==PAGE_TYPE)pe_choose_type(row,x>=pe_width()-112);
 else if(page==PAGE_MODE)select_mode(row);
 else if(page==PAGE_DAYS){if(row<7)draft.weekdays^=(uint8_t)(1u<<row);else draft.weekdays=row==7?127:row==8?62:65;}
}
static bool pe_input(const t5_app_input_t *input) {
 pe_scroll_prepare();unsigned old_page=page;
 portable_touch_sample sample;uint32_t now;portable_paper_scroll_sample(&sample,&now);
 if(input->buttons||input->exit_requested) {
  productivity_scroll_interrupt(&pe_scroll);
  if(input->buttons&(T5_APP_BUTTON_UP|T5_APP_BUTTON_LEFT|T5_APP_BUTTON_DOWN|T5_APP_BUTTON_RIGHT)) {
   bool next=!!(input->buttons&(T5_APP_BUTTON_DOWN|T5_APP_BUTTON_RIGHT));unsigned n=(unsigned)pe_scroll.motion.count;
   if(n){pe_focus=next?(pe_focus+1)%n:(pe_focus+n-1)%n;pe_focus_visible=true;portable_scroll_reveal(&pe_scroll.motion,(int)pe_focus);}
   else if(page==PAGE_TIME||page==PAGE_DURATION)pe_tap(next?pe_width()-80:80,input->buttons&(T5_APP_BUTTON_UP|T5_APP_BUTTON_DOWN)?400:280);
  }
  if(input->buttons&T5_APP_BUTTON_CONFIRM) {
   if(pe_scroll.motion.count) {
    if(productivity_scroll_confirm(&pe_scroll,(int)pe_focus))pe_scroll_activate(pe_focus,80);
    pe_focus_visible=true;
   }else if(page==PAGE_LIST)nova_new_point();
  }
 } else {
  unsigned result=productivity_scroll_input(&pe_scroll,&sample,now);
  if(result&PORTABLE_SCROLL_BACK){pe_back();pe_scroll.dirty=true;}
  else if(result&PORTABLE_SCROLL_TAP) {
   if(pe_scroll.hit_row>=0)pe_scroll_activate((unsigned)pe_scroll.hit_row,pe_scroll.hit_x);
   else pe_tap(pe_scroll.hit_x,pe_scroll.hit_y);
   pe_scroll.dirty=true;
  }
 }
 if(page!=old_page)pe_reset_page();
 pe_scroll_prepare();return pe_scroll.dirty&&portable_paper_scroll_available();
}
