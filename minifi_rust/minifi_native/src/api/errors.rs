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

use minifi_native_sys::minifi_status;
use std::borrow::Cow;
use std::error::Error;
use std::ffi::NulError;
use std::fmt;
use std::num::{NonZeroU32, ParseFloatError, ParseIntError};
use std::str::ParseBoolError;

#[derive(Debug, Clone)]
pub enum ParseError {
    Strum(strum::ParseError),
    Bool(ParseBoolError),
    Int(ParseIntError),
    Duration(humantime::DurationError),
    Size(byte_unit::ParseError),
    Nul(NulError),
    Float(ParseFloatError),
    Other,
}

#[derive(Debug)]
pub enum MinifiError {
    UnknownError,
    StatusError((Cow<'static, str>, NonZeroU32)),
    MissingRequiredAttribute(Cow<'static, str>),
    MissingRequiredProperty(Cow<'static, str>),
    ControllerServiceError(Cow<'static, str>),
    ValidationError(Cow<'static, str>),
    ScheduleError(Cow<'static, str>),
    TriggerError(Cow<'static, str>),
    Parse(ParseError),
    MissingFlowFileError,
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

impl From<ParseFloatError> for MinifiError {
    fn from(err: ParseFloatError) -> Self {
        MinifiError::Parse(ParseError::Float(err))
    }
}

impl From<std::convert::Infallible> for MinifiError {
    fn from(_: std::convert::Infallible) -> Self {
        unreachable!("Infallible errors can never happen")
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
            MinifiError::Parse(_) => {
                minifi_native_sys::minifi_status_MINIFI_STATUS_VALIDATION_FAILED
            }
            MinifiError::StatusError((_, ecode)) => u32::from(*ecode),
            _ => minifi_native_sys::minifi_status_MINIFI_STATUS_UNKNOWN_ERROR,
        }
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

    pub fn missing_required_attribute<S: Into<Cow<'static, str>>>(msg: S) -> Self {
        MinifiError::MissingRequiredAttribute(msg.into())
    }

    pub fn controller_service_err<S: Into<Cow<'static, str>>>(msg: S) -> Self {
        MinifiError::ControllerServiceError(msg.into())
    }

    pub fn parse_err() -> Self {
        MinifiError::Parse(ParseError::Other)
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

impl Error for MinifiError {}
