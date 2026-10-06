#include "../Apps/timecard_portable_validation.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static char document[TCP_JSON_MAX_BYTES + 2u];

static bool valid(const char *text) {
    return tcp_validate_json(text, strlen(text));
}

static void reject_time(const char *text) {
    int16_t minutes = 321;
    assert(!tcp_parse_time(text, &minutes));
    assert(minutes == 321);
}

static void expect_time(const char *text, int16_t expected) {
    int16_t minutes = -2;
    assert(tcp_parse_time(text, &minutes));
    assert(minutes == expected);
}

static void schema_tests(void) {
    static const char *const accepted[] = {
        "{\"days\":[]}", " \r\n\t{ \"days\" : [ ] } \t\n",
        "{\"days\":[{\"d\":19700101}]}",
        "{\"days\":[{\"d\":99991231,\"in\":0,\"out\":1439,\"ls\":-1,\"le\":-0}]}",
        "{\"days\":[{\"out\":1020,\"ls\":720,\"in\":480,\"le\":750,\"d\":20261005},"
        "{\"d\":20000229,\"in\":-1},{\"d\":24000229},{\"d\":21000301}]}",
    };
    static const char *const rejected[] = {
        "", " ", "null", "[]", "{}", "{\"day\":[]}", "{\"days\":null}",
        "{\"days\":[]", "{\"days\":[}", "{\"days\":[{}]}", "{\"days\":[1]}",
        "{\"days\":[{\"in\":480}]}", "{\"days\":[{\"d\":19691231}]}",
        "{\"days\":[{\"d\":100000101}]}", "{\"days\":[{\"d\":20260230}]}",
        "{\"days\":[{\"d\":21000229}]}", "{\"days\":[{\"d\":20260001}]}",
        "{\"days\":[{\"d\":20261301}]}", "{\"days\":[{\"d\":20260431}]}",
        "{\"days\":[{\"d\":20260100}]}", "{\"days\":[{\"d\":-20260101}]}",
        "{\"days\":[{\"d\":2147483648}]}", "{\"days\":[{\"d\":-2147483648}]}",
        "{\"days\":[{\"d\":99999999999999999999999999999999999999}]}",
        "{\"days\":[{\"d\":20261005,\"in\":1439junk}]}",
        "{\"days\":[{\"d\":20261005,\"in\":1440}]}",
        "{\"days\":[{\"d\":20261005,\"in\":65536}]}",
        "{\"days\":[{\"d\":20261005,\"in\":-2}]}",
        "{\"days\":[{\"d\":20261005,\"in\":-2147483648}]}",
        "{\"days\":[{\"d\":20261005,\"in\":01}]}",
        "{\"days\":[{\"d\":20261005,\"in\":-01}]}",
        "{\"days\":[{\"d\":020261005}]}",
        "{\"days\":[{\"d\":20261005,\"in\":+1}]}",
        "{\"days\":[{\"d\":20261005,\"in\":1.0}]}",
        "{\"days\":[{\"d\":20261005,\"in\":1e2}]}",
        "{\"days\":[{\"d\":20261005,\"in\":null}]}",
        "{\"days\":[{\"d\":20261005,\"in\":true}]}",
        "{\"days\":[{\"d\":20261005,\"in\":\"480\"}]}",
        "{\"days\":[{\"d\":20261005,\"in\":[]}]}",
        "{\"days\":[{\"d\":20261005,\"note\":\"keep me\"}]}",
        "{\"days\":[{\"d\":20261005,\"d\":20261006}]}",
        "{\"days\":[{\"d\":20261005,\"in\":1,\"in\":2}]}",
        "{\"days\":[{\"d\":20261005,\"ls\":1,\"ls\":2}]}",
        "{\"days\":[{\"d\":20261005,\"le\":1,\"le\":2}]}",
        "{\"days\":[{\"d\":20261005,\"out\":1,\"out\":2}]}",
        "{\"days\":[{\"d\":20261005},{\"d\":20261005}]}",
        "{\"days\":[{\"d\":20261005},{\"d\":20261006},{\"in\":1,\"d\":20261005}]}",
        "{\"days\":[{\"d\":20261005}{\"d\":20261006}]}",
        "{\"days\":[{\"d\":20261005},]}", "{\"days\":[,{\"d\":20261005}]}",
        "{\"days\":[{\"d\":20261005,}]}", "{\"days\":[{\"d\" 20261005}]}",
        "{\"days\":[{\"d\":20261005 \"in\":480}]}",
        "{\"days\":[],\"days\":[]}", "{\"days\":[],\"note\":1}",
        "{\"days\":[]}{}", "{\"days\":[]}junk", "junk\"days\":[]}",
        "{\"\\u0064ays\":[]}", "{\"days\":[{\"\\u0064\":20261005}]}",
        "{\"days\":[]\v}", "{\"days\":[]\f}", "{\"days\":[]\x01}",
    };
    for (size_t i = 0; i < sizeof(accepted) / sizeof(*accepted); ++i) assert(valid(accepted[i]));
    for (size_t i = 0; i < sizeof(rejected) / sizeof(*rejected); ++i) assert(!valid(rejected[i]));
    assert(!tcp_validate_json(NULL, 0));
    assert(!tcp_validate_json(NULL, 10));
    assert(!tcp_validate_json("", SIZE_MAX));
    assert(!tcp_validate_json("{\"days\":[]}", sizeof("{\"days\":[]}")));
    const char embedded[] = "{\"days\":[\0]}";
    assert(!tcp_validate_json(embedded, sizeof(embedded) - 1));
    const char exact[] = {'{','"','d','a','y','s','"',':','[',']','}'};
    assert(tcp_validate_json(exact, sizeof(exact)));
    for (size_t n = 0; n < sizeof(exact); ++n) assert(!tcp_validate_json(exact, n));
}

