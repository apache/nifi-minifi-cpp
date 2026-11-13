// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

// This processor is used to test Errors and panic during schedule/trigger

mod properties;
mod relationships;

use crate::processors::kamikaze_processor::properties::{
    NOT_REGISTERED_PROPERTY, SCHEDULE_BEHAVIOUR, TRIGGER_BEHAVIOUR, UNREGISTERED_CONTROLLER_SERVICE,
};
use minifi_native::macros::{ComponentIdentifier, PropertyType};
use minifi_native::{
    GetProperty, Logger, MinifiError, OnTriggerResult, ProcessContext, ProcessError,
    ProcessSession, Schedule, Trigger,
};
use strum_macros::{Display, EnumString, IntoStaticStr, VariantNames};

#[derive(
    Debug, Clone, Copy, PartialEq, Display, EnumString, VariantNames, IntoStaticStr, PropertyType,
)]
#[strum(serialize_all = "PascalCase", const_into_str)]
enum KamikazeBehaviour {
    ReturnErr,
    ReturnOk,
    GetNotRegisteredProperty,
    GetInvalidControllerService,
    Panic,
}

#[derive(Debug, ComponentIdentifier)]
pub(crate) struct KamikazeProcessorRs {
    trigger_behaviour: KamikazeBehaviour,
}

impl Schedule for KamikazeProcessorRs {
    fn schedule<P: GetProperty, L: Logger>(context: &P, _logger: &L) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        let trigger_behaviour = context.get_property(&TRIGGER_BEHAVIOUR)?;

        let schedule_behaviour = context.get_property(&SCHEDULE_BEHAVIOUR)?;

        match schedule_behaviour {
            KamikazeBehaviour::ReturnErr => Err(MinifiError::custom(
                "it was designed to fail during schedule",
            )),
            KamikazeBehaviour::ReturnOk => Ok(KamikazeProcessorRs { trigger_behaviour }),
            KamikazeBehaviour::GetNotRegisteredProperty => {
                let _ = context.get_property(&NOT_REGISTERED_PROPERTY)?;
                Ok(KamikazeProcessorRs { trigger_behaviour })
            }
            KamikazeBehaviour::Panic => {
                panic!("KamikazeProcessor::schedule panic")
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
    ) -> Result<OnTriggerResult, ProcessError>
    where
        PC: ProcessContext,
        PS: ProcessSession<FlowFile = PC::FlowFile>,
        L: Logger,
    {
        match self.trigger_behaviour {
            KamikazeBehaviour::ReturnErr => {
                Err(MinifiError::custom("it was designed to fail in trigger").into())
            }
            KamikazeBehaviour::ReturnOk => Ok(OnTriggerResult::Ok),
            KamikazeBehaviour::Panic => {
                panic!("KamikazeProcessor::trigger panic")
            }
            KamikazeBehaviour::GetNotRegisteredProperty => {
                let _ = context.get_raw_property(&NOT_REGISTERED_PROPERTY, None)?;
                Ok(OnTriggerResult::Ok)
            }
            KamikazeBehaviour::GetInvalidControllerService => {
                let _ = context.get_controller_service(&UNREGISTERED_CONTROLLER_SERVICE)?;
                Ok(OnTriggerResult::Ok)
            }
        }
    }
}

pub(crate) mod processor_definition;

#[cfg(test)]
mod tests;
