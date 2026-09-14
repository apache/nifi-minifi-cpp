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

// Test processor to test lazy logging

use minifi_native::macros::ComponentIdentifier;
use minifi_native::{
    GetProperty, Logger, MinifiError, MutTrigger, OnTriggerResult, OutputAttribute, ProcessContext,
    ProcessSession, ProcessorDefinition, ProcessorInputRequirement, PropertyDefinition,
    Relationship, Schedule, debug, info, trace,
};

#[derive(Debug, ComponentIdentifier)]
pub(crate) struct CountActualLogging {
    log_count: usize,
}

impl CountActualLogging {
    fn get_incremented_log_count(&mut self) -> usize {
        self.log_count += 1;
        self.log_count
    }
}

impl Schedule for CountActualLogging {
    fn schedule<P: GetProperty, L: Logger>(_context: &P, _logger: &L) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        Ok(Self { log_count: 0 })
    }
}

impl MutTrigger for CountActualLogging {
    fn trigger<PC, PS, L>(
        &mut self,
        _context: &mut PC,
        _session: &mut PS,
        logger: &L,
    ) -> Result<OnTriggerResult, MinifiError>
    where
        PC: ProcessContext,
        PS: ProcessSession<FlowFile = PC::FlowFile>,
        L: Logger,
    {
        trace!(logger, "trace {}", self.get_incremented_log_count());
        debug!(logger, "debug {}", self.get_incremented_log_count());
        info!(logger, "info {}", self.get_incremented_log_count());

        Ok(OnTriggerResult::Ok)
    }
}

impl ProcessorDefinition for CountActualLogging {
    const DESCRIPTION: &'static str = "RUST TEST PROCESSOR: For testing lazy logging";
    const INPUT_REQUIREMENT: ProcessorInputRequirement = ProcessorInputRequirement::Forbidden;
    const SUPPORTS_DYNAMIC_PROPERTIES: bool = false;
    const SUPPORTS_DYNAMIC_RELATIONSHIPS: bool = false;
    const OUTPUT_ATTRIBUTES: &'static [OutputAttribute] = &[];
    const RELATIONSHIPS: &'static [Relationship] = &[];
    const PROPERTIES: &'static [PropertyDefinition] = &[];
}