static size_t make_history(unsigned count, bool duplicate_last) {
    size_t used = (size_t)snprintf(document, sizeof(document), "{\"days\":[");
    for (unsigned i = 0; i < count; ++i) {
        unsigned slot = duplicate_last && i + 1 == count ? 0 : i;
        unsigned date = (1970u + slot / 12u) * 10000u + (1u + slot % 12u) * 100u + 1u;
        int n = snprintf(document + used, sizeof(document) - used,
            "%s{\"d\":%u,\"in\":480,\"ls\":720,\"le\":750,\"out\":1020}", i ? "," : "", date);
        assert(n > 0 && (size_t)n < sizeof(document) - used);
        used += (size_t)n;
    }
    assert(used + 3 < sizeof(document));
    memcpy(document + used, "]}", 3);
    return used + 2;
}

static void capacity_tests(void) {
    size_t size = make_history(400, false);
    assert(tcp_validate_json(document, size));
    memset(document + size, ' ', TCP_JSON_MAX_BYTES + 1u - size);
    assert(tcp_validate_json(document, TCP_JSON_MAX_BYTES - 1u));
    assert(tcp_validate_json(document, TCP_JSON_MAX_BYTES));
    assert(!tcp_validate_json(document, TCP_JSON_MAX_BYTES + 1u));
    size = make_history(401, false);
    assert(!tcp_validate_json(document, size));
    size = make_history(400, true);
    assert(!tcp_validate_json(document, size));
    size = make_history(1, false);
    for (size_t n = 0; n < size; ++n) assert(!tcp_validate_json(document, n));
}

static void time_tests(void) {
    static const char *const rejected[] = {
        "24", "99", "000", "0008", "-1", "+8", "8:", ":30", "8:000", "12:345",
        "8:60", "8:99", "1.5", "8 :30", "8: 30", "8:30:00", "8 AM PM", "8APM", "8 AP",
        "8a", "8 p", "8 A.M.", "8 PMz", "8 PM PM", "0am", "00 PM", "13 AM", "24PM",
        "12:30 garbage", "8junk", "08:00 xyz", "1e1", "8\v", "8\f", "8\x01",
        "9999999999999999999999999999999999999999999999999999999999999999",
    };
    for (size_t i = 0; i < sizeof(rejected) / sizeof(*rejected); ++i) reject_time(rejected[i]);
    expect_time("", -1);
    expect_time(" \t\n\r", -1);
    expect_time("0", 0);
    expect_time("00", 0);
    expect_time("23", 1380);
    expect_time("8:3", 483);
    expect_time("8am", 480);
    expect_time("8Am", 480);
    expect_time("8aM", 480);
    expect_time("8PM", 1200);
    expect_time("12 AM", 0);
    expect_time("12 PM", 720);
    expect_time(" \t12:05 pM \r\n", 725);
    char text[80];
    for (unsigned minute = 0; minute < 1440; ++minute) {
        unsigned h = minute / 60u, m = minute % 60u;
        snprintf(text, sizeof(text), "%02u:%02u", h, m);
        expect_time(text, (int16_t)minute);
        snprintf(text, sizeof(text), "%u:%02u %s", h % 12u ? h % 12u : 12u, m, h < 12u ? "AM" : "PM");
        expect_time(text, (int16_t)minute);
    }
    memset(text, ' ', sizeof(text));
    text[TCP_TIME_MAX_BYTES] = 0;
    expect_time(text, -1);
    text[TCP_TIME_MAX_BYTES] = ' ';
    text[TCP_TIME_MAX_BYTES + 1u] = 0;
    reject_time(text);
    int16_t unchanged = 321;
    assert(!tcp_parse_time(NULL, &unchanged) && unchanged == 321);
    assert(!tcp_parse_time("8", NULL));
}

static void mutation_tests(void) {
    static const char seed[] = "{\"days\":[{\"d\":20261005,\"in\":480,\"out\":1020}]}";
    char copy[sizeof(seed)], before[sizeof(seed)];
    for (size_t at = 0; at < sizeof(seed) - 1; ++at) {
        for (unsigned ch = 0; ch < 256; ++ch) {
            memcpy(copy, seed, sizeof(seed));
            copy[at] = (char)ch;
            memcpy(before, copy, sizeof(copy));
            (void)tcp_validate_json(copy, sizeof(copy) - 1);
            assert(memcmp(before, copy, sizeof(copy)) == 0);
        }
    }
}

int main(void) {
    schema_tests();
    capacity_tests();
    time_tests();
    mutation_tests();
    puts("Timecard portable validation: schema, 400 days, 49,151 bytes, times and immutable mutation sweep passed");
    return 0;
}
