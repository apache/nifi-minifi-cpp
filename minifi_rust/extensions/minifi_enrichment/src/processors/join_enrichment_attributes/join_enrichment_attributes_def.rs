use crate::processors::attributes::JOIN_ROLE_ATTR;
use crate::processors::join_enrichment_attributes::JoinEnrichmentAttributesRs;
use minifi_native::{
    OutputAttribute, ProcessorDefinition, ProcessorInputRequirement, Property, PropertyDefinition,
    Relationship, property_definitions,
};
use std::time::Duration;

pub(crate) const INVALID: Relationship = Relationship {
    name: "invalid",
    description: "Any FlowFiles without the requisite attributes will be routed here",
};

pub(crate) const JOINED: Relationship = Relationship {
    name: "joined",
    description: "The resultant FlowFile with Records joined together from both the original and enrichment FlowFiles will be routed to this relationship",
};

pub(crate) const ORIGINAL: Relationship = Relationship {
    name: "original",
    description: "Both of the incoming FlowFiles ('original' and 'enrichment') will be routed to this Relationship. I.e., this is the 'original' version of both of these FlowFiles.",
};

pub(crate) const TIMEOUT_REL: Relationship = Relationship {
    name: "timeout",
    description: "If one of the incoming FlowFiles (i.e., the 'original' FlowFile or the 'enrichment' FlowFile) arrives to this Processor but the other does not arrive within the configured Timeout period, the FlowFile that did arrive is routed to this relationship.",
};

pub(crate) const BATCH_SIZE: Property<Option<usize>> = Property::new(
    "Batch Size",
    "The maximum number of FlowFiles to process in each trigger",
);

pub(crate) const TIMEOUT_PROP: Property<Option<Duration>> = Property::new(
    "Timeout",
    "Specifies the maximum amount of time to wait for the second FlowFile once the first arrives at the processor, after which point the first FlowFile will be routed to the 'timeout' relationship.",
);

impl ProcessorDefinition for JoinEnrichmentAttributesRs {
    const DESCRIPTION: &'static str = "Rejoins the forked FlowFiles coming from ForkEnrichmentRs processor, the resulting FlowFile will have the Original's content and all attributes from both of them (prioritizing Enrichment's).";
    const INPUT_REQUIREMENT: ProcessorInputRequirement = ProcessorInputRequirement::Required;
    const SUPPORTS_DYNAMIC_PROPERTIES: bool = false;
    const SUPPORTS_DYNAMIC_RELATIONSHIPS: bool = false;
    const OUTPUT_ATTRIBUTES: &'static [OutputAttribute] = &[JOIN_ROLE_ATTR];
    const RELATIONSHIPS: &'static [Relationship] = &[INVALID, JOINED, ORIGINAL, TIMEOUT_REL];

    const PROPERTIES: &[PropertyDefinition] = property_definitions![BATCH_SIZE, TIMEOUT_PROP];
}
