use std::env;
use std::path::PathBuf;

fn main() {
    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let root = manifest_dir.join("../..");
    let harness_c = root.join("harness/c");
    let include = root.join("harness/include");

    println!("cargo:rerun-if-changed={}", harness_c.join("polyontest_core.c").display());
    println!("cargo:rerun-if-changed={}", harness_c.join("polyontest_assert.c").display());
    println!("cargo:rerun-if-changed={}", manifest_dir.join("tests.c").display());
    println!("cargo:rerun-if-changed={}", include.join("polyontest/polyontest.h").display());

    let mut build = cc::Build::new();
    build
        .file(harness_c.join("polyontest_core.c"))
        .file(harness_c.join("polyontest_assert.c"))
        .file(manifest_dir.join("tests.c"))
        .include(&include)
        .std("c11")
        .define("POLYONTEST_PROFILE_FULL", None);

    let text = env::var_os("CARGO_FEATURE_TEXT").is_some();
    let cobs = env::var_os("CARGO_FEATURE_COBS").is_some();
    if text || !cobs {
        build.define("POLYONTEST_MINIMAL_PRINT", None);
    }

    build.compile("polyontest_host_rust");
}
