//! PolyOnTest CLI — composition root (Dependency Inversion).

use std::io::Read;
use std::path::{Path, PathBuf};
use std::time::Duration;

use anyhow::{bail, Context, Result};
use clap::{Parser, Subcommand};
use polyontest_builtins::{
    CobsCodec, ConsoleReporter, HostBoard, HostFilter, JsonReporter, JunitReporter, QemuM33Board,
    TextCodec,
};
use polyontest_plugin_api::{Board, Codec, Reporter, Transport};
use polyontest_protocol::{Event, TestStatus};
use serde::Deserialize;

#[derive(Parser, Debug)]
#[command(name = "polyontest", version, about = "PolyOnTest — embedded-first test framework CLI")]
struct Cli {
    #[command(subcommand)]
    cmd: Commands,
}

#[derive(Subcommand, Debug)]
enum Commands {
    /// Run tests using polyontest.toml plugin composition
    Run {
        /// Target name from polyontest.toml ([target.<name>])
        #[arg(long, default_value = "host")]
        target: String,
        /// Config file
        #[arg(long, default_value = "polyontest.toml")]
        config: PathBuf,
        /// Filter by tag (host only; sets POLYONTEST_TAG)
        #[arg(long)]
        tag: Option<String>,
        /// Filter by suite name (host only; sets POLYONTEST_SUITE)
        #[arg(long)]
        suite: Option<String>,
        /// Filter by group name; requires --suite (host only; sets POLYONTEST_GROUP)
        #[arg(long)]
        group: Option<String>,
        /// Run each test case in a separate child process (host target only)
        #[arg(long)]
        isolate: bool,
        /// Crash behavior: continue | abort
        #[arg(long)]
        on_crash: Option<String>,
    },
    /// List built-in plugins
    Plugins,
}

#[derive(Debug, Deserialize)]
struct Config {
    #[serde(default)]
    target: std::collections::HashMap<String, TargetConfig>,
}

#[derive(Debug, Deserialize, Clone)]
struct TargetConfig {
    #[serde(default = "default_board")]
    board: String,
    #[serde(default = "default_transport")]
    transport: String,
    #[serde(default = "default_codec")]
    codec: String,
    #[serde(default = "default_mode")]
    mode: String,
    #[serde(default = "default_reporters")]
    reporters: Vec<String>,
    /// Host binary path or QEMU ELF
    binary: Option<PathBuf>,
    build: Option<String>,
    #[serde(default = "default_timeout_ms")]
    timeout_ms: u64,
    /// Optional host filters (CLI flags override these).
    tag: Option<String>,
    suite: Option<String>,
    group: Option<String>,
    #[serde(default)]
    isolate: bool,
    #[serde(default = "default_on_crash")]
    on_crash: String,
}

fn default_board() -> String {
    "host".into()
}
fn default_transport() -> String {
    "stdio".into()
}
fn default_codec() -> String {
    "cobs".into()
}
fn default_mode() -> String {
    "stream".into()
}
fn default_reporters() -> Vec<String> {
    vec!["console".into(), "junit".into()]
}
fn default_timeout_ms() -> u64 {
    30_000
}
fn default_on_crash() -> String {
    "continue".into()
}

fn main() -> Result<()> {
    let cli = Cli::parse();
    match cli.cmd {
        Commands::Plugins => {
            println!("transports: stdio, uart (via qemu/child stdio)");
            println!("codecs:     cobs, text");
            println!("boards:     host, qemu_m33");
            println!("reporters:  console, junit, json");
            println!("extensions: core_stream (default)");
            Ok(())
        }
        Commands::Run {
            target,
            config,
            tag,
            suite,
            group,
            isolate,
            on_crash,
        } => run_target(&config, &target, tag, suite, group, isolate, on_crash),
    }
}

fn merge_filter(
    t: &TargetConfig,
    tag: Option<String>,
    suite: Option<String>,
    group: Option<String>,
) -> HostFilter {
    HostFilter {
        tag: tag.or_else(|| t.tag.clone()),
        suite: suite.or_else(|| t.suite.clone()),
        group: group.or_else(|| t.group.clone()),
    }
}

