// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

use crate::GetControllerService;
use crate::api::{RawProcessor, ThreadingModel};
use crate::{
    GetProperty, LogLevel, Logger, MinifiError, ProcessContext, SingleThreaded, StateAccess,
    StateSchema, Stateful, Stateless,
};
use std::collections::HashMap;
use std::marker::PhantomData;

pub trait Schedule {
    fn schedule<Ctx: GetProperty + GetControllerService, L: Logger>(
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
    // The persisted fields as `schedule` left them, so a reload can restore a field the store has no entry for.
    pub(crate) scheduled_state: Option<HashMap<String, String>>,
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
            scheduled_state: None,
            threading_model: PhantomData,
            flow_file_type: PhantomData,
        }
    }

    fn log(&self, log_level: LogLevel, args: std::fmt::Arguments) {
        self.logger.log(log_level, args);
    }

    fn schedule<P: ProcessContext>(&mut self, context: &P) -> Result<(), MinifiError> {
        self.scheduled_impl = Some(Impl::schedule(context, &self.logger)?);
        self.scheduled_state = None;
        Ok(())
    }

    fn unschedule(&mut self) {
        if let Some(ref mut scheduled_impl) = self.scheduled_impl {
            scheduled_impl.unschedule()
        }
    }
}

/// Bridges the single-threaded trigger dispatch to a processor's [`StateSchema`]. A no-op for
/// [`SingleThreaded`]; only [`crate::SingleThreadedStateful`] processors need a schema and perform state I/O.
///
/// State is read before and written after every trigger, never cached across them: a write stays pending until the
/// session commits, and a rollback discards it without telling this side, so a cache could claim progress that was
/// never persisted. C++ ListFile re-reads per trigger for the same reason.
pub trait SingleThreadedStateHooks {
    fn load_persisted_state<PC: StateAccess>(&mut self, context: &PC) -> Result<(), MinifiError>;

    fn save_persisted_state<PC: StateAccess>(&self, context: &PC) -> Result<(), MinifiError>;
}

impl<Impl, Kind, L> SingleThreadedStateHooks for Processor<Impl, Kind, SingleThreaded<Stateless>, L>
where
    Impl: Schedule,
    L: Logger,
{
    fn load_persisted_state<PC: StateAccess>(&mut self, _context: &PC) -> Result<(), MinifiError> {
        Ok(())
    }

    fn save_persisted_state<PC: StateAccess>(&self, _context: &PC) -> Result<(), MinifiError> {
        Ok(())
    }
}

