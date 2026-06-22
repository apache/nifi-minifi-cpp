use crate::api::InputStream;
use crate::api::processor::{Processor};
use crate::api::processor_wrappers::utils::context_session_flowfile_bundle::ContextSessionFlowFileBundle;
use crate::api::processor_wrappers::utils::flow_file_content::Content;
use crate::api::property::{GetControllerService, GetProperty};
use crate::api::raw_processor::{MultiThreadedTrigger, SingleThreadedTrigger};
use crate::{
    Concurrent, Exclusive, GetAttribute, LogLevel, Logger, MinifiError,
    OnTriggerResult, ProcessContext, ProcessSession, Relationship, Schedule, info,
};
use std::collections::HashMap;

#[derive(Debug)]
pub struct TransformedFlowFile<'a> {
    target_relationship_name: &'static str,
    new_content: Option<Content<'a>>,  // If None the content doesn't change
    attributes_to_add: HashMap<String, String>,
}

impl<'a> TransformedFlowFile<'a> {
    pub fn route_without_changes(target_relationship: &Relationship) -> Self {
        Self {
            target_relationship_name: target_relationship.name,
            new_content: None,
            attributes_to_add: HashMap::new(),
        }
    }

    pub fn new(
        target_relationship: &Relationship,
        new_content: Option<Vec<u8>>,
        attributes_to_add: HashMap<String, String>,
    ) -> Self {
        Self {
            target_relationship_name: target_relationship.name,
            new_content: new_content.map(|b| Content::Buffer(b)),
            attributes_to_add,
        }
    }

    pub fn new_content(&'_ self) -> Option<&'_ Content<'_>> {
        self.new_content.as_ref()
    }

    pub fn target_relationship(&self) -> &'static str {
        self.target_relationship_name
    }

    pub fn attributes_to_add(&self) -> &HashMap<String, String> {
        &self.attributes_to_add
    }
}

pub trait FlowFileTransform {
    fn transform<
        'a,
        Context: GetProperty + GetControllerService + GetAttribute,
        LoggerImpl: Logger,
    >(
        &self,
        context: &Context,
        input_stream: &'a mut dyn InputStream,
        logger: &LoggerImpl,
    ) -> Result<TransformedFlowFile<'a>, MinifiError>;
}

pub trait MutFlowFileTransform {
    fn transform<
        'a,
        Context: GetProperty + GetControllerService + GetAttribute,
        LoggerImpl: Logger,
    >(
        &mut self,
        context: &Context,
        input_stream: &'a mut dyn InputStream,
        logger: &LoggerImpl,
    ) -> Result<TransformedFlowFile<'a>, MinifiError>;
}

pub struct FlowFileTransformProcessorType {}

fn handle_transform<PC, PS, L, F>(
    context: &mut PC,
    session: &mut PS,
    logger: &L,
    mut transform_fn: F,
) -> Result<OnTriggerResult, MinifiError>
where
    PC: ProcessContext,
    PS: ProcessSession<FlowFile = PC::FlowFile>,
    L: Logger,
    F: for<'stream> FnMut(
        &ContextSessionFlowFileBundle<'_, PC, PS>,
        &'stream mut dyn InputStream,
    ) -> Result<TransformedFlowFile<'stream>, MinifiError>,
{
    if let Some(mut flow_file) = session.get() {
        let simple_context = ContextSessionFlowFileBundle::new(context, session, Some(&flow_file));

        let (attrs_to_add, relationship) = session.read_stream(&flow_file, |input_stream| {
            let transformed = transform_fn(&simple_context, input_stream)?;

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

        session.transfer(flow_file, relationship)?;
        Ok(OnTriggerResult::Ok)
    } else {
        logger.log(LogLevel::Trace, format_args!("No flowfile to transform"));
        Ok(OnTriggerResult::Yield)
    }
}

impl<Implementation, L> MultiThreadedTrigger
    for Processor<Implementation, FlowFileTransformProcessorType, Concurrent, L>
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
            handle_transform(context, session, &self.logger, |ctx, input| {
                scheduled_impl.transform(ctx, input, &self.logger)
            })
        } else {
            Err(MinifiError::trigger_err(
                "The processor hasn't been scheduled yet",
            ))
        }
    }
}

impl<Implementation, L> SingleThreadedTrigger
    for Processor<Implementation, FlowFileTransformProcessorType, Exclusive, L>
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
            handle_transform(context, session, &self.logger, |ctx, input| {
                scheduled_impl.transform(ctx, input, &self.logger)
            })
        } else {
            Err(MinifiError::trigger_err(
                "The processor hasn't been scheduled yet",
            ))
        }
    }
}
