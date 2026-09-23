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

use crate::processors::list_file::output_attributes::{
    ABSOLUTE_PATH_OUTPUT_ATTRIBUTE, FILE_SIZE_OUTPUT_ATTRIBUTE, FILENAME_OUTPUT_ATTRIBUTE,
};
use crate::processors::list_file::properties::{DIRECTORY, IGNORE_HIDDEN_FILES, RECURSE};
use crate::processors::list_file::{ListFileRs, relationships};
use minifi_native::{
    OutputAttribute, ProcessorDefinition, ProcessorInputRequirement, PropertyDefinition,
    Relationship, property_definitions,
};

impl ProcessorDefinition for ListFileRs {
    const DESCRIPTION: &'static str = "RUST TEST PROCESSOR: Lists files in a directory, emitting one FlowFile (without content) per newly-seen file. Already-listed files are remembered in processor state, so each file is listed once even across restarts.";
    const INPUT_REQUIREMENT: ProcessorInputRequirement = ProcessorInputRequirement::Forbidden;
    const SUPPORTS_DYNAMIC_PROPERTIES: bool = false;
    const SUPPORTS_DYNAMIC_RELATIONSHIPS: bool = false;
    const OUTPUT_ATTRIBUTES: &'static [OutputAttribute] = &[
        FILENAME_OUTPUT_ATTRIBUTE,
        ABSOLUTE_PATH_OUTPUT_ATTRIBUTE,
        FILE_SIZE_OUTPUT_ATTRIBUTE,
    ];
    const RELATIONSHIPS: &'static [Relationship] = &[relationships::SUCCESS];
    const PROPERTIES: &'static [PropertyDefinition] =
        property_definitions![DIRECTORY, RECURSE, IGNORE_HIDDEN_FILES];
}
