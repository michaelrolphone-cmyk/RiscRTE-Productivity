#pragma once
#include "RiscSceneCheckpointV1.h"
static const risc_scene_api_v1 *policy_scene_base;
static risc_scene_checkpoint_api_v1 policy_scene;
static bool policy_paused,policy_continuity,policy_activity,policy_input_busy;
static unsigned policy_pauses,policy_resumes,policy_full_pauses,policy_pause_again;
static int policy_pause_fault,policy_resume_fault,policy_configure_fault;
static uint32_t policy_resume_flags;
static int32_t policy_next(void *c,uint64_t s,risc_scene_event_v1 *out){
 assert(!policy_paused);int32_t rc=policy_scene_base->next(c,s,out);
 if(rc==RISC_SCENE_OK)policy_activity=true;return rc;
}
static int32_t policy_snapshot(void *c,uint64_t s,risc_scene_navigation_v1 *out,uint32_t *flags){
 int32_t rc=policy_scene_base->snapshot(c,s,out,flags);
 if(policy_activity)*flags|=RISC_SCENE_ACTIVITY;
 if(policy_input_busy)*flags|=RISC_SCENE_INPUT_BUSY;
 policy_activity=false;return rc;
}
static int32_t policy_configure(void *c,uint64_t s,uint32_t flags){
 (void)c;(void)s;assert(!(flags&~RISC_SCENE_FEATURE_POLICY_INPUT_CONTINUITY));
 if(policy_configure_fault)return policy_configure_fault;
 policy_continuity=!!(flags&RISC_SCENE_FEATURE_POLICY_INPUT_CONTINUITY);return 0;
}
static int32_t policy_pause(void *c,uint64_t s){
 (void)c;(void)s;if(policy_pause_fault)return policy_pause_fault;
 if(policy_pause_again){policy_pause_again--;return RISC_SCENE_AGAIN;}
 assert(!policy_paused);policy_paused=true;policy_pauses++;if(!policy_continuity)policy_full_pauses++;return 0;
}
static int32_t policy_resume(void *c,uint64_t s,uint32_t flags){
 (void)c;(void)s;assert(policy_paused);if(policy_resume_fault)return policy_resume_fault;
 policy_resume_flags=flags;policy_paused=false;policy_resumes++;return 0;
}
static int32_t policy_unused_open(void*c,const risc_components_document_v1*d,const risc_scene_navigation_v1*n,uint64_t*s){(void)c;(void)d;(void)n;(void)s;assert(0);return -1;}
static int32_t policy_unused_update(void*c,uint64_t s,const risc_components_document_v1*d){(void)c;(void)s;(void)d;assert(0);return -1;}
static int32_t policy_unused_geometry(void*c,risc_scene_page_geometry_v1*g){(void)c;(void)g;assert(0);return -1;}
static int32_t policy_unused_page(void*c,uint64_t s,const risc_scene_page_document_v1*d,const uint8_t*p,size_t n){(void)c;(void)s;(void)d;(void)p;(void)n;assert(0);return -1;}
static int32_t policy_unused_refresh(void*c,uint64_t s,const risc_scene_page_document_v1*d,const uint8_t*p,size_t n,uint32_t f){(void)c;(void)s;(void)d;(void)p;(void)n;(void)f;assert(0);return -1;}
static const void *timecard_policy_scene_api(const risc_scene_api_v1 *base){
 policy_scene_base=base;policy_scene.presentation.page.components.lifecycle.base=*base;
 policy_scene.presentation.page.components.lifecycle.base.struct_size=sizeof(policy_scene);
 policy_scene.presentation.page.components.lifecycle.base.next=policy_next;
 policy_scene.presentation.page.components.lifecycle.base.snapshot=policy_snapshot;
 policy_scene.presentation.page.components.lifecycle.configure=policy_configure;
 policy_scene.presentation.page.components.tag=RISC_COMPONENTS_TAG;policy_scene.presentation.page.components.version=1;
 policy_scene.presentation.page.components.open=policy_unused_open;policy_scene.presentation.page.components.update=policy_unused_update;
 policy_scene.presentation.page.page_tag=RISC_SCENE_PAGE_TAG;policy_scene.presentation.page.page_version=1;policy_scene.presentation.page.geometry=policy_unused_geometry;policy_scene.presentation.page.present_page=policy_unused_page;
 policy_scene.presentation.refresh_tag=RISC_SCENE_PAGE_REFRESH_TAG;policy_scene.presentation.refresh_version=1;policy_scene.presentation.present_page_with_refresh=policy_unused_refresh;
 policy_scene.checkpoint_tag=RISC_SCENE_CHECKPOINT_TAG;policy_scene.checkpoint_version=1;policy_scene.pause=policy_pause;policy_scene.resume=policy_resume;
 return &policy_scene;
}
