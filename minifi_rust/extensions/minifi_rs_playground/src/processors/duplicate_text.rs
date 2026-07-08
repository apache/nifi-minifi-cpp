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

use minifi_native::macros::ComponentIdentifier;
use minifi_native::{
    GetAttribute, GetControllerService, GetProperty, InputStream, Logger, MinifiError,
    MutFlowFileStreamTransform, OutputAttribute, OutputStream, ProcessError, ProcessorDefinition,
    ProcessorInputRequirement, PropertyDefinition, Relationship, Schedule, TransformStreamResult,
};

#[derive(Debug, ComponentIdentifier)]
pub(crate) struct DuplicateStreamText {}

pub(crate) const SUCCESS: Relationship = Relationship {
    name: "success",
    description: "",
};

impl Schedule for DuplicateStreamText {
    fn schedule<Ctx: GetProperty, L: Logger>(
        _context: &Ctx,
        _logger: &L,
    ) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        Ok(Self {})
    }
}

impl MutFlowFileStreamTransform for DuplicateStreamText {
    fn transform<Ctx: GetProperty + GetControllerService + GetAttribute, LoggerImpl: Logger>(
        &mut self,
        _context: &Ctx,
        input_stream: &mut dyn InputStream,
        output_stream: &mut dyn OutputStream,
        _logger: &LoggerImpl,
    ) -> Result<TransformStreamResult, ProcessError> {
        let mut byte = [0u8; 1];
        while input_stream.read(&mut byte)? > 0 {
            let _ = output_stream.write(&byte)?;
            let _ = output_stream.write(&byte)?;
        }
        Ok(TransformStreamResult::new(&SUCCESS))
    }
}

impl ProcessorDefinition for DuplicateStreamText {
    const DESCRIPTION: &'static str = "RUST TEST PROCESSOR: Duplicate text";
    const INPUT_REQUIREMENT: ProcessorInputRequirement = ProcessorInputRequirement::Required;
    const SUPPORTS_DYNAMIC_PROPERTIES: bool = false;
    const SUPPORTS_DYNAMIC_RELATIONSHIPS: bool = false;
    const OUTPUT_ATTRIBUTES: &'static [OutputAttribute] = &[];
    const RELATIONSHIPS: &'static [Relationship] = &[SUCCESS];
    fn properties() -> &'static [PropertyDefinition] {
        &[]
    }
}
