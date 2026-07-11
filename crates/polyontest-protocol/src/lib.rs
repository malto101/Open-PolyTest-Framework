//! PTWP — PolyOnTest Wire Protocol event model.
//!
//! Events are codec-agnostic. Codecs (COBS, text, nanopb) serialize these.

use serde::{Deserialize, Serialize};

/// Wire protocol major version for structured frames.
pub const PTWP_VERSION: u8 = 1;

/// Magic bytes prefix for binary PTWP frames (before COBS).
pub const PTWP_MAGIC: &[u8; 2] = b"PT";

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum TestStatus {
    Passed,
    Failed,
    Skipped,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum Event {
    SuiteStart {
        name: String,
    },
    CaseStart {
        suite: String,
        name: String,
    },
    AssertFail {
        suite: String,
        name: String,
        file: String,
        line: u32,
        message: String,
    },
    CaseEnd {
        suite: String,
        name: String,
        status: TestStatus,
    },
    SuiteEnd {
        name: String,
        passed: u32,
        failed: u32,
        skipped: u32,
    },
    Log {
        message: String,
    },
    Done {
        passed: u32,
        failed: u32,
        skipped: u32,
    },
}

impl Event {
    pub fn is_terminal(&self) -> bool {
        matches!(self, Event::Done { .. })
    }
}

/// Compact binary message type tags used by the C harness COBS encoder.
#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MsgType {
    SuiteStart = 1,
    CaseStart = 2,
    AssertFail = 3,
    CaseEnd = 4,
    SuiteEnd = 5,
    Log = 6,
    Done = 7,
}

impl MsgType {
    pub fn from_u8(v: u8) -> Option<Self> {
        match v {
            1 => Some(Self::SuiteStart),
            2 => Some(Self::CaseStart),
            3 => Some(Self::AssertFail),
            4 => Some(Self::CaseEnd),
            5 => Some(Self::SuiteEnd),
            6 => Some(Self::Log),
            7 => Some(Self::Done),
            _ => None,
        }
    }
}
