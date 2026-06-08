use crate::processors::asciify_german::relationships::FAILURE;
use minifi_native::macros::{ComponentIdentifier, DefaultMetrics, NoAdvancedProcessorFeatures};
use minifi_native::{
    FlowFileStreamTransform, GetProperty, InputStream, Logger, MinifiError, OutputStream, Schedule,
    TransformStreamResult,
};
use std::collections::HashMap;

mod relationships;

#[derive(Debug, ComponentIdentifier, DefaultMetrics, NoAdvancedProcessorFeatures)]
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
    fn transform<Ctx: GetProperty, LoggerImpl: Logger>(
        &self,
        _context: &Ctx,
        input_stream: &mut dyn InputStream,
        output_stream: &mut dyn OutputStream,
        _logger: &LoggerImpl,
    ) -> Result<TransformStreamResult, MinifiError> {
        let mut byte = [0u8; 1];

        while input_stream.read(&mut byte)? > 0 {
            match byte[0] {
                0..=127 => {
                    output_stream.write_all(&byte)?;
                }
                0xC3 => {
                    let mut next = [0u8; 1];
                    if input_stream.read(&mut next)? > 0 {
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
                }
                _ => return Ok(TransformStreamResult::route_without_changes(&FAILURE)),
            }
        }

        output_stream.flush()?;
        Ok(TransformStreamResult::new(
            &relationships::SUCCESS,
            HashMap::new(),
        ))
    }
}

mod processor_definition;
#[cfg(test)]
mod tests;
