/* Provider controls for the actual native adapter and app controller. */
static bool sc_enabled,sc_busy,sc_down,sc_invalid,sc_up,sc_replace;
static unsigned sc_x=120,sc_y=200;
static bool sc_snapshot(risc_touch_snapshot_v1 *s) {
 *s=(risc_touch_snapshot_v1){.width=480,.height=800};
 if(sc_invalid){s->contact_count=2;return true;}
 if(sc_down){s->contact_count=1;s->contacts[0]=(risc_touch_contact_v1){.id=sc_replace?2:1,.x=sc_x,.y=sc_y};
  if(getenv("TEST_PAPER_FLIP")){s->contacts[0].x=479-s->contacts[0].x;s->contacts[0].y=799-s->contacts[0].y;}}
 return true;
}
static int sc_next(risc_touch_event_v1 *event) {
 if(!sc_up)return 0;
 sc_up=false;
 *event=(risc_touch_event_v1){.id=1,.kind=RISC_TOUCH_EVENT_UP,.x=sc_x,.y=sc_y};
 if(getenv("TEST_PAPER_FLIP")){event->x=479-event->x;event->y=799-event->y;}return 1;
}
