use super::{DecryptContentPGP, output_attributes, properties, relationships};
use minifi_native::{
    OutputAttribute, ProcessorDefinition, ProcessorInputRequirement, Property, Relationship,
};

impl ProcessorDefinition for DecryptContentPGP {
    const DESCRIPTION: &'static str = "Decrypt contents of OpenPGP messages. Using the Packaged Decryption Strategy preserves OpenPGP encoding to support subsequent signature verification.";
    const INPUT_REQUIREMENT: ProcessorInputRequirement = ProcessorInputRequirement::Required;
    const SUPPORTS_DYNAMIC_PROPERTIES: bool = false;
    const SUPPORTS_DYNAMIC_RELATIONSHIPS: bool = false;
    const OUTPUT_ATTRIBUTES: &'static [OutputAttribute] = &[
        output_attributes::LITERAL_DATA_FILENAME,
        output_attributes::LITERAL_DATA_MODIFIED,
    ];
    const RELATIONSHIPS: &'static [Relationship] =
        &[relationships::SUCCESS, relationships::FAILURE];
    const PROPERTIES: &'static [Property] = &[
        properties::DECRYPTION_STRATEGY,
        properties::SYMMETRIC_PASSWORD,
        properties::PRIVATE_KEY_SERVICE,
    ];
}
