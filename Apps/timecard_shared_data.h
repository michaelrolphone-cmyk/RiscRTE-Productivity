#pragma once
/* Watch's app-data facade now shares the modal client's custody fence. Each
 * provider callback is checked before the ledger bridge can make another call.
 * The civil-day format, namespace, revisions and atomic write are unchanged. */
static risc_app_data_v1 tcp_checked_data;
static int32_t tcp_checked_data_result(int32_t result) {
    if(!portable_text_client_live(&tcp_text_client))return RISC_APP_DATA_RETAINED;
    switch(result) {
        case RISC_APP_DATA_OK:case RISC_APP_DATA_NOT_FOUND:case RISC_APP_DATA_BUFFER_SMALL:
        case RISC_APP_DATA_INVALID:case RISC_APP_DATA_UNAVAILABLE:case RISC_APP_DATA_IO:
        case RISC_APP_DATA_NO_SPACE:case RISC_APP_DATA_COMMIT_UNKNOWN:case RISC_APP_DATA_STALE:return result;
        default:(void)portable_text_client_retain(&tcp_text_client);return RISC_APP_DATA_RETAINED;
    }
}
static int32_t tcp_checked_stat(void *context,const char *name,uint32_t *size,uint64_t *revision) {
    if(!tcp_text_background_ready())return RISC_APP_DATA_RETAINED;
    const risc_app_data_v1 *source=context;
    return tcp_checked_data_result(source->stat(source->context,name,size,revision));
}
static int32_t tcp_checked_read(void *context,const char *name,uint64_t revision,void *data,uint32_t capacity,uint32_t *size,uint64_t *actual) {
    if(!tcp_text_background_ready())return RISC_APP_DATA_RETAINED;
    const risc_app_data_v1 *source=context;
    return tcp_checked_data_result(source->read(source->context,name,revision,data,capacity,size,actual));
}
static int32_t tcp_checked_replace(void *context,const char *name,uint64_t revision,const void *data,uint32_t size) {
    if(!tcp_text_background_ready())return RISC_APP_DATA_RETAINED;
    const risc_app_data_v1 *source=context;
    return tcp_checked_data_result(source->replace(source->context,name,revision,data,size));
}
static const risc_app_data_v1 *tcp_checked_bind(const risc_app_data_v1 *source) {
    if(!source || source->api_version!=1 || source->struct_size<sizeof(*source) || !source->stat || !source->read || !source->replace)return NULL;
    tcp_checked_data=(risc_app_data_v1){1,sizeof(tcp_checked_data),(void *)source,tcp_checked_stat,tcp_checked_read,tcp_checked_replace};
    return &tcp_checked_data;
}
