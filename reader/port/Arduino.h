#pragma once
/* Source compatibility only. Every environmental operation is capability-backed. */
#include "WString.h"
#include "Print.h"
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <cstring>
#include <strings.h>
#include <cstdint>
#define PROGMEM
#define pgm_read_byte(p) (*(const uint8_t*)(p))
#define pgm_read_word(p) (*(const uint16_t*)(p))
#define pgm_read_dword(p) (*(const uint32_t*)(p))
uint32_t millis();
void delay(uint32_t);
void yield();
struct ReaderHeapInfo {uint32_t getFreeHeap() const;uint32_t getMaxAllocHeap() const;void restart() const;};
extern ReaderHeapInfo ESP;

#include <cmath>
#define memcpy_P memcpy
inline uint32_t micros(){return millis()*1000u;}
inline void vTaskDelay(uint32_t t){delay(t);}

inline size_t readerStrnlen(const char* s,size_t limit){size_t n=0;while(n<limit&&s[n])++n;return n;}
inline int readerCasecmp(const char* a,const char* b){while(*a&&*b){unsigned char x=*a++,y=*b++;if(x>='A'&&x<='Z')x+=32;if(y>='A'&&y<='Z')y+=32;if(x!=y)return x-y;}return (unsigned char)*a-(unsigned char)*b;}
#define strnlen readerStrnlen
#define strcasecmp readerCasecmp