impl<Impl, Kind, L> SingleThreadedStateHooks for Processor<Impl, Kind, SingleThreaded<Stateful>, L>
where
    Impl: Schedule + StateSchema,
    L: Logger,
{
    fn load_persisted_state<PC: StateAccess>(&mut self, context: &PC) -> Result<(), MinifiError> {
        let stored = context.get_state()?;
        if let Some(ref mut scheduled_impl) = self.scheduled_impl {
            if self.scheduled_state.is_none() {
                let mut scheduled = HashMap::new();
                scheduled_impl.store_to_state(&mut scheduled)?;
                self.scheduled_state = Some(scheduled);
            }
            // `load_from_state` only applies entries it finds, so seed with the scheduled values first. Otherwise a
            // field the store lacks keeps what the previous trigger put there - after a rollback, work to be redone.
            if let Some(ref scheduled) = self.scheduled_state {
                scheduled_impl.load_from_state(scheduled)?;
            }
            scheduled_impl.load_from_state(&stored)?;
        }
        Ok(())
    }

    fn save_persisted_state<PC: StateAccess>(&self, context: &PC) -> Result<(), MinifiError> {
        let Some(ref scheduled_impl) = self.scheduled_impl else {
            return Ok(());
        };
        // Sent unconditionally: only the agent side can compare against the real stored state.
        let mut next = HashMap::new();
        scheduled_impl.store_to_state(&mut next)?;
        context.set_state(&next)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::macros::{ComponentIdentifier, StateSchema};
    use crate::{ListingState, MockLogger, MockProcessContext, SingleThreadedStateful};

    #[derive(Debug, Default, ComponentIdentifier, StateSchema)]
    struct Stateful {
        #[persisted]
        cursor: u64,
        #[persisted(flatten)]
        listing: ListingState,
        // Untagged on purpose: asserted to stay out of the store.
        #[allow(dead_code)]
        transient: u64,
    }

    #[derive(Debug, Default, ComponentIdentifier)]
    struct NoState {
        #[allow(dead_code)]
        transient: u64,
    }

    // `schedule` can derive a starting value from properties, so the resync fallback must be what it produced and
    // not `Default`. Every other fixture here makes the two coincide.
    #[derive(Debug, Default, ComponentIdentifier, StateSchema)]
    struct ScheduledNonDefault {
        #[persisted]
        cursor: u64,
    }

    impl Schedule for ScheduledNonDefault {
        fn schedule<Ctx: GetProperty + GetControllerService, L: Logger>(
            _context: &Ctx,
            _logger: &L,
        ) -> Result<Self, MinifiError> {
            Ok(Self { cursor: 42 })
        }
    }

    macro_rules! impl_schedule {
        ($ty:ty) => {
            impl Schedule for $ty {
                fn schedule<Ctx: GetProperty + GetControllerService, L: Logger>(
                    _context: &Ctx,
                    _logger: &L,
                ) -> Result<Self, MinifiError> {
                    Ok(Self::default())
                }
            }
        };
    }
    impl_schedule!(Stateful);
    impl_schedule!(NoState);

    // `Kind` only tags the flow file type and is unconstrained here.
    type StatefulProcessor = Processor<Stateful, (), SingleThreadedStateful, MockLogger>;
    type StatelessProcessor = Processor<NoState, (), SingleThreaded, MockLogger>;

    fn scheduled_stateful(context: &MockProcessContext) -> StatefulProcessor {
        let mut processor = StatefulProcessor::new(MockLogger::new());
        processor.schedule(context).unwrap();
        processor
    }

    #[test]
    fn a_stateless_processor_performs_no_state_io() {
        let context = MockProcessContext::new();
        let mut processor = StatelessProcessor::new(MockLogger::new());
        processor.schedule(&context).unwrap();

        processor.load_persisted_state(&context).unwrap();
        processor.save_persisted_state(&context).unwrap();

        assert_eq!(context.state_writes.get(), 0);
        assert!(context.committed_state().is_empty());
    }

    #[test]
    fn persisted_fields_are_stored_in_their_plain_form() {
        let context = MockProcessContext::new();
        let mut processor = scheduled_stateful(&context);
        processor.load_persisted_state(&context).unwrap();

        processor.scheduled_impl.as_mut().unwrap().cursor = 7;
        processor.save_persisted_state(&context).unwrap();

        let stored = context.committed_state();
        assert_eq!(stored.get("cursor").map(String::as_str), Some("7"));
        assert!(!stored.contains_key("transient"));
    }

    #[test]
    fn saving_an_unchanged_state_does_not_write() {
        let context = MockProcessContext::new();
        let mut processor = scheduled_stateful(&context);
        processor.load_persisted_state(&context).unwrap();

        processor.scheduled_impl.as_mut().unwrap().cursor = 1;
        processor.save_persisted_state(&context).unwrap();
        let writes_after_change = context.storage_writes.get();
        assert_eq!(writes_after_change, 1);

        // Two triggers that touch nothing. Still sent, but must not reach storage.
        processor.save_persisted_state(&context).unwrap();
        processor.save_persisted_state(&context).unwrap();
        assert_eq!(context.storage_writes.get(), writes_after_change);

        processor.scheduled_impl.as_mut().unwrap().cursor = 2;
        processor.save_persisted_state(&context).unwrap();
        assert_eq!(context.storage_writes.get(), writes_after_change + 1);
    }

    #[test]
    fn a_populated_listing_state_still_skips_unchanged_saves() {
        let context = MockProcessContext::new();
        let mut processor = scheduled_stateful(&context);
        processor.load_persisted_state(&context).unwrap();
        {
            let listing = &mut processor.scheduled_impl.as_mut().unwrap().listing;
            listing.update("a.txt", 100);
            listing.update("b.txt", 100);
        }
        processor.save_persisted_state(&context).unwrap();
        let writes = context.storage_writes.get();
        assert_eq!(writes, 1);

        // The idle-poll case: a flattened group rewrites every entry each save, so it only stays off storage if
        // that rewrite is byte-identical.
        processor.save_persisted_state(&context).unwrap();
        processor.save_persisted_state(&context).unwrap();
        assert_eq!(context.storage_writes.get(), writes);
    }

    #[test]
    fn a_save_replaces_the_whole_map() {
        let context = MockProcessContext::new();
        context
            .set_state(&HashMap::from([("stale".to_owned(), "gone".to_owned())]))
            .unwrap();

        let mut processor = scheduled_stateful(&context);
        processor.load_persisted_state(&context).unwrap();
        processor.scheduled_impl.as_mut().unwrap().cursor = 3;
        processor.save_persisted_state(&context).unwrap();

        let stored = context.committed_state();
        // The `#[persisted]` fields own the state; nothing can write behind the framework's back.
        assert!(!stored.contains_key("stale"));
        assert_eq!(stored.get("cursor").map(String::as_str), Some("3"));
    }

    #[test]
    fn state_is_restored_into_a_freshly_scheduled_processor() {
        let context = MockProcessContext::new();
        let mut first = scheduled_stateful(&context);
        first.load_persisted_state(&context).unwrap();
        {
            let stateful = first.scheduled_impl.as_mut().unwrap();
            stateful.cursor = 9;
            stateful.listing.update("a.txt", 100);
        }
        first.save_persisted_state(&context).unwrap();

        let mut second = scheduled_stateful(&context);
        second.load_persisted_state(&context).unwrap();
        let restored = second.scheduled_impl.as_ref().unwrap();
        assert_eq!(restored.cursor, 9);
        assert!(restored.listing.was_listed_already("a.txt", 100));
    }

    #[test]
    fn rescheduling_reloads_state_on_the_next_trigger() {
        let context = MockProcessContext::new();
        let mut processor = scheduled_stateful(&context);
        processor.load_persisted_state(&context).unwrap();
        processor.scheduled_impl.as_mut().unwrap().cursor = 5;
        processor.save_persisted_state(&context).unwrap();

        processor.schedule(&context).unwrap();
        assert_eq!(processor.scheduled_impl.as_ref().unwrap().cursor, 0);
        processor.load_persisted_state(&context).unwrap();
        assert_eq!(processor.scheduled_impl.as_ref().unwrap().cursor, 5);
    }

    #[test]
    fn rollback_discards_the_save_so_the_next_trigger_reloads() {
        let context = MockProcessContext::new();
        let mut processor = scheduled_stateful(&context);

        context.begin_transaction();
        processor.load_persisted_state(&context).unwrap();
        processor.scheduled_impl.as_mut().unwrap().cursor = 7;
        processor.save_persisted_state(&context).unwrap();

        context.rollback();
        assert!(context.committed_state().is_empty());

        // Nothing told it the rollback happened; the per-trigger read is enough on its own.
        processor.load_persisted_state(&context).unwrap();
        assert_eq!(processor.scheduled_impl.as_ref().unwrap().cursor, 0);
    }

    #[test]
    fn an_error_in_a_later_trigger_discards_earlier_saves_in_the_same_session() {
        let context = MockProcessContext::new();
        context
            .set_state(&HashMap::from([("cursor".to_owned(), "1".to_owned())]))
            .unwrap();

        let mut processor = scheduled_stateful(&context);

        // The event-driven agent runs many triggers against one session, for a whole time slice.
        context.begin_transaction();
        processor.load_persisted_state(&context).unwrap();
        assert_eq!(processor.scheduled_impl.as_ref().unwrap().cursor, 1);

        for cursor in [2, 3] {
            processor.scheduled_impl.as_mut().unwrap().cursor = cursor;
            processor.save_persisted_state(&context).unwrap();
        }
        // The session commits once, after the slice, so nothing is durable yet.
        assert_eq!(
            context.committed_state().get("cursor").map(String::as_str),
            Some("1")
        );

        // A later trigger fails, so the agent discards every save made during the slice.
        context.rollback();
        assert_eq!(
            context.committed_state().get("cursor").map(String::as_str),
            Some("1")
        );

        processor.load_persisted_state(&context).unwrap();
        assert_eq!(processor.scheduled_impl.as_ref().unwrap().cursor, 1);
    }

    #[test]
    fn a_rollback_this_side_cannot_see_still_resyncs() {
        let context = MockProcessContext::new();
        context
            .set_state(&HashMap::from([("cursor".to_owned(), "1".to_owned())]))
            .unwrap();
        let mut processor = scheduled_stateful(&context);

        context.begin_transaction();
        processor.load_persisted_state(&context).unwrap();
        processor.scheduled_impl.as_mut().unwrap().cursor = 7;
        processor.save_persisted_state(&context).unwrap();

        // A commit that throws rolls back after the trigger already reported success, so nothing on this side is
        // ever told. Only the per-trigger read makes that survivable.
        context.rollback();

        processor.load_persisted_state(&context).unwrap();
        assert_eq!(processor.scheduled_impl.as_ref().unwrap().cursor, 1);
    }

    #[test]
    fn a_resync_falls_back_to_the_scheduled_value_not_the_type_default() {
        let context = MockProcessContext::new();
        let mut processor =
            Processor::<ScheduledNonDefault, (), SingleThreadedStateful, MockLogger>::new(
                MockLogger::new(),
            );
        processor.schedule(&context).unwrap();

        context.begin_transaction();
        processor.load_persisted_state(&context).unwrap();
        assert_eq!(processor.scheduled_impl.as_ref().unwrap().cursor, 42);

        processor.scheduled_impl.as_mut().unwrap().cursor = 99;
        processor.save_persisted_state(&context).unwrap();
        context.rollback();

        processor.load_persisted_state(&context).unwrap();
        assert_eq!(processor.scheduled_impl.as_ref().unwrap().cursor, 42);
    }

    #[test]
    fn an_event_driven_session_coalesces_saves_into_one_storage_write() {
        let context = MockProcessContext::new();
        let mut processor = scheduled_stateful(&context);

        context.begin_transaction();
        processor.load_persisted_state(&context).unwrap();
        for cursor in 1..=3 {
            processor.scheduled_impl.as_mut().unwrap().cursor = cursor;
            processor.save_persisted_state(&context).unwrap();
        }
        // Every trigger saves, but an open session keeps all of them off storage.
        assert_eq!(context.state_writes.get(), 3);
        assert_eq!(context.storage_writes.get(), 0);

        context.commit();
        assert_eq!(context.storage_writes.get(), 1);
        assert_eq!(
            context.committed_state().get("cursor").map(String::as_str),
            Some("3")
        );
    }

    #[test]
    fn a_reload_inside_a_session_sees_the_sessions_own_pending_write() {
        let context = MockProcessContext::new();
        let mut processor = scheduled_stateful(&context);

        // Each trigger in a shared session reloads, so it must see the last one's pending write: a flattened field
        // would read an empty map as "watermark 0, no keys" and wipe itself.
        context.begin_transaction();
        processor.load_persisted_state(&context).unwrap();
        processor.scheduled_impl.as_mut().unwrap().cursor = 5;
        processor
            .scheduled_impl
            .as_mut()
            .unwrap()
            .listing
            .update("a.txt", 100);
        processor.save_persisted_state(&context).unwrap();

        processor.load_persisted_state(&context).unwrap();
        let reloaded = processor.scheduled_impl.as_ref().unwrap();
        assert_eq!(reloaded.cursor, 5);
        assert!(reloaded.listing.was_listed_already("a.txt", 100));
    }
}
