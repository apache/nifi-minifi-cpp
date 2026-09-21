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

use crate::api::flow_file::GetId;
use crate::api::processor::Processor;
use crate::api::processor_wrappers::utils::context_session_flowfile_bundle::ContextSessionFlowFileBundle;
use crate::api::processor_wrappers::utils::flow_file_content::Content;
use crate::api::property::{GetControllerService, GetProperty};
use crate::api::raw_processor::{MultiThreadedTrigger, SingleThreadedTrigger};
use crate::{
    GetAttribute, LogLevel, Logger, MinifiError, MultiThreaded, OnTriggerResult, ProcessContext,
    ProcessSession, Relationship, Schedule, SingleThreaded, TransformError, impl_with_attributes,
    info,
};

use minifi_native::InputStream;
use std::borrow::Cow;

pub type FlowFileAttribute = (Cow<'static, str>, Cow<'static, str>);

#[derive(Debug)]
pub struct TransformedFlowFile<'a> {
    target_relationship_name: &'static str,
    new_content: Option<Content<'a>>, // If None, the content doesn't change
    attributes_to_add: Vec<FlowFileAttribute>,
}

impl<'a> TransformedFlowFile<'a> {
    pub fn route_without_changes(target_relationship: &Relationship) -> Self {
        Self::route_without_changes_by_name(target_relationship.name)
    }

    pub fn route_without_changes_by_name(relationship: &'static str) -> Self {
        Self {
            target_relationship_name: relationship,
            new_content: None,
            attributes_to_add: Vec::new(),
        }
    }

    pub fn new(target_relationship: &Relationship, new_content: Option<Content<'a>>) -> Self {
        Self {
            target_relationship_name: target_relationship.name,
            new_content,
            attributes_to_add: Vec::new(),
        }
    }

    #[must_use]
    pub fn with_content(mut self, content: Content<'a>) -> Self {
        self.new_content = Some(content);
        self
    }

    pub fn new_content(&'_ self) -> Option<&'_ Content<'_>> {
        self.new_content.as_ref()
    }

    pub fn target_relationship(&self) -> &str {
        self.target_relationship_name
    }

    pub fn attributes_to_add(&self) -> &[FlowFileAttribute] {
        &self.attributes_to_add
    }

    pub fn attribute(&self, name: &str) -> Option<&str> {
        self.attributes_to_add
            .iter()
            .rfind(|(k, _)| k == name)
            .map(|(_, v)| v.as_ref())
    }

    #[cfg(any(test, feature = "test-utils"))]
    pub fn into_bytes(self) -> std::io::Result<Option<Vec<u8>>> {
        match self.new_content {
            Some(Content::Buffer(vec)) => Ok(Some(vec)),
            Some(Content::Stream(mut stream)) => {
                let mut buffer = Vec::new();
                stream.read_to_end(&mut buffer)?;
                Ok(Some(buffer))
            }
            None => Ok(None),
        }
    }
}

