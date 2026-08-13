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

use crate::StandardPropertyValidator::{
    BoolValidator, DataSizeValidator, NonBlankValidator, TimePeriodValidator, U64Validator,
};
use crate::{
    ComponentIdentifier, ControllerServiceDefinition, EnableControllerService, MinifiError,
};
use minifi_native::StandardPropertyValidator::{F64Validator, I64Validator};
use std::marker::PhantomData;
use std::str::FromStr;
use std::time::Duration;

#[derive(Debug, Eq, PartialEq)]
pub enum StandardPropertyValidator {
    NonBlankValidator,
    TimePeriodValidator,
    BoolValidator,
    I64Validator,
    U64Validator,
    DataSizeValidator,
    PortValidator,
    F64Validator,
}

#[derive(Debug, PartialEq)]
pub enum PropertyConstraints {
    Validator(StandardPropertyValidator),
    AllowedValues(&'static [&'static str]),
    ControllerService(&'static str),
}

pub struct PropertyDefinition {
    pub name: &'static str,
    pub description: &'static str,
    pub is_required: bool,
    pub is_sensitive: bool,
    pub supports_expr_lang: bool,
    pub default_value: Option<&'static str>,
    pub constraints: Option<PropertyConstraints>,
}

#[macro_export]
macro_rules! property_definitions {
    ($($property:expr),* $(,)?) => {
        &[$($property.definition()),*]
    };
}

pub struct Property<P: ?Sized + PropertySchema> {
    pub(crate) name: &'static str,
    pub(crate) description: &'static str,
    pub(crate) is_sensitive: bool,
    pub(crate) supports_expr_lang: bool,
    pub(crate) default_value: Option<&'static str>,
    pub(crate) marker: PhantomData<P>,
}

impl<P: ?Sized + PropertySchema> Property<P> {
    pub const fn new(name: &'static str, description: &'static str) -> Self {
        Property {
            name,
            description,
            is_sensitive: false,
            supports_expr_lang: false,
            default_value: None,
            marker: PhantomData,
        }
    }

    pub const fn sensitive(mut self) -> Self {
        self.is_sensitive = true;
        self
    }

    pub const fn supports_expression_language(mut self) -> Self {
        self.supports_expr_lang = true;
        self
    }

    pub const fn with_default(mut self, default_value: &'static str) -> Self {
        self.default_value = Some(default_value);
        self
    }

    pub const fn name(&self) -> &'static str {
        self.name
    }

    pub const fn definition(&self) -> PropertyDefinition {
        PropertyDefinition {
            name: self.name,
            description: self.description,
            is_required: P::IS_REQUIRED,
            is_sensitive: self.is_sensitive,
            supports_expr_lang: self.supports_expr_lang,
            default_value: self.default_value,
            constraints: P::CONSTRAINT,
        }
    }

    pub(crate) const fn with_marker<K2: ?Sized + PropertySchema>(&self) -> Property<K2> {
        Property {
            name: self.name,
            description: self.description,
            is_sensitive: self.is_sensitive,
            supports_expr_lang: self.supports_expr_lang,
            default_value: self.default_value,
            marker: PhantomData,
        }
    }
}

pub trait PropertySchema {
    const CONSTRAINT: Option<PropertyConstraints>;
    const IS_REQUIRED: bool;
}

impl<T: PropertySchema> PropertySchema for Option<T> {
    const CONSTRAINT: Option<PropertyConstraints> = T::CONSTRAINT;
    const IS_REQUIRED: bool = false;
}

pub trait PropertyType: PropertySchema {
    type Output;
    fn parse(s: &str) -> Result<Self::Output, MinifiError>;
}

pub trait PropertyValue: PropertySchema {
    type Output;
    fn from_raw(raw: Option<String>, name: &str) -> Result<Self::Output, MinifiError>;
}

impl<T: PropertyType> PropertyValue for T {
    type Output = T::Output;
    fn from_raw(raw: Option<String>, name: &str) -> Result<Self::Output, MinifiError> {
        match raw {
            Some(value) => T::parse(&value),
            None => Err(MinifiError::missing_required_property(name.to_string())),
        }
    }
}

impl<T: PropertyType> PropertyValue for Option<T> {
    type Output = Option<T::Output>;
    fn from_raw(raw: Option<String>, _name: &str) -> Result<Self::Output, MinifiError> {
        match raw {
            Some(value) => Ok(Some(T::parse(&value)?)),
            None => Ok(None),
        }
    }
}

