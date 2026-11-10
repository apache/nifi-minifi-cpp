// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

use super::*;
use minifi_native::{
    OutputAttribute, ProcessorDefinition, ProcessorInputRequirement, PropertyDefinition,
    Relationship, property_definitions,
};

impl ProcessorDefinition for KamikazeProcessorRs {
    const DESCRIPTION: &'static str = "RUST TEST PROCESSOR: This processor can fail or panic in on_trigger and on_schedule calls based on configuration. Only for testing purposes.";
    const INPUT_REQUIREMENT: ProcessorInputRequirement = ProcessorInputRequirement::Allowed;
    const SUPPORTS_DYNAMIC_PROPERTIES: bool = false;
    const SUPPORTS_DYNAMIC_RELATIONSHIPS: bool = false;
    const OUTPUT_ATTRIBUTES: &'static [OutputAttribute] = &[];
    const RELATIONSHIPS: &'static [Relationship] = &[relationships::SUCCESS];
    const PROPERTIES: &'static [PropertyDefinition] = property_definitions![
        properties::SCHEDULE_BEHAVIOUR,
        properties::TRIGGER_BEHAVIOUR
    ];
}
