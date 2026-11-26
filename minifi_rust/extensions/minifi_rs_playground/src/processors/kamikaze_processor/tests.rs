// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

use super::*;
use crate::processors::kamikaze_processor::properties::{SCHEDULE_BEHAVIOUR, TRIGGER_BEHAVIOUR};
use minifi_native::{MockLogger, MockScheduleContext, MockTriggerContext};
use std::panic::AssertUnwindSafe;

#[test]
fn on_schedule_ok() {
    let context = MockScheduleContext::new();
    let processor = KamikazeProcessorRs::schedule(&context, &MockLogger::new());
    assert!(processor.is_ok());
}

#[test]
fn on_schedule_err() {
    let mut context = MockScheduleContext::new();
    context.properties.insert(
        SCHEDULE_BEHAVIOUR.name().to_string(),
        "ReturnErr".to_string(),
    );
    let processor = KamikazeProcessorRs::schedule(&context, &MockLogger::new());
    assert!(matches!(processor, Err(MinifiError::CustomError(_))));
}

#[test]
fn on_schedule_panic() {
    let mut context = MockScheduleContext::new();
    context
        .properties
        .insert(SCHEDULE_BEHAVIOUR.name().to_string(), "Panic".to_string());

    let result = std::panic::catch_unwind(AssertUnwindSafe(|| {
        KamikazeProcessorRs::schedule(&context, &MockLogger::new())
    }));
    assert!(result.is_err());
}

#[test]
fn on_trigger_ok() {
    let context = MockScheduleContext::new();
    let processor = KamikazeProcessorRs::schedule(&context, &MockLogger::new()).unwrap();

    let mut trigger_context = MockTriggerContext::from(context);
    assert_eq!(
        processor
            .trigger(&mut trigger_context, &MockLogger::new())
            .expect("Should trigger successfully"),
        OnTriggerResult::Ok
    );
}

#[test]
fn on_trigger_err() {
    let mut context = MockScheduleContext::new();
    context.properties.insert(
        TRIGGER_BEHAVIOUR.name().to_string(),
        "ReturnErr".to_string(),
    );
    let processor = KamikazeProcessorRs::schedule(&context, &MockLogger::new()).unwrap();

    let mut trigger_context = MockTriggerContext::from(context);
    assert!(matches!(
        processor.trigger(&mut trigger_context, &MockLogger::new()),
        Err(MinifiError::CustomError(_))
    ));
}

#[test]
fn on_trigger_panic() {
    let mut context = MockScheduleContext::new();
    context
        .properties
        .insert(TRIGGER_BEHAVIOUR.name().to_string(), "Panic".to_string());
    let processor = KamikazeProcessorRs::schedule(&context, &MockLogger::new()).unwrap();

    let mut trigger_context = MockTriggerContext::from(context);
    let result = std::panic::catch_unwind(AssertUnwindSafe(|| {
        processor.trigger(&mut trigger_context, &MockLogger::new())
    }));
    assert!(result.is_err());
}
