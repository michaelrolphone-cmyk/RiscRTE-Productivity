#pragma once
/* Reusable intent-app bridge to the resident shell. The application keeps its
 * model; the shell owns Quick Actions and policy UI. No presentation code or
 * product identity is linked into a foreground application. */
#include "RiscRuntimeV1.h"
#include "RiscResidentShellV1.h"
#include "RiscSceneLifecycleV1.h"
typedef struct {risc_resident_client_v1 client;bool enabled,activity,policy;} risc_scene_resident_v1;
static inline bool risc_scene_resident_bind_v1(const risc_runtime_api_v1 *rt,risc_scene_resident_v1 *s){
    *s=(risc_scene_resident_v1){0};
    if(rt->struct_size<RISC_RUNTIME_RESIDENT_SHELL_V1_SIZE||!rt->resident_shell)return true;
    s->client.struct_size=sizeof(s->client);bool found=rt->resident_shell(&s->client);
    if(!risc_runtime_get_api(1))return false;
    if(!found)return true; /* Ordinary non-resident deployment. */
    if(s->client.api_version!=1||s->client.struct_size<sizeof(s->client)||!s->client.invocation||s->client.role!=RISC_RESIDENT_ROLE_FOREGROUND||!s->client.checkpoint)return false;
    s->enabled=true;return true;
}
/* POLL never borrows the child's focus. Do not call while a surface, held input,
 * storage write or other app-owned operation is live. Latch input activity even
 * while presentation prevents a checkpoint. A POLICY request is serviced by
 * closing the scene first, just like CONTROLS. */
static inline int32_t risc_scene_resident_poll_v1(risc_scene_resident_v1 *s,uint32_t scene_flags){
    if(scene_flags&RISC_SCENE_ACTIVITY)s->activity=true;
    if(!s->enabled||scene_flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY|RISC_SCENE_CLOSING))return RISC_RESIDENT_BUSY;
    risc_resident_request_v1 request={sizeof(request),RISC_RESIDENT_CHECKPOINT_POLL,s->activity?RISC_RESIDENT_POLL_ACTIVITY:0,0};
    risc_resident_reply_v1 reply={sizeof(reply),0};int32_t rc=s->client.checkpoint(s->client.invocation,&request,&reply);
    if(!risc_runtime_get_api(1)||rc==RISC_RESIDENT_RETAINED)return RISC_RESIDENT_RETAINED;
    if(rc==RISC_RESIDENT_OK){s->activity=false;if(reply.flags&RISC_RESIDENT_REPLY_POLICY_REQUEST)s->policy=true;}
    return rc;
}
/* Precondition: scene.close completed, including input and display custody.
 * OK/BUSY permits reopening with the caller's copied document and navigation. */
static inline int32_t risc_scene_resident_dispatch_v1(risc_scene_resident_v1 *s,uint32_t reason){
    if(!s->enabled)return RISC_RESIDENT_DENIED;
    risc_resident_request_v1 request={sizeof(request),reason,0,0};risc_resident_reply_v1 reply={sizeof(reply),0};
    int32_t rc=s->client.checkpoint(s->client.invocation,&request,&reply);
    if(!risc_runtime_get_api(1)||rc==RISC_RESIDENT_RETAINED)return RISC_RESIDENT_RETAINED;
    if(rc==RISC_RESIDENT_OK&&reason==RISC_RESIDENT_CHECKPOINT_POLICY)s->policy=false;
    return rc;
}
