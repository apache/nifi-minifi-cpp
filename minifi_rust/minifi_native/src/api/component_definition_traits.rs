use crate::{OutputAttribute, ProcessorInputRequirement, Property, Relationship};

pub trait ComponentIdentifier {
    const CLASS_NAME: &'static str;
    const GROUP_NAME: &'static str;
    const VERSION: &'static str;
}

pub trait ProcessorDefinition {
    const DESCRIPTION: &'static str;
    const INPUT_REQUIREMENT: ProcessorInputRequirement;
    const SUPPORTS_DYNAMIC_PROPERTIES: bool;
    const SUPPORTS_DYNAMIC_RELATIONSHIPS: bool;
    const OUTPUT_ATTRIBUTES: &'static [OutputAttribute];
    const RELATIONSHIPS: &'static [Relationship];
    const PROPERTIES: &'static [Property];
}

pub trait ControllerServiceDefinition {
    const DESCRIPTION: &'static str;
    const PROPERTIES: &'static [Property];
}
