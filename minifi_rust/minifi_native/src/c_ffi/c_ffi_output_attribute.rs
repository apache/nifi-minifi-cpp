use crate::OutputAttribute;
use crate::c_ffi::StaticStrAsMinifiCStr;
use minifi_native_sys::{minifi_string_view, minifi_output_attribute_definition};

#[allow(dead_code)] // the c_ vecs are holding the values referenced from the output attributes
pub struct COutputAttributes {
    c_relationship_names: Vec<Vec<minifi_string_view>>,
    c_output_attributes: Vec<minifi_output_attribute_definition>,
}

impl COutputAttributes {
    pub(crate) fn new(output_attributes: &[OutputAttribute]) -> Self {
        let mut c_relationship_names = Vec::new();
        let mut c_output_attributes = Vec::new();
        for output_attribute in output_attributes {
            let mut output_attribute_relationships = Vec::new();
            for relationship in output_attribute.relationships {
                output_attribute_relationships.push(relationship.as_minifi_c_type());
            }
            c_output_attributes.push(minifi_output_attribute_definition {
                name: output_attribute.name.as_minifi_c_type(),
                relationships_count: output_attribute_relationships.len(),
                relationships_ptr: output_attribute_relationships.as_ptr(),
                description: output_attribute.description.as_minifi_c_type(),
            });
            c_relationship_names.push(output_attribute_relationships);
        }
        COutputAttributes {
            c_relationship_names,
            c_output_attributes,
        }
    }

    pub(crate) fn len(&self) -> usize {
        self.c_output_attributes.len()
    }

    pub(crate) unsafe fn get_ptr(&self) -> *const minifi_output_attribute_definition {
        self.c_output_attributes.as_ptr()
    }
}
