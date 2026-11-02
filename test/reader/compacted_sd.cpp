// Apply the selected Runtime's ordinary application ELF/import validator to
// every metadata-compacted SD image, in addition to actual Reader admission.
#include "bootstrap/Runtime.h"
#undef STORE_ADMISSION_ENCODED_IMAGES
#include "cohort_elf_admission.h"
#include <cstdio>
int main(int argc,char**argv){
 if(argc<2)return 2;
 for(int i=1;i<argc;++i){
  std::ifstream input(argv[i],std::ios::binary);if(!input)return 3;
  std::vector<uint8_t> bytes(std::istreambuf_iterator<char>(input),{});
  if(!CohortElf::admitElf(bytes.data(),bytes.size()))return 4;
  bytes[0]=0;if(CohortElf::admitElf(bytes.data(),bytes.size()))return 5;
 }
 printf("metadata_sd_elfs=%d\n",argc-1);return 0;
}
