#ifndef PRODUCTIVITY_POINTS_SCENE_STORAGE_H
#define PRODUCTIVITY_POINTS_SCENE_STORAGE_H
/* Adapt the catalog editor to the unchanged legacy eight-slot/two-label
 * records. Opening and cancelling never write. IDs are slot + 1, not list
 * positions. The caller supplies the negotiated native/raw key adapter.
 *
 * The optional alive callback must remain callable after storage/allocator
 * custody is lost. Install it before entry. Once fenced, only terminal and
 * allocation-ownership bookkeeping changes; no further I/O, allocation, free,
 * or saved-state adoption occurs. The owner must retain this entire state. */
#include "points_catalog_controller.h"
#include "points_writer.h"

typedef struct {
    points_writer writer;
    uint32_t domain;
    unsigned pending_action;
    bool active, terminal;
    bool (*alive)(void *context);
    void *alive_context;
    /* Keep scratch and retired allocations reachable if a callback fences. */
    points_catalog staged, retired;
    uint32_t *staged_order, *retired_order;
} points_scene_legacy;

static inline void psl_set_alive(points_scene_legacy *l,bool (*alive)(void *),void *context) {
    if(!l||l->terminal)return;
    l->alive=alive;l->alive_context=context;
}
static inline bool psl_guard(points_scene_legacy *l,points_editor *e) {
    if(!l||!e)return false;
    if(l->terminal||e->store.retained||(l->alive&&!l->alive(l->alive_context))) {
        l->terminal=true;e->store.retained=true;e->error=RISC_APP_DATA_RETAINED;
        return false;
    }
    return true;
}
static inline int32_t psl_result(int32_t rc) {
    if(rc==ALARM_OK)return RISC_APP_DATA_OK;
    if(rc==ALARM_BUSY)return RISC_APP_DATA_BUSY;
    if(rc==ALARM_EXHAUSTED)return RISC_APP_DATA_NO_SPACE;
    if(rc==ALARM_INVALID)return RISC_APP_DATA_INVALID;
    return RISC_APP_DATA_IO;
}
typedef struct {
    points_scene_legacy *owner;
    points_editor *editor;
    risc_key_value_v1 api; /* Never dereference a revoked provider table. */
    int32_t last_result;
} psl_io;
static inline int32_t psl_io_result(psl_io *io,int32_t rc) {
    if(rc==RISC_KEY_VALUE_CONTEXT)io->owner->terminal=true;
    if(!psl_guard(io->owner,io->editor))return RISC_KEY_VALUE_CONTEXT;
    io->last_result=rc;return rc;
}
static inline int32_t psl_get(void *context,const char *key,void *bytes,uint32_t capacity,uint32_t *size) {
    psl_io *io=context;
    if(!psl_guard(io->owner,io->editor))return RISC_KEY_VALUE_CONTEXT;
    int32_t rc=psl_io_result(io,io->api.get(io->api.context,key,bytes,capacity,size));
    if(rc==RISC_KEY_VALUE_OK&&!strcmp(key,POINTS_META_KEY)) {
        points_meta decoded;
        if(!points_editor_legacy_meta_decode(&decoded,bytes,*size))return io->last_result=RISC_KEY_VALUE_IO;
    }
    return rc;
}
static inline int32_t psl_put(void *context,const char *key,const void *bytes,uint32_t size) {
    psl_io *io=context;
    if(!psl_guard(io->owner,io->editor))return RISC_KEY_VALUE_CONTEXT;
    return psl_io_result(io,io->api.put(io->api.context,key,bytes,size));
}
static inline risc_key_value_v1 psl_storage(psl_io *io,points_scene_legacy *l,points_editor *e,const risc_key_value_v1 *kv) {
    *io=(psl_io){.owner=l,.editor=e,.api=*kv};
    return (risc_key_value_v1){RISC_KEY_VALUE_API_V1,sizeof(risc_key_value_v1),io,psl_get,psl_put};
}
/* Detach before entering free: a revoking callback cannot leave a reachable
 * pointer that a later disposer could free a second time. */
