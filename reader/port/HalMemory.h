#pragma once
#include "ReaderPort.h"
namespace HalMemory {
struct HeapInfo {size_t freeBytes,largestBlockBytes,totalBytes;};
inline HeapInfo getHeap(unsigned kind){risc_memory_heap_snapshot_v1 s{};if(!reader::heapSnapshot(kind,&s))return {};return {size_t(s.free_bytes),size_t(s.largest_block),size_t(s.total_bytes)};}
inline HeapInfo getDefaultHeap(){return getHeap(RISC_MEMORY_DEFAULT);}
inline HeapInfo getPsramHeap(){return getHeap(RISC_MEMORY_EXTERNAL);}
}
