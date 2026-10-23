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
    pub relationship: &'static str,
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
pub enum TransformError {
    /// The error was explicitly routed to a specific relationship (e.g. via
    /// [`TransformErrorExt::route_err`]).
    Route(RouteError),
    /// An error propagated with `?` that has not been explicitly routed. The
    /// processor wrapper routes it to the transform's declared error
    /// relationship (see `ERROR_RELATIONSHIP` on the transform traits).
    Bubbled(Box<dyn Error + Send + Sync + 'static>),
    /// The processor asked to roll back the session (see
    /// [`TransformErrorExt::rollback_err`]).
    Rollback(MinifiError),
}

impl TransformError {
    /// Resolve this error into either a concrete route or a rollback.
    ///
    /// `Bubbled` errors are routed to `default_relationship` at [`LogLevel::Warn`];
    /// already-`Route`d errors keep their relationship. Logging the returned
    /// [`RouteError`] is left to the caller.
    pub(crate) fn into_route(
        self,
        default_relationship: &Relationship,
    ) -> Result<RouteError, MinifiError> {
        match self {
            TransformError::Route(route) => Ok(route),
            TransformError::Bubbled(source) => Ok(RouteError {
                relationship: default_relationship.name,
                source,
                log_level: LogLevel::Warn,
            }),
            TransformError::Rollback(err) => Err(err),
        }
    }

    pub fn into_boxed(self) -> Box<dyn Error + Send + Sync + 'static> {
        match self {
            TransformError::Route(route) => Box::new(route),
            TransformError::Bubbled(source) => source,
            TransformError::Rollback(err) => Box::new(err),
        }
    }
}

impl<E> From<E> for TransformError
where
    E: Into<Box<dyn Error + Send + Sync + 'static>>,
{
    fn from(err: E) -> Self {
        let boxed: Box<dyn Error + Send + Sync + 'static> = err.into();
        match boxed.downcast::<RouteError>() {
            Ok(route) => TransformError::Route(*route),
            Err(source) => TransformError::Bubbled(source),
        }
    }
}

impl fmt::Display for TransformError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            TransformError::Route(err) => write!(f, "{}", err),
            TransformError::Bubbled(err) => write!(f, "{}", err),
            TransformError::Rollback(err) => write!(f, "{}", err),
        }
    }
}

/// Return early from a transform with a message, routing the flow file to the transform's
/// `ERROR_RELATIONSHIP` (see [`TransformError::Bubbled`]).
///
/// Takes `format!` arguments, implicit captures included:
///
/// ```
/// use minifi_native::{TransformError, route_to_err};
///
/// fn check(shape: &[usize]) -> Result<(), TransformError> {
///     if shape.is_empty() {
///         route_to_err!("Score tensor is empty; nothing to classify");
///     }
///     if shape.len() > 1 {
///         route_to_err!("expected a single score vector, got {shape:?}");
///     }
///     Ok(())
/// }
///
/// assert_eq!(
///     check(&[2, 3]).unwrap_err().to_string(),
///     "expected a single score vector, got [2, 3]"
/// );
/// ```
///
/// Names no error type: this works in any function whose error type converts from `String`.
/// To roll the session back instead of routing, use [`TransformErrorExt::rollback_err`].
#[macro_export]
macro_rules! route_to_err {
    ($($arg:tt)+) => {
        return Err(::std::format!($($arg)+).into())
    };
}

pub trait TransformErrorExt<T> {
    fn route_err(self, rel: &Relationship, level: LogLevel) -> Result<T, TransformError>;

