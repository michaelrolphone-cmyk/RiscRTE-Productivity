/* Reuse the real source/renderer fixture with explicit deployment time policy. */
#define main timecard_general_fixture_main
#include "timecard_portable_test.c"
#undef main
int main(void) {
 init();now=(twatch_rtc_time_v1){2026,1,1,4,0,5,0};
 t5_local_datetime_t local;assert(tcp_read_datetime(&local));
#ifdef PORTABLE_RTC_UTC8_DENVER
 assert(local.year==2025&&local.month==12&&local.day==31&&local.hour==9&&local.minute==5&&local.weekday==3&&local.yearday==364);
#else
 assert(local.year==2026&&local.month==1&&local.day==1&&local.hour==0&&local.minute==5&&local.weekday==4&&local.yearday==0);
#endif
 for(unsigned mode=0;mode<2;mode++)for(unsigned hour=0;hour<24;hour++) {
  char source[24],actual[24],expected[24];
  format_ampm((int16_t)(hour*60+5),source,sizeof(source));tcp_time_format=mode;
  tcp_display_time(source,actual,sizeof(actual));
  if(mode==PORTABLE_TIME_FORMAT_24)snprintf(expected,sizeof(expected),"%u:05",hour);
  else snprintf(expected,sizeof(expected),"%u:05 %s",hour%12?hour%12:12,hour<12?"AM":"PM");
  assert(!strcmp(actual,expected));
 }
 tcp_snapshot=local;tcp_clock_valid=true;unsigned before=clock_reads;
 int32_t anchor=today();for(unsigned i=0;i<20;i++)assert(sunday(-(int32_t)i)==add_days(anchor,-weekday(anchor)-(int32_t)i*7));assert(clock_reads==before);
 t5_local_datetime_t preserved=local;rtc_ok=false;assert(!tcp_read_datetime(&local)&&!memcmp(&preserved,&local,sizeof(local)));
 puts("Timecard timezone/year-boundary bridge and one-snapshot calendar rendering passed");return 0;
}
