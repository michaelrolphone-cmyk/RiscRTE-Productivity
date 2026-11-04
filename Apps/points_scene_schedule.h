#ifndef POINTS_SCENE_SCHEDULE_H
#define POINTS_SCENE_SCHEDULE_H
#include "AlarmServiceV1.h"
#include "PointsRecords.h"
#include "RiscKeyValueV1.h"
/* Semantic, copied schedule information. No display or hardware policy. */
typedef struct {char clock_text[64],status[64];uint32_t next_event_id;bool valid;} points_scene_schedule;
bool points_scene_native_schedule(const alarm_service_v1 *alarm,const points_config *config,
 const risc_key_value_v1 *preferences,uint32_t seconds,unsigned format,
 bool(*alive)(void*),void *context,points_scene_schedule *out);
#endif
