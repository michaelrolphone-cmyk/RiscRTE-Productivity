#pragma once
#include "ListsModel.h"
#include "RiscAppDataV1.h"
enum {LISTS_STORE_OK=0,LISTS_STORE_IO=-1,LISTS_STORE_CORRUPT=-2,LISTS_STORE_CHANGED=-3,LISTS_STORE_RETAINED=-9};
typedef struct {
    const risc_app_data_v1 *api;
    bool (*alive)(void *);void *guard;
    uint64_t revision;bool ready;
    uint8_t wire[LISTS_WIRE_SIZE];
} lists_store;
int lists_store_load(lists_store *,lists_model *);
/* Confirmed replacement or reload-on-uncertainty. Never retries a mutation. */
int lists_store_save(lists_store *,lists_model *current,const lists_model *candidate);
