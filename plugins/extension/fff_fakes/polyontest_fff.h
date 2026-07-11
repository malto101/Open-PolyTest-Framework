/**
 * Minimal FFF-style fake helpers (header-only).
 * Not a full FFF port — enough for v1 function-pointer fakes without codegen.
 *
 * Philosophy (same as Fake Function Framework):
 *   https://github.com/meekrosoft/fff
 * Header-only, no Ruby/codegen (unlike CMock). Define fakes in the test TU,
 * set return values / inspect call counts and args, RESET between cases.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef POLYONTEST_FFF_FAKES_H
#define POLYONTEST_FFF_FAKES_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef POLYONTEST_FFF_ARG_HISTORY_LEN
#define POLYONTEST_FFF_ARG_HISTORY_LEN 10
#endif

/* -------------------------------------------------------------------------- */
/* Value fakes                                                                */
/* -------------------------------------------------------------------------- */

/* Fake *definitions* are non-static so they can satisfy a public HAL header. */

#define POLYONTEST_FAKE_VALUE_FUNC0(ret_type, fn_name, default_ret)               \
    static ret_type fn_name##_return = (default_ret);                          \
    static int fn_name##_call_count;                                           \
    static ret_type (*fn_name##_custom_fake)(void);                            \
    ret_type fn_name(void) {                                                   \
        fn_name##_call_count++;                                                \
        if (fn_name##_custom_fake) {                                           \
            return fn_name##_custom_fake();                                    \
        }                                                                      \
        return fn_name##_return;                                               \
    }

#define POLYONTEST_FAKE_VALUE_FUNC1(ret_type, fn_name, arg0_type, default_ret)   \
    static ret_type fn_name##_return = (default_ret);                          \
    static int fn_name##_call_count;                                           \
    static arg0_type fn_name##_arg0_val;                                       \
    static arg0_type fn_name##_arg0_history[POLYONTEST_FFF_ARG_HISTORY_LEN];     \
    static ret_type (*fn_name##_custom_fake)(arg0_type);                       \
    ret_type fn_name(arg0_type a0) {                                           \
        if (fn_name##_call_count < POLYONTEST_FFF_ARG_HISTORY_LEN) {             \
            fn_name##_arg0_history[fn_name##_call_count] = a0;                 \
        }                                                                      \
        fn_name##_arg0_val = a0;                                               \
        fn_name##_call_count++;                                                \
        if (fn_name##_custom_fake) {                                           \
            return fn_name##_custom_fake(a0);                                  \
        }                                                                      \
        return fn_name##_return;                                               \
    }

#define POLYONTEST_FAKE_VALUE_FUNC2(ret_type, fn_name, arg0_type, arg1_type,     \
                                  default_ret)                                 \
    static ret_type fn_name##_return = (default_ret);                          \
    static int fn_name##_call_count;                                           \
    static arg0_type fn_name##_arg0_val;                                       \
    static arg1_type fn_name##_arg1_val;                                       \
    static arg0_type fn_name##_arg0_history[POLYONTEST_FFF_ARG_HISTORY_LEN];     \
    static arg1_type fn_name##_arg1_history[POLYONTEST_FFF_ARG_HISTORY_LEN];     \
    static ret_type (*fn_name##_custom_fake)(arg0_type, arg1_type);            \
    ret_type fn_name(arg0_type a0, arg1_type a1) {                             \
        if (fn_name##_call_count < POLYONTEST_FFF_ARG_HISTORY_LEN) {             \
            fn_name##_arg0_history[fn_name##_call_count] = a0;                 \
            fn_name##_arg1_history[fn_name##_call_count] = a1;                 \
        }                                                                      \
        fn_name##_arg0_val = a0;                                               \
        fn_name##_arg1_val = a1;                                               \
        fn_name##_call_count++;                                                \
        if (fn_name##_custom_fake) {                                           \
            return fn_name##_custom_fake(a0, a1);                              \
        }                                                                      \
        return fn_name##_return;                                               \
    }

/* -------------------------------------------------------------------------- */
/* Void fakes                                                                 */
/* -------------------------------------------------------------------------- */

#define POLYONTEST_FAKE_VOID_FUNC0(fn_name)                                      \
    static int fn_name##_call_count;                                           \
    static void (*fn_name##_custom_fake)(void);                                \
    void fn_name(void) {                                                       \
        fn_name##_call_count++;                                                \
        if (fn_name##_custom_fake) {                                           \
            fn_name##_custom_fake();                                           \
        }                                                                      \
    }

#define POLYONTEST_FAKE_VOID_FUNC1(fn_name, arg0_type)                           \
    static int fn_name##_call_count;                                           \
    static arg0_type fn_name##_arg0_val;                                       \
    static arg0_type fn_name##_arg0_history[POLYONTEST_FFF_ARG_HISTORY_LEN];     \
    static void (*fn_name##_custom_fake)(arg0_type);                           \
    void fn_name(arg0_type a0) {                                               \
        if (fn_name##_call_count < POLYONTEST_FFF_ARG_HISTORY_LEN) {             \
            fn_name##_arg0_history[fn_name##_call_count] = a0;                 \
        }                                                                      \
        fn_name##_arg0_val = a0;                                               \
        fn_name##_call_count++;                                                \
        if (fn_name##_custom_fake) {                                           \
            fn_name##_custom_fake(a0);                                         \
        }                                                                      \
    }

#define POLYONTEST_FAKE_VOID_FUNC2(fn_name, arg0_type, arg1_type)                \
    static int fn_name##_call_count;                                           \
    static arg0_type fn_name##_arg0_val;                                       \
    static arg1_type fn_name##_arg1_val;                                       \
    static arg0_type fn_name##_arg0_history[POLYONTEST_FFF_ARG_HISTORY_LEN];     \
    static arg1_type fn_name##_arg1_history[POLYONTEST_FFF_ARG_HISTORY_LEN];     \
    static void (*fn_name##_custom_fake)(arg0_type, arg1_type);                \
    void fn_name(arg0_type a0, arg1_type a1) {                                 \
        if (fn_name##_call_count < POLYONTEST_FFF_ARG_HISTORY_LEN) {             \
            fn_name##_arg0_history[fn_name##_call_count] = a0;                 \
            fn_name##_arg1_history[fn_name##_call_count] = a1;                 \
        }                                                                      \
        fn_name##_arg0_val = a0;                                               \
        fn_name##_arg1_val = a1;                                               \
        fn_name##_call_count++;                                                \
        if (fn_name##_custom_fake) {                                           \
            fn_name##_custom_fake(a0, a1);                                     \
        }                                                                      \
    }

/* -------------------------------------------------------------------------- */
/* RESET — clear call count, last args, history, custom fake                  */
/* -------------------------------------------------------------------------- */

#define POLYONTEST_FAKE_RESET(fn_name)                                           \
    do {                                                                       \
        fn_name##_call_count = 0;                                              \
        fn_name##_custom_fake = 0;                                             \
    } while (0)

/** Reset value-returning fake + scalar arg0 history (zero-fill). */
#define POLYONTEST_FAKE_RESET_VALUE1(fn_name, default_ret)                       \
    do {                                                                       \
        unsigned _pt_i;                                                        \
        POLYONTEST_FAKE_RESET(fn_name);                                          \
        fn_name##_return = (default_ret);                                      \
        for (_pt_i = 0; _pt_i < POLYONTEST_FFF_ARG_HISTORY_LEN; ++_pt_i) {       \
            fn_name##_arg0_history[_pt_i] = 0;                                 \
        }                                                                      \
        fn_name##_arg0_val = 0;                                                \
    } while (0)

/** Reset value-returning fake + scalar arg0/arg1 history (zero-fill). */
#define POLYONTEST_FAKE_RESET_VALUE2(fn_name, default_ret)                       \
    do {                                                                       \
        unsigned _pt_i;                                                        \
        POLYONTEST_FAKE_RESET(fn_name);                                          \
        fn_name##_return = (default_ret);                                      \
        for (_pt_i = 0; _pt_i < POLYONTEST_FFF_ARG_HISTORY_LEN; ++_pt_i) {       \
            fn_name##_arg0_history[_pt_i] = 0;                                 \
            fn_name##_arg1_history[_pt_i] = 0;                                 \
        }                                                                      \
        fn_name##_arg0_val = 0;                                                \
        fn_name##_arg1_val = 0;                                                \
    } while (0)

#define POLYONTEST_FAKE_RESET_VOID1(fn_name)                                     \
    do {                                                                       \
        unsigned _pt_i;                                                        \
        POLYONTEST_FAKE_RESET(fn_name);                                          \
        for (_pt_i = 0; _pt_i < POLYONTEST_FFF_ARG_HISTORY_LEN; ++_pt_i) {       \
            fn_name##_arg0_history[_pt_i] = 0;                                 \
        }                                                                      \
        fn_name##_arg0_val = 0;                                                \
    } while (0)

#define POLYONTEST_FAKE_RESET_VOID2(fn_name)                                     \
    do {                                                                       \
        unsigned _pt_i;                                                        \
        POLYONTEST_FAKE_RESET(fn_name);                                          \
        for (_pt_i = 0; _pt_i < POLYONTEST_FFF_ARG_HISTORY_LEN; ++_pt_i) {       \
            fn_name##_arg0_history[_pt_i] = 0;                                 \
            fn_name##_arg1_history[_pt_i] = 0;                                 \
        }                                                                      \
        fn_name##_arg0_val = 0;                                                \
        fn_name##_arg1_val = 0;                                                \
    } while (0)

/* Short aliases matching FFF naming when POLYONTEST_FFF_ALIASES is set. */
#ifdef POLYONTEST_FFF_ALIASES
#define FAKE_VALUE_FUNC0 POLYONTEST_FAKE_VALUE_FUNC0
#define FAKE_VALUE_FUNC1 POLYONTEST_FAKE_VALUE_FUNC1
#define FAKE_VALUE_FUNC2 POLYONTEST_FAKE_VALUE_FUNC2
#define FAKE_VOID_FUNC0 POLYONTEST_FAKE_VOID_FUNC0
#define FAKE_VOID_FUNC1 POLYONTEST_FAKE_VOID_FUNC1
#define FAKE_VOID_FUNC2 POLYONTEST_FAKE_VOID_FUNC2
#define RESET_FAKE POLYONTEST_FAKE_RESET
#endif

#ifdef __cplusplus
}
#endif

#endif /* POLYONTEST_FFF_FAKES_H */
