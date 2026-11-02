// Test-only owner checkpoint. Compile the exact selected Runtime implementation
// in this TU so the fixture can enter the same native installation broker.
// No production Runtime source or policy is changed and no target ELF runs.
#define private public
#include "bootstrap/Runtime.h"
#undef private
#include "bootstrap/Runtime.cpp"

extern "C" bool reader_test_catalog_admit(RiscBoot::Runtime* r,const char* json) {
  using RiscBoot::Runtime;
  if(!r || !r->installation_ || currentRuntime || r->invocation_->active_)return false;
  const size_t before=r->policyCount_;
  currentRuntime=r;r->invocation_->active_=true;
  char route[193]{};
  const auto status=Runtime::installationAdmit(r,"Apps/ebook-reader/manifest.json",json,route,sizeof(route));
  bool ok=status==RISC_INSTALL_OK && !strcmp(route,"Apps/ebook-reader/ebook_reader.elf") && r->policyCount_==before+1;
  if(ok) {
    const auto& p=r->policies_[before];
    ok=p.enabled && !p.catalogAuthority && !strcmp(p.id,"ebook-reader") &&
       p.sourceDriver>=0 && r->drivers_[p.sourceDriver].instance==9 &&
       !strcmp(p.sourcePath,"/Apps/ebook-reader/ebook_reader.elf") && p.count==5 &&
       r->residentForeground_[before] && !r->residentLegacy_[before] &&
       r->fileHandlers_[before].count==3;
    const char* capabilities[]={"ui.scene","storage.volume","memory.heap","random.bytes","file.open"};
    for(size_t c=0;c<5;++c) {
      unsigned count=0;
      for(size_t g=0;g<p.count;++g)if(!strcmp(p.grants[g].capability,capabilities[c]) &&
        p.grants[g].api==1 && p.grants[g].instance==(c==1?9u:0u))++count;
      ok=ok && count==1;
    }
    for(const char* type:{".epub",".txt",".md"}) {
      unsigned count=0;
      for(size_t t=0;t<r->fileHandlers_[before].count;++t)
        if(!strcmp(type,r->fileHandlers_[before].types[t]))++count;
      ok=ok && count==1;
    }
    // Idempotent discovery retains one installed row and the exact launch route.
    char repeated[193]{};
    ok=ok && Runtime::installationAdmit(r,"Apps/ebook-reader/manifest.json",json,repeated,sizeof(repeated))==RISC_INSTALL_OK &&
       !strcmp(route,repeated) && r->policyCount_==before+1;
    // SD manifests cannot extend the immutable reservation's authority.
    ArduinoJson::JsonDocument bad;
    if(deserializeJson(bad,json))ok=false;
    auto extra=bad["requires"].add<ArduinoJson::JsonObject>();
    extra["capability"]="reader.ungranted";extra["api"]=1;
    std::string invalid;serializeJson(bad,invalid);
    ok=ok && Runtime::installationAdmit(r,"Apps/ebook-reader/manifest.json",invalid.c_str(),repeated,sizeof(repeated))==RISC_INSTALL_DENIED &&
       r->policyCount_==before+1;
  }
  if(!ok)fprintf(stderr,"Reader catalog failed status=%d route=%s before=%zu after=%zu error=%s\n",int(status),route,before,r->policyCount_,r->error());
  r->invocation_->active_=false;currentRuntime=nullptr;
  return ok;
}
