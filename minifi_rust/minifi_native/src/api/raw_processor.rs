use crate::api::errors::MinifiError;
use crate::{LogLevel, Logger, ProcessContext, ProcessSession};

pub enum ProcessorInputRequirement {
    Required,
    Allowed,
    Forbidden,
}

#[derive(Debug, PartialEq, Eq)]
pub enum OnTriggerResult {
    Ok,
    Yield,
}

/// This RawProcessor will be instantiated, and called on by the agent
pub trait RawProcessor: Sized {
    type Threading: ThreadingModel;
    type LoggerType: Logger;

    fn new(logger: Self::LoggerType) -> Self;
    fn log(&self, log_level: LogLevel, args: std::fmt::Arguments);
    fn on_schedule<P: ProcessContext>(&mut self, context: &P) -> Result<(), MinifiError>;
    fn on_unschedule(&mut self);
}

/// To differentiate between single and multithreaded processors
pub trait ThreadingModel: sealed::Sealed {
    const IS_EXCLUSIVE: bool;
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Concurrent;
impl ThreadingModel for Concurrent {
    const IS_EXCLUSIVE: bool = false;
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Exclusive;
impl ThreadingModel for Exclusive {
    const IS_EXCLUSIVE: bool = true;
}

mod sealed {
    pub trait Sealed {}
    impl Sealed for super::Concurrent {}
    impl Sealed for super::Exclusive {}
}

pub trait SingleThreadedTrigger: RawProcessor<Threading = Exclusive> {
    fn on_trigger<PC, PS>(
        &mut self,
        context: &mut PC,
        session: &mut PS,
    ) -> Result<OnTriggerResult, MinifiError>
    where
        PC: ProcessContext,
        PS: ProcessSession<FlowFile = PC::FlowFile>;
}

pub trait MultiThreadedTrigger: RawProcessor<Threading = Concurrent> {
    fn on_trigger<PC, PS>(
        &self,
        context: &mut PC,
        session: &mut PS,
    ) -> Result<OnTriggerResult, MinifiError>
    where
        PC: ProcessContext,
        PS: ProcessSession<FlowFile = PC::FlowFile>;
}
