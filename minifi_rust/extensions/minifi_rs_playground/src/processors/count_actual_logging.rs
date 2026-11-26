// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

// Test processor to test lazy logging

use minifi_native::macros::ComponentIdentifier;
use minifi_native::{
    GetProperty, Logger, MinifiError, MutTrigger, OnTriggerResult, OutputAttribute,
    ProcessorDefinition, ProcessorInputRequirement, PropertyDefinition, Relationship, Schedule,
    TriggerContext, debug, info, trace,
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
    fn trigger<TC, L>(
        &mut self,
        _context: &mut TC,
        logger: &L,
    ) -> Result<OnTriggerResult, MinifiError>
    where
        TC: TriggerContext,
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
