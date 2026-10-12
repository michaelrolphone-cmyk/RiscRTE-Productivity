/* Compare actual app renders, ledger bytes and navigation with the migration
 * flag on/off. FNV hashes replace multi-megabyte frame dumps. */
#ifdef TIMECARD_NATIVE_TIME
#define main native_preservation_main
#include "timecard_native_time_test.c"
#undef main
#else
#include <stdbool.h>
#define TIMECARD_FIXTURE_ONLY
#include "timecard_portable_test.c"
#ifdef PORTABLE_TIMECARD_SHARED_TEXT_INPUT
int portable_text_adapter_suspend(void){assert(!"No keyboard in parity test");return RISC_TEXT_ENTRY_UNAVAILABLE;}
int portable_text_adapter_resume(void){assert(!"No keyboard in parity test");return RISC_TEXT_ENTRY_UNAVAILABLE;}
int portable_text_adapter_attention(void){return 0;}
void portable_text_adapter_retain(void){assert(!"No retention in parity test");}
bool portable_text_adapter_retained(void){return false;}
#endif
#endif
static uint64_t digest(const void *data,size_t size){const unsigned char *p=data;uint64_t h=UINT64_C(1469598103934665603);for(size_t i=0;i<size;i++)h=(h^p[i])*UINT64_C(1099511628211);return h;}
static void capture(const char *name){tcp_draw();
#ifdef TIMECARD_NATIVE_TIME
 printf("%s %016llx\n",name,(unsigned long long)digest(pixels,sizeof(pixels)));
#else
 printf("%s %016llx\n",name,(unsigned long long)digest(test_pixels,sizeof(test_pixels)));
#endif
}
int main(void){
#ifdef TIMECARD_NATIVE_TIME
 zone("UTC");start();
#else
 init();
#endif
 capture("weeks");open_week(0);capture("week");open_day(20261004);capture("day");
 assert(tcp_mutate(20261004,0,480));assert(tcp_mutate(20261004,1,720));assert(tcp_mutate(20261004,2,765));assert(tcp_mutate(20261004,3,1020));
 capture("saved-day");tcp_time_format=PORTABLE_TIME_FORMAT_24;capture("saved-24-hour");tcp_back();capture("saved-week");tcp_back();capture("weeks-return");
#ifdef TIMECARD_NATIVE_TIME
 printf("ledger %016llx\n",(unsigned long long)digest(history,strlen(history)));finish();
#else
 printf("ledger %016llx\n",(unsigned long long)digest(persisted,strlen(persisted)));
#endif
 return 0;
}
