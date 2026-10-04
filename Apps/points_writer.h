#ifndef PRODUCTIVITY_POINTS_WRITER_H
#define PRODUCTIVITY_POINTS_WRITER_H
/* App-owned namespace-5 writer. Never reads or changes service occurrences. */
#include "PointsRecords.h"
#include "RiscKeyValueV1.h"
typedef struct {
    points_config saved;
    uint8_t pending[POINTS_RECORD_SIZE];
    bool loaded, uncertain;
    int32_t error;
} points_writer;
static inline bool points_writer_storage_valid(const risc_key_value_v1 *kv) {
    return kv && kv->api_version==RISC_KEY_VALUE_API_V1 && kv->struct_size>=sizeof(*kv) && kv->get && kv->put;
}
static inline int32_t points_writer_load(points_writer *w,const risc_key_value_v1 *kv) {
    if(!w || !points_writer_storage_valid(kv))return ALARM_INVALID;
    /* An uncertain put may have committed. Only retry the identical bytes. */
    if(w->uncertain)return ALARM_BUSY;
    uint8_t bytes[POINTS_RECORD_SIZE];uint32_t size=0;
    int32_t rc=kv->get(kv->context,POINTS_CONFIG_KEY,bytes,sizeof(bytes),&size);
    if(rc==RISC_KEY_VALUE_NOT_FOUND) {
        w->saved=(points_config){0};w->loaded=true;w->error=ALARM_OK;return ALARM_OK;
    }
    points_config decoded;
    if(rc!=RISC_KEY_VALUE_OK || !points_config_decode(&decoded,bytes,size)) {
        w->loaded=false;w->error=ALARM_STORAGE;return w->error;
    }
    w->saved=decoded;w->loaded=true;w->error=ALARM_OK;return ALARM_OK;
}
static inline int32_t points_writer_retry(points_writer *w,const risc_key_value_v1 *kv) {
    if(!w || !w->loaded || !w->uncertain || !points_writer_storage_valid(kv))return ALARM_INVALID;
    uint8_t actual[POINTS_RECORD_SIZE];uint32_t size=0;
    /* put's error does not prove rollback. Exact readback determines success. */
    (void)kv->put(kv->context,POINTS_CONFIG_KEY,w->pending,sizeof(w->pending));
    int32_t rc=kv->get(kv->context,POINTS_CONFIG_KEY,actual,sizeof(actual),&size);
    points_config decoded;
    if(rc!=RISC_KEY_VALUE_OK || size!=sizeof(actual) || memcmp(actual,w->pending,sizeof(actual)) ||
       !points_config_decode(&decoded,actual,size)) {
        w->error=ALARM_STORAGE;return w->error;
    }
    w->saved=decoded;w->uncertain=false;w->error=ALARM_OK;return ALARM_OK;
}
static inline int32_t points_writer_save(points_writer *w,const risc_key_value_v1 *kv,const points_config *desired) {
    if(!w || !w->loaded || !points_writer_storage_valid(kv))return ALARM_INVALID;
    if(w->uncertain)return ALARM_BUSY;
    if(!points_config_valid(desired))return ALARM_INVALID;
    if(w->saved.revision==UINT32_MAX || desired->revision!=w->saved.revision+1)return ALARM_EXHAUSTED;
    points_config_encode(desired,w->pending);w->uncertain=true;
    return points_writer_retry(w,kv);
}
#endif
