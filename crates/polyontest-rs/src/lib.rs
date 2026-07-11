//! Rust adapter for the PolyOnTest C ABI.
//!
//! Default: `#![no_std]`. Enable feature `std` for host binaries.
//!
//! Link against the C harness (`polyontest_core.c` / amalgam `polyontest.c`),
//! register cases from C or via FFI, then call [`ffi::polyontest_run_all`] or
//! [`std_support::run_from_env`].
//!
//! A `#[polyontest::test]` proc-macro is planned later; it is not required for v1.
//!
//! # Example (host)
//!
//! See `examples/host_rust`.

#![cfg_attr(not(feature = "std"), no_std)]

/// Crate version string.
pub const POLYONTEST_RS_VERSION: &str = env!("CARGO_PKG_VERSION");

/// FFI declarations matching `harness/include/polyontest/polyontest.h`.
pub mod ffi {
    use core::ffi::{c_char, c_int, c_void};

    pub type PolyontestFn = Option<unsafe extern "C" fn()>;
    pub type PolyontestWriteFn =
        Option<unsafe extern "C" fn(data: *const c_void, len: usize, user: *mut c_void)>;
    pub type PolyontestLockFn = Option<unsafe extern "C" fn(user: *mut c_void)>;

    extern "C" {
        pub fn polyontest_set_writer(fn_: PolyontestWriteFn, user: *mut c_void);
        pub fn polyontest_run_all() -> c_int;
        pub fn polyontest_run_tag(tag: *const c_char) -> c_int;
        pub fn polyontest_run_suite(suite: *const c_char) -> c_int;
        pub fn polyontest_run_group(suite: *const c_char, group: *const c_char) -> c_int;
        pub fn polyontest_run_from_env() -> c_int;
        pub fn polyontest_fail(message: *const c_char, file: *const c_char, line: c_int);
        pub fn polyontest_fail_at(file: *const c_char, line: c_int, message: *const c_char);
        pub fn polyontest_set_param(index: usize, param: *const c_void);
        pub fn polyontest_clear_param();
        pub fn polyontest_param_index() -> usize;
        pub fn polyontest_current_param() -> *const c_void;
        pub fn polyontest_set_locks(
            lock: PolyontestLockFn,
            unlock: PolyontestLockFn,
            user: *mut c_void,
        );
    }
}

#[cfg(feature = "std")]
pub mod std_support {
    //! Host helpers when the `std` feature is enabled.
    use std::ffi::CString;

    use crate::ffi;

    /// Run all registered C cases and return the process-style exit code.
    pub fn run_all() -> i32 {
        unsafe { ffi::polyontest_run_all() }
    }

    /// Run cases matching a tag.
    pub fn run_tag(tag: &str) -> i32 {
        let c = CString::new(tag).expect("tag contains NUL");
        unsafe { ffi::polyontest_run_tag(c.as_ptr()) }
    }

    /// Run cases in a suite.
    pub fn run_suite(suite: &str) -> i32 {
        let c = CString::new(suite).expect("suite contains NUL");
        unsafe { ffi::polyontest_run_suite(c.as_ptr()) }
    }

    /// Run cases in a suite/group.
    pub fn run_group(suite: &str, group: &str) -> i32 {
        let s = CString::new(suite).expect("suite contains NUL");
        let g = CString::new(group).expect("group contains NUL");
        unsafe { ffi::polyontest_run_group(s.as_ptr(), g.as_ptr()) }
    }

    /// Honor `POLYONTEST_TAG` / `POLYONTEST_SUITE` / `POLYONTEST_GROUP`.
    pub fn run_from_env() -> i32 {
        unsafe { ffi::polyontest_run_from_env() }
    }
}
