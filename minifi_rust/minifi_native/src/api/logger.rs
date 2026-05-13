use std::fmt;
use std::fmt::Debug;

use strum_macros::{Display, EnumString, VariantNames};

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Display, EnumString, VariantNames)]
#[strum(serialize_all = "PascalCase", const_into_str)]
pub enum LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Critical,
    Off,
}

pub trait Logger: Debug {
    fn log(&self, level: LogLevel, args: fmt::Arguments);
    fn should_log(&self, level: LogLevel) -> bool;
}

/// The "Master" macro that handles the core logic.
/// It takes the logger instance, the level, and the format string/args.
#[macro_export]
macro_rules! log {
    ($logger:expr, $level:expr, $($arg:tt)+) => {
        if $logger.should_log($level) {
            $logger.log($level, format_args!($($arg)+))
        }
    };
}

/// Log at the Trace level
#[macro_export]
macro_rules! trace {
    ($logger:expr, $($arg:tt)+) => {
        $crate::log!($logger, $crate::LogLevel::Trace, $($arg)+)
    };
}

/// Log at the Debug level
#[macro_export]
macro_rules! debug {
    ($logger:expr, $($arg:tt)+) => {
        $crate::log!($logger, $crate::LogLevel::Debug, $($arg)+)
    };
}

/// Log at the Info level
#[macro_export]
macro_rules! info {
    ($logger:expr, $($arg:tt)+) => {
        $crate::log!($logger, $crate::LogLevel::Info, $($arg)+)
    };
}

/// Log at the Warn level
#[macro_export]
macro_rules! warn {
    ($logger:expr, $($arg:tt)+) => {
        $crate::log!($logger, $crate::LogLevel::Warn, $($arg)+)
    };
}

/// Log at the Error level
#[macro_export]
macro_rules! error {
    ($logger:expr, $($arg:tt)+) => {
        $crate::log!($logger, $crate::LogLevel::Error, $($arg)+)
    };
}

/// Log at the Critical level
#[macro_export]
macro_rules! critical {
    ($logger:expr, $($arg:tt)+) => {
        $crate::log!($logger, $crate::LogLevel::Critical, $($arg)+)
    };
}

#[cfg(test)]
mod tests {
    use crate::LogLevel;

    #[test]
    fn order_test() {
        assert!(LogLevel::Debug > LogLevel::Trace);
        assert!(LogLevel::Info > LogLevel::Debug);
        assert!(LogLevel::Warn > LogLevel::Info);
        assert!(LogLevel::Error > LogLevel::Warn);
        assert!(LogLevel::Critical > LogLevel::Error);
        assert!(LogLevel::Off > LogLevel::Critical);
    }
}
