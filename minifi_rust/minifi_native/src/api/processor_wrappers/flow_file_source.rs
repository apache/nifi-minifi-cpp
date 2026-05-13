use crate::api::processor::AdvancedProcessorFeatures;
use crate::api::processor_wrappers::utils::flow_file_content::Content;
use crate::api::raw_processor::{MultiThreadedTrigger, SingleThreadedTrigger};
use crate::{
    CalculateMetrics, Concurrent, Exclusive, GetControllerService, GetProperty, Logger,
    MinifiError, OnTriggerResult, ProcessContext, ProcessSession, Processor, Relationship,
    Schedule,
};
use std::collections::HashMap;

pub struct GeneratedFlowFile<'a> {
    target_relationship_name: &'static str,
    new_content: Option<Content<'a>>,
    attributes_to_add: HashMap<String, String>,
}

impl<'a> GeneratedFlowFile<'a> {
    pub fn new(
        target_relationship: &'a Relationship,
        new_content: Option<Content<'a>>,
        attributes_to_add: HashMap<String, String>,
    ) -> Self {
        Self {
            target_relationship_name: target_relationship.name,
            new_content,
            attributes_to_add,
        }
    }

    pub fn target_relationship_name(&self) -> &'static str {
        self.target_relationship_name
    }
}

pub trait FlowFileSource {
    fn generate<'a, Context: GetProperty + GetControllerService, LoggerImpl: Logger>(
        &self,
        context: &'a mut Context,
        logger: &LoggerImpl,
    ) -> Result<Vec<GeneratedFlowFile<'a>>, MinifiError>;
}

pub trait MutFlowFileSource {
    fn generate<'a, Context: GetProperty + GetControllerService, LoggerImpl: Logger>(
        &mut self,
        context: &'a mut Context,
        logger: &LoggerImpl,
    ) -> Result<Vec<GeneratedFlowFile<'a>>, MinifiError>;
}

fn handle_generated_flow_files<PC, PS>(
    session: &mut PS,
    generated_flow_files: Vec<GeneratedFlowFile>,
) -> Result<OnTriggerResult, MinifiError>
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
            Some(Content::Buffer(buffer)) => session.write(&mut ff, &buffer)?,
            Some(Content::Stream(stream)) => session.write_lazy(&mut ff, stream)?,
        }
        for (k, v) in &new_flow_file_data.attributes_to_add {
            session.set_attribute(&mut ff, k, v)?;
        }
        session.transfer(ff, new_flow_file_data.target_relationship_name)?;
    }
    Ok(OnTriggerResult::Ok)
}

pub struct FlowFileSourceProcessorType {}

impl<'a, Implementation, L> MultiThreadedTrigger
    for Processor<Implementation, FlowFileSourceProcessorType, Concurrent, L>
where
    Implementation: Schedule + CalculateMetrics + FlowFileSource + AdvancedProcessorFeatures,
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
            let files = scheduled_impl.generate(context, &self.logger)?;
            handle_generated_flow_files::<PC, PS>(session, files)
        } else {
            Err(MinifiError::trigger_err(
                "The processor hasn't been scheduled yet",
            ))
        }
    }
}

impl<'a, Implementation, L> SingleThreadedTrigger
    for Processor<Implementation, FlowFileSourceProcessorType, Exclusive, L>
where
    Implementation: Schedule + CalculateMetrics + MutFlowFileSource + AdvancedProcessorFeatures,
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
            let files = scheduled_impl.generate(context, &self.logger)?;
            handle_generated_flow_files::<PC, PS>(session, files)
        } else {
            Err(MinifiError::trigger_err(
                "The processor hasn't been scheduled yet",
            ))
        }
    }
}
