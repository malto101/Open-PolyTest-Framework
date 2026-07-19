/**
 * PolyOnTest Core — runner, emit, protect/ignore, registration
 * Copyright 2026 Dhruv Menon
 * SPDX-License-Identifier: Apache-2.0
 *
 * Define POLYONTEST_FREESTANDING for no-stdlib builds (must call polyontest_set_writer).
 */
#include "polyontest/polyontest.h"

#if defined(POLYONTEST_FREESTANDING)
void *memcpy(void *dst, const void *src, size_t n);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
#elif !defined(POLYONTEST_NO_LONGJMP)
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#else
#include <stdio.h>
#include <string.h>
#endif

#if POLYONTEST_CFG_HAS_HEAP || !defined(POLYONTEST_FREESTANDING)
#include <stdlib.h>
#endif

enum {
    PT_MSG_SUITE_START = 1,
    PT_MSG_CASE_START = 2,
    PT_MSG_ASSERT_FAIL = 3,
    PT_MSG_CASE_END = 4,
    PT_MSG_SUITE_END = 5,
    PT_MSG_LOG = 6,
    PT_MSG_DONE = 7,
    PT_STATUS_PASSED = 0,
    PT_STATUS_FAILED = 1,
    PT_STATUS_SKIPPED = 2
};

static polyontest_case_t *g_tests;
#if POLYONTEST_CFG_HAS_FIXTURES
static polyontest_suite_t *g_suites;
static polyontest_group_t *g_groups;
#endif
static polyontest_write_fn_t g_write;
static void *g_write_user;
static int g_current_failed;
static int g_current_skipped;
static unsigned g_passed;
static unsigned g_failed;
static unsigned g_skipped;
static const char *g_cur_suite;
static const char *g_cur_group;
static const char *g_cur_name;

static const void *g_param;
static int g_param_active;
static size_t g_param_index;
static const char *g_filter_group;
static const char *g_filter_case;

#if POLYONTEST_CFG_HAS_MUTEX
static polyontest_lock_fn_t g_lock;
static polyontest_lock_fn_t g_unlock;
static void *g_lock_user;
#endif

#if POLYONTEST_CFG_HAS_PROTECT
static int g_protect_active;
#ifdef POLYONTEST_FREESTANDING
static void *g_jmp_buf[32];
#else
static jmp_buf g_jmp_buf;
#endif
#endif

static void default_write(const void *data, size_t len, void *user) {
    (void)user;
#ifdef POLYONTEST_FREESTANDING
    (void)data;
    (void)len;
#else
    fwrite(data, 1, len, stdout);
    fflush(stdout);
#endif
}

void polyontest_set_writer(polyontest_write_fn_t fn, void *user) {
    g_write = fn;
    g_write_user = user;
}

#if POLYONTEST_CFG_HAS_MUTEX
void polyontest_set_locks(polyontest_lock_fn_t lock, polyontest_lock_fn_t unlock,
                        void *user) {
    g_lock = lock;
    g_unlock = unlock;
    g_lock_user = user;
}

static void pt_lock(void) {
    if (g_lock) {
        g_lock(g_lock_user);
    }
}

static void pt_unlock(void) {
    if (g_unlock) {
        g_unlock(g_lock_user);
    }
}
#else
static void pt_lock(void) {}
static void pt_unlock(void) {}
#endif

static int already_linked_case(polyontest_case_t *test_case) {
    for (polyontest_case_t *t = g_tests; t; t = t->next) {
        if (t == test_case) {
            return 1;
        }
    }
    return 0;
}

#if POLYONTEST_CFG_HAS_FIXTURES
static int already_linked_suite(polyontest_suite_t *suite) {
    for (polyontest_suite_t *s = g_suites; s; s = s->next) {
        if (s == suite) {
            return 1;
        }
    }
    return 0;
}

static int already_linked_group(polyontest_group_t *group) {
    for (polyontest_group_t *g = g_groups; g; g = g->next) {
        if (g == group) {
            return 1;
        }
    }
    return 0;
}

void polyontest_register_suite(polyontest_suite_t *suite) {
    if (!suite || already_linked_suite(suite)) {
        return;
    }
    suite->next = g_suites;
    g_suites = suite;
}

