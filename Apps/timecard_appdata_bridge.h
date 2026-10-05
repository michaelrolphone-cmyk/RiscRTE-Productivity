#pragma once
/* App-local facade over the reviewed app-data prototype. It maps exactly the
 * original Timecard file, never a caller-selected namespace or native path.
 * The Runtime header must come from the exact separately recorded SDK source. */
#include "RiscAppDataV1.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#define TCP_APPDATA_PATH "/sd/.crosspoint/timecard.json"
#define TCP_APPDATA_NAME "timecard.json"
#define TCP_APPDATA_MAX 49151u
typedef struct {
    const risc_app_data_v1 *api;
    uint64_t revision;
    uint32_t size;
    int32_t status;
    bool ready,missing,retained;
} tcp_appdata;
static inline bool tcp_appdata_error(tcp_appdata *state,int32_t result) {
    state->status=result;state->ready=false;state->missing=false;
    if(result==RISC_APP_DATA_RETAINED)state->retained=true;
    return false;
}
static inline bool tcp_appdata_bind(tcp_appdata *state,const risc_app_data_v1 *api) {
    if(!state || state->retained)return false;
    *state=(tcp_appdata){.status=RISC_APP_DATA_UNAVAILABLE};
    if(!api || api->api_version!=RISC_APP_DATA_API_V1 || api->struct_size<sizeof(*api) || !api->stat || !api->read || !api->replace)return false;
    state->api=api;return true;
}
static inline bool tcp_appdata_path(tcp_appdata *state,const char *path) {
    if(!state)return false;
    if(state->retained)return false;
    if(!state->api)return tcp_appdata_error(state,RISC_APP_DATA_UNAVAILABLE);
    if(!path || strcmp(path,TCP_APPDATA_PATH))return tcp_appdata_error(state,RISC_APP_DATA_INVALID);
    return true;
}
/* T5 exists has no error channel. Report false ONLY for confirmed absence;
 * an error must lead the source loader to a failed read, never an empty history. */
static inline bool tcp_appdata_exists(tcp_appdata *state,const char *path) {
    if(!tcp_appdata_path(state,path))return true;
    uint32_t size=0;uint64_t revision=0;
    int32_t result=state->api->stat(state->api->context,TCP_APPDATA_NAME,&size,&revision);
    if(result==RISC_APP_DATA_NOT_FOUND && !size && !revision) {
        state->revision=0;state->size=0;state->status=result;state->ready=state->missing=true;return false;
    }
    if(result!=RISC_APP_DATA_OK || !revision || size>TCP_APPDATA_MAX) {
        tcp_appdata_error(state,result==RISC_APP_DATA_OK || result==RISC_APP_DATA_NOT_FOUND?RISC_APP_DATA_IO:result);return true;
    }
    state->revision=revision;state->size=size;state->status=RISC_APP_DATA_OK;state->ready=true;state->missing=false;return true;
}
static inline bool tcp_appdata_read(tcp_appdata *state,const char *path,void *buffer,size_t capacity,size_t *size_out) {
    if(size_out)*size_out=0;
    if(!tcp_appdata_path(state,path) || !state->ready)return false;
    if(!size_out || (!buffer && capacity) || capacity>UINT32_MAX)return tcp_appdata_error(state,RISC_APP_DATA_INVALID);
    if(state->missing){state->status=RISC_APP_DATA_NOT_FOUND;return false;}
    if(capacity<state->size){state->status=RISC_APP_DATA_BUFFER_SMALL;return false;}
    uint32_t got=0;uint64_t revision=0;
    int32_t result=state->api->read(state->api->context,TCP_APPDATA_NAME,state->revision,buffer,(uint32_t)capacity,&got,&revision);
    if(result!=RISC_APP_DATA_OK)return tcp_appdata_error(state,result);
    if(got!=state->size || got>capacity || revision!=state->revision)return tcp_appdata_error(state,RISC_APP_DATA_IO);
    *size_out=got;state->status=RISC_APP_DATA_OK;return true;
}
static inline bool tcp_appdata_write(tcp_appdata *state,const char *path,const void *data,size_t size) {
    if(!tcp_appdata_path(state,path) || !state->ready)return false;
    if(!data || !size || size>TCP_APPDATA_MAX)return tcp_appdata_error(state,RISC_APP_DATA_INVALID);
    int32_t result=state->api->replace(state->api->context,TCP_APPDATA_NAME,state->revision,data,(uint32_t)size);
    if(result!=RISC_APP_DATA_OK)return tcp_appdata_error(state,result);
    /* The service already verified the exact committed bytes. Refresh the token
     * for the next write; failure now is explicitly post-commit uncertainty. */
    uint32_t got=0;uint64_t revision=0;
    result=state->api->stat(state->api->context,TCP_APPDATA_NAME,&got,&revision);
    if(result!=RISC_APP_DATA_OK || !revision || got!=size)
        return tcp_appdata_error(state,result==RISC_APP_DATA_RETAINED?result:RISC_APP_DATA_COMMIT_UNKNOWN);
    state->size=got;state->revision=revision;state->status=RISC_APP_DATA_OK;state->missing=false;state->ready=true;return true;
}
