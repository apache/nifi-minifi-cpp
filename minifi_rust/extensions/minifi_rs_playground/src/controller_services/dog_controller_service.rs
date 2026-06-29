use crate::controller_services::animal_controller_apis::{
    CanFlyControllerApi, NumberOfLegsControllerApi,
};
use minifi_native::ControllerServiceApi;
use minifi_native::macros::ComponentIdentifier;
use minifi_native::{
    ControllerServiceDefinition, EnableControllerService, GetProperty, Logger, MinifiError,
    Property, ProvidedInterface, StandardPropertyValidator, create_provided_interface,
};

pub(crate) const HAS_JETPACK: Property = Property {
    name: "Has Jetpack",
    description: "Whether or not the dog has a jetpack",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some("false"),
    validator: StandardPropertyValidator::BoolValidator,
    allowed_values: &[],
    allowed_type: None,
};

pub(crate) const EXTRA_INFO: Property = Property {
    name: "Extra information",
    description: "We need this to verify the casting was done correctly",
    is_required: false,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: None,
    validator: StandardPropertyValidator::AlwaysValidValidator,
    allowed_values: &[],
    allowed_type: None,
};

#[allow(dead_code)] // extra_info is only used by {:?}
#[derive(Debug, ComponentIdentifier)]
pub(crate) struct DogControllerRs {
    has_jetpack: bool,
    extra_info: String,
}

impl NumberOfLegsControllerApi for DogControllerRs {
    fn number_of_legs(&self) -> u8 {
        4
    }
}

impl CanFlyControllerApi for DogControllerRs {
    fn can_fly(&self) -> bool {
        self.has_jetpack
    }
}

impl EnableControllerService for DogControllerRs {
    fn enable<Ctx: GetProperty, L: Logger>(context: &Ctx, _logger: &L) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        let has_jetpack = context.get_bool_property(&HAS_JETPACK)?.ok_or(
            MinifiError::missing_required_property("Has jetpack is required"),
        )?;

        let extra_info = context.get_property(&EXTRA_INFO)?.unwrap_or("".into());

        Ok(Self {
            has_jetpack,
            extra_info,
        })
    }
}

impl ControllerServiceDefinition for DogControllerRs {
    const DESCRIPTION: &'static str = "Test DogControllerRs";
    const PROPERTIES: &'static [Property] = &[HAS_JETPACK, EXTRA_INFO];
    const PROVIDED_APIS: &'static [ProvidedInterface<Self>] = &[
        create_provided_interface!(dyn CanFlyControllerApi),
        create_provided_interface!(dyn NumberOfLegsControllerApi),
    ];
}
