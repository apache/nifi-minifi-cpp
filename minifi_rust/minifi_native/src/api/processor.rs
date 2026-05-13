use crate::api::{RawProcessor, ThreadingModel};
use crate::{GetProperty, LogLevel, Logger, MinifiError, ProcessContext};
use std::marker::PhantomData;

pub trait Schedule {
    fn schedule<Ctx: GetProperty, L: Logger>(
        context: &Ctx,
        logger: &L,
    ) -> Result<Self, MinifiError>
    where
        Self: Sized;

    fn unschedule(&mut self) {}
}

pub struct Processor<Impl, Kind, T, L>
where
    Impl: Schedule,
    T: ThreadingModel,
    L: Logger,
{
    pub(crate) logger: L,
    pub(crate) scheduled_impl: Option<Impl>,
    threading_model: PhantomData<T>,
    flow_file_type: PhantomData<Kind>,
}

impl<Impl, Kind, T, L> RawProcessor for Processor<Impl, Kind, T, L>
where
    Impl: Schedule,
    T: ThreadingModel,
    L: Logger,
{
    type Threading = T;
    type LoggerType = L;

    fn new(logger: Self::LoggerType) -> Self {
        Self {
            logger,
            scheduled_impl: None,
            threading_model: PhantomData,
            flow_file_type: PhantomData,
        }
    }

    fn log(&self, log_level: LogLevel, args: std::fmt::Arguments) {
        self.logger.log(log_level, args);
    }

    fn on_schedule<P: ProcessContext>(&mut self, context: &P) -> Result<(), MinifiError> {
        self.scheduled_impl = Some(Impl::schedule(context, &self.logger)?);
        Ok(())
    }

    fn on_unschedule(&mut self) {
        if let Some(ref mut scheduled_impl) = self.scheduled_impl {
            scheduled_impl.unschedule()
        }
    }
}
