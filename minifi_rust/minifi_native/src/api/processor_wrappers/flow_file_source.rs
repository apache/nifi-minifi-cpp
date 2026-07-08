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

use crate::api::processor_wrappers::utils::flow_file_content::Content;
use crate::api::raw_processor::{MultiThreadedTrigger, SingleThreadedTrigger};
use crate::{FlowFileAttribute, impl_with_attributes};
use crate::{
    GetControllerService, GetProperty, Logger, MinifiError, MultiThreaded, OnTriggerResult,
    ProcessContext, ProcessError, ProcessSession, Processor, Relationship, Schedule,
    SingleThreaded,
};
use std::borrow::Cow;

pub struct GeneratedFlowFile<'a> {
    target_relationship_name: Cow<'static, str>,
    new_content: Option<Content<'a>>,
    attributes_to_add: Vec<FlowFileAttribute>,
}

impl<'a> GeneratedFlowFile<'a> {
    pub fn new(target_relationship: &'a Relationship, new_content: Option<Content<'a>>) -> Self {
        Self {
            target_relationship_name: Cow::Borrowed(target_relationship.name),
            new_content,
            attributes_to_add: Vec::new(),
        }
    }

    pub fn target_relationship_name(&self) -> &str {
        &self.target_relationship_name
    }
}

impl_with_attributes!(GeneratedFlowFile<'a>);

pub trait FlowFileSource {
    fn generate<'a, Context: GetProperty + GetControllerService, LoggerImpl: Logger>(
        &self,
        context: &'a mut Context,
        logger: &LoggerImpl,
    ) -> Result<Vec<GeneratedFlowFile<'a>>, ProcessError>;
}

pub trait MutFlowFileSource {
    fn generate<'a, Context: GetProperty + GetControllerService, LoggerImpl: Logger>(
        &mut self,
        context: &'a mut Context,
        logger: &LoggerImpl,
    ) -> Result<Vec<GeneratedFlowFile<'a>>, ProcessError>;
}

fn handle_generated_flow_files<PC, PS>(
    session: &mut PS,
    generated_flow_files: Vec<GeneratedFlowFile>,
) -> Result<OnTriggerResult, ProcessError>
where
    PC: ProcessContext,
    PS: ProcessSession<FlowFile = PC::FlowFile>,
{
    if generated_flow_files.is_empty() {
        return Ok(OnTriggerResult::Yield);
    }

    for new_flow_file_data in generated_flow_files {
        let mut ff = session.create()?;
        match new_flow_file_data.new_content {
            None => {}
            Some(Content::Buffer(buffer)) => session.write(&ff, &buffer)?,
            Some(Content::Stream(stream)) => session.write_from_stream(&ff, stream)?,
        }
        for (k, v) in &new_flow_file_data.attributes_to_add {
            session.set_attribute(&mut ff, k, v)?;
        }
        session.transfer(ff, new_flow_file_data.target_relationship_name.as_ref())?;
    }
    Ok(OnTriggerResult::Ok)
}

pub struct FlowFileSourceProcessorType {}

impl<Implementation, L> MultiThreadedTrigger
    for Processor<Implementation, FlowFileSourceProcessorType, MultiThreaded, L>
where
    Implementation: Schedule + FlowFileSource,
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
            let files = scheduled_impl.generate(context, &self.logger)?;
            handle_generated_flow_files::<PC, PS>(session, files)
        } else {
            Err(MinifiError::UnscheduledProcessor.into())
        }
    }
}

impl<Implementation, L> SingleThreadedTrigger
    for Processor<Implementation, FlowFileSourceProcessorType, SingleThreaded, L>
where
    Implementation: Schedule + MutFlowFileSource,
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
            let files = scheduled_impl.generate(context, &self.logger)?;
            handle_generated_flow_files::<PC, PS>(session, files)
        } else {
            Err(MinifiError::UnscheduledProcessor.into())
        }
    }
}
