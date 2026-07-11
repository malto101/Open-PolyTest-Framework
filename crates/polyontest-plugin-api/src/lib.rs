//! Plugin traits for PolyOnTest — Interface Segregation + Dependency Inversion.
//!
//! Core/CLI depend on these abstractions. Concrete plugins live in
//! `polyontest-builtins` (and later dynamic plugins).

use std::io;
use std::path::PathBuf;
use std::time::Duration;

use polyontest_protocol::Event;
use thiserror::Error;

#[derive(Debug, Error)]
pub enum PluginError {
    #[error("io error: {0}")]
    Io(#[from] io::Error),
    #[error("codec error: {0}")]
    Codec(String),
    #[error("board error: {0}")]
    Board(String),
    #[error("unsupported: {0}")]
    Unsupported(String),
    #[error("{0}")]
    Other(String),
}

pub type Result<T> = std::result::Result<T, PluginError>;

/// Byte pipe — no framing knowledge (SOLID: S + I).
pub trait Transport: Send {
    fn id(&self) -> &'static str;
    fn open(&mut self) -> Result<()>;
    fn read(&mut self, buf: &mut [u8]) -> Result<usize>;
    fn write_all(&mut self, data: &[u8]) -> Result<()>;
    fn close(&mut self) -> Result<()>;
    fn set_timeout(&mut self, _timeout: Option<Duration>) -> Result<()> {
        Ok(())
    }
}

/// Framing / serialization — independent of Transport.
pub trait Codec: Send {
    fn id(&self) -> &'static str;
    fn encode(&self, event: &Event) -> Result<Vec<u8>>;
    /// Feed bytes; return zero or more decoded events (may buffer internally).
    fn decode_feed(&mut self, data: &[u8]) -> Result<Vec<Event>>;
}

/// Board bring-up: prepare, flash/reset, expose how to attach a transport.
pub trait Board: Send {
    fn id(&self) -> &'static str;
    fn prepare(&mut self) -> Result<()>;
    fn flash(&mut self) -> Result<()> {
        Ok(())
    }
    fn reset(&mut self) -> Result<()> {
        Ok(())
    }
    /// Path to the artifact to run (host binary, ELF, UF2, …).
    fn artifact(&self) -> Option<PathBuf> {
        None
    }
}

/// Consumes domain events and writes reports.
pub trait Reporter: Send {
    fn id(&self) -> &'static str;
    fn on_event(&mut self, event: &Event) -> Result<()>;
    fn finish(&mut self) -> Result<()>;
}

/// Optional capability bundle (stream runner, command mode, HIL, …).
pub trait ExtensionPack: Send {
    fn id(&self) -> &'static str;
}

/// Registry entry metadata for discovery.
#[derive(Debug, Clone, Copy)]
pub struct PluginMeta {
    pub kind: &'static str,
    pub id: &'static str,
}
