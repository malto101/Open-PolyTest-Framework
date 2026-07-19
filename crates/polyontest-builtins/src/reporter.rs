use std::fs::File;
use std::io::Write;
use std::path::PathBuf;

use polyontest_plugin_api::{Reporter, Result};
use polyontest_protocol::{Event, TestStatus};
pub struct ConsoleReporter {
    pub passed: u32,
    pub failed: u32,
    pub skipped: u32,
}

impl ConsoleReporter {
    pub fn new() -> Self {
        Self {
            passed: 0,
            failed: 0,
            skipped: 0,
        }
    }
}

impl Default for ConsoleReporter {
    fn default() -> Self {
        Self::new()
    }
}

impl Reporter for ConsoleReporter {
    fn id(&self) -> &'static str {
        "console"
    }

    fn on_event(&mut self, event: &Event) -> Result<()> {
        match event {
            Event::SuiteStart { name } => println!("suite  {name}"),
            Event::CaseStart { suite, name } => print!("  run   {suite}.{name} ... "),
            Event::CaseEnd { status, .. } => {
                match status {
                    TestStatus::Passed => {
                        self.passed += 1;
                        println!("ok");
                    }
                    TestStatus::Failed => {
                        self.failed += 1;
                        println!("FAILED");
                    }
                    TestStatus::Skipped => {
                        self.skipped += 1;
                        println!("skipped");
                    }
                }
            }
            Event::AssertFail {
                suite,
                name,
                message,
                line,
                ..
            } => {
                println!();
                println!("    assert fail {suite}.{name}:{line}: {message}");
            }
            Event::Done {
                passed,
                failed,
                skipped,
            } => {
                // Prefer DONE counters when present
                self.passed = *passed;
                self.failed = *failed;
                self.skipped = *skipped;
            }
            _ => {}
        }
        Ok(())
    }

    fn finish(&mut self) -> Result<()> {
        println!(
            "\n{} passed, {} failed, {} skipped",
            self.passed, self.failed, self.skipped
        );
        Ok(())
    }
}

pub struct JsonReporter {
    path: PathBuf,
    events: Vec<Event>,
}

impl JsonReporter {
    pub fn new(path: PathBuf) -> Self {
        Self {
            path,
            events: Vec::new(),
        }
    }
}

impl Reporter for JsonReporter {
    fn id(&self) -> &'static str {
        "json"
    }

    fn on_event(&mut self, event: &Event) -> Result<()> {
        self.events.push(event.clone());
        Ok(())
    }

    fn finish(&mut self) -> Result<()> {
        let mut file = File::create(&self.path)?;
        serde_json::to_writer_pretty(&mut file, &self.events)
            .map_err(|e| polyontest_plugin_api::PluginError::Other(e.to_string()))?;
        file.write_all(b"\n")?;
        Ok(())
    }
}

pub struct JunitReporter {
    path: PathBuf,
    cases: Vec<(String, String, TestStatus, Option<String>)>,
    suite: String,
}

impl JunitReporter {
    pub fn new(path: PathBuf) -> Self {
        Self {
            path,
            cases: Vec::new(),
            suite: "polyontest".into(),
        }
    }
}

impl Reporter for JunitReporter {
    fn id(&self) -> &'static str {
        "junit"
    }

    fn on_event(&mut self, event: &Event) -> Result<()> {
        match event {
            Event::SuiteStart { name } => self.suite = name.clone(),
            Event::CaseEnd {
                suite,
                name,
                status,
            } => {
                if let Some(c) = self
                    .cases
                    .iter_mut()
                    .rev()
                    .find(|(s, n, _, _)| s == suite && n == name)
                {
                    c.2 = status.clone();
                } else {
                    self.cases
                        .push((suite.clone(), name.clone(), status.clone(), None));
                }
            }
            Event::AssertFail {
                suite,
                name,
                message,
                ..
            } => {
                if let Some(c) = self
                    .cases
                    .iter_mut()
                    .rev()
                    .find(|(s, n, _, _)| s == suite && n == name)
                {
                    c.3 = Some(message.clone());
                    c.2 = TestStatus::Failed;
                } else {
                    self.cases.push((
                        suite.clone(),
                        name.clone(),
                        TestStatus::Failed,
                        Some(message.clone()),
                    ));
                }
            }
            _ => {}
        }
        Ok(())
    }

    fn finish(&mut self) -> Result<()> {
        let failures = self
            .cases
            .iter()
            .filter(|(_, _, s, _)| *s == TestStatus::Failed)
            .count();
        let mut xml = String::new();
        xml.push_str(r#"<?xml version="1.0" encoding="UTF-8"?>"#);
        xml.push('\n');
        xml.push_str(&format!(
            r#"<testsuite name="{}" tests="{}" failures="{}">"#,
            xml_escape(&self.suite),
            self.cases.len(),
            failures
        ));
        xml.push('\n');
        for (suite, name, status, msg) in &self.cases {
            xml.push_str(&format!(
                r#"  <testcase classname="{}" name="{}""#,
                xml_escape(suite),
                xml_escape(name)
            ));
            if *status == TestStatus::Failed {
                xml.push_str(">\n");
                xml.push_str(&format!(
                    "    <failure message=\"{}\"/>\n",
                    xml_escape(msg.as_deref().unwrap_or("failed"))
                ));
                xml.push_str("  </testcase>\n");
            } else {
                xml.push_str("/>\n");
            }
        }
        xml.push_str("</testsuite>\n");
        let mut file = File::create(&self.path)?;
        file.write_all(xml.as_bytes())?;
        Ok(())
    }
}

fn xml_escape(s: &str) -> String {
    s.replace('&', "&amp;")
        .replace('<', "&lt;")
        .replace('>', "&gt;")
        .replace('"', "&quot;")
}
