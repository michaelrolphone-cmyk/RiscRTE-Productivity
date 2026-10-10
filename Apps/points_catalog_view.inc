/* Shared controller, native NOVA typography and portrait paper typography.
 * Stable controls accept input independently of their pending paper frame. */
#ifdef ALARM_NATIVE_UTC
static int pcp_edit_offset;
#endif
static int pc_width(void){return app->screen_width();}
static int pc_height(void){return app->screen_height();}
static int pc_left(void){return paper?32:20;}
static int pc_top(void){return paper?112:82;}
static int pc_row_height(void){return paper?88:40;}
static unsigned pc_rows(void){return paper?(unsigned)(pc_height()-256)/88:3;}
static int pc_footer_top(void){return paper?pc_height()-112:204;}
static void pc_text(int x,int y,int width,const char *s,bool heading,uint32_t color,bool selected) {
    if(paper)paper->text(x,y,width,s,(heading?2u:1u)|PAPER_TEXT_LITERAL|PAPER_TEXT_FIT,heading,!selected);
    else portable_nova_text(heading?0:2,x,y,width,s,color);
}
static void pc_button(int x,int y,int width,int height,const char *s,bool selected) {
    if(paper) {
        app->fill_rect(x,y,width,height,true);
        if(!selected)app->fill_rect(x+3,y+3,width-6,height-6,false);
        int n=paper->measure(s,false);pc_text(x+(width-n)/2,y+(height-28)/2,width-10,s,false,0,selected);
    } else portable_nova_button(x,y,width,height,s,selected);
}
static void pc_header(const char *title,const char *sub) {
    int l=pc_left(),w=pc_width()-2*l;
    pc_text(l,paper?24:49,w,title,true,NOVA_CYAN,false);
    pc_text(l,paper?66:68,w,sub?sub:"",false,NOVA_CAP,false);
    if(paper)app->fill_rect(l,106,w,3,true);else portable_nova_fill(l,80,w,1,NOVA_DIM);
}
static void pc_footer(const char *left,const char *right) {
    int l=pc_left(),gap=paper?16:8,w=(pc_width()-2*l-gap)/2,h=paper?88:30;
    pc_button(l,pc_footer_top(),w,h,left,false);pc_button(l+w+gap,pc_footer_top(),w,h,right,true);
}
static void pc_row(unsigned shown,const char *name,const char *value,uint32_t color,bool chosen) {
    int y=pc_top()+(int)shown*pc_row_height(),l=pc_left(),w=pc_width()-2*l;
    if(paper) {
        if(chosen)app->fill_rect(l,y,w,88,true);
        pc_text(l+16,y+10,w-32,name,false,0,chosen);
        pc_text(l+16,y+42,w-32,value,false,0,chosen);
        app->fill_rect(l,y+85,w,3,true);
    } else {
        portable_nova_fill(l,y,2,39,color);
        portable_nova_text(3,l+10,y+3,w-20,name,chosen?NOVA_CYAN:NOVA_CAP);
        portable_nova_text(2,l+10,y+17,w-20,value,chosen?NOVA_CYAN:NOVA_WHITE);
        portable_nova_fill(l,y+39,w,1,NOVA_LINE);
    }
}
static unsigned pc_count(void) {
    if(page==PC_LIST)return editor.order_count;
    if(page==PC_EDIT)return 10;
    if(page==PC_TYPES)return editor.store.saved.type_count+1;
    if(page==PC_CUSTOM)return 9;
    if(page==PC_MODE)return 4;
    return 0;
}
static void pc_scroll(int direction) {
#ifdef ALARM_NATIVE_UTC
    if(paper&&page==PC_EDIT){pcp_edit_offset+=direction>0?186:-186;if(pcp_edit_offset<0)pcp_edit_offset=0;if(pcp_edit_offset>248)pcp_edit_offset=248;delete_confirm=false;dirty=true;return;}
#endif
    unsigned count=pc_count(),rows=pc_rows(),max=count>rows?count-rows:0;
    if(direction>0){unsigned next=first+rows;first=next>max?max:next;}
    else first=first>rows?first-rows:0;
    focus=first;delete_confirm=false;dirty=true;
}
static void pc_list(void) {
    char sub[64];unsigned active=0;
    for(uint32_t i=0;i<editor.store.saved.event_count;i++)active+=editor.store.saved.events[i].enabled;
    snprintf(sub,sizeof(sub),"%u active  %u total",active,editor.store.saved.event_count);pc_header("POINTS IN TIME",sub);
    for(unsigned i=0;i<pc_rows();i++) {
        const points_catalog_item *e=points_editor_at(&editor,first+i);if(!e)break;
        char tm[24],ds[32],meta[64];points_editor_time(e,time_format,tm,sizeof(tm));points_editor_days(e->weekdays,ds,sizeof(ds));
        if(!e->enabled)snprintf(meta,sizeof(meta),"OFF  %s",ds);
        else if(e->duration_minutes)snprintf(meta,sizeof(meta),"%s  %u min",ds,e->duration_minutes);
        else snprintf(meta,sizeof(meta),"%s",ds);
        int y=pc_top()+(int)i*pc_row_height(),l=pc_left();
        if(paper) {
            pc_text(l+8,y+10,154,tm,true,0,false);
            pc_text(l+176,y+8,pc_width()-l*2-176,pc_name(e->type_id),false,0,false);
            pc_text(l+176,y+44,pc_width()-l*2-176,meta,false,0,false);
            app->fill_rect(l,y+85,pc_width()-2*l,3,true);
        } else {
            portable_nova_fill(l,y,2,39,e->enabled?pc_color(e->type_id):NOVA_DIM);
            char short_time[12];points_editor_time(e,PORTABLE_TIME_FORMAT_24,short_time,sizeof(short_time));
            if(time_format==PORTABLE_TIME_FORMAT_12)snprintf(short_time,sizeof(short_time),"%u:%02u",e->hour%12?e->hour%12:12,e->minute);
            portable_nova_text(NOVA_FACE_ORBITRON_10,l+8,y+6,59,short_time,e->enabled?NOVA_CYAN:NOVA_CAP);
            if(time_format==PORTABLE_TIME_FORMAT_12)portable_nova_text(3,l+10,y+24,50,e->hour<12?"AM":"PM",NOVA_CAP);
            portable_nova_text(2,l+70,y+4,130,pc_name(e->type_id),e->enabled?NOVA_WHITE:NOVA_CAP);
            portable_nova_text(3,l+70,y+24,130,meta,NOVA_CAP);
            portable_nova_fill(l,y+39,200,1,NOVA_LINE);
        }
    }
    if(!editor.order_count)pc_text(pc_left(),paper?160:120,pc_width()-2*pc_left(),"No points yet",false,NOVA_CAP,false);
    if(paper)pc_text(32,pc_height()-146,pc_width()-64,pc_error(),false,0,false);
    pc_footer("BACK","+ ADD POINT");
}
static const char *pc_edit_title(void){return editor.event.id?"EDIT POINT":"ADD POINT";}
static void pc_edit_view(void) {
    static const char *const labels[]={"TYPE","TIME","DAYS","DURATION","NOTIFY AT END","3 MIN WARNING","NOTIFY START","ENABLED","CUSTOM TYPES","DELETE POINT"};
    pc_header(pc_edit_title(),notice&&*notice?notice:pc_name(editor.event.type_id));
    for(unsigned i=0;i<pc_rows()&&first+i<10;i++) {
        unsigned row=first+i;char value[64];
        if(row==0)snprintf(value,sizeof(value),"%s",pc_name(editor.event.type_id));
        else if(row==1)points_editor_time(&editor.event,time_format,value,sizeof(value));
        else if(row==2)points_editor_days(editor.event.weekdays,value,sizeof(value));
        else if(row==3){if(pc_duration())snprintf(value,sizeof(value),"%u min",editor.event.duration_minutes);else snprintf(value,sizeof(value),"Not used by type");}
        else if(row==4||row==5||row==7)snprintf(value,sizeof(value),"%s",(row==4?editor.event.notify_end:row==5?editor.event.warn3:editor.event.enabled)?"ON":"OFF");
        else if(row==6)snprintf(value,sizeof(value),"%s",pc_visual_only()?"Visual only":mode_names[editor.event.mode]);
        else if(row==8)snprintf(value,sizeof(value),"Create or edit saved types");
        else snprintf(value,sizeof(value),"%s",delete_confirm?"Tap again to delete":"Delete this point");
        pc_row(i,labels[row],value,row==9?0xff6a5f:pc_color(editor.event.type_id),false);
    }
    if(paper)pc_text(32,pc_height()-146,pc_width()-64,pc_error(),false,0,false);
    pc_footer("BACK","SAVE");
}
static void pc_types_view(void) {
    pc_header("POINT TYPE","Tap right to edit a custom type");
    for(unsigned i=0;i<pc_rows()&&first+i<pc_count();i++) {
        unsigned index=first+i;
        if(index==editor.store.saved.type_count){pc_row(i,"+ CUSTOM TYPE","Create a new saved type",NOVA_CYAN,false);continue;}
        const points_catalog_type *t=&editor.store.saved.types[index];char detail[64];
        if(t->flags&POINTS_TYPE_DURATION)snprintf(detail,sizeof(detail),"%u min / %s",t->duration_minutes,mode_names[t->mode]);
        else snprintf(detail,sizeof(detail),"%s",mode_names[t->mode]);
        pc_row(i,t->name,detail,t->color,editor.editing_event&&editor.event.type_id==t->id);
    }
    pc_footer("BACK","MORE");
}
static void pc_custom_view(void) {
    static const char *const labels[]={"NAME","COLOR","DURATION OPTION","DEFAULT DURATION","DEFAULT MODE","NOTIFY AT END","3 MIN WARNING","USE THIS TYPE","DELETE TYPE"};
    pc_header("CUSTOM TYPE",notice&&*notice?notice:"Defaults apply when choosing this type");
    for(unsigned i=0;i<pc_rows()&&first+i<9;i++) {
        unsigned row=first+i;char value[64];
        if(row==0)snprintf(value,sizeof(value),"%s",editor.type.name[0]?editor.type.name:"Tap to enter a name");
        else if(row==1)snprintf(value,sizeof(value),"#%06X",(unsigned)editor.type.color);
        else if(row==2)snprintf(value,sizeof(value),"%s",editor.type.flags&POINTS_TYPE_DURATION?"Enabled":"Disabled");
        else if(row==3)snprintf(value,sizeof(value),"%u min",editor.type.duration_minutes);
        else if(row==4)snprintf(value,sizeof(value),"%s",mode_names[editor.type.mode]);
        else if(row==5||row==6)snprintf(value,sizeof(value),"%s",editor.type.flags&(row==5?POINTS_TYPE_NOTIFY_END:POINTS_TYPE_WARN3)?"ON":"OFF");
        else if(row==7)snprintf(value,sizeof(value),"Save type, then review point");
        else snprintf(value,sizeof(value),"%s",delete_confirm?"Tap again to delete":"Only when no point uses it");
        pc_row(i,labels[row],value,editor.type.color,false);
    }
    if(paper)pc_text(32,pc_height()-146,pc_width()-64,pc_error(),false,0,false);
    pc_footer("CANCEL","SAVE TYPE");
}
static void pc_days_view(void) {
    pc_header("WEEKDAYS","Sunday through Saturday");
    int l=pc_left(),gap=paper?6:2,w=(pc_width()-2*l-6*gap)/7,y=paper?150:92,h=paper?80:38;
    for(unsigned i=0;i<7;i++)pc_button(l+(int)i*(w+gap),y,w,h,day_names[i],!!(editor.event.weekdays&(1u<<i)));
    int bw=pc_width()-2*l;
    pc_button(l,paper?278:135,bw,paper?80:20,"EVERY DAY",editor.event.weekdays==127);
    pc_button(l,paper?378:158,bw,paper?80:20,"MON-FRI",editor.event.weekdays==62);
    pc_button(l,paper?478:181,bw,paper?80:20,"WEEKENDS",editor.event.weekdays==65);
    pc_footer("BACK","DONE");
}
static bool pc_name_nonempty(void){for(unsigned i=0;name_draft[i];i++)if(name_draft[i]!=' ')return true;return false;}
static void pc_name_step(void){
    if(!name_client.active||!pc_live())return;
    if(!name_finished){
        int rc=portable_text_client_poll(&name_client,&name_result);
        if(rc==RISC_TEXT_ENTRY_AGAIN)return;
        if(rc!=RISC_TEXT_ENTRY_OK){pc_retain();return;}
        if(name_result.state==RISC_TEXT_ENTRY_EDITING)return;
        name_finished=true;
    }
    if(!pc_name_close())return;
#ifdef PORTABLE_RESIDENT_SHELL_CLIENT
    /* Only a negotiated physical/root Home reason exits the child. Back and
     * Escape remain ordinary editor cancellation after the same clean close. */
    if(name_result.flags&RISC_TEXT_ENTRY_HOME_CANCEL){exit_app=true;return;}
#endif
    if(name_result.state==RISC_TEXT_ENTRY_ACCEPTED){
        memcpy(name_draft,name_result.text,sizeof(name_draft));
        if(pc_name_nonempty()){memcpy(editor.type.name,name_draft,sizeof(name_draft));pc_page(PC_CUSTOM);notice="Name draft updated";}
        else {pc_page(PC_CUSTOM);notice="Choose a nonempty type name";}
    } else exit_app=pc_back();
}
static void pc_adjust_view(void) {
    bool custom=return_page==PC_CUSTOM;char value[48];
    if(page==PC_TIME)points_editor_time(&editor.event,time_format,value,sizeof(value));
    else snprintf(value,sizeof(value),"%u minutes",custom?editor.type.duration_minutes:editor.event.duration_minutes);
    pc_header(page==PC_TIME?"TIME":"DURATION",page==PC_TIME?"24-hour editor":"Draft only: use Save to keep");
    pc_text(pc_left(),paper?154:92,pc_width()-2*pc_left(),value,true,NOVA_CYAN,false);
    int l=pc_left(),bw=(pc_width()-2*l-8)/2,h=paper?88:30;
    pc_button(l,paper?256:128,bw,h,page==PC_TIME?"HOUR -":"- 5 MIN",false);
    pc_button(l+bw+8,paper?256:128,bw,h,page==PC_TIME?"HOUR +":"+ 5 MIN",false);
    pc_button(l,paper?368:166,bw,h,page==PC_TIME?"MINUTE -":"- 1 MIN",false);
    pc_button(l+bw+8,paper?368:166,bw,h,page==PC_TIME?"MINUTE +":"+ 1 MIN",false);
    pc_footer("BACK","DONE");
}
#ifdef ALARM_NATIVE_UTC
#include "points_catalog_paper.inc"
#endif
static void pc_draw(void) {
    if(!pc_live()||name_client.active||(paper&&!paper_frame_ready()))return;
#ifdef ALARM_NATIVE_UTC
    if(paper){pcp_draw();return;}
#endif
    if(paper)paper->begin();else portable_nova_begin();
    if(!pc_live())return;
    if(!ready||!editor.store.loaded||!editor.index_ready||editor.store.uncertain||editor.conflict) {
        pc_header("POINTS IN TIME",pc_error());
        pc_text(pc_left(),paper?180:115,pc_width()-2*pc_left(),editor.store.uncertain?"Check reads the saved file":"Saved points are preserved",false,NOVA_WHITE,false);
        pc_footer("BACK",editor.store.uncertain?"CHECK SAVE":"RETRY");
    } else if(page==PC_LIST)pc_list();
    else if(page==PC_EDIT)pc_edit_view();
    else if(page==PC_TYPES)pc_types_view();
    else if(page==PC_CUSTOM)pc_custom_view();
    else if(page==PC_DAYS)pc_days_view();
    else if(page==PC_TIME||page==PC_DURATION)pc_adjust_view();
    else if(page==PC_MODE) {
        pc_header("ALERT MODE","Choose the saved mode");
        for(unsigned i=0;i<pc_rows()&&first+i<4;i++)pc_row(i,mode_names[first+i],"",NOVA_CYAN,(return_page==PC_CUSTOM?editor.type.mode:editor.event.mode)==first+i);
        pc_footer("BACK","MORE");
    } else if(page==PC_COLOR) {
        pc_header("TYPE COLOR","Choose a color");
        int l=pc_left(),w=(pc_width()-2*l-18)/4,h=paper?96:42;
        for(unsigned i=0;i<8;i++) {
            int x=l+(int)(i%4)*(w+6),y=(paper?180:100)+(int)(i/4)*(h+12);char s[12];
            snprintf(s,sizeof(s),"%u",i+1);pc_button(x,y,w,h,s,editor.type.color==palette[i]);
            if(!paper)portable_nova_fill(x+3,y+h-8,w-6,5,palette[i]);
        }
        pc_footer("BACK","DONE");
    }
    if(!pc_live())return;
    app->present(clean_frame);if(!pc_live())return;clean_frame=false;dirty=false;
}
static void pc_custom_action(unsigned row);
static void pc_select_type(unsigned at,bool edit) {
    if(at==editor.store.saved.type_count){if(points_editor_begin_type(&editor,0)){notice="New type draft";pc_page(PC_CUSTOM);if(paper){pc_custom_action(0);name_return_page=PC_TYPES;}}return;}
    if(at>=editor.store.saved.type_count)return;
    uint32_t id=editor.store.saved.types[at].id;
    if(edit&&id>POINTS_BEDTIME){if(points_editor_begin_type(&editor,id)){notice="Type draft only";pc_page(PC_CUSTOM);}}
    else if(editor.editing_event){(void)points_catalog_apply_type(&editor.event,&editor.store.saved,id);notice="Type defaults applied to draft";pc_page(PC_EDIT);}
}
static void pc_edit_action(unsigned row) {
    if(row!=9)delete_confirm=false;
    return_page=PC_EDIT;
    if(row==0||row==8)pc_page(PC_TYPES);
    else if(row==1)pc_page(PC_TIME);
    else if(row==2)pc_page(PC_DAYS);
    else if(row==3){if(pc_duration())pc_page(PC_DURATION);else notice="This type has no duration";}
    else if(row==4){if(editor.event.duration_minutes)editor.event.notify_end^=1;else notice="Choose a duration first";}
    else if(row==5){if(editor.event.duration_minutes>=3)editor.event.warn3^=1;else notice="Choose at least 3 minutes";}
    else if(row==6){if(!pc_visual_only())pc_page(PC_MODE);else notice="Visual only: saved mode kept";}
    else if(row==7)editor.event.enabled^=1;
    else if(row==9){if(!editor.event.id){points_editor_cancel_event(&editor);pc_page(PC_LIST);}else if(delete_confirm)pc_save(true);else {delete_confirm=true;notice="Tap delete again to confirm";}}
    dirty=true;
}
static void pc_custom_action(unsigned row) {
    if(row!=8)delete_confirm=false;
    return_page=PC_CUSTOM;
    if(row==0){name_return_page=PC_CUSTOM;memcpy(name_draft,editor.type.name,sizeof(name_draft));pc_page(PC_NAME);}
    else if(row==1)pc_page(PC_COLOR);
    else if(row==2){editor.type.flags^=POINTS_TYPE_DURATION;if(!(editor.type.flags&POINTS_TYPE_DURATION)){editor.type.flags=0;editor.type.duration_minutes=0;}}
    else if(row==3){if(editor.type.flags&POINTS_TYPE_DURATION)pc_page(PC_DURATION);else notice="Enable the duration option first";}
    else if(row==4)pc_page(PC_MODE);
    else if(row==5){if(editor.type.duration_minutes)editor.type.flags^=POINTS_TYPE_NOTIFY_END;else notice="Choose a duration first";}
    else if(row==6){if(editor.type.duration_minutes>=3)editor.type.flags^=POINTS_TYPE_WARN3;else notice="Choose at least 3 minutes";}
    else if(row==7)pc_save(false);
    else if(row==8){if(!editor.type.id){points_editor_cancel_type(&editor);pc_page(PC_TYPES);}else if(delete_confirm)pc_save(true);else{delete_confirm=true;notice="Tap delete again to confirm";}}
    dirty=true;
}
static void pc_activate(unsigned index,bool right) {
    if(page==PC_LIST){const points_catalog_item *e=points_editor_at(&editor,index);if(e&&points_editor_begin_event(&editor,e->id)){notice="Draft only: Save keeps changes";pc_page(PC_EDIT);}}
    else if(page==PC_EDIT)pc_edit_action(index);
    else if(page==PC_TYPES)pc_select_type(index,right);
    else if(page==PC_CUSTOM)pc_custom_action(index);
    else if(page==PC_MODE&&index<4){if(return_page==PC_CUSTOM)editor.type.mode=(uint8_t)index;else if(!pc_visual_only())editor.event.mode=(uint8_t)index;pc_page(return_page);}
}
static void pc_tap(int x,int y) {
    if(!pc_live())return;
#ifdef ALARM_NATIVE_UTC
    if(paper){pcp_tap(x,y);return;}
#endif
    int l=pc_left(),w=pc_width(),mid=w/2;
    if(y>=pc_footer_top()&&y<pc_footer_top()+(paper?88:30)&&x>=l&&x<w-l) {
        if(x>=mid-4&&x<mid+4)return;
        if(x<mid){exit_app=pc_back();return;}
        if(!ready||!editor.store.loaded||!editor.index_ready||editor.store.uncertain||editor.conflict){pc_retry();return;}
        if(page==PC_LIST){if(points_editor_begin_event(&editor,0)){notice="New point draft";pc_page(PC_EDIT);}}
        else if(page==PC_EDIT||page==PC_CUSTOM)pc_save(false);
        else if(page==PC_TYPES||page==PC_MODE){unsigned max=pc_count()>pc_rows()?pc_count()-pc_rows():0;if(first==max)first=0;else pc_scroll(1);}
        else pc_page(return_page==PC_CUSTOM?PC_CUSTOM:PC_EDIT);
        dirty=true;return;
    }
    if(!ready||!points_editor_editable(&editor))return;
    if(pc_count()&&y>=pc_top()&&y<pc_top()+(int)pc_rows()*pc_row_height()&&x>=l&&x<w-l) {
        pc_activate(first+(unsigned)(y-pc_top())/(unsigned)pc_row_height(),x>=w-(paper?112:52));dirty=true;return;
    }
    if(page==PC_DAYS) {
        int gap=paper?6:2,bw=(w-2*l-6*gap)/7,top=paper?150:92,h=paper?80:38;
        if(y>=top&&y<top+h&&x>=l&&x<w-l){unsigned i=(unsigned)(x-l)/(unsigned)(bw+gap);if(i<7&&(x-l)%(bw+gap)<bw)editor.event.weekdays^=(uint8_t)(1u<<i);}
        else if(x>=l&&x<w-l) {
            if(y>=(paper?278:135)&&y<(paper?358:155))editor.event.weekdays=127;
            else if(y>=(paper?378:158)&&y<(paper?458:178))editor.event.weekdays=62;
            else if(y>=(paper?478:181)&&y<(paper?558:201))editor.event.weekdays=65;
        }
    } else if(page==PC_TIME||page==PC_DURATION) {
        if(x<l||x>=w-l||(x>=mid-4&&x<mid+4))return;
        int direction=x<mid?-1:1,which=-1;
        if(y>=(paper?256:128)&&y<(paper?344:158))which=0;
        else if(y>=(paper?368:166)&&y<(paper?456:196))which=1;
        if(which<0)return;
        if(page==PC_TIME){if(!which)editor.event.hour=(uint8_t)((editor.event.hour+24+direction)%24);else editor.event.minute=(uint8_t)((editor.event.minute+60+direction)%60);}
        else {
            uint16_t *duration=return_page==PC_CUSTOM?&editor.type.duration_minutes:&editor.event.duration_minutes;
            int value=*duration+direction*(which?1:5);*duration=(uint16_t)(value<0?0:value>720?720:value);
            if(return_page==PC_CUSTOM){if(!*duration)editor.type.flags&=~POINTS_TYPE_NOTIFY_END;if(*duration<3)editor.type.flags&=~POINTS_TYPE_WARN3;}
            else {if(!*duration)editor.event.notify_end=0;if(*duration<3)editor.event.warn3=0;}
        }
    } else if(page==PC_COLOR) {
        int bw=(w-2*l-18)/4,h=paper?96:42,top=paper?180:100;
        if(x>=l&&x<w-l&&y>=top&&y<top+2*(h+12)) {
            unsigned col=(unsigned)(x-l)/(unsigned)(bw+6),row=(unsigned)(y-top)/(unsigned)(h+12);
            if(col<4&&row<2&&(x-l)%(bw+6)<bw&&(y-top)%(h+12)<h)editor.type.color=palette[row*4+col];
        }
    }
    dirty=true;
}
static void pc_input(const t5_app_input_t *input) {
    if(!pc_live()||name_client.active)return;
#ifdef ALARM_NATIVE_UTC
    if(paper&&!paper_input_ready&&!clean_frame&&paper_frame_ready())paper_input_ready=true;
#endif

    /* A new page must become visible once before it can accept contacts.
     * After that its stable controls remain interactive during presentation;
     * input changes the model while rendering waits for its own frame lease. */
#ifdef ALARM_NATIVE_UTC
    if(paper?!paper_input_ready:dirty){touch_down=false;touch_moved=true;return;}
#else
    if(dirty||(paper&&!paper_frame_ready())){touch_down=false;touch_moved=true;return;}
#endif
    if((input->buttons&T5_APP_BUTTON_BACK)||(input->tapped&&input->touch_y<32&&input->touch_x<48)){exit_app=pc_back();return;}
    if(input->buttons&(T5_APP_BUTTON_UP|T5_APP_BUTTON_LEFT))pc_scroll(-1);
    if(input->buttons&(T5_APP_BUTTON_DOWN|T5_APP_BUTTON_RIGHT))pc_scroll(1);
    if(input->buttons&T5_APP_BUTTON_CONFIRM){pc_activate(focus,false);dirty=true;}
    springboard_contact c={0};if(paper)paper->contact(&c);else nova->contact(&c);
#ifdef ALARM_NATIVE_UTC
    if(paper&&page==PC_TYPES){pcp_types_input(&c);return;}
#endif
    if(!c.valid||c.cancelled){touch_down=false;touch_moved=true;}
    if(c.valid&&c.began){touch_down=true;touch_moved=false;touch_x=c.x;touch_y=touch_last_y=c.y;}
    if(c.valid&&c.down&&touch_down){touch_last_y=c.y;if(abs(c.x-touch_x)>12||abs(c.y-touch_y)>12)touch_moved=true;}
    if(c.valid&&c.released&&!c.cancelled) {
        int dy=touch_last_y-touch_y,dx=c.x-touch_x;
        if(touch_down&&touch_moved&&abs(dy)>=(paper?44:20)&&abs(dy)>abs(dx)*2)pc_scroll(dy<0?1:-1);
        else if(touch_down&&!touch_moved&&c.tap_eligible)pc_tap(c.x,c.y);
        touch_down=false;
    } else if(input->tapped&&!c.valid)pc_tap(input->touch_x,input->touch_y);
}
