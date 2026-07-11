use polyontest_plugin_api::{Codec, PluginError, Result};
use polyontest_protocol::{Event, MsgType, TestStatus, PTWP_MAGIC, PTWP_VERSION};

/// Human-readable line codec (`POLYONTEST_MINIMAL_PRINT` compatible).
pub struct TextCodec {
    buf: String,
}

impl TextCodec {
    pub fn new() -> Self {
        Self {
            buf: String::new(),
        }
    }
}

impl Default for TextCodec {
    fn default() -> Self {
        Self::new()
    }
}

impl Codec for TextCodec {
    fn id(&self) -> &'static str {
        "text"
    }

    fn encode(&self, event: &Event) -> Result<Vec<u8>> {
        let line = match event {
            Event::SuiteStart { name } => format!("SUITE_START {name}\n"),
            Event::CaseStart { suite, name } => format!("CASE_START {suite}.{name}\n"),
            Event::AssertFail {
                suite,
                name,
                file,
                line,
                message,
            } => format!("FAIL {suite}.{name} {file}:{line} {message}\n"),
            Event::CaseEnd {
                suite,
                name,
                status,
            } => {
                let s = match status {
                    TestStatus::Passed => "PASS",
                    TestStatus::Failed => "FAIL",
                    TestStatus::Skipped => "SKIP",
                };
                format!("{s} {suite}.{name}\n")
            }
            Event::SuiteEnd {
                name,
                passed,
                failed,
                skipped,
            } => format!(
                "SUITE_END {name} passed={passed} failed={failed} skipped={skipped}\n"
            ),
            Event::Log { message } => format!("LOG {message}\n"),
            Event::Done {
                passed,
                failed,
                skipped,
            } => format!("DONE passed={passed} failed={failed} skipped={skipped}\n"),
        };
        Ok(line.into_bytes())
    }

    fn decode_feed(&mut self, data: &[u8]) -> Result<Vec<Event>> {
        self.buf.push_str(&String::from_utf8_lossy(data));
        let mut out = Vec::new();
        while let Some(pos) = self.buf.find('\n') {
            let line = self.buf[..pos].trim_end_matches('\r').to_string();
            self.buf = self.buf[pos + 1..].to_string();
            if let Some(ev) = parse_text_line(&line) {
                out.push(ev);
            }
        }
        Ok(out)
    }
}

fn parse_text_line(line: &str) -> Option<Event> {
    if line.is_empty() || line.starts_with("===") {
        return None;
    }
    if let Some(rest) = line.strip_prefix("SUITE_START ") {
        return Some(Event::SuiteStart {
            name: rest.to_string(),
        });
    }
    if let Some(rest) = line.strip_prefix("CASE_START ") {
        let (suite, name) = split_suite_case(rest)?;
        return Some(Event::CaseStart { suite, name });
    }
    if let Some(rest) = line.strip_prefix("SUITE_END ") {
        // SUITE_END name passed=N failed=N skipped=N
        let parts: Vec<&str> = rest.split_whitespace().collect();
        if parts.is_empty() {
            return None;
        }
        let name = parts[0].to_string();
        let mut passed = 0;
        let mut failed = 0;
        let mut skipped = 0;
        for p in &parts[1..] {
            if let Some(v) = p.strip_prefix("passed=") {
                passed = v.parse().unwrap_or(0);
            } else if let Some(v) = p.strip_prefix("failed=") {
                failed = v.parse().unwrap_or(0);
            } else if let Some(v) = p.strip_prefix("skipped=") {
                skipped = v.parse().unwrap_or(0);
            }
        }
        return Some(Event::SuiteEnd {
            name,
            passed,
            failed,
            skipped,
        });
    }
    if let Some(rest) = line.strip_prefix("DONE ") {
        let mut passed = 0;
        let mut failed = 0;
        let mut skipped = 0;
        for p in rest.split_whitespace() {
            if let Some(v) = p.strip_prefix("passed=") {
                passed = v.parse().unwrap_or(0);
            } else if let Some(v) = p.strip_prefix("failed=") {
                failed = v.parse().unwrap_or(0);
            } else if let Some(v) = p.strip_prefix("skipped=") {
                skipped = v.parse().unwrap_or(0);
            }
        }
        return Some(Event::Done {
            passed,
            failed,
            skipped,
        });
    }
    if let Some(rest) = line.strip_prefix("PASS ") {
        let (suite, name) = split_suite_case(rest)?;
        return Some(Event::CaseEnd {
            suite,
            name,
            status: TestStatus::Passed,
        });
    }
    if let Some(rest) = line.strip_prefix("SKIP ") {
        let (suite, name) = split_suite_case(rest)?;
        return Some(Event::CaseEnd {
            suite,
            name,
            status: TestStatus::Skipped,
        });
    }
    if let Some(rest) = line.strip_prefix("FAIL ") {
        // Either "FAIL suite.name" or "FAIL suite.name file:line msg"
        let tokens: Vec<&str> = rest.splitn(3, ' ').collect();
        if tokens.is_empty() {
            return None;
        }
        let (suite, name) = split_suite_case(tokens[0])?;
        if tokens.len() == 1 {
            return Some(Event::CaseEnd {
                suite,
                name,
                status: TestStatus::Failed,
            });
        }
        return Some(Event::AssertFail {
            suite,
            name,
            file: tokens.get(1).unwrap_or(&"").to_string(),
            line: 0,
            message: tokens.get(2).unwrap_or(&"").to_string(),
        });
    }
    None
}

