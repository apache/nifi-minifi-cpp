mod properties;

use crate::controller_services::lorem_ipsum_controller_service::properties::LENGTH;
use lipsum::lipsum;
use minifi_native::macros::ComponentIdentifier;
use minifi_native::{
    ControllerServiceDefinition, EnableControllerService, GetProperty, Logger, MinifiError,
    Property,
};

#[derive(Debug, ComponentIdentifier)]
pub(crate) struct LoremIpsumControllerService {
    pub data: String,
}

impl EnableControllerService for LoremIpsumControllerService {
    fn enable<P: GetProperty, L: Logger>(context: &P, _logger: &L) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        let length = context
            .get_u64_property(&LENGTH)?
            .ok_or(MinifiError::missing_required_property("Length is required"))?;

        let data = lipsum(length as usize);
        Ok(Self { data })
    }
}

impl ControllerServiceDefinition for LoremIpsumControllerService {
    const DESCRIPTION: &'static str = "Simple Rusty Controller Service to test API";
    const PROPERTIES: &'static [Property] = &[LENGTH];
}
