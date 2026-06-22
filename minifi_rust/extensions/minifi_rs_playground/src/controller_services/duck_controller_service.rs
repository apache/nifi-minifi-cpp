use minifi_native::ControllerServiceApi;
use minifi_native::{create_provided_interface, ControllerServiceDefinition, EnableControllerService, GetProperty, Logger, MinifiError, Property, ProvidedInterface};
use minifi_native::macros::ComponentIdentifier;
use crate::controller_services::animal_controller_apis::{CanFlyControllerApi, NumberOfLegsControllerApi};

#[derive(Debug, ComponentIdentifier)]
pub(crate) struct DuckController {
    
}


impl NumberOfLegsControllerApi for DuckController {
    fn number_of_legs(&self) -> u8 {
        2
    }
}

impl CanFlyControllerApi for DuckController {
    fn can_fly(&self) -> bool {
        true
    }
}

impl EnableControllerService for DuckController {
    fn enable<Ctx: GetProperty, L: Logger>(_context: &Ctx, _logger: &L) -> Result<Self, MinifiError>
    where
        Self: Sized
    {
        Ok(Self{})
    }
}

impl ControllerServiceDefinition for DuckController {
    const DESCRIPTION: &'static str = "Test DuckController";
    const PROPERTIES: &'static [Property] = &[];
    const PROVIDED_APIS: &'static [ProvidedInterface<Self>] = &[
        create_provided_interface!(dyn CanFlyControllerApi),
        create_provided_interface!(dyn NumberOfLegsControllerApi),
    ];
}