void polyontest_register_group(polyontest_group_t *group) {
    if (!group || already_linked_group(group)) {
        return;
    }
    group->next = g_groups;
    g_groups = group;
}
#else
void polyontest_register_suite(polyontest_suite_t *suite) { (void)suite; }
void polyontest_register_group(polyontest_group_t *group) { (void)group; }
#endif

void polyontest_register(polyontest_case_t *test_case) {
    if (!test_case || already_linked_case(test_case)) {
        return;
    }
    test_case->next = g_tests;
    g_tests = test_case;
}

#if POLYONTEST_CFG_HAS_HEAP
int polyontest_register_heap_case(const char *suite, const char *group,
                                const char *name, polyontest_fn_t fn) {
    polyontest_case_t *c = (polyontest_case_t *)malloc(sizeof(polyontest_case_t));
    if (!c) {
        return -1;
    }
    c->suite = suite;
    c->group = group;
    c->name = name;
    c->fn = fn;
    c->tags = NULL;
    c->next = NULL;
    polyontest_register(c);
    return 0;
}
#endif

#ifdef POLYONTEST_USE_SECTION_REGISTRY
#if (defined(__GNUC__) || defined(__clang__)) && !defined(__APPLE__)
/* GNU ld: PROVIDE(__start_polyontest_info) / __stop_polyontest_info around
 * KEEP(*(.polyontest_info)) — see docs/profiles.md. */
extern polyontest_case_t __start_polyontest_info[];
extern polyontest_case_t __stop_polyontest_info[];

static void polyontest_collect_section_cases(void) {
    polyontest_case_t *p;
    for (p = __start_polyontest_info; p < __stop_polyontest_info; ++p) {
        polyontest_register(p);
    }
}
#else
static void polyontest_collect_section_cases(void) {
    /* Apple / non-GNU: use ctor registration or a custom section walker. */
}
#endif
#else
static void polyontest_collect_section_cases(void) {}
#endif

static void emit_raw(const void *data, size_t len) {
    polyontest_write_fn_t w = g_write ? g_write : default_write;
    pt_lock();
    w(data, len, g_write_user);
    pt_unlock();
}

static size_t append_str(char *dst, size_t cap, size_t at, const char *s) {
    if (!s || at >= cap) {
        return at;
    }
    while (*s && at + 1 < cap) {
        dst[at++] = *s++;
    }
    dst[at] = '\0';
    return at;
}

static size_t append_i32(char *dst, size_t cap, size_t at, long v) {
    char tmp[24];
    int i = 0;
    unsigned long u;
    if (v < 0) {
        if (at + 1 < cap) {
            dst[at++] = '-';
            dst[at] = '\0';
        }
        u = (unsigned long)(-(v + 1)) + 1ul;
    } else {
        u = (unsigned long)v;
    }
    if (u == 0) {
        tmp[i++] = '0';
    } else {
        while (u > 0 && i < (int)sizeof(tmp)) {
            tmp[i++] = (char)('0' + (u % 10ul));
            u /= 10ul;
        }
    }
    while (i > 0 && at + 1 < cap) {
        dst[at++] = tmp[--i];
    }
    dst[at] = '\0';
    return at;
}

__attribute__((unused)) static size_t append_u32(char *dst, size_t cap, size_t at,
                                                 unsigned v) {
    return append_i32(dst, cap, at, (long)v);
}

static void format_case_leaf(char *dst, size_t cap, const char *group,
                             const char *name) {
    size_t at = 0;
    at = append_str(dst, cap, at, group ? group : "?");
    at = append_str(dst, cap, at, ".");
    at = append_str(dst, cap, at, name ? name : "?");
    (void)at;
}

#if !POLYONTEST_CFG_HAS_COBS || defined(POLYONTEST_MINIMAL_PRINT)

static size_t append_case_id(char *dst, size_t cap, size_t at) {
    at = append_str(dst, cap, at, g_cur_suite ? g_cur_suite : "?");
    at = append_str(dst, cap, at, ".");
    at = append_str(dst, cap, at, g_cur_group ? g_cur_group : "?");
    at = append_str(dst, cap, at, ".");
    at = append_str(dst, cap, at, g_cur_name ? g_cur_name : "?");
    return at;
}

