#pragma once
#include "RiscResidentShellV1.h"
#include "PortableResidentPolicyInput.h"
static unsigned resident_test_polls,resident_test_controls,resident_test_policies;
static bool resident_test_request_policy,resident_test_fail,resident_test_exit,policy_continuity,policy_exit_during_apply;
static uint32_t policy_poll_flags;
static int32_t resident_test_checkpoint(uint64_t invocation,const risc_resident_request_v1 *request,risc_resident_reply_v1 *reply){
 assert(invocation==2&&request&&request->struct_size==sizeof(*request)&&reply->struct_size==sizeof(*reply));
 if(resident_test_fail)return RISC_RESIDENT_RETAINED;
 if(request->reason==RISC_RESIDENT_CHECKPOINT_POLL){resident_test_polls++;policy_poll_flags=request->flags;reply->flags=resident_test_request_policy?RISC_RESIDENT_REPLY_POLICY_REQUEST|(policy_continuity?PORTABLE_RESIDENT_REPLY_INPUT_CONTINUITY:0):0;}
 if(request->reason==RISC_RESIDENT_CHECKPOINT_POLICY){
  if(text_opened)assert(policy_paused&&!live_frames&&!live_subs);
  resident_test_policies++;resident_test_request_policy=false;
  if(policy_exit_during_apply)return RISC_RESIDENT_EXIT;
 }
 if(request->reason==RISC_RESIDENT_CHECKPOINT_CONTROLS)resident_test_controls++;
 return resident_test_exit?RISC_RESIDENT_EXIT:RISC_RESIDENT_OK;
}
static bool resident_test_get(risc_resident_client_v1 *out){*out=(risc_resident_client_v1){.api_version=1,.struct_size=sizeof(*out),.invocation=2,.role=RISC_RESIDENT_ROLE_FOREGROUND,.checkpoint=resident_test_checkpoint};return true;}
static bool policy_exit_requested(void){return policy_runtime_exit;}
#define TEST_RESIDENT_BINDING .resident_shell=resident_test_get,.exit_requested=policy_exit_requested,
