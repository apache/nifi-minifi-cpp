use super::c_ffi_primitives::StaticStrAsMinifiCStr;
use crate::{Property, StandardPropertyValidator};
use minifi_native_sys::{minifi_property_definition, minifi_validator, minifi_string_view, minifi_validator_MINIFI_VALIDATOR_ALWAYS_VALID, minifi_validator_MINIFI_VALIDATOR_BOOLEAN, minifi_validator_MINIFI_VALIDATOR_DATA_SIZE, minifi_validator_MINIFI_VALIDATOR_INTEGER, minifi_validator_MINIFI_VALIDATOR_NON_BLANK, minifi_validator_MINIFI_VALIDATOR_PORT, minifi_validator_MINIFI_VALIDATOR_TIME_PERIOD, minifi_validator_MINIFI_VALIDATOR_UNSIGNED_INTEGER};
use std::ptr;

#[allow(dead_code)] // these c_ vecs are holding the values referenced from the properties, so they live long enough for registration
pub struct CProperties {
    c_default_values: Vec<minifi_string_view>,
    c_allowed_values: Vec<Vec<minifi_string_view>>,
    c_allowed_types: Vec<minifi_string_view>,
    properties: Vec<minifi_property_definition>,
}

impl CProperties {
    pub(crate) fn new(
        c_default_values: Vec<minifi_string_view>,
        c_allowed_values: Vec<Vec<minifi_string_view>>,
        c_allowed_types: Vec<minifi_string_view>,
        properties: Vec<minifi_property_definition>,
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

    pub(crate) unsafe fn get_ptr(&self) -> *const minifi_property_definition {
        self.properties.as_ptr()
    }
}

impl Property {
    fn create_c_default_value_holder(properties: &[Self]) -> Vec<minifi_string_view> {
        properties
            .iter()
            .map(|p| match p.default_value {
                Some(dv) => dv.as_minifi_c_type(),
                None => minifi_string_view {
                    data: ptr::null(),
                    length: 0,
                },
            })
            .collect()
    }

    fn create_c_allowed_values_vec_vec(properties: &[Self]) -> Vec<Vec<minifi_string_view>> {
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

    fn create_c_allowed_types_vec(properties: &[Self]) -> Vec<minifi_string_view> {
        properties
            .iter()
            .map(|p| match p.allowed_type {
                Some(dv) => dv.as_minifi_c_type(),
                None => minifi_string_view {
                    data: ptr::null(),
                    length: 0,
                },
            })
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
                minifi_property_definition {
                    name: property.name.as_minifi_c_type(),
                    display_name: property.name.as_minifi_c_type(),
                    description: property.description.as_minifi_c_type(),
                    is_required: property.is_required,
                    is_sensitive: property.is_sensitive,
                    default_value: if def_value.data == std::ptr::null() { std::ptr::null() } else { def_value },
                    allowed_values_count: allowed_values.len(),
                    allowed_values_ptr: allowed_values.as_ptr(),
                    validator: property.validator.as_minifi_c_type(),
                    allowed_type: if allowed_type.data == std::ptr::null() { std::ptr::null() } else { allowed_type },
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
    pub(crate) fn as_minifi_c_type(&self) -> minifi_validator {
        match self {
            StandardPropertyValidator::AlwaysValidValidator => {
                minifi_validator_MINIFI_VALIDATOR_ALWAYS_VALID
            }
            StandardPropertyValidator::NonBlankValidator => {
                minifi_validator_MINIFI_VALIDATOR_NON_BLANK
            }
            StandardPropertyValidator::TimePeriodValidator => {
                minifi_validator_MINIFI_VALIDATOR_TIME_PERIOD
            }
            StandardPropertyValidator::BoolValidator => minifi_validator_MINIFI_VALIDATOR_BOOLEAN,
            StandardPropertyValidator::I64Validator => minifi_validator_MINIFI_VALIDATOR_INTEGER,
            StandardPropertyValidator::U64Validator => {
                minifi_validator_MINIFI_VALIDATOR_UNSIGNED_INTEGER
            }
            StandardPropertyValidator::DataSizeValidator => {
                minifi_validator_MINIFI_VALIDATOR_DATA_SIZE
            }
            StandardPropertyValidator::PortValidator => minifi_validator_MINIFI_VALIDATOR_PORT,
        }
    }
}