static void emit_line(const char *line) {
    emit_raw(line, strlen(line));
    emit_raw("\n", 1);
}

static void emit_case_result(const char *status) {
    char buf[256];
    size_t at = 0;
    at = append_str(buf, sizeof(buf), at, status);
    at = append_str(buf, sizeof(buf), at, " ");
    at = append_case_id(buf, sizeof(buf), at);
    (void)at;
    emit_line(buf);
}

#define POLYONTEST_EMIT_TEXT 1

#else /* COBS PTWP */

static size_t cobs_encode(const uint8_t *input, size_t length, uint8_t *out,
                          size_t out_max) {
    size_t read_index = 0;
    size_t write_index = 1;
    size_t code_index = 0;
    uint8_t code = 1;

    if (out_max < 2) {
        return 0;
    }
    out[0] = 0;

    while (read_index < length) {
        if (input[read_index] == 0) {
            out[code_index] = code;
            code_index = write_index++;
            code = 1;
            read_index++;
            if (write_index >= out_max) {
                return 0;
            }
        } else {
            if (write_index >= out_max) {
                return 0;
            }
            out[write_index++] = input[read_index++];
            code++;
            if (code == 0xFF) {
                out[code_index] = code;
                code_index = write_index++;
                code = 1;
                if (write_index >= out_max) {
                    return 0;
                }
            }
        }
    }
    out[code_index] = code;
    if (write_index >= out_max) {
        return 0;
    }
    out[write_index++] = 0;
    return write_index;
}

static void emit_frame(const uint8_t *payload, size_t len) {
    uint8_t encoded[512];
    size_t n = cobs_encode(payload, len, encoded, sizeof(encoded));
    if (n > 0) {
        emit_raw(encoded, n);
    }
}

static size_t put_str(uint8_t *buf, size_t max, size_t at, const char *s) {
    size_t n = s ? strlen(s) : 0;
    if (n > 255) {
        n = 255;
    }
    if (at + 1 + n > max) {
        return at;
    }
    buf[at++] = (uint8_t)n;
    if (n && s) {
        memcpy(buf + at, s, n);
        at += n;
    }
    return at;
}

static void emit_msg(uint8_t type, const char *a, const char *b, const char *c,
                     uint32_t u0, uint32_t u1, uint32_t u2) {
    uint8_t buf[384];
    size_t at = 0;
    buf[at++] = 'P';
    buf[at++] = 'T';
    buf[at++] = 1;
    buf[at++] = type;
    at = put_str(buf, sizeof(buf), at, a);
    at = put_str(buf, sizeof(buf), at, b);
    at = put_str(buf, sizeof(buf), at, c);
    if (at + 12 > sizeof(buf)) {
        return;
    }
    buf[at++] = (uint8_t)(u0);
    buf[at++] = (uint8_t)(u0 >> 8);
    buf[at++] = (uint8_t)(u0 >> 16);
    buf[at++] = (uint8_t)(u0 >> 24);
    buf[at++] = (uint8_t)(u1);
    buf[at++] = (uint8_t)(u1 >> 8);
    buf[at++] = (uint8_t)(u1 >> 16);
    buf[at++] = (uint8_t)(u1 >> 24);
    buf[at++] = (uint8_t)(u2);
    buf[at++] = (uint8_t)(u2 >> 8);
    buf[at++] = (uint8_t)(u2 >> 16);
    buf[at++] = (uint8_t)(u2 >> 24);
    emit_frame(buf, at);
}

#define POLYONTEST_EMIT_TEXT 0

#endif /* COBS */

void polyontest_set_param(size_t index, const void *param) {
    g_param_index = index;
    g_param = param;
    g_param_active = 1;
}

void polyontest_clear_param(void) {
    g_param = NULL;
    g_param_active = 0;
    g_param_index = 0;
}

size_t polyontest_param_index(void) { return g_param_index; }

const void *polyontest_current_param(void) { return g_param; }

