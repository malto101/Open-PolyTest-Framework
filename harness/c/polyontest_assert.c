/**
 * PolyOnTest Core — assertion implementations
 * Copyright 2026 Dhruv Menon
 * SPDX-License-Identifier: Apache-2.0
 */
#include "polyontest/polyontest.h"

#ifdef POLYONTEST_FREESTANDING
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
int memcmp(const void *a, const void *b, size_t n);
#else
#include <string.h>
#endif

static size_t pt_as_append_str(char *dst, size_t cap, size_t at, const char *s) {
    if (!s || at >= cap) {
        return at;
    }
    while (*s && at + 1 < cap) {
        dst[at++] = *s++;
    }
    dst[at] = '\0';
    return at;
}

/* Base-10 digit without 64-bit hardware/libgcc division (bare-metal friendly). */
static void pt_as_u64_div10(uint64_t *u, unsigned *digit) {
    uint64_t q = 0;
    uint64_t r = 0;
    int bit;
    for (bit = 63; bit >= 0; --bit) {
        r = (r << 1) | ((*u >> (unsigned)bit) & 1ull);
        if (r >= 10ull) {
            r -= 10ull;
            q |= (uint64_t)1 << (unsigned)bit;
        }
    }
    *digit = (unsigned)r;
    *u = q;
}

static size_t pt_as_append_u64(char *dst, size_t cap, size_t at, uint64_t u) {
    char tmp[32];
    int i = 0;
    if (u == 0) {
        tmp[i++] = '0';
    } else {
        while (u > 0 && i < (int)sizeof(tmp)) {
            unsigned dig = 0;
            pt_as_u64_div10(&u, &dig);
            tmp[i++] = (char)('0' + dig);
        }
    }
    while (i > 0 && at + 1 < cap) {
        dst[at++] = tmp[--i];
    }
    dst[at] = '\0';
    return at;
}

static size_t pt_as_append_i64(char *dst, size_t cap, size_t at, int64_t v) {
    uint64_t u;
    if (v < 0) {
        if (at + 1 < cap) {
            dst[at++] = '-';
            dst[at] = '\0';
        }
        u = (uint64_t)(-(v + 1)) + 1ull;
    } else {
        u = (uint64_t)v;
    }
    return pt_as_append_u64(dst, cap, at, u);
}

static size_t pt_as_append_hex(char *dst, size_t cap, size_t at, uint64_t u,
                               int nibbles) {
    static const char *digits = "0123456789ABCDEF";
    int i;
    at = pt_as_append_str(dst, cap, at, "0x");
    if (nibbles < 1) {
        nibbles = 1;
    }
    if (nibbles > 16) {
        nibbles = 16;
    }
    for (i = nibbles - 1; i >= 0; --i) {
        if (at + 1 < cap) {
            dst[at++] = digits[(u >> (unsigned)(i * 4)) & 0xFull];
        }
    }
    dst[at] = '\0';
    return at;
}

static void pt_as_fail_msg(const char *file, int line, const char *base,
                           const char *extra) {
    char buf[256];
    size_t at = 0;
    at = pt_as_append_str(buf, sizeof(buf), at, base ? base : "assert");
    if (extra && extra[0]) {
        at = pt_as_append_str(buf, sizeof(buf), at, ": ");
        at = pt_as_append_str(buf, sizeof(buf), at, extra);
    }
    (void)at;
    polyontest_fail_at(file, line, buf);
}

void polyontest_assert_true(int cond, const char *expr, const char *msg,
                          const char *file, int line) {
    if (cond) {
        return;
    }
    {
        char buf[192];
        size_t at = 0;
        at = pt_as_append_str(buf, sizeof(buf), at, "Expected TRUE was FALSE: ");
        at = pt_as_append_str(buf, sizeof(buf), at, expr ? expr : "?");
        (void)at;
        pt_as_fail_msg(file, line, buf, msg);
    }
}

void polyontest_assert_false(int cond, const char *expr, const char *msg,
                           const char *file, int line) {
    if (!cond) {
        return;
    }
    {
        char buf[192];
        size_t at = 0;
        at = pt_as_append_str(buf, sizeof(buf), at, "Expected FALSE was TRUE: ");
        at = pt_as_append_str(buf, sizeof(buf), at, expr ? expr : "?");
        (void)at;
        pt_as_fail_msg(file, line, buf, msg);
    }
}

void polyontest_assert_null(const void *ptr, const char *msg, const char *file,
                          int line) {
    if (ptr == NULL) {
        return;
    }
    pt_as_fail_msg(file, line, "Expected NULL", msg);
}

void polyontest_assert_not_null(const void *ptr, const char *msg, const char *file,
                              int line) {
    if (ptr != NULL) {
        return;
    }
    pt_as_fail_msg(file, line, "Expected Not-NULL", msg);
}

