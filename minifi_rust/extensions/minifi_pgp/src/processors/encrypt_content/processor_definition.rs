use super::{EncryptContentPGP, output_attributes, properties, relationships};
use minifi_native::{
    OutputAttribute, ProcessorDefinition, ProcessorInputRequirement, Property, Relationship,
};

impl ProcessorDefinition for EncryptContentPGP {
    const DESCRIPTION: &'static str = "Encrypt contents using OpenPGP.";
    const INPUT_REQUIREMENT: ProcessorInputRequirement = ProcessorInputRequirement::Required;
    const SUPPORTS_DYNAMIC_PROPERTIES: bool = false;
    const SUPPORTS_DYNAMIC_RELATIONSHIPS: bool = false;
    const OUTPUT_ATTRIBUTES: &'static [OutputAttribute] = &[output_attributes::FILE_ENCODING];
    const RELATIONSHIPS: &'static [Relationship] =
        &[relationships::SUCCESS, relationships::FAILURE];
    const PROPERTIES: &'static [Property] = &[
        properties::FILE_ENCODING,
        properties::PASSWORD,
        properties::PUBLIC_KEY_SEARCH,
        properties::PUBLIC_KEY_SERVICE,
    ];
}
