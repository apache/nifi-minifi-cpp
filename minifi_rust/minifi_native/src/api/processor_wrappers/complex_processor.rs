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

use crate::api::raw_processor::{MultiThreadedTrigger, SingleThreadedTrigger};
use crate::{
    ComponentIdentifier, Logger, MinifiError, MultiThreaded, OnTriggerResult, ProcessContext,
    ProcessError, ProcessSession, Processor, ProcessorDefinition, Schedule, SingleThreaded,
};

pub trait MutTrigger {
    fn trigger<Ctx, Session, Lggr>(
        &mut self,
        context: &mut Ctx,
        session: &mut Session,
        logger: &Lggr,
    ) -> Result<OnTriggerResult, ProcessError>
    where
        Ctx: ProcessContext,
        Session: ProcessSession<FlowFile = Ctx::FlowFile>,
        Lggr: Logger;
}

pub trait Trigger {
    fn trigger<Context, Session, Lggr>(
        &self,
        context: &mut Context,
        session: &mut Session,
        logger: &Lggr,
    ) -> Result<OnTriggerResult, ProcessError>
    where
        Context: ProcessContext,
        Session: ProcessSession<FlowFile = Context::FlowFile>,
        Lggr: Logger;
}

pub struct ComplexProcessorType {}

impl<Implementation, L> SingleThreadedTrigger
    for Processor<Implementation, ComplexProcessorType, SingleThreaded, L>
where
    Implementation: Schedule + MutTrigger + ComponentIdentifier + ProcessorDefinition,
    L: Logger,
{
    fn trigger<PC, PS>(
        &mut self,
        context: &mut PC,
        session: &mut PS,
    ) -> Result<OnTriggerResult, ProcessError>
    where
        PC: ProcessContext,
        PS: ProcessSession<FlowFile = PC::FlowFile>,
    {
        if let Some(ref mut scheduled_impl) = self.scheduled_impl {
            scheduled_impl.trigger(context, session, &self.logger)
        } else {
            Err(MinifiError::UnscheduledProcessor.into())
        }
    }
}

impl<Implementation, L> MultiThreadedTrigger
    for Processor<Implementation, ComplexProcessorType, MultiThreaded, L>
where
    Implementation: Schedule + Trigger + ComponentIdentifier + ProcessorDefinition,
    L: Logger,
{
    fn trigger<PC, PS>(
        &self,
        context: &mut PC,
        session: &mut PS,
    ) -> Result<OnTriggerResult, ProcessError>
    where
        PC: ProcessContext,
        PS: ProcessSession<FlowFile = PC::FlowFile>,
    {
        if let Some(ref scheduled_impl) = self.scheduled_impl {
            scheduled_impl.trigger(context, session, &self.logger)
        } else {
            Err(MinifiError::UnscheduledProcessor.into())
        }
    }
}
