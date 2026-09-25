// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

use minifi_native::OutputAttribute;
use strum_macros::{EnumString, IntoStaticStr};

const ENRICHMENT_ROLE: &str = "enrichment.role";
const ENRICHMENT_GROUP_ID: &str = "enrichment.group.id";

/// The roles ForkEnrichment writes and JoinEnrichmentAttributes pairs on.
///
/// `EnumString` parses the attribute back; `IntoStaticStr` writes it, so the wire strings
/// (`ORIGINAL` / `ENRICHMENT`) have exactly one definition.
#[derive(Debug, Clone, Copy, PartialEq, EnumString, IntoStaticStr)]
#[strum(serialize_all = "UPPERCASE", const_into_str)]
pub(crate) enum Role {
    Original,
    Enrichment,
}

/// The role a joined FlowFile gets. Deliberately not a [`Role`] variant: `Role` is the set of roles
/// this processor *pairs* on, and a `JOINED` flow file fed back in must not parse as a pairable
/// half - it should fall through to the `invalid` relationship.
pub(crate) const JOINED_ROLE: &str = "JOINED";

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
