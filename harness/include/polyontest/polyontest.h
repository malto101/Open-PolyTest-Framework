/**
 * PolyOnTest Core — public C ABI
 * Copyright 2026 Dhruv Menon
 * SPDX-License-Identifier: Apache-2.0
 *
 * Size profiles (pick one; default = full):
 *   POLYONTEST_PROFILE_TINY   — text only; no tags/fixtures/float/longjmp (~1–3 KB)
 *   POLYONTEST_PROFILE_SMALL  — hierarchy + tags + fixtures + COBS; float off by default
 *   POLYONTEST_PROFILE_FULL   — everything (floats, tags, hierarchy, COBS, protect, mutex)
 *
 * Feature knobs (override / refine profiles):
 *   POLYONTEST_MINIMAL_PRINT       — human-readable output (no COBS)
 *   POLYONTEST_USE_HEAP            — enable polyontest_register_heap_case (malloc)
 *   POLYONTEST_USE_SECTION_REGISTRY — place cases in .polyontest_info (see docs/profiles.md)
 *   POLYONTEST_SECTION             — override linker section for descriptors
 *   POLYONTEST_NO_ALIASES          — do not define TEST / ASSERT_* short names
 *   POLYONTEST_EXCLUDE_FLOAT       — omit float/double asserts
 *   POLYONTEST_FREESTANDING        — no-stdlib; use polyontest_set_writer
 *   POLYONTEST_NO_LONGJMP          — PROTECT/ABORT without setjmp (abort = fail flag)
 *
 * Derived (from polyontest_profile.h): POLYONTEST_CFG_HAS_{COBS,TAGS,FIXTURES,FLOAT,
 * PROTECT,MUTEX,EXTENDED_ASSERTS,HEAP}.
 */
#ifndef POLYONTEST_H
#define POLYONTEST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include "polyontest/polyontest_profile.h"

#ifndef POLYONTEST_SECTION
#if defined(__APPLE__) && defined(__MACH__)
#define POLYONTEST_SECTION __attribute__((section("__DATA,polyontest")))
#elif defined(__GNUC__) || defined(__clang__)
#define POLYONTEST_SECTION __attribute__((section(".polyontest_info")))
#else
#define POLYONTEST_SECTION
#endif
#endif

typedef void (*polyontest_fn_t)(void);
typedef void (*polyontest_fixture_fn_t)(void);

/** Suite: fixtures run once around the whole suite. */
typedef struct polyontest_suite {
    const char *name;
    polyontest_fixture_fn_t setup;
    polyontest_fixture_fn_t teardown;
    const char *const *tags; /* NULL-terminated; may be NULL */
    struct polyontest_suite *next;
} polyontest_suite_t;

/** Group: fixtures run before/after each case in the group. */
typedef struct polyontest_group {
    const char *suite;
    const char *name;
    polyontest_fixture_fn_t setup;
    polyontest_fixture_fn_t teardown;
    const char *const *tags;
    struct polyontest_group *next;
} polyontest_group_t;

typedef struct polyontest_case {
    const char *suite;
    const char *group;
    const char *name;
    polyontest_fn_t fn;
    const char *const *tags;
    struct polyontest_case *next;
} polyontest_case_t;

/** Byte sink — board/transport glue provides this (Dependency Inversion). */
typedef void (*polyontest_write_fn_t)(const void *data, size_t len, void *user);

void polyontest_set_writer(polyontest_write_fn_t fn, void *user);

#if POLYONTEST_CFG_HAS_MUTEX
/** Optional lock hooks for multithreaded host (full profile). NULL = no-op. */
typedef void (*polyontest_lock_fn_t)(void *user);
void polyontest_set_locks(polyontest_lock_fn_t lock, polyontest_lock_fn_t unlock,
                        void *user);
#endif

void polyontest_register_suite(polyontest_suite_t *suite);
void polyontest_register_group(polyontest_group_t *group);
void polyontest_register(polyontest_case_t *test_case);

#if POLYONTEST_CFG_HAS_HEAP
/**
 * Heap-allocate a case descriptor and register it.
 * Returns 0 on success, -1 on allocation failure.
 */
int polyontest_register_heap_case(const char *suite, const char *group,
                                const char *name, polyontest_fn_t fn);
