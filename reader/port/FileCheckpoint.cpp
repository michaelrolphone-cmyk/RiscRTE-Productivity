#include "HalStorage.h"
#include "Logging.h"
#include <utility>

HalFile* HalFile::first_=nullptr;
void HalFile::link(){if(listed_)return;next_=first_;if(next_)next_->previous_=this;first_=this;previous_=nullptr;listed_=true;}
void HalFile::unlink(){if(!listed_)return;if(previous_)previous_->next_=next_;else first_=next_;if(next_)next_->previous_=previous_;previous_=next_=nullptr;listed_=false;}
void HalFile::take(HalFile& o){
 handle_=o.handle_;flags_=o.flags_;directory_=o.directory_;path_=std::move(o.path_);
 parked_=o.parked_;readOnly_=o.readOnly_;length_=o.length_;cursor_=o.cursor_;physical_=o.physical_;
 parkPosition_=o.parkPosition_;parkSize_=o.parkSize_;parkModified_=o.parkModified_;parkModifiedValid_=o.parkModifiedValid_;
 cache_=std::move(o.cache_);cacheStart_=o.cacheStart_;cacheSize_=o.cacheSize_;windowSize_=o.windowSize_;
 o.unlink();if(handle_||parked_)link();o.handle_=0;o.parked_=false;o.cacheSize_=0;
}
bool HalFile::park(){
 if(parked_||!handle_)return true;if(!reader::alive())return false;
 const auto* v=reader::volume();const auto* f=risc_storage_volume_fs(&v->base);
 // Finish every active write before releasing its token. Reopening never uses
 // CREATE/TRUNCATE/EXCLUSIVE/APPEND: it must not replay a destructive open.
 if(!directory_&&(flags_&O_WRONLY)&&!v->file_sync(v->base.context,handle_))return false;
 if(!reader::alive())return false;
 if(directory_){if(!f->dir_tell(v->base.context,handle_,&parkPosition_))return false;parkSize_=0;}
 else if(readOnly_){parkPosition_=cursor_;parkSize_=length_;}
 else if(!v->file_info(v->base.context,handle_,&parkSize_,&parkPosition_))return false;
 if(!reader::alive())return false;
 risc_storage_metadata_v1 m{};m.struct_size=sizeof(m);
 if(f->metadata(v->base.context,path_.c_str(),&m)!=RISC_STORAGE_FS_OK||!reader::alive())return false;
 if(bool(m.flags&RISC_STORAGE_DIRECTORY)!=directory_||(!directory_&&m.size!=parkSize_))return false;
 parkModifiedValid_=m.flags&RISC_STORAGE_MODIFIED_VALID;parkModified_=m.modified_seconds;
 bool closed=directory_?v->dir_close_checked(v->base.context,handle_):v->base.file_close(v->base.context,handle_,true);
 if(!closed||!reader::alive())return false;
 handle_=0;parked_=true;return true;
}
bool HalFile::unpark(){
 if(!parked_)return true;if(!reader::alive())return false;
 const auto* v=reader::volume();const auto* f=risc_storage_volume_fs(&v->base);
 risc_storage_metadata_v1 m{};m.struct_size=sizeof(m);
 if(f->metadata(v->base.context,path_.c_str(),&m)!=RISC_STORAGE_FS_OK||!reader::alive())return false;
 if(bool(m.flags&RISC_STORAGE_DIRECTORY)!=directory_||(!directory_&&m.size!=parkSize_)||
    (parkModifiedValid_&&(!(m.flags&RISC_STORAGE_MODIFIED_VALID)||m.modified_seconds!=parkModified_)))return false;
 handle_=directory_?v->base.dir_open(v->base.context,path_.c_str()):v->file_open(v->base.context,path_.c_str(),flags_&(O_RDONLY|O_WRONLY));
 if(!handle_||!reader::alive())return false;
 parked_=false;cacheSize_=0; // no stale read window crosses a handoff
 if(directory_)return f->dir_seek(v->base.context,handle_,parkPosition_)&&reader::alive();
 uint64_t size=0,pos=0;
 if(!v->file_info(v->base.context,handle_,&size,&pos)||!reader::alive()||size!=parkSize_)return false;
 if(!v->file_seek(v->base.context,handle_,parkPosition_)||!reader::alive())return false;
 cursor_=physical_=parkPosition_;length_=size;return true;
}
bool HalFile::parkAll(){
 for(auto* p=first_;p;p=p->next_)if(!p->park()){LOG_ERR("checkpoint","file park failed; retaining invocation");reader::retain();return false;}
 return reader::alive();
}
bool HalFile::unparkAll(){
 for(auto* p=first_;p;p=p->next_)if(!p->unpark()){LOG_ERR("checkpoint","file changed or resume failed; retaining invocation");reader::retain();return false;}
 return reader::alive();
}
