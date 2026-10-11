#pragma once
#include <stdbool.h>
#include <stddef.h>
#define DAILY_BASE "https://raw.githubusercontent.com/michaelrolphone-cmyk/Daily-Digest-Epub/main"
#define DAILY_LATEST DAILY_BASE "/latest.json"
#define DAILY_DIRECTORY "/Books/Daily Digest"
#define DAILY_METADATA_MAX 8192u
#define DAILY_URL_MAX 512u
#ifdef __cplusplus
extern "C" {
#endif
bool daily_metadata(const char *, size_t, char date[11], char url[DAILY_URL_MAX]);
#ifdef __cplusplus
}
#endif