fn split_suite_case(s: &str) -> Option<(String, String)> {
    let (a, b) = s.split_once('.')?;
    Some((a.to_string(), b.to_string()))
}

/// COBS + PTWP binary codec matching the C harness encoder.
pub struct CobsCodec {
    buf: Vec<u8>,
}

impl CobsCodec {
    pub fn new() -> Self {
        Self { buf: Vec::new() }
    }
}

impl Default for CobsCodec {
    fn default() -> Self {
        Self::new()
    }
}

impl Codec for CobsCodec {
    fn id(&self) -> &'static str {
        "cobs"
    }

    fn encode(&self, event: &Event) -> Result<Vec<u8>> {
        let payload = encode_payload(event)?;
        Ok(cobs_encode(&payload))
    }

    fn decode_feed(&mut self, data: &[u8]) -> Result<Vec<Event>> {
        self.buf.extend_from_slice(data);
        let mut out = Vec::new();
        while let Some(pos) = self.buf.iter().position(|&b| b == 0) {
            let frame = self.buf[..pos].to_vec();
            self.buf.drain(..=pos);
            if frame.is_empty() {
                continue;
            }
            let decoded = cobs_decode(&frame)
                .map_err(|e| PluginError::Codec(e))?;
            if let Some(ev) = decode_payload(&decoded)? {
                out.push(ev);
            }
        }
        Ok(out)
    }
}

fn encode_payload(event: &Event) -> Result<Vec<u8>> {
    let mut buf = Vec::new();
    buf.extend_from_slice(PTWP_MAGIC);
    buf.push(PTWP_VERSION);
    match event {
        Event::SuiteStart { name } => {
            buf.push(MsgType::SuiteStart as u8);
            push_str(&mut buf, name);
            push_str(&mut buf, "");
            push_str(&mut buf, "");
            push_u32s(&mut buf, 0, 0, 0);
        }
        Event::CaseStart { suite, name } => {
            buf.push(MsgType::CaseStart as u8);
            push_str(&mut buf, suite);
            push_str(&mut buf, name);
            push_str(&mut buf, "");
            push_u32s(&mut buf, 0, 0, 0);
        }
        Event::AssertFail {
            suite,
            name,
            message,
            line,
            ..
        } => {
            buf.push(MsgType::AssertFail as u8);
            push_str(&mut buf, suite);
            push_str(&mut buf, name);
            push_str(&mut buf, message);
            push_u32s(&mut buf, *line, 0, 0);
        }
        Event::CaseEnd {
            suite,
            name,
            status,
        } => {
            buf.push(MsgType::CaseEnd as u8);
            push_str(&mut buf, suite);
            push_str(&mut buf, name);
            push_str(&mut buf, "");
            let st = match status {
                TestStatus::Passed => 0u32,
                TestStatus::Failed => 1,
                TestStatus::Skipped => 2,
            };
            push_u32s(&mut buf, st, 0, 0);
        }
        Event::SuiteEnd {
            name,
            passed,
            failed,
            skipped,
        } => {
            buf.push(MsgType::SuiteEnd as u8);
            push_str(&mut buf, name);
            push_str(&mut buf, "");
            push_str(&mut buf, "");
            push_u32s(&mut buf, *passed, *failed, *skipped);
        }
        Event::Log { message } => {
            buf.push(MsgType::Log as u8);
            push_str(&mut buf, message);
            push_str(&mut buf, "");
            push_str(&mut buf, "");
            push_u32s(&mut buf, 0, 0, 0);
        }
        Event::Done {
            passed,
            failed,
            skipped,
        } => {
            buf.push(MsgType::Done as u8);
            push_str(&mut buf, "");
            push_str(&mut buf, "");
            push_str(&mut buf, "");
            push_u32s(&mut buf, *passed, *failed, *skipped);
        }
    }
    Ok(buf)
}