void polyontest_assert_int(int64_t expected, int64_t actual, int size, int is_hex,
                         const char *msg, const char *file, int line) {
    (void)size;
    if (expected == actual) {
        return;
    }
    {
        char buf[192];
        size_t at = 0;
        at = pt_as_append_str(buf, sizeof(buf), at, "Expected ");
        if (is_hex) {
            at = pt_as_append_hex(buf, sizeof(buf), at, (uint64_t)expected,
                            size <= 0 ? 8 : size * 2);
        } else {
            at = pt_as_append_i64(buf, sizeof(buf), at, expected);
        }
        at = pt_as_append_str(buf, sizeof(buf), at, " Was ");
        if (is_hex) {
            at = pt_as_append_hex(buf, sizeof(buf), at, (uint64_t)actual,
                            size <= 0 ? 8 : size * 2);
        } else {
            at = pt_as_append_i64(buf, sizeof(buf), at, actual);
        }
        (void)at;
        pt_as_fail_msg(file, line, buf, msg);
    }
}

void polyontest_assert_uint(uint64_t expected, uint64_t actual, int size,
                          int is_hex, const char *msg, const char *file,
                          int line) {
    if (expected == actual) {
        return;
    }
    {
        char buf[192];
        size_t at = 0;
        at = pt_as_append_str(buf, sizeof(buf), at, "Expected ");
        if (is_hex) {
            at = pt_as_append_hex(buf, sizeof(buf), at, expected,
                            size <= 0 ? 8 : size * 2);
        } else {
            at = pt_as_append_u64(buf, sizeof(buf), at, expected);
        }
        at = pt_as_append_str(buf, sizeof(buf), at, " Was ");
        if (is_hex) {
            at = pt_as_append_hex(buf, sizeof(buf), at, actual,
                            size <= 0 ? 8 : size * 2);
        } else {
            at = pt_as_append_u64(buf, sizeof(buf), at, actual);
        }
        (void)at;
        pt_as_fail_msg(file, line, buf, msg);
    }
}

void polyontest_assert_not_equal_int(int64_t expected, int64_t actual,
                                   const char *msg, const char *file, int line) {
    if (expected != actual) {
        return;
    }
    {
        char buf[128];
        size_t at = 0;
        at = pt_as_append_str(buf, sizeof(buf), at, "Not Expected ");
        at = pt_as_append_i64(buf, sizeof(buf), at, expected);
        (void)at;
        pt_as_fail_msg(file, line, buf, msg);
    }
}

void polyontest_assert_greater_than(int64_t threshold, int64_t actual,
                                  const char *msg, const char *file, int line) {
    if (actual > threshold) {
        return;
    }
    {
        char buf[160];
        size_t at = 0;
        at = pt_as_append_str(buf, sizeof(buf), at, "Expected ");
        at = pt_as_append_i64(buf, sizeof(buf), at, actual);
        at = pt_as_append_str(buf, sizeof(buf), at, " > ");
        at = pt_as_append_i64(buf, sizeof(buf), at, threshold);
        (void)at;
        pt_as_fail_msg(file, line, buf, msg);
    }
}

void polyontest_assert_less_than(int64_t threshold, int64_t actual,
                               const char *msg, const char *file, int line) {
    if (actual < threshold) {
        return;
    }
    {
        char buf[160];
        size_t at = 0;
        at = pt_as_append_str(buf, sizeof(buf), at, "Expected ");
        at = pt_as_append_i64(buf, sizeof(buf), at, actual);
        at = pt_as_append_str(buf, sizeof(buf), at, " < ");
        at = pt_as_append_i64(buf, sizeof(buf), at, threshold);
        (void)at;
        pt_as_fail_msg(file, line, buf, msg);
    }
}

void polyontest_assert_int_within(int64_t delta, int64_t expected, int64_t actual,
                                const char *msg, const char *file, int line) {
    int64_t diff = (actual > expected) ? (actual - expected) : (expected - actual);
    if (diff <= delta) {
        return;
    }
    {
        char buf[192];
        size_t at = 0;
        at = pt_as_append_str(buf, sizeof(buf), at, "Expected ");
        at = pt_as_append_i64(buf, sizeof(buf), at, actual);
        at = pt_as_append_str(buf, sizeof(buf), at, " within ");
        at = pt_as_append_i64(buf, sizeof(buf), at, delta);
        at = pt_as_append_str(buf, sizeof(buf), at, " of ");
        at = pt_as_append_i64(buf, sizeof(buf), at, expected);
        (void)at;
        pt_as_fail_msg(file, line, buf, msg);
    }
}

#ifndef POLYONTEST_EXCLUDE_FLOAT
void polyontest_assert_float_within(float delta, float expected, float actual,
                                  const char *msg, const char *file, int line) {
    float diff = (actual > expected) ? (actual - expected) : (expected - actual);
    if (diff <= delta) {
        return;
    }
    pt_as_fail_msg(file, line, "Float value not within delta", msg);
}

void polyontest_assert_double_within(double delta, double expected, double actual,
                                   const char *msg, const char *file, int line) {
    double diff =
        (actual > expected) ? (actual - expected) : (expected - actual);
    if (diff <= delta) {
        return;
    }
    pt_as_fail_msg(file, line, "Double value not within delta", msg);
}
#endif

