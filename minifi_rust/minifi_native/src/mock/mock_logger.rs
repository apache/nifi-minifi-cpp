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

        trace!(
            mock_logger,
            "This is a trace message {}",
            || -> u32 {
                call_count += 1;
                call_count
            }()
        );
        error!(
            mock_logger,
            "This is an error message {}",
            || -> u32 {
                call_count += 1;
                call_count
            }()
        );

        assert_eq!(mock_logger.logs.lock().unwrap().len(), 1);
        assert_eq!(call_count, 1);
    }
}
