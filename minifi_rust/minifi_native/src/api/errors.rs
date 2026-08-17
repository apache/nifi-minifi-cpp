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

use minifi_native::{LogLevel, Relationship};
use minifi_native_sys::minifi_status;
use std::borrow::Cow;
use std::error::Error;
use std::ffi::NulError;
use std::fmt;
use std::num::{NonZeroU32, ParseFloatError, ParseIntError};
use std::str::ParseBoolError;

#[derive(Debug)]
pub struct RouteError {
    pub relationship: Cow<'static, str>,
    pub source: Box<dyn Error + Send + Sync + 'static>,
    pub log_level: LogLevel,
}

impl RouteError {
    pub(crate) fn log<L: crate::Logger>(&self, logger: &L) {
        logger.log(
            self.log_level,
            format_args!(
                "Routing flow file to '{}': {}",
                self.relationship, self.source
            ),
        );
    }
}

impl fmt::Display for RouteError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "route to '{}' due to: {}",
            self.relationship, self.source
        )
    }
}

impl Error for RouteError {}

#[derive(Debug)]
pub enum ProcessError {
    Route(RouteError),
    Fatal(MinifiError),
}

impl From<RouteError> for ProcessError {
    fn from(err: RouteError) -> Self {
        ProcessError::Route(err)
    }
}

impl From<MinifiError> for ProcessError {
    fn from(err: MinifiError) -> Self {
        ProcessError::Fatal(err)
    }
}

macro_rules! process_error_from_fatal {
    ($($t:ty),* $(,)?) => {
        $(
            impl From<$t> for ProcessError {
                fn from(err: $t) -> Self {
                    ProcessError::Fatal(MinifiError::from(err))
                }
            }
        )*
    };
}

process_error_from_fatal!(
    std::io::Error,
    strum::ParseError,
    ParseBoolError,
    ParseIntError,
    humantime::DurationError,
    byte_unit::ParseError,
    NulError,
    ParseFloatError,
    std::convert::Infallible,
);

impl fmt::Display for ProcessError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            ProcessError::Route(err) => write!(f, "{}", err),
            ProcessError::Fatal(err) => write!(f, "{}", err),
        }
    }
}

impl Error for ProcessError {}

pub trait RouteErrorExt<T> {
    fn route_err(self, rel: &Relationship, level: LogLevel) -> Result<T, ProcessError>;

    fn route_to<S: Into<Cow<'static, str>>>(
        self,
        relationship: S,
        level: LogLevel,
    ) -> Result<T, ProcessError>;

    fn route_err_to_failure(self) -> Result<T, ProcessError>;
}

impl<T, E> RouteErrorExt<T> for Result<T, E>
where
    E: Into<Box<dyn Error + Send + Sync + 'static>>,
{
    fn route_err(self, rel: &Relationship, level: LogLevel) -> Result<T, ProcessError> {
        self.route_to(rel.name, level)
    }

    fn route_to<S: Into<Cow<'static, str>>>(
        self,
        relationship: S,
        level: LogLevel,
    ) -> Result<T, ProcessError> {
        self.map_err(|e| {
            ProcessError::Route(RouteError {
                relationship: relationship.into(),
                source: e.into(),
                log_level: level,
            })
        })
    }

    fn route_err_to_failure(self) -> Result<T, ProcessError> {
        self.route_to("failure", LogLevel::Warn)
    }
}

