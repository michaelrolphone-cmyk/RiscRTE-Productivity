#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define LISTS_MAX_LISTS 12u
#define LISTS_MAX_TASKS 64u
#define LISTS_NAME 25u
#define LISTS_LAST_DAY 36524 /* 2099-12-31, civil days since 2000-01-01 */
#define LISTS_WIRE_SIZE (32u+LISTS_MAX_LISTS*40u+LISTS_MAX_TASKS*64u)
enum { LISTS_OK,LISTS_INVALID,LISTS_FULL,LISTS_NOT_FOUND };
typedef struct {uint32_t id;char name[LISTS_NAME];uint8_t marker,show_completed;} lists_list;
typedef struct {
    uint32_t id,list_id;
    char name[LISTS_NAME];
    int32_t due_day;int16_t minute;
    uint8_t priority,reminder,repeat,done,spawned;
    uint32_t acknowledged,snooze;
} lists_task;
typedef struct {
    uint32_t next_id,list_count,task_count;
    lists_list lists[LISTS_MAX_LISTS];lists_task tasks[LISTS_MAX_TASKS];
} lists_model;
void lists_init(lists_model *);
bool lists_valid(const lists_model *);
int lists_name(char out[LISTS_NAME],const char *);
lists_list *lists_find_list(lists_model *,uint32_t);
lists_task *lists_find_task(lists_model *,uint32_t);
int lists_add_list(lists_model *,const char *,unsigned marker,uint32_t *id);
int lists_add_task(lists_model *,uint32_t list,const char *,int32_t day,uint32_t *id);
int lists_complete(lists_model *,uint32_t id,bool done,int32_t today);
int lists_delete_task(lists_model *,uint32_t);
int lists_delete_list(lists_model *,uint32_t);
void lists_clear_completed(lists_model *,uint32_t);
uint32_t lists_reminder_deadline(const lists_task *);
uint32_t lists_pending_reminder(const lists_model *,uint32_t now);
bool lists_overdue(const lists_task *,int32_t today,unsigned minute);
int32_t lists_day(unsigned year,unsigned month,unsigned day);
bool lists_civil(int32_t day,unsigned *year,unsigned *month,unsigned *date);
bool lists_encode(const lists_model *,uint8_t out[LISTS_WIRE_SIZE]);
bool lists_decode(lists_model *,const uint8_t *,size_t);
