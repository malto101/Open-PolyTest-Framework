use std::path::PathBuf;
use std::process::Command;

struct LockGuard {
    lock_path: PathBuf,
}

impl Drop for LockGuard {
    fn drop(&mut self) {
        let _ = std::fs::remove_file(&self.lock_path);
    }
}

fn acquire_lock(lock_path: PathBuf) -> LockGuard {
    let start = std::time::Instant::now();
    loop {
        match std::fs::OpenOptions::new()
            .write(true)
            .create_new(true)
            .open(&lock_path)
        {
            Ok(_) => return LockGuard { lock_path },
            Err(e) if e.kind() == std::io::ErrorKind::AlreadyExists => {
                if let Ok(metadata) = std::fs::metadata(&lock_path) {
                    if let Ok(modified) = metadata.modified() {
                        if let Ok(elapsed) = modified.elapsed() {
                            if elapsed > std::time::Duration::from_secs(60) {
                                let _ = std::fs::remove_file(&lock_path);
                            }
                        }
                    }
                }
                if start.elapsed() > std::time::Duration::from_secs(60) {
                    panic!("timeout waiting to acquire CMake build lock");
                }
                std::thread::sleep(std::time::Duration::from_millis(50));
            }
            Err(e) => {
                panic!("failed to acquire lock: {:?}", e);
            }
        }
    }
}

static BUILD_HARNESS: std::sync::Once = std::sync::Once::new();

fn build_harness_binaries() {
    BUILD_HARNESS.call_once(|| {
        let manifest_dir = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
        let workspace_root = manifest_dir.join("../..");
        let build_dir = workspace_root.join("build/host_c");
        std::fs::create_dir_all(&build_dir).unwrap();
        let lock_path = build_dir.join("build.lock");

        let _guard = acquire_lock(lock_path);

        let source_dir = workspace_root.join("examples/host_c");

        // 1. Configure CMake
        let mut configure = Command::new("cmake");
        configure.current_dir(&workspace_root)
            .arg("-S")
            .arg(&source_dir)
            .arg("-B")
            .arg(&build_dir)
            .arg("-DPOLYONTEST_MINIMAL_PRINT=OFF");
        let status = configure.status().expect("failed to run cmake configure");
        assert!(status.success(), "cmake configuration failed");

        // 2. Build CMake
        let mut build = Command::new("cmake");
        build.current_dir(&workspace_root)
            .arg("--build")
            .arg(&build_dir);
        let status = build.status().expect("failed to run cmake build");
        assert!(status.success(), "cmake build failed");
    });
}

fn get_binary_path(name: &str) -> PathBuf {
    build_harness_binaries();
    let path = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join(format!("../../build/host_c/{}", name))
        .canonicalize()
        .unwrap();
    assert!(path.exists(), "binary {} does not exist", name);
    path
}

fn get_harness_binary_path() -> PathBuf {
    get_binary_path("host_c_tests")
}

fn decode_stdout_to_events(stdout: &[u8]) -> Vec<polyontest_protocol::Event> {
    use polyontest_plugin_api::Codec;
    // Try COBS first
    let mut cobs = polyontest_builtins::CobsCodec::new();
    if let Ok(events) = cobs.decode_feed(stdout) {
        if !events.is_empty() {
            return events;
        }
    }
    // Fallback to Text
    let mut text = polyontest_builtins::TextCodec::new();
    text.decode_feed(stdout).unwrap_or_default()
}

#[test]
fn test_c_harness_discovery() {
    let mut cmd = Command::new(get_harness_binary_path());
    cmd.env("POLY_DISCOVER", "1");
    let output = cmd.output().expect("failed to execute C harness tests");
    let stdout = String::from_utf8(output.stdout).unwrap();
    
    assert!(output.status.success(), "expected exit status 0, got {:?}", output.status);
    
    // Check that we see the listed cases in the correct format
    assert!(stdout.contains("list-case:Math.Basic.AddPositive"), "stdout should contain discovery line, got: {}", stdout);
    assert!(stdout.contains("list-case:Expect.Pointers.NotNull"), "stdout should contain discovery line, got: {}", stdout);
    
    // Check that no actual tests ran
    assert!(!stdout.contains("PASS Math.Basic.AddPositive"), "should not run tests during discovery");
    assert!(!stdout.contains("DONE passed="), "should not emit DONE during discovery");
}

