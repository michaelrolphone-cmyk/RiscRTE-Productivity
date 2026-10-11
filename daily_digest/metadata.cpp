#include "metadata.h"
#include "JsonCursor.h"
#include <cstdio>
#include <cstring>

static bool valid_date(const char *s) {
    if (std::strlen(s) != 10 || s[4] != '-' || s[7] != '-') return false;
    for (unsigned i = 0; i < 10; ++i)
        if (i != 4 && i != 7 && (s[i] < '0' || s[i] > '9')) return false;
    unsigned y = unsigned(s[0]-'0')*1000 + unsigned(s[1]-'0')*100 + unsigned(s[2]-'0')*10 + unsigned(s[3]-'0');
    unsigned m = unsigned(s[5]-'0')*10 + unsigned(s[6]-'0');
    unsigned d = unsigned(s[8]-'0')*10 + unsigned(s[9]-'0');
    const unsigned days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    return y && m >= 1 && m <= 12 && d && d <= days[m-1] +
        (m == 2 && y%4 == 0 && (y%100 != 0 || y%400 == 0));
}
static bool valid_json(const char *json, size_t length) {
    WatchUpdate::WorkBudget budget(nullptr, nullptr);
    WatchUpdate::Cursor cursor(json, length, budget);
    return cursor.objectOnly();
}
extern "C" bool daily_metadata(const char *json, size_t length, char date[11], char url[DAILY_URL_MAX]) {
    date[0] = url[0] = 0;
    if (!json || !length || length > DAILY_METADATA_MAX) return false;
    /* Reuse the existing update-service cursor for strict JSON, duplicate keys,
     * Unicode in ignored text, and bounded nesting. No catalog or hash code. */
    WatchUpdate::WorkBudget budget(nullptr, nullptr);
    if (!valid_json(json, length)) return false;
    WatchUpdate::Cursor cursor(json, length, budget);
    if (!cursor.take('{')) return false;
    bool more = true;
    if (cursor.take('}')) return false;
    while (more) {
        char key[64];
        if (!cursor.key(key, sizeof(key))) return false;
        if (!std::strcmp(key, "date")) {
            if (!cursor.text(date, 11)) return false;
        } else if (!std::strcmp(key, "download_url")) {
            if (!cursor.text(url, DAILY_URL_MAX)) return false;
        } else if (!cursor.skip()) return false;
        if (!cursor.next('}', more)) return false;
    }
    if (!cursor.end() || !valid_date(date)) return false;
    /* The published repository contract binds the file to its edition date.
     * Do not label latest.epub with a date from a different cached response. */
    char expected[DAILY_URL_MAX];
    int n = std::snprintf(expected, sizeof(expected), DAILY_BASE
        "/editions/%.4s/%.2s/%.2s/daily-digest-%s.epub", date, date+5, date+8, date);
    return n > 0 && size_t(n) < sizeof(expected) && !std::strcmp(url, expected);
}
