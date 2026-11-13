// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

use minifi_native::macros::ComponentIdentifier;
use minifi_native::{
    GetAttribute, GetControllerService, GetProperty, InputStream, Logger, MinifiError,
    MutFlowFileStreamTransform, OutputAttribute, OutputStream, ProcessorDefinition,
    ProcessorInputRequirement, PropertyDefinition, Relationship, Schedule, TransformError,
    TransformStreamResult,
};

#[derive(Debug, ComponentIdentifier)]
pub(crate) struct DuplicateStreamText {}

pub(crate) const SUCCESS: Relationship = Relationship {
    name: "success",
    description: "",
};

pub(crate) const FAILURE: Relationship = Relationship {
    name: "failure",
    description: "Flowfiles that could not be duplicated are routed here",
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
    const ERROR_RELATIONSHIP: &'static Relationship = &FAILURE;

    fn transform<Ctx: GetProperty + GetControllerService + GetAttribute, LoggerImpl: Logger>(
        &mut self,
        _context: &Ctx,
        input_stream: &mut dyn InputStream,
        output_stream: &mut dyn OutputStream,
        _logger: &LoggerImpl,
    ) -> Result<TransformStreamResult, TransformError> {
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
    const RELATIONSHIPS: &'static [Relationship] = &[SUCCESS, FAILURE];
    const PROPERTIES: &'static [PropertyDefinition] = &[];
}
