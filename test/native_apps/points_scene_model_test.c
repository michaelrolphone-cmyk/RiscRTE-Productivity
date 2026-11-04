#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../Apps/points_scene_model.h"

static uint8_t *disk;
static uint32_t disk_size,capacity=65536;
static uint64_t token,sequence;
static unsigned writes,gets,reads;
static bool unknown,commit_unknown,fail_read,stale;
static points_editor editor;
static points_scene_model model;
static risc_components_document_v1 document;
static int32_t file_stat(void *c,const char *name,uint32_t *size,uint64_t *revision) {
    (void)c;assert(!strcmp(name,POINTS_CATALOG_FILE));reads++;
    if(fail_read)return RISC_APP_DATA_IO;
    if(!disk)return RISC_APP_DATA_NOT_FOUND;
    *size=disk_size;*revision=token;return RISC_APP_DATA_OK;
}
static int32_t file_read(void *c,const char *name,uint64_t expected,void *out,uint32_t cap,uint32_t *size,uint64_t *revision) {
    (void)c;assert(!strcmp(name,POINTS_CATALOG_FILE));reads++;
    if(fail_read)return RISC_APP_DATA_IO;
    assert(expected==token&&cap>=disk_size&&disk);
    memcpy(out,disk,disk_size);*size=disk_size;*revision=token;return RISC_APP_DATA_OK;
}
static int32_t file_replace(void *c,const char *name,uint64_t expected,const void *bytes,uint32_t size) {
    (void)c;assert(!strcmp(name,POINTS_CATALOG_FILE));writes++;
    if(stale||expected!=token)return RISC_APP_DATA_STALE;
    if(size>capacity)return RISC_APP_DATA_NO_SPACE;
    if(!unknown||commit_unknown) {
        uint8_t *p=malloc(size);assert(p);memcpy(p,bytes,size);free(disk);disk=p;disk_size=size;token++;
    }
    if(unknown){fail_read=true;return RISC_APP_DATA_COMMIT_UNKNOWN;}
    return RISC_APP_DATA_OK;
}
static int32_t legacy_get(void *c,const char *key,void *out,uint32_t cap,uint32_t *size) {
    (void)c;assert(cap==POINTS_RECORD_SIZE);gets++;
    if(!strcmp(key,POINTS_CONFIG_KEY)) {
        points_config old=points_default_config();points_config_encode(&old,out);
    } else {
        assert(!strcmp(key,POINTS_META_KEY));points_meta meta=points_default_meta();points_meta_encode(&meta,out);
    }
    *size=POINTS_RECORD_SIZE;return RISC_KEY_VALUE_OK;
}
static const risc_app_data_v1 files={1,sizeof(files),NULL,file_stat,file_read,file_replace};
static const risc_key_value_v1 legacy={1,sizeof(legacy),NULL,legacy_get,NULL};
static void fixture(void) {
    points_editor_dispose(&editor);free(disk);disk=NULL;disk_size=0;token=sequence=0;
    writes=gets=reads=0;capacity=65536;unknown=commit_unknown=fail_read=stale=false;
    assert(points_editor_load(&editor,&files,&legacy,POINTS_TIME_RAW_RTC)==0);
    points_scene_init(&model,&editor);assert(!writes&&gets==2);
}
static void check_document(void) {
    assert(document.api_version==1&&document.struct_size==sizeof(document));
    assert(document.root==1&&document.route_count==1&&document.node_count<=RISC_COMPONENTS_MAX_NODES);
    assert(document.revision&&document.screen_key&&document.routes[0].back_action==POINTS_SCENE_BACK);
    for(uint32_t i=0;i<document.node_count;i++) {
        const risc_scene_node_v1 *n=&document.nodes[i];assert(n->id&&n->route==1);
        assert(memchr(n->label,0,sizeof(n->label))&&memchr(n->text,0,sizeof(n->text)));
        for(uint32_t j=0;j<i;j++)assert(document.nodes[j].id!=n->id);
        assert(n->kind!=RISC_SCENE_INTEGER&&n->kind!=RISC_SCENE_BOOLEAN&&n->kind!=RISC_SCENE_TIME_OF_DAY);
        if(n->kind==RISC_COMPONENT_SWITCH)assert(n->minimum==0&&n->maximum==1&&n->step==1&&n->value>=0&&n->value<=1);
        if(n->kind==RISC_COMPONENT_TIME_PICKER)assert(n->minimum==0&&n->maximum==1439);
    }
}
static void publish(void) {
    assert(points_scene_build(&model,&document));check_document();
    assert(!model.accepting);assert(points_scene_commit(&model,document.revision));
    assert(!model.dirty);
}
static const points_scene_binding *binding(unsigned action,uint32_t record,bool exact) {
    for(uint32_t i=0;i<model.binding_count;i++) {
        const points_scene_binding *b=&model.bindings[i];
        if(b->action==action&&(!exact||b->record==record))return b;
    }
    return NULL;
}
static risc_scene_event_v1 intent(unsigned action,uint32_t record,bool exact,int value) {
    const points_scene_binding *b=binding(action,record,exact);assert(b&&b->enabled);
    return (risc_scene_event_v1){.struct_size=sizeof(risc_scene_event_v1),.kind=b->kind,
        .document_revision=model.committed_revision,.node=b->node,.action=action,.value=value,.sequence=++sequence};
}
static enum points_scene_effect act_record(unsigned action,uint32_t record,bool exact,int value) {
    publish();risc_scene_event_v1 event=intent(action,record,exact,value);
    enum points_scene_effect result=points_scene_reduce(&model,&event);assert(model.dirty);return result;
}
static enum points_scene_effect act(unsigned action,int value) {return act_record(action,0,false,value);}
static void back(void) {assert(act(POINTS_SCENE_BACK,0)==POINTS_SCENE_EFFECT_NONE);}
static int32_t finish_save(enum points_scene_effect effect) {
    int32_t rc;unsigned action;
    switch(effect) {
    case POINTS_SCENE_EFFECT_SAVE_EVENT:action=POINTS_EDITOR_EVENT;rc=points_editor_save_event(&editor,&files,5000,false);break;
    case POINTS_SCENE_EFFECT_DELETE_EVENT:action=POINTS_EDITOR_DELETE;rc=points_editor_save_event(&editor,&files,5000,true);break;
    case POINTS_SCENE_EFFECT_SAVE_TYPE:action=POINTS_EDITOR_TYPE;rc=points_editor_save_type(&editor,&files,false);break;
    case POINTS_SCENE_EFFECT_DELETE_TYPE:action=POINTS_EDITOR_DELETE_TYPE;rc=points_editor_save_type(&editor,&files,true);break;
    default:assert(0);return -99;
    }
    points_scene_saved(&model,rc,action);return rc;
}
static void editing(void) {
    fixture();assert(act(POINTS_SCENE_ADD,0)==POINTS_SCENE_EFFECT_NONE);
    assert(editor.event.enabled&&editor.event.weekdays==127&&editor.event.hour==9);
    points_catalog_item original=editor.event;
    act(POINTS_SCENE_TIME_ACTION,0);act(POINTS_SCENE_TIME_VALUE,1439);assert(editor.event.hour==23&&editor.event.minute==59);
    back();assert(!memcmp(&editor.event,&original,sizeof(original)));
    act(POINTS_SCENE_TIME_ACTION,0);act(POINTS_SCENE_TIME_VALUE,65);act(POINTS_SCENE_DONE,0);
    assert(editor.event.hour==1&&editor.event.minute==5);
    act(POINTS_SCENE_DAYS_ACTION,0);act_record(POINTS_SCENE_DAY_PRESET,65,true,0);
    act_record(POINTS_SCENE_DAY_VALUE,1,true,1);assert(editor.event.weekdays==67);back();assert(editor.event.weekdays==127);
    act(POINTS_SCENE_DAYS_ACTION,0);act_record(POINTS_SCENE_DAY_PRESET,62,true,0);act(POINTS_SCENE_DONE,0);assert(editor.event.weekdays==62);
    act(POINTS_SCENE_TYPES_ACTION,0);act_record(POINTS_SCENE_SELECT_TYPE,POINTS_LUNCH,true,0);
    assert(editor.event.type_id==POINTS_LUNCH&&points_scene_duration(&model));
    act(POINTS_SCENE_DURATION_ACTION,0);uint16_t prior=editor.event.duration_minutes;
    act(POINTS_SCENE_DURATION_VALUE,0);assert(!editor.event.notify_end&&!editor.event.warn3);back();assert(editor.event.duration_minutes==prior);
    act(POINTS_SCENE_DURATION_ACTION,0);act(POINTS_SCENE_DURATION_VALUE,720);act(POINTS_SCENE_DONE,0);
    act(POINTS_SCENE_WARN3,1);act(POINTS_SCENE_NOTIFY_END,1);
    act(POINTS_SCENE_MODE_ACTION,0);act(POINTS_SCENE_MODE_VALUE,3);back();assert(editor.event.mode==original.mode);
    act(POINTS_SCENE_MODE_ACTION,0);act(POINTS_SCENE_MODE_VALUE,2);act(POINTS_SCENE_DONE,0);
    act(POINTS_SCENE_ENABLED,0);assert(!editor.event.enabled);act(POINTS_SCENE_ENABLED,1);
    assert(finish_save(act(POINTS_SCENE_SAVE,0))==0);assert(model.page==POINTS_SCENE_LIST&&editor.order_count==8&&writes==1);
    uint32_t id=editor.event.id;assert(points_catalog_find_event(&editor.store.saved,id)->duration_minutes==720);
    act_record(POINTS_SCENE_OPEN_EVENT,id,true,0);act(POINTS_SCENE_DELETE,0);assert(model.delete_confirm);
    act(POINTS_SCENE_ENABLED,0);assert(!model.delete_confirm);act(POINTS_SCENE_DELETE,0);
    assert(finish_save(act(POINTS_SCENE_DELETE,0))==0&&!points_catalog_find_event(&editor.store.saved,id));
    unsigned before=writes;act(POINTS_SCENE_ADD,0);back();assert(writes==before&&!editor.editing_event);
    act(POINTS_SCENE_ADD,0);act(POINTS_SCENE_DAYS_ACTION,0);
    for(unsigned day=0;day<7;day++)act_record(POINTS_SCENE_DAY_VALUE,day,true,0);
    act(POINTS_SCENE_DONE,0);assert(act(POINTS_SCENE_SAVE,0)==POINTS_SCENE_EFFECT_NONE&&model.page==POINTS_SCENE_DAYS&&writes==before);
    back();back();assert(!editor.editing_event);
}
static void custom_types(void) {
    fixture();act(POINTS_SCENE_ADD,0);act(POINTS_SCENE_TYPES_ACTION,0);act(POINTS_SCENE_ADD_TYPE,0);
    assert(editor.editing_event&&editor.editing_type&&editor.type.duration_minutes==15&&editor.type.flags==POINTS_TYPE_DURATION);
    act(POINTS_SCENE_NAME_ACTION,0);assert(model.pending_effect==POINTS_SCENE_EFFECT_NAME);
    publish();assert(!model.accepting);assert(points_scene_name_result(&model,true,"A custom type"));
    act(POINTS_SCENE_COLOR_ACTION,0);uint32_t prior=editor.type.color;
    act_record(POINTS_SCENE_COLOR_VALUE,0xff7a1a,true,0);back();assert(editor.type.color==prior);
    act(POINTS_SCENE_COLOR_ACTION,0);act_record(POINTS_SCENE_COLOR_VALUE,0xff7a1a,true,0);act(POINTS_SCENE_DONE,0);
    act(POINTS_SCENE_DURATION_ACTION,0);act(POINTS_SCENE_DURATION_VALUE,45);act(POINTS_SCENE_DONE,0);
    act(POINTS_SCENE_MODE_ACTION,0);act(POINTS_SCENE_MODE_VALUE,3);act(POINTS_SCENE_DONE,0);
    act(POINTS_SCENE_WARN3,1);act(POINTS_SCENE_NOTIFY_END,1);
    assert(finish_save(act(POINTS_SCENE_SAVE,0))==0&&model.page==POINTS_SCENE_EDIT);
    uint32_t id=editor.type.id;assert(editor.event.type_id==id&&editor.event.duration_minutes==45&&editor.event.mode==3&&editor.event.notify_end&&editor.event.warn3);
    assert(finish_save(act(POINTS_SCENE_SAVE,0))==0);
    act(POINTS_SCENE_TYPES_ACTION,0);act_record(POINTS_SCENE_EDIT_TYPE,id,true,0);
    act(POINTS_SCENE_NAME_ACTION,0);assert(points_scene_name_result(&model,false,NULL));assert(!strcmp(editor.type.name,"A custom type"));
    act(POINTS_SCENE_NAME_ACTION,0);assert(!points_scene_name_result(&model,true,"   "));assert(!strcmp(editor.type.name,"A custom type"));
    act(POINTS_SCENE_NAME_ACTION,0);assert(!points_scene_name_result(&model,true,"1234567890123456789012345678901234"));
    act(POINTS_SCENE_NAME_ACTION,0);assert(!points_scene_name_result(&model,true,"Bad\nName"));
    act(POINTS_SCENE_NAME_ACTION,0);assert(points_scene_name_result(&model,true,"Renamed type "));
    assert(finish_save(act(POINTS_SCENE_SAVE,0))==0);assert(!strcmp(points_catalog_find_type(&editor.store.saved,id)->name,"Renamed type"));
    act_record(POINTS_SCENE_EDIT_TYPE,id,true,0);act(POINTS_SCENE_DELETE,0);
    assert(finish_save(act(POINTS_SCENE_DELETE,0))==RISC_APP_DATA_UNAVAILABLE&&editor.editing_type);
    act(POINTS_SCENE_DURATION_ENABLED,0);assert(!editor.type.flags&&!editor.type.duration_minutes);
    back();assert(points_catalog_find_type(&editor.store.saved,id)->duration_minutes==45);
    act(POINTS_SCENE_ADD_TYPE,0);act(POINTS_SCENE_NAME_ACTION,0);assert(points_scene_name_result(&model,true,"Unused"));
    assert(finish_save(act(POINTS_SCENE_SAVE,0))==0);uint32_t unused=editor.type.id;
    act_record(POINTS_SCENE_EDIT_TYPE,unused,true,0);act(POINTS_SCENE_DELETE,0);
    assert(finish_save(act(POINTS_SCENE_DELETE,0))==0&&!points_catalog_find_type(&editor.store.saved,unused));
}
static void validation(void) {
    fixture();publish();risc_scene_event_v1 valid=intent(POINTS_SCENE_ADD,0,false,0),bad=valid;
    bad.node=123456;assert(points_scene_reduce(&model,&bad)==0&&!editor.editing_event);
    bad=valid;bad.action=POINTS_SCENE_DELETE;assert(points_scene_reduce(&model,&bad)==0&&!editor.editing_event);
    bad=valid;bad.kind=RISC_SCENE_VALUE_EVENT;assert(points_scene_reduce(&model,&bad)==0&&!editor.editing_event);
    bad=valid;bad.value=1;assert(points_scene_reduce(&model,&bad)==0&&!editor.editing_event);
    bad=valid;bad.document_revision++;assert(points_scene_reduce(&model,&bad)==0&&!editor.editing_event);
    bad=valid;bad.sequence=0;assert(points_scene_reduce(&model,&bad)==0&&!editor.editing_event);
    bad=valid;bad.struct_size=0;assert(points_scene_reduce(&model,&bad)==0&&!editor.editing_event);
    assert(points_scene_reduce(&model,&valid)==0&&editor.editing_event);
    publish();bad=intent(POINTS_SCENE_ENABLED,0,false,0);bad.sequence=valid.sequence;
    assert(points_scene_reduce(&model,&bad)==0&&editor.event.enabled);
    bad.sequence=++sequence;assert(points_scene_reduce(&model,&bad)==0&&!editor.event.enabled);
    assert(points_scene_reduce(&model,&bad)==0&&!model.accepting);
    act(POINTS_SCENE_TIME_ACTION,0);publish();bad=intent(POINTS_SCENE_TIME_VALUE,0,false,1440);
    assert(points_scene_reduce(&model,&bad)==0&&editor.event.hour==9&&model.accepting);
    bad.value=-1;assert(points_scene_reduce(&model,&bad)==0&&model.accepting);
    bad.value=0;assert(points_scene_reduce(&model,&bad)==0&&editor.event.hour==0);
    publish();uint32_t committed=model.committed_revision;
    bad=intent(POINTS_SCENE_BACK,0,false,0);assert(points_scene_build(&model,&document));
    assert(model.committed_revision==committed&&!model.accepting);
    assert(points_scene_reduce(&model,&bad)==0&&model.page==POINTS_SCENE_TIME);
    assert(!points_scene_commit(&model,committed));assert(points_scene_commit(&model,document.revision));
    assert(points_scene_reduce(&model,&bad)==0&&model.page==POINTS_SCENE_TIME);
    bad=(risc_scene_event_v1){.struct_size=sizeof(bad),.kind=RISC_SCENE_ACTION_EVENT,
        .document_revision=model.committed_revision,.node=0,.action=POINTS_SCENE_BACK,.sequence=++sequence};
    assert(points_scene_reduce(&model,&bad)==0&&model.page==POINTS_SCENE_EDIT);
    points_scene_begin_session(&model);assert(model.revision==0&&model.committed_revision==0&&!model.last_sequence&&editor.editing_event);
    publish();bad=intent(POINTS_SCENE_BACK,0,false,0);bad.sequence=1;assert(points_scene_reduce(&model,&bad)==0&&!editor.editing_event);
    assert(points_scene_build(&model,&document));uint32_t candidate=document.revision;
    points_scene_notice(&model,"New notice during publication");assert(points_scene_commit(&model,candidate));
    assert(model.dirty&&!model.accepting);publish();assert(!model.dirty&&model.accepting);
    snprintf(model.clock_text,sizeof(model.clock_text),"12:15 PM UTC");snprintf(model.schedule_status,sizeof(model.schedule_status),"DST gap: start skipped");
    model.next_event_id=points_editor_at(&editor,0)->id;publish();bool clock_seen=false,next_seen=false;
    assert(!strcmp(document.subtitle,model.schedule_status));
    for(uint32_t i=0;i<document.node_count;i++){
        if(!strcmp(document.nodes[i].label,"Local time"))clock_seen=true;
        if(!strcmp(document.details[i].badge,"Next"))next_seen=true;
    }
    assert(clock_seen&&next_seen);
    model.revision=UINT32_MAX;assert(!points_scene_build(&model,&document));
}
static void large_catalog(void) {
    fixture();points_catalog *c=&editor.store.saved;
    for(unsigned i=0;i<1001;i++) {
        points_catalog_item item={.type_id=1,.enabled=1,.weekdays=127,.hour=(uint8_t)(i%24),.minute=(uint8_t)(i%60)};
        uint32_t id;assert(points_catalog_add_event(c,&item,&id)==POINTS_CATALOG_OK);
    }
    for(unsigned i=0;i<250;i++) {
        points_catalog_type type={.color=0x123456,.duration_minutes=3,.flags=7,.mode=3};uint32_t id;
        snprintf(type.name,sizeof(type.name),"Custom type %u",i);assert(points_catalog_save_type(c,&type,&id)==POINTS_CATALOG_OK);
    }
    c->next_event_id=UINT32_C(0xf0000001);
    points_catalog_item high={.type_id=1,.enabled=1,.weekdays=127,.hour=23,.minute=59};uint32_t high_id;
    assert(points_catalog_add_event(c,&high,&high_id)==0&&high_id==UINT32_C(0xf0000001));
    assert(points_editor_reindex(&editor)==0&&editor.order_count==1009);
    uint32_t visited=0;
    for(;;) {
        publish();for(uint32_t i=0;i<model.binding_count;i++)if(model.bindings[i].action==POINTS_SCENE_OPEN_EVENT) {
            assert(points_catalog_find_event(c,model.bindings[i].record));visited++;
        }
        const points_scene_binding *next=binding(POINTS_SCENE_NEXT,0,false);assert(next);
        if(!next->enabled)break;
        risc_scene_event_v1 event=intent(POINTS_SCENE_NEXT,0,false,0);points_scene_reduce(&model,&event);
    }
    assert(visited==1009&&model.first==1000);
    act_record(POINTS_SCENE_OPEN_EVENT,high_id,true,0);assert(editor.event.id==high_id);back();
    act(POINTS_SCENE_TYPES_ACTION,0);visited=0;
    for(;;) {
        publish();for(uint32_t i=0;i<model.binding_count;i++)if(model.bindings[i].action==POINTS_SCENE_SELECT_TYPE)visited++;
        const points_scene_binding *next=binding(POINTS_SCENE_NEXT,0,false);if(!next->enabled)break;
        risc_scene_event_v1 event=intent(POINTS_SCENE_NEXT,0,false,0);points_scene_reduce(&model,&event);
    }
    assert(visited==257);act(POINTS_SCENE_PREVIOUS,0);assert(model.first==220);
    assert(!writes); /* Pagination and editing never persist. */
}
static void storage_outcomes(void) {
    fixture();act(POINTS_SCENE_ADD,0);capacity=1;
    assert(finish_save(act(POINTS_SCENE_SAVE,0))==RISC_APP_DATA_NO_SPACE&&editor.editing_event&&editor.order_count==7);
    capacity=65536;assert(finish_save(act(POINTS_SCENE_SAVE,0))==0&&editor.order_count==8);
    act(POINTS_SCENE_ADD,0);unknown=commit_unknown=true;
    assert(finish_save(act(POINTS_SCENE_SAVE,0))==RISC_APP_DATA_IO&&editor.store.uncertain);
    unsigned before=writes;publish();assert(!binding(POINTS_SCENE_SAVE,0,false));
    risc_scene_event_v1 back_event={.struct_size=sizeof(back_event),.kind=RISC_SCENE_ACTION_EVENT,
        .document_revision=model.committed_revision,.action=POINTS_SCENE_BACK,.sequence=++sequence};
    points_scene_reduce(&model,&back_event);assert(editor.editing_event&&model.page==POINTS_SCENE_EDIT);
    assert(act(POINTS_SCENE_RETRY_ACTION,0)==POINTS_SCENE_EFFECT_RETRY);
    unsigned action=editor.pending_action;int rc=points_editor_resolve(&editor,&files);points_scene_saved(&model,rc,action);
    assert(editor.store.uncertain&&writes==before);
    fail_read=unknown=false;act(POINTS_SCENE_RETRY_ACTION,0);action=editor.pending_action;
    rc=points_editor_resolve(&editor,&files);points_scene_saved(&model,rc,action);
    assert(rc==0&&!editor.store.uncertain&&editor.order_count==9&&writes==before&&model.page==POINTS_SCENE_LIST);
    act(POINTS_SCENE_ADD,0);unknown=true;commit_unknown=false;
    assert(finish_save(act(POINTS_SCENE_SAVE,0))==RISC_APP_DATA_IO);before=writes;
    fail_read=unknown=false;act(POINTS_SCENE_RETRY_ACTION,0);action=editor.pending_action;
    rc=points_editor_resolve(&editor,&files);points_scene_saved(&model,rc,action);
    assert(rc==RISC_APP_DATA_STALE&&editor.editing_event&&!editor.store.uncertain&&writes==before);
    assert(finish_save(act(POINTS_SCENE_SAVE,0))==0);
    act(POINTS_SCENE_ADD,0);stale=true;assert(finish_save(act(POINTS_SCENE_SAVE,0))==RISC_APP_DATA_STALE);
    before=writes;act(POINTS_SCENE_RETRY_ACTION,0);stale=false;
    assert(points_editor_load(&editor,&files,&legacy,POINTS_TIME_RAW_RTC)==0);points_scene_retry_done(&model);
    assert(writes==before&&editor.editing_event&&!editor.conflict);assert(finish_save(act(POINTS_SCENE_SAVE,0))==0);
}
static void capability_states(void) {
    fixture();model.ready=false;publish();assert(!binding(POINTS_SCENE_ADD,0,false));
    assert(act(POINTS_SCENE_RETRY_ACTION,0)==POINTS_SCENE_EFFECT_RETRY);points_scene_retry_done(&model);model.ready=true;
    act(POINTS_SCENE_ADD,0);model.clock_valid=false;publish();assert(!binding(POINTS_SCENE_SAVE,0,false)->enabled);
    model.clock_valid=true;model.service_valid=false;publish();assert(!binding(POINTS_SCENE_SAVE,0,false)->enabled);
    model.service_valid=true;model.service_busy=true;publish();assert(!binding(POINTS_SCENE_SAVE,0,false)->enabled);
    model.service_busy=false;model.visual_only=true;publish();assert(!binding(POINTS_SCENE_MODE_ACTION,0,false)->enabled);
    back();model.event_limit=7;publish();assert(!binding(POINTS_SCENE_ADD,0,false)->enabled);
    model.event_limit=8;publish();assert(binding(POINTS_SCENE_ADD,0,false)->enabled);
    model.custom_type_limit=2;act(POINTS_SCENE_TYPES_ACTION,0);publish();assert(!binding(POINTS_SCENE_ADD_TYPE,0,false)->enabled);
    model.legacy_type_metadata_only=true;model.type_name_limit=13;act_record(POINTS_SCENE_EDIT_TYPE,6,true,0);publish();
    assert(!binding(POINTS_SCENE_DURATION_ENABLED,0,false)&&!binding(POINTS_SCENE_DURATION_ACTION,0,false)&&!binding(POINTS_SCENE_DELETE,0,false));
    act(POINTS_SCENE_NAME_ACTION,0);assert(!points_scene_name_result(&model,true,"Fourteen chars"));
    act(POINTS_SCENE_NAME_ACTION,0);assert(points_scene_name_result(&model,true,"Short name"));
    assert(finish_save(act(POINTS_SCENE_SAVE,0))==0);
    editor.store.retained=true;assert(!points_scene_build(&model,&document));editor.store.retained=false;
}
static void retry_descriptions(void) {
    fixture();act(POINTS_SCENE_ADD,0);editor.store.uncertain=true;
    for(unsigned writes_pending=0;writes_pending<2;writes_pending++) {
        model.retry_writes=writes_pending!=0;
        points_scene_saved(&model,RISC_APP_DATA_IO,POINTS_EDITOR_EVENT);
        assert(!strcmp(model.notice,writes_pending?"Retry writes the pending saved draft, then verifies it":"Check reads storage; it never rewrites"));
        publish();bool button=false,description=false;
        for(uint32_t i=0;i<document.node_count;i++) {
            const risc_scene_node_v1 *node=&document.nodes[i];
            if(node->action==POINTS_SCENE_RETRY_ACTION) {
                assert(!strcmp(node->label,writes_pending?"Retry save":"Check save"));button=true;
            }
            if(node->kind==RISC_SCENE_TEXT_NODE) {
                assert(!strcmp(node->text,writes_pending?"Retry writes the pending saved draft, then verifies it":"Check reads the saved file without rewriting"));description=true;
            }
        }
        assert(button&&description&&!writes);
        assert(strstr(document.subtitle,writes_pending?"retry pending save":"check storage"));
        assert(points_scene_back(&model)==POINTS_SCENE_EFFECT_NONE);
        assert(!strcmp(model.notice,writes_pending?"Retry save before leaving":"Check save before leaving"));
        assert(editor.editing_event&&model.page==POINTS_SCENE_EDIT&&!writes);
    }
    editor.store.uncertain=false;
}
int main(void) {
    editing();custom_types();validation();large_catalog();storage_outcomes();capability_states();retry_descriptions();
    points_editor_dispose(&editor);free(disk);
    puts("Points scene model: semantic editing, rollback, dynamic pagination, stable IDs, committed intent validation, name service, capability negotiation, migration and uncertain-save outcomes passed");
    return 0;
}
