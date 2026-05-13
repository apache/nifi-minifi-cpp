use crate::mock::mock_process_context::MockPropertyMap;
use crate::{GetProperty, MinifiError, Property};

pub struct MockControllerServiceContext {
    pub properties: MockPropertyMap,
}

impl GetProperty for MockControllerServiceContext {
    fn get_property(&self, property: &Property) -> Result<Option<String>, MinifiError> {
        self.properties.get_property(property, None)
    }
}

impl MockControllerServiceContext {
    pub fn new() -> Self {
        Self {
            properties: MockPropertyMap::new(),
        }
    }
}
