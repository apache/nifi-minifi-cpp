use crate::processors::attributes::{FORK_ROLE_ATTR, GROUP_ID_ATTR};
use crate::processors::fork_enrichment::ForkEnrichmentRs;
use minifi_native::{
    OutputAttribute, ProcessorDefinition, ProcessorInputRequirement, Property, PropertyDefinition,
    Relationship, property_definitions,
};

pub(crate) const ORIGINAL: Relationship = Relationship {
    name: "original",
    description: "The incoming FlowFile will be routed to this relationship, after adding appropriate attributes.",
};

pub(crate) const ENRICHMENT: Relationship = Relationship {
    name: "enrichment",
    description: "A clone of the incoming FlowFile will be routed to this relationship, after adding appropriate attributes.",
};

pub(crate) const BATCH_SIZE: Property<Option<usize>> = Property::new(
    "Batch Size",
    "The maximum number of FlowFiles to fork in each trigger",
);

impl ProcessorDefinition for ForkEnrichmentRs {
    const DESCRIPTION: &'static str = "Used in conjunction with the JoinEnrichmentAttributesRs processor, this processor is responsible for adding the attributes that are necessary for the JoinEnrichmentAttributesRs processor to perform its function. Each incoming FlowFile will be cloned. The original FlowFile will have appropriate attributes added and then be transferred to the 'original' relationship. The clone will have appropriate attributes added and then be routed to the 'enrichment' relationship.";
    const INPUT_REQUIREMENT: ProcessorInputRequirement = ProcessorInputRequirement::Required;
    const SUPPORTS_DYNAMIC_PROPERTIES: bool = false;
    const SUPPORTS_DYNAMIC_RELATIONSHIPS: bool = false;
    const OUTPUT_ATTRIBUTES: &'static [OutputAttribute] = &[FORK_ROLE_ATTR, GROUP_ID_ATTR];
    const RELATIONSHIPS: &'static [Relationship] = &[ORIGINAL, ENRICHMENT];

    const PROPERTIES: &[PropertyDefinition] = property_definitions![BATCH_SIZE];
}
