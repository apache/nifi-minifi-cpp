use crate::api::LogLevel;
use crate::api::Logger;
use minifi_native_sys::{
    MinifiLogLevel, MinifiLogger, MinifiLoggerLogString, MinifiLoggerShouldLog, MinifiStringView,
};
use std::ffi::CString;
use std::fmt;

impl From<LogLevel> for MinifiLogLevel {
    fn from(level: LogLevel) -> Self {
        match level {
            LogLevel::Trace => minifi_native_sys::MinifiLogLevel_MINIFI_LOG_LEVEL_TRACE,
            LogLevel::Debug => minifi_native_sys::MinifiLogLevel_MINIFI_LOG_LEVEL_DEBUG,
            LogLevel::Info => minifi_native_sys::MinifiLogLevel_MINIFI_LOG_LEVEL_INFO,
            LogLevel::Warn => minifi_native_sys::MinifiLogLevel_MINIFI_LOG_LEVEL_WARNING,
            LogLevel::Error => minifi_native_sys::MinifiLogLevel_MINIFI_LOG_LEVEL_ERROR,
            LogLevel::Critical => minifi_native_sys::MinifiLogLevel_MINIFI_LOG_LEVEL_CRITICAL,
            LogLevel::Off => minifi_native_sys::MinifiLogLevel_MINIFI_LOG_LEVEL_OFF,
        }
    }
}

#[derive(Clone, Copy, Debug)]
pub struct CffiLogger {
    ptr: *mut MinifiLogger,
}

impl CffiLogger {
    pub fn new(logger: *mut MinifiLogger) -> Self {
        Self { ptr: logger }
    }
}

impl Logger for CffiLogger {
    fn log(&self, level: LogLevel, args: fmt::Arguments) {
        unsafe {
            let message = fmt::format(args);
            if let Ok(c_message) = CString::new(message) {
                MinifiLoggerLogString(
                    self.ptr,
                    level.into(),
                    MinifiStringView {
                        data: c_message.as_ptr(),
                        length: c_message.as_bytes().len(),
                    },
                );
            }
        }
    }

    fn should_log(&self, level: LogLevel) -> bool {
        unsafe { MinifiLoggerShouldLog(self.ptr, level.into()) }
    }
}
