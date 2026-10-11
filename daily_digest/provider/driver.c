#include "../RiscNetHttpClientV1.h"
#include "RiscProviderV2.h"
#include "RiscRuntimeV1.h"
#include <string.h>

static const risc_http_client_v1 *transport;
static uint64_t owned, last_closed;
static bool started, retained;
static bool alive(void) {
    if (!retained && !risc_runtime_get_api(1)) retained = true;
    return !retained;
}
static int32_t opened(void *context, const risc_http_request_v1 *request, uint64_t *out) {
    (void)context;
    if (out) *out = 0;
    if (!alive()) return RISC_HTTP_RETAINED;
    if (!started || !out) return RISC_HTTP_INVALID;
    if (owned) return RISC_HTTP_BUSY;
    int32_t rc = transport->open(transport->context, request, &owned);
    *out = owned; /* Failed opens may still own native cleanup. */
    if (rc == RISC_HTTP_RETAINED || !alive()) retained = true;
    return retained ? RISC_HTTP_RETAINED : rc;
}
static int32_t read_data(void *context, uint64_t token, void *buffer, uint32_t capacity, uint32_t *count) {
    (void)context;
    if (count) *count = 0;
    if (!alive()) return RISC_HTTP_RETAINED;
    if (!started || !owned || token != owned) return RISC_HTTP_CLOSED;
    int32_t rc = transport->read(transport->context, token, buffer, capacity, count);
    if (rc == RISC_HTTP_RETAINED || !alive()) retained = true;
    return retained ? RISC_HTTP_RETAINED : rc;
}
static int32_t information(void *context, uint64_t token, risc_http_response_v1 *out) {
    (void)context;
    if (!alive()) return RISC_HTTP_RETAINED;
    if (!started || !owned || token != owned) return RISC_HTTP_CLOSED;
    int32_t rc = transport->info(transport->context, token, out);
    if (rc == RISC_HTTP_RETAINED || !alive()) retained = true;
    return retained ? RISC_HTTP_RETAINED : rc;
}
static int32_t close_owned(void) {
    if (retained) return RISC_HTTP_RETAINED;
    if (!owned) return RISC_HTTP_OK;
    int32_t rc = transport->close(transport->context, owned);
    if (rc != RISC_HTTP_OK) { retained = true; return RISC_HTTP_RETAINED; }
    last_closed = owned; owned = 0;
    return RISC_HTTP_OK;
}
static int32_t closed(void *context, uint64_t token) {
    (void)context;
    if (!alive()) return RISC_HTTP_RETAINED;
    if (started && token && !owned && token == last_closed) return RISC_HTTP_OK;
    if (!started || !owned || token != owned) return RISC_HTTP_CLOSED;
    int32_t rc = close_owned();
    return alive() ? rc : RISC_HTTP_RETAINED;
}
static bool start(const risc_provider_dependency_v1 *dependencies, size_t count) {
    if (started || retained || !dependencies || count != 1 ||
        !dependencies[0].capability_id ||
        strcmp(dependencies[0].capability_id, RISC_HTTP_CLIENT_CAPABILITY) ||
        dependencies[0].api_version != 1) return false;
    const risc_http_client_v1 *api = dependencies[0].api;
    if (!api || api->api_version != 1 || api->struct_size < sizeof(*api) ||
        !api->open || !api->read || !api->info || !api->close) return false;
    transport = api; started = true;
    return true;
}
static bool quiesce(void) { return !retained && close_owned() == RISC_HTTP_OK; }
static void stop(void) {
    if (!owned && !retained) { transport = NULL; started = false; }
}
static const risc_http_client_v1 api = {1, sizeof(api), NULL, opened, read_data, information, closed};
static const risc_driver_v2 driver = {2, sizeof(driver), "net-http-client", RISC_NET_HTTP_CLIENT_CAPABILITY,
    1, &api, start, stop, quiesce};
__attribute__((visibility("default"))) const risc_driver_v2 *t5_driver_get(uint32_t version) {
    return version == 2 ? &driver : NULL;
}
