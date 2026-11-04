#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static bool live=true;
static unsigned allocations,frees,fail_alloc,die_alloc,die_free,outstanding;
static void capture(void);
static void *tracked_alloc(size_t n) {
    assert(live);++allocations;
    void *p=allocations==fail_alloc?NULL:malloc(n);
    if(p)++outstanding;
    if(allocations==die_alloc){capture();live=false;}
    return p;
}
static void tracked_free(void *p) {
    assert(live);if(!p)return;
    ++frees;assert(outstanding);--outstanding;free(p);
    if(frees==die_free){capture();live=false;}
}
#define POINTS_CATALOG_ALLOC tracked_alloc
#define POINTS_CATALOG_FREE tracked_free
#include "../../Apps/points_scene_storage.h"

static struct {
    uint8_t cfg[64],meta[64];bool has_cfg,has_meta;
    unsigned gets,puts,io,die_io,context_io;
    int fail_get,drop_put,fail_put,mismatch_get;
    uint32_t cfg_size,meta_size;
} db;
static points_scene_legacy *watched_l;
static points_editor *watched_e;
static points_writer death_writer;
static points_catalog death_catalog;
static uint32_t *death_order;
static unsigned death_allocations,death_frees,death_io;
static void capture(void) {
    death_allocations=allocations;death_frees=frees;death_io=db.io;
    if(watched_l)death_writer=watched_l->writer;
    if(watched_e){death_catalog=watched_e->store.saved;death_order=watched_e->order;}
}
static bool is_alive(void *context){assert(context==&db);return live;}
static int which_key(const char *key) {
    if(!strcmp(key,POINTS_CONFIG_KEY))return 0;
    assert(!strcmp(key,POINTS_META_KEY));return 1;
}
static int32_t finish_io(int32_t rc) {
    if(db.io==db.context_io)return RISC_KEY_VALUE_CONTEXT;
    if(db.io==db.die_io){capture();live=false;}
    return rc;
}
static int32_t get(void *context,const char *key,void *bytes,uint32_t cap,uint32_t *size) {
    assert(context==&db&&live&&cap==64);int m=which_key(key);++db.gets;++db.io;*size=0;
    if((int)db.gets==db.fail_get)return finish_io(RISC_KEY_VALUE_IO);
    if(!(m?db.has_meta:db.has_cfg))return finish_io(RISC_KEY_VALUE_NOT_FOUND);
    memcpy(bytes,m?db.meta:db.cfg,64);*size=m?db.meta_size:db.cfg_size;
    if((int)db.gets==db.mismatch_get)((uint8_t*)bytes)[12]^=1;
    return finish_io(RISC_KEY_VALUE_OK);
}
static int32_t put(void *context,const char *key,const void *bytes,uint32_t size) {
    assert(context==&db&&live&&size==64);int m=which_key(key);++db.puts;++db.io;
    if((int)db.puts!=db.drop_put) {
        memcpy(m?db.meta:db.cfg,bytes,64);
        if(m){db.has_meta=true;db.meta_size=64;}else{db.has_cfg=true;db.cfg_size=64;}
    }
    return finish_io((int)db.puts==db.fail_put?RISC_KEY_VALUE_IO:RISC_KEY_VALUE_OK);
}
static const risc_key_value_v1 kv={1,sizeof(kv),&db,get,put};
static void reset(void) {
    assert(!outstanding);
    memset(&db,0,sizeof(db));db.cfg_size=db.meta_size=64;
    allocations=frees=fail_alloc=die_alloc=die_free=0;live=true;watched_l=NULL;watched_e=NULL;
}
static void seed(const points_config *c,const points_meta *m) {
    if(c){points_config_encode(c,db.cfg);db.has_cfg=true;}
    if(m){points_meta_encode(m,db.meta);db.has_meta=true;}
}
static void load(points_scene_legacy *l,points_editor *e) {
    watched_l=l;watched_e=e;psl_set_alive(l,is_alive,&db);
    assert(psl_load(l,e,&kv,POINTS_TIME_RAW_RTC)==0);
    assert(l->active&&points_editor_editable(e)&&!db.puts);
}
static void cleanup(points_scene_legacy *l,points_editor *e) {
    /* Test-only reclamation after inspecting the terminal owner's retained
     * allocations. Production never revives a terminal invocation. */
    live=true;die_alloc=die_free=0;db.die_io=db.context_io=0;l->terminal=false;e->store.retained=false;
    assert(psl_discard(l,e,&l->staged,&l->staged_order));
    assert(psl_discard(l,e,&l->retired,&l->retired_order));
    points_editor_dispose(e);watched_l=NULL;watched_e=NULL;
}
static void assert_projection(const points_scene_legacy *l,const points_editor *e) {
    points_catalog reference={0};
    assert(points_catalog_migrate(&reference,&l->writer.saved,&l->writer.meta,l->domain)==POINTS_CATALOG_OK);
    assert(points_catalog_equal(&reference,&e->store.saved));points_catalog_dispose(&reference);
    assert(e->order_count==e->store.saved.event_count);
    for(unsigned i=1;i<e->order_count;i++)assert(!points_editor_order_less(&e->store.saved,e->order[i],e->order[i-1]));
}
static void assert_terminal(points_scene_legacy *l,points_editor *e) {
    assert(l->terminal&&e->store.retained);
    if(!live) {
        assert(allocations==death_allocations&&frees==death_frees&&db.io==death_io);
        assert(!memcmp(&l->writer,&death_writer,sizeof(death_writer)));
        assert(!memcmp(&e->store.saved,&death_catalog,sizeof(death_catalog))&&e->order==death_order);
    }
    unsigned io=db.io,a=allocations,f=frees;
    assert(psl_load(l,e,&kv,0)==RISC_APP_DATA_RETAINED);
    assert(psl_save(l,e,&kv,100,POINTS_EDITOR_EVENT)==RISC_APP_DATA_RETAINED);
    assert(psl_retry(l,e,&kv)==RISC_APP_DATA_RETAINED);
    assert(io==db.io&&a==allocations&&f==frees);
}
static void test_defaults_and_reentry(void) {
    for(unsigned mask=0;mask<4;mask++) {
        reset();points_scene_legacy l={0};points_editor e={0};
        points_config c={.revision=11};points_meta m={.revision=12,.custom={{.color=2,.name="Focus"}}};
        seed(mask&1?&c:NULL,mask&2?&m:NULL);load(&l,&e);assert_projection(&l,&e);
        assert(e.order_count==((mask&1)?0:7));
        assert(l.writer.default_meta==(mask==0));
        assert(!strcmp(points_catalog_find_type(&e.store.saved,6)->name,mask&2?"Focus":mask&1?"Custom 1":"Drive to Work"));
        assert(points_editor_begin_event(&e,0));e.event.hour=22;points_editor_cancel_event(&e);
        assert(points_editor_begin_type(&e,6));strcpy(e.type.name,"Discard");points_editor_cancel_type(&e);
        assert(psl_load(&l,&e,&kv,0)==0);assert(!db.puts);assert_projection(&l,&e);cleanup(&l,&e);
    }
}
static void test_slots_and_preservation(void) {
    reset();points_scene_legacy l={0};points_editor e={0};points_config c=points_default_config();points_meta m=points_default_meta();
    c.points[1]=(points_item){0};c.points[6]=(points_item){0};c.points[7]=(points_item){POINTS_BREAK,1,3,127,3,0,10,1,1};
    seed(&c,&m);load(&l,&e);assert_projection(&l,&e);
    uint8_t saved_meta[64];memcpy(saved_meta,db.meta,64);
    for(unsigned n=0;n<2;n++) {
        points_config before=l.writer.saved;assert(points_editor_begin_event(&e,0));e.event.hour=23-n;
        assert(psl_save(&l,&e,&kv,100+n,POINTS_EDITOR_EVENT)==0);
        unsigned slot=n?6:1;assert(e.event.id==slot+1);
        for(unsigned i=0;i<8;i++)if(i!=slot)assert(!memcmp(&l.writer.saved.points[i],&before.points[i],sizeof(points_item)));
        assert(!memcmp(saved_meta,db.meta,64));assert_projection(&l,&e);
    }
    assert(e.order_count==8&&points_editor_begin_event(&e,0));unsigned writes=db.puts,reads=db.gets;
    assert(psl_save(&l,&e,&kv,110,POINTS_EDITOR_EVENT)==RISC_APP_DATA_NO_SPACE&&e.editing_event);
    assert(db.puts==writes&&db.gets==reads);
    points_editor_cancel_event(&e);assert(points_editor_begin_event(&e,3));points_config before=l.writer.saved;
    assert(psl_save(&l,&e,&kv,111,POINTS_EDITOR_DELETE)==0);assert(!l.writer.saved.points[2].kind);
    for(unsigned i=0;i<8;i++)if(i!=2)assert(!memcmp(&l.writer.saved.points[i],&before.points[i],sizeof(points_item)));
    assert(points_editor_begin_event(&e,0));e.event.id=3;writes=db.puts;
    assert(psl_save(&l,&e,&kv,112,POINTS_EDITOR_EVENT)==RISC_APP_DATA_INVALID&&db.puts==writes);
    e.event.id=0;assert(psl_save(&l,&e,&kv,113,POINTS_EDITOR_EVENT)==0&&e.event.id==3);
    assert(psl_load(&l,&e,&kv,0)==0&&e.order_count==8);assert_projection(&l,&e);
    assert(points_editor_begin_event(&e,0));assert(psl_save(&l,&e,&kv,113,POINTS_EDITOR_DELETE)==RISC_APP_DATA_INVALID);
    e.event.id=9;assert(psl_save(&l,&e,&kv,113,POINTS_EDITOR_DELETE)==RISC_APP_DATA_INVALID);cleanup(&l,&e);
}
static void test_every_slot_occupancy(void) {
    for(unsigned mask=0;mask<256;mask++) {
        reset();points_scene_legacy l={0};points_editor e={0};points_config c={.revision=42,.created=99};points_meta m=points_default_meta();
        for(unsigned i=0;i<8;i++)if(mask&(1u<<i)) {
            unsigned kind=1+i%7;
            c.points[i]=(points_item){.kind=(uint8_t)kind,.enabled=1,.mode=(uint8_t)(i%4),.weekdays=(uint8_t)(1u<<(i%7)),
                .hour=(uint8_t)(23-i*3),.minute=(uint8_t)(i*7),.duration_minutes=points_duration_kind(kind)?15:0};
        }
        seed(&c,&m);load(&l,&e);assert_projection(&l,&e);
        for(unsigned i=0;i<8;i++)assert(!!points_catalog_find_event(&e.store.saved,i+1)==!!(mask&(1u<<i)));
        if(mask) {
            unsigned slot=0;while(!(mask&(1u<<slot)))++slot;
            assert(points_editor_begin_event(&e,slot+1));e.event.minute=(uint8_t)((e.event.minute+1)%60);
            assert(psl_save(&l,&e,&kv,100,POINTS_EDITOR_EVENT)==0);
            for(unsigned i=0;i<8;i++)if(i!=slot)assert(!memcmp(&l.writer.saved.points[i],&c.points[i],sizeof(points_item)));
            assert(points_editor_begin_event(&e,slot+1));assert(psl_save(&l,&e,&kv,101,POINTS_EDITOR_DELETE)==0);
            c=l.writer.saved;
        }
        unsigned slot=0;while(c.points[slot].kind)++slot;
        assert(points_editor_begin_event(&e,0));e.event.hour=17;e.event.minute=59;
        assert(psl_save(&l,&e,&kv,102,POINTS_EDITOR_EVENT)==0&&e.event.id==slot+1);
        for(unsigned i=0;i<8;i++)if(i!=slot)assert(!memcmp(&l.writer.saved.points[i],&c.points[i],sizeof(points_item)));
        unsigned writes=db.puts;assert(psl_load(&l,&e,&kv,0)==0&&db.puts==writes);assert_projection(&l,&e);cleanup(&l,&e);
    }
}
static void test_custom_constraints(void) {
    reset();points_scene_legacy l={0};points_editor e={0};load(&l,&e);
    assert(points_editor_begin_type(&e,6));points_catalog_type original=e.type;
    static const char *invalid[]={"","abcdefghijklmn","bad\nname","bad\177"};
    for(unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++) {
        e.type=original;strcpy(e.type.name,invalid[i]);assert(psl_save(&l,&e,&kv,0,POINTS_EDITOR_TYPE)==RISC_APP_DATA_INVALID);
    }
    e.type=original;memset(e.type.name,'a',sizeof(e.type.name));assert(psl_save(&l,&e,&kv,0,POINTS_EDITOR_TYPE)==RISC_APP_DATA_INVALID);
    e.type=original;e.type.id=0;assert(psl_save(&l,&e,&kv,0,POINTS_EDITOR_TYPE)==RISC_APP_DATA_INVALID);
    e.type=original;e.type.id=8;assert(psl_save(&l,&e,&kv,0,POINTS_EDITOR_TYPE)==RISC_APP_DATA_INVALID);
    e.type=original;e.type.color=0xffffff;assert(psl_save(&l,&e,&kv,0,POINTS_EDITOR_TYPE)==RISC_APP_DATA_INVALID);
    e.type=original;e.type.duration_minutes=15;assert(psl_save(&l,&e,&kv,0,POINTS_EDITOR_TYPE)==RISC_APP_DATA_INVALID);
    e.type=original;e.type.flags=0;assert(psl_save(&l,&e,&kv,0,POINTS_EDITOR_TYPE)==RISC_APP_DATA_INVALID);
    e.type=original;e.type.mode=1;assert(psl_save(&l,&e,&kv,0,POINTS_EDITOR_TYPE)==RISC_APP_DATA_INVALID);
    assert(!db.puts&&db.gets==2);
    e.type=original;strcpy(e.type.name,"ABCDEFGHIJKLM");e.type.color=0xff3d71;
    assert(psl_save(&l,&e,&kv,0,POINTS_EDITOR_TYPE)==0);
    assert(!db.has_cfg&&db.puts==1&&!memcmp(db.meta,"PTM2",4));
    assert(l.writer.meta.custom[0].color==2&&!strcmp(l.writer.meta.custom[1].name,"Wakeup"));
    assert_projection(&l,&e);assert(psl_load(&l,&e,&kv,0)==0);assert_projection(&l,&e);
    assert(points_editor_begin_type(&e,7));strcpy(e.type.name,"Wake");
    assert(psl_save(&l,&e,&kv,0,POINTS_EDITOR_DELETE_TYPE)==RISC_APP_DATA_UNAVAILABLE);
    assert(psl_save(&l,&e,&kv,0,POINTS_EDITOR_TYPE)==0&&!strcmp(l.writer.meta.custom[0].name,"ABCDEFGHIJKLM"));
    cleanup(&l,&e);
}
static void test_fail_closed_and_conflicts(void) {
    for(unsigned variant=0;variant<9;variant++) {
        reset();points_scene_legacy l={0};points_editor e={0};points_config c=points_default_config();points_meta m=points_default_meta();seed(&c,&m);load(&l,&e);
        if(variant==0)db.cfg[0]^=1;
        if(variant==1)db.meta[0]^=1;
        if(variant==2)db.cfg_size=63;
        if(variant==3)db.meta_size=65;
        if(variant==4){db.meta[59]=1;alarm_write32(db.meta+60,points_checksum(db.meta));}
        if(variant==5){db.meta[8]=8;alarm_write32(db.meta+60,points_checksum(db.meta));}
        if(variant==6){db.cfg[14]=24;alarm_write32(db.cfg+60,points_checksum(db.cfg));}
        if(variant==7)db.fail_get=(int)db.gets+1;
        if(variant==8){db.meta[11]=0;alarm_write32(db.meta+60,points_checksum(db.meta));}
        assert(psl_load(&l,&e,&kv,0)==RISC_APP_DATA_IO);
        assert(!l.active&&!points_editor_editable(&e)&&!db.puts);cleanup(&l,&e);
    }
    for(unsigned variant=0;variant<3;variant++) {
        reset();points_scene_legacy l={0};points_editor e={0};load(&l,&e);assert(points_editor_begin_event(&e,2));e.event.hour=20;
        if(variant==0){points_config c=l.writer.saved;c.points[4].hour=13;seed(&c,NULL);}
        if(variant==1){points_meta m=l.writer.meta;strcpy(m.custom[1].name,"Changed");seed(NULL,&m);}
        if(variant==2){points_config c=l.writer.saved;points_meta m=l.writer.meta;seed(&c,&m);}
        assert(psl_save(&l,&e,&kv,100,POINTS_EDITOR_EVENT)==RISC_APP_DATA_STALE);
        assert(e.conflict&&e.editing_event&&!db.puts);
        assert(psl_save(&l,&e,&kv,100,POINTS_EDITOR_EVENT)==RISC_APP_DATA_INVALID);
        assert(psl_load(&l,&e,&kv,0)==0&&!e.conflict);cleanup(&l,&e);
    }
}
static void test_uncertain_retries(void) {
    for(unsigned variant=0;variant<5;variant++) {
        reset();points_scene_legacy l={0};points_editor e={0};load(&l,&e);assert(points_editor_begin_event(&e,0));e.event.hour=21;
        /* Fresh save: observed config/meta, meta put/get, config put/get. */
        if(variant==0)db.fail_get=5;
        if(variant==1)db.fail_get=6;
        if(variant==2)db.drop_put=1;
        if(variant==3)db.drop_put=2;
        if(variant==4)db.mismatch_get=6;
        assert(psl_save(&l,&e,&kv,100,POINTS_EDITOR_EVENT)==RISC_APP_DATA_COMMIT_UNKNOWN);
        assert(e.store.uncertain&&e.editing_event&&l.writer.uncertain&&e.pending_id==8);
        unsigned writes=db.puts;points_editor_cancel_event(&e);assert(e.editing_event);
        assert(psl_load(&l,&e,&kv,0)==RISC_APP_DATA_COMMIT_UNKNOWN&&db.puts==writes);
        assert(psl_save(&l,&e,&kv,100,POINTS_EDITOR_EVENT)==RISC_APP_DATA_INVALID&&db.puts==writes);
        db.fail_get=db.drop_put=db.mismatch_get=0;
        assert(psl_retry(&l,&e,&kv)==0&&!e.store.uncertain&&!e.editing_event&&e.event.id==8);
        assert(l.writer.saved.revision==2&&e.order_count==8);assert_projection(&l,&e);
        writes=db.puts;assert(psl_retry(&l,&e,&kv)==RISC_APP_DATA_INVALID&&db.puts==writes);cleanup(&l,&e);
    }
    reset();points_scene_legacy l={0};points_editor e={0};load(&l,&e);assert(points_editor_begin_type(&e,7));strcpy(e.type.name,"Morning");
    db.fail_get=5;assert(psl_save(&l,&e,&kv,0,POINTS_EDITOR_TYPE)==RISC_APP_DATA_COMMIT_UNKNOWN);
    assert(l.writer.meta_uncertain&&!l.writer.uncertain);points_editor_cancel_type(&e);assert(e.editing_type);
    db.fail_get=0;assert(psl_retry(&l,&e,&kv)==0&&!db.has_cfg&&!strcmp(l.writer.meta.custom[1].name,"Morning"));cleanup(&l,&e);
    /* A provider's uncertain put is confirmed by an exact successful read. */
    reset();l=(points_scene_legacy){0};e=(points_editor){0};load(&l,&e);assert(points_editor_begin_event(&e,1));db.fail_put=1;
    assert(psl_save(&l,&e,&kv,100,POINTS_EDITOR_EVENT)==0&&!e.store.uncertain);cleanup(&l,&e);
}
static void test_terminal_callbacks(void) {
    for(unsigned where=1;where<=2;where++) {
        reset();points_scene_legacy l={0};points_editor e={0};watched_l=&l;watched_e=&e;psl_set_alive(&l,is_alive,&db);db.die_io=where;
        assert(psl_load(&l,&e,&kv,0)==RISC_APP_DATA_RETAINED);assert_terminal(&l,&e);cleanup(&l,&e);
    }
    for(unsigned where=1;where<=6;where++) {
        reset();points_scene_legacy l={0};points_editor e={0};load(&l,&e);assert(points_editor_begin_event(&e,1));db.die_io=db.io+where;
        assert(psl_save(&l,&e,&kv,100,POINTS_EDITOR_EVENT)==RISC_APP_DATA_RETAINED);assert_terminal(&l,&e);cleanup(&l,&e);
    }
    for(unsigned where=1;where<=4;where++) {
        reset();points_scene_legacy l={0};points_editor e={0};load(&l,&e);assert(points_editor_begin_event(&e,1));db.fail_get=5;
        assert(psl_save(&l,&e,&kv,100,POINTS_EDITOR_EVENT)==RISC_APP_DATA_COMMIT_UNKNOWN);db.fail_get=0;db.die_io=db.io+where;
        assert(psl_retry(&l,&e,&kv)==RISC_APP_DATA_RETAINED);assert_terminal(&l,&e);cleanup(&l,&e);
    }
    reset();points_scene_legacy l={0};points_editor e={0};watched_l=&l;watched_e=&e;db.context_io=1;
    assert(psl_load(&l,&e,&kv,0)==RISC_APP_DATA_RETAINED&&db.io==1);assert_terminal(&l,&e);cleanup(&l,&e);
    for(unsigned where=1;where<=3;where++) {
        reset();l=(points_scene_legacy){0};e=(points_editor){0};watched_l=&l;watched_e=&e;psl_set_alive(&l,is_alive,&db);die_alloc=where;
        assert(psl_load(&l,&e,&kv,0)==RISC_APP_DATA_RETAINED);assert_terminal(&l,&e);cleanup(&l,&e);
    }
    for(unsigned where=1;where<=3;where++) {
        reset();l=(points_scene_legacy){0};e=(points_editor){0};load(&l,&e);die_free=frees+where;
        assert(psl_load(&l,&e,&kv,0)==RISC_APP_DATA_RETAINED);assert_terminal(&l,&e);cleanup(&l,&e);
    }
}
static void test_memory_failures_and_limits(void) {
    for(unsigned where=1;where<=3;where++) {
        reset();points_scene_legacy l={0};points_editor e={0};load(&l,&e);fail_alloc=allocations+where;
        assert(psl_load(&l,&e,&kv,0)==POINTS_STORAGE_MEMORY&&!points_editor_editable(&e)&&!db.puts);
        fail_alloc=0;assert(psl_load(&l,&e,&kv,0)==0);assert_projection(&l,&e);cleanup(&l,&e);
    }
    for(unsigned where=1;where<=3;where++) {
        reset();points_scene_legacy l={0};points_editor e={0};load(&l,&e);assert(points_editor_begin_event(&e,0));
        fail_alloc=allocations+where;
        assert(psl_save(&l,&e,&kv,100,POINTS_EDITOR_EVENT)==POINTS_STORAGE_MEMORY);
        assert(!points_editor_editable(&e)&&!l.writer.uncertain&&l.writer.saved.revision==2);
        unsigned writes=db.puts;fail_alloc=0;assert(psl_load(&l,&e,&kv,0)==0&&db.puts==writes&&e.order_count==8);
        assert_projection(&l,&e);cleanup(&l,&e);
    }
    reset();points_scene_legacy l={0};points_editor e={0};points_config c=points_default_config();points_meta m=points_default_meta();
    c.revision=m.revision=UINT32_MAX;seed(&c,&m);load(&l,&e);
    assert(points_editor_begin_event(&e,1));assert(psl_save(&l,&e,&kv,100,POINTS_EDITOR_EVENT)==RISC_APP_DATA_NO_SPACE);
    assert(points_editor_begin_type(&e,6));assert(psl_save(&l,&e,&kv,100,POINTS_EDITOR_TYPE)==RISC_APP_DATA_NO_SPACE&&!db.puts);
    assert(psl_load(&l,&e,&kv,2)==RISC_APP_DATA_INVALID);cleanup(&l,&e);
}
static int32_t missing_stat(void *ctx,const char *name,uint32_t *size,uint64_t *revision) {
    (void)ctx;assert(live&&!strcmp(name,POINTS_CATALOG_FILE));*size=0;*revision=0;return RISC_APP_DATA_NOT_FOUND;
}
static int32_t forbidden_read(void *ctx,const char *name,uint64_t expected,void *out,uint32_t cap,uint32_t *size,uint64_t *revision) {
    (void)ctx;(void)name;(void)expected;(void)out;(void)cap;(void)size;(void)revision;assert(false);return RISC_APP_DATA_IO;
}
static int32_t forbidden_replace(void *ctx,const char *name,uint64_t expected,const void *bytes,uint32_t size) {
    (void)ctx;(void)name;(void)expected;(void)bytes;(void)size;assert(false);return RISC_APP_DATA_IO;
}
static void test_guarded_catalog_import(void) {
    const risc_app_data_v1 files={1,sizeof(files),NULL,missing_stat,forbidden_read,forbidden_replace};
    /* Read-only legacy grants remain valid migration sources. */
    risc_key_value_v1 read_only=kv;read_only.put=NULL;
    for(unsigned where=0;where<=2;where++) {
        reset();points_scene_legacy l={0};points_editor e={0};watched_l=&l;watched_e=&e;db.die_io=where;
        int rc=points_editor_load_guarded(&e,&files,&read_only,0,is_alive,&db);
        if(where) {
            assert(rc==RISC_APP_DATA_RETAINED&&e.store.retained&&db.io==where&&!allocations&&!frees&&!e.store.loaded);
            assert(!memcmp(&e.store.saved,&death_catalog,sizeof(death_catalog)));
        } else assert(rc==0&&e.store.loaded&&e.store.missing&&e.order_count==7&&!db.puts);
        cleanup(&l,&e);
    }
    reset();points_scene_legacy l={0};points_editor e={0};db.context_io=1;
    assert(points_editor_load_guarded(&e,&files,&read_only,0,NULL,NULL)==RISC_APP_DATA_RETAINED&&db.io==1&&!allocations);
    cleanup(&l,&e);
}
int main(void) {
    test_defaults_and_reentry();test_slots_and_preservation();test_every_slot_occupancy();test_custom_constraints();test_fail_closed_and_conflicts();
    test_uncertain_retries();test_terminal_callbacks();test_memory_failures_and_limits();test_guarded_catalog_import();
    assert(!outstanding);
    puts("points scene legacy storage: defaults, slots, constraints, conflict/retry, callback custody and allocation faults passed");return 0;
}
