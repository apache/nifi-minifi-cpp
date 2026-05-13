use super::*;
use crate::processors::generate_flow_file::properties::{
    BATCH_SIZE, CUSTOM_TEXT, DATA_FORMAT, UNIQUE_FLOW_FILES,
};
use minifi_native::{MockLogger, MockProcessContext, MockProcessSession};

#[test]
fn schedule_succeeds_with_default_values() {
    assert!(GenerateFlowFileRs::schedule(&MockProcessContext::new(), &MockLogger::new()).is_ok());
}

#[test]
fn generate_flow_file_empty_test() {
    let logger = MockLogger::new();
    let mut context = MockProcessContext::new();
    context
        .properties
        .insert(properties::FILE_SIZE.name.to_string(), "0".to_string());
    context
        .properties
        .insert(UNIQUE_FLOW_FILES.name.to_string(), "false".to_string());
    context
        .properties
        .insert(DATA_FORMAT.name.to_string(), "Text".to_string());

    let processor = GenerateFlowFileRs::schedule(&context, &logger).unwrap();
    let mut session = MockProcessSession::new();
    assert_eq!(
        processor
            .trigger(&mut context, &mut session, &logger)
            .unwrap(),
        OnTriggerResult::Ok
    );
    let result_flow_files = session.transferred_flow_files.borrow();
    assert_eq!(result_flow_files.len(), 1);
    assert_eq!(result_flow_files[0].flow_file.content_len(), 0);
}

#[test]
fn generate_custom_text() {
    let mut context = MockProcessContext::new();
    context
        .properties
        .insert(properties::FILE_SIZE.name.to_string(), "0".to_string());
    context
        .properties
        .insert(UNIQUE_FLOW_FILES.name.to_string(), "false".to_string());
    context
        .properties
        .insert(DATA_FORMAT.name.to_string(), "Text".to_string());
    context
        .properties
        .insert(CUSTOM_TEXT.name.to_string(), "foo bar baz".to_string());

    let logger = MockLogger::new();
    let processor = GenerateFlowFileRs::schedule(&context, &logger).unwrap();

    let mut session = MockProcessSession::new();
    assert_eq!(
        processor
            .trigger(&mut context, &mut session, &logger)
            .expect("Should trigger successfully"),
        OnTriggerResult::Ok
    );
    let result_flow_files = session.transferred_flow_files.borrow();
    assert_eq!(result_flow_files.len(), 1);
    assert!(result_flow_files[0].flow_file.content_eq("foo bar baz"),);
}

#[test]
fn random_bytes_unique() {
    let mut context = MockProcessContext::new();
    context
        .properties
        .insert(properties::FILE_SIZE.name.to_string(), "40 B".to_string());
    context
        .properties
        .insert(UNIQUE_FLOW_FILES.name.to_string(), "true".to_string());
    context
        .properties
        .insert(DATA_FORMAT.name.to_string(), "Bytes".to_string());
    context
        .properties
        .insert(BATCH_SIZE.name.to_string(), "2".to_string());

    let logger = MockLogger::new();
    let processor = GenerateFlowFileRs::schedule(&context, &logger).unwrap();
    let mut session = MockProcessSession::new();
    assert_eq!(
        processor
            .trigger(&mut context, &mut session, &logger)
            .expect("Should trigger successfully"),
        OnTriggerResult::Ok
    );
    let result_flow_files = session.transferred_flow_files.borrow();
    assert_eq!(result_flow_files.len(), 2);
    assert_eq!(result_flow_files[0].flow_file.content_len(), 40);
    assert_eq!(result_flow_files[1].flow_file.content_len(), 40);
    assert_ne!(
        *result_flow_files[0].flow_file.content.borrow(),
        *result_flow_files[1].flow_file.content.borrow()
    );
}

#[test]
fn random_bytes_non_unique() {
    let mut context = MockProcessContext::new();
    context
        .properties
        .insert(properties::FILE_SIZE.name.to_string(), "40 B".to_string());
    context
        .properties
        .insert(UNIQUE_FLOW_FILES.name.to_string(), "false".to_string());
    context
        .properties
        .insert(DATA_FORMAT.name.to_string(), "Bytes".to_string());
    context
        .properties
        .insert(BATCH_SIZE.name.to_string(), "2".to_string());

    let logger = MockLogger::new();
    let processor = GenerateFlowFileRs::schedule(&context, &logger).unwrap();
    let mut session = MockProcessSession::new();
    assert_eq!(
        processor
            .trigger(&mut context, &mut session, &logger)
            .expect("Should trigger successfully"),
        OnTriggerResult::Ok
    );
    let result_flow_files = session.transferred_flow_files.borrow();
    assert_eq!(result_flow_files.len(), 2);
    assert_eq!(result_flow_files[0].flow_file.content_len(), 40);
    assert_eq!(result_flow_files[1].flow_file.content_len(), 40);
    assert_eq!(
        *result_flow_files[0].flow_file.content.borrow(),
        *result_flow_files[1].flow_file.content.borrow()
    );
}
