use minifi_native_sys::minifi_status;
use std::borrow::Cow;
use std::ffi::NulError;
use std::fmt;
use std::num::{NonZeroU32, ParseIntError};
use std::str::ParseBoolError;

#[derive(Debug, Clone)]
pub enum ParseError {
    Strum(strum::ParseError),
    Bool(ParseBoolError),
    Int(ParseIntError),
    Duration(humantime::DurationError),
    Size(byte_unit::ParseError),
    Nul(NulError),
    Other,
}

#[derive(Debug)]
pub enum MinifiError {
    UnknownError,
    StatusError((Cow<'static, str>, NonZeroU32)),
    MissingRequiredProperty(Cow<'static, str>),
    ControllerServiceError(Cow<'static, str>),
    ValidationError(Cow<'static, str>),
    ScheduleError(Cow<'static, str>),
    TriggerError(Cow<'static, str>),
    Parse(ParseError),
    IoError(std::io::Error),
}

impl From<std::io::Error> for MinifiError {
    fn from(error: std::io::Error) -> Self {
        MinifiError::IoError(error)
    }
}

impl From<strum::ParseError> for MinifiError {
    fn from(err: strum::ParseError) -> Self {
        MinifiError::Parse(ParseError::Strum(err))
    }
}

impl From<ParseBoolError> for MinifiError {
    fn from(err: ParseBoolError) -> Self {
        MinifiError::Parse(ParseError::Bool(err))
    }
}

impl From<ParseIntError> for MinifiError {
    fn from(err: ParseIntError) -> Self {
        MinifiError::Parse(ParseError::Int(err))
    }
}

impl From<humantime::DurationError> for MinifiError {
    fn from(err: humantime::DurationError) -> Self {
        MinifiError::Parse(ParseError::Duration(err))
    }
}

impl From<byte_unit::ParseError> for MinifiError {
    fn from(err: byte_unit::ParseError) -> Self {
        MinifiError::Parse(ParseError::Size(err))
    }
}

impl From<NulError> for MinifiError {
    fn from(err: NulError) -> Self {
        MinifiError::Parse(ParseError::Nul(err))
    }
}

impl MinifiError {
    pub(crate) fn to_status(&self) -> minifi_status {
        // TODO expand this
        minifi_native_sys::minifi_status_MINIFI_STATUS_UNKNOWN_ERROR
    }

    pub fn validation_err<S: Into<Cow<'static, str>>>(msg: S) -> Self {
        MinifiError::ValidationError(msg.into())
    }

    pub fn schedule_err<S: Into<Cow<'static, str>>>(msg: S) -> Self {
        MinifiError::ScheduleError(msg.into())
    }

    pub fn trigger_err<S: Into<Cow<'static, str>>>(msg: S) -> Self {
        MinifiError::TriggerError(msg.into())
    }

    pub fn missing_required_property<S: Into<Cow<'static, str>>>(msg: S) -> Self {
        MinifiError::MissingRequiredProperty(msg.into())
    }

    pub fn controller_service_err<S: Into<Cow<'static, str>>>(msg: S) -> Self {
        MinifiError::ControllerServiceError(msg.into())
    }
}

impl fmt::Display for MinifiError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            MinifiError::StatusError((context, code)) => match code.get() {
                minifi_native_sys::minifi_status_MINIFI_STATUS_UNKNOWN_ERROR => {
                    write!(f, "{}, unknown error", context)
                }
                minifi_native_sys::minifi_status_MINIFI_STATUS_NOT_SUPPORTED_PROPERTY => {
                    write!(f, "{}, not supported property", context)
                }
                minifi_native_sys::minifi_status_MINIFI_STATUS_DYNAMIC_PROPERTIES_NOT_SUPPORTED => {
                    write!(f, "{}, dynamic properties not supported", context)
                }
                minifi_native_sys::minifi_status_MINIFI_STATUS_PROPERTY_NOT_SET => {
                    write!(f, "{}, property not set", context)
                }
                minifi_native_sys::minifi_status_MINIFI_STATUS_VALIDATION_FAILED => {
                    write!(f, "{}, validation failed", context)
                }
                minifi_native_sys::minifi_status_MINIFI_STATUS_PROCESSOR_YIELD => {
                    write!(f, "{}, processor yield", context)
                }
                _ => write!(f, "{} (Unknown Status Code: {})", context, code),
            },
            _ => write!(f, "{:?}", self),
        }
    }
}