#if POLYONTEST_CFG_HAS_EXTENDED_ASSERTS
void polyontest_assert_string(const char *expected, const char *actual,
                            const char *msg, const char *file, int line) {
    if (expected == actual) {
        return;
    }
    if (expected && actual && strcmp(expected, actual) == 0) {
        return;
    }
    {
        char buf[192];
        size_t at = 0;
        at = pt_as_append_str(buf, sizeof(buf), at, "Expected '");
        at = pt_as_append_str(buf, sizeof(buf), at, expected ? expected : "(null)");
        at = pt_as_append_str(buf, sizeof(buf), at, "' Was '");
        at = pt_as_append_str(buf, sizeof(buf), at, actual ? actual : "(null)");
        at = pt_as_append_str(buf, sizeof(buf), at, "'");
        (void)at;
        pt_as_fail_msg(file, line, buf, msg);
    }
}

void polyontest_assert_string_len(const char *expected, const char *actual,
                                size_t len, const char *msg, const char *file,
                                int line) {
    if (expected == actual) {
        return;
    }
    if (expected && actual && strncmp(expected, actual, len) == 0) {
        return;
    }
    pt_as_fail_msg(file, line, "String length compare failed", msg);
}

void polyontest_assert_memory(const void *expected, const void *actual, size_t len,
                            const char *msg, const char *file, int line) {
    if (len == 0) {
        return;
    }
    if (expected == actual) {
        return;
    }
    if (expected && actual && memcmp(expected, actual, len) == 0) {
        return;
    }
    pt_as_fail_msg(file, line, "Memory compare failed", msg);
}

void polyontest_assert_int_array(const int *expected, const int *actual,
                               size_t num, const char *msg, const char *file,
                               int line) {
    size_t i;
    if (!expected || !actual) {
        pt_as_fail_msg(file, line, "INT array NULL pointer", msg);
        return;
    }
    for (i = 0; i < num; ++i) {
        if (expected[i] != actual[i]) {
            char buf[160];
            size_t at = 0;
            at = pt_as_append_str(buf, sizeof(buf), at, "INT array element ");
            at = pt_as_append_u64(buf, sizeof(buf), at, (uint64_t)i);
            at = pt_as_append_str(buf, sizeof(buf), at, " Expected ");
            at = pt_as_append_i64(buf, sizeof(buf), at, expected[i]);
            at = pt_as_append_str(buf, sizeof(buf), at, " Was ");
            at = pt_as_append_i64(buf, sizeof(buf), at, actual[i]);
            (void)at;
            pt_as_fail_msg(file, line, buf, msg);
            return;
        }
    }
}

void polyontest_assert_uint8_array(const uint8_t *expected, const uint8_t *actual,
                                 size_t num, const char *msg, const char *file,
                                 int line) {
    size_t i;
    if (!expected || !actual) {
        pt_as_fail_msg(file, line, "UINT8 array NULL pointer", msg);
        return;
    }
    for (i = 0; i < num; ++i) {
        if (expected[i] != actual[i]) {
            char buf[160];
            size_t at = 0;
            at = pt_as_append_str(buf, sizeof(buf), at, "UINT8 array element ");
            at = pt_as_append_u64(buf, sizeof(buf), at, (uint64_t)i);
            at = pt_as_append_str(buf, sizeof(buf), at, " Expected ");
            at = pt_as_append_hex(buf, sizeof(buf), at, expected[i], 2);
            at = pt_as_append_str(buf, sizeof(buf), at, " Was ");
            at = pt_as_append_hex(buf, sizeof(buf), at, actual[i], 2);
            (void)at;
            pt_as_fail_msg(file, line, buf, msg);
            return;
        }
    }
}

void polyontest_assert_bits(uint32_t mask, uint32_t expected, uint32_t actual,
                          const char *msg, const char *file, int line) {
    if ((mask & expected) == (mask & actual)) {
        return;
    }
    {
        char buf[160];
        size_t at = 0;
        at = pt_as_append_str(buf, sizeof(buf), at, "Bits Expected ");
        at = pt_as_append_hex(buf, sizeof(buf), at, mask & expected, 8);
        at = pt_as_append_str(buf, sizeof(buf), at, " Was ");
        at = pt_as_append_hex(buf, sizeof(buf), at, mask & actual, 8);
        (void)at;
        pt_as_fail_msg(file, line, buf, msg);
    }
}

void polyontest_assert_bits_high(uint32_t mask, uint32_t actual, const char *msg,
                               const char *file, int line) {
    if ((mask & actual) == mask) {
        return;
    }
    pt_as_fail_msg(file, line, "Expected bits high", msg);
}

void polyontest_assert_bits_low(uint32_t mask, uint32_t actual, const char *msg,
                              const char *file, int line) {
    if ((mask & actual) == 0u) {
        return;
    }
    pt_as_fail_msg(file, line, "Expected bits low", msg);
}
#endif /* POLYONTEST_CFG_HAS_EXTENDED_ASSERTS */
