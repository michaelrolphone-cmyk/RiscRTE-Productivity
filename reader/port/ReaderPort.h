#pragma once
#include "RiscRuntimeV1.h"
#include "RiscStorageVolumeV1.h"
#include "RiscStorageVolumeFsV1.h"
#include "RiscMemoryHeapV1.h"
#include "RiscRandomV1.h"
#include <cstdint>
namespace reader {
constexpr const char* stateRoot="/System/State/Applications/ebook-reader";
void bind(const risc_runtime_api_v1*,const risc_storage_volume_api_v1*,const risc_memory_heap_api_v1* = nullptr,const risc_random_api_v1* = nullptr);
bool heapSnapshot(unsigned,risc_memory_heap_snapshot_v1*);
uint64_t heapBytes(unsigned memoryClass,bool largest);
void setTerminal(void(*)());
void setService(void(*)(void*),void*);
void service();
bool alive();
void retain();
bool failed();
void clearError();
const risc_storage_volume_api_v1_ext* volume();
const risc_runtime_api_v1* runtime();
}
