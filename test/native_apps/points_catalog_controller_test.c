#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../Apps/points_catalog_controller.h"
static uint8_t *document;
static uint32_t document_size,capacity=65536;
static uint64_t token;
static unsigned stats,reads,replaces,gets;
static bool fail_read,retain_read,unknown,commit_unknown,stale;
static points_config old;
static points_meta meta;
static bool missing_old,missing_meta,bad_old;
static int32_t stat_file(void *c,const char *name,uint32_t *size,uint64_t *rev) {
    (void)c;assert(!strcmp(name,POINTS_CATALOG_FILE));stats++;
    if(fail_read)return RISC_APP_DATA_IO;
    if(retain_read)return RISC_APP_DATA_RETAINED;
    if(!document)return RISC_APP_DATA_NOT_FOUND;
    *size=document_size;*rev=token;return 0;
}
static int32_t read_file(void *c,const char *name,uint64_t expected,void *out,uint32_t cap,uint32_t *size,uint64_t *rev) {
    (void)c;assert(!strcmp(name,POINTS_CATALOG_FILE));reads++;
    assert(expected==token&&document&&cap>=document_size);
    memcpy(out,document,document_size);*size=document_size;*rev=token;return 0;
}
static int32_t replace_file(void *c,const char *name,uint64_t expected,const void *bytes,uint32_t size) {
    (void)c;assert(!strcmp(name,POINTS_CATALOG_FILE));replaces++;
    if(expected!=token||stale)return RISC_APP_DATA_STALE;
    if(size>capacity)return RISC_APP_DATA_NO_SPACE;
    if(!unknown||commit_unknown) {
        uint8_t *next=malloc(size);assert(next);memcpy(next,bytes,size);free(document);document=next;document_size=size;token++;
    }
    if(unknown){fail_read=true;return RISC_APP_DATA_COMMIT_UNKNOWN;}return 0;
}
static int32_t legacy_get(void *c,const char *name,void *bytes,uint32_t cap,uint32_t *size) {
    (void)c;assert(cap==64);gets++;*size=0;
    if(!strcmp(name,POINTS_CONFIG_KEY)) {
        if(missing_old)return RISC_KEY_VALUE_NOT_FOUND;
        points_config_encode(&old,bytes);if(bad_old)((uint8_t*)bytes)[0]=0;
    } else {assert(!strcmp(name,POINTS_META_KEY));if(missing_meta)return RISC_KEY_VALUE_NOT_FOUND;points_meta_encode(&meta,bytes);}
    *size=64;return 0;
}
static const risc_app_data_v1 files={1,sizeof(files),NULL,stat_file,read_file,replace_file};
/* A read-only grant has no put callback: migration must accept it. */
static const risc_key_value_v1 legacy={1,sizeof(legacy),NULL,legacy_get,NULL};
static void reset(void) {
    free(document);document=NULL;document_size=0;token=0;stats=reads=replaces=gets=0;capacity=65536;
    fail_read=retain_read=unknown=commit_unknown=stale=missing_old=missing_meta=bad_old=false;
    old=points_default_config();meta=points_default_meta();
}
static void load(points_editor *e){assert(points_editor_load(e,&files,&legacy,POINTS_TIME_RAW_RTC)==0);}
int main(void) {
    points_editor e={0};reset();old.points[1]=(points_item){0};old.revision=42;old.created=99;strcpy(meta.custom[0].name,"Drive custom");load(&e);
    assert(gets==2&&!replaces&&e.store.missing&&e.store.saved.event_count==6&&e.store.saved.next_event_id==9);
    const points_catalog_item *saved=points_catalog_find_event(&e.store.saved,3);assert(saved&&saved->revision==42&&saved->created==99);
    assert(!strcmp(points_catalog_find_type(&e.store.saved,6)->name,"Drive custom"));
    assert(points_editor_begin_event(&e,3));points_catalog_item original=e.event;e.event.hour=22;e.event.weekdays=1;points_editor_cancel_event(&e);
    assert(!memcmp(points_catalog_find_event(&e.store.saved,3),&original,sizeof(original))&&!replaces);
    for(unsigned i=0;i<120;i++) {
        assert(points_editor_begin_event(&e,0));e.event.hour=(uint8_t)(i%24);e.event.minute=(uint8_t)(i%60);e.event.weekdays=(uint8_t)(1u<<(i%7));
        assert(points_editor_save_event(&e,&files,1000+i,false)==0);assert(e.event.id==9+i);
    }
    assert(e.store.saved.event_count==126&&e.order_count==126);
    for(unsigned i=1;i<e.order_count;i++) {
        const points_catalog_item *a=points_editor_at(&e,i-1),*b=points_editor_at(&e,i);
        assert(a->hour*60u+a->minute<=b->hour*60u+b->minute);
    }
    points_editor_dispose(&e);unsigned legacy_reads=gets;load(&e);assert(e.order_count==126&&gets==legacy_reads);
    /* Full volume cannot damage the committed catalog or discard a draft. */
    assert(points_editor_begin_event(&e,0));uint32_t count=e.order_count;capacity=document_size;
    unsigned writes=replaces;assert(points_editor_save_event(&e,&files,2000,false)==RISC_APP_DATA_NO_SPACE);
    assert(e.editing_event&&e.order_count==count&&!e.store.uncertain&&replaces==writes+1);
    capacity=65536;assert(points_editor_save_event(&e,&files,2000,false)==0&&e.order_count==count+1);
    /* A committed but unconfirmed save only resolves by reading; duplicate
     * replacement would create a second ID or overwrite a newer document. */
    assert(points_editor_begin_event(&e,0));unknown=commit_unknown=true;
    assert(points_editor_save_event(&e,&files,2100,false)==RISC_APP_DATA_IO&&e.store.uncertain);
    writes=replaces;points_editor_cancel_event(&e);assert(e.editing_event&&!points_editor_begin_event(&e,0));
    assert(points_editor_save_event(&e,&files,2100,false)==RISC_APP_DATA_INVALID&&replaces==writes);
    assert(points_editor_resolve(&e,&files)==RISC_APP_DATA_IO&&replaces==writes&&e.store.uncertain);
    fail_read=unknown=false;assert(points_editor_resolve(&e,&files)==0&&!e.store.uncertain&&replaces==writes&&e.order_count==count+2);
    /* When old bytes survived, adopt them, keep the draft, require Save. */
    assert(points_editor_begin_event(&e,0));unknown=true;commit_unknown=false;
    assert(points_editor_save_event(&e,&files,2200,false)==RISC_APP_DATA_IO);writes=replaces;
    fail_read=unknown=false;assert(points_editor_resolve(&e,&files)==RISC_APP_DATA_STALE&&e.editing_event&&replaces==writes);
    assert(points_editor_save_event(&e,&files,2200,false)==0&&e.order_count==count+3);
    /* Actual conflicts reload explicitly before another write. */
    assert(points_editor_begin_event(&e,0));stale=true;
    assert(points_editor_save_event(&e,&files,2300,false)==RISC_APP_DATA_STALE&&e.conflict);writes=replaces;
    assert(points_editor_save_event(&e,&files,2300,false)==RISC_APP_DATA_INVALID&&replaces==writes);
    stale=false;load(&e);assert(e.editing_event&&!e.conflict);assert(points_editor_save_event(&e,&files,2300,false)==0);
    /* Types are dynamic, persistent and have independent revisions/defaults. */
    points_catalog_item untouched=e.store.saved.events[0];
    for(unsigned i=0;i<25;i++) {
        assert(points_editor_begin_type(&e,0));snprintf(e.type.name,sizeof(e.type.name),"Custom activity %u",i);
        e.type.color=0x123456+i;e.type.symbol=(uint8_t)(i%8);e.type.duration_minutes=45;e.type.mode=ALARM_MODE_BOTH;e.type.flags=7;
        assert(points_editor_save_type(&e,&files,false)==0);
    }
    assert(e.store.saved.type_count==32&&!memcmp(&e.store.saved.events[0],&untouched,sizeof(untouched)));
    uint32_t type_id=e.type.id;load(&e);assert(points_catalog_find_type(&e.store.saved,type_id)->symbol==0);assert(points_catalog_find_type(&e.store.saved,type_id-1)->symbol==7);assert(points_editor_begin_event(&e,0));assert(points_catalog_apply_type(&e.event,&e.store.saved,type_id)==0);
    assert(e.event.duration_minutes==45&&e.event.notify_end&&e.event.warn3&&e.event.mode==3);
    assert(points_editor_save_event(&e,&files,2400,false)==0);uint32_t event_id=e.event.id;
    assert(points_editor_begin_type(&e,type_id));strcpy(e.type.name,"A genuinely custom saved type");e.type.duration_minutes=30;
    assert(points_editor_save_type(&e,&files,false)==0);
    saved=points_catalog_find_event(&e.store.saved,event_id);assert(saved->duration_minutes==45&&saved->revision==1);
    assert(points_editor_begin_type(&e,type_id));assert(points_editor_save_type(&e,&files,true)==RISC_APP_DATA_UNAVAILABLE);
    e.type.flags=0;e.type.duration_minutes=0;assert(points_editor_save_type(&e,&files,false)==RISC_APP_DATA_UNAVAILABLE);
    points_editor_cancel_type(&e);assert(points_editor_begin_event(&e,event_id));e.event.weekdays=0;
    assert(points_editor_save_event(&e,&files,2500,false)==RISC_APP_DATA_INVALID);
    for(unsigned d=0;d<7;d++){e.event.weekdays=(uint8_t)(1u<<d);assert(points_editor_save_event(&e,&files,2500+d,false)==0);assert(points_editor_begin_event(&e,event_id));}
    assert(points_editor_save_event(&e,&files,2600,true)==0);assert(!points_catalog_find_event(&e.store.saved,event_id));
    assert(points_editor_begin_type(&e,type_id));assert(points_editor_save_type(&e,&files,true)==0);
    points_editor_dispose(&e);load(&e);assert(e.store.saved.type_count==31);
    char time[32];points_catalog_item tm={.hour=4,.minute=3};points_editor_time(&tm,1,time,sizeof(time));assert(!strcmp(time,"4:03"));points_editor_time(&tm,0,time,sizeof(time));assert(!strcmp(time,"4:03 AM"));
    /* Retention is terminal, including repeated entry attempts. */
    retain_read=true;unsigned before=stats;assert(points_editor_load(&e,&files,&legacy,0)==RISC_APP_DATA_RETAINED&&stats==before+1);
    before=stats+reads+replaces+gets;assert(points_editor_load(&e,&files,&legacy,0)==RISC_APP_DATA_RETAINED);assert(points_editor_resolve(&e,&files)==RISC_APP_DATA_RETAINED);assert(!points_editor_begin_event(&e,0));assert(stats+reads+replaces+gets==before);
    points_editor_dispose(&e);assert(e.store.retained);e=(points_editor){0}; /* Simulated cold invocation; old allocations stay pinned. */
    reset();bad_old=true;assert(points_editor_load(&e,&files,&legacy,0)==RISC_APP_DATA_IO&&!e.store.loaded&&!replaces);points_editor_dispose(&e);
    reset();missing_old=missing_meta=true;load(&e);assert(e.order_count==7&&!replaces);points_editor_dispose(&e);free(document);
    puts("Points catalog controller: 126+ events, dynamic custom types, migration, storage full/retry, uncertain-save resolution, stable IDs, day masks, cancel/reentry and terminal retention passed");
}
