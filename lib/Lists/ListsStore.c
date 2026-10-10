#include "ListsStore.h"
#include <string.h>
static bool live(lists_store *s,int rc){return (!s->alive||s->alive(s->guard))&&rc!=RISC_APP_DATA_RETAINED&&rc!=RISC_APP_DATA_CONTEXT;}
int lists_store_load(lists_store *s,lists_model *m){
    s->ready=false;uint32_t size=0;uint64_t revision=0;
    int rc=s->api->stat(s->api->context,"lists.bin",&size,&revision);
    if(!live(s,rc))return LISTS_STORE_RETAINED;
    if(rc==RISC_APP_DATA_NOT_FOUND){lists_init(m);s->revision=0;s->ready=true;return LISTS_STORE_OK;}
    if(rc!=RISC_APP_DATA_OK)return LISTS_STORE_IO;
    if(size!=LISTS_WIRE_SIZE||!revision)return LISTS_STORE_CORRUPT;
    uint64_t read_revision=0;rc=s->api->read(s->api->context,"lists.bin",revision,s->wire,sizeof(s->wire),&size,&read_revision);
    if(!live(s,rc))return LISTS_STORE_RETAINED;
    if(rc!=RISC_APP_DATA_OK||revision!=read_revision)return LISTS_STORE_IO;
    if(!lists_decode(m,s->wire,size))return LISTS_STORE_CORRUPT;
    s->revision=read_revision;s->ready=true;return LISTS_STORE_OK;
}
int lists_store_save(lists_store *s,lists_model *m,const lists_model *candidate){
    if(!s->ready||!lists_encode(candidate,s->wire))return LISTS_STORE_IO;
    int rc=s->api->replace(s->api->context,"lists.bin",s->revision,s->wire,sizeof(s->wire));
    if(!live(s,rc)){s->ready=false;return LISTS_STORE_RETAINED;}
    /* All write outcomes consume our cached revision. Reload the authoritative
     * file even after success. STALE/UNKNOWN must never trigger an auto-retry. */
    int loaded=lists_store_load(s,m);if(loaded!=LISTS_STORE_OK)return loaded;
    if(rc==RISC_APP_DATA_OK||rc==RISC_APP_DATA_COMMIT_UNKNOWN){
        if(!lists_encode(candidate,s->wire))return LISTS_STORE_IO;
        uint8_t current[LISTS_WIRE_SIZE];if(lists_encode(m,current)&&!memcmp(current,s->wire,sizeof(current)))return LISTS_STORE_OK;
    }
    return LISTS_STORE_CHANGED;
}
