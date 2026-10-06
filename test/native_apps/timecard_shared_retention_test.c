/* The Timecard consumer must respect shared alarm/sleep retention too, before
 * final drawing/launch/release. Runtime separately owns the pre-fini barrier. */
#define PORTABLE_ALARM_CLIENT
#define main timecard_profile_fixture_main
#include "timecard_portable_test.c"
#undef main
static bool shared_retained;
bool portable_app_sleep_retained(void){return shared_retained;}
int main(void){
 init();tcp_runtime=&runtime;tcp_grant_count=1;grants=1;shared_retained=true;
 tcp_dependencies_close();assert(grants==1&&tcp_retained());
 /* A retained initial invocation returns without presenting or releasing. */
 init();shared_retained=true;poll_count=launches=grants=presentations=0;
 app_main();assert(!poll_count&&!launches&&!presentations&&grants==2);
 /* A fresh independent invocation with no retention follows normal cleanup. */
 init();shared_retained=false;poll_count=launches=grants=0;
 app_main();assert(launches==1&&!grants&&back_exits);
 puts("Timecard shared alarm/sleep retained state prevents display, launch and grant cleanup");return 0;
}
