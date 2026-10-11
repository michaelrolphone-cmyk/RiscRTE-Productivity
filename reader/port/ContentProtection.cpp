#include <ContentProtection.h>
#include <ZipFile.h>
namespace freeink {namespace content {
std::unique_ptr<ContentDecryptor> openProtectedBook(const std::string& path,std::string& err){
 // Encrypted containers must never be mistaken for readable plain EPUBs.
 if(path.size()<5||readerCasecmp(path.c_str()+path.size()-5,".epub")!=0)return {};
 ZipFile zip(path);size_t n=0;
 if(zip.getInflatedFileSize("META-INF/encryption.xml",&n)&&n)err="Encrypted EPUB is not supported";
 return {};
}
}}
