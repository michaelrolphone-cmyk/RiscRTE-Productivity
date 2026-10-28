#define POINTS_SERVICE_CONTEXT
#define POINTS_NATIVE_UTC_TEST_ENTRY native_context_fixture_main
#include "points_native_utc_test.c"
static points_service_context context_value;
static int32_t context_result;
static int32_t context_copy(void *c,points_service_context *out){(void)c;*out=context_value;return context_result;}
int main(void){
 native_setup();set_direct();assert(writer.loaded);
 points_context_service_v2 descriptor={native_alarm_descriptor,{POINTS_SERVICE_CONTEXT_TAG,1,context_copy}};
 descriptor.base.base.struct_size=sizeof(descriptor);service=&descriptor.base.base;
 writer.saved=(points_config){.revision=3,.created=utc(2026,10,5,0,0)};
 writer.saved.points[0]=(points_item){.kind=POINTS_BREAK,.enabled=1,.mode=3,.weekdays=127,.hour=13};
 context_value=(points_service_context){.struct_size=sizeof(context_value),.timezone_id="UTC",.sampled_utc=utc(2026,10,4,12,0),.effective_created=utc(2026,10,3,0,0),.snapshot=2};
 points_config_encode(&writer.saved,context_value.config);
 uint32_t now;assert(read_clock(&now)&&points_context_status==ALARM_OK&&points_preview.count);
 assert(points_preview.next[0].deadline==utc(2026,10,4,13,0));
 /* Projection uses the service floor without changing stored creation identity. */
 assert(writer.saved.created==utc(2026,10,5,0,0)&&!puts_count);
 points_service_context good=context_value;context_value.config[20]^=1;
 assert(read_clock(&now)&&points_context_status==ALARM_STALE&&!points_preview.count);
 context_value=good;context_value.timezone_id[0]='X';
 assert(read_clock(&now)&&points_context_status==ALARM_STALE&&!points_preview.count);
 context_value=good;context_result=ALARM_PENDING;
 assert(read_clock(&now)&&points_context_status==ALARM_PENDING&&!points_preview.count);
 context_result=ALARM_RETAINED;assert(!read_clock(&now)&&points_retained&&retains==1);
 unsigned before=provider_calls;assert(!read_clock(&now)&&provider_calls==before);
 puts("Actual Points controller: service-authorized preview, stale identities and retained custody passed");return 0;
}
