use super::LoremIpsumCSUser;
use super::properties::*;
use crate::processors::lorem_ipsum_cs_user::relationships::SUCCESS;
use minifi_native::{
    OutputAttribute, ProcessorDefinition, ProcessorInputRequirement, Property, Relationship,
};

impl ProcessorDefinition for LoremIpsumCSUser {
    const DESCRIPTION: &'static str = "Processor to test Controller Service API";
    const INPUT_REQUIREMENT: ProcessorInputRequirement = ProcessorInputRequirement::Forbidden;
    const SUPPORTS_DYNAMIC_PROPERTIES: bool = false;
    const SUPPORTS_DYNAMIC_RELATIONSHIPS: bool = false;
    const OUTPUT_ATTRIBUTES: &'static [OutputAttribute] = &[];
    const RELATIONSHIPS: &'static [Relationship] = &[SUCCESS];
    const PROPERTIES: &'static [Property] = &[CONTROLLER_SERVICE, WRITE_METHOD];
}
