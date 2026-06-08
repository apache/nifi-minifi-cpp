use super::properties::*;
use super::{GenerateFlowFileRs, relationships};
use minifi_native::{
    OutputAttribute, ProcessorDefinition, ProcessorInputRequirement, Property, Relationship,
};

impl ProcessorDefinition for GenerateFlowFileRs {
    const DESCRIPTION: &'static str = "This processor creates FlowFiles with random data or custom content. GenerateFlowFile is useful for load testing, configuration, and simulation.";
    const INPUT_REQUIREMENT: ProcessorInputRequirement = ProcessorInputRequirement::Forbidden;
    const SUPPORTS_DYNAMIC_PROPERTIES: bool = false;
    const SUPPORTS_DYNAMIC_RELATIONSHIPS: bool = false;
    const OUTPUT_ATTRIBUTES: &'static [OutputAttribute] = &[];
    const RELATIONSHIPS: &'static [Relationship] = &[relationships::SUCCESS];
    const PROPERTIES: &'static [Property] = &[
        FILE_SIZE,
        BATCH_SIZE,
        DATA_FORMAT,
        UNIQUE_FLOW_FILES,
        CUSTOM_TEXT,
    ];
}
