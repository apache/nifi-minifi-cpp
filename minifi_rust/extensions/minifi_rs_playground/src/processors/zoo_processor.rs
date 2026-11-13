// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

use crate::controller_services::animal_controller_apis::{
    CanFlyControllerApi, NumberOfLegsControllerApi,
};
use minifi_native::macros::ComponentIdentifier;
use minifi_native::{
    GetProperty, Logger, MinifiError, OnTriggerResult, OutputAttribute, ProcessContext,
    ProcessError, ProcessSession, ProcessorDefinition, ProcessorInputRequirement, Property,
    PropertyDefinition, Relationship, Schedule, Trigger, critical, info, property_definitions,
};

pub(crate) const CAN_FLY_SERVICE: Property<dyn CanFlyControllerApi> =
    Property::new("Can fly service", "Test CanFlyService");

pub(crate) const NUMBER_OF_LEGS: Property<dyn NumberOfLegsControllerApi> =
    Property::new("Number of Legs service", "Test NumberOfLegsService");

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
    ) -> Result<OnTriggerResult, ProcessError>
    where
        Context: ProcessContext,
        Session: ProcessSession<FlowFile = Context::FlowFile>,
        Lggr: Logger,
    {
        info!(logger, "{:?}", self);
        if let Some(maybe_flyer) = context.get_controller_service_api(&CAN_FLY_SERVICE)? {
            critical!(
                logger,
                "Can {:?} fly? {}",
                maybe_flyer,
                maybe_flyer.can_fly()
            );
        }
        if let Some(legged) = context.get_controller_service_api(&NUMBER_OF_LEGS)? {
            critical!(logger, "{:?} has {} legs", legged, legged.number_of_legs());
        }
        Ok(OnTriggerResult::Ok)
    }
}

impl ProcessorDefinition for ZooProcessorRs {
    const DESCRIPTION: &'static str = "RUST TEST PROCESSOR: ZooProcessorRs";
    const INPUT_REQUIREMENT: ProcessorInputRequirement = ProcessorInputRequirement::Forbidden;
    const SUPPORTS_DYNAMIC_PROPERTIES: bool = false;
    const SUPPORTS_DYNAMIC_RELATIONSHIPS: bool = false;
    const OUTPUT_ATTRIBUTES: &'static [OutputAttribute] = &[];
    const RELATIONSHIPS: &'static [Relationship] = &[];
    const PROPERTIES: &'static [PropertyDefinition] =
        property_definitions![CAN_FLY_SERVICE, NUMBER_OF_LEGS];
}
