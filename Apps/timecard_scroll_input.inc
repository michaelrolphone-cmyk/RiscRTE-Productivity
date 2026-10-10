static void tcp_paper_input(const t5_app_input_t *input){
 tcs_prepare();
 if(input->exit_requested && !(input->buttons&T5_APP_BUTTON_BACK)){productivity_scroll_interrupt(&tcs);tcp_external_exit=true;return;}
 if(input->buttons&T5_APP_BUTTON_BACK){productivity_scroll_interrupt(&tcs);tcp_reset_gesture();tcp_back();return;}
 portable_touch_sample sample;uint32_t now;portable_paper_scroll_sample(&sample,&now);
 sample.x=(uint16_t)((unsigned)sample.x*480/(unsigned)app->screen_width());
 sample.y=(uint16_t)((unsigned)sample.y*800/(unsigned)app->screen_height());
 if(input->buttons) {
  productivity_scroll_interrupt(&tcs);
  if(input->buttons&(T5_APP_BUTTON_UP|T5_APP_BUTTON_LEFT)){
   if(tcp_editor){tcp_key_page=(tcp_key_page+4)%5;tcp_dirty=true;}else tpp_page(-1);
  }else if(input->buttons&(T5_APP_BUTTON_DOWN|T5_APP_BUTTON_RIGHT)){
   if(tcp_editor){tcp_key_page=(tcp_key_page+1)%5;tcp_dirty=true;}else tpp_page(1);
  }else if(input->buttons&T5_APP_BUTTON_CONFIRM){
   if(tcp_editor)tcp_editor_done();else if(!store_ready||productivity_scroll_confirm(&tcs,selected))tcp_activate();tcp_dirty=true;
  }
 }else {
  unsigned result=productivity_scroll_input(&tcs,&sample,now);
  if(result&PORTABLE_SCROLL_BACK){productivity_scroll_interrupt(&tcs);tcp_back();}
  else if(result&PORTABLE_SCROLL_TAP) {
   if(tcs.hit_row>=0){selected=tcs.hit_row;tcp_activate();}
   else tpp_tap(tcs.hit_x,tcs.hit_y);
   tcp_dirty=true;
  }
 }
 tcs_prepare();if(tcs.dirty)tcp_dirty=true;
}
