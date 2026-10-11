#pragma once
#include "Arduino.h"
void readerLog(const char*,const char*,...);
#define LOG_ERR(tag,fmt,...) readerLog(tag,fmt,##__VA_ARGS__)
#define LOG_INF(tag,fmt,...) readerLog(tag,fmt,##__VA_ARGS__)
#define LOG_DBG(tag,fmt,...) ((void)0)
