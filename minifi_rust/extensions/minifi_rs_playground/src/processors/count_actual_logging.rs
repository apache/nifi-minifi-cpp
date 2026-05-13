use minifi_native::macros::{ComponentIdentifier, DefaultMetrics, NoAdvancedProcessorFeatures};
use minifi_native::{
    GetProperty, Logger, MinifiError, MutTrigger, OnTriggerResult, OutputAttribute, ProcessContext,
    ProcessSession, ProcessorDefinition, ProcessorInputRequirement, Property, Relationship,
    Schedule, debug, info, trace,
};

#[derive(Debug, DefaultMetrics, NoAdvancedProcessorFeatures, ComponentIdentifier)]
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
    const DESCRIPTION: &'static str = "For testing lazy logging";
    const INPUT_REQUIREMENT: ProcessorInputRequirement = ProcessorInputRequirement::Forbidden;
    const SUPPORTS_DYNAMIC_PROPERTIES: bool = false;
    const SUPPORTS_DYNAMIC_RELATIONSHIPS: bool = false;
    const OUTPUT_ATTRIBUTES: &'static [OutputAttribute] = &[];
    const RELATIONSHIPS: &'static [Relationship] = &[];
    const PROPERTIES: &'static [Property] = &[];
}
