#define main legacy_adapter_main
#include "timecard_portable_adapter_test.c"
#undef main
int main(int argc,char **argv){
 assert(argc==2);
 for(unsigned source=0;source<2;source++)for(unsigned depth=0;depth<4;depth++){
  reset(argv[1]);for(unsigned d=0;d<depth;d++)tap(5+d*10,90,100);
  if(depth==3)tap(35,50,86); /* a dirty in-memory editor */
  if(source)primary_at=45;else home_at=45;
  expected_launch="default.elf";assert(app_module_init()==0);app_main();app_module_fini();
  assert(launch_count==1&&!history_writes&&!grants&&!frames&&!subs);
 }
 /* Repeat save/cancel and root Back with the actual global-Home build flag. */
 reset(argv[1]);tap(5,90,100);tap(15,90,100);tap(25,90,100);tap(35,190,192);
 tap(45,90,100);tap(55,50,86);crown_at=65;
 tap(75,45,224);tap(85,45,224);tap(95,45,224);
 assert(app_module_init()==0);app_main();app_module_fini();
 assert(history_writes==1&&launch_count==1&&!grants&&!frames&&!subs);
 puts("Watch nested global Home and repeated save/cancel/local Back passed");return 0;
}