#endif

int polyontest_run_all(void);
/** Run cases whose suite, group, or case tags include `tag` (stub on tiny). */
int polyontest_run_tag(const char *tag);
/** Run cases belonging to `suite` (strcmp on case suite name). */
int polyontest_run_suite(const char *suite);
/** Run cases in `suite`/`group`. */
int polyontest_run_group(const char *suite, const char *group);
/**
 * Host helper: honor POLYONTEST_TAG, POLYONTEST_SUITE+POLYONTEST_GROUP, or
 * POLYONTEST_SUITE from the environment; otherwise run all.
 * On freestanding targets, always runs all.
 */
int polyontest_run_from_env(void);

/** Parameterized-test cursor (set by PARAM_TEST / FOR_EACH). */
void polyontest_set_param(size_t index, const void *param);
void polyontest_clear_param(void);
size_t polyontest_param_index(void);
const void *polyontest_current_param(void);

void polyontest_ignore(const char *message);
int polyontest_protect(void);
void polyontest_abort(void);

void polyontest_fail(const char *message, const char *file, int line);
void polyontest_fail_at(const char *file, int line, const char *message);

void polyontest_assert_true(int cond, const char *expr, const char *msg,
                          const char *file, int line);
void polyontest_assert_false(int cond, const char *expr, const char *msg,
                           const char *file, int line);
void polyontest_assert_null(const void *ptr, const char *msg, const char *file,
                          int line);
void polyontest_assert_not_null(const void *ptr, const char *msg, const char *file,
                              int line);

void polyontest_assert_int(int64_t expected, int64_t actual, int size,
                         int is_hex, const char *msg, const char *file,
                         int line);
void polyontest_assert_uint(uint64_t expected, uint64_t actual, int size,
                          int is_hex, const char *msg, const char *file,
                          int line);
void polyontest_assert_not_equal_int(int64_t expected, int64_t actual,
                                   const char *msg, const char *file, int line);
void polyontest_assert_greater_than(int64_t threshold, int64_t actual,
                                  const char *msg, const char *file, int line);
void polyontest_assert_less_than(int64_t threshold, int64_t actual,
                               const char *msg, const char *file, int line);
void polyontest_assert_int_within(int64_t delta, int64_t expected, int64_t actual,
                                const char *msg, const char *file, int line);

#ifndef POLYONTEST_EXCLUDE_FLOAT
void polyontest_assert_float_within(float delta, float expected, float actual,
                                  const char *msg, const char *file, int line);
void polyontest_assert_double_within(double delta, double expected, double actual,
                                   const char *msg, const char *file, int line);
#endif

#if POLYONTEST_CFG_HAS_EXTENDED_ASSERTS
void polyontest_assert_string(const char *expected, const char *actual,
                            const char *msg, const char *file, int line);
void polyontest_assert_string_len(const char *expected, const char *actual,
                                size_t len, const char *msg, const char *file,
                                int line);
void polyontest_assert_memory(const void *expected, const void *actual, size_t len,
                            const char *msg, const char *file, int line);
void polyontest_assert_int_array(const int *expected, const int *actual,
                               size_t num, const char *msg, const char *file,
                               int line);
void polyontest_assert_uint8_array(const uint8_t *expected, const uint8_t *actual,
                                 size_t num, const char *msg, const char *file,
                                 int line);
void polyontest_assert_bits(uint32_t mask, uint32_t expected, uint32_t actual,
                          const char *msg, const char *file, int line);
void polyontest_assert_bits_high(uint32_t mask, uint32_t actual, const char *msg,
                               const char *file, int line);
void polyontest_assert_bits_low(uint32_t mask, uint32_t actual, const char *msg,
                              const char *file, int line);
#endif

/* -------------------------------------------------------------------------- */
/* Assert macros                                                              */
/* -------------------------------------------------------------------------- */

