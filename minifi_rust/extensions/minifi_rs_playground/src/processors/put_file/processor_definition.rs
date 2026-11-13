// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

use super::*;
use minifi_native::{
    OutputAttribute, ProcessorDefinition, ProcessorInputRequirement, PropertyDefinition,
    Relationship, property_definitions,
};

impl ProcessorDefinition for PutFileRs {
    const DESCRIPTION: &'static str =
        "RUST TEST PROCESSOR: Writes the contents of a FlowFile to the local file system.";
    const INPUT_REQUIREMENT: ProcessorInputRequirement = ProcessorInputRequirement::Required;
    const SUPPORTS_DYNAMIC_PROPERTIES: bool = false;
    const SUPPORTS_DYNAMIC_RELATIONSHIPS: bool = false;
    const OUTPUT_ATTRIBUTES: &'static [OutputAttribute] = &[];
    const RELATIONSHIPS: &'static [Relationship] = &[SUCCESS, FAILURE];

    #[cfg(windows)]
    const PROPERTIES: &'static [PropertyDefinition] = property_definitions![
        properties::DIRECTORY,
        properties::CONFLICT_RESOLUTION,
        properties::CREATE_DIRS,
        properties::MAX_FILE_COUNT,
    ];

    #[cfg(unix)]
    const PROPERTIES: &'static [PropertyDefinition] = property_definitions![
        properties::DIRECTORY,
        properties::CONFLICT_RESOLUTION,
        properties::CREATE_DIRS,
        properties::MAX_FILE_COUNT,
        unix_only_properties::PERMISSIONS,
        unix_only_properties::DIRECTORY_PERMISSIONS,
    ];
}