macro_rules! impl_from_str_property {
    ($t:ty, $constraint:expr) => {
        impl PropertyType for $t {
            type Output = $t;

            fn parse(s: &str) -> Result<Self::Output, MinifiError> {
                s.parse::<$t>().map_err(Into::into)
            }
        }
        impl PropertySchema for $t {
            const CONSTRAINT: Option<PropertyConstraints> = $constraint;
            const IS_REQUIRED: bool = true;
        }
    };
    ($t:ty) => {
        impl_from_str_property!($t, None);
    };
}

impl_from_str_property!(String);
impl_from_str_property!(std::path::PathBuf);
impl_from_str_property!(f64, Some(PropertyConstraints::Validator(F64Validator)));
impl_from_str_property!(f32, Some(PropertyConstraints::Validator(F64Validator)));
impl_from_str_property!(i64, Some(PropertyConstraints::Validator(I64Validator)));
impl_from_str_property!(i32, Some(PropertyConstraints::Validator(I64Validator)));
impl_from_str_property!(bool, Some(PropertyConstraints::Validator(BoolValidator)));
impl_from_str_property!(u64, Some(PropertyConstraints::Validator(U64Validator)));
impl_from_str_property!(u32, Some(PropertyConstraints::Validator(U64Validator)));
impl_from_str_property!(usize, Some(PropertyConstraints::Validator(U64Validator)));

impl PropertyType for Duration {
    type Output = Duration;
    fn parse(s: &str) -> Result<Self::Output, MinifiError> {
        humantime::parse_duration(s).map_err(Into::into)
    }
}
impl PropertySchema for Duration {
    const CONSTRAINT: Option<PropertyConstraints> =
        Some(PropertyConstraints::Validator(TimePeriodValidator));
    const IS_REQUIRED: bool = true;
}

pub struct DataSize;
impl PropertyType for DataSize {
    type Output = u64;
    fn parse(s: &str) -> Result<Self::Output, MinifiError> {
        byte_unit::Byte::from_str(s)
            .map(|b| b.as_u64())
            .map_err(Into::into)
    }
}
impl PropertySchema for DataSize {
    const CONSTRAINT: Option<PropertyConstraints> =
        Some(PropertyConstraints::Validator(DataSizeValidator));
    const IS_REQUIRED: bool = true;
}

pub struct NonBlankPath;
impl PropertyType for NonBlankPath {
    type Output = std::path::PathBuf;
    fn parse(s: &str) -> Result<Self::Output, MinifiError> {
        <std::path::PathBuf as PropertyType>::parse(s)
    }
}
impl PropertySchema for NonBlankPath {
    const CONSTRAINT: Option<PropertyConstraints> =
        Some(PropertyConstraints::Validator(NonBlankValidator));
    const IS_REQUIRED: bool = true;
}

pub trait GetProperty {
    fn get_raw_property<P: PropertySchema + ?Sized>(
        &self,
        property: &Property<P>,
    ) -> Result<Option<String>, MinifiError>;

    fn get_property<P: PropertyValue + ?Sized>(
        &self,
        property: &Property<P>,
    ) -> Result<P::Output, MinifiError> {
        P::from_raw(self.get_raw_property(property)?, property.name)
    }
}

pub trait ControllerServiceValue: PropertySchema {
    type Cs: EnableControllerService
        + ComponentIdentifier
        + ControllerServiceDefinition
        + PropertySchema
        + 'static;
    type Output<'a>;
    fn from_service<'a>(
        service: Option<&'a Self::Cs>,
        name: &str,
    ) -> Result<Self::Output<'a>, MinifiError>;
}

impl<Cs> ControllerServiceValue for Cs
where
    Cs: EnableControllerService
        + ComponentIdentifier
        + ControllerServiceDefinition
        + PropertySchema
        + 'static,
{
    type Cs = Cs;
    type Output<'a> = &'a Cs;
    fn from_service<'a>(service: Option<&'a Cs>, name: &str) -> Result<&'a Cs, MinifiError> {
        service.ok_or_else(|| MinifiError::missing_required_property(name.to_string()))
    }
}

impl<Cs> ControllerServiceValue for Option<Cs>
where
    Cs: EnableControllerService
        + ComponentIdentifier
        + ControllerServiceDefinition
        + PropertySchema
        + 'static,
{
    type Cs = Cs;
    type Output<'a> = Option<&'a Cs>;
    fn from_service<'a>(
        service: Option<&'a Cs>,
        _name: &str,
    ) -> Result<Option<&'a Cs>, MinifiError> {
        Ok(service)
    }
}

pub trait GetControllerService {
    fn get_controller_service<P>(
        &self,
        property: &Property<P>,
    ) -> Result<P::Output<'_>, MinifiError>
    where
        P: ControllerServiceValue + ?Sized;
}