fn filter_active(f: &HostFilter) -> bool {
    f.tag.as_ref().is_some_and(|s| !s.is_empty())
        || f.suite.as_ref().is_some_and(|s| !s.is_empty())
        || f.group.as_ref().is_some_and(|s| !s.is_empty())
}

fn run_target(
    config_path: &Path,
    target_name: &str,
    tag: Option<String>,
    suite: Option<String>,
    group: Option<String>,
    cli_isolate: bool,
    cli_on_crash: Option<String>,
) -> Result<()> {
    let raw = std::fs::read_to_string(config_path)
        .with_context(|| format!("reading {}", config_path.display()))?;
    let cfg: Config = toml::from_str(&raw)?;
    let t = cfg
        .target
        .get(target_name)
        .with_context(|| format!("unknown target '{target_name}' in {}", config_path.display()))?
        .clone();

    if t.mode != "stream" {
        bail!(
            "mode '{}' not supported in v0.1 (only stream). Command mode arrives in v2.",
            t.mode
        );
    }

    if t.transport != "stdio" && t.board == "host" {
        bail!("board.host currently wires child stdio (set transport = \"stdio\")");
    }
    if t.transport != "uart" && t.board == "qemu_m33" {
        bail!("board.qemu_m33 expects transport = \"uart\"");
    }

    let filter = merge_filter(&t, tag, suite, group);
    if filter.group.as_ref().is_some_and(|g| !g.is_empty())
        && !filter.suite.as_ref().is_some_and(|s| !s.is_empty())
    {
        bail!("--group / toml group requires --suite / toml suite");
    }

    let binary = t.binary.clone().unwrap_or_else(|| PathBuf::from("./test_bin"));

    match t.board.as_str() {
        "host" => {
            let isolate = cli_isolate || t.isolate;
            let on_crash = cli_on_crash.unwrap_or(t.on_crash.clone());
            if on_crash != "continue" && on_crash != "abort" {
                bail!("invalid on-crash policy '{}': must be 'continue' or 'abort'", on_crash);
            }

            if isolate {
                run_isolated(&binary, &t, &filter, &on_crash)?;
            } else {
                let mut board = HostBoard::new(binary);
                board.build_cmd = t.build.clone();
                board.filter = filter;
                board.prepare()?;
                let mut transport = board.spawn_transport()?;
                drain_stream(&mut transport, &t)?;
                let status = transport.wait()?;
                if !status.success() && status.code() != Some(1) {
                    // exit 1 from harness means test failures — still parse stream
                }
            }
        }
        "qemu_m33" => {
            if filter_active(&filter) {
                bail!(
                    "tag/suite/group filters are host-only (freestanding QEMU has no getenv). \
                     Filter in the DUT main, or run on board.host."
                );
            }
            let mut board = QemuM33Board::new(binary);
            if let Some(cmd) = &t.build {
                let status = std::process::Command::new("sh").arg("-c").arg(cmd).status()?;
                if !status.success() {
                    bail!("qemu_m33 build failed: {cmd}");
                }
            }
            board.prepare()?;
            let mut transport = board.spawn_transport()?;
            drain_stream(&mut transport, &t)?;
            let _ = transport.wait();
        }
        other => bail!("unknown board plugin '{other}'"),
    }

    Ok(())
}

