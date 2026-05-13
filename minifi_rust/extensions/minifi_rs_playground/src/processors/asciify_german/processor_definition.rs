use crate::processors::asciify_german::AsciifyGerman;
use minifi_native::{
    OutputAttribute, ProcessorDefinition, ProcessorInputRequirement, Property, Relationship,
};

impl ProcessorDefinition for AsciifyGerman {
    const DESCRIPTION: &'static str = "This processor switches German characters with their ascii counterparts. (to test stream API)";
    const INPUT_REQUIREMENT: ProcessorInputRequirement = ProcessorInputRequirement::Required;
    const SUPPORTS_DYNAMIC_PROPERTIES: bool = false;
    const SUPPORTS_DYNAMIC_RELATIONSHIPS: bool = false;
    const OUTPUT_ATTRIBUTES: &'static [OutputAttribute] = &[];
    const RELATIONSHIPS: &'static [Relationship] =
        &[super::relationships::SUCCESS, super::relationships::FAILURE];
    const PROPERTIES: &'static [Property] = &[];
}
