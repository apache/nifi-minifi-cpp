use crate::api::process_session::IoState;
use crate::api::processor::AdvancedProcessorFeatures;
use crate::api::processor_wrappers::utils::context_session_flowfile_bundle::ContextSessionFlowFileBundle;
use crate::api::raw_processor::{MultiThreadedTrigger, SingleThreadedTrigger};
use crate::{
    CalculateMetrics, Concurrent, Exclusive, GetAttribute, GetControllerService, GetProperty,
    InputStream, LogLevel, Logger, MinifiError, OnTriggerResult, OutputStream, ProcessContext,
    ProcessSession, Processor, Relationship, Schedule,
};
use std::collections::HashMap;

pub struct TransformStreamResult {
    target_relationship_name: &'static str,
    attributes_to_add: HashMap<String, String>,
    write_status: IoState,
}

impl TransformStreamResult {
    pub fn new(
        target_relationship: &Relationship,
        attributes_to_add: HashMap<String, String>,
    ) -> Self {
        Self {
            target_relationship_name: target_relationship.name,
            attributes_to_add,
            write_status: IoState::Ok,
        }
    }

    pub fn route_without_changes(target_relationship: &Relationship) -> Self {
        Self {
            target_relationship_name: target_relationship.name,
            attributes_to_add: HashMap::new(),
            write_status: IoState::Cancel,
        }
    }

    pub fn target_relationship_name(&self) -> &'static str {
        self.target_relationship_name
    }

    pub fn get_attribute(&self, name: &str) -> Option<String> {
        self.attributes_to_add.get(name).cloned()
    }

    pub fn write_status(&self) -> IoState {
        self.write_status
    }
}

pub trait FlowFileStreamTransform {
    fn transform<Ctx: GetProperty + GetControllerService + GetAttribute, LoggerImpl: Logger>(
        &self,
        context: &Ctx,
        input_stream: &mut dyn InputStream,
        output_stream: &mut dyn OutputStream,
        logger: &LoggerImpl,
    ) -> Result<TransformStreamResult, MinifiError>;
}

pub trait MutFlowFileStreamTransform {
    fn transform<Ctx: GetProperty + GetControllerService + GetAttribute, LoggerImpl: Logger>(
        &mut self,
        context: &Ctx,
        input_stream: &mut dyn InputStream,
        output_stream: &mut dyn OutputStream,
        logger: &LoggerImpl,
    ) -> Result<TransformStreamResult, MinifiError>;
}

pub struct FlowFileStreamTransformProcessorType {}

fn handle_stream_transform<PC, PS, L, F>(
    context: &mut PC,
    session: &mut PS,
    logger: &L,
    mut transform_fn: F,
) -> Result<OnTriggerResult, MinifiError>
where
    PC: ProcessContext,
    PS: ProcessSession<FlowFile = PC::FlowFile>,
    L: Logger,
    F: FnMut(
        &ContextSessionFlowFileBundle<PC, PS>,
        &mut dyn InputStream,
        &mut dyn OutputStream,
    ) -> Result<TransformStreamResult, MinifiError>,
{
    if let Some(mut flow_file) = session.get() {
        let simple_context = ContextSessionFlowFileBundle::new(context, session, Some(&flow_file));

        let (relationship, attrs) = session.read_stream(&flow_file, |input_stream| {
            session.write_stream(&flow_file, |output_stream| {
                let transformed = transform_fn(&simple_context, input_stream, output_stream)?;

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

        session.transfer(flow_file, relationship)?;

        Ok(OnTriggerResult::Ok)
    } else {
        logger.log(LogLevel::Trace, format_args!("No flowfile to transform"));
        Ok(OnTriggerResult::Yield)
    }
}

// Concurrent Implementation (Multi-Threaded)
impl<'a, Implementation, L> MultiThreadedTrigger
    for Processor<Implementation, FlowFileStreamTransformProcessorType, Concurrent, L>
where
    Implementation:
        Schedule + CalculateMetrics + FlowFileStreamTransform + AdvancedProcessorFeatures,
    L: Logger,
{
    fn on_trigger<PC, PS>(
        &self,
        context: &mut PC,
        session: &mut PS,
    ) -> Result<OnTriggerResult, MinifiError>
    where
        PC: ProcessContext,
        PS: ProcessSession<FlowFile = PC::FlowFile>,
    {
        if let Some(ref scheduled_impl) = self.scheduled_impl {
            handle_stream_transform(context, session, &self.logger, |ctx, input, output| {
                scheduled_impl.transform(ctx, input, output, &self.logger)
            })
        } else {
            Err(MinifiError::trigger_err(
                "The processor hasn't been scheduled yet",
            ))
        }
    }
}

// Exclusive Implementation (Single-Threaded)
impl<'a, Implementation, L> SingleThreadedTrigger
    for Processor<Implementation, FlowFileStreamTransformProcessorType, Exclusive, L>
where
    Implementation:
        Schedule + CalculateMetrics + MutFlowFileStreamTransform + AdvancedProcessorFeatures,
    L: Logger,
{
    fn on_trigger<PC, PS>(
        &mut self,
        context: &mut PC,
        session: &mut PS,
    ) -> Result<OnTriggerResult, MinifiError>
    where
        PC: ProcessContext,
        PS: ProcessSession<FlowFile = PC::FlowFile>,
    {
        if let Some(ref mut scheduled_impl) = self.scheduled_impl {
            handle_stream_transform(context, session, &self.logger, |ctx, input, output| {
                scheduled_impl.transform(ctx, input, output, &self.logger)
            })
        } else {
            Err(MinifiError::trigger_err(
                "The processor hasn't been scheduled yet",
            ))
        }
    }
}