static const char *message_with_param(const char *message, char *buf,
                                      size_t cap) {
    const char *base = message ? message : "fail";
    if (!g_param_active) {
        return base;
    }
    size_t at = 0;
    at = append_str(buf, cap, at, base);
    at = append_str(buf, cap, at, " [param=");
    at = append_u32(buf, cap, at, (unsigned)g_param_index);
    at = append_str(buf, cap, at, "]");
    (void)at;
    return buf;
}

void polyontest_fail_at(const char *file, int line, const char *message) {
    char param_msg[192];
    const char *msg = message_with_param(message, param_msg, sizeof(param_msg));
    pt_lock();
    g_current_failed = 1;
    pt_unlock();
#if POLYONTEST_EMIT_TEXT
    char buf[320];
    size_t at = 0;
    at = append_str(buf, sizeof(buf), at, "FAIL ");
    at = append_case_id(buf, sizeof(buf), at);
    at = append_str(buf, sizeof(buf), at, " ");
    at = append_str(buf, sizeof(buf), at, file ? file : "?");
    at = append_str(buf, sizeof(buf), at, ":");
    at = append_i32(buf, sizeof(buf), at, line);
    at = append_str(buf, sizeof(buf), at, " ");
    at = append_str(buf, sizeof(buf), at, msg);
    (void)at;
    emit_line(buf);
#else
    char leaf[128];
    format_case_leaf(leaf, sizeof(leaf), g_cur_group, g_cur_name);
    emit_msg(PT_MSG_ASSERT_FAIL, g_cur_suite, leaf, msg, (uint32_t)line, 0, 0);
    (void)file;
#endif
}

void polyontest_fail(const char *message, const char *file, int line) {
    polyontest_fail_at(file, line, message ? message : "fail");
}

void polyontest_ignore(const char *message) {
    pt_lock();
    g_current_skipped = 1;
    pt_unlock();
    if (message && message[0]) {
#if POLYONTEST_EMIT_TEXT
        char buf[256];
        size_t at = 0;
        at = append_str(buf, sizeof(buf), at, "IGNORE ");
        at = append_case_id(buf, sizeof(buf), at);
        at = append_str(buf, sizeof(buf), at, " ");
        at = append_str(buf, sizeof(buf), at, message);
        (void)at;
        emit_line(buf);
#else
        (void)message;
#endif
    }
}

int polyontest_protect(void) {
#if !POLYONTEST_CFG_HAS_PROTECT
    return 1;
#elif defined(POLYONTEST_FREESTANDING)
    g_protect_active = 1;
    if (__builtin_setjmp(g_jmp_buf) == 0) {
        return 1;
    }
    g_protect_active = 0;
    return 0;
#else
    g_protect_active = 1;
    if (setjmp(g_jmp_buf) == 0) {
        return 1;
    }
    g_protect_active = 0;
    return 0;
#endif
}

void polyontest_abort(void) {
    pt_lock();
    g_current_failed = 1;
    pt_unlock();
#if !POLYONTEST_CFG_HAS_PROTECT
    return;
#elif defined(POLYONTEST_FREESTANDING)
    if (g_protect_active) {
        __builtin_longjmp(g_jmp_buf, 1);
    }
#else
    if (g_protect_active) {
        longjmp(g_jmp_buf, 1);
    }
#endif
}

static int str_cmp(const char *a, const char *b) {
    if (!a && !b) {
        return 0;
    }
    if (!a) {
        return -1;
    }
    if (!b) {
        return 1;
    }
    return strcmp(a, b);
}

static int case_cmp(const polyontest_case_t *a, const polyontest_case_t *b) {
    int c = str_cmp(a->suite, b->suite);
    if (c != 0) {
        return c;
    }
    c = str_cmp(a->group, b->group);
    if (c != 0) {
        return c;
    }
    return str_cmp(a->name, b->name);
}

