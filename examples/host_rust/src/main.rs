//! Host Rust example — links C harness + C TEST cases via build.rs.

unsafe extern "C" {
    /// Pulls `tests.c` into the link so TEST constructors are retained.
    fn polyontest_host_rust_link_anchor() -> i32;
}

fn main() {
    unsafe {
        let _ = polyontest_host_rust_link_anchor();
    }
    let code = polyontest_rs::std_support::run_from_env();
    std::process::exit(code);
}
