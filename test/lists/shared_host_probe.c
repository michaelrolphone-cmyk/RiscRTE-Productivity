/* Read-only hit-map inspection. Input still traverses the shipping presenter. */
#include SCENE_HOST_SOURCE
bool lists_test_target(uint32_t action,int value,int*x,int*y){
 for(unsigned i=0;i<visible.count;i++){
  const hit*h=&visible.items[i];int at=node_index(&document,h->node);
  if(at<0||h->h<12)continue;
  uint32_t intent=h->kind==HIT_SECONDARY?document.details[at].secondary_action:document.nodes[at].action;
  if(intent!=action||(value!=-999&&h->value!=value))continue;
  for(int part=1;part<=3;part++){
   int px=h->x+h->w*part/4,py=h->y+h->h/2;bool covered=false;
   for(unsigned j=i+1;j<visible.count;j++){const hit*b=&visible.items[j];if(px>=b->x&&py>=b->y&&px<b->x+b->w&&py<b->y+b->h)covered=true;}
   if(!covered){*x=px;*y=py;return true;}
  }
 }
 return false;
}
uint64_t lists_test_session(void){return session;}
bool lists_test_header(bool home,int*x,int*y){
 for(unsigned i=0;i<visible.count;i++)if(visible.items[i].kind==(home?HIT_HOME:HIT_BACK)){
  const hit*h=&visible.items[i];*x=h->x+h->w/2;*y=h->y+h->h/2;return true;
 }
 return false;
}
