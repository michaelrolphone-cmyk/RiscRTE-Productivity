#pragma once
/* Native clock profile only. Timecard remains a civil-date/minutes ledger:
 * no UTC history, guessed offsets, DST inversion or storage migration. The
 * shared source projects a fresh UTC sample into the selected zone per read.
 * All app-owned external grants and file/prefs calls share the adapter fence. */
static risc_app_data_v1 tcp_native_data;
static bool tcp_native_release(risc_runtime_capability_v1 *grant) {
    if(tcp_retained())return false;
    risc_runtime_capability_v1 released=*grant;
    bool ok=tcp_runtime->release(&released);
    if(tcp_retained())return false;
    if(!ok || released.struct_size!=sizeof(released) || released.api || released.slot || released.generation) {
        portable_adapter_retain();return false;
    }
    *grant=released;return true;
}
static const void *tcp_native_acquire(const char *name,uint32_t api,uint64_t instance) {
    if(tcp_retained())return NULL;
    risc_runtime_capability_v1 *grant=&tcp_grants[tcp_grant_count];
    *grant=(risc_runtime_capability_v1){.struct_size=sizeof(*grant)};
    bool ok=tcp_runtime->acquire(name,api,instance,grant);
    if(tcp_retained())return NULL;
    /* Boolean external-provider failure cannot establish clean rollback. */
    if(!ok || grant->struct_size!=sizeof(*grant) || !grant->api || !grant->slot || !grant->generation) {
        portable_adapter_retain();return NULL;
    }
    ++tcp_grant_count;return grant->api;
}
static int32_t tcp_native_get(void *context,const char *key,void *data,uint32_t size,uint32_t *used) {
    if(tcp_retained())return RISC_KEY_VALUE_CONTEXT;
#ifdef PORTABLE_BLE_BROADCAST
    if(!portable_broadcast_stop()){portable_adapter_retain();return RISC_KEY_VALUE_CONTEXT;}
#endif
    const risc_key_value_v1 *source=context;
    int32_t result=source->get(source->context,key,data,size,used);
    if(tcp_retained())return RISC_KEY_VALUE_CONTEXT;
    switch(result) {
        case RISC_KEY_VALUE_OK:case RISC_KEY_VALUE_NOT_FOUND:case RISC_KEY_VALUE_BUFFER_SMALL:
        case RISC_KEY_VALUE_INVALID:case RISC_KEY_VALUE_IO:return result;
        default:portable_adapter_retain();return RISC_KEY_VALUE_CONTEXT;
    }
}
static int32_t tcp_native_data_status(int32_t result) {
    if(tcp_retained())return RISC_APP_DATA_RETAINED;
    switch(result) {
        case RISC_APP_DATA_OK:case RISC_APP_DATA_NOT_FOUND:case RISC_APP_DATA_BUFFER_SMALL:
        case RISC_APP_DATA_INVALID:case RISC_APP_DATA_UNAVAILABLE:case RISC_APP_DATA_IO:
        case RISC_APP_DATA_NO_SPACE:case RISC_APP_DATA_COMMIT_UNKNOWN:case RISC_APP_DATA_STALE:return result;
        default:portable_adapter_retain();return RISC_APP_DATA_RETAINED;
    }
}
static int32_t tcp_native_stat(void *context,const char *name,uint32_t *size,uint64_t *revision) {
    if(tcp_retained())return RISC_APP_DATA_RETAINED;
    const risc_app_data_v1 *source=context;
    return tcp_native_data_status(source->stat(source->context,name,size,revision));
}
static int32_t tcp_native_read(void *context,const char *name,uint64_t revision,void *data,uint32_t size,uint32_t *used,uint64_t *actual) {
    if(tcp_retained())return RISC_APP_DATA_RETAINED;
    const risc_app_data_v1 *source=context;
    return tcp_native_data_status(source->read(source->context,name,revision,data,size,used,actual));
}
static int32_t tcp_native_replace(void *context,const char *name,uint64_t revision,const void *data,uint32_t size) {
    if(tcp_retained())return RISC_APP_DATA_RETAINED;
    const risc_app_data_v1 *source=context;
    return tcp_native_data_status(source->replace(source->context,name,revision,data,size));
}
static void tcp_native_open(void) {
    if(tcp_retained())return;
    tcp_runtime=risc_runtime_get_api(1);tcp_grant_count=0;tcp_time_format=PORTABLE_TIME_FORMAT_12;tcp_rtc=NULL;
    tcp_data=(tcp_appdata){0};
    if(!tcp_runtime || tcp_runtime->api_version!=1 ||
       tcp_runtime->struct_size<RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE ||
       !tcp_runtime->acquire || !tcp_runtime->release || !tcp_runtime->request_launch || !tcp_runtime->retain_invocation) {
        tcp_runtime=NULL;return;
    }
    const risc_key_value_v1 *prefs=tcp_native_acquire(RISC_KEY_VALUE_CAPABILITY,1,1);
    if(tcp_retained())return;
    if(prefs && prefs->api_version==1 && prefs->struct_size>=sizeof(*prefs) && prefs->get) {
        const risc_key_value_v1 readonly={.api_version=1,.struct_size=sizeof(readonly),
            .context=(void *)prefs,.get=tcp_native_get};
        portable_time_format_load(&readonly,&tcp_time_format);
    }
    if(tcp_retained())return;
    const risc_app_data_v1 *files=tcp_native_acquire(RISC_APP_DATA_CAPABILITY,1,TIMECARD_APP_DATA_INSTANCE);
    if(tcp_retained())return;
    if(files && files->api_version==1 && files->struct_size>=sizeof(*files) && files->stat && files->read && files->replace) {
        tcp_native_data=(risc_app_data_v1){1,sizeof(tcp_native_data),(void *)files,
            tcp_native_stat,tcp_native_read,tcp_native_replace};
        (void)tcp_bind_data(&tcp_native_data,tcp_runtime);
    }
}
