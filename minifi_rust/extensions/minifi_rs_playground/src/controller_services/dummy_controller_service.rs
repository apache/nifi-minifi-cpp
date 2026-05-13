use minifi_native::macros::ComponentIdentifier;
use minifi_native::{
    ControllerServiceDefinition, EnableControllerService, GetProperty, Logger, MinifiError,
    Property,
};

#[derive(Debug, ComponentIdentifier)]
pub(crate) struct DummyControllerService {}

impl EnableControllerService for DummyControllerService {
    fn enable<Ctx: GetProperty, L: Logger>(_context: &Ctx, _logger: &L) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        Ok(Self {})
    }
}

impl ControllerServiceDefinition for DummyControllerService {
    const DESCRIPTION: &'static str = "Dummy Controller Service";
    const PROPERTIES: &'static [Property] = &[];
}
