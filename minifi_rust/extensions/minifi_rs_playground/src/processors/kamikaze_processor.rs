// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

// This processor is used to test Errors and panic during schedule/trigger

mod properties;
mod relationships;

use crate::controller_services::lorem_ipsum_controller_service::LoremIpsumControllerService;
use crate::processors::kamikaze_processor::properties::{
    NOT_REGISTERED_PROPERTY, SCHEDULE_BEHAVIOUR, TRIGGER_BEHAVIOUR,
};
use minifi_native::macros::{ComponentIdentifier, PropertyType};
use minifi_native::{
    GetProperty, Logger, MinifiError, OnTriggerResult, ProcessContext, ProcessSession, Schedule,
    Trigger,
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
        let trigger_behaviour =
            context.get_req_property::<KamikazeBehaviour>(&TRIGGER_BEHAVIOUR)?;

        let schedule_behaviour =
            context.get_req_property::<KamikazeBehaviour>(&SCHEDULE_BEHAVIOUR)?;

        match schedule_behaviour {
            KamikazeBehaviour::ReturnErr => Err(MinifiError::schedule_err(
                "it was designed to fail during schedule",
            )),
            KamikazeBehaviour::ReturnOk => Ok(KamikazeProcessorRs { trigger_behaviour }),
            KamikazeBehaviour::GetNotRegisteredProperty => {
                let _ = context.get_property::<String>(&NOT_REGISTERED_PROPERTY)?;
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
    ) -> Result<OnTriggerResult, MinifiError>
    where
        PC: ProcessContext,
        PS: ProcessSession<FlowFile = PC::FlowFile>,
        L: Logger,
    {
        match self.trigger_behaviour {
            KamikazeBehaviour::ReturnErr => Err(MinifiError::trigger_err(
                "it was designed to fail in trigger",
            )),
            KamikazeBehaviour::ReturnOk => Ok(OnTriggerResult::Ok),
            KamikazeBehaviour::Panic => {
                panic!("KamikazeProcessor::trigger panic")
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

pub(crate) mod processor_definition;

#[cfg(test)]
mod tests;
