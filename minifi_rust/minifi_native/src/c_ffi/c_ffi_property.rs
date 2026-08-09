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

use super::c_ffi_primitives::StaticStrAsMinifiCStr;
use crate::api::property::PropertyConstraints;
use crate::{PropertyDefinition, StandardPropertyValidator};
use minifi_native_sys::{
    minifi_property_definition, minifi_string_view, minifi_validator,
    minifi_validator_MINIFI_VALIDATOR_ALWAYS_VALID, minifi_validator_MINIFI_VALIDATOR_BOOLEAN,
    minifi_validator_MINIFI_VALIDATOR_DATA_SIZE, minifi_validator_MINIFI_VALIDATOR_INTEGER,
    minifi_validator_MINIFI_VALIDATOR_NON_BLANK, minifi_validator_MINIFI_VALIDATOR_PORT,
    minifi_validator_MINIFI_VALIDATOR_TIME_PERIOD,
    minifi_validator_MINIFI_VALIDATOR_UNSIGNED_INTEGER,
    minifi_validator_MINIFI_VALIDATOR_NUMBER
};
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

impl PropertyDefinition {
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
            .map(|p| match p.constraints {
                Some(PropertyConstraints::AllowedValues(allowed_values)) => allowed_values
                    .iter()
                    .map(|av| av.as_minifi_c_type())
                    .collect(),
                _ => {
                    vec![]
                }
            })
            .collect()
    }

    fn create_c_allowed_types_vec(properties: &[Self]) -> Vec<minifi_string_view> {
        properties
            .iter()
            .map(|p| match p.constraints {
                Some(PropertyConstraints::ControllerService(allowed_type)) => {
                    allowed_type.as_minifi_c_type()
                }
                _ => minifi_string_view {
                    data: ptr::null(),
                    length: 0,
                },
            })
            .collect()
    }

    pub(crate) fn create_c_properties(properties: &[Self]) -> CProperties {
        let c_default_values = PropertyDefinition::create_c_default_value_holder(properties);
        let c_allowed_values = PropertyDefinition::create_c_allowed_values_vec_vec(properties);
        let c_allowed_types = PropertyDefinition::create_c_allowed_types_vec(properties);
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
                    default_value: if def_value.data.is_null() {
                        std::ptr::null()
                    } else {
                        def_value
                    },
                    allowed_values_count: allowed_values.len(),
                    allowed_values_ptr: allowed_values.as_ptr(),
                    validator: match &property.constraints {
                        Some(PropertyConstraints::Validator(s)) => s.as_minifi_c_type(),
                        _ => minifi_validator_MINIFI_VALIDATOR_ALWAYS_VALID,
                    },
                    allowed_type: if allowed_type.data.is_null() {
                        std::ptr::null()
                    } else {
                        allowed_type
                    },
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
            StandardPropertyValidator::F64Validator => minifi_validator_MINIFI_VALIDATOR_NUMBER,
        }
    }
}
