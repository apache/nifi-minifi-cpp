use super::*;
use minifi_native::{
    OutputAttribute, ProcessorDefinition, ProcessorInputRequirement, Property, Relationship,
};

impl ProcessorDefinition for KamikazeProcessorRs {
    const DESCRIPTION: &'static str = "This processor can fail or panic in on_trigger and on_schedule calls based on configuration. Only for testing purposes.";
    const INPUT_REQUIREMENT: ProcessorInputRequirement = ProcessorInputRequirement::Allowed;
    const SUPPORTS_DYNAMIC_PROPERTIES: bool = false;
    const SUPPORTS_DYNAMIC_RELATIONSHIPS: bool = false;
    const OUTPUT_ATTRIBUTES: &'static [OutputAttribute] = &[];
    const RELATIONSHIPS: &'static [Relationship] = &[relationships::SUCCESS];
    const PROPERTIES: &'static [Property] = &[
        properties::ON_SCHEDULE_BEHAVIOUR,
        properties::ON_TRIGGER_BEHAVIOUR,
    ];
}
