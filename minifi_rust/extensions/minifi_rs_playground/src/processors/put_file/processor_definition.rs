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
