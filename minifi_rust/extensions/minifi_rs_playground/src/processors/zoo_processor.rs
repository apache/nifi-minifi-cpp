use crate::controller_services::animal_controller_apis::{
    CanFlyControllerApi, NumberOfLegsControllerApi,
};
use minifi_native::ControllerServiceApi;
use minifi_native::macros::ComponentIdentifier;
use minifi_native::{
    GetProperty, Logger, MinifiError, OnTriggerResult, OutputAttribute, ProcessContext,
    ProcessSession, ProcessorDefinition, ProcessorInputRequirement, Property, Relationship,
    Schedule, StandardPropertyValidator, Trigger, critical, info,
};

pub(crate) const CAN_FLY_SERVICE: Property = Property {
    name: "Can fly service",
    description: "Test CanFlyService",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: None,
    validator: StandardPropertyValidator::AlwaysValidValidator,
    allowed_values: &[],
    allowed_type: Some(<dyn CanFlyControllerApi>::INTERFACE_NAME),
};

pub(crate) const NUMBER_OF_LEGS: Property = Property {
    name: "Number of Legs Service",
    description: "Test NumberOfLegsService",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: None,
    validator: StandardPropertyValidator::AlwaysValidValidator,
    allowed_values: &[],
    allowed_type: Some(<dyn NumberOfLegsControllerApi>::INTERFACE_NAME),
};

#[derive(Debug, ComponentIdentifier)]
pub(crate) struct ZooProcessor {}

impl Schedule for ZooProcessor {
    fn schedule<Ctx: GetProperty, L: Logger>(
        _context: &Ctx,
        _logger: &L,
    ) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        Ok(Self {})
    }
}

impl Trigger for ZooProcessor {
    fn trigger<Context, Session, Lggr>(
        &self,
        context: &mut Context,
        _session: &mut Session,
        logger: &Lggr,
    ) -> Result<OnTriggerResult, MinifiError>
    where
        Context: ProcessContext,
        Session: ProcessSession<FlowFile = Context::FlowFile>,
        Lggr: Logger,
    {
        info!(logger, "{:?}", self);
        if let Some(maybe_flyer) =
            context.get_controller_service_api::<dyn CanFlyControllerApi>(&CAN_FLY_SERVICE)?
        {
            critical!(
                logger,
                "Can {:?} fly? {}",
                maybe_flyer,
                maybe_flyer.can_fly()
            );
        }
        if let Some(legged) =
            context.get_controller_service_api::<dyn NumberOfLegsControllerApi>(&NUMBER_OF_LEGS)?
        {
            critical!(logger, "{:?} has {} legs", legged, legged.number_of_legs());
        }
        Ok(OnTriggerResult::Ok)
    }
}

impl ProcessorDefinition for ZooProcessor {
    const DESCRIPTION: &'static str = "Test ZooProcessor";
    const INPUT_REQUIREMENT: ProcessorInputRequirement = ProcessorInputRequirement::Forbidden;
    const SUPPORTS_DYNAMIC_PROPERTIES: bool = false;
    const SUPPORTS_DYNAMIC_RELATIONSHIPS: bool = false;
    const OUTPUT_ATTRIBUTES: &'static [OutputAttribute] = &[];
    const RELATIONSHIPS: &'static [Relationship] = &[];
    const PROPERTIES: &'static [Property] = &[CAN_FLY_SERVICE, NUMBER_OF_LEGS];
}