static polyontest_case_t *sort_cases(polyontest_case_t *head) {
    polyontest_case_t *sorted = NULL;
    while (head) {
        polyontest_case_t *n = head->next;
        if (!sorted || case_cmp(head, sorted) < 0) {
            head->next = sorted;
            sorted = head;
        } else {
            polyontest_case_t *s = sorted;
            while (s->next && case_cmp(head, s->next) >= 0) {
                s = s->next;
            }
            head->next = s->next;
            s->next = head;
        }
        head = n;
    }
    return sorted;
}

#if POLYONTEST_CFG_HAS_FIXTURES
static polyontest_suite_t *find_suite(const char *name) {
    for (polyontest_suite_t *s = g_suites; s; s = s->next) {
        if (str_cmp(s->name, name) == 0) {
            return s;
        }
    }
    return NULL;
}

static polyontest_group_t *find_group(const char *suite, const char *group) {
    for (polyontest_group_t *g = g_groups; g; g = g->next) {
        if (str_cmp(g->suite, suite) == 0 && str_cmp(g->name, group) == 0) {
            return g;
        }
    }
    return NULL;
}
#endif

#if POLYONTEST_CFG_HAS_TAGS
static int tags_contain(const char *const *tags, const char *tag) {
    if (!tags || !tag) {
        return 0;
    }
    for (; *tags; ++tags) {
        if (strcmp(*tags, tag) == 0) {
            return 1;
        }
    }
    return 0;
}

static int case_matches_tag(const polyontest_case_t *t, const char *tag) {
    if (tags_contain(t->tags, tag)) {
        return 1;
    }
#if POLYONTEST_CFG_HAS_FIXTURES
    {
        polyontest_suite_t *s = find_suite(t->suite);
        if (s && tags_contain(s->tags, tag)) {
            return 1;
        }
    }
    {
        polyontest_group_t *g = find_group(t->suite, t->group);
        if (g && tags_contain(g->tags, tag)) {
            return 1;
        }
    }
#endif
    return 0;
}
#endif

static void close_suite(const char *open_suite, unsigned suite_passed,
                        unsigned suite_failed, unsigned suite_skipped) {
    if (!open_suite) {
        return;
    }
#if POLYONTEST_CFG_HAS_FIXTURES
    {
        polyontest_suite_t *s = find_suite(open_suite);
        if (s && s->teardown) {
            s->teardown();
        }
    }
#endif
#if POLYONTEST_EMIT_TEXT
    {
        char buf[160];
        size_t at = 0;
        at = append_str(buf, sizeof(buf), at, "SUITE_END ");
        at = append_str(buf, sizeof(buf), at, open_suite);
        at = append_str(buf, sizeof(buf), at, " passed=");
        at = append_u32(buf, sizeof(buf), at, suite_passed);
        at = append_str(buf, sizeof(buf), at, " failed=");
        at = append_u32(buf, sizeof(buf), at, suite_failed);
        at = append_str(buf, sizeof(buf), at, " skipped=");
        at = append_u32(buf, sizeof(buf), at, suite_skipped);
        (void)at;
        emit_line(buf);
    }
#else
    emit_msg(PT_MSG_SUITE_END, open_suite, NULL, NULL, suite_passed, suite_failed,
             suite_skipped);
#endif
}

static void open_suite_emit(const char *name) {
#if POLYONTEST_CFG_HAS_FIXTURES
    polyontest_suite_t *s = find_suite(name);
    if (s && s->setup) {
        s->setup();
    }
#endif
#if POLYONTEST_EMIT_TEXT
    {
        char buf[128];
        size_t at = 0;
        at = append_str(buf, sizeof(buf), at, "SUITE_START ");
        at = append_str(buf, sizeof(buf), at, name);
        (void)at;
        emit_line(buf);
    }
#else
    emit_msg(PT_MSG_SUITE_START, name, NULL, NULL, 0, 0, 0);
#endif
}