impl_with_attributes!(TransformedFlowFile<'a>);

pub trait FlowFileTransform {
    /// Relationship that errors propagated with `?` (i.e. [`TransformError::Bubbled`])
    /// are routed to.
    const ERROR_RELATIONSHIP: &'static Relationship;

    fn transform<
        'a,
        Context: GetProperty + GetControllerService + GetAttribute + GetId,
        LoggerImpl: Logger,
    >(
        &self,
        context: &Context,
        input_stream: &'a mut dyn InputStream,
        logger: &LoggerImpl,
    ) -> Result<TransformedFlowFile<'a>, TransformError>;
}

pub trait MutFlowFileTransform {
    /// Relationship that errors propagated with `?` (i.e. [`TransformError::Bubbled`])
    /// are routed to.
    const ERROR_RELATIONSHIP: &'static Relationship;

    fn transform<
        'a,
        Context: GetProperty + GetControllerService + GetAttribute,
        LoggerImpl: Logger,
    >(
        &mut self,
        context: &Context,
        input_stream: &'a mut dyn InputStream,
        logger: &LoggerImpl,
    ) -> Result<TransformedFlowFile<'a>, TransformError>;
}

pub struct FlowFileTransformProcessorType {}

fn handle_transform<PC, PS, L, F>(
    context: &mut PC,
    session: &mut PS,
    logger: &L,
    error_relationship: &Relationship,
    mut transform_fn: F,
) -> Result<OnTriggerResult, MinifiError>
where
    PC: ProcessContext,
    PS: ProcessSession<FlowFile = PC::FlowFile>,
    L: Logger,
    F: for<'stream> FnMut(
        &ContextSessionFlowFileBundle<'_, PC, PS>,
        &'stream mut dyn InputStream,
    ) -> Result<TransformedFlowFile<'stream>, TransformError>,
{
    if let Some(mut flow_file) = session.get() {
        let simple_context = ContextSessionFlowFileBundle::new(context, session, Some(&flow_file));

        let (attrs_to_add, relationship) = session.read_stream(&flow_file, |input_stream| {
            let transformed = match transform_fn(&simple_context, input_stream) {
                Ok(transform_success) => transform_success,
                Err(err) => match err.into_route(error_relationship) {
                    Ok(route) => {
                        route.log(logger);
                        TransformedFlowFile::route_without_changes_by_name(route.relationship)
                    }
                    Err(minifi_error) => return Err(minifi_error),
                },
            };

            info!(logger, "{:?}", transformed);
            match transformed.new_content {
                None => {}
                Some(Content::Buffer(buffer)) => {
                    session.write(&flow_file, &buffer)?;
                }
                Some(Content::Stream(stream)) => {
                    session.write_from_stream(&flow_file, stream)?;
                }
            };

            Ok((
                transformed.attributes_to_add,
                transformed.target_relationship_name,
            ))
        })?;

        for (k, v) in attrs_to_add {
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
    for Processor<Implementation, FlowFileTransformProcessorType, MultiThreaded, L>
where
    Implementation: Schedule + FlowFileTransform,
    L: Logger,
{
    fn trigger<PC, PS>(
        &self,
        context: &mut PC,
        session: &mut PS,
    ) -> Result<OnTriggerResult, MinifiError>
    where
        PC: ProcessContext,
        PS: ProcessSession<FlowFile = PC::FlowFile>,
    {
        if let Some(ref scheduled_impl) = self.scheduled_impl {
            handle_transform(
                context,
                session,
                &self.logger,
                Implementation::ERROR_RELATIONSHIP,
                |ctx, input| scheduled_impl.transform(ctx, input, &self.logger),
            )
        } else {
            Err(MinifiError::UnscheduledProcessor)
        }
    }
}

impl<Implementation, L> SingleThreadedTrigger
    for Processor<Implementation, FlowFileTransformProcessorType, SingleThreaded, L>
where
    Implementation: Schedule + MutFlowFileTransform,
    L: Logger,
{
    fn trigger<PC, PS>(
        &mut self,
        context: &mut PC,
        session: &mut PS,
    ) -> Result<OnTriggerResult, MinifiError>
    where
        PC: ProcessContext,
        PS: ProcessSession<FlowFile = PC::FlowFile>,
    {
        if let Some(ref mut scheduled_impl) = self.scheduled_impl {
            handle_transform(
                context,
                session,
                &self.logger,
                Implementation::ERROR_RELATIONSHIP,
                |ctx, input| scheduled_impl.transform(ctx, input, &self.logger),
            )
        } else {
            Err(MinifiError::UnscheduledProcessor)
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::api::RawProcessor;
    use crate::api::raw_processor::MultiThreadedTrigger;
    use crate::{
        GetControllerService, GetId, MockFlowFile, MockLogger, MockProcessContext,
        MockProcessSession, TransformError,
    };

    const FAILURE: Relationship = Relationship {
        name: "failure",
        description: "test failure relationship",
    };

    struct RouteToFailure;
    impl Schedule for RouteToFailure {
        fn schedule<Ctx: GetProperty, L: Logger>(_c: &Ctx, _l: &L) -> Result<Self, MinifiError> {
            Ok(RouteToFailure)
        }
    }
    impl FlowFileTransform for RouteToFailure {
        const ERROR_RELATIONSHIP: &'static Relationship = &FAILURE;

        fn transform<
            'a,
            Context: GetProperty + GetControllerService + GetAttribute + GetId,
            LoggerImpl: Logger,
        >(
            &self,
            _context: &Context,
            _input_stream: &'a mut dyn InputStream,
            _logger: &LoggerImpl,
        ) -> Result<TransformedFlowFile<'a>, TransformError> {
            let bad: Result<TransformedFlowFile<'a>, std::io::Error> = Err(std::io::Error::new(
                std::io::ErrorKind::InvalidData,
                "bad data",
            ));
            // Propagated with `?`, so it becomes `Bubbled` and the wrapper routes
            // it to `ERROR_RELATIONSHIP`.
            Ok(bad?)
        }
    }

    struct RollbackTransform;
    impl Schedule for RollbackTransform {
        fn schedule<Ctx: GetProperty, L: Logger>(_c: &Ctx, _l: &L) -> Result<Self, MinifiError> {
            Ok(RollbackTransform)
        }
    }
    impl FlowFileTransform for RollbackTransform {
        const ERROR_RELATIONSHIP: &'static Relationship = &FAILURE;

        fn transform<
            'a,
            Context: GetProperty + GetControllerService + GetAttribute + GetId,
            LoggerImpl: Logger,
        >(
            &self,
            _context: &Context,
            _input_stream: &'a mut dyn InputStream,
            _logger: &LoggerImpl,
        ) -> Result<TransformedFlowFile<'a>, TransformError> {
            Err(TransformError::Rollback(MinifiError::custom("real error")))
        }
    }

    fn seeded_session() -> MockProcessSession {
        let mut session = MockProcessSession::new();
        session
            .input_flow_files
            .push(MockFlowFile::with_content(b"data"));
        session
    }

    #[test]
    fn route_error_transfers_to_failure_and_commits() {
        let mut processor: Processor<
            RouteToFailure,
            FlowFileTransformProcessorType,
            MultiThreaded,
            MockLogger,
        > = Processor::new(MockLogger::new());
        processor.scheduled_impl = Some(RouteToFailure);

        let mut context = MockProcessContext::new();
        let mut session = seeded_session();

        let result = MultiThreadedTrigger::trigger(&processor, &mut context, &mut session);

        assert_eq!(
            result.expect("should commit, not roll back"),
            OnTriggerResult::Ok
        );
        let transferred = session.transferred_flow_files.borrow();
        assert_eq!(transferred.len(), 1);
        assert_eq!(transferred[0].relationship, "failure");
    }

    #[test]
    fn rollback_error_propagates_and_transfers_nothing() {
        let mut processor: Processor<
            RollbackTransform,
            FlowFileTransformProcessorType,
            MultiThreaded,
            MockLogger,
        > = Processor::new(MockLogger::new());
        processor.scheduled_impl = Some(RollbackTransform);

        let mut context = MockProcessContext::new();
        let mut session = seeded_session();

        let result = MultiThreadedTrigger::trigger(&processor, &mut context, &mut session);

        assert!(matches!(result, Err(MinifiError::CustomError(_))));
        assert_eq!(session.num_of_transferred_flow_files(), 0);
    }

    const TEST_RELATIONSHIP: Relationship = Relationship {
        name: "test",
        description: "test desc",
    };
    #[test]
    fn test_with_attributes() {
        let mut gen_ff = TransformedFlowFile::route_without_changes(&TEST_RELATIONSHIP);
        assert!(gen_ff.attributes_to_add.is_empty());

        gen_ff = gen_ff.with_attribute("foo", "bar");
        assert_eq!(1, gen_ff.attributes_to_add.len());

        gen_ff = gen_ff.with_attributes([("A", "apple"), ("B", "banana")]);
        assert_eq!(3, gen_ff.attributes_to_add.len());
        let (key_1, value_1) = gen_ff.attributes_to_add.get(1).unwrap();
        assert_eq!(key_1, "A");
        assert_eq!(value_1, "apple");

        assert!(gen_ff.new_content.is_none());
        gen_ff = gen_ff.with_content(Content::Buffer("hello".into()));
        assert!(matches!(gen_ff.new_content.unwrap(), Content::Buffer(_)));
    }
}