#[derive(Debug)]
pub enum MinifiError {
    UnknownError,
    StatusError((Cow<'static, str>, NonZeroU32)),
    MissingRequiredAttribute(Cow<'static, str>),
    MissingRequiredProperty(Cow<'static, str>),
    UnscheduledProcessor,
    ValidationError(Cow<'static, str>),
    CustomError(Cow<'static, str>),
    MissingFlowFileError,
    IoError(std::io::Error),

    Other(Box<dyn Error + Send + Sync + 'static>),
}

impl From<std::io::Error> for MinifiError {
    fn from(error: std::io::Error) -> Self {
        MinifiError::IoError(error)
    }
}

macro_rules! minifi_error_from_validation {
    ($($t:ty),* $(,)?) => {
        $(
            impl From<$t> for MinifiError {
                fn from(err: $t) -> Self {
                    MinifiError::ValidationError(err.to_string().into())
                }
            }
        )*
    };
}

minifi_error_from_validation!(
    strum::ParseError,
    ParseBoolError,
    ParseIntError,
    humantime::DurationError,
    byte_unit::ParseError,
    NulError,
    ParseFloatError,
);

impl From<std::convert::Infallible> for MinifiError {
    fn from(_: std::convert::Infallible) -> Self {
        unreachable!("Infallible errors can never happen")
    }
}

impl From<anyhow::Error> for MinifiError {
    fn from(err: anyhow::Error) -> Self {
        Self::other(err)
    }
}

impl MinifiError {
    pub(crate) fn to_status(&self) -> minifi_status {
        match self {
            MinifiError::MissingRequiredProperty(_) => {
                minifi_native_sys::minifi_status_MINIFI_STATUS_PROPERTY_NOT_SET
            }
            MinifiError::UnknownError => {
                minifi_native_sys::minifi_status_MINIFI_STATUS_UNKNOWN_ERROR
            }
            MinifiError::ValidationError(_) => {
                minifi_native_sys::minifi_status_MINIFI_STATUS_VALIDATION_FAILED
            }
            MinifiError::StatusError((_, ecode)) => u32::from(*ecode),
            _ => minifi_native_sys::minifi_status_MINIFI_STATUS_UNKNOWN_ERROR,
        }
    }

    pub fn validation<S: Into<Cow<'static, str>>>(msg: S) -> Self {
        MinifiError::ValidationError(msg.into())
    }

    pub fn custom<S: Into<Cow<'static, str>>>(msg: S) -> Self {
        MinifiError::CustomError(msg.into())
    }

    pub fn other<E>(err: E) -> Self
    where
        E: Into<Box<dyn Error + Send + Sync + 'static>>,
    {
        MinifiError::Other(err.into())
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
            MinifiError::Other(err) => write!(f, "Custom error: {}", err),
            MinifiError::ValidationError(msg) => write!(f, "{}", msg),
            _ => write!(f, "{:?}", self),
        }
    }
}

impl Error for MinifiError {}

#[cfg(test)]
mod tests {
    use super::*;

    fn io_err() -> std::io::Error {
        std::io::Error::other("boom")
    }

    #[test]
    fn route_err_to_failure_uses_warn() {
        let res: Result<(), std::io::Error> = Err(io_err());
        match res.route_err_to_failure() {
            Err(ProcessError::Route(route)) => {
                assert_eq!(route.relationship.as_ref(), "failure");
                assert_eq!(route.log_level, LogLevel::Warn);
                assert_eq!(route.source.to_string(), "boom");
            }
            other => panic!("expected a route error, got {other:?}"),
        }
    }

    #[test]
    fn route_err_uses_the_relationships_name() {
        const REJECT: Relationship = Relationship {
            name: "reject",
            description: "",
        };
        let res: Result<(), std::io::Error> = Err(io_err());
        match res.route_err(&REJECT, LogLevel::Info) {
            Err(ProcessError::Route(route)) => {
                assert_eq!(route.relationship.as_ref(), "reject");
                assert_eq!(route.log_level, LogLevel::Info);
            }
            other => panic!("expected a route error, got {other:?}"),
        }
    }

    #[test]
    fn ok_values_pass_through_unchanged() {
        let res: Result<u8, std::io::Error> = Ok(5);
        assert_eq!(res.route_err_to_failure().unwrap(), 5);
    }

    #[test]
    fn minifi_error_converts_to_fatal_via_from() {
        let pe: ProcessError = MinifiError::custom("nope").into();
        assert!(matches!(
            pe,
            ProcessError::Fatal(MinifiError::CustomError(_))
        ));
    }

    #[test]
    fn raw_error_question_mark_becomes_fatal() {
        fn inner() -> Result<(), ProcessError> {
            Err(io_err())?;
            Ok(())
        }
        assert!(matches!(
            inner(),
            Err(ProcessError::Fatal(MinifiError::IoError(_)))
        ));
    }
}
