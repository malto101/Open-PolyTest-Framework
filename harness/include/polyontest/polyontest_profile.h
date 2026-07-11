/**
 * PolyOnTest compile-time size profiles
 * Copyright 2026 Dhruv Menon
 * SPDX-License-Identifier: Apache-2.0
 *
 * Select one of:
 *   POLYONTEST_PROFILE_TINY   — ~1–3 KB: text only, no tags/fixtures/float/longjmp
 *   POLYONTEST_PROFILE_SMALL  — hierarchy + tags + fixtures + COBS; no float by default
 *   POLYONTEST_PROFILE_FULL   — everything (default when no profile define is set)
 *
 * Or leave unset for FULL. Explicit feature knobs (POLYONTEST_MINIMAL_PRINT,
 * POLYONTEST_EXCLUDE_FLOAT, …) still override after the profile is applied.
 *
 * Derived feature macros (1 = enabled):
 *   POLYONTEST_CFG_HAS_COBS
 *   POLYONTEST_CFG_HAS_TAGS
 *   POLYONTEST_CFG_HAS_FIXTURES
 *   POLYONTEST_CFG_HAS_FLOAT
 *   POLYONTEST_CFG_HAS_PROTECT
 *   POLYONTEST_CFG_HAS_MUTEX
 *   POLYONTEST_CFG_HAS_EXTENDED_ASSERTS
 *   POLYONTEST_CFG_HAS_HEAP
 */
#ifndef POLYONTEST_PROFILE_H
#define POLYONTEST_PROFILE_H

/* Resolve which profile is active (exactly one). */
#if defined(POLYONTEST_PROFILE_TINY)
#define POLYONTEST_PROFILE_ACTIVE_TINY 1
#elif defined(POLYONTEST_PROFILE_SMALL)
#define POLYONTEST_PROFILE_ACTIVE_SMALL 1
#elif defined(POLYONTEST_PROFILE_FULL)
#define POLYONTEST_PROFILE_ACTIVE_FULL 1
#else
#define POLYONTEST_PROFILE_ACTIVE_FULL 1
#endif

/* -------------------------------------------------------------------------- */
/* tiny                                                                        */
/* -------------------------------------------------------------------------- */
#if defined(POLYONTEST_PROFILE_ACTIVE_TINY)

#ifndef POLYONTEST_MINIMAL_PRINT
#define POLYONTEST_MINIMAL_PRINT
#endif
#ifndef POLYONTEST_EXCLUDE_FLOAT
#define POLYONTEST_EXCLUDE_FLOAT
#endif
#ifndef POLYONTEST_NO_LONGJMP
#define POLYONTEST_NO_LONGJMP
#endif

#define POLYONTEST_CFG_HAS_COBS 0
#define POLYONTEST_CFG_HAS_TAGS 0
#define POLYONTEST_CFG_HAS_FIXTURES 0
#define POLYONTEST_CFG_HAS_FLOAT 0
#define POLYONTEST_CFG_HAS_PROTECT 0
#define POLYONTEST_CFG_HAS_MUTEX 0
#define POLYONTEST_CFG_HAS_EXTENDED_ASSERTS 0

/* -------------------------------------------------------------------------- */
/* small                                                                       */
/* -------------------------------------------------------------------------- */
#elif defined(POLYONTEST_PROFILE_ACTIVE_SMALL)

#ifndef POLYONTEST_EXCLUDE_FLOAT
#define POLYONTEST_EXCLUDE_FLOAT
#endif

#if defined(POLYONTEST_MINIMAL_PRINT)
#define POLYONTEST_CFG_HAS_COBS 0
#else
#define POLYONTEST_CFG_HAS_COBS 1
#endif
#define POLYONTEST_CFG_HAS_TAGS 1
#define POLYONTEST_CFG_HAS_FIXTURES 1
#if defined(POLYONTEST_EXCLUDE_FLOAT)
#define POLYONTEST_CFG_HAS_FLOAT 0
#else
#define POLYONTEST_CFG_HAS_FLOAT 1
#endif
#if defined(POLYONTEST_NO_LONGJMP)
#define POLYONTEST_CFG_HAS_PROTECT 0
#else
#define POLYONTEST_CFG_HAS_PROTECT 1
#endif
#define POLYONTEST_CFG_HAS_MUTEX 0
#define POLYONTEST_CFG_HAS_EXTENDED_ASSERTS 1

/* -------------------------------------------------------------------------- */
/* full (default)                                                              */
/* -------------------------------------------------------------------------- */
#else /* POLYONTEST_PROFILE_ACTIVE_FULL */

#if defined(POLYONTEST_MINIMAL_PRINT)
#define POLYONTEST_CFG_HAS_COBS 0
#else
#define POLYONTEST_CFG_HAS_COBS 1
#endif
#define POLYONTEST_CFG_HAS_TAGS 1
#define POLYONTEST_CFG_HAS_FIXTURES 1
#if defined(POLYONTEST_EXCLUDE_FLOAT)
#define POLYONTEST_CFG_HAS_FLOAT 0
#else
#define POLYONTEST_CFG_HAS_FLOAT 1
#endif
#if defined(POLYONTEST_NO_LONGJMP)
#define POLYONTEST_CFG_HAS_PROTECT 0
#else
#define POLYONTEST_CFG_HAS_PROTECT 1
#endif
#define POLYONTEST_CFG_HAS_MUTEX 1
#define POLYONTEST_CFG_HAS_EXTENDED_ASSERTS 1

#endif /* profiles */

/* Heap registration is orthogonal to size profiles. */
#if defined(POLYONTEST_USE_HEAP)
#define POLYONTEST_CFG_HAS_HEAP 1
#else
#define POLYONTEST_CFG_HAS_HEAP 0
#endif

/* Sync float exclusion with CFG when profile already stripped floats. */
#if !POLYONTEST_CFG_HAS_FLOAT && !defined(POLYONTEST_EXCLUDE_FLOAT)
#define POLYONTEST_EXCLUDE_FLOAT
#endif

/* Sync longjmp strip when protect is disabled. */
#if !POLYONTEST_CFG_HAS_PROTECT && !defined(POLYONTEST_NO_LONGJMP)
#define POLYONTEST_NO_LONGJMP
#endif

/* Tiny always forces text path even if user forgot MINIMAL_PRINT. */
#if !POLYONTEST_CFG_HAS_COBS && !defined(POLYONTEST_MINIMAL_PRINT)
#define POLYONTEST_MINIMAL_PRINT
#endif

#endif /* POLYONTEST_PROFILE_H */
