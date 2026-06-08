use minifi_native::macros::{ComponentIdentifier, NoAdvancedProcessorFeatures};
use minifi_native::{
    CalculateMetrics, GetProperty, Logger, MinifiError, OnTriggerResult, ProcessContext,
    ProcessSession, Schedule, Trigger,
};
use rand::RngExt;
use rand::distr::Alphanumeric;
use std::cmp::PartialEq;

mod properties;
mod relationships;

#[derive(Debug, PartialEq)]
enum Mode {
    UniqueBytes,
    UniqueText,
    NotUniqueBytes,
    NotUniqueText,
    CustomText,
    Empty,
}

#[derive(Debug, ComponentIdentifier, NoAdvancedProcessorFeatures)]
pub(crate) struct GenerateFlowFileRs {
    mode: Mode,
    batch_size: u64,
    file_size: u64,
    data_generated_during_on_schedule: Vec<u8>,
}

impl Schedule for GenerateFlowFileRs {
    fn schedule<P: GetProperty, L: Logger>(context: &P, _logger: &L) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        let is_unique = context
            .get_bool_property(&properties::UNIQUE_FLOW_FILES)?
            .expect("Required property");
        let is_text = context
            .get_property(&properties::DATA_FORMAT)?
            .expect("Required property")
            .as_str()
            == "Text";
        let has_custom_text = context.get_property(&properties::CUSTOM_TEXT)?.is_some();

        let file_size = context
            .get_size_property(&properties::FILE_SIZE)?
            .expect("Required property");
        let batch_size = context
            .get_u64_property(&properties::BATCH_SIZE)?
            .expect("Required property");

        let mode = Self::get_mode(is_unique, is_text, has_custom_text, file_size);
        let data_generated_during_on_schedule =
            if mode == Mode::NotUniqueText || mode == Mode::NotUniqueBytes {
                let mut data = vec![0; file_size as usize];
                Self::generate_data(&mut data, is_text);
                data
            } else {
                vec![]
            };

        Ok(Self {
            mode,
            batch_size,
            file_size,
            data_generated_during_on_schedule,
        })
    }
}

impl GenerateFlowFileRs {
    fn is_unique(&self) -> bool {
        match self.mode {
            Mode::UniqueBytes => true,
            Mode::UniqueText => true,
            Mode::NotUniqueBytes => false,
            Mode::NotUniqueText => false,
            Mode::CustomText => false,
            Mode::Empty => false,
        }
    }

    fn is_text(&self) -> bool {
        match self.mode {
            Mode::UniqueBytes => false,
            Mode::UniqueText => true,
            Mode::NotUniqueBytes => false,
            Mode::NotUniqueText => true,
            Mode::CustomText => true,
            Mode::Empty => false,
        }
    }

    fn get_mode(is_unique: bool, is_text: bool, has_custom_text: bool, file_size: u64) -> Mode {
        if is_text && !is_unique && has_custom_text {
            return Mode::CustomText;
        }

        if file_size == 0 {
            return Mode::Empty;
        }

        match (is_unique, is_text) {
            (true, true) => Mode::UniqueText,
            (true, false) => Mode::UniqueBytes,
            (false, true) => Mode::NotUniqueText,
            (false, false) => Mode::NotUniqueBytes,
        }
    }

    fn generate_data(data: &mut [u8], text_data: bool) {
        let mut rng = rand::rng();

        if text_data {
            for byte in data.iter_mut() {
                *byte = rng.sample(Alphanumeric);
            }
        } else {
            rng.fill(data);
        }
    }
}

impl Trigger for GenerateFlowFileRs {
    fn trigger<PC, PS, L>(
        &self,
        context: &mut PC,
        session: &mut PS,
        _logger: &L,
    ) -> Result<OnTriggerResult, MinifiError>
    where
        PC: ProcessContext,
        PS: ProcessSession<FlowFile = PC::FlowFile>,
    {
        let non_unique_data_buffer: &[u8];
        let custom_text_for_batch: Option<String>;

        if self.mode == Mode::CustomText
            && let Some(custom_text) = context.get_property(&properties::CUSTOM_TEXT, None)?
        {
            custom_text_for_batch = Some(custom_text);
            non_unique_data_buffer = custom_text_for_batch.as_ref().unwrap().as_bytes();
        } else {
            non_unique_data_buffer = self.data_generated_during_on_schedule.as_slice();
        }

        for _ in 0..self.batch_size {
            let mut ff = session.create()?;
            if self.mode != Mode::Empty {
                if self.is_unique() {
                    let mut unique_data: Vec<u8> = vec![0; self.file_size as usize];
                    Self::generate_data(&mut unique_data, self.is_text());
                    session.write(&mut ff, unique_data.as_slice())?;
                } else {
                    session.write(&mut ff, non_unique_data_buffer)?;
                }
            }
            session.transfer(ff, relationships::SUCCESS.name)?;
        }
        Ok(OnTriggerResult::Ok)
    }
}

impl CalculateMetrics for GenerateFlowFileRs {}

pub(crate) mod processor_definition;

#[cfg(test)]
mod tests;
