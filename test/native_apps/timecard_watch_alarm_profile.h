#pragma once
/* Same compatibility declarations as the published Watch component recipe. */
#include TIMECARD_TEST_ALARM_HEADER
#define ALARM_MODE_VISUAL 0u
static inline uint32_t alarm_service_output_modes(const alarm_service_v1 *s){return s?ALARM_MODE_BOTH:ALARM_MODE_VISUAL;}
/* Frozen Watch profile 089868395268b696c05651c33816345bfe0425df,
 * apps/clock/faces/{ContextsFaces.h,names.h}; vocabulary only, no device code. */
#define PORTABLE_CONTEXT_FACE_COUNT 34u
#define PORTABLE_CONTEXT_FACE_NAMES {"NOVA","ANALOG","RADAR","HEX","TERMINAL","MINIMAL","BINARY","CHRONO","AURORA","HORIZON","GIANT","MOON","TOPO","ORRERY","FLIP","MATRIX","GRID","RIBBON","YEAR","24H","AGENDA","DATE","DOTS","PROGRESS","NEXT","RING","LADDER","DAYLINE","TRIPLE","STATUS","BOARD","WORKDAY","UP NEXT","CONTEXTS"}
