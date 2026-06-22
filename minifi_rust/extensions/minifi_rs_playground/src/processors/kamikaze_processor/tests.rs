use super::*;
use crate::processors::kamikaze_processor::properties::{
    SCHEDULE_BEHAVIOUR, TRIGGER_BEHAVIOUR,
};
use minifi_native::MinifiError::{ScheduleError, TriggerError};
use minifi_native::{MockLogger, MockProcessContext, MockProcessSession};
use std::panic::AssertUnwindSafe;

#[test]
fn on_schedule_ok() {
    let context = MockProcessContext::new();
    let processor = KamikazeProcessorRs::schedule(&context, &MockLogger::new());
    assert!(processor.is_ok());
}

#[test]
fn on_schedule_err() {
    let mut context = MockProcessContext::new();
    context.properties.insert(
        SCHEDULE_BEHAVIOUR.name.to_string(),
        "ReturnErr".to_string(),
    );
    let processor = KamikazeProcessorRs::schedule(&context, &MockLogger::new());
    assert!(matches!(processor, Err(ScheduleError(_))));
}

#[test]
fn on_schedule_panic() {
    let mut context = MockProcessContext::new();
    context
        .properties
        .insert(SCHEDULE_BEHAVIOUR.name.to_string(), "Panic".to_string());

    let result = std::panic::catch_unwind(AssertUnwindSafe(|| {
        KamikazeProcessorRs::schedule(&context, &MockLogger::new())
    }));
    assert!(result.is_err());
}

#[test]
fn on_trigger_ok() {
    let mut context = MockProcessContext::new();
    let processor = KamikazeProcessorRs::schedule(&context, &MockLogger::new()).unwrap();

    let mut session = MockProcessSession::new();
    assert_eq!(
        processor
            .trigger(&mut context, &mut session, &MockLogger::new())
            .expect("Should trigger successfully"),
        OnTriggerResult::Ok
    );
}

#[test]
fn on_trigger_err() {
    let mut context = MockProcessContext::new();
    context.properties.insert(
        TRIGGER_BEHAVIOUR.name.to_string(),
        "ReturnErr".to_string(),
    );
    let processor = KamikazeProcessorRs::schedule(&context, &MockLogger::new()).unwrap();

    let mut session = MockProcessSession::new();
    assert!(matches!(
        processor.trigger(&mut context, &mut session, &MockLogger::new()),
        Err(TriggerError(_))
    ));
}

#[test]
fn on_trigger_panic() {
    let mut context = MockProcessContext::new();
    context
        .properties
        .insert(TRIGGER_BEHAVIOUR.name.to_string(), "Panic".to_string());
    let processor = KamikazeProcessorRs::schedule(&context, &MockLogger::new()).unwrap();

    let mut session = MockProcessSession::new();
    let result = std::panic::catch_unwind(AssertUnwindSafe(|| {
        processor.trigger(&mut context, &mut session, &MockLogger::new())
    }));
    assert!(result.is_err());
}
