#pragma once
/* App-facing, provider-owned forwarding of the existing bounded HTTP ABI.
 * Applications never acquire the native platform capability directly. */
#include "RiscHttpClientV1.h"
#define RISC_NET_HTTP_CLIENT_CAPABILITY "net.http-client"
