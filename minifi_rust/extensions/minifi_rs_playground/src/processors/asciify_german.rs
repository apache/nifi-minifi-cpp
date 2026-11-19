// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

// This processor is used to test streaming flow file transforms, with changing FlowFile sizes

use crate::processors::asciify_german::relationships::FAILURE;
use minifi_native::macros::ComponentIdentifier;
use minifi_native::{
    FlowFileStreamTransform, GetProperty, InputStream, Logger, MinifiError, OutputStream,
    Relationship, Schedule, TransformError, TransformStreamResult, route_to_err,
};

mod relationships;

#[derive(Debug, ComponentIdentifier)]
pub(crate) struct AsciifyGerman {}

impl Schedule for AsciifyGerman {
    fn schedule<P: GetProperty, L: Logger>(_context: &P, _logger: &L) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        Ok(Self {})
    }
}

impl FlowFileStreamTransform for AsciifyGerman {
    const ERROR_RELATIONSHIP: &'static Relationship = &FAILURE;

    fn transform<Ctx: GetProperty, LoggerImpl: Logger>(
        &self,
        _context: &Ctx,
        input_stream: &mut dyn InputStream,
        output_stream: &mut dyn OutputStream,
        _logger: &LoggerImpl,
    ) -> Result<TransformStreamResult, TransformError> {
        let mut byte = [0u8; 1];

        while input_stream.read(&mut byte)? > 0 {
            match byte[0] {
                0..=127 => {
                    output_stream.write_all(&byte)?;
                }
                0xC3 => {
                    let mut next = [0u8; 1];
                    if input_stream.read(&mut next)? == 0 {
                        route_to_err!("Truncated multi-byte sequence at EOF");
                    }
                    match next[0] {
                        0xA4 => output_stream.write_all(b"ae")?, // ä
                        0xB6 => output_stream.write_all(b"oe")?, // ö
                        0xBC => output_stream.write_all(b"ue")?, // ü
                        0x84 => output_stream.write_all(b"Ae")?, // Ä
                        0x96 => output_stream.write_all(b"Oe")?, // Ö
                        0x9C => output_stream.write_all(b"Ue")?, // Ü
                        0x9F => output_stream.write_all(b"ss")?, // ß
                        _ => return Ok(TransformStreamResult::route_without_changes(&FAILURE)),
                    }
                }
                _ => return Ok(TransformStreamResult::route_without_changes(&FAILURE)),
            }
        }

        output_stream.flush()?;
        Ok(TransformStreamResult::new(&relationships::SUCCESS))
    }
}

mod processor_definition;
#[cfg(test)]
mod tests;
