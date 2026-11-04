/* Render actual application declarations with the production shared provider. */
#define main baseline_main
#include "host_test.c"
#undef main
#include "RiscSceneComponentsV1.h"
extern const risc_components_document_v1 *timecard_scene_fixture(unsigned);
static void save_frame(const char *folder,const char *profile,unsigned page){
 settle();char path[1024];snprintf(path,sizeof(path),"%s/%s-%u.ppm",folder,profile,page);
 FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%u %u\n255\n",w,h);
 for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){
  uint8_t rgb[3];if(format==5){unsigned at=16+(y*w+x)*2;uint16_t c=pixels[at]|(uint16_t)pixels[at+1]<<8;rgb[0]=(uint8_t)((c>>11)*255/31);rgb[1]=(uint8_t)(((c>>5)&63)*255/63);rgb[2]=(uint8_t)((c&31)*255/31);}
  else{unsigned bit=y*w+x;rgb[0]=rgb[1]=rgb[2]=pixels[16+bit/8]&(128>>(bit%8))?0:255;}
  assert(fwrite(rgb,1,3,f)==3);
 }assert(!fclose(f));
}
int main(int argc,char **argv){
 assert(argc==3);
 if(!strcmp(argv[1],"paper")){w=800;h=480;format=1;prof=(risc_scene_profile_v1){1,sizeof(prof),1,3,88,20,0,0xffff,0,270,0,0};}
 else if(!strcmp(argv[1],"amoled")){w=410;h=502;}
 else if(!strcmp(argv[1],"round-badge")){w=466;h=466;}
 startup();assert(api->close(NULL,session)==0);
 const risc_scene_components_api_v1 *c=risc_scene_components_get_v1(api);assert(c);
 expected_intent=RISC_DISPLAY_PRESENT_LOW_LATENCY;
 for(unsigned page=0;page<4;page++){
  const risc_components_document_v1 *d=timecard_scene_fixture(page);
  assert(!c->open(NULL,d,NULL,&session));save_frame(argv[2],argv[1],page);
  risc_scene_navigation_v1 n={.struct_size=sizeof(n)};uint32_t flags=0;assert(!api->snapshot(NULL,session,&n,&flags));
  assert(n.depth==1);assert(!api->close(NULL,session));
 }
 assert(!held&&!presenting&&!subscribed);printf("Timecard actual scene rendering %s %ux%u passed\n",argv[1],w,h);return 0;
}