static int run_filtered(int (*match)(const polyontest_case_t *, const char *),
                        const char *tag) {
    polyontest_collect_section_cases();
    polyontest_case_t *list = sort_cases(g_tests);
    g_tests = list;
    g_passed = g_failed = g_skipped = 0;

    const char *open_suite = NULL;
    unsigned suite_passed = 0, suite_failed = 0, suite_skipped = 0;

#if POLYONTEST_EMIT_TEXT
    emit_line("=== PolyOnTest ===");
#endif

    for (polyontest_case_t *t = list; t; t = t->next) {
        if (match && !match(t, tag)) {
            continue;
        }

        if (str_cmp(open_suite, t->suite) != 0) {
            close_suite(open_suite, suite_passed, suite_failed, suite_skipped);
            open_suite = t->suite;
            suite_passed = suite_failed = suite_skipped = 0;
            open_suite_emit(open_suite);
        }

        g_cur_suite = t->suite;
        g_cur_group = t->group;
        g_cur_name = t->name;
        g_current_failed = 0;
        g_current_skipped = 0;

        {
            char leaf[128];
            format_case_leaf(leaf, sizeof(leaf), t->group, t->name);
#if POLYONTEST_EMIT_TEXT
            {
                char buf[192];
                size_t at = 0;
                at = append_str(buf, sizeof(buf), at, "CASE_START ");
                at = append_case_id(buf, sizeof(buf), at);
                (void)at;
                emit_line(buf);
            }
#else
            emit_msg(PT_MSG_CASE_START, t->suite, leaf, NULL, 0, 0, 0);
#endif
            (void)leaf;
        }

        {
#if POLYONTEST_CFG_HAS_FIXTURES
            polyontest_group_t *g = find_group(t->suite, t->group);
            if (g && g->setup) {
                g->setup();
            }
            if (t->fn) {
                t->fn();
            }
            if (g && g->teardown) {
                g->teardown();
            }
#else
            if (t->fn) {
                t->fn();
            }
#endif
        }

        if (g_current_skipped) {
            g_skipped++;
            suite_skipped++;
#if POLYONTEST_EMIT_TEXT
            emit_case_result("SKIP");
#else
            {
                char leaf[128];
                format_case_leaf(leaf, sizeof(leaf), t->group, t->name);
                emit_msg(PT_MSG_CASE_END, t->suite, leaf, NULL, PT_STATUS_SKIPPED,
                         0, 0);
            }
#endif
        } else if (g_current_failed) {
            g_failed++;
            suite_failed++;
#if POLYONTEST_EMIT_TEXT
            emit_case_result("FAIL");
#else
            {
                char leaf[128];
                format_case_leaf(leaf, sizeof(leaf), t->group, t->name);
                emit_msg(PT_MSG_CASE_END, t->suite, leaf, NULL, PT_STATUS_FAILED,
                         0, 0);
            }
#endif
        } else {
            g_passed++;
            suite_passed++;
#if POLYONTEST_EMIT_TEXT
            emit_case_result("PASS");
#else
            {
                char leaf[128];
                format_case_leaf(leaf, sizeof(leaf), t->group, t->name);
                emit_msg(PT_MSG_CASE_END, t->suite, leaf, NULL, PT_STATUS_PASSED,
                         0, 0);
            }
#endif
        }
    }

    close_suite(open_suite, suite_passed, suite_failed, suite_skipped);

#if POLYONTEST_EMIT_TEXT
    {
        char buf[160];
        size_t at = 0;
        at = append_str(buf, sizeof(buf), at, "DONE passed=");
        at = append_u32(buf, sizeof(buf), at, g_passed);
        at = append_str(buf, sizeof(buf), at, " failed=");
        at = append_u32(buf, sizeof(buf), at, g_failed);
        at = append_str(buf, sizeof(buf), at, " skipped=");
        at = append_u32(buf, sizeof(buf), at, g_skipped);
        (void)at;
        emit_line(buf);
    }
#else
    emit_msg(PT_MSG_DONE, NULL, NULL, NULL, g_passed, g_failed, g_skipped);
#endif

    return g_failed ? 1 : 0;
}

static int match_all(const polyontest_case_t *t, const char *tag) {
    (void)t;
    (void)tag;
    return 1;
}

static int match_suite(const polyontest_case_t *t, const char *suite) {
    return suite && str_cmp(t->suite, suite) == 0;
}

static int match_group(const polyontest_case_t *t, const char *suite) {
    return suite && g_filter_group && str_cmp(t->suite, suite) == 0 &&
           str_cmp(t->group, g_filter_group) == 0;
}

