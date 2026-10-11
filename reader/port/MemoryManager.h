#pragma once
#include "Arduino.h"
#include <functional>
#include <array>
namespace freeink {
enum class MemPool:uint8_t {Internal,Psram,Default};
struct CacheSink {const char* name=nullptr;uint8_t priority=128;std::function<size_t(size_t)> evict;};
class MemoryManager {
 std::array<CacheSink,8> sinks{};
public:
 static MemoryManager& instance(){static MemoryManager m;return m;}
 int registerSink(const CacheSink& s){for(unsigned i=0;i<sinks.size();++i)if(!sinks[i].name||strcmp(sinks[i].name,s.name)==0){sinks[i]=s;return i;}return -1;}
 void unregisterSink(const char* name){for(auto& s:sinks)if(s.name&&strcmp(s.name,name)==0)s={};}
 size_t freeBytes(MemPool=MemPool::Default)const{return ESP.getFreeHeap();}
 size_t largestFreeBlock(MemPool=MemPool::Default)const{return ESP.getMaxAllocHeap();}
 size_t clearCaches(size_t target=0){size_t total=0;for(unsigned priority=0;priority<256;++priority)for(auto& s:sinks)if(s.name&&s.priority==priority&&s.evict){total+=s.evict(target);if(target&&total>=target)return total;}return total;}
 bool ensureFree(size_t bytes,MemPool=MemPool::Default){if(freeBytes()>=bytes)return true;clearCaches(bytes);return freeBytes()>=bytes;}
 void reset(){for(auto& s:sinks)s={};}
};
}
