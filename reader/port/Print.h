#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
class Print {
public:
 virtual ~Print()=default;
 virtual size_t write(uint8_t)=0;
 virtual size_t write(const uint8_t* p,size_t n){size_t i=0;for(;i<n&&write(p[i]);++i){}return i;}
 size_t write(const char* p){return write(reinterpret_cast<const uint8_t*>(p),strlen(p));}
 size_t print(const char* p){return write(p);}
 size_t println(const char* p){return print(p)+write(uint8_t('\n'));}
};
