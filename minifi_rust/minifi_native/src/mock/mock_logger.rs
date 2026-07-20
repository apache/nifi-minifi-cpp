// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

use crate::LogLevel::Trace;
use crate::api::LogLevel;
use crate::api::Logger;
use std::fmt;
use std::sync::Mutex;

#[derive(Debug)]
pub struct MockLogger {
    pub logs: Mutex<Vec<(LogLevel, String)>>,
    pub log_level: LogLevel,
}

impl Logger for MockLogger {
    fn log(&self, level: LogLevel, args: fmt::Arguments) {
        let message = fmt::format(args);
        let mut logs_guard = self.logs.lock().unwrap();
        logs_guard.push((level, message.to_string()));
    }

    fn should_log(&self, level: LogLevel) -> bool {
        level >= self.log_level
    }
}

impl Default for MockLogger {
    fn default() -> Self {
        Self::new()
    }
}

impl MockLogger {
    pub fn new() -> Self {
        MockLogger {
            logs: Mutex::new(Vec::new()),
            log_level: Trace,
        }
    }
}

/// For easier debugging
#[derive(Debug)]
pub struct StdLogger {
    pub log_level: LogLevel,
}

impl Logger for StdLogger {
    fn log(&self, level: LogLevel, args: fmt::Arguments) {
        let message = fmt::format(args);
        println!("[{}] {}", level, message);
    }

    fn should_log(&self, level: LogLevel) -> bool {
        level >= self.log_level
    }
}

#[cfg(test)]
mod tests {
    use crate::api::logger::Logger;
    use crate::{LogLevel, MockLogger, error, trace};

    #[test]
    fn test_macro_laziness() {
        let mut mock_logger = MockLogger::new();
        mock_logger.log_level = LogLevel::Warn;

        let mut call_count = 0;

        trace!(mock_logger, "This is a trace message {}", {
            call_count += 1;
            call_count
        });
        error!(mock_logger, "This is an error message {}", {
            call_count += 1;
            call_count
        });

        assert_eq!(mock_logger.logs.lock().unwrap().len(), 1);
        assert_eq!(call_count, 1);
    }
}
