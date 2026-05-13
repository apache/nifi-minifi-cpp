use super::c_ffi_primitives::StaticStrAsMinifiCStr;
use crate::{Property, StandardPropertyValidator};
use minifi_native_sys::{
    MinifiPropertyDefinition, MinifiStringView, MinifiValidator,
    MinifiValidator_MINIFI_VALIDATOR_ALWAYS_VALID, MinifiValidator_MINIFI_VALIDATOR_BOOLEAN,
    MinifiValidator_MINIFI_VALIDATOR_DATA_SIZE, MinifiValidator_MINIFI_VALIDATOR_INTEGER,
    MinifiValidator_MINIFI_VALIDATOR_NON_BLANK, MinifiValidator_MINIFI_VALIDATOR_PORT,
    MinifiValidator_MINIFI_VALIDATOR_TIME_PERIOD,
    MinifiValidator_MINIFI_VALIDATOR_UNSIGNED_INTEGER,
};
use std::ptr;

#[allow(dead_code)] // these c_ vecs are holding the values referenced from the properties, so they live long enough for registration
pub struct CProperties {
    c_default_values: Vec<MinifiStringView>,
    c_allowed_values: Vec<Vec<MinifiStringView>>,
    c_allowed_types: Vec<MinifiStringView>,
    properties: Vec<MinifiPropertyDefinition>,
}

impl CProperties {
    pub(crate) fn new(
        c_default_values: Vec<MinifiStringView>,
        c_allowed_values: Vec<Vec<MinifiStringView>>,
        c_allowed_types: Vec<MinifiStringView>,
        properties: Vec<MinifiPropertyDefinition>,
    ) -> Self {
        Self {
            c_default_values,
            c_allowed_values,
            c_allowed_types,
            properties,
        }
    }

    pub(crate) fn len(&self) -> usize {
        self.properties.len()
    }

    pub(crate) unsafe fn get_ptr(&self) -> *const MinifiPropertyDefinition {
        self.properties.as_ptr()
    }
}

impl Property {
    fn create_c_default_value_holder(properties: &[Self]) -> Vec<MinifiStringView> {
        properties
            .iter()
            .map(|p| match p.default_value {
                Some(dv) => dv.as_minifi_c_type(),
                None => MinifiStringView {
                    data: ptr::null(),
                    length: 0,
                },
            })
            .collect()
    }

    fn create_c_allowed_values_vec_vec(properties: &[Self]) -> Vec<Vec<MinifiStringView>> {
        properties
            .iter()
            .map(|p| {
                p.allowed_values
                    .iter()
                    .map(|av| av.as_minifi_c_type())
                    .collect()
            })
            .collect()
    }

    fn create_c_allowed_types_vec(properties: &[Self]) -> Vec<MinifiStringView> {
        properties
            .iter()
            .map(|p| p.allowed_type.as_minifi_c_type())
            .collect()
    }

    pub(crate) fn create_c_properties(properties: &[Self]) -> CProperties {
        let c_default_values = Property::create_c_default_value_holder(properties);
        let c_allowed_values = Property::create_c_allowed_values_vec_vec(properties);
        let c_allowed_types = Property::create_c_allowed_types_vec(properties);
        assert_eq!(c_default_values.len(), properties.len());
        assert_eq!(c_allowed_values.len(), properties.len());
        assert_eq!(c_allowed_types.len(), properties.len());

        let c_properties = properties
            .iter()
            .zip(c_default_values.iter())
            .zip(c_allowed_values.iter())
            .zip(c_allowed_types.iter())
            .map(|(((property, def_value), allowed_values), allowed_type)| {
                MinifiPropertyDefinition {
                    name: property.name.as_minifi_c_type(),
                    display_name: property.name.as_minifi_c_type(),
                    description: property.description.as_minifi_c_type(),
                    is_required: property.is_required,
                    is_sensitive: property.is_sensitive,
                    default_value: def_value,
                    allowed_values_count: allowed_values.len(),
                    allowed_values_ptr: allowed_values.as_ptr(),
                    validator: property.validator.as_minifi_c_type(),
                    type_: allowed_type,
                    supports_expression_language: property.supports_expr_lang,
                }
            })
            .collect();
        CProperties::new(
            c_default_values,
            c_allowed_values,
            c_allowed_types,
            c_properties,
        )
    }
}

impl StandardPropertyValidator {
    pub(crate) fn as_minifi_c_type(&self) -> MinifiValidator {
        match self {
            StandardPropertyValidator::AlwaysValidValidator => {
                MinifiValidator_MINIFI_VALIDATOR_ALWAYS_VALID
            }
            StandardPropertyValidator::NonBlankValidator => {
                MinifiValidator_MINIFI_VALIDATOR_NON_BLANK
            }
            StandardPropertyValidator::TimePeriodValidator => {
                MinifiValidator_MINIFI_VALIDATOR_TIME_PERIOD
            }
            StandardPropertyValidator::BoolValidator => MinifiValidator_MINIFI_VALIDATOR_BOOLEAN,
            StandardPropertyValidator::I64Validator => MinifiValidator_MINIFI_VALIDATOR_INTEGER,
            StandardPropertyValidator::U64Validator => {
                MinifiValidator_MINIFI_VALIDATOR_UNSIGNED_INTEGER
            }
            StandardPropertyValidator::DataSizeValidator => {
                MinifiValidator_MINIFI_VALIDATOR_DATA_SIZE
            }
            StandardPropertyValidator::PortValidator => MinifiValidator_MINIFI_VALIDATOR_PORT,
        }
    }
}