#[test]
fn test_c_harness_case_filtering() {
    let mut cmd = Command::new(get_harness_binary_path());
    cmd.env("POLY_SUITE", "Math");
    cmd.env("POLY_GROUP", "Basic");
    cmd.env("POLY_CASE", "AddPositive");
    let output = cmd.output().expect("failed to execute C harness tests");
    
    assert!(output.status.success(), "expected exit status 0, got {:?}", output.status);
    
    let events = decode_stdout_to_events(&output.stdout);
    
    // Check that only AddPositive ran
    assert!(
        events.iter().any(|ev| matches!(ev, polyontest_protocol::Event::CaseStart { suite, name } if suite == "Math" && name == "Basic.AddPositive")),
        "expected CaseStart for AddPositive, got: {:?}", events
    );
    assert!(
        events.iter().any(|ev| matches!(ev, polyontest_protocol::Event::CaseEnd { suite, name, status: polyontest_protocol::TestStatus::Passed } if suite == "Math" && name == "Basic.AddPositive")),
        "expected CaseEnd Passed for AddPositive"
    );
    
    // Check that other tests did NOT run
    assert!(
        !events.iter().any(|ev| matches!(ev, polyontest_protocol::Event::CaseStart { name, .. } if name == "Basic.AddZero")),
        "AddZero should be skipped"
    );
    assert!(
        !events.iter().any(|ev| matches!(ev, polyontest_protocol::Event::CaseStart { suite, .. } if suite == "Expect")),
        "Expect suite should be skipped"
    );
}

