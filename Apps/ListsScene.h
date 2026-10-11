#pragma once
#include "../lib/Lists/ListsModel.h"
#include "RiscSceneComponentsV1.h"
enum {LISTS_HOME=1,LISTS_VIEW,LISTS_TASK,LISTS_ADD_TASK,LISTS_ADD_LIST,LISTS_OPTIONS,LISTS_NAME_EDIT,LISTS_TIME,LISTS_CONFIRM,LISTS_ALERT};
enum {LA_BACK=1,LA_TODAY,LA_ALL,LA_LIST,LA_NEW_LIST,LA_NEW_TASK,LA_OPTIONS,LA_TASK,LA_DONE,
LA_FILTER,LA_RENAME_TASK,LA_RENAME_LIST,LA_TASK_LIST,LA_DUE,LA_TIME,LA_PRIORITY,LA_REMINDER,LA_REPEAT,
LA_DELETE_TASK,LA_DELETE_LIST,LA_CLEAR_DONE,LA_SHOW_DONE,LA_MARKER,LA_SUGGEST_LIST,LA_SUGGEST_TASK,
LA_CUSTOM_LIST,LA_CUSTOM_TASK,LA_KEY,LA_TIME_VALUE,LA_SET_TIME,LA_NO_TIME,LA_KEEP,LA_CONFIRM,
LA_ALERT_DONE,LA_SNOOZE,LA_DISMISS,LA_UNDO,LA_RETRY};
#define LISTS_VIEW_ALL UINT32_MAX
#define LISTS_VIEW_TODAY (UINT32_MAX-1u)
typedef struct {
    lists_model data,undo;
    uint32_t page,parent,view,task,marker,edit_kind,confirm_kind,alert,refs[RISC_COMPONENTS_MAX_NODES];
    uint32_t revision,toast_token;uint64_t last_event;
    int32_t today;unsigned minute,filter,time_value,key_layer;
    bool readonly,undo_valid;
    char draft[LISTS_NAME],notice[RISC_SCENE_LABEL];
    risc_components_document_v1 document;
} lists_ui;
void lists_ui_init(lists_ui *);
void lists_ui_declare(lists_ui *);
/* Returns true only for data mutations. All events are checked against the
 * latest declaration, enabled state, intent, value domain and sequence. */
bool lists_ui_event(lists_ui *,const risc_scene_event_v1 *);
void lists_ui_notice(lists_ui *,const char *);
void lists_ui_clock(lists_ui *,int32_t day,unsigned minute);
