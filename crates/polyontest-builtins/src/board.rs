use std::path::PathBuf;
use std::process::{Command, Stdio};

use polyontest_plugin_api::{Board, PluginError, Result};

use crate::transport::ChildStdioTransport;

/// Optional host-side filter env vars forwarded to the DUT.
#[derive(Debug, Clone, Default)]
pub struct HostFilter {
    pub tag: Option<String>,
    pub suite: Option<String>,
    pub group: Option<String>,
}

pub struct HostBoard {
    pub binary: PathBuf,
    pub build_cmd: Option<String>,
    pub filter: HostFilter,
}

impl HostBoard {
    pub fn new(binary: PathBuf) -> Self {
        Self {
            binary,
            build_cmd: None,
            filter: HostFilter::default(),
        }
    }

    pub fn spawn_transport(&self) -> Result<ChildStdioTransport> {
        if !self.binary.exists() {
            return Err(PluginError::Board(format!(
                "missing binary {}",
                self.binary.display()
            )));
        }
        let mut cmd = Command::new(&self.binary);
        cmd.stdin(Stdio::null())
            .stdout(Stdio::piped())
            .stderr(Stdio::inherit());
        if let Some(tag) = &self.filter.tag {
            cmd.env("POLYONTEST_TAG", tag);
        }
        if let Some(suite) = &self.filter.suite {
            cmd.env("POLYONTEST_SUITE", suite);
        }
        if let Some(group) = &self.filter.group {
            cmd.env("POLYONTEST_GROUP", group);
        }
        let child = cmd.spawn()?;
        ChildStdioTransport::from_child(child)
    }
}

impl Board for HostBoard {
    fn id(&self) -> &'static str {
        "host"
    }

    fn prepare(&mut self) -> Result<()> {
        if let Some(cmd) = &self.build_cmd {
            let status = Command::new("sh").arg("-c").arg(cmd).status()?;
            if !status.success() {
                return Err(PluginError::Board(format!(
                    "build failed: {cmd} ({status})"
                )));
            }
        }
        Ok(())
    }

    fn artifact(&self) -> Option<PathBuf> {
        Some(self.binary.clone())
    }
}

/// QEMU Cortex-M33 board stub — launches qemu-system-arm when configured.
pub struct QemuM33Board {
    pub elf: PathBuf,
    pub qemu_bin: String,
    pub machine: String,
}

impl QemuM33Board {
    pub fn new(elf: PathBuf) -> Self {
        Self {
            elf,
            qemu_bin: "qemu-system-arm".into(),
            machine: "mps2-an505".into(),
        }
    }

    pub fn spawn_transport(&self) -> Result<ChildStdioTransport> {
        if !self.elf.exists() {
            return Err(PluginError::Board(format!(
                "missing ELF {}",
                self.elf.display()
            )));
        }
        // Semihosting I/O on mps2-an505 appears on QEMU stderr (not UART0/stdout).
        let child = Command::new(&self.qemu_bin)
            .args([
                "-machine",
                &self.machine,
                "-nographic",
                "-monitor",
                "none",
                "-semihosting-config",
                "enable=on,target=native",
                "-kernel",
            ])
            .arg(&self.elf)
            .stdin(Stdio::null())
            .stdout(Stdio::null())
            .stderr(Stdio::piped())
            .spawn()
            .map_err(|e| {
                PluginError::Board(format!(
                    "failed to launch {}: {e} (is qemu-system-arm installed?)",
                    self.qemu_bin
                ))
            })?;
        ChildStdioTransport::from_child_stderr(child)
    }
}

impl Board for QemuM33Board {
    fn id(&self) -> &'static str {
        "qemu_m33"
    }

    fn prepare(&mut self) -> Result<()> {
        Ok(())
    }

    fn artifact(&self) -> Option<PathBuf> {
        Some(self.elf.clone())
    }
}
