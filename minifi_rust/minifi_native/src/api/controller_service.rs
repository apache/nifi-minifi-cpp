use crate::api::RawControllerService;
use crate::{ComponentIdentifier, GetProperty, LogLevel, Logger, MinifiError};

pub trait EnableControllerService {
    fn enable<Ctx: GetProperty, L: Logger>(context: &Ctx, logger: &L) -> Result<Self, MinifiError>
    where
        Self: Sized;
}

#[derive(Debug)]
pub struct ControllerService<Implementation, L>
where
    Implementation: EnableControllerService + ComponentIdentifier,
    L: Logger,
{
    logger: L,
    enabled_impl: Option<Implementation>,
}

impl<Implementation, L> ControllerService<Implementation, L>
where
    Implementation: EnableControllerService + ComponentIdentifier,
    L: Logger,
{
    pub fn get_implementation(&self) -> Option<&Implementation> {
        self.enabled_impl.as_ref()
    }
}

impl<Implementation, L> RawControllerService for ControllerService<Implementation, L>
where
    Implementation: EnableControllerService + ComponentIdentifier,
    L: Logger,
{
    type LoggerType = L;

    fn new(logger: Self::LoggerType) -> Self {
        Self {
            logger,
            enabled_impl: None,
        }
    }

    fn log(&self, log_level: LogLevel, args: std::fmt::Arguments) {
        self.logger.log(log_level, args);
    }

    fn enable<P: GetProperty>(&mut self, context: &P) -> Result<(), MinifiError> {
        self.enabled_impl = Some(Implementation::enable(context, &self.logger)?);
        Ok(())
    }
}

impl<Implementation, L> ComponentIdentifier for ControllerService<Implementation, L>
where
    Implementation: EnableControllerService + ComponentIdentifier,
    L: Logger,
{
    const CLASS_NAME: &'static str = Implementation::CLASS_NAME;
    const GROUP_NAME: &'static str = Implementation::GROUP_NAME;
    const VERSION: &'static str = Implementation::VERSION;
}
