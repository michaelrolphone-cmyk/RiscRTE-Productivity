#include "metadata.h"
#include "RiscNetHttpClientV1.h"
#include "RiscRuntimeV1.h"
#include "RiscSceneComponentsV1.h"
#include "RiscStorageVolumeFsV1.h"
#include "RiscRealtimeV1.h"
#include "T5FileOpenApi.h"
#include "PortableWifiSavedNetwork.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>
extern "C" void app_main(void);
using Bytes=std::vector<unsigned char>;
static risc_runtime_api_v1 rt{};
static risc_scene_components_api_v1 ui{};
static risc_storage_volume_api_v1_fs fs{};
static risc_realtime_api_v1 clock_api{};
static wifi_api_v1 wifi{};
static risc_key_value_v1 kv{};
static risc_http_client_v1 http{};
static t5_file_open_api_v1 files_api{};
static struct Fake {
 std::map<std::string,Bytes> files,credentials;
 std::set<std::string> dirs;
 std::set<unsigned> grants;
 std::string date="2026-10-10",meta,payload,writing,source;
 std::vector<std::string> errors,diagnostics,headings;
 risc_components_document_v1 doc{};
 uint32_t ms=0; unsigned yields=0,releases=0,opens=0,downloads=0,gets=0,reads=0,updates=0,connections=0,disconnects=0;
 uint64_t transfer=0,cookie=0; size_t cursor=0;
 bool scene=false,terminal=false,sd=true,clock=true,cancel=false,retry=false,return_result=false,fail_reader=false;
 bool short_write=false,full=false,sync_fail=false,replace_fail=false,retained_close=false,http404=false,httpfail=false,html=false,invalid_meta=false;
 wifi_link_t link=WIFI_LINK_UP;
} f;
static std::string url(const std::string& date){return std::string(DAILY_BASE)+"/editions/"+date.substr(0,4)+"/"+date.substr(5,2)+"/"+date.substr(8,2)+"/daily-digest-"+date+".epub";}
static std::string metadata(const std::string& d){return "{\"date\":\""+d+"\",\"download_url\":\""+url(d)+"\",\"sha256\":\"ignored\",\"title\":\"Today's \\u2014 paper\"}";}
static std::string dest(){return std::string(DAILY_DIRECTORY)+"/"+f.date+".epub";}
static std::string part(){return std::string(DAILY_DIRECTORY)+"/."+f.date+".part";}
static Bytes book(){Bytes b(6000,'a');b[0]='P';b[1]='K';b[2]=3;b[3]=4;return b;}
static void live(){assert(!f.terminal);}
extern "C" const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return !f.terminal&&v==1?&rt:nullptr;}
static void reset(){f=Fake{};f.dirs={"/"};}
static void show(const risc_components_document_v1 *d){
 live(); assert(d->api_version==1&&d->node_count>=2); f.doc=*d; f.headings.emplace_back(d->nodes[0].label);
 if(d->nodes[0].kind==RISC_COMPONENT_EMPTY)f.errors.emplace_back(std::string(d->nodes[0].label)+": "+d->nodes[0].text);
}
static bool partials(){for(const auto& i:f.files)if(i.first.find(".part")!=std::string::npos)return true;return false;}
static void initialize(){
 rt.api_version=1;rt.struct_size=sizeof(rt);
 rt.yield_ms=[](uint32_t ms){live();assert(++f.yields<20000);f.ms+=ms;};
 rt.monotonic_ms=[](uint32_t *out){live();*out=f.ms;return true;};
 rt.diagnostic=[](const char*s){live();f.diagnostics.emplace_back(s);return true;};
 rt.retain_invocation=[](){live();f.terminal=true;return true;};
 rt.acquire=[](const char*cap,uint32_t ver,uint64_t instance,risc_runtime_capability_v1 *out){
  live();assert(ver==1);unsigned slot=0;const void *api=nullptr;
  if(!strcmp(cap,"ui.scene")){slot=1;api=&ui;}
  else if(!strcmp(cap,"file.open")){slot=2;api=&files_api;}
  else if(!strcmp(cap,"storage.volume")){slot=3;api=&fs;}
  else if(!strcmp(cap,"runtime.realtime")){slot=4;api=&clock_api;}
  else if(!strcmp(cap,"net.wifi")){slot=5;api=&wifi;}
  else if(!strcmp(cap,"storage.key-value")){assert(instance==6);slot=6;api=&kv;}
  else if(!strcmp(cap,"net.http-client")){slot=7;api=&http;}
  else assert(false);
  assert(f.grants.insert(slot).second);out->api=api;out->slot=slot;out->generation=1;return true;
 };
 rt.release=[](risc_runtime_capability_v1 *g){live();assert(!f.transfer&&f.writing.empty());assert(f.grants.erase(g->slot)==1);++f.releases;return true;};
 auto& sb=ui.lifecycle.base;sb.api_version=1;sb.struct_size=sizeof(ui);
 ui.tag=RISC_COMPONENTS_TAG;ui.version=1;
 ui.lifecycle.configure=[](void*,uint64_t session,uint32_t flags)->int32_t{live();assert(session==1&&flags==0);return 0;};
 ui.open=[](void*,const risc_components_document_v1*d,const risc_scene_navigation_v1*,uint64_t*out)->int32_t{live();assert(!f.scene);f.scene=true;show(d);*out=1;return 0;};
 ui.update=[](void*,uint64_t,const risc_components_document_v1*d)->int32_t{live();assert(f.scene&&d->revision>f.doc.revision);++f.updates;show(d);return 0;};
 sb.close=[](void*,uint64_t)->int32_t{live();assert(f.scene);f.scene=false;return 0;};
 sb.snapshot=[](void*,uint64_t,risc_scene_navigation_v1*,uint32_t*flags)->int32_t{live();*flags=0;return 0;};
 sb.next=[](void*,uint64_t,risc_scene_event_v1 *e)->int32_t{
  live();assert(f.scene);unsigned action=0;
  if(f.doc.nodes[0].kind==RISC_COMPONENT_EMPTY){action=f.retry?2:1;if(f.retry){f.retry=false;f.http404=false;f.httpfail=false;}}
  else if(f.cancel&&f.downloads&&f.cursor>512)action=1;
  if(!action)return RISC_SCENE_IDLE;
  e->kind=RISC_SCENE_ACTION_EVENT;e->action=action;e->document_revision=f.doc.revision;e->sequence=uint64_t(f.yields)+100;
  for(unsigned i=0;i<f.doc.node_count;++i)if(f.doc.nodes[i].action==action)e->node=i+1;
  assert(e->node);return 0;
 };
 auto& ext=*reinterpret_cast<risc_storage_volume_api_v1_ext*>(&fs);auto& base=ext.base;
 base.api_version=1;base.struct_size=sizeof(fs);
 base.refresh=[](void*){live();return true;};base.ready=[](void*){live();return f.sd;};
 base.last_error=[](void*,char*out,size_t cap){live();snprintf(out,cap,"SD test I/O failure");return true;};
 base.file_open_write=[](void*,const char*path)->risc_storage_file_t{live();assert(f.sd&&f.writing.empty());if(f.files.count(path))return 0;f.files[path]={};f.writing=path;return 7;};
 base.file_write=[](void*,risc_storage_file_t h,const void*b,size_t n)->size_t{live();assert(h==7&&!f.writing.empty());if(f.full)return 0;if(f.short_write)n=std::min(n,size_t(17));auto*p=static_cast<const unsigned char*>(b);auto&file=f.files[f.writing];file.insert(file.end(),p,p+n);return n;};
 base.file_close=[](void*,risc_storage_file_t h,bool commit){live();assert(h==7&&!f.writing.empty());if(!commit)f.files.erase(f.writing);f.writing.clear();return true;};
 base.remove=[](void*,const char*path){live();assert(f.writing!=path);return f.files.erase(path)==1;};
 ext.file_sync=[](void*,risc_storage_file_t){live();return !f.sync_fail;};
 ext.handle_error=[](void*,uint32_t,bool)->uint32_t{live();return 0;};
 ext.mkdir=[](void*,const char*path){live();return f.dirs.insert(path).second;};
 fs.state.state_tag=RISC_STORAGE_STATE_TAG;fs.state.state_version=1;
 fs.state.observe=[](void*)->int32_t{live();return f.sd?RISC_STORAGE_STATE_READY:RISC_STORAGE_STATE_UNAVAILABLE;};
 fs.fs_tag=RISC_STORAGE_FS_TAG;fs.fs_version=1;
 fs.dir_tell=[](void*,risc_storage_dir_t,uint64_t*){return true;};fs.dir_seek=[](void*,risc_storage_dir_t,uint64_t){return true;};
 fs.file_truncate=[](void*,risc_storage_file_t,uint64_t){return true;};
 fs.metadata=[](void*,const char*p,risc_storage_metadata_v1*m)->int32_t{live();if(f.dirs.count(p)){m->flags=RISC_STORAGE_DIRECTORY;return 0;}auto i=f.files.find(p);if(i==f.files.end())return 1;m->size=i->second.size();m->flags=0;return 0;};
 fs.replace_file=[](void*,const char*source,const char*target){live();assert(f.writing.empty());if(f.replace_fail)return false;assert(f.files.count(source));f.files[target]=f.files.at(source);f.files.erase(source);return true;};
 fs.recover_replace=[](void*,const char*){live();return true;};
 clock_api.api_version=1;clock_api.struct_size=sizeof(clock_api);
 clock_api.read=[](void*,risc_realtime_snapshot_v1*s)->int32_t{live();s->validity=f.clock?1:0;s->epoch_seconds=f.clock?1791684000:0;return 0;};
 wifi.api_version=1;wifi.struct_size=sizeof(wifi);
 wifi.connect=[](void*,const char*ssid,const char*password){live();assert(std::string(ssid)=="Saved Network"&&std::string(password)=="password123");++f.connections;f.link=WIFI_LINK_JOINING;return true;};
 wifi.status=[](void*){live();auto link=f.link;if(link==WIFI_LINK_JOINING)f.link=WIFI_LINK_UP;return link;};
 wifi.rssi=[](void*)->int8_t{return -40;};
 wifi.addresses=[](void*,wifi_ipv4_v1*ip,wifi_ipv4_v1*){live();ip->address[0]=192;ip->address[1]=168;ip->address[2]=1;ip->address[3]=42;return true;};
 wifi.scan_start=[](void*){return false;};wifi.scan_poll=[](void*,garden_radio_scan_result_v1*){return false;};wifi.scan_cancel=[](void*){return true;};
 wifi.disconnect_checked=[](void*){live();assert(!f.transfer);++f.disconnects;f.link=WIFI_LINK_DOWN;return true;};
 kv.api_version=1;kv.struct_size=sizeof(kv);
 kv.get=[](void*,const char*key,void*buf,uint32_t cap,uint32_t*out)->int32_t{live();auto i=f.credentials.find(key);*out=0;if(i==f.credentials.end())return -1;*out=uint32_t(i->second.size());if(cap<*out)return -2;memcpy(buf,i->second.data(),*out);return 0;};
 kv.put=[](void*,const char*key,const void*buf,uint32_t n)->int32_t{live();auto*b=static_cast<const unsigned char*>(buf);f.credentials[key]=Bytes(b,b+n);return 0;};
 http.api_version=1;http.struct_size=sizeof(http);
 http.open=[](void*,const risc_http_request_v1*r,uint64_t*out)->int32_t{
  live();assert(!f.transfer);assert(r->utc_seconds&&f.link==WIFI_LINK_UP);f.transfer=123;*out=123;++f.gets;f.cursor=0;f.reads=0;
  if(std::string(r->url)==DAILY_LATEST)f.payload=f.invalid_meta?"{\"date\": false}":(f.meta.empty()?metadata(f.date):f.meta);
  else {assert(std::string(r->url)==url(f.date));++f.downloads;auto b=book();f.payload=f.html?"<html>server problem</html>":std::string(b.begin(),b.end());}
  return 0;
 };
 http.read=[](void*,uint64_t token,void*buf,uint32_t cap,uint32_t*out)->int32_t{
  live();assert(token==f.transfer);*out=0;f.ms+=100;++f.reads;
  if(f.http404)return RISC_HTTP_STATUS;
  if(f.httpfail&&f.downloads&&f.cursor>512)return RISC_HTTP_TRANSPORT;
  if(f.reads%3==1)return RISC_HTTP_AGAIN;
  if(f.cursor==f.payload.size())return RISC_HTTP_EOF;
  *out=uint32_t(std::min(size_t(cap),f.payload.size()-f.cursor));memcpy(buf,f.payload.data()+f.cursor,*out);f.cursor+=*out;return 0;
 };
 http.info=[](void*,uint64_t token,risc_http_response_v1*r)->int32_t{live();assert(token==f.transfer);r->status_code=f.http404?404:200;r->content_length=f.payload.size();r->received_bytes=uint32_t(f.cursor);return 0;};
 http.close=[](void*,uint64_t token)->int32_t{live();assert(token==f.transfer);if(f.retained_close)return RISC_HTTP_RETAINED;f.transfer=0;return 0;};
 files_api.api_version=1;files_api.struct_size=sizeof(files_api);
 files_api.open_request=[](const char*source,const char*id,uint64_t cookie){live();assert(!f.scene&&!f.transfer&&f.writing.empty()&&f.grants.size()==2);assert(std::string(id)=="ebook-reader");assert(std::string(source)=="/sd"+dest());assert(f.files.count(dest())&&!partials());if(f.fail_reader)return false;f.source=source;f.cookie=cookie;++f.opens;return true;};
 files_api.open_take_result=[](int32_t*error,uint64_t*cookie){live();if(!f.return_result)return false;f.return_result=false;*error=0;*cookie=f.cookie;return true;};
}
static unsigned passed=0;
static void done(const char*name,bool success){
 assert(f.terminal||(f.grants.empty()&&!f.scene&&!f.transfer&&f.writing.empty()));
 if(success)assert(f.opens==1&&f.files.at(dest())==book()&&!partials());
 else assert(!f.opens&&(!f.errors.empty()||f.cancel||f.terminal));
 printf("PASS %s\n",name);++passed;
}
static void badmeta(const std::string&s){char d[11],u[DAILY_URL_MAX];assert(!daily_metadata(s.data(),s.size(),d,u));}
int main(){
 initialize();char date[11],u[DAILY_URL_MAX];auto good=metadata("2024-02-29");assert(daily_metadata(good.data(),good.size(),date,u));
 badmeta(metadata("2026-02-29"));badmeta(metadata("0000-01-01"));badmeta(metadata("2026-13-01"));badmeta(metadata("2026-04-31"));badmeta("{}");badmeta("[]");badmeta("{\"date\":\"2026-10-10\",\"date\":\"2026-10-10\"}");badmeta(std::string(8193,' '));badmeta(good+"false");
 auto wrong=good;wrong.replace(wrong.find("raw.githubusercontent.com"),25,"evil.example.invalid");badmeta(wrong);puts("PASS metadata date/URL/JSON validation");++passed;
 reset();app_main();done("download and reader handoff",true);assert(f.updates>2&&f.disconnects==0);auto old=f.files.at(dest());
 auto gets=f.gets;f.return_result=true;app_main();assert(f.opens==1&&f.gets==gets&&f.grants.empty());puts("PASS Reader return does not redownload/reopen");++passed;
 reset();f.files[DAILY_DIRECTORY "/2026-10-09.epub"]=old;app_main();done("preserve other dates",true);assert(f.files.at(DAILY_DIRECTORY "/2026-10-09.epub")==old);
 reset();f.files[dest()]={1,2,3};app_main();done("same-day replacement",true);
 reset();f.short_write=true;app_main();done("partial SD writes",true);
 reset();f.files[part()]={1,2,3};app_main();done("remove stale incomplete transfer",true);
 reset();f.httpfail=true;f.files[dest()]=old;app_main();done("interrupted HTTP preserves existing paper",false);assert(f.files.at(dest())==old&&!partials());
 reset();f.http404=true;app_main();done("HTTP status error",false);assert(f.errors.back().find("404")!=std::string::npos);
 reset();f.http404=true;f.retry=true;app_main();done("Retry after transient HTTP failure",true);
 reset();f.invalid_meta=true;app_main();done("malformed metadata error",false);assert(!f.downloads&&!partials());
 reset();f.full=true;f.files[dest()]=old;app_main();done("SD full leaves original intact",false);assert(f.files.at(dest())==old&&!partials());
 reset();f.sync_fail=true;app_main();done("SD sync failure",false);assert(!partials());
 reset();f.replace_fail=true;f.files[dest()]=old;app_main();done("SD replacement failure",false);assert(f.files.at(dest())==old&&!partials());
 reset();f.cancel=true;app_main();done("cancel removes partial transfer",false);assert(!partials());
 reset();f.html=true;app_main();done("HTML is not opened as EPUB",false);assert(!partials());
 reset();f.clock=false;app_main();done("unset clock error",false);assert(!f.gets);
 reset();f.sd=false;app_main();done("missing SD error",false);assert(!f.gets);
 reset();f.retained_close=true;app_main();done("uncertain HTTP cleanup fences invocation",false);assert(f.terminal&&f.transfer&&f.releases==0);
 reset();f.fail_reader=true;app_main();done("Reader missing leaves paper saved",false);assert(f.files.at(dest())==book());
 reset();f.link=WIFI_LINK_DOWN;portable_wifi_credentials saved{};strcpy(saved.ssid,"Saved Network");strcpy(saved.password,"password123");assert(portable_wifi_credentials_save(&kv,&saved)==0);app_main();done("saved Wi-Fi connection and checked cleanup",true);assert(f.connections==1&&f.disconnects==1);
 reset();f.link=WIFI_LINK_DOWN;app_main();done("no saved Wi-Fi error",false);assert(!f.connections&&!f.gets);
 printf("%u actual-app/metadata scenarios passed.\n",passed);
}