static inline bool psl_discard(points_scene_legacy *l,points_editor *e,points_catalog *c,uint32_t **order) {
    if(!psl_guard(l,e))return false;
    if(c->events){void *p=c->events;c->events=NULL;POINTS_CATALOG_FREE(p);if(!psl_guard(l,e))return false;}
    if(c->types){void *p=c->types;c->types=NULL;POINTS_CATALOG_FREE(p);if(!psl_guard(l,e))return false;}
    if(*order){void *p=*order;*order=NULL;POINTS_CATALOG_FREE(p);if(!psl_guard(l,e))return false;}
    memset(c,0,sizeof(*c));return true;
}
static inline int32_t psl_project_error(points_scene_legacy *l,points_editor *e,int32_t rc) {
    if(!psl_discard(l,e,&l->staged,&l->staged_order))return RISC_APP_DATA_RETAINED;
    e->store.loaded=false;e->index_ready=false;return e->error=rc;
}
static inline int32_t psl_project(points_scene_legacy *l,points_editor *e) {
    if(!psl_guard(l,e))return RISC_APP_DATA_RETAINED;
    const points_config *old=&l->writer.saved;const points_meta *meta=&l->writer.meta;
    if(!points_config_valid(old)||!points_meta_valid(meta)||l->domain>POINTS_TIME_NATIVE_UTC)
        return psl_project_error(l,e,RISC_APP_DATA_INVALID);
    /* Match points_catalog_migrate, but fence each allocator callback. Do not
     * call the unfenced multi-allocation helper from this lifecycle adapter. */
    static const char *const names[]={"Work","Work End","Lunch","Break","Bedtime"};
    static const uint32_t colors[]={0x3d9bffu,0xff3d71u,0xffb020u,0x3dff9au,0x6d7bffu};
    static const uint8_t symbols[]={1,5,3,2,7};
    static const uint32_t custom_colors[]={0xffd24au,0xff7a1au,0xff3d71u,0xb24dffu,0x6d7bffu,0x3d9bffu,0x19e3ffu,0x3dff9au};
    points_catalog *c=&l->staged;
    *c=(points_catalog){.revision=old->revision,.next_event_id=POINTS_MAX+1,.next_type_id=8,.type_count=7,.time_domain=l->domain};
    for(unsigned i=0;i<POINTS_MAX;i++)if(old->points[i].kind)++c->event_count;
    c->types=POINTS_CATALOG_ALLOC(c->type_count*sizeof(*c->types));
    if(!psl_guard(l,e))return RISC_APP_DATA_RETAINED;
    if(!c->types)return psl_project_error(l,e,POINTS_STORAGE_MEMORY);
    memset(c->types,0,c->type_count*sizeof(*c->types));
    for(unsigned i=0;i<5;i++) {
        c->types[i]=(points_catalog_type){.id=i+1,.revision=1,.color=colors[i],.symbol=symbols[i],.flags=(i==2||i==3)?POINTS_TYPE_DURATION:0};
        memcpy(c->types[i].name,names[i],strlen(names[i])+1);
    }
    for(unsigned i=0;i<POINTS_CUSTOM_COUNT;i++) {
        const points_custom_type *m=&meta->custom[i];
        c->types[5+i]=(points_catalog_type){.id=6+i,.revision=meta->revision?meta->revision:1,
            .color=custom_colors[m->color],.symbol=m->color,.flags=POINTS_TYPE_DURATION};
        if(m->name[0])memcpy(c->types[5+i].name,m->name,sizeof(m->name));
        else memcpy(c->types[5+i].name,i?"Custom 2":"Custom 1",9);
    }
    if(c->event_count) {
        c->events=POINTS_CATALOG_ALLOC(c->event_count*sizeof(*c->events));
        if(!psl_guard(l,e))return RISC_APP_DATA_RETAINED;
        if(!c->events)return psl_project_error(l,e,POINTS_STORAGE_MEMORY);
        l->staged_order=POINTS_CATALOG_ALLOC(c->event_count*sizeof(*l->staged_order));
        if(!psl_guard(l,e))return RISC_APP_DATA_RETAINED;
        if(!l->staged_order)return psl_project_error(l,e,POINTS_STORAGE_MEMORY);
    }
    uint32_t at=0;
    for(unsigned i=0;i<POINTS_MAX;i++)if(old->points[i].kind) {
        const points_item *p=&old->points[i];
        c->events[at++]=(points_catalog_item){.id=i+1,.type_id=p->kind,.revision=old->revision,.created=old->created,
            .duration_minutes=p->duration_minutes,.mode=p->mode,.weekdays=p->weekdays,.hour=p->hour,.minute=p->minute,
            .enabled=p->enabled,.notify_end=p->notify_end,.warn3=p->warn3};
    }
    if(!points_catalog_valid(c))return psl_project_error(l,e,RISC_APP_DATA_INVALID);
    for(uint32_t i=0;i<c->event_count;i++)l->staged_order[i]=i;
    for(uint32_t i=c->event_count/2;i>0;i--)points_editor_sift(c,l->staged_order,i-1,c->event_count);
    for(uint32_t n=c->event_count;n>1;n--) {
        uint32_t tmp=l->staged_order[0];l->staged_order[0]=l->staged_order[n-1];l->staged_order[n-1]=tmp;
        points_editor_sift(c,l->staged_order,0,n-1);
    }
    l->retired=e->store.saved;l->retired_order=e->order;
    e->store.saved=*c;e->order=l->staged_order;e->order_count=c->event_count;
    *c=(points_catalog){0};l->staged_order=NULL;
    e->store.loaded=true;e->store.uncertain=false;e->store.missing=false;e->conflict=false;e->index_ready=true;
    if(!psl_discard(l,e,&l->retired,&l->retired_order))return RISC_APP_DATA_RETAINED;
    return e->error=RISC_APP_DATA_OK;
}
static inline int32_t psl_load(points_scene_legacy *l,points_editor *e,const risc_key_value_v1 *kv,uint32_t domain) {
    if(!l||!e)return RISC_APP_DATA_INVALID;
    if(!psl_guard(l,e))return RISC_APP_DATA_RETAINED;
    if(l->writer.uncertain||l->writer.meta_uncertain)return e->error=RISC_APP_DATA_COMMIT_UNKNOWN;
    if(domain>POINTS_TIME_NATIVE_UTC||!points_writer_storage_valid(kv))return e->error=RISC_APP_DATA_INVALID;
    points_writer candidate={0};psl_io io;risc_key_value_v1 guarded=psl_storage(&io,l,e,kv);
    int32_t rc=points_writer_load(&candidate,&guarded);
    if(!psl_guard(l,e))return RISC_APP_DATA_RETAINED;
    if(rc!=ALARM_OK) {
        l->active=false;e->store.loaded=false;e->index_ready=false;
        return e->error=io.last_result==RISC_KEY_VALUE_BUSY?RISC_APP_DATA_BUSY:psl_result(rc);
    }
    l->writer=candidate;l->domain=domain;l->active=true;
    return psl_project(l,e);
}
static inline bool psl_same_config(const points_config *a,const points_config *b) {
    uint8_t x[POINTS_RECORD_SIZE],y[POINTS_RECORD_SIZE];points_config_encode(a,x);points_config_encode(b,y);return !memcmp(x,y,sizeof(x));
}
static inline bool psl_same_meta(const points_meta *a,const points_meta *b) {
    uint8_t x[POINTS_RECORD_SIZE],y[POINTS_RECORD_SIZE];points_meta_encode(a,x);points_meta_encode(b,y);return !memcmp(x,y,sizeof(x));
}
static inline int32_t psl_finish(points_scene_legacy *l,points_editor *e,int32_t rc) {
    if(!psl_guard(l,e))return RISC_APP_DATA_RETAINED;
    e->store.uncertain=l->writer.uncertain||l->writer.meta_uncertain;
    if(e->store.uncertain)return e->error=RISC_APP_DATA_COMMIT_UNKNOWN;
    if(rc!=ALARM_OK)return e->error=psl_result(rc);
    unsigned action=l->pending_action;l->pending_action=0;
    if(action==POINTS_EDITOR_EVENT||action==POINTS_EDITOR_DELETE){e->event.id=e->pending_id;e->editing_event=false;}
    else if(action==POINTS_EDITOR_TYPE)e->editing_type=false;
    e->pending_action=0;e->pending_id=0;
    return psl_project(l,e);
}
static inline int32_t psl_save(points_scene_legacy *l,points_editor *e,const risc_key_value_v1 *kv,uint32_t now,unsigned action) {
    if(!l||!e)return RISC_APP_DATA_INVALID;
    if(!psl_guard(l,e))return RISC_APP_DATA_RETAINED;
    if(!l->active||!points_editor_editable(e)||!points_writer_storage_valid(kv)||l->writer.uncertain||l->writer.meta_uncertain)
        return e->error=RISC_APP_DATA_INVALID;
    points_config desired=l->writer.saved;points_meta desired_meta=l->writer.meta;uint32_t id=0;
    if(action==POINTS_EDITOR_TYPE) {
        /* Legacy metadata has no duration/mode/default-alert fields. Refuse
         * unsupported mutations instead of reporting that they were saved. */
        const points_catalog_type *base=points_catalog_find_type(&e->store.saved,e->type.id);
        if(!e->editing_type||e->type.id<POINTS_CUSTOM_1||e->type.id>POINTS_CUSTOM_2||!base||
           !e->type.name[0]||!points_custom_name_valid(e->type.name)||
           e->type.duration_minutes!=base->duration_minutes||e->type.mode!=base->mode||e->type.flags!=base->flags||e->type.symbol!=base->symbol)
            return e->error=RISC_APP_DATA_INVALID;
        static const uint32_t colors[]={0xffd24a,0xff7a1a,0xff3d71,0xb24dff,0x6d7bff,0x3d9bff,0x19e3ff,0x3dff9a};
        unsigned color=POINTS_COLOR_COUNT;
        for(unsigned i=0;i<POINTS_COLOR_COUNT;i++)if(colors[i]==e->type.color)color=i;
        if(color==POINTS_COLOR_COUNT)return e->error=RISC_APP_DATA_INVALID;
        if(desired_meta.revision==UINT32_MAX)return e->error=RISC_APP_DATA_NO_SPACE;
        desired_meta.revision++;id=e->type.id;unsigned slot=id-POINTS_CUSTOM_1;
        memset(&desired_meta.custom[slot],0,sizeof(desired_meta.custom[slot]));desired_meta.custom[slot].color=(uint8_t)color;
        memcpy(desired_meta.custom[slot].name,e->type.name,strlen(e->type.name)+1);
    } else {
        if(action!=POINTS_EDITOR_EVENT&&action!=POINTS_EDITOR_DELETE)return e->error=RISC_APP_DATA_UNAVAILABLE;
        if(!e->editing_event||now>ALARM_RTC_MAX-ALARM_RECOVERY_SECONDS)return e->error=RISC_APP_DATA_INVALID;
        if(desired.revision==UINT32_MAX)return e->error=RISC_APP_DATA_NO_SPACE;
        unsigned slot=POINTS_MAX;
        if(e->event.id) {
            if(e->event.id>POINTS_MAX||!desired.points[e->event.id-1].kind)return e->error=RISC_APP_DATA_INVALID;
            slot=e->event.id-1;
        } else if(action==POINTS_EDITOR_EVENT) {
            for(unsigned i=0;i<POINTS_MAX;i++)if(!desired.points[i].kind){slot=i;break;}
        } else return e->error=RISC_APP_DATA_INVALID;
        if(slot==POINTS_MAX)return e->error=RISC_APP_DATA_NO_SPACE;
        desired.revision++;desired.created=now;id=slot+1;
        if(action==POINTS_EDITOR_DELETE)desired.points[slot]=(points_item){0};
        else {
            if(e->event.type_id<POINTS_WORK_START||e->event.type_id>POINTS_CUSTOM_2)return e->error=RISC_APP_DATA_INVALID;
            desired.points[slot]=(points_item){(uint8_t)e->event.type_id,e->event.enabled,e->event.mode,e->event.weekdays,
                e->event.hour,e->event.minute,e->event.duration_minutes,e->event.notify_end,e->event.warn3};
        }
        if(!points_config_valid(&desired))return e->error=RISC_APP_DATA_INVALID;
    }
    /* KV has no CAS. Re-read immediately before the exact-write/readback
     * protocol, but do not claim exclusion of a writer racing that interval. */
    psl_io io;risc_key_value_v1 guarded=psl_storage(&io,l,e,kv);points_writer observed={0};
    int32_t rc=points_writer_load(&observed,&guarded);
    if(!psl_guard(l,e))return RISC_APP_DATA_RETAINED;
    if(rc!=ALARM_OK)return e->error=io.last_result==RISC_KEY_VALUE_BUSY?RISC_APP_DATA_BUSY:psl_result(rc);
    if(!psl_same_config(&observed.saved,&l->writer.saved)||!psl_same_meta(&observed.meta,&l->writer.meta)||
       observed.default_meta!=l->writer.default_meta) {
        e->conflict=true;return e->error=RISC_APP_DATA_STALE;
    }
    points_writer candidate=l->writer;
    l->pending_action=action;e->pending_action=(uint8_t)action;e->pending_id=id;
    rc=action==POINTS_EDITOR_TYPE?points_writer_save_meta(&candidate,&guarded,&desired_meta):points_writer_save(&candidate,&guarded,&desired);
    if(!psl_guard(l,e))return RISC_APP_DATA_RETAINED;
    l->writer=candidate;return psl_finish(l,e,rc);
}
static inline int32_t psl_retry(points_scene_legacy *l,points_editor *e,const risc_key_value_v1 *kv) {
    if(!l||!e)return RISC_APP_DATA_INVALID;
    if(!psl_guard(l,e))return RISC_APP_DATA_RETAINED;
    if(!l->active||!points_writer_storage_valid(kv)||(!l->writer.uncertain&&!l->writer.meta_uncertain))return e->error=RISC_APP_DATA_INVALID;
    psl_io io;risc_key_value_v1 guarded=psl_storage(&io,l,e,kv);points_writer candidate=l->writer;
    int32_t rc=ALARM_OK;
    if(candidate.meta_uncertain)rc=points_writer_retry_meta(&candidate,&guarded);
    if(!psl_guard(l,e))return RISC_APP_DATA_RETAINED;
    if(rc==ALARM_OK&&candidate.uncertain)rc=points_writer_retry(&candidate,&guarded);
    if(!psl_guard(l,e))return RISC_APP_DATA_RETAINED;
    l->writer=candidate;return psl_finish(l,e,rc);
}
#endif
