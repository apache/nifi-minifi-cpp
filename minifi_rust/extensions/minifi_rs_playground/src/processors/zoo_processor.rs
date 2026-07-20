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
    name: "Number of Legs service",
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
pub(crate) struct ZooProcessorRs {}

impl Schedule for ZooProcessorRs {
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

impl Trigger for ZooProcessorRs {
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

impl ProcessorDefinition for ZooProcessorRs {
    const DESCRIPTION: &'static str = "Test ZooProcessorRs";
    const INPUT_REQUIREMENT: ProcessorInputRequirement = ProcessorInputRequirement::Forbidden;
    const SUPPORTS_DYNAMIC_PROPERTIES: bool = false;
    const SUPPORTS_DYNAMIC_RELATIONSHIPS: bool = false;
    const OUTPUT_ATTRIBUTES: &'static [OutputAttribute] = &[];
    const RELATIONSHIPS: &'static [Relationship] = &[];
    const PROPERTIES: &'static [Property] = &[CAN_FLY_SERVICE, NUMBER_OF_LEGS];
}
