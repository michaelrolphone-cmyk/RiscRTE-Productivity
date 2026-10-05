#pragma once
/* Fail-closed input guards for the portable profile. The original Timecard
 * model/parser remains authoritative after these guards accept its input.
 * No allocation, storage, runtime, UI, or mutation of caller-owned input. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TCP_JSON_MAX_BYTES 49151u
#define TCP_HISTORY_MAX_DAYS 400u
#define TCP_TIME_MAX_BYTES 63u

static inline bool tcpv_space(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

static inline void tcpv_skip_space(const char **at, const char *end) {
    while (*at < end && tcpv_space(**at)) ++*at;
}

static inline bool tcpv_take(const char **at, const char *end, char expected) {
    tcpv_skip_space(at, end);
    if (*at == end || **at != expected) return false;
    ++*at;
    return true;
}

static inline bool tcpv_literal(const char **at, const char *end, const char *text) {
    const char *p = *at;
    while (*text) {
        if (p == end || *p++ != *text++) return false;
    }
    *at = p;
    return true;
}

static inline bool tcpv_date(int32_t date) {
    static const uint8_t lengths[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (date < 19700101 || date > 99991231) return false;
    unsigned year = (unsigned)date / 10000u;
    unsigned month = (unsigned)date / 100u % 100u;
    unsigned day = (unsigned)date % 100u;
    if (month < 1 || month > 12 || day < 1) return false;
    unsigned limit = lengths[month - 1];
    if (month == 2 && year % 4u == 0 && (year % 100u != 0 || year % 400u == 0)) ++limit;
    return day <= limit;
}

/* All valid schema integers fit -1..99991231. Check before multiplication;
 * accepting longer integer tokens then narrowing would already be too late. */
static inline bool tcpv_integer(const char **at, const char *end, int32_t *out) {
    const char *p = *at;
    tcpv_skip_space(&p, end);
    bool negative = p < end && *p == '-';
    if (negative) ++p;
    if (p == end || *p < '0' || *p > '9') return false;
    bool leading_zero = *p == '0';
    uint32_t value = 0, limit = negative ? 1u : 99991231u;
    unsigned digits = 0;
    while (p < end && *p >= '0' && *p <= '9') {
        uint32_t digit = (uint32_t)(*p - '0');
        if ((leading_zero && digits) || digit > limit || value > (limit - digit) / 10u) return false;
        value = value * 10u + digit;
        ++p;
        ++digits;
    }
    *out = negative ? -(int32_t)value : (int32_t)value;
    *at = p;
    return true;
}

static inline bool tcpv_record(const char **at, const char *end, int32_t *date_out) {
    const char *p = *at;
    unsigned fields = 0;
    int32_t date = 0;
    if (!tcpv_take(&p, end, '{')) return false;
    for (unsigned n = 0; n < 5; ++n) {
        tcpv_skip_space(&p, end);
        unsigned field;
        if (tcpv_literal(&p, end, "\"d\"")) field = 1u;
        else if (tcpv_literal(&p, end, "\"in\"")) field = 2u;
        else if (tcpv_literal(&p, end, "\"ls\"")) field = 4u;
        else if (tcpv_literal(&p, end, "\"le\"")) field = 8u;
        else if (tcpv_literal(&p, end, "\"out\"")) field = 16u;
        else return false;
        if (fields & field) return false;
        fields |= field;
        int32_t value;
        if (!tcpv_take(&p, end, ':') || !tcpv_integer(&p, end, &value)) return false;
        if (field == 1u) {
            if (!tcpv_date(value)) return false;
            date = value;
        } else if (value < -1 || value > 1439) return false;
        tcpv_skip_space(&p, end);
        if (p == end) return false;
        if (*p == '}') {
            if (!(fields & 1u)) return false;
            *date_out = date;
            *at = p + 1;
            return true;
        }
        if (*p++ != ',') return false;
    }
    return false;
}

static inline bool tcp_validate_json(const char *data, size_t size) {
    if (!data || !size || size > TCP_JSON_MAX_BYTES) return false;
    const char *p = data, *end = data + size;
    if (!tcpv_take(&p, end, '{')) return false;
    tcpv_skip_space(&p, end);
    if (!tcpv_literal(&p, end, "\"days\"") ||
        !tcpv_take(&p, end, ':') || !tcpv_take(&p, end, '[')) return false;
    const char *records = p;
    unsigned count = 0;
    tcpv_skip_space(&p, end);
    if (p < end && *p != ']') {
        for (;;) {
            const char *record_start = p;
            int32_t date;
            if (count == TCP_HISTORY_MAX_DAYS || !tcpv_record(&p, end, &date)) return false;
            /* At most 400 records: bounded rescan avoids a large stack array.
             * Prior records were validated and are only read again here. */
            const char *prior = records;
            for (unsigned i = 0; i < count; ++i) {
                int32_t prior_date;
                if (!tcpv_record(&prior, record_start, &prior_date) || prior_date == date) return false;
                if (i + 1 < count && !tcpv_take(&prior, record_start, ',')) return false;
            }
            ++count;
            tcpv_skip_space(&p, end);
            if (p == end) return false;
            if (*p == ']') break;
            if (*p++ != ',') return false;
        }
    }
    if (!tcpv_take(&p, end, ']') || !tcpv_take(&p, end, '}')) return false;
    tcpv_skip_space(&p, end);
    return p == end;
}

/* Accept H/HH with optional :M/:MM, and an optional exact AM/PM suffix
 * (case-insensitive, with optional intervening whitespace). Blank clears.
 * Reject the legacy parser's ignored junk and ambiguous single-letter suffixes.
 * Bounded to the original 64-byte keyboard-result buffer, including its NUL. */
static inline bool tcp_parse_time(const char *text, int16_t *minutes) {
    if (!text || !minutes) return false;
    size_t size = 0;
    while (size < TCP_TIME_MAX_BYTES && text[size]) ++size;
    if (text[size]) return false;
    const char *p = text, *end = text + size;
    tcpv_skip_space(&p, end);
    if (p == end) {
        *minutes = -1;
        return true;
    }
    unsigned hour = 0, minute = 0, digits = 0;
    while (p < end && *p >= '0' && *p <= '9') {
        if (digits == 2) return false;
        hour = hour * 10u + (unsigned)(*p++ - '0');
        ++digits;
    }
    if (!digits) return false;
    if (p < end && *p == ':') {
        ++p;
        digits = 0;
        while (p < end && *p >= '0' && *p <= '9') {
            if (digits == 2) return false;
            minute = minute * 10u + (unsigned)(*p++ - '0');
            ++digits;
        }
        if (!digits) return false;
    }
    if (minute > 59) return false;
    tcpv_skip_space(&p, end);
    if (p < end) {
        bool pm = *p == 'p' || *p == 'P';
        if (!pm && *p != 'a' && *p != 'A') return false;
        ++p;
        if (p == end || (*p != 'm' && *p != 'M')) return false;
        ++p;
        if (hour < 1 || hour > 12) return false;
        hour = hour % 12u + (pm ? 12u : 0u);
        tcpv_skip_space(&p, end);
        if (p != end) return false;
    } else if (hour > 23) return false;
    *minutes = (int16_t)(hour * 60u + minute);
    return true;
}
