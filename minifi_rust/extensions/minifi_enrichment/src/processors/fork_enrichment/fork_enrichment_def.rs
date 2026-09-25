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

use crate::processors::attributes::{FORK_ROLE_ATTR, GROUP_ID_ATTR};
use crate::processors::fork_enrichment::ForkEnrichment;
use minifi_native::{
    OutputAttribute, ProcessorDefinition, ProcessorInputRequirement, PropertyDefinition,
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

impl ProcessorDefinition for ForkEnrichment {
    const DESCRIPTION: &'static str = "Used in conjunction with the JoinEnrichmentAttributes processor, this processor is responsible for adding the attributes that are necessary for the JoinEnrichmentAttributes processor to perform its function. Each incoming FlowFile will be cloned. The original FlowFile will have appropriate attributes added and then be transferred to the 'original' relationship. The clone will have appropriate attributes added and then be routed to the 'enrichment' relationship.";
    const INPUT_REQUIREMENT: ProcessorInputRequirement = ProcessorInputRequirement::Required;
    const SUPPORTS_DYNAMIC_PROPERTIES: bool = false;
    const SUPPORTS_DYNAMIC_RELATIONSHIPS: bool = false;
    const OUTPUT_ATTRIBUTES: &'static [OutputAttribute] = &[FORK_ROLE_ATTR, GROUP_ID_ATTR];
    const RELATIONSHIPS: &'static [Relationship] = &[ORIGINAL, ENRICHMENT];

    const PROPERTIES: &[PropertyDefinition] = property_definitions![];
}
