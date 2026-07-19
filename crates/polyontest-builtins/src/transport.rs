use std::io::{self, Read, Write};
use std::process::{Child, ChildStdin};
use std::time::Duration;

use polyontest_plugin_api::{Result, Transport};

/// Host-side stdio of the current process (rarely used alone).
pub struct StdioTransport;

impl Transport for StdioTransport {
    fn id(&self) -> &'static str {
        "stdio"
    }

    fn open(&mut self) -> Result<()> {
        Ok(())
    }

    fn read(&mut self, buf: &mut [u8]) -> Result<usize> {
        Ok(io::stdin().read(buf)?)
    }

    fn write_all(&mut self, data: &[u8]) -> Result<()> {
        io::stdout().write_all(data)?;
        io::stdout().flush()?;
        Ok(())
    }

    fn close(&mut self) -> Result<()> {
        Ok(())
    }

    fn set_timeout(&mut self, _timeout: Option<Duration>) -> Result<()> {
        Ok(())
    }
}

/// Reads/writes a child process pipe — used by `board.host` / `board.qemu_m33`.
pub struct ChildStdioTransport {
    child: Child,
    stdin: Option<ChildStdin>,
    reader: Box<dyn Read + Send>,
}

impl ChildStdioTransport {
    pub fn from_child(mut child: Child) -> Result<Self> {
        let stdin = child.stdin.take();
        let stdout = child
            .stdout
            .take()
            .ok_or_else(|| polyontest_plugin_api::PluginError::Board("missing stdout".into()))?;
        Ok(Self {
            child,
            stdin,
            reader: Box::new(stdout),
        })
    }

    /// QEMU semihosting typically emits on stderr.
    pub fn from_child_stderr(mut child: Child) -> Result<Self> {
        let stdin = child.stdin.take();
        let stderr = child
            .stderr
            .take()
            .ok_or_else(|| polyontest_plugin_api::PluginError::Board("missing stderr".into()))?;
        Ok(Self {
            child,
            stdin,
            reader: Box::new(stderr),
        })
    }

    pub fn wait(mut self) -> Result<std::process::ExitStatus> {
        Ok(self.child.wait()?)
    }

    pub fn kill(&mut self) -> Result<()> {
        Ok(self.child.kill()?)
    }
}

impl Transport for ChildStdioTransport {
    fn id(&self) -> &'static str {
        "stdio"
    }

    fn open(&mut self) -> Result<()> {
        Ok(())
    }

    fn read(&mut self, buf: &mut [u8]) -> Result<usize> {
        Ok(self.reader.read(buf)?)
    }

    fn write_all(&mut self, data: &[u8]) -> Result<()> {
        if let Some(ref mut stdin) = self.stdin {
            stdin.write_all(data)?;
            stdin.flush()?;
        }
        Ok(())
    }

    fn close(&mut self) -> Result<()> {
        Ok(())
    }
}
