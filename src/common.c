#include "td_common.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

uint64_t td_hash64_bytes(const void *data, size_t len) {
    const unsigned char *bytes = (const unsigned char *)data;
    uint64_t hash = 1469598103934665603ULL;
    size_t i;

    for (i = 0; i < len; ++i) {
        hash ^= (uint64_t)bytes[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

uint64_t td_hash64_string(const char *text) {
    return td_hash64_bytes(text, strlen(text));
}

uint64_t td_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((uint64_t)ts.tv_sec * 1000000000ULL) + (uint64_t)ts.tv_nsec;
}

char *td_trim(char *text) {
    char *end;

    while (*text && isspace((unsigned char)*text)) {
        ++text;
    }
    if (*text == '\0') {
        return text;
    }

    end = text + strlen(text) - 1;
    while (end > text && isspace((unsigned char)*end)) {
        *end = '\0';
        --end;
    }
    return text;
}

static int td_hex_digit(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return 10 + (ch - 'a');
    }
    if (ch >= 'A' && ch <= 'F') {
        return 10 + (ch - 'A');
    }
    return -1;
}

int td_hex_to_bytes(const char *hex, unsigned char *out, size_t out_len) {
    size_t hex_len = strlen(hex);
    size_t i;

    if (hex_len != (out_len * 2)) {
        return -1;
    }

    for (i = 0; i < out_len; ++i) {
        int hi = td_hex_digit(hex[i * 2]);
        int lo = td_hex_digit(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) {
            return -1;
        }
        out[i] = (unsigned char)((hi << 4) | lo);
    }
    return 0;
}

void td_format_error(char *buf, size_t buf_len, const char *fmt, ...) {
    va_list args;

    if (buf_len == 0) {
        return;
    }

    va_start(args, fmt);
    vsnprintf(buf, buf_len, fmt, args);
    va_end(args);
}

int td_parse_host_port(const char *input, td_endpoint_t *endpoint) {
    char scratch[128];
    char *sep;

    if (strlen(input) >= sizeof(scratch)) {
        return -1;
    }

    snprintf(scratch, sizeof(scratch), "%s", input);
    sep = strrchr(scratch, ':');
    if (sep == NULL) {
        return -1;
    }

    *sep = '\0';
    snprintf(endpoint->host, sizeof(endpoint->host), "%s", td_trim(scratch));
    endpoint->port = atoi(td_trim(sep + 1));
    endpoint->node_id = -1;

    if (endpoint->host[0] == '\0' || endpoint->port <= 0) {
        return -1;
    }
    return 0;
}
