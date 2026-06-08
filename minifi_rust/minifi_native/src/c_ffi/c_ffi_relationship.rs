use super::c_ffi_primitives::StaticStrAsMinifiCStr;
use crate::Relationship;
use minifi_native_sys::MinifiRelationshipDefinition;

impl Relationship {
    pub(crate) fn create_c_vec(relationships: &[Self]) -> Vec<MinifiRelationshipDefinition> {
        relationships
            .iter()
            .map(|r| MinifiRelationshipDefinition {
                name: r.name.as_minifi_c_type(),
                description: r.description.as_minifi_c_type(),
            })
            .collect()
    }
}
