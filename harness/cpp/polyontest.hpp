#ifndef POLYONTEST_HPP
#define POLYONTEST_HPP

/**
 * Thin C++ sugar over the PolyOnTest C ABI.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Keep TEST / ASSERT_* as the C macros (embedded-normal). This header wraps
 * runners and optional lock hooks.
 */

#include "polyontest/polyontest.h"

namespace polyontest {

inline int run_all() { return ::polyontest_run_all(); }

inline int run_tag(const char *tag) { return ::polyontest_run_tag(tag); }

inline int run_suite(const char *suite) { return ::polyontest_run_suite(suite); }

inline int run_group(const char *suite, const char *group) {
  return ::polyontest_run_group(suite, group);
}

/** Honor POLYONTEST_TAG / POLYONTEST_SUITE / POLYONTEST_GROUP from the environment. */
inline int run_from_env() { return ::polyontest_run_from_env(); }

inline void set_writer(polyontest_write_fn_t fn, void *user) {
  ::polyontest_set_writer(fn, user);
}

#if POLYONTEST_CFG_HAS_MUTEX
inline void set_locks(polyontest_lock_fn_t lock, polyontest_lock_fn_t unlock,
                      void *user) {
  ::polyontest_set_locks(lock, unlock, user);
}
#endif

} // namespace polyontest

#endif
