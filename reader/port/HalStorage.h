#pragma once
#include "Arduino.h"
#include "ReaderPort.h"
#include <memory>
#include <string>
using oflag_t=uint32_t;
constexpr oflag_t O_RDONLY=RISC_STORAGE_OPEN_READ,O_WRONLY=RISC_STORAGE_OPEN_WRITE,
 O_RDWR=O_RDONLY|O_WRONLY,O_CREAT=RISC_STORAGE_OPEN_CREATE,O_TRUNC=RISC_STORAGE_OPEN_TRUNCATE,
 O_EXCL=RISC_STORAGE_OPEN_EXCLUSIVE,O_APPEND=RISC_STORAGE_OPEN_APPEND;
class HalFile : public Print {
 uint32_t handle_=0;bool directory_=false;std::string path_;
 friend class HalStorage;
 public:
 HalFile()=default;~HalFile(){close();}
 HalFile(HalFile&& other) noexcept;
 HalFile& operator=(HalFile&& other) noexcept;
 HalFile(const HalFile&)=delete;HalFile& operator=(const HalFile&)=delete;
 bool close();bool isOpen()const{return handle_!=0&&reader::alive();} operator bool()const{return isOpen();}
 bool isDirectory()const{return directory_;}
 size_t getName(char*,size_t);uint64_t fileSize64();size_t size(){return fileSize64();}size_t fileSize(){return size();}
 size_t position()const;uint32_t modificationTime();
 bool seek64(uint64_t);bool seek(size_t p){return seek64(p);}bool seekSet(size_t p){return seek64(p);}bool seekCur(int64_t);
 int available()const;int read(void*,size_t);int read(){uint8_t c;return read(&c,1)==1?c:-1;}
 size_t write(const uint8_t*,size_t)override;size_t write(const void* p,size_t n){return write((const uint8_t*)p,n);}size_t write(uint8_t c)override{return write(&c,1);}
 using Print::write;
 void flush();bool rename(const char*);bool truncate(uint64_t);
 void rewindDirectory();HalFile openNextFile();
};
class HalStorage {
public:
 static HalStorage& getInstance();
 bool ready()const;
 HalFile open(const char*,oflag_t=O_RDONLY);
 bool exists(const char*);bool mkdir(const char*,bool=true);bool ensureDirectoryExists(const char* p){return mkdir(p);}
 bool remove(const char*);bool rmdir(const char* p){return remove(p);}bool rename(const char*,const char*);bool replaceFile(const char*,const char*);bool recoverFile(const char*);
 bool removeDir(const char*);
 bool openFileForRead(const char*,const char*,HalFile&);bool openFileForRead(const char* m,const std::string& p,HalFile& f){return openFileForRead(m,p.c_str(),f);}
 bool openFileForWrite(const char*,const char*,HalFile&);bool openFileForWrite(const char* m,const std::string& p,HalFile& f){return openFileForWrite(m,p.c_str(),f);}
 bool readFileToString(const char*,const std::string&,size_t,std::string&);
};
#define Storage HalStorage::getInstance()