fn drain_stream(transport: &mut dyn Transport, t: &TargetConfig) -> Result<()> {
    let mut codec: Box<dyn Codec> = match t.codec.as_str() {
        "cobs" => Box::new(CobsCodec::new()),
        "text" => Box::new(TextCodec::new()),
        other => bail!("unknown codec '{other}'"),
    };

    let mut reporters: Vec<Box<dyn Reporter>> = Vec::new();
    for r in &t.reporters {
        match r.as_str() {
            "console" => reporters.push(Box::new(ConsoleReporter::new())),
            "junit" => reporters.push(Box::new(JunitReporter::new(PathBuf::from("report.xml")))),
            "json" => reporters.push(Box::new(JsonReporter::new(PathBuf::from("report.json")))),
            other => bail!("unknown reporter '{other}'"),
        }
    }

    transport.open()?;
    transport.set_timeout(Some(Duration::from_millis(t.timeout_ms)))?;

    let deadline = std::time::Instant::now() + Duration::from_millis(t.timeout_ms);
    let mut buf = [0u8; 1024];
    let mut failed = 0u32;

    loop {
        if std::time::Instant::now() > deadline {
            bail!("timeout waiting for DONE event");
        }
        match transport.read(&mut buf) {
            Ok(0) => break,
            Ok(n) => {
                let events = codec.decode_feed(&buf[..n])?;
                for ev in events {
                    if let Event::Done { failed: f, .. } = &ev {
                        failed = *f;
                    }
                    for r in reporters.iter_mut() {
                        r.on_event(&ev)?;
                    }
                    if ev.is_terminal() {
                        for r in reporters.iter_mut() {
                            r.finish()?;
                        }
                        if failed > 0 {
                            std::process::exit(1);
                        }
                        return Ok(());
                    }
                }
            }
            Err(e) => {
                // Treat broken pipe / EOF as end
                if e.to_string().contains("os error") {
                    break;
                }
                return Err(e.into());
            }
        }
    }

    for r in reporters.iter_mut() {
        r.finish()?;
    }
    if failed > 0 {
        std::process::exit(1);
    }
    Ok(())
}

#[derive(Debug, Clone)]
struct TestCase {
    suite: String,
    group: String,
    name: String,
}

fn parse_discovery_output(stdout: &str) -> Result<Vec<TestCase>> {
    let mut cases = Vec::new();
    for line in stdout.lines() {
        if let Some(rest) = line.strip_prefix("list-case:") {
            let parts: Vec<&str> = rest.trim().splitn(3, '.').collect();
            if parts.len() != 3 {
                anyhow::bail!("invalid list-case format: {}", line);
            }
            cases.push(TestCase {
                suite: parts[0].to_string(),
                group: parts[1].to_string(),
                name: parts[2].to_string(),
            });
        }
    }
    Ok(cases)
}

fn discover_cases(
    binary: &Path,
    build_cmd: &Option<String>,
    filter: &HostFilter,
) -> Result<Vec<TestCase>> {
    if let Some(cmd) = build_cmd {
        let status = std::process::Command::new("sh")
            .arg("-c")
            .arg(cmd)
            .status()
            .with_context(|| format!("failed to execute build command: {cmd}"))?;
        if !status.success() {
            anyhow::bail!("build failed: {cmd} ({status})");
        }
    }
    let mut cmd = std::process::Command::new(binary);
    cmd.env("POLY_DISCOVER", "1");
    if let Some(tag) = &filter.tag {
        cmd.env("POLYONTEST_TAG", tag);
    }
    if let Some(suite) = &filter.suite {
        cmd.env("POLY_SUITE", suite);
    }
    if let Some(group) = &filter.group {
        cmd.env("POLY_GROUP", group);
    }
    let output = cmd.output().with_context(|| format!("failed to run {}", binary.display()))?;
    if !output.status.success() {
        anyhow::bail!("binary execution failed during discovery: {:?}", output.status);
    }
    let stdout = String::from_utf8(output.stdout)?;
    parse_discovery_output(&stdout)
}