#define POLYONTEST_ASSERT_TRUE(cond)                                             \
    polyontest_assert_true(!!(cond), #cond, NULL, __FILE__, __LINE__)
#define POLYONTEST_ASSERT_TRUE_MESSAGE(cond, msg)                                \
    polyontest_assert_true(!!(cond), #cond, (msg), __FILE__, __LINE__)
#define POLYONTEST_ASSERT_FALSE(cond)                                            \
    polyontest_assert_false(!!(cond), #cond, NULL, __FILE__, __LINE__)
#define POLYONTEST_ASSERT_FALSE_MESSAGE(cond, msg)                               \
    polyontest_assert_false(!!(cond), #cond, (msg), __FILE__, __LINE__)

#define POLYONTEST_ASSERT_NULL(ptr)                                              \
    polyontest_assert_null((const void *)(ptr), NULL, __FILE__, __LINE__)
#define POLYONTEST_ASSERT_NULL_MESSAGE(ptr, msg)                                 \
    polyontest_assert_null((const void *)(ptr), (msg), __FILE__, __LINE__)
#define POLYONTEST_ASSERT_NOT_NULL(ptr)                                          \
    polyontest_assert_not_null((const void *)(ptr), NULL, __FILE__, __LINE__)
#define POLYONTEST_ASSERT_NOT_NULL_MESSAGE(ptr, msg)                             \
    polyontest_assert_not_null((const void *)(ptr), (msg), __FILE__, __LINE__)

#define POLYONTEST_ASSERT_EQUAL_INT(expected, actual)                            \
    polyontest_assert_int((int64_t)(expected), (int64_t)(actual), (int)sizeof(int), \
                        0, NULL, __FILE__, __LINE__)
#define POLYONTEST_ASSERT_EQUAL_INT_MESSAGE(expected, actual, msg)               \
    polyontest_assert_int((int64_t)(expected), (int64_t)(actual), (int)sizeof(int), \
                        0, (msg), __FILE__, __LINE__)
#define POLYONTEST_ASSERT_EQUAL_INT8(expected, actual)                           \
    polyontest_assert_int((int64_t)(int8_t)(expected), (int64_t)(int8_t)(actual), \
                        1, 0, NULL, __FILE__, __LINE__)
#define POLYONTEST_ASSERT_EQUAL_INT16(expected, actual)                          \
    polyontest_assert_int((int64_t)(int16_t)(expected),                          \
                        (int64_t)(int16_t)(actual), 2, 0, NULL, __FILE__,      \
                        __LINE__)
#define POLYONTEST_ASSERT_EQUAL_INT32(expected, actual)                          \
    polyontest_assert_int((int64_t)(int32_t)(expected),                          \
                        (int64_t)(int32_t)(actual), 4, 0, NULL, __FILE__,      \
                        __LINE__)
#define POLYONTEST_ASSERT_EQUAL_INT64(expected, actual)                          \
    polyontest_assert_int((int64_t)(expected), (int64_t)(actual), 8, 0, NULL,    \
                        __FILE__, __LINE__)

#define POLYONTEST_ASSERT_EQUAL_UINT(expected, actual)                           \
    polyontest_assert_uint((uint64_t)(expected), (uint64_t)(actual),             \
                         (int)sizeof(unsigned), 0, NULL, __FILE__, __LINE__)
#define POLYONTEST_ASSERT_EQUAL_UINT8(expected, actual)                          \
    polyontest_assert_uint((uint64_t)(uint8_t)(expected),                        \
                         (uint64_t)(uint8_t)(actual), 1, 0, NULL, __FILE__,    \
                         __LINE__)
#define POLYONTEST_ASSERT_EQUAL_UINT16(expected, actual)                         \
    polyontest_assert_uint((uint64_t)(uint16_t)(expected),                       \
                         (uint64_t)(uint16_t)(actual), 2, 0, NULL, __FILE__,   \
                         __LINE__)
#define POLYONTEST_ASSERT_EQUAL_UINT32(expected, actual)                         \
    polyontest_assert_uint((uint64_t)(uint32_t)(expected),                       \
                         (uint64_t)(uint32_t)(actual), 4, 0, NULL, __FILE__,   \
                         __LINE__)
#define POLYONTEST_ASSERT_EQUAL_UINT64(expected, actual)                         \
    polyontest_assert_uint((uint64_t)(expected), (uint64_t)(actual), 8, 0, NULL, \
                         __FILE__, __LINE__)

#define POLYONTEST_ASSERT_EQUAL_HEX8(expected, actual)                           \
    polyontest_assert_uint((uint64_t)(uint8_t)(expected),                        \
                         (uint64_t)(uint8_t)(actual), 1, 1, NULL, __FILE__,    \
                         __LINE__)
#define POLYONTEST_ASSERT_EQUAL_HEX16(expected, actual)                          \
    polyontest_assert_uint((uint64_t)(uint16_t)(expected),                       \
                         (uint64_t)(uint16_t)(actual), 2, 1, NULL, __FILE__,   \
                         __LINE__)
#define POLYONTEST_ASSERT_EQUAL_HEX32(expected, actual)                          \
    polyontest_assert_uint((uint64_t)(uint32_t)(expected),                       \
                         (uint64_t)(uint32_t)(actual), 4, 1, NULL, __FILE__,   \
                         __LINE__)
#define POLYONTEST_ASSERT_EQUAL_HEX64(expected, actual)                          \
    polyontest_assert_uint((uint64_t)(expected), (uint64_t)(actual), 8, 1, NULL, \
                         __FILE__, __LINE__)

#define POLYONTEST_ASSERT_NOT_EQUAL_INT(expected, actual)                        \
    polyontest_assert_not_equal_int((int64_t)(expected), (int64_t)(actual), NULL,\
                                  __FILE__, __LINE__)
#define POLYONTEST_ASSERT_GREATER_THAN(threshold, actual)                        \
    polyontest_assert_greater_than((int64_t)(threshold), (int64_t)(actual), NULL,\
                                 __FILE__, __LINE__)
#define POLYONTEST_ASSERT_LESS_THAN(threshold, actual)                           \
    polyontest_assert_less_than((int64_t)(threshold), (int64_t)(actual), NULL,   \
                              __FILE__, __LINE__)
#define POLYONTEST_ASSERT_INT_WITHIN(delta, expected, actual)                    \
    polyontest_assert_int_within((int64_t)(delta), (int64_t)(expected),          \
                               (int64_t)(actual), NULL, __FILE__, __LINE__)
#define POLYONTEST_ASSERT_INT_WITHIN_MESSAGE(delta, expected, actual, msg)       \
    polyontest_assert_int_within((int64_t)(delta), (int64_t)(expected),          \
                               (int64_t)(actual), (msg), __FILE__, __LINE__)

#ifndef POLYONTEST_EXCLUDE_FLOAT
#define POLYONTEST_ASSERT_FLOAT_WITHIN(delta, expected, actual)                  \
    polyontest_assert_float_within((float)(delta), (float)(expected),            \
                                 (float)(actual), NULL, __FILE__, __LINE__)
#define POLYONTEST_ASSERT_EQUAL_FLOAT(expected, actual)                          \
    POLYONTEST_ASSERT_FLOAT_WITHIN(0.00001f, (expected), (actual))
#define POLYONTEST_ASSERT_DOUBLE_WITHIN(delta, expected, actual)                 \
    polyontest_assert_double_within((double)(delta), (double)(expected),         \
                                  (double)(actual), NULL, __FILE__, __LINE__)
#define POLYONTEST_ASSERT_EQUAL_DOUBLE(expected, actual)                         \
    POLYONTEST_ASSERT_DOUBLE_WITHIN(1.0e-12, (expected), (actual))
#endif

#if POLYONTEST_CFG_HAS_EXTENDED_ASSERTS
#define POLYONTEST_ASSERT_EQUAL_STRING(expected, actual)                         \
    polyontest_assert_string((expected), (actual), NULL, __FILE__, __LINE__)
#define POLYONTEST_ASSERT_EQUAL_STRING_MESSAGE(expected, actual, msg)            \
    polyontest_assert_string((expected), (actual), (msg), __FILE__, __LINE__)
#define POLYONTEST_ASSERT_EQUAL_STRING_LEN(expected, actual, len)                \
    polyontest_assert_string_len((expected), (actual), (size_t)(len), NULL,      \
                               __FILE__, __LINE__)
#define POLYONTEST_ASSERT_EQUAL_MEMORY(expected, actual, len)                    \
    polyontest_assert_memory((expected), (actual), (size_t)(len), NULL,          \
                           __FILE__, __LINE__)
#define POLYONTEST_ASSERT_EQUAL_INT_ARRAY(expected, actual, num)                 \
    polyontest_assert_int_array((expected), (actual), (size_t)(num), NULL,       \
                              __FILE__, __LINE__)
#define POLYONTEST_ASSERT_EQUAL_UINT8_ARRAY(expected, actual, num)               \
    polyontest_assert_uint8_array((expected), (actual), (size_t)(num), NULL,     \
                                __FILE__, __LINE__)

#define POLYONTEST_ASSERT_BITS(mask, expected, actual)                           \
    polyontest_assert_bits((uint32_t)(mask), (uint32_t)(expected),               \
                         (uint32_t)(actual), NULL, __FILE__, __LINE__)
#define POLYONTEST_ASSERT_BITS_HIGH(mask, actual)                                \
    polyontest_assert_bits_high((uint32_t)(mask), (uint32_t)(actual), NULL,      \
                              __FILE__, __LINE__)
#define POLYONTEST_ASSERT_BITS_LOW(mask, actual)                                 \
    polyontest_assert_bits_low((uint32_t)(mask), (uint32_t)(actual), NULL,       \
                             __FILE__, __LINE__)
#endif

#define POLYONTEST_FAIL(msg) polyontest_fail((msg), __FILE__, __LINE__)
#define POLYONTEST_FAIL_MESSAGE(msg) POLYONTEST_FAIL(msg)

#define POLYONTEST_IGNORE()                                                      \
    do {                                                                       \
        polyontest_ignore(NULL);                                                 \
        return;                                                                \
    } while (0)
#define POLYONTEST_IGNORE_MESSAGE(msg)                                           \
    do {                                                                       \
        polyontest_ignore(msg);                                                  \
        return;                                                                \
    } while (0)

#define POLYONTEST_PROTECT() (polyontest_protect())
#define POLYONTEST_ABORT() polyontest_abort()

/* -------------------------------------------------------------------------- */
/* Suite / group / case registration                                          */
/* -------------------------------------------------------------------------- */

#if POLYONTEST_CFG_HAS_FIXTURES

/** Define the shared suite object (use once per suite; SETUP/TEARDOWN/TAGS attach). */
#define POLYONTEST_SUITE(suite_name)                                             \
    static polyontest_suite_t polyontest_suite_##suite_name = {                    \
        #suite_name, NULL, NULL, NULL, NULL};                                  \
    static void polyontest_suite_base_reg_##suite_name(void)                     \
        __attribute__((constructor));                                          \
    static void polyontest_suite_base_reg_##suite_name(void) {                   \
        polyontest_register_suite(&polyontest_suite_##suite_name);                 \
    }

/**
 * Define suite + setup. Prefer this *or* POLYONTEST_SUITE (not both) so there is
 * a single polyontest_suite_##name object. TEARDOWN/TAGS mutate the same object.
 */
#define POLYONTEST_SUITE_SETUP(suite_name)                                       \
    static void polyontest_suite_setup_##suite_name(void);                       \
    static polyontest_suite_t polyontest_suite_##suite_name = {                    \
        #suite_name, polyontest_suite_setup_##suite_name, NULL, NULL, NULL};     \
    static void polyontest_suite_su_reg_##suite_name(void)                       \
        __attribute__((constructor));                                          \
    static void polyontest_suite_su_reg_##suite_name(void) {                     \
        polyontest_register_suite(&polyontest_suite_##suite_name);                 \
    }                                                                          \
    static void polyontest_suite_setup_##suite_name(void)

#define POLYONTEST_SUITE_TEARDOWN(suite_name)                                    \
    static void polyontest_suite_teardown_##suite_name(void);                    \
    static void polyontest_suite_td_reg_##suite_name(void)                       \
        __attribute__((constructor));                                          \
    static void polyontest_suite_td_reg_##suite_name(void) {                     \
        polyontest_suite_##suite_name.teardown =                                 \
            polyontest_suite_teardown_##suite_name;                              \
        polyontest_register_suite(&polyontest_suite_##suite_name);                 \
    }                                                                          \
    static void polyontest_suite_teardown_##suite_name(void)

#if POLYONTEST_CFG_HAS_TAGS
#define POLYONTEST_SUITE_TAGS(suite_name, ...)                                   \
    static const char *const polyontest_suite_tags_##suite_name[] = {            \
        __VA_ARGS__, NULL};                                                    \
    static void polyontest_suite_tags_reg_##suite_name(void)                     \
        __attribute__((constructor));                                          \
    static void polyontest_suite_tags_reg_##suite_name(void) {                   \
        polyontest_suite_##suite_name.tags = polyontest_suite_tags_##suite_name;   \
        polyontest_register_suite(&polyontest_suite_##suite_name);                 \
    }
#else
#define POLYONTEST_SUITE_TAGS(suite_name, ...)                                   \
    enum { polyontest_suite_tags_unused_##suite_name = 0 }
#endif

#define POLYONTEST_GROUP_SETUP(suite_name, group_name)                           \
    static void polyontest_group_setup_##suite_name##_##group_name(void);        \
    static polyontest_group_t polyontest_group_##suite_name##_##group_name = {     \
        #suite_name,                                                           \
        #group_name,                                                           \
        polyontest_group_setup_##suite_name##_##group_name,                      \
        NULL,                                                                  \
        NULL,                                                                  \
        NULL};                                                                  \
    static void polyontest_group_su_reg_##suite_name##_##group_name(void)        \
        __attribute__((constructor));                                          \
    static void polyontest_group_su_reg_##suite_name##_##group_name(void) {      \
        polyontest_register_group(                                               \
            &polyontest_group_##suite_name##_##group_name);                      \
    }                                                                          \
    static void polyontest_group_setup_##suite_name##_##group_name(void)

#define POLYONTEST_GROUP_TEARDOWN(suite_name, group_name)                        \
    static void polyontest_group_teardown_##suite_name##_##group_name(void);     \
    static void polyontest_group_td_reg_##suite_name##_##group_name(void)        \
        __attribute__((constructor));                                          \
    static void polyontest_group_td_reg_##suite_name##_##group_name(void) {      \
        polyontest_group_##suite_name##_##group_name.teardown =                  \
            polyontest_group_teardown_##suite_name##_##group_name;               \
        polyontest_register_group(                                               \
            &polyontest_group_##suite_name##_##group_name);                      \
    }                                                                          \
    static void polyontest_group_teardown_##suite_name##_##group_name(void)

#if POLYONTEST_CFG_HAS_TAGS
#define POLYONTEST_GROUP_TAGS(suite_name, group_name, ...)                       \
    static const char *const polyontest_group_tags_##suite_name##_##group_name[] \
        = {__VA_ARGS__, NULL};                                                 \
    static void polyontest_group_tags_reg_##suite_name##_##group_name(void)      \
        __attribute__((constructor));                                          \
    static void polyontest_group_tags_reg_##suite_name##_##group_name(void) {    \
        polyontest_group_##suite_name##_##group_name.tags =                      \
            polyontest_group_tags_##suite_name##_##group_name;                   \
        polyontest_register_group(                                               \
            &polyontest_group_##suite_name##_##group_name);                      \
    }
#else
#define POLYONTEST_GROUP_TAGS(suite_name, group_name, ...)                       \
    enum { polyontest_group_tags_unused_##suite_name##_##group_name = 0 }
#endif

#else /* !POLYONTEST_CFG_HAS_FIXTURES — tiny: accept macros, skip registration */

#define POLYONTEST_SUITE(suite_name)                                             \
    enum { polyontest_suite_unused_##suite_name = 0 }
#define POLYONTEST_SUITE_SETUP(suite_name)                                       \
    static void polyontest_suite_setup_##suite_name(void)
#define POLYONTEST_SUITE_TEARDOWN(suite_name)                                    \
    static void polyontest_suite_teardown_##suite_name(void)
#define POLYONTEST_SUITE_TAGS(suite_name, ...)                                   \
    enum { polyontest_suite_tags_unused_##suite_name = 0 }
#define POLYONTEST_GROUP_SETUP(suite_name, group_name)                           \
    static void polyontest_group_setup_##suite_name##_##group_name(void)
#define POLYONTEST_GROUP_TEARDOWN(suite_name, group_name)                        \
    static void polyontest_group_teardown_##suite_name##_##group_name(void)
#define POLYONTEST_GROUP_TAGS(suite_name, group_name, ...)                       \
    enum { polyontest_group_tags_unused_##suite_name##_##group_name = 0 }

#endif /* POLYONTEST_CFG_HAS_FIXTURES */

#ifdef POLYONTEST_USE_SECTION_REGISTRY
#define POLYONTEST_TEST(suite_name, group_name, case_name)                       \
    static void polyontest_body_##suite_name##_##group_name##_##case_name(void); \
    static polyontest_case_t                                                     \
        polyontest_desc_##suite_name##_##group_name##_##case_name                \
            POLYONTEST_SECTION = {#suite_name,                                   \
                                #group_name,                                   \
                                #case_name,                                    \
                                polyontest_body_##suite_name##_##group_name##_##case_name, \
                                NULL,                                          \
                                NULL};                                          \
    static void polyontest_body_##suite_name##_##group_name##_##case_name(void)
#else
#define POLYONTEST_TEST(suite_name, group_name, case_name)                       \
    static void polyontest_body_##suite_name##_##group_name##_##case_name(void); \
    static polyontest_case_t                                                     \
        polyontest_desc_##suite_name##_##group_name##_##case_name = {            \
            #suite_name,                                                       \
            #group_name,                                                       \
            #case_name,                                                        \
            polyontest_body_##suite_name##_##group_name##_##case_name,           \
            NULL,                                                              \
            NULL};                                                              \
    static void polyontest_reg_##suite_name##_##group_name##_##case_name(void)   \
        __attribute__((constructor));                                          \
    static void polyontest_reg_##suite_name##_##group_name##_##case_name(void) { \
        polyontest_register(                                                     \
            &polyontest_desc_##suite_name##_##group_name##_##case_name);         \
    }                                                                          \
    static void polyontest_body_##suite_name##_##group_name##_##case_name(void)
#endif

#if POLYONTEST_CFG_HAS_TAGS
#define POLYONTEST_TEST_TAGS(suite_name, group_name, case_name, ...)             \
    static void polyontest_body_##suite_name##_##group_name##_##case_name(void); \
    static const char *const                                                   \
        polyontest_case_tags_##suite_name##_##group_name##_##case_name[] = {     \
            __VA_ARGS__, NULL};                                                \
    static polyontest_case_t                                                     \
        polyontest_desc_##suite_name##_##group_name##_##case_name = {            \
            #suite_name,                                                       \
            #group_name,                                                       \
            #case_name,                                                        \
            polyontest_body_##suite_name##_##group_name##_##case_name,           \
            polyontest_case_tags_##suite_name##_##group_name##_##case_name,      \
            NULL};                                                              \
    static void polyontest_reg_##suite_name##_##group_name##_##case_name(void)   \
        __attribute__((constructor));                                          \
    static void polyontest_reg_##suite_name##_##group_name##_##case_name(void) { \
        polyontest_register(                                                     \
            &polyontest_desc_##suite_name##_##group_name##_##case_name);         \
    }                                                                          \
    static void polyontest_body_##suite_name##_##group_name##_##case_name(void)
#else
#define POLYONTEST_TEST_TAGS(suite_name, group_name, case_name, ...)             \
    POLYONTEST_TEST(suite_name, group_name, case_name)
#endif

/**
 * Parameterized helper — invoke body once per table row from inside a TEST.
 * Sets the param cursor so failures can append `[param=<index>]`.
 */
#define POLYONTEST_FOR_EACH(type, var, array)                                    \
    for (size_t _pt_i = 0;                                                     \
         _pt_i < sizeof(array) / sizeof((array)[0]); ++_pt_i)                  \
        for (type var = (array)[_pt_i],                                         \
                 *_pt_once = (polyontest_set_param(_pt_i, &(var)), &var);        \
             _pt_once;                                                         \
             _pt_once = (polyontest_clear_param(), (type *)NULL))

/** Typed view of the current PARAM_TEST / FOR_EACH row. */
#define POLYONTEST_PARAM_AS(type) (*(const type *)polyontest_current_param())

/**
 * Register one case that runs `table` row-by-row. Body uses PARAM_AS(type).
 * Requires small/full profile (fixtures/hierarchy enabled).
 */
#if POLYONTEST_CFG_HAS_FIXTURES
#define POLYONTEST_PARAM_TEST(suite_name, group_name, case_name, type, table)    \
    static void polyontest_param_impl_##suite_name##_##group_name##_##case_name( \
        void);                                                                 \
    POLYONTEST_TEST(suite_name, group_name, case_name) {                         \
        size_t _pt_n = sizeof(table) / sizeof((table)[0]);                     \
        size_t _pt_i;                                                          \
        for (_pt_i = 0; _pt_i < _pt_n; ++_pt_i) {                              \
            polyontest_set_param(_pt_i, &(table)[_pt_i]);                        \
            polyontest_param_impl_##suite_name##_##group_name##_##case_name();   \
            polyontest_clear_param();                                            \
        }                                                                      \
    }                                                                          \
    static void polyontest_param_impl_##suite_name##_##group_name##_##case_name( \
        void)
#else
#define POLYONTEST_PARAM_TEST(suite_name, group_name, case_name, type, table)    \
    POLYONTEST_PARAM_TEST_requires_small_or_full_profile(suite_name, group_name, \
                                                       case_name, type, table)
#endif

#ifndef POLYONTEST_NO_ALIASES
#define TEST POLYONTEST_TEST
#define TEST_TAGS POLYONTEST_TEST_TAGS
#define FOR_EACH POLYONTEST_FOR_EACH
#define PARAM_AS POLYONTEST_PARAM_AS
#if POLYONTEST_CFG_HAS_FIXTURES
#define PARAM_TEST POLYONTEST_PARAM_TEST
#endif
#define ASSERT_TRUE POLYONTEST_ASSERT_TRUE
#define ASSERT_TRUE_MESSAGE POLYONTEST_ASSERT_TRUE_MESSAGE
#define ASSERT_FALSE POLYONTEST_ASSERT_FALSE
#define ASSERT_FALSE_MESSAGE POLYONTEST_ASSERT_FALSE_MESSAGE
#define ASSERT_EQ POLYONTEST_ASSERT_EQUAL_INT
#define ASSERT_NE POLYONTEST_ASSERT_NOT_EQUAL_INT
#define ASSERT_EQUAL_INT POLYONTEST_ASSERT_EQUAL_INT
#define ASSERT_EQUAL_UINT POLYONTEST_ASSERT_EQUAL_UINT
#define ASSERT_EQUAL_HEX32 POLYONTEST_ASSERT_EQUAL_HEX32
#define ASSERT_NULL POLYONTEST_ASSERT_NULL
#define ASSERT_NOT_NULL POLYONTEST_ASSERT_NOT_NULL
#if POLYONTEST_CFG_HAS_EXTENDED_ASSERTS
#define ASSERT_BITS POLYONTEST_ASSERT_BITS
#define ASSERT_BITS_HIGH POLYONTEST_ASSERT_BITS_HIGH
#define ASSERT_BITS_LOW POLYONTEST_ASSERT_BITS_LOW
#define ASSERT_EQUAL_STRING POLYONTEST_ASSERT_EQUAL_STRING
#define ASSERT_EQUAL_MEMORY POLYONTEST_ASSERT_EQUAL_MEMORY
#endif
#define ASSERT_GREATER_THAN POLYONTEST_ASSERT_GREATER_THAN
#define ASSERT_LESS_THAN POLYONTEST_ASSERT_LESS_THAN
#define ASSERT_INT_WITHIN POLYONTEST_ASSERT_INT_WITHIN
#define FAIL POLYONTEST_FAIL
#define IGNORE POLYONTEST_IGNORE
#define IGNORE_MESSAGE POLYONTEST_IGNORE_MESSAGE
#define TEST_PROTECT POLYONTEST_PROTECT
#define TEST_ABORT POLYONTEST_ABORT
#ifndef POLYONTEST_EXCLUDE_FLOAT
#define ASSERT_FLOAT_WITHIN POLYONTEST_ASSERT_FLOAT_WITHIN
#define ASSERT_EQUAL_FLOAT POLYONTEST_ASSERT_EQUAL_FLOAT
#define ASSERT_DOUBLE_WITHIN POLYONTEST_ASSERT_DOUBLE_WITHIN
#define ASSERT_EQUAL_DOUBLE POLYONTEST_ASSERT_EQUAL_DOUBLE
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif /* POLYONTEST_H */
