use minifi_native::OutputAttribute;
use strum_macros::{Display, EnumString, IntoStaticStr, VariantNames};

const ENRICHMENT_ROLE: &str = "enrichment.role";
const ENRICHMENT_GROUP_ID: &str = "enrichment.group.id";

#[derive(Debug, Clone, Copy, PartialEq, Display, EnumString, VariantNames, IntoStaticStr)]
#[strum(serialize_all = "UPPERCASE", const_into_str)]
pub(crate) enum Role {
    Original,
    Enrichment,
}

pub(crate) const FORK_ROLE_ATTR: OutputAttribute = OutputAttribute {
    name: ENRICHMENT_ROLE,
    relationships: &["enrichment", "original"],
    description: "The role to use for enrichment. This will either be ORIGINAL or ENRICHMENT.",
};

pub(crate) const GROUP_ID_ATTR: OutputAttribute = OutputAttribute {
    name: ENRICHMENT_GROUP_ID,
    relationships: &["enrichment", "original"],
    description: "The Group ID to use in order to correlate the 'original' FlowFile with the 'enrichment' FlowFile.",
};

pub(crate) const JOIN_ROLE_ATTR: OutputAttribute = OutputAttribute {
    name: ENRICHMENT_ROLE,
    relationships: &["joined"],
    description: "JOINED",
};
