use crate::controller_services::animal_controller_apis::{
    CanFlyControllerApi, NumberOfLegsControllerApi,
};
use minifi_native::ControllerServiceApi;
use minifi_native::macros::ComponentIdentifier;
use minifi_native::{
    ControllerServiceDefinition, EnableControllerService, GetProperty, Logger, MinifiError,
    Property, ProvidedInterface, create_provided_interface,
};

#[derive(Debug, ComponentIdentifier)]
pub(crate) struct DuckControllerRs {}

impl NumberOfLegsControllerApi for DuckControllerRs {
    fn number_of_legs(&self) -> u8 {
        2
    }
}

impl CanFlyControllerApi for DuckControllerRs {
    fn can_fly(&self) -> bool {
        true
    }
}

impl EnableControllerService for DuckControllerRs {
    fn enable<Ctx: GetProperty, L: Logger>(_context: &Ctx, _logger: &L) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        Ok(Self {})
    }
}

impl ControllerServiceDefinition for DuckControllerRs {
    const DESCRIPTION: &'static str = "Test DuckControllerRs";
    const PROPERTIES: &'static [Property] = &[];
    const PROVIDED_APIS: &'static [ProvidedInterface<Self>] = &[
        create_provided_interface!(dyn CanFlyControllerApi),
        create_provided_interface!(dyn NumberOfLegsControllerApi),
    ];
}
