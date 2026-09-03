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

use crate::api::process_session::IoState;
use crate::api::processor_wrappers::utils::context_session_flowfile_bundle::ContextSessionFlowFileBundle;
use crate::api::raw_processor::{MultiThreadedTrigger, SingleThreadedTrigger};
use crate::{FlowFileAttribute, impl_with_attributes};
use crate::{
    GetAttribute, GetControllerService, GetProperty, InputStream, LogLevel, Logger, MinifiError,
    MultiThreaded, OnTriggerResult, OutputStream, ProcessContext, ProcessError, ProcessSession,
    Processor, Relationship, Schedule, SingleThreaded,
};
use std::borrow::Cow;

#[derive(Debug)]
pub struct TransformStreamResult {
    target_relationship_name: Cow<'static, str>,
    attributes_to_add: Vec<FlowFileAttribute>,
    write_status: IoState,
}

impl TransformStreamResult {
    pub fn new(target_relationship: &Relationship) -> Self {
        Self {
            target_relationship_name: Cow::Borrowed(target_relationship.name),
            attributes_to_add: Vec::new(),
            write_status: IoState::Ok,
        }
    }

    pub fn route_without_changes(target_relationship: &Relationship) -> Self {
        Self::route_without_changes_by_name(Cow::Borrowed(target_relationship.name))
    }

    pub fn route_without_changes_by_name(relationship: Cow<'static, str>) -> Self {
        Self {
            target_relationship_name: relationship,
            attributes_to_add: Vec::new(),
            write_status: IoState::Cancel,
        }
    }

    pub fn target_relationship_name(&self) -> &str {
        &self.target_relationship_name
    }

    pub fn get_attribute(&self, name: &str) -> Option<&str> {
        self.attributes_to_add
            .iter()
            .rfind(|(k, _)| k == name)
            .map(|(_, v)| v.as_ref())
    }

    pub fn write_status(&self) -> IoState {
        self.write_status
    }
}

impl_with_attributes!(TransformStreamResult);

pub trait FlowFileStreamTransform {
    fn transform<Ctx: GetProperty + GetControllerService + GetAttribute, LoggerImpl: Logger>(
        &self,
        context: &Ctx,
        input_stream: &mut dyn InputStream,
        output_stream: &mut dyn OutputStream,
        logger: &LoggerImpl,
    ) -> Result<TransformStreamResult, ProcessError>;
}

pub trait MutFlowFileStreamTransform {
    fn transform<Ctx: GetProperty + GetControllerService + GetAttribute, LoggerImpl: Logger>(
        &mut self,
        context: &Ctx,
        input_stream: &mut dyn InputStream,
        output_stream: &mut dyn OutputStream,
        logger: &LoggerImpl,
    ) -> Result<TransformStreamResult, ProcessError>;
}

pub struct FlowFileStreamTransformProcessorType {}

fn handle_stream_transform<PC, PS, L, F>(
    context: &mut PC,
    session: &mut PS,
    logger: &L,
    mut transform_fn: F,
) -> Result<OnTriggerResult, ProcessError>
where
    PC: ProcessContext,
    PS: ProcessSession<FlowFile = PC::FlowFile>,
    L: Logger,
    F: FnMut(
        &ContextSessionFlowFileBundle<PC, PS>,
        &mut dyn InputStream,
        &mut dyn OutputStream,
    ) -> Result<TransformStreamResult, ProcessError>,
{
    if let Some(mut flow_file) = session.get() {
        let simple_context = ContextSessionFlowFileBundle::new(context, session, Some(&flow_file));

        let (relationship, attrs) = session.read_stream(&flow_file, |input_stream| {
            session.write_stream(&flow_file, |output_stream| {
                let transformed = match transform_fn(&simple_context, input_stream, output_stream) {
                    Ok(t) => t,
                    Err(ProcessError::Route(route)) => {
                        route.log(logger);
                        TransformStreamResult::route_without_changes_by_name(route.relationship)
                    }
                    Err(ProcessError::Fatal(e)) => {
                        return Err(e);
                    }
                };

                Ok((
                    (
                        transformed.target_relationship_name,
                        transformed.attributes_to_add,
                    ),
                    transformed.write_status,
                ))
            })
        })?;

        for (k, v) in attrs {
            session.set_attribute(&mut flow_file, &k, &v)?;
        }

        session.transfer(flow_file, relationship.as_ref())?;

        Ok(OnTriggerResult::Ok)
    } else {
        logger.log(LogLevel::Trace, format_args!("No flowfile to transform"));
        Ok(OnTriggerResult::Yield)
    }
}

impl<Implementation, L> MultiThreadedTrigger
    for Processor<Implementation, FlowFileStreamTransformProcessorType, MultiThreaded, L>
where
    Implementation: Schedule + FlowFileStreamTransform,
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
            handle_stream_transform(context, session, &self.logger, |ctx, input, output| {
                scheduled_impl.transform(ctx, input, output, &self.logger)
            })
        } else {
            Err(MinifiError::UnscheduledProcessor.into())
        }
    }
}

impl<Implementation, L> SingleThreadedTrigger
    for Processor<Implementation, FlowFileStreamTransformProcessorType, SingleThreaded, L>
where
    Implementation: Schedule + MutFlowFileStreamTransform,
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
            handle_stream_transform(context, session, &self.logger, |ctx, input, output| {
                scheduled_impl.transform(ctx, input, output, &self.logger)
            })
        } else {
            Err(MinifiError::UnscheduledProcessor.into())
        }
    }
}

#[cfg(test)]
mod tests {
    use crate::Relationship;
    use minifi_native::TransformStreamResult;

    const TEST_RELATIONSHIP: Relationship = Relationship {
        name: "test",
        description: "test desc",
    };
    #[test]
    fn test_with_attributes() {
        let mut gen_ff = TransformStreamResult::new(&TEST_RELATIONSHIP);
        assert!(gen_ff.attributes_to_add.is_empty());

        gen_ff = gen_ff.with_attribute("foo", "bar");
        assert_eq!(1, gen_ff.attributes_to_add.len());

        gen_ff = gen_ff.with_attributes([("A", "apple"), ("B", "banana")]);
        assert_eq!(3, gen_ff.attributes_to_add.len());
        let (key_1, value_1) = gen_ff.attributes_to_add.get(1).unwrap();
        assert_eq!(key_1, "A");
        assert_eq!(value_1, "apple");
    }
}
