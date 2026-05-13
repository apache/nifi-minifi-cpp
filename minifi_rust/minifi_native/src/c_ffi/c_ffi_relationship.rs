use super::c_ffi_primitives::StaticStrAsMinifiCStr;
use crate::Relationship;
use minifi_native_sys::minifi_relationship_definition;

impl Relationship {
    pub(crate) fn create_c_vec(relationships: &[Self]) -> Vec<minifi_relationship_definition> {
        relationships
            .iter()
            .map(|r| minifi_relationship_definition {
                name: r.name.as_minifi_c_type(),
                description: r.description.as_minifi_c_type(),
            })
            .collect()
    }
}
