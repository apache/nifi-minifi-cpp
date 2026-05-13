use crate::api::LogLevel;
use crate::api::Logger;
use minifi_native_sys::{minifi_log_level, minifi_logger, minifi_logger_log_string, minifi_logger_should_log, minifi_string_view};
use std::ffi::CString;
use std::fmt;

impl From<LogLevel> for minifi_log_level {
    fn from(level: LogLevel) -> Self {
        match level {
            LogLevel::Trace => minifi_native_sys::minifi_log_level_MINIFI_LOG_LEVEL_TRACE,
            LogLevel::Debug => minifi_native_sys::minifi_log_level_MINIFI_LOG_LEVEL_DEBUG,
            LogLevel::Info => minifi_native_sys::minifi_log_level_MINIFI_LOG_LEVEL_INFO,
            LogLevel::Warn => minifi_native_sys::minifi_log_level_MINIFI_LOG_LEVEL_WARNING,
            LogLevel::Error => minifi_native_sys::minifi_log_level_MINIFI_LOG_LEVEL_ERROR,
            LogLevel::Critical => minifi_native_sys::minifi_log_level_MINIFI_LOG_LEVEL_CRITICAL,
            LogLevel::Off => minifi_native_sys::minifi_log_level_MINIFI_LOG_LEVEL_OFF,
        }
    }
}

#[derive(Clone, Copy, Debug)]
pub struct CffiLogger {
    ptr: *mut minifi_logger,
}

impl CffiLogger {
    pub fn new(logger: *mut minifi_logger) -> Self {
        Self { ptr: logger }
    }
}

impl Logger for CffiLogger {
    fn log(&self, level: LogLevel, args: fmt::Arguments) {
        unsafe {
            let message = fmt::format(args);
            if let Ok(c_message) = CString::new(message) {
                minifi_logger_log_string(
                    self.ptr,
                    level.into(),
                    minifi_string_view {
                        data: c_message.as_ptr(),
                        length: c_message.as_bytes().len(),
                    },
                );
            }
        }
    }

    fn should_log(&self, level: LogLevel) -> bool {
        unsafe { minifi_logger_should_log(self.ptr, level.into()) }
    }
}
