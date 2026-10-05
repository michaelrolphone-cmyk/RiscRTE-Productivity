/* Complete real portable app + unmodified adapter, with raw touch/RTC/display. */
#include "timecard_adapter_fixture.h"
#include "T5StorageApi.h"
static char history[49152];
static unsigned history_writes;
static bool history_read_ok=true;
static bool tc_exists(const char *path){assert(!strcmp(path,"/sd/.crosspoint/timecard.json"));return true;}
static bool tc_read(const char *path,void *out,size_t cap,size_t *size){assert(tc_exists(path));*size=0;if(!history_read_ok)return false;size_t n=strlen(history);if(n>cap)return false;memcpy(out,history,n);*size=n;return true;}
static bool tc_write(const char *path,const void *data,size_t size){assert(tc_exists(path)&&size<sizeof(history));memcpy(history,data,size);history[size]=0;history_writes++;return true;}
static const t5_storage_api_v1 tc_files={.api_version=1,.struct_size=sizeof(tc_files),.exists=tc_exists,.read_file=tc_read,.write_file_atomic=tc_write};
const t5_storage_api_v1 *timecard_portable_file_storage(void){return &tc_files;}
static void tap(unsigned at,int x,int y){for(unsigned i=0;i<2;i++){actions[action_count].at=at+i;actions[action_count].x=x;actions[action_count].y=y;action_count++;}}
static void reset(const char *frames_path){
 directory=frames_path;memset(pixels,0xa5,sizeof(pixels));memset(cells,0,sizeof(cells));ticks=polls=grants=frames=0;
 subs=presents=action_count=launch_count=launch_at=history_writes=crown_at=0;stop_poll=120;history_read_ok=true;
 strcpy(history,"{\"days\":[{\"d\":20261004,\"in\":480}]}");
}
int main(int argc,char **argv){
 assert(argc==2);reset(argv[1]);
 tap(5,90,100);tap(15,90,110);tap(25,90,100);tap(35,190,192);
 tap(45,45,224);tap(55,45,224);tap(65,45,224);
 assert(app_module_init()==0);app_main();app_module_fini();
 assert(history_writes==1&&!strcmp(history,"{\"days\":[{\"d\":20261004,\"in\":480}]}"));
 assert(launch_count==1&&launch_at==67&&!grants&&!frames&&!subs);
 /* Crown Back cancels the real keyboard before later touch Back exits levels. */
 reset(argv[1]);tap(5,90,100);tap(15,90,110);tap(25,90,100);tap(30,50,86);crown_at=35;
 tap(45,45,224);tap(55,45,224);tap(65,45,224);
 assert(app_module_init()==0);app_main();app_module_fini();assert(!history_writes&&launch_count==1&&launch_at==67&&!grants&&!frames&&!subs);assert(!strcmp(history,"{\"days\":[{\"d\":20261004,\"in\":480}]}"));
 puts("Timecard production adapter raw-touch edit/Done, crown cancel, nested Back and grant cleanup passed");return 0;
}
