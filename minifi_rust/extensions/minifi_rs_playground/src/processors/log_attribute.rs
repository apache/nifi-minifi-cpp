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

// This is the (not production ready) reimplementation of the already existing standard LogAttribute processor

use crate::processors::log_attribute::properties::{FLOW_FILES_TO_LOG, LOG_LEVEL, LOG_PAYLOAD};
use minifi_native::macros::ComponentIdentifier;
use minifi_native::{
    GetProperty, LogLevel, Logger, MinifiError, OnTriggerResult, ProcessContext, ProcessSession,
    Property, Schedule, Trigger, debug, log, trace,
};

mod properties;
mod relationships;

#[derive(Debug, ComponentIdentifier)]
pub(crate) struct LogAttributeRs {
    log_level: LogLevel,
    attributes_to_log: Option<Vec<String>>,
    attributes_to_ignore: Option<Vec<String>>,
    log_payload: bool,
    flow_files_to_log: usize,
    dash_line: String,
    hex_encode_payload: bool,
}

impl LogAttributeRs {
    fn generate_log_message<PS>(&self, session: &mut PS, flow_file: &PS::FlowFile) -> String
    where
        PS: ProcessSession,
    {
        let mut log_msg = String::with_capacity(1024);
        log_msg.push_str("Logging for flow file\n");
        log_msg.push_str(self.dash_line.as_str());

        log_msg.push_str("\nFlowFile Attributes Map Content");
        session.for_each_attribute(flow_file, |key, value| {
            if let Some(attributes_to_ignore) = &self.attributes_to_ignore
                && attributes_to_ignore.iter().any(|ign| ign == key)
            {
                return;
            }
            if let Some(attributes_to_log) = &self.attributes_to_log
                && !attributes_to_log.iter().any(|ign| ign == key)
            {
                return;
            }
            log_msg.push_str(format!("\nkey:{} value:{}", key, value).as_str());
        });
        if self.log_payload {
            log_msg.push_str("\nPayload:\n");
            if let Some(flow_file_payload) = session.read(flow_file) {
                if self.hex_encode_payload {
                    log_msg.push_str(&hex::encode(flow_file_payload));
                } else {
                    log_msg.push_str(
                        String::from_utf8(flow_file_payload)
                            .unwrap_or_default()
                            .as_str(),
                    );
                }
            }
        }
        log_msg.push('\n');
        log_msg.push_str(self.dash_line.as_str());
        log_msg
    }
}

impl Trigger for LogAttributeRs {
    fn trigger<PC, PS, L>(
        &self,
        _context: &mut PC,
        session: &mut PS,
        logger: &L,
    ) -> Result<OnTriggerResult, MinifiError>
    where
        PC: ProcessContext,
        PS: ProcessSession<FlowFile = PC::FlowFile>,
        L: Logger,
    {
        trace!(
            logger,
            "enter log attribute, attempting to retrieve {} flow files", self.flow_files_to_log
        );
        let max_flow_files_to_process = if self.flow_files_to_log == 0 {
            usize::MAX
        } else {
            self.flow_files_to_log
        };
        let mut flow_files_processed = 0usize;
        for _ in 0..max_flow_files_to_process {
            if let Some(flow_file) = session.get() {
                let log_msg = self.generate_log_message(session, &flow_file);
                log!(logger, self.log_level, "{}", log_msg);
                session.transfer(flow_file, relationships::SUCCESS.name)?;
                flow_files_processed += 1;
            } else {
                break;
            }
        }
        debug!(logger, "Logged {} flow files", flow_files_processed);

        Ok(OnTriggerResult::Ok)
    }
}

impl Schedule for LogAttributeRs {
    fn schedule<P: GetProperty, L>(context: &P, _logger: &L) -> Result<Self, MinifiError> {
        let log_level = context.get_req_property::<LogLevel>(&LOG_LEVEL)?;
        let log_payload = context.get_req_property::<bool>(&LOG_PAYLOAD)?;
        let flow_files_to_log = context.get_req_property::<usize>(&FLOW_FILES_TO_LOG)?;

        fn get_csv_property<P: GetProperty>(
            context: &P,
            property: &Property,
        ) -> Result<Option<Vec<String>>, MinifiError> {
            Ok(context.get_property::<String>(property)?.map(|s| {
                s.split(',')
                    .map(|s| s.trim().to_string())
                    .collect::<Vec<String>>()
            }))
        }

        let attributes_to_log = get_csv_property(context, &properties::ATTRIBUTES_TO_LOG)?;
        let attributes_to_ignore = get_csv_property(context, &properties::ATTRIBUTES_TO_IGNORE)?;

        let dash_line = format!(
            "{:-^50}",
            context
                .get_property::<String>(&properties::LOG_PREFIX)?
                .unwrap_or_default()
        );

        let hex_encode_payload =
            context.get_req_property::<bool>(&properties::HEX_ENCODE_PAYLOAD)?;

        Ok(LogAttributeRs {
            log_level,
            attributes_to_log,
            attributes_to_ignore,
            log_payload,
            flow_files_to_log,
            dash_line,
            hex_encode_payload,
        })
    }
}

pub(crate) mod processor_definition;

#[cfg(test)]
mod tests;
