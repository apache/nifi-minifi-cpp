// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

use super::*;
use crate::processors::kamikaze_processor::properties::{SCHEDULE_BEHAVIOUR, TRIGGER_BEHAVIOUR};
use minifi_native::{MockLogger, MockProcessContext, MockProcessSession, ProcessError};
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
        SCHEDULE_BEHAVIOUR.name().to_string(),
        "ReturnErr".to_string(),
    );
    let processor = KamikazeProcessorRs::schedule(&context, &MockLogger::new());
    assert!(matches!(processor, Err(MinifiError::CustomError(_))));
}

#[test]
fn on_schedule_panic() {
    let mut context = MockProcessContext::new();
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
        TRIGGER_BEHAVIOUR.name().to_string(),
        "ReturnErr".to_string(),
    );
    let processor = KamikazeProcessorRs::schedule(&context, &MockLogger::new()).unwrap();

    let mut session = MockProcessSession::new();
    assert!(matches!(
        processor.trigger(&mut context, &mut session, &MockLogger::new()),
        Err(ProcessError::Fatal(MinifiError::CustomError(_)))
    ));
}

#[test]
fn on_trigger_panic() {
    let mut context = MockProcessContext::new();
    context
        .properties
        .insert(TRIGGER_BEHAVIOUR.name().to_string(), "Panic".to_string());
    let processor = KamikazeProcessorRs::schedule(&context, &MockLogger::new()).unwrap();

    let mut session = MockProcessSession::new();
    let result = std::panic::catch_unwind(AssertUnwindSafe(|| {
        processor.trigger(&mut context, &mut session, &MockLogger::new())
    }));
    assert!(result.is_err());
}
