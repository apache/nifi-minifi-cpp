// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

use crate::api::raw_processor::{MultiThreadedTrigger, SingleThreadedTrigger};
use crate::{
    ComponentIdentifier, Logger, MinifiError, MultiThreaded, OnTriggerResult, Processor,
    ProcessorDefinition, Schedule, SingleThreaded, TriggerContext,
};

pub trait MutTrigger {
    fn trigger<TC, Lggr>(
        &mut self,
        context: &mut TC,
        logger: &Lggr,
    ) -> Result<OnTriggerResult, MinifiError>
    where
        TC: TriggerContext,
        Lggr: Logger;
}

pub trait Trigger {
    fn trigger<TC, Lggr>(
        &self,
        context: &mut TC,
        logger: &Lggr,
    ) -> Result<OnTriggerResult, MinifiError>
    where
        TC: TriggerContext,
        Lggr: Logger;
}

pub struct ComplexProcessorType {}

impl<Implementation, L> SingleThreadedTrigger
    for Processor<Implementation, ComplexProcessorType, SingleThreaded, L>
where
    Implementation: Schedule + MutTrigger + ComponentIdentifier + ProcessorDefinition,
    L: Logger,
{
    fn trigger<TC: TriggerContext>(
        &mut self,
        context: &mut TC,
    ) -> Result<OnTriggerResult, MinifiError> {
        if let Some(ref mut scheduled_impl) = self.scheduled_impl {
            scheduled_impl.trigger(context, &self.logger)
        } else {
            Err(MinifiError::UnscheduledProcessor)
        }
    }
}

impl<Implementation, L> MultiThreadedTrigger
    for Processor<Implementation, ComplexProcessorType, MultiThreaded, L>
where
    Implementation: Schedule + Trigger + ComponentIdentifier + ProcessorDefinition,
    L: Logger,
{
    fn trigger<TC: TriggerContext>(
        &self,
        context: &mut TC,
    ) -> Result<OnTriggerResult, MinifiError> {
        if let Some(ref scheduled_impl) = self.scheduled_impl {
            scheduled_impl.trigger(context, &self.logger)
        } else {
            Err(MinifiError::UnscheduledProcessor)
        }
    }
}