fn run_isolated(
    binary: &Path,
    t: &TargetConfig,
    filter: &HostFilter,
    on_crash: &str,
) -> Result<()> {
    let all_cases = discover_cases(binary, &t.build, filter)?;
    if all_cases.is_empty() {
        anyhow::bail!("no test cases discovered for isolated run");
    }

    let mut reporters: Vec<Box<dyn Reporter>> = Vec::new();
    for r in &t.reporters {
        match r.as_str() {
            "console" => reporters.push(Box::new(ConsoleReporter::new())),
            "junit" => reporters.push(Box::new(JunitReporter::new(PathBuf::from("report.xml")))),
            "json" => reporters.push(Box::new(JsonReporter::new(PathBuf::from("report.json")))),
            other => anyhow::bail!("unknown reporter '{other}'"),
        }
    }

    let mut seen_suites = std::collections::HashSet::new();
    let mut passed_count: u32 = 0;
    let mut failed_count: u32 = 0;
    let mut skipped_count: u32 = 0;
    let mut suite_failed = false;

    for c in &all_cases {


        let mut cmd = std::process::Command::new(binary);
        cmd.env("POLY_SUITE", &c.suite);
        cmd.env("POLY_GROUP", &c.group);
        cmd.env("POLY_CASE", &c.name);
        cmd.stdin(std::process::Stdio::null())
           .stdout(std::process::Stdio::piped())
           .stderr(std::process::Stdio::inherit());

        let mut child = cmd.spawn().with_context(|| format!("failed to spawn case {}", c.name))?;
        let mut stdout_pipe = child.stdout.take().unwrap();

        let (tx, rx) = std::sync::mpsc::channel();
        let codec_type = t.codec.clone();

        std::thread::spawn(move || {
            let mut codec: Box<dyn Codec> = match codec_type.as_str() {
                "cobs" => Box::new(CobsCodec::new()),
                "text" => Box::new(TextCodec::new()),
                other => {
                    let _ = tx.send(Err(anyhow::anyhow!("unknown codec '{other}'")));
                    return;
                }
            };
            let mut buf = [0u8; 1024];
            loop {
                match stdout_pipe.read(&mut buf) {
                    Ok(0) => break,
                    Ok(n) => {
                        match codec.decode_feed(&buf[..n]) {
                            Ok(events) => {
                                for ev in events {
                                    if tx.send(Ok(ev)).is_err() {
                                        return;
                                    }
                                }
                            }
                            Err(e) => {
                                let _ = tx.send(Err(e.into()));
                                return;
                            }
                        }
                    }
                    Err(e) => {
                        if e.to_string().contains("os error") {
                            break;
                        }
                        let _ = tx.send(Err(e.into()));
                        return;
                    }
                }
            }
        });

        let mut ran_case = false;
        let mut received_case_end = false;
        let mut parsed_status = TestStatus::Passed;
        let deadline = std::time::Instant::now() + Duration::from_millis(t.timeout_ms);
        let mut child_res = Ok(());

        loop {
            let now = std::time::Instant::now();
            if now >= deadline {
                child_res = Err(anyhow::anyhow!("timeout waiting for child process events"));
                break;
            }
            let timeout = deadline - now;
            match rx.recv_timeout(timeout) {
                Ok(Ok(ev)) => {
                    match &ev {
                        Event::SuiteStart { name } => {
                            if seen_suites.insert(name.clone()) {
                                for r in reporters.iter_mut() {
                                    r.on_event(&ev)?;
                                }
                            }
                        }
                        Event::CaseStart { .. } => {
                            ran_case = true;
                            for r in reporters.iter_mut() {
                                r.on_event(&ev)?;
                            }
                        }
                        Event::AssertFail { .. } => {
                            for r in reporters.iter_mut() {
                                r.on_event(&ev)?;
                            }
                        }
                        Event::CaseEnd { status, .. } => {
                            received_case_end = true;
                            parsed_status = status.clone();
                            match status {
                                TestStatus::Passed => passed_count += 1,
                                TestStatus::Failed => {
                                    failed_count += 1;
                                    suite_failed = true;
                                }
                                TestStatus::Skipped => skipped_count += 1,
                            }
                            for r in reporters.iter_mut() {
                                r.on_event(&ev)?;
                            }
                        }
                        _ => {}
                    }
                }
                Ok(Err(e)) => {
                    child_res = Err(e);
                    break;
                }
                Err(std::sync::mpsc::RecvTimeoutError::Timeout) => {
                    child_res = Err(anyhow::anyhow!("timeout waiting for child process events"));
                    break;
                }
                Err(std::sync::mpsc::RecvTimeoutError::Disconnected) => {
                    break; // EOF
                }
            }
        }

        if child_res.is_err() {
            let _ = child.kill();
        }

        let status = child.wait()?;
        let exit_ok = status.success() || status.code() == Some(1);

        if child_res.is_err() || !exit_ok || !ran_case {
            let err_msg = if let Err(e) = &child_res {
                format!("Error reading child stream: {}", e)
            } else if !exit_ok {
                format!("Process exited with status {:?}", status)
            } else {
                "Process exited without running test case".to_string()
            };

            let case_leaf = format!("{}.{}", c.group, c.name);
            let assert_fail = Event::AssertFail {
                suite: c.suite.clone(),
                name: case_leaf.clone(),
                message: err_msg,
                file: "".to_string(),
                line: 0,
            };

            if !ran_case {
                let case_start = Event::CaseStart {
                    suite: c.suite.clone(),
                    name: format!("{}.{}", c.group, c.name),
                };
                for r in reporters.iter_mut() {
                    r.on_event(&case_start)?;
                }
            }

            for r in reporters.iter_mut() {
                r.on_event(&assert_fail)?;
            }

            if !received_case_end {
                let case_end = Event::CaseEnd {
                    suite: c.suite.clone(),
                    name: case_leaf,
                    status: TestStatus::Failed,
                };
                for r in reporters.iter_mut() {
                    r.on_event(&case_end)?;
                }
                failed_count += 1;
                suite_failed = true;
            } else {
                // If it already received a CaseEnd but then crashed (e.g. teardown crash),
                // we correct the counters if it wasn't already failed.
                if parsed_status != TestStatus::Failed {
                    match parsed_status {
                        TestStatus::Passed => passed_count = passed_count.saturating_sub(1),
                        TestStatus::Skipped => skipped_count = skipped_count.saturating_sub(1),
                        _ => {}
                    }
                    failed_count += 1;
                    suite_failed = true;
                }
            }

            if on_crash == "abort" {
                break;
            }
        }
    }

    let done_event = Event::Done {
        passed: passed_count,
        failed: failed_count,
        skipped: skipped_count,
    };
    for r in reporters.iter_mut() {
        r.on_event(&done_event)?;
        r.finish()?;
    }

    if suite_failed {
        std::process::exit(1);
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use clap::Parser;

    #[test]
    fn test_toml_deserialization_isolation() {
        let toml_str = r#"
            [target.host]
            board = "host"
            isolate = true
            on_crash = "abort"
        "#;
        let config: Config = toml::from_str(toml_str).unwrap();
        let target = config.target.get("host").unwrap();
        assert!(target.isolate, "expected isolate to be true");
        assert_eq!(target.on_crash, "abort", "expected on_crash to be abort");
    }

    #[test]
    fn test_clap_parsing_isolation() {
        let args = vec!["polyontest", "run", "--isolate", "--on-crash", "abort"];
        let cli = Cli::try_parse_from(args).unwrap();
        match cli.cmd {
            Commands::Run { isolate, on_crash, .. } => {
                assert!(isolate, "expected CLI isolate flag to be true");
                assert_eq!(on_crash, Some("abort".to_string()), "expected CLI on-crash to be abort");
            }
            _ => panic!("expected Commands::Run"),
        }
    }

    #[test]
    fn test_parse_discovery_output() {
        let stdout = "=== PolyOnTest ===\nlist-case:Math.Basic.AddPositive\nlist-case:Expect.Pointers.NotNull\n";
        let cases = parse_discovery_output(stdout).unwrap();
        assert_eq!(cases.len(), 2);
        assert_eq!(cases[0].suite, "Math");
        assert_eq!(cases[0].group, "Basic");
        assert_eq!(cases[0].name, "AddPositive");
        assert_eq!(cases[1].suite, "Expect");
        assert_eq!(cases[1].group, "Pointers");
        assert_eq!(cases[1].name, "NotNull");
    }

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

    fn build_harness_binaries() {
        use std::process::Command;
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
    }

    #[test]
    fn test_discover_cases_from_binary() {
        build_harness_binaries();
        let binary_path = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
            .join("../../build/host_c/host_c_tests");
        assert!(binary_path.exists(), "binary does not exist: {:?}", binary_path);
        let filter = HostFilter::default();
        let cases = discover_cases(&binary_path, &None, &filter).unwrap();
        assert!(!cases.is_empty(), "expected discovered cases to not be empty");
        // Check that Math.Basic.AddPositive is in the discovered list
        assert!(
            cases.iter().any(|c| c.suite == "Math" && c.group == "Basic" && c.name == "AddPositive"),
            "expected Math.Basic.AddPositive to be discovered"
        );
    }
}

