#ifndef PRODUCTIVITY_POINTS_CATALOG_CONTROLLER_H
#define PRODUCTIVITY_POINTS_CATALOG_CONTROLLER_H
/* Selected storage-scaled editor. The saved document is immutable while a
 * draft is open. Every mutation is made on a private clone and atomically
 * replaced with the observed storage revision. No legacy KV writes. */
#include "PointsCatalogStorage.h"
#include "RiscKeyValueV1.h"
#include <stdio.h>
typedef struct {
    points_catalog_storage store;
    points_catalog staged; /* Kept alive if the replace fences custody. */
    points_catalog_item event;
    points_catalog_type type;
    uint32_t *order, order_count, pending_id;
    uint8_t pending_action;
    bool editing_event, editing_type, conflict, index_ready;
    int32_t error;
} points_editor;
enum { POINTS_EDITOR_EVENT=1, POINTS_EDITOR_DELETE=2, POINTS_EDITOR_TYPE=3, POINTS_EDITOR_DELETE_TYPE=4 };
static inline void points_editor_dispose(points_editor *e) {
    if(!e||e->store.retained)return;
    points_catalog_storage_dispose(&e->store);points_catalog_dispose(&e->staged);POINTS_CATALOG_FREE(e->order);memset(e,0,sizeof(*e));
}
static inline bool points_editor_editable(const points_editor *e) {
    return e&&e->store.loaded&&e->index_ready&&!e->store.retained&&!e->store.uncertain&&!e->conflict;
}
static inline bool points_editor_order_less(const points_catalog *c,uint32_t a,uint32_t b) {
    const points_catalog_item *x=&c->events[a],*y=&c->events[b];
    unsigned xt=x->hour*60u+x->minute,yt=y->hour*60u+y->minute;
    return xt<yt||(xt==yt&&x->id<y->id);
}
static inline void points_editor_sift(const points_catalog *c,uint32_t *order,uint32_t root,uint32_t count) {
    while(root<count/2) {
        uint32_t child=root*2+1;
        if(child+1<count&&points_editor_order_less(c,order[child],order[child+1]))child++;
        if(!points_editor_order_less(c,order[root],order[child]))return;
        uint32_t tmp=order[root];order[root]=order[child];order[child]=tmp;root=child;
    }
}
static inline int32_t points_editor_reindex(points_editor *e) {
    uint32_t count=e->store.saved.event_count;
    uint32_t *order=count?POINTS_CATALOG_ALLOC((size_t)count*sizeof(*order)):NULL;
    if(count&&!order){POINTS_CATALOG_FREE(e->order);e->order=NULL;e->order_count=0;e->index_ready=false;return e->error=POINTS_STORAGE_MEMORY;}
    /* Iterative heapsort: one heap index per event, no count limit, recursion,
     * quadratic insertion pass, or large automatic bucket table. */
    for(uint32_t i=0;i<count;i++)order[i]=i;
    for(uint32_t i=count/2;i>0;i--)points_editor_sift(&e->store.saved,order,i-1,count);
    for(uint32_t n=count;n>1;n--) {
        uint32_t tmp=order[0];order[0]=order[n-1];order[n-1]=tmp;
        points_editor_sift(&e->store.saved,order,0,n-1);
    }
    POINTS_CATALOG_FREE(e->order);e->order=order;e->order_count=count;e->index_ready=true;return e->error=RISC_APP_DATA_OK;
}
static inline int32_t points_editor_load(points_editor *e,const risc_app_data_v1 *files,const risc_key_value_v1 *legacy,uint32_t domain) {
    if(!e||e->store.retained)return RISC_APP_DATA_RETAINED;
    if(e->store.uncertain)return RISC_APP_DATA_COMMIT_UNKNOWN;
    int32_t rc=points_catalog_storage_load(&e->store,files,domain);
    if(rc==RISC_APP_DATA_NOT_FOUND) {
        if(!legacy||legacy->api_version!=1||legacy->struct_size<sizeof(*legacy)||!legacy->get)return e->error=RISC_APP_DATA_INVALID;
        uint8_t bytes[POINTS_RECORD_SIZE];uint32_t n=0;points_config old={0};points_meta meta={0};
        rc=legacy->get(legacy->context,POINTS_CONFIG_KEY,bytes,sizeof(bytes),&n);
        bool fresh=rc==RISC_KEY_VALUE_NOT_FOUND;
        if(fresh)old=points_default_config();
        else if(rc!=RISC_KEY_VALUE_OK||!points_config_decode(&old,bytes,n))return e->error=RISC_APP_DATA_IO;
        n=0;rc=legacy->get(legacy->context,POINTS_META_KEY,bytes,sizeof(bytes),&n);
        if(rc==RISC_KEY_VALUE_NOT_FOUND)meta=fresh?points_default_meta():(points_meta){0};
        else if(rc!=RISC_KEY_VALUE_OK||!points_meta_decode(&meta,bytes,n))return e->error=RISC_APP_DATA_IO;
        rc=points_catalog_storage_migrate(&e->store,&old,&meta,domain);
        /* A missing file remains a virtual migration until explicit Save.
         * Cancellation and ordinary entry never persist factory defaults. */
    }
    if(rc!=RISC_APP_DATA_OK)return e->error=rc;
    e->conflict=false;return points_editor_reindex(e);
}
static inline const points_catalog_item *points_editor_at(const points_editor *e,uint32_t index) {
    return e&&index<e->order_count?&e->store.saved.events[e->order[index]]:NULL;
}
static inline bool points_editor_begin_event(points_editor *e,uint32_t id) {
    if(!points_editor_editable(e))return false;
    if(id){const points_catalog_item *found=points_catalog_find_event(&e->store.saved,id);if(!found)return false;e->event=*found;}
    else {
        if(!e->store.saved.type_count)return false;
        e->event=(points_catalog_item){.enabled=1,.weekdays=127,.hour=9};
        (void)points_catalog_apply_type(&e->event,&e->store.saved,e->store.saved.types[0].id);
    }
    e->editing_event=true;e->editing_type=false;e->error=0;return true;
}
static inline bool points_editor_begin_type(points_editor *e,uint32_t id) {
    if(!points_editor_editable(e))return false;
    if(id){const points_catalog_type *found=points_catalog_find_type(&e->store.saved,id);if(!found||id<=POINTS_BEDTIME)return false;e->type=*found;}
    else e->type=(points_catalog_type){.color=0x19e3ff,.flags=POINTS_TYPE_DURATION,.duration_minutes=15};
    e->editing_type=true;e->error=0;return true;
}
static inline void points_editor_cancel_event(points_editor *e) {
    if(!e||e->store.uncertain||e->store.retained)return;
    e->event=(points_catalog_item){0};e->type=(points_catalog_type){0};e->editing_event=e->editing_type=false;
}
static inline void points_editor_cancel_type(points_editor *e) {
    if(!e||e->store.uncertain||e->store.retained)return;
    e->type=(points_catalog_type){0};e->editing_type=false;
}
static inline int32_t points_editor_publish(points_editor *e,const risc_app_data_v1 *files,points_catalog *desired,uint8_t action,uint32_t id) {
    e->pending_action=action;e->pending_id=id;
    e->staged=*desired;memset(desired,0,sizeof(*desired));
    int32_t rc=points_catalog_storage_save(&e->store,files,&e->staged);
    if(e->store.retained)return e->error=rc;
    points_catalog_dispose(&e->staged);
    if(rc==RISC_APP_DATA_OK) {
        if(action==POINTS_EDITOR_TYPE){e->type.id=id;e->editing_type=false;}
        else if(action==POINTS_EDITOR_DELETE_TYPE)e->editing_type=false;
        else {e->event.id=id;e->editing_event=false;}
        e->pending_action=0;(void)points_editor_reindex(e);
    } else if(rc==RISC_APP_DATA_STALE)e->conflict=true;
    if(e->index_ready)e->error=rc;
    return rc;
}
static inline int32_t points_editor_save_event(points_editor *e,const risc_app_data_v1 *files,uint32_t now,bool remove) {
    if(!points_editor_editable(e)||!e->editing_event)return RISC_APP_DATA_INVALID;
    if(now>ALARM_RTC_MAX-ALARM_RECOVERY_SECONDS)return e->error=RISC_APP_DATA_INVALID;
    points_catalog c={0};int rc=points_catalog_clone(&c,&e->store.saved);uint32_t id=e->event.id;
    if(rc!=POINTS_CATALOG_OK)return e->error=POINTS_STORAGE_MEMORY;
    points_catalog_item draft=e->event;draft.created=now;
    if(remove)rc=points_catalog_delete_event(&c,id);
    else if(id)rc=points_catalog_update_event(&c,&draft);
    else rc=points_catalog_add_event(&c,&draft,&id);
    if(rc!=POINTS_CATALOG_OK){points_catalog_dispose(&c);return e->error=rc==POINTS_CATALOG_MEMORY?POINTS_STORAGE_MEMORY:RISC_APP_DATA_INVALID;}
    return points_editor_publish(e,files,&c,remove?POINTS_EDITOR_DELETE:POINTS_EDITOR_EVENT,id);
}
static inline int32_t points_editor_save_type(points_editor *e,const risc_app_data_v1 *files,bool remove) {
    if(!points_editor_editable(e)||!e->editing_type)return RISC_APP_DATA_INVALID;
    points_catalog c={0};int rc=points_catalog_clone(&c,&e->store.saved);uint32_t id=e->type.id;
    if(rc!=POINTS_CATALOG_OK)return e->error=POINTS_STORAGE_MEMORY;
    rc=remove?points_catalog_delete_type(&c,id):points_catalog_save_type(&c,&e->type,&id);
    if(rc!=POINTS_CATALOG_OK){points_catalog_dispose(&c);return e->error=rc==POINTS_CATALOG_MEMORY?POINTS_STORAGE_MEMORY:rc==POINTS_CATALOG_IN_USE?RISC_APP_DATA_UNAVAILABLE:RISC_APP_DATA_INVALID;}
    return points_editor_publish(e,files,&c,remove?POINTS_EDITOR_DELETE_TYPE:POINTS_EDITOR_TYPE,id);
}
static inline int32_t points_editor_resolve(points_editor *e,const risc_app_data_v1 *files) {
    if(!e||e->store.retained)return RISC_APP_DATA_RETAINED;
    uint8_t action=e->pending_action;uint32_t id=e->pending_id;
    int32_t rc=points_catalog_storage_resolve(&e->store,files);
    if(!e->store.uncertain) {
        if(rc==RISC_APP_DATA_OK) {
            if(action==POINTS_EDITOR_TYPE){e->type.id=id;e->editing_type=false;}
            else if(action==POINTS_EDITOR_DELETE_TYPE)e->editing_type=false;
            else {e->event.id=id;e->editing_event=false;}
        }
        e->pending_action=0;(void)points_editor_reindex(e);
        /* Resolution already adopted the complete conflicting document. The
         * next explicit Save can rebase the still-visible draft on that base. */
        e->conflict=false;
    }
    if(e->index_ready)e->error=rc;
    return rc;
}
static inline void points_editor_time(const points_catalog_item *e,unsigned format,char *out,size_t n) {
    if(format==1)snprintf(out,n,"%u:%02u",e->hour,e->minute);
    else snprintf(out,n,"%u:%02u %s",e->hour%12?e->hour%12:12,e->minute,e->hour<12?"AM":"PM");
}
static inline void points_editor_days(unsigned mask,char *out,size_t n) {
    if(mask==127)snprintf(out,n,"Every day");
    else if(mask==62)snprintf(out,n,"Mon-Fri");
    else if(mask==65)snprintf(out,n,"Weekends");
    else if(!mask)snprintf(out,n,"Choose days");
    else snprintf(out,n,"%c %c %c %c %c %c %c",mask&1?'S':'-',mask&2?'M':'-',mask&4?'T':'-',mask&8?'W':'-',mask&16?'T':'-',mask&32?'F':'-',mask&64?'S':'-');
}
#endif
