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
    BoolValidator, DataSizeValidator, TimePeriodValidator, U64Validator,
};
use crate::{
    ComponentIdentifier, ControllerServiceDefinition, EnableControllerService, MinifiError,
};
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
}

#[derive(Debug, PartialEq)]
pub enum PropertyConstraints {
    Validator(StandardPropertyValidator),
    AllowedValues(&'static [&'static str]),
    ControllerService(&'static str),
}

pub trait ProvidesPropertyConstraint {
    const PROPERTY_CONSTRAINT: Option<PropertyConstraints>;
}

#[derive(Debug)]
pub struct Property {
    pub name: &'static str,
    pub description: &'static str,
    pub is_required: bool,
    pub is_sensitive: bool,
    pub supports_expr_lang: bool,
    pub default_value: Option<&'static str>,
    pub constraints: Option<PropertyConstraints>,
}

pub trait PropertyType {
    type Output;
    const EXPECTED_CONSTRAINTS: Option<PropertyConstraints> = None;

    fn parse(s: &str) -> Result<Self::Output, MinifiError>;
}

impl PropertyConstraints {
    pub const fn non_blank() -> Option<Self> {
        Some(Self::Validator(
            StandardPropertyValidator::NonBlankValidator,
        ))
    }
}

pub const fn property_constraint<T: ProvidesPropertyConstraint + ?Sized>()
-> Option<PropertyConstraints> {
    T::PROPERTY_CONSTRAINT
}

macro_rules! impl_from_str_property {
    ($t:ty, $validator:expr) => {
        impl PropertyType for $t {
            type Output = $t;
            const EXPECTED_CONSTRAINTS: Option<PropertyConstraints> = $validator;

            fn parse(s: &str) -> Result<Self::Output, MinifiError> {
                s.parse::<$t>().map_err(Into::into)
            }
        }
        impl ProvidesPropertyConstraint for $t {
            const PROPERTY_CONSTRAINT: Option<PropertyConstraints> = $validator;
        }
    };
    ($t:ty) => {
        impl_from_str_property!($t, None);
    };
}

impl_from_str_property!(String);
impl_from_str_property!(std::path::PathBuf);
impl_from_str_property!(f64);
impl_from_str_property!(f32);
impl_from_str_property!(i64);
impl_from_str_property!(i32);
impl_from_str_property!(bool, Some(PropertyConstraints::Validator(BoolValidator)));
impl_from_str_property!(u64, Some(PropertyConstraints::Validator(U64Validator)));
impl_from_str_property!(u32, Some(PropertyConstraints::Validator(U64Validator)));
impl_from_str_property!(usize, Some(PropertyConstraints::Validator(U64Validator)));

impl PropertyType for Duration {
    type Output = Duration;
    const EXPECTED_CONSTRAINTS: Option<PropertyConstraints> =
        Some(PropertyConstraints::Validator(TimePeriodValidator));
    fn parse(s: &str) -> Result<Self::Output, MinifiError> {
        humantime::parse_duration(s).map_err(Into::into)
    }
}
impl ProvidesPropertyConstraint for Duration {
    const PROPERTY_CONSTRAINT: Option<PropertyConstraints> = Self::EXPECTED_CONSTRAINTS;
}

pub struct DataSize;
impl PropertyType for DataSize {
    type Output = u64;
    const EXPECTED_CONSTRAINTS: Option<PropertyConstraints> =
        Some(PropertyConstraints::Validator(DataSizeValidator));
    fn parse(s: &str) -> Result<Self::Output, MinifiError> {
        byte_unit::Byte::from_str(s)
            .map(|b| b.as_u64())
            .map_err(Into::into)
    }
}

impl ProvidesPropertyConstraint for DataSize {
    const PROPERTY_CONSTRAINT: Option<PropertyConstraints> = Self::EXPECTED_CONSTRAINTS;
}

pub trait GetProperty {
    fn get_raw_property(&self, property: &Property) -> Result<Option<String>, MinifiError>;

    fn get_property<T: PropertyType>(
        &self,
        property: &Property,
    ) -> Result<Option<T::Output>, MinifiError> {
        if let Some(expected) = T::EXPECTED_CONSTRAINTS
            && Some(expected) != property.constraints
        {
            return Err(MinifiError::validation_err(format!(
                "to use get_property for this type, {:?} must have validator {:?}",
                property.name,
                T::EXPECTED_CONSTRAINTS
            )));
        }

        if let Some(property_val) = self.get_raw_property(property)? {
            Ok(Some(T::parse(&property_val)?))
        } else {
            Ok(None)
        }
    }

    fn get_req_property<T: PropertyType>(
        &self,
        property: &Property,
    ) -> Result<T::Output, MinifiError> {
        if !property.is_required {
            return Err(MinifiError::validation_err(format!(
                "to use get_req_property, {:?} must be required",
                property.name
            )));
        }
        self.get_property::<T>(property)?
            .ok_or_else(|| MinifiError::missing_required_property(property.name))
    }
}

pub trait GetControllerService {
    fn get_controller_service<Cs>(&self, property: &Property) -> Result<Option<&Cs>, MinifiError>
    where
        Cs: EnableControllerService + ComponentIdentifier + ControllerServiceDefinition + 'static;
}
