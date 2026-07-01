use crate::api::raw_processor::{MultiThreadedTrigger, SingleThreadedTrigger};
use crate::{
    ComponentIdentifier, Concurrent, Exclusive, Logger, MinifiError, OnTriggerResult,
    ProcessContext, ProcessSession, Processor, ProcessorDefinition, Schedule,
};

pub trait MutTrigger {
    fn trigger<Ctx, Session, Lggr>(
        &mut self,
        context: &mut Ctx,
        session: &mut Session,
        logger: &Lggr,
    ) -> Result<OnTriggerResult, MinifiError>
    where
        Ctx: ProcessContext,
        Session: ProcessSession<FlowFile = Ctx::FlowFile>,
        Lggr: Logger;
}

pub trait Trigger {
    fn trigger<Context, Session, Lggr>(
        &self,
        context: &mut Context,
        session: &mut Session,
        logger: &Lggr,
    ) -> Result<OnTriggerResult, MinifiError>
    where
        Context: ProcessContext,
        Session: ProcessSession<FlowFile = Context::FlowFile>,
        Lggr: Logger;
}

pub struct ComplexProcessorType {}

impl<Implementation, L> SingleThreadedTrigger
    for Processor<Implementation, ComplexProcessorType, Exclusive, L>
where
    Implementation: Schedule + MutTrigger + ComponentIdentifier + ProcessorDefinition,
    L: Logger,
{
    fn trigger<PC, PS>(
        &mut self,
        context: &mut PC,
        session: &mut PS,
    ) -> Result<OnTriggerResult, MinifiError>
    where
        PC: ProcessContext,
        PS: ProcessSession<FlowFile = PC::FlowFile>,
    {
        if let Some(ref mut scheduled_impl) = self.scheduled_impl {
            scheduled_impl.trigger(context, session, &self.logger)
        } else {
            Err(MinifiError::trigger_err(
                "The processor hasn't been scheduled yet",
            ))
        }
    }
}

impl<Implementation, L> MultiThreadedTrigger
    for Processor<Implementation, ComplexProcessorType, Concurrent, L>
where
    Implementation: Schedule + Trigger + ComponentIdentifier + ProcessorDefinition,
    L: Logger,
{
    fn trigger<PC, PS>(
        &self,
        context: &mut PC,
        session: &mut PS,
    ) -> Result<OnTriggerResult, MinifiError>
    where
        PC: ProcessContext,
        PS: ProcessSession<FlowFile = PC::FlowFile>,
    {
        if let Some(ref scheduled_impl) = self.scheduled_impl {
            scheduled_impl.trigger(context, session, &self.logger)
        } else {
            Err(MinifiError::trigger_err(
                "The processor hasn't been scheduled yet",
            ))
        }
    }
}
