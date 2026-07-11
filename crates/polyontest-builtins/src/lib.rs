//! Built-in plugins for PolyOnTest v1.

pub mod board;
pub mod codec;
pub mod reporter;
pub mod transport;

pub use board::{HostBoard, HostFilter, QemuM33Board};
pub use codec::{CobsCodec, TextCodec};
pub use reporter::{ConsoleReporter, JsonReporter, JunitReporter};
pub use transport::{ChildStdioTransport, StdioTransport};
