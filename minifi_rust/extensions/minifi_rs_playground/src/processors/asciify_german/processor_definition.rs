// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

use crate::processors::asciify_german::AsciifyGerman;
use minifi_native::{
    OutputAttribute, ProcessorDefinition, ProcessorInputRequirement, PropertyDefinition,
    Relationship,
};

impl ProcessorDefinition for AsciifyGerman {
    const DESCRIPTION: &'static str = "RUST TEST PROCESSOR: This processor switches German characters with their ascii counterparts. (to test stream API)";
    const INPUT_REQUIREMENT: ProcessorInputRequirement = ProcessorInputRequirement::Required;
    const SUPPORTS_DYNAMIC_PROPERTIES: bool = false;
    const SUPPORTS_DYNAMIC_RELATIONSHIPS: bool = false;
    const OUTPUT_ATTRIBUTES: &'static [OutputAttribute] = &[];
    const RELATIONSHIPS: &'static [Relationship] =
        &[super::relationships::SUCCESS, super::relationships::FAILURE];
    const PROPERTIES: &'static [PropertyDefinition] = &[];
}
