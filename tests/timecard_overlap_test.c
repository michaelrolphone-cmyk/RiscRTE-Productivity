#include "../Apps/timecard.c"
#include <assert.h>
#include <stdio.h>
const t5_app_api_v1 *t5_app_get_api(uint32_t v) { (void)v; return NULL; }
const t5_storage_api_v1 *t5_storage_get_api(uint32_t v) { (void)v; return NULL; }
const t5_system_api_v1 *t5_system_get_api(uint32_t v) { (void)v; return NULL; }
const t5_system_ui_api_v1 *t5_system_ui_get_api(uint32_t v) { (void)v; return NULL; }
const t5_ui_api_v1 *t5_ui_get_api(uint32_t v) { (void)v; return NULL; }
int main(void) {
    const int16_t cases[][5] = {
      {480,720,750,1020,510}, {480,400,500,1020,520}, {480,1000,1100,1020,520},
      {480,400,450,1020,540}, {480,1030,1100,1020,540}, {480,400,1100,1020,0},
      {480,-1,-1,1020,540}, {480,750,720,1020,540}, {-1,720,750,1020,-1},
      {1020,720,750,480,-1}, {480,480,480,480,0}, {480,400,480,1020,540}
    };
    assert(worked(NULL)==-1);
    for (size_t i=0;i<sizeof(cases)/sizeof(cases[0]);++i) {
        tc_day_t day={0,{cases[i][0],cases[i][1],cases[i][2],cases[i][3]}};
        assert(worked(&day)==cases[i][4]);
    }
    puts("Timecard actual lunch overlap paths PASS"); return 0;
}