fn decode_payload(data: &[u8]) -> Result<Option<Event>> {
    if data.len() < 4 || &data[0..2] != PTWP_MAGIC || data[2] != PTWP_VERSION {
        return Ok(None);
    }
    let msg = MsgType::from_u8(data[3])
        .ok_or_else(|| PluginError::Codec(format!("unknown msg type {}", data[3])))?;
    let mut at = 4usize;
    let a = read_str(data, &mut at)?;
    let b = read_str(data, &mut at)?;
    let c = read_str(data, &mut at)?;
    let u0 = read_u32(data, &mut at)?;
    let u1 = read_u32(data, &mut at)?;
    let u2 = read_u32(data, &mut at)?;

    let ev = match msg {
        MsgType::SuiteStart => Event::SuiteStart { name: a },
        MsgType::CaseStart => Event::CaseStart {
            suite: a,
            name: b,
        },
        MsgType::AssertFail => Event::AssertFail {
            suite: a,
            name: b,
            file: String::new(),
            line: u0,
            message: c,
        },
        MsgType::CaseEnd => Event::CaseEnd {
            suite: a,
            name: b,
            status: match u0 {
                0 => TestStatus::Passed,
                2 => TestStatus::Skipped,
                _ => TestStatus::Failed,
            },
        },
        MsgType::SuiteEnd => Event::SuiteEnd {
            name: a,
            passed: u0,
            failed: u1,
            skipped: u2,
        },
        MsgType::Log => Event::Log { message: a },
        MsgType::Done => Event::Done {
            passed: u0,
            failed: u1,
            skipped: u2,
        },
    };
    Ok(Some(ev))
}

fn push_str(buf: &mut Vec<u8>, s: &str) {
    let bytes = s.as_bytes();
    let n = bytes.len().min(255);
    buf.push(n as u8);
    buf.extend_from_slice(&bytes[..n]);
}

fn push_u32s(buf: &mut Vec<u8>, a: u32, b: u32, c: u32) {
    for v in [a, b, c] {
        buf.extend_from_slice(&v.to_le_bytes());
    }
}

fn read_str(data: &[u8], at: &mut usize) -> Result<String> {
    if *at >= data.len() {
        return Err(PluginError::Codec("truncated".into()));
    }
    let n = data[*at] as usize;
    *at += 1;
    if *at + n > data.len() {
        return Err(PluginError::Codec("truncated str".into()));
    }
    let s = String::from_utf8_lossy(&data[*at..*at + n]).into_owned();
    *at += n;
    Ok(s)
}

fn read_u32(data: &[u8], at: &mut usize) -> Result<u32> {
    if *at + 4 > data.len() {
        return Err(PluginError::Codec("truncated u32".into()));
    }
    let v = u32::from_le_bytes(data[*at..*at + 4].try_into().unwrap());
    *at += 4;
    Ok(v)
}

fn cobs_encode(input: &[u8]) -> Vec<u8> {
    let mut out = Vec::with_capacity(input.len() + input.len() / 254 + 2);
    out.push(0); // placeholder
    let mut code_index = 0;
    let mut code: u8 = 1;
    for &b in input {
        if b == 0 {
            out[code_index] = code;
            code_index = out.len();
            out.push(0);
            code = 1;
        } else {
            out.push(b);
            code += 1;
            if code == 0xFF {
                out[code_index] = code;
                code_index = out.len();
                out.push(0);
                code = 1;
            }
        }
    }
    out[code_index] = code;
    out.push(0); // delimiter
    out
}

fn cobs_decode(input: &[u8]) -> std::result::Result<Vec<u8>, String> {
    let mut out = Vec::with_capacity(input.len());
    let mut i = 0;
    while i < input.len() {
        let code = input[i];
        if code == 0 {
            return Err("zero code".into());
        }
        i += 1;
        let end = i + (code as usize) - 1;
        if end > input.len() {
            return Err("overrun".into());
        }
        out.extend_from_slice(&input[i..end]);
        i = end;
        if code != 0xFF && i < input.len() {
            out.push(0);
        }
    }
    // Remove trailing zero that COBS adds between blocks when code != 0xFF —
    // standard decode already handles; trim last spurious 0 if present from loop.
    // Our encoder format matches Wikipedia COBS; final block should not add extra 0
    // when i == input.len() after last block. The push(0) only happens when i < len.
    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn cobs_roundtrip() {
        let payload = b"PT\x01\x07\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00";
        let enc = cobs_encode(payload);
        assert_eq!(*enc.last().unwrap(), 0);
        let body = &enc[..enc.len() - 1];
        let dec = cobs_decode(body).unwrap();
        assert_eq!(dec, payload);
    }
}
