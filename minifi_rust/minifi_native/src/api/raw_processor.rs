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
    fn schedule<P: ProcessContext>(&mut self, context: &P) -> Result<(), MinifiError>;
    fn unschedule(&mut self);
}

/// To differentiate between single and multithreaded processors
pub trait ThreadingModel: sealed::Sealed {
    const IS_EXCLUSIVE: bool;
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct MultiThreaded;
impl ThreadingModel for MultiThreaded {
    const IS_EXCLUSIVE: bool = false;
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct SingleThreaded;
impl ThreadingModel for SingleThreaded {
    const IS_EXCLUSIVE: bool = true;
}

mod sealed {
    pub trait Sealed {}
    impl Sealed for super::MultiThreaded {}
    impl Sealed for super::SingleThreaded {}
}

pub trait SingleThreadedTrigger: RawProcessor<Threading = SingleThreaded> {
    fn trigger<PC, PS>(
        &mut self,
        context: &mut PC,
        session: &mut PS,
    ) -> Result<OnTriggerResult, MinifiError>
    where
        PC: ProcessContext,
        PS: ProcessSession<FlowFile = PC::FlowFile>;
}

pub trait MultiThreadedTrigger: RawProcessor<Threading = MultiThreaded> {
    fn trigger<PC, PS>(
        &self,
        context: &mut PC,
        session: &mut PS,
    ) -> Result<OnTriggerResult, MinifiError>
    where
        PC: ProcessContext,
        PS: ProcessSession<FlowFile = PC::FlowFile>;
}