#[test]
fn test_cli_isolation_loop_success() {
    let config_path = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("../../examples/host_c/polyontest.toml")
        .canonicalize()
        .unwrap();

    get_binary_path("host_c_tests");

    let mut cmd = Command::new("cargo");
    cmd.current_dir(PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../.."));
    cmd.arg("run")
       .arg("--bin")
       .arg("polyontest")
       .arg("--")
       .arg("run")
       .arg("--target")
       .arg("host")
       .arg("--config")
       .arg(config_path)
       .arg("--isolate");

    let output = cmd.output().expect("failed to execute cargo run");
    let stdout = String::from_utf8(output.stdout).unwrap();
    let stderr = String::from_utf8(output.stderr).unwrap();

    assert!(output.status.success(), "expected CLI run to succeed, got error: {}", stderr);
    assert!(stdout.contains("Math.Basic.AddPositive ... ok"), "expected AddPositive to pass, got: {}", stdout);
    assert!(stdout.contains("Expect.Pointers.NotNull ... ok"), "expected NotNull to pass");
}

#[test]
fn test_cli_isolation_loop_crash_continue() {
    let temp_dir = std::env::temp_dir();
    let temp_config_path = temp_dir.join("polyontest_temp_continue.toml");
    let crash_binary_path = get_binary_path("host_c_crash_tests");

    let toml_content = format!(
        r#"
        [target.host]
        board = "host"
        transport = "stdio"
        codec = "text"
        mode = "stream"
        reporters = ["console"]
        binary = "{}"
        timeout_ms = 10000
        "#,
        crash_binary_path.to_str().unwrap()
    );
    std::fs::write(&temp_config_path, toml_content).unwrap();

    let mut cmd = Command::new("cargo");
    cmd.current_dir(PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../.."));
    cmd.arg("run")
       .arg("--bin")
       .arg("polyontest")
       .arg("--")
       .arg("run")
       .arg("--target")
       .arg("host")
       .arg("--config")
       .arg(&temp_config_path)
       .arg("--isolate")
       .arg("--on-crash")
       .arg("continue");

    let output = cmd.output().expect("failed to execute cargo run");
    let stdout = String::from_utf8(output.stdout).unwrap();

    // Should fail because CrashMe failed/crashed
    assert!(!output.status.success(), "expected CLI to exit with failure code");
    assert!(stdout.contains("Math.Basic.CrashMe"), "expected CrashMe to be reported, got: {}", stdout);
    assert!(stdout.contains("FAILED"), "expected FAILED status for CrashMe");
    assert!(stdout.contains("Process exited with status"), "expected exit status error message");
    // Assert that a subsequent test case (ProtectRegion) runs and passes successfully
    assert!(stdout.contains("Math.Basic.ProtectRegion ... ok"), "expected subsequent tests like ProtectRegion to run and pass, got: {}", stdout);
}

#[test]
fn test_cli_isolation_loop_crash_abort() {
    let temp_dir = std::env::temp_dir();
    let temp_config_path = temp_dir.join("polyontest_temp_abort.toml");
    let crash_binary_path = get_binary_path("host_c_crash_tests");

    let toml_content = format!(
        r#"
        [target.host]
        board = "host"
        transport = "stdio"
        codec = "text"
        mode = "stream"
        reporters = ["console"]
        binary = "{}"
        timeout_ms = 10000
        "#,
        crash_binary_path.to_str().unwrap()
    );
    std::fs::write(&temp_config_path, toml_content).unwrap();

    let mut cmd = Command::new("cargo");
    cmd.current_dir(PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../.."));
    cmd.arg("run")
       .arg("--bin")
       .arg("polyontest")
       .arg("--")
       .arg("run")
       .arg("--target")
       .arg("host")
       .arg("--config")
       .arg(&temp_config_path)
       .arg("--isolate")
       .arg("--on-crash")
       .arg("abort");

    let output = cmd.output().expect("failed to execute cargo run");
    let stdout = String::from_utf8(output.stdout).unwrap();

    // Should fail
    assert!(!output.status.success());
    assert!(stdout.contains("Math.Basic.CrashMe"), "expected CrashMe to be reported, got: {}", stdout);
    assert!(stdout.contains("FAILED"), "expected FAILED status for CrashMe");
    assert!(stdout.contains("Process exited with status"), "expected exit status error message");
    // Under abort mode, subsequent tests (like ProtectRegion) should NOT run
    assert!(!stdout.contains("Math.Basic.ProtectRegion"), "expected suite execution to abort before running ProtectRegion");
}

#[test]
fn test_cli_isolation_with_tag() {
    get_binary_path("host_c_tests");
    let config_path = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("../../examples/host_c/polyontest.toml")
        .canonicalize()
        .unwrap();

    let mut cmd = Command::new("cargo");
    cmd.current_dir(PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../.."));
    cmd.arg("run")
       .arg("--bin")
       .arg("polyontest")
       .arg("--")
       .arg("run")
       .arg("--target")
       .arg("host")
       .arg("--config")
       .arg(config_path)
       .arg("--isolate")
       .arg("--tag")
       .arg("smoke");

    let output = cmd.output().expect("failed to execute cargo run");
    let stdout = String::from_utf8(output.stdout).unwrap();
    let stderr = String::from_utf8(output.stderr).unwrap();

    assert!(output.status.success(), "expected CLI run to succeed, got error: {}", stderr);
    
    // Verify each tagged test case runs exactly once
    assert_eq!(stdout.matches("Math.Basic.TypedAndBits ... ok").count(), 1, "expected TypedAndBits to run exactly once under isolation, got: {}", stdout);
    assert_eq!(stdout.matches("Math.Basic.UsesGroupSetup ... ok").count(), 1, "expected UsesGroupSetup to run exactly once under isolation, got: {}", stdout);
    
    // Verify non-tagged test cases are excluded
    assert!(!stdout.contains("Expect.Pointers.NotNull"), "expected non-smoke tests to be excluded, got: {}", stdout);
}

#[test]
fn test_cli_isolation_loop_timeout() {
    let temp_dir = std::env::temp_dir();
    let temp_config_path = temp_dir.join("polyontest_temp_timeout.toml");
    let hang_binary_path = get_binary_path("host_c_hang_tests");

    // Set a very short timeout of 200 milliseconds to trigger timeout on HangMe
    let toml_content = format!(
        r#"
        [target.host]
        board = "host"
        transport = "stdio"
        codec = "text"
        mode = "stream"
        reporters = ["console"]
        binary = "{}"
        timeout_ms = 200
        "#,
        hang_binary_path.to_str().unwrap()
    );
    std::fs::write(&temp_config_path, toml_content).unwrap();

    let start = std::time::Instant::now();

    let mut cmd = Command::new("cargo");
    cmd.current_dir(PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../.."));
    cmd.arg("run")
       .arg("--bin")
       .arg("polyontest")
       .arg("--")
       .arg("run")
       .arg("--target")
       .arg("host")
       .arg("--config")
       .arg(&temp_config_path)
       .arg("--isolate")
       .arg("--suite")
       .arg("HangSuite")
       .arg("--group")
       .arg("HangGroup")
       .arg("--on-crash")
       .arg("continue");

    let output = cmd.output().expect("failed to execute cargo run");
    let stdout = String::from_utf8(output.stdout).unwrap();
    let elapsed = start.elapsed();

    // Verify it terminated without hanging forever (should be way under 10 seconds)
    assert!(elapsed < std::time::Duration::from_secs(6), "test execution hung past timeout: {:?}", elapsed);
    assert!(!output.status.success(), "expected failure due to timeout");
    assert!(stdout.contains("HangSuite.HangGroup.HangMe"), "expected HangMe to be reported");
    assert!(stdout.contains("FAILED"), "expected FAILED status due to timeout");
    assert!(stdout.contains("timeout waiting for child process events"), "expected error message containing timeout, got: {}", stdout);
}

#[test]
fn test_cli_invalid_on_crash() {
    let config_path = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("../../examples/host_c/polyontest.toml")
        .canonicalize()
        .unwrap();

    let mut cmd = Command::new("cargo");
    cmd.current_dir(PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../.."));
    cmd.arg("run")
       .arg("--bin")
       .arg("polyontest")
       .arg("--")
       .arg("run")
       .arg("--target")
       .arg("host")
       .arg("--config")
       .arg(config_path)
       .arg("--on-crash")
       .arg("invalid_typo");

    let output = cmd.output().expect("failed to execute cargo run");
    let stderr = String::from_utf8(output.stderr).unwrap();

    assert!(!output.status.success(), "expected validation error for invalid on-crash option");
    assert!(stderr.contains("invalid on-crash policy"), "expected stderr validation message, got: {}", stderr);
}


