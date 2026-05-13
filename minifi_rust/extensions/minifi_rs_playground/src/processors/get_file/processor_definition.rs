use crate::processors::get_file::output_attributes::{
    ABSOLUTE_PATH_OUTPUT_ATTRIBUTE, FILENAME_OUTPUT_ATTRIBUTE,
};
use crate::processors::get_file::properties::*;
use crate::processors::get_file::{GetFileRs, relationships};
use minifi_native::{
    OutputAttribute, ProcessorDefinition, ProcessorInputRequirement, Property, Relationship,
};

impl ProcessorDefinition for GetFileRs {
    const DESCRIPTION: &'static str = "Creates FlowFiles from files in a directory. MiNiFi will ignore files for which it doesn't have read permissions.";
    const INPUT_REQUIREMENT: ProcessorInputRequirement = ProcessorInputRequirement::Forbidden;
    const SUPPORTS_DYNAMIC_PROPERTIES: bool = false;
    const SUPPORTS_DYNAMIC_RELATIONSHIPS: bool = false;
    const OUTPUT_ATTRIBUTES: &'static [OutputAttribute] =
        &[ABSOLUTE_PATH_OUTPUT_ATTRIBUTE, FILENAME_OUTPUT_ATTRIBUTE];
    const RELATIONSHIPS: &'static [Relationship] = &[relationships::SUCCESS];
    const PROPERTIES: &'static [Property] = &[
        DIRECTORY,
        POLLING_INTERVAL,
        RECURSE,
        KEEP_SOURCE_FILE,
        MIN_AGE,
        MAX_AGE,
        MIN_SIZE,
        MAX_SIZE,
        IGNORE_HIDDEN_FILES,
        BATCH_SIZE,
    ];
}
