// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

use super::LoremIpsumCSUser;
use super::properties::*;
use crate::processors::lorem_ipsum_cs_user::relationships::SUCCESS;
use minifi_native::{
    OutputAttribute, ProcessorDefinition, ProcessorInputRequirement, PropertyDefinition,
    Relationship, property_definitions,
};

impl ProcessorDefinition for LoremIpsumCSUser {
    const DESCRIPTION: &'static str =
        "RUST TEST PROCESSOR: Processor to test Controller Service API";
    const INPUT_REQUIREMENT: ProcessorInputRequirement = ProcessorInputRequirement::Forbidden;
    const SUPPORTS_DYNAMIC_PROPERTIES: bool = false;
    const SUPPORTS_DYNAMIC_RELATIONSHIPS: bool = false;
    const OUTPUT_ATTRIBUTES: &'static [OutputAttribute] = &[];
    const RELATIONSHIPS: &'static [Relationship] = &[SUCCESS];
    const PROPERTIES: &'static [PropertyDefinition] =
        property_definitions![CONTROLLER_SERVICE, DUMMY_CONTROLLER_SERVICE, WRITE_METHOD];
}
