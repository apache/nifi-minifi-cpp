use crate::StandardPropertyValidator::{
    BoolValidator, DataSizeValidator, TimePeriodValidator, U64Validator,
};
use crate::{ComponentIdentifier, EnableControllerService, MinifiError};
use std::str::FromStr;
use std::time::Duration;

#[derive(Debug, Eq, PartialEq)]
pub enum StandardPropertyValidator {
    AlwaysValidValidator,
    NonBlankValidator,
    TimePeriodValidator,
    BoolValidator,
    I64Validator,
    U64Validator,
    DataSizeValidator,
    PortValidator,
}

#[derive(Debug)]
pub struct Property {
    pub name: &'static str,
    pub description: &'static str,
    pub is_required: bool,
    pub is_sensitive: bool,
    pub supports_expr_lang: bool,
    pub default_value: Option<&'static str>,
    pub validator: StandardPropertyValidator,
    pub allowed_values: &'static [&'static str],
    pub allowed_type: &'static str,
}

pub trait GetProperty {
    fn get_property(&self, property: &Property) -> Result<Option<String>, MinifiError>;
    fn get_bool_property(&self, property: &Property) -> Result<Option<bool>, MinifiError> {
        if property.validator != BoolValidator {
            return Err(MinifiError::validation_err(format!(
                "to use get_bool_property {:?} must have BoolValidator",
                property
            )));
        }

        if let Some(property_val) = self.get_property(property)? {
            Ok(Some(bool::from_str(&property_val)?))
        } else {
            Ok(None)
        }
    }

    fn get_duration_property(&self, property: &Property) -> Result<Option<Duration>, MinifiError> {
        if property.validator != TimePeriodValidator {
            return Err(MinifiError::validation_err(format!(
                "to use get_duration_property {:?} must have TimePeriodValidator",
                property
            )));
        }

        if let Some(property_val) = self.get_property(property)? {
            Ok(Some(humantime::parse_duration(property_val.as_str())?))
        } else {
            Ok(None)
        }
    }

    fn get_size_property(&self, property: &Property) -> Result<Option<u64>, MinifiError> {
        if property.validator != DataSizeValidator {
            return Err(MinifiError::validation_err(format!(
                "to use get_size_property {:?} must have DataSizeValidator",
                property
            )));
        }
        if let Some(property_val) = self.get_property(property)? {
            Ok(Some(byte_unit::Byte::from_str(&property_val)?.as_u64()))
        } else {
            Ok(None)
        }
    }

    fn get_u64_property(&self, property: &Property) -> Result<Option<u64>, MinifiError> {
        if property.validator != U64Validator {
            return Err(MinifiError::validation_err(format!(
                "to use get_u64_property {:?} must have U64Validator",
                property
            )));
        }
        if let Some(property_val) = self.get_property(property)? {
            Ok(Some(u64::from_str(&property_val)?))
        } else {
            Ok(None)
        }
    }
}

pub trait GetControllerService {
    fn get_controller_service<Cs>(&self, property: &Property) -> Result<Option<&Cs>, MinifiError>
    where
        Cs: EnableControllerService + ComponentIdentifier + 'static;
}
