use crate::StandardPropertyValidator::*;
use crate::api::RawControllerService;
use crate::api::component_definition_traits::ComponentIdentifier;
use crate::api::flow_file::FlowFile;
use crate::api::property::GetControllerService;
use crate::{EnableControllerService, GetProperty, MinifiError, Property};
use std::str::FromStr;
use std::time::Duration;

pub trait ProcessContext {
    type FlowFile: FlowFile;

    fn get_property(
        &self,
        property: &Property,
        flow_file: Option<&Self::FlowFile>,
    ) -> Result<Option<String>, MinifiError>;

    fn get_bool_property(
        &self,
        property: &Property,
        flow_file: Option<&Self::FlowFile>,
    ) -> Result<Option<bool>, MinifiError> {
        if property.validator != BoolValidator {
            return Err(MinifiError::validation_err(format!(
                "to use get_bool_property {:?} must have BoolValidator",
                property
            )));
        }

        if let Some(property_val) = self.get_property(property, flow_file)? {
            Ok(Some(bool::from_str(&property_val)?))
        } else {
            Ok(None)
        }
    }

    fn get_duration_property(
        &self,
        property: &Property,
        flow_file: Option<&Self::FlowFile>,
    ) -> Result<Option<Duration>, MinifiError> {
        if property.validator != TimePeriodValidator {
            return Err(MinifiError::validation_err(format!(
                "to use get_duration_property {:?} must have TimePeriodValidator",
                property
            )));
        }

        if let Some(property_val) = self.get_property(property, flow_file)? {
            Ok(Some(humantime::parse_duration(property_val.as_str())?))
        } else {
            Ok(None)
        }
    }

    fn get_size_property(
        &self,
        property: &Property,
        flow_file: Option<&Self::FlowFile>,
    ) -> Result<Option<u64>, MinifiError> {
        if property.validator != DataSizeValidator {
            return Err(MinifiError::validation_err(format!(
                "to use get_size_property {:?} must have DataSizeValidator",
                property
            )));
        }
        if let Some(property_val) = self.get_property(property, flow_file)? {
            Ok(Some(byte_unit::Byte::from_str(&property_val)?.as_u64()))
        } else {
            Ok(None)
        }
    }

    fn get_u64_property(
        &self,
        property: &Property,
        flow_file: Option<&Self::FlowFile>,
    ) -> Result<Option<u64>, MinifiError> {
        if property.validator != U64Validator {
            return Err(MinifiError::validation_err(format!(
                "to use get_u64_property {:?} must have U64Validator",
                property
            )));
        }
        if let Some(property_val) = self.get_property(property, flow_file)? {
            Ok(Some(u64::from_str(&property_val)?))
        } else {
            Ok(None)
        }
    }

    fn get_raw_controller_service<Cs>(
        &self,
        property: &Property,
    ) -> Result<Option<&Cs>, MinifiError>
    where
        Cs: RawControllerService + ComponentIdentifier + 'static;

    fn get_controller_service<Cs>(&self, property: &Property) -> Result<Option<&Cs>, MinifiError>
    where
        Cs: EnableControllerService + ComponentIdentifier + 'static;
}

impl<S> GetProperty for S
where
    S: ProcessContext,
{
    fn get_property(&self, property: &Property) -> Result<Option<String>, MinifiError> {
        self.get_property(property, None)
    }
}

impl<S> GetControllerService for S
where
    S: ProcessContext,
{
    fn get_controller_service<Cs>(&self, property: &Property) -> Result<Option<&Cs>, MinifiError>
    where
        Cs: EnableControllerService + ComponentIdentifier + 'static,
    {
        self.get_controller_service(property)
    }
}
