//! PolyOnTest CLI — composition root (Dependency Inversion).

use std::path::{Path, PathBuf};
use std::time::Duration;

use anyhow::{bail, Context, Result};
use clap::{Parser, Subcommand};
use polyontest_builtins::{
    CobsCodec, ConsoleReporter, HostBoard, HostFilter, JsonReporter, JunitReporter, QemuM33Board,
    TextCodec,
};
use polyontest_plugin_api::{Board, Codec, Reporter, Transport};
use polyontest_protocol::Event;
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
        } => run_target(&config, &target, tag, suite, group),
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
