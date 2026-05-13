use crate::processors::log_attribute::properties::*;
use crate::processors::log_attribute::{LogAttributeRs, relationships};
use minifi_native::{
    OutputAttribute, ProcessorDefinition, ProcessorInputRequirement, Property, Relationship,
};

impl ProcessorDefinition for LogAttributeRs {
    const DESCRIPTION: &'static str =
        "Logs attributes of flow files in the MiNiFi application log.";
    const INPUT_REQUIREMENT: ProcessorInputRequirement = ProcessorInputRequirement::Required;
    const SUPPORTS_DYNAMIC_PROPERTIES: bool = false;
    const SUPPORTS_DYNAMIC_RELATIONSHIPS: bool = false;
    const OUTPUT_ATTRIBUTES: &'static [OutputAttribute] = &[];
    const RELATIONSHIPS: &'static [Relationship] = &[relationships::SUCCESS];
    const PROPERTIES: &'static [Property] = &[
        LOG_LEVEL,
        ATTRIBUTES_TO_LOG,
        ATTRIBUTES_TO_IGNORE,
        LOG_PAYLOAD,
        LOG_PREFIX,
        FLOW_FILES_TO_LOG,
        HEX_ENCODE_PAYLOAD,
    ];
}