static int match_case(const polyontest_case_t *t, const char *suite) {
    return suite && g_filter_group && g_filter_case &&
           str_cmp(t->suite, suite) == 0 &&
           str_cmp(t->group, g_filter_group) == 0 &&
           str_cmp(t->name, g_filter_case) == 0;
}

#if POLYONTEST_CFG_HAS_TAGS
static int match_tag(const polyontest_case_t *t, const char *tag) {
    return case_matches_tag(t, tag);
}
#endif

int polyontest_run_all(void) { return run_filtered(match_all, NULL); }

int polyontest_run_tag(const char *tag) {
#if POLYONTEST_CFG_HAS_TAGS
    if (!tag) {
        return polyontest_run_all();
    }
    return run_filtered(match_tag, tag);
#else
    (void)tag;
    return polyontest_run_all();
#endif
}

int polyontest_run_suite(const char *suite) {
    if (!suite || !suite[0]) {
        return polyontest_run_all();
    }
    return run_filtered(match_suite, suite);
}

int polyontest_run_group(const char *suite, const char *group) {
    if (!suite || !suite[0] || !group || !group[0]) {
        return polyontest_run_all();
    }
    g_filter_group = group;
    return run_filtered(match_group, suite);
}

int polyontest_run_case(const char *suite, const char *group, const char *case_name) {
    if (!suite || !suite[0] || !group || !group[0] || !case_name || !case_name[0]) {
        return polyontest_run_all();
    }
    g_filter_group = group;
    g_filter_case = case_name;
    return run_filtered(match_case, suite);
}

int polyontest_run_from_env(void) {
#if defined(POLYONTEST_FREESTANDING)
    return polyontest_run_all();
#else
    if (getenv("POLY_DISCOVER")) {
        polyontest_collect_section_cases();
        polyontest_case_t *list = sort_cases(g_tests);
        const char *tag = getenv("POLYONTEST_TAG");
        const char *suite = getenv("POLY_SUITE");
        if (!suite || !suite[0]) {
            suite = getenv("POLYONTEST_SUITE");
        }
        const char *group = getenv("POLY_GROUP");
        if (!group || !group[0]) {
            group = getenv("POLYONTEST_GROUP");
        }
        for (polyontest_case_t *t = list; t; t = t->next) {
#if POLYONTEST_CFG_HAS_TAGS
            if (tag && tag[0] && !case_matches_tag(t, tag)) {
                continue;
            }
#endif
            if (suite && suite[0] && str_cmp(t->suite, suite) != 0) {
                continue;
            }
            if (group && group[0] && str_cmp(t->group, group) != 0) {
                continue;
            }
            char buf[256];
            size_t at = 0;
            at = append_str(buf, sizeof(buf), at, "list-case:");
            at = append_str(buf, sizeof(buf), at, t->suite ? t->suite : "?");
            at = append_str(buf, sizeof(buf), at, ".");
            at = append_str(buf, sizeof(buf), at, t->group ? t->group : "?");
            at = append_str(buf, sizeof(buf), at, ".");
            at = append_str(buf, sizeof(buf), at, t->name ? t->name : "?");
            (void)at;
            emit_raw(buf, strlen(buf));
            emit_raw("\n", 1);
        }
        exit(0);
    }

    const char *tag = getenv("POLYONTEST_TAG");
    const char *suite = getenv("POLY_SUITE");
    if (!suite || !suite[0]) {
        suite = getenv("POLYONTEST_SUITE");
    }
    const char *group = getenv("POLY_GROUP");
    if (!group || !group[0]) {
        group = getenv("POLYONTEST_GROUP");
    }
    const char *case_name = getenv("POLY_CASE");

    if (suite && suite[0] && group && group[0] && case_name && case_name[0]) {
        return polyontest_run_case(suite, group, case_name);
    }
    if (tag && tag[0]) {
        return polyontest_run_tag(tag);
    }
    if (suite && suite[0] && group && group[0]) {
        return polyontest_run_group(suite, group);
    }
    if (suite && suite[0]) {
        return polyontest_run_suite(suite);
    }
    return polyontest_run_all();
#endif
}
