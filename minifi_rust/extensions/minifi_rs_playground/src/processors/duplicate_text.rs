use minifi_native::macros::{ComponentIdentifier, DefaultMetrics, NoAdvancedProcessorFeatures};
use minifi_native::{
    GetAttribute, GetControllerService, GetProperty, InputStream, Logger, MinifiError,
    MutFlowFileStreamTransform, OutputAttribute, OutputStream, ProcessorDefinition,
    ProcessorInputRequirement, Property, Relationship, Schedule, TransformStreamResult,
};
use std::collections::HashMap;

#[derive(Debug, ComponentIdentifier, DefaultMetrics, NoAdvancedProcessorFeatures)]
pub(crate) struct DuplicateStreamText {}

pub(crate) const SUCCESS: Relationship = Relationship {
    name: "success",
    description: "",
};

impl Schedule for DuplicateStreamText {
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

impl MutFlowFileStreamTransform for DuplicateStreamText {
    fn transform<Ctx: GetProperty + GetControllerService + GetAttribute, LoggerImpl: Logger>(
        &mut self,
        _context: &Ctx,
        input_stream: &mut dyn InputStream,
        output_stream: &mut dyn OutputStream,
        _logger: &LoggerImpl,
    ) -> Result<TransformStreamResult, MinifiError> {
        let mut byte = [0u8; 1];
        while input_stream.read(&mut byte)? > 0 {
            output_stream.write(&byte)?;
            output_stream.write(&byte)?;
        }
        Ok(TransformStreamResult::new(&SUCCESS, HashMap::new()))
    }
}

impl ProcessorDefinition for DuplicateStreamText {
    const DESCRIPTION: &'static str = "Duplicate text";
    const INPUT_REQUIREMENT: ProcessorInputRequirement = ProcessorInputRequirement::Required;
    const SUPPORTS_DYNAMIC_PROPERTIES: bool = false;
    const SUPPORTS_DYNAMIC_RELATIONSHIPS: bool = false;
    const OUTPUT_ATTRIBUTES: &'static [OutputAttribute] = &[];
    const RELATIONSHIPS: &'static [Relationship] = &[SUCCESS];
    const PROPERTIES: &'static [Property] = &[];
}
