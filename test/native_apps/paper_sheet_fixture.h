/* Scripted input goes through the production adapter and app controller.
 * Time is deterministic provider time, not an e-paper latency measurement. */
static unsigned motion_case, motion_start, motion_opens, motion_closes;
static unsigned motion_frames, motion_distinct, motion_partial;
static bool motion_enabled, motion_navigation_sent;
static uint8_t motion_background[48000], motion_previous[48000];

static void motion_diagnostic(const char *text) {
 if(!motion_enabled)return;
 if(strstr(text,"name=quick-controls-open"))motion_opens++;
 if(strstr(text,"name=quick-controls-close"))motion_closes++;
}
static void motion_capture(void) {
 if(!motion_enabled)return;
 motion_frames++;
 if(memcmp(motion_previous,pixels,sizeof(pixels)))motion_distinct++;
 memcpy(motion_previous,pixels,sizeof(pixels));
 unsigned changed_columns=0;
 for(unsigned x=0;x<800;x++) {
  bool changed=false;
  for(unsigned y=0;y<480;y++) {
   unsigned byte=y*100+x/8,bit=1u<<(7-x%8);
   if((motion_background[byte]^pixels[byte])&bit){changed=true;break;}
  }
  if(changed)changed_columns++;
 }
 if(changed_columns>0&&changed_columns<600)motion_partial++;
}
static bool motion_snapshot(risc_touch_snapshot_v1 *out) {
 unsigned now=ticks-motion_start;
 assert(now<9000);
 if(motion_case==107&&now>=230)return false;
 *out=(risc_touch_snapshot_v1){.width=480,.height=800};
 if((motion_case==103||motion_case==104||motion_case==105)&&now>=230&&now<340) {
  if(motion_case==103){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=600,.y=210};return true;}
  out->contact_count=motion_case==105?2:1;
  out->contacts[0]=(risc_touch_contact_v1){.id=2,.x=240,.y=210};
  if(out->contact_count==2)out->contacts[1]=(risc_touch_contact_v1){.id=3,.x=250,.y=220};
  return true;
 }
 if(motion_case==102&&now>=220&&now<340){out->buttons=RISC_TOUCH_BUTTON_PRIMARY;return true;}
 unsigned cycle=now>=2000?now-2000:now;
 bool down=false;unsigned y=20;
 if(cycle>=100&&cycle<340){down=true;y=cycle<180?20:cycle<260?180:400;}
 if(cycle>=1200&&cycle<1320&&(now>=2000||motion_case==100)){
  down=true;y=cycle<1260?670:500;
 }
 if(down){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=240,.y=(uint16_t)y};}
 return true;
}
static void motion_navigation(risc_input_navigation_frame_v1 *out) {
 if(!motion_enabled||motion_navigation_sent||ticks-motion_start<220)return;
 if(motion_case==101||motion_case==106) {
  out->buttons=out->pressed=motion_case==101?RISC_NAV_BACK:RISC_NAV_HOME;
  motion_navigation_sent=true;
 }
}
static void motion_begin(unsigned scenario) {
 motion_case=scenario;motion_start=ticks;motion_enabled=true;
 memcpy(motion_background,pixels,sizeof(pixels));
 memcpy(motion_previous,pixels,sizeof(pixels));
}
static void motion_check(void) {
 assert(motion_opens==2&&motion_closes==2);
 assert(motion_frames>=6&&motion_distinct>=5&&motion_partial>=2);
 assert(!memcmp(motion_background,pixels,sizeof(pixels)));
 assert(!puts_count&&!launches&&!frames&&!native_live&&!retained);
 printf("Sheet scenario%u: opens%u closes%u frames%u distinct%u partial%u exact-restore\n",
        motion_case,motion_opens,motion_closes,motion_frames,motion_distinct,motion_partial);
 motion_enabled=false;
}

static void motion_transform_touch(risc_touch_snapshot_v1 *out) {
 if(!getenv("TEST_PAPER_FLIP"))return;
 for(unsigned n=0;n<out->contact_count;n++)if(out->contacts[n].x<480&&out->contacts[n].y<800){out->contacts[n].x=479-out->contacts[n].x;out->contacts[n].y=799-out->contacts[n].y;}
}
