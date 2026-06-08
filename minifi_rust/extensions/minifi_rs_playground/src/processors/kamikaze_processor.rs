mod properties;
mod relationships;

use crate::controller_services::lorem_ipsum_controller_service::LoremIpsumControllerService;
use crate::processors::kamikaze_processor::properties::NOT_REGISTERED_PROPERTY;
use minifi_native::macros::{ComponentIdentifier, NoAdvancedProcessorFeatures};
use minifi_native::{
    CalculateMetrics, GetProperty, Logger, MinifiError, OnTriggerResult, ProcessContext,
    ProcessSession, Schedule, Trigger,
};
use strum_macros::{Display, EnumString, IntoStaticStr, VariantNames};

#[derive(Debug, Clone, Copy, PartialEq, Display, EnumString, VariantNames, IntoStaticStr)]
#[strum(serialize_all = "PascalCase", const_into_str)]
enum KamikazeBehaviour {
    ReturnErr,
    ReturnOk,
    GetNotRegisteredProperty,
    GetInvalidControllerService,
    Panic,
}

#[derive(Debug, ComponentIdentifier, NoAdvancedProcessorFeatures)]
pub(crate) struct KamikazeProcessorRs {
    on_trigger_behaviour: KamikazeBehaviour,
}

impl Schedule for KamikazeProcessorRs {
    fn schedule<P: GetProperty, L: Logger>(context: &P, _logger: &L) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        let on_trigger_behaviour = context
            .get_property(&properties::ON_TRIGGER_BEHAVIOUR)?
            .expect("required property")
            .parse::<KamikazeBehaviour>()?;

        let on_schedule_behaviour = context
            .get_property(&properties::ON_SCHEDULE_BEHAVIOUR)?
            .expect("required property")
            .parse::<KamikazeBehaviour>()?;

        match on_schedule_behaviour {
            KamikazeBehaviour::ReturnErr => Err(MinifiError::schedule_err(
                "it was designed to fail during schedule",
            )),
            KamikazeBehaviour::ReturnOk => Ok(KamikazeProcessorRs {
                on_trigger_behaviour,
            }),
            KamikazeBehaviour::GetNotRegisteredProperty => {
                let _ = context.get_property(&NOT_REGISTERED_PROPERTY)?;
                Ok(KamikazeProcessorRs {
                    on_trigger_behaviour,
                })
            }
            KamikazeBehaviour::Panic => {
                panic!("KamikazeProcessor::on_schedule panic")
            }
            KamikazeBehaviour::GetInvalidControllerService => {
                unimplemented!("KamikazeProcessor::get_invalid_controller_service");
            }
        }
    }
}

impl Trigger for KamikazeProcessorRs {
    fn trigger<PC, PS, L>(
        &self,
        context: &mut PC,
        _session: &mut PS,
        _logger: &L,
    ) -> Result<OnTriggerResult, MinifiError>
    where
        PC: ProcessContext,
        PS: ProcessSession<FlowFile = PC::FlowFile>,
        L: Logger,
    {
        match self.on_trigger_behaviour {
            KamikazeBehaviour::ReturnErr => Err(MinifiError::trigger_err(
                "it was designed to fail in trigger",
            )),
            KamikazeBehaviour::ReturnOk => Ok(OnTriggerResult::Ok),
            KamikazeBehaviour::Panic => {
                panic!("KamikazeProcessor::on_trigger panic")
            }
            KamikazeBehaviour::GetNotRegisteredProperty => {
                let _ = context.get_property(&NOT_REGISTERED_PROPERTY, None)?;
                Ok(OnTriggerResult::Ok)
            }
            KamikazeBehaviour::GetInvalidControllerService => {
                let _ = context.get_controller_service::<LoremIpsumControllerService>(
                    &NOT_REGISTERED_PROPERTY,
                )?;
                Ok(OnTriggerResult::Ok)
            }
        }
    }
}

impl CalculateMetrics for KamikazeProcessorRs {}

pub(crate) mod processor_definition;

#[cfg(test)]
mod tests;
