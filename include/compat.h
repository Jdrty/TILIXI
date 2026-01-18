#pragma once
#include <stdlib.h>
#include <string.h>

static inline char *tilixi_strdup(const char *s) {
    if (s == NULL) {
        return NULL;
    }
    size_t len = strlen(s) + 1;
    char *out = (char*)malloc(len);
    if (out == NULL) {
        return NULL;
    }
    memcpy(out, s, len);
    return out;
}

#ifndef strdup
#define strdup tilixi_strdup
#endif

static inline size_t tilixi_strnlen(const char *s, size_t maxlen) {
    size_t len = 0;
    if (s == NULL) {
        return 0;
    }
    while (len < maxlen && s[len] != '\0') {
        len++;
    }
    return len;
}

#ifndef strnlen
#define strnlen tilixi_strnlen
#endif