    fn route_to(self, relationship: &'static str, level: LogLevel) -> Result<T, TransformError>;

    fn rollback_err(self) -> Result<T, TransformError>;
}

impl<T, E> TransformErrorExt<T> for Result<T, E>
where
    E: Into<Box<dyn Error + Send + Sync + 'static>>,
{
    fn route_err(self, rel: &Relationship, level: LogLevel) -> Result<T, TransformError> {
        self.route_to(rel.name, level)
    }

    fn route_to(
        self,
        relationship_name: &'static str,
        level: LogLevel,
    ) -> Result<T, TransformError> {
        self.map_err(|e| {
            TransformError::Route(RouteError {
                relationship: relationship_name,
                source: e.into(),
                log_level: level,
            })
        })
    }

    fn rollback_err(self) -> Result<T, TransformError> {
        self.map_err(|e| {
            let boxed: Box<dyn Error + Send + Sync + 'static> = e.into();
            match boxed.downcast::<MinifiError>() {
                Ok(minifi_error) => TransformError::Rollback(*minifi_error),
                Err(other) => TransformError::Rollback(MinifiError::Other(other)),
            }
        })
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
            MinifiError::Other(err) => write!(f, "{}", err),
            MinifiError::IoError(err) => write!(f, "{}", err),
            MinifiError::ValidationError(msg) => write!(f, "{}", msg),
            MinifiError::CustomError(msg) => write!(f, "{}", msg),
            MinifiError::MissingRequiredAttribute(name) => {
                write!(f, "missing required attribute '{}'", name)
            }
            MinifiError::MissingRequiredProperty(name) => {
                write!(f, "missing required property '{}'", name)
            }
            MinifiError::MissingFlowFileError => write!(f, "no flow file available"),
            MinifiError::UnscheduledProcessor => write!(f, "processor is not scheduled"),
            MinifiError::UnknownError => write!(f, "unknown error"),
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

    const REJECT: Relationship = Relationship {
        name: "reject",
        description: "",
    };

    #[test]
    fn route_err_uses_the_relationships_name() {
        let res: Result<(), std::io::Error> = Err(io_err());
        match res.route_err(&REJECT, LogLevel::Info) {
            Err(TransformError::Route(route)) => {
                assert_eq!(route.relationship, "reject");
                assert_eq!(route.log_level, LogLevel::Info);
            }
            other => panic!("expected a route error, got {other:?}"),
        }
    }

    #[test]
    fn ok_values_pass_through_unchanged() {
        let res: Result<u8, std::io::Error> = Ok(5);
        assert_eq!(res.route_err(&REJECT, LogLevel::Info).unwrap(), 5);
    }

    #[test]
    fn minifi_error_converts_to_bubbled_via_from() {
        let pe: TransformError = MinifiError::custom("nope").into();
        assert!(matches!(pe, TransformError::Bubbled(_)));
    }

    #[test]
    fn raw_error_question_mark_becomes_bubbled() {
        fn inner() -> Result<(), TransformError> {
            Err(io_err())?;
            Ok(())
        }
        match inner() {
            Err(TransformError::Bubbled(source)) => {
                assert_eq!(source.to_string(), "boom");
            }
            other => panic!("expected a bubbled error, got {other:?}"),
        }
    }

    #[test]
    fn foreign_error_without_a_from_impl_becomes_bubbled() {
        #[derive(Debug)]
        struct Foreign;

        impl fmt::Display for Foreign {
            fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
                write!(f, "foreign")
            }
        }

        impl Error for Foreign {}

        fn inner() -> Result<(), TransformError> {
            Err(Foreign)?;
            Ok(())
        }
        match inner() {
            Err(TransformError::Bubbled(source)) => assert_eq!(source.to_string(), "foreign"),
            other => panic!("expected a bubbled error, got {other:?}"),
        }
    }

    #[test]
    fn anyhow_error_becomes_bubbled() {
        fn inner() -> Result<(), TransformError> {
            Err(anyhow::anyhow!("boom"))?;
            Ok(())
        }
        match inner() {
            Err(TransformError::Bubbled(source)) => assert_eq!(source.to_string(), "boom"),
            other => panic!("expected a bubbled error, got {other:?}"),
        }
    }

    #[test]
    fn bail_bubbles_a_literal_message() {
        fn inner() -> Result<(), TransformError> {
            route_to_err!("nothing to classify");
        }
        match inner() {
            Err(TransformError::Bubbled(source)) => {
                assert_eq!(source.to_string(), "nothing to classify")
            }
            other => panic!("expected a bubbled error, got {other:?}"),
        }
    }

    // The message must go through `format!`, so implicit captures are interpolated rather than
    // printed as literal braces.
    #[test]
    fn bail_interpolates_implicit_captures() {
        fn inner() -> Result<(), TransformError> {
            let shape = [2, 3];
            route_to_err!("expected a single score vector, got {shape:?}");
        }
        match inner() {
            Err(TransformError::Bubbled(source)) => {
                assert_eq!(
                    source.to_string(),
                    "expected a single score vector, got [2, 3]"
                )
            }
            other => panic!("expected a bubbled error, got {other:?}"),
        }
    }

    // Guards the reflexive `impl<T> From<T> for T`: a nested transform can be propagated
    // with `?` without being re-wrapped. Adding `impl Error for TransformError` breaks this.
    #[test]
    fn transform_error_question_mark_is_not_rewrapped() {
        fn nested() -> Result<(), TransformError> {
            Err(MinifiError::validation("bad"))?;
            Ok(())
        }
        fn outer() -> Result<(), TransformError> {
            nested()?;
            Ok(())
        }
        match outer() {
            Err(TransformError::Bubbled(source)) => assert_eq!(source.to_string(), "bad"),
            other => panic!("expected a bubbled error, got {other:?}"),
        }
    }

    #[test]
    fn route_error_question_mark_keeps_its_relationship() {
        fn inner() -> Result<(), TransformError> {
            Err(RouteError {
                relationship: "reject",
                source: Box::new(io_err()),
                log_level: LogLevel::Info,
            })?;
            Ok(())
        }
        match inner() {
            Err(TransformError::Route(route)) => {
                assert_eq!(route.relationship, "reject");
                assert_eq!(route.log_level, LogLevel::Info);
            }
            other => panic!("expected a route error, got {other:?}"),
        }
    }

    #[test]
    fn into_boxed_erases_every_variant() {
        let route: TransformError = RouteError {
            relationship: "reject",
            source: Box::new(io_err()),
            log_level: LogLevel::Info,
        }
        .into();
        assert_eq!(
            route.into_boxed().to_string(),
            "route to 'reject' due to: boom"
        );

        let bubbled: TransformError = io_err().into();
        assert_eq!(bubbled.into_boxed().to_string(), "boom");

        let rollback: Result<(), MinifiError> = Err(MinifiError::validation("bad"));
        let rollback = rollback.rollback_err().unwrap_err();
        assert_eq!(rollback.into_boxed().to_string(), "bad");
    }

    #[test]
    fn into_route_routes_bubbled_to_default_relationship_at_warn() {
        let bubbled: TransformError = io_err().into();
        match bubbled.into_route(&REJECT) {
            Ok(route) => {
                assert_eq!(route.relationship, "reject");
                assert_eq!(route.log_level, LogLevel::Warn);
                assert_eq!(route.source.to_string(), "boom");
            }
            Err(e) => panic!("expected a route, got {e:?}"),
        }
    }

    #[test]
    fn into_route_keeps_explicit_relationship() {
        let res: Result<(), std::io::Error> = Err(io_err());
        let routed = res.route_to("explicit", LogLevel::Info).unwrap_err();
        let route = routed.into_route(&REJECT).expect("should stay a route");
        assert_eq!(route.relationship, "explicit");
        assert_eq!(route.log_level, LogLevel::Info);
    }

    #[test]
    fn into_route_propagates_rollback_as_minifi_error() {
        let res: Result<(), MinifiError> = Err(MinifiError::validation("bad"));
        let rollback = res.rollback_err().unwrap_err();
        assert!(matches!(
            rollback.into_route(&REJECT),
            Err(MinifiError::ValidationError(_))
        ));
    }

    #[test]
    fn rollback_err_wraps_foreign_error_as_other() {
        let res: Result<(), std::io::Error> = Err(io_err());
        assert!(matches!(
            res.rollback_err(),
            Err(TransformError::Rollback(MinifiError::Other(_)))
        ));
    }

    #[test]
    fn rollback_err_preserves_minifi_error_variant() {
        let res: Result<(), MinifiError> = Err(MinifiError::validation("bad"));
        assert!(matches!(
            res.rollback_err(),
            Err(TransformError::Rollback(MinifiError::ValidationError(_)))
        ));
    }
}
