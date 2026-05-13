use super::*;
use crate::processors::get_file::relationships::SUCCESS;
use filetime::FileTime;
use minifi_native::{MockLogger, MockProcessContext, MockProcessSession};
use tempfile::TempDir;

#[test]
fn schedule_fails_without_input_dir() {
    assert!(matches!(
        GetFileRs::schedule(&MockProcessContext::new(), &MockLogger::new())
            .err()
            .unwrap(),
        MinifiError::MissingRequiredProperty(_)
    ));
}

#[test]
fn schedule_fails_with_invalid_input_dir() {
    let mut context = MockProcessContext::new();
    context.properties.insert(
        "Input Directory".to_string(),
        "/invalid_directory".to_string(),
    );
    assert!(matches!(
        GetFileRs::schedule(&context, &MockLogger::new()),
        Err(MinifiError::ScheduleError(_))
    ));
}

#[test]
fn simple_get_file_test() {
    let temp_dir = tempfile::tempdir().expect("temp dir is required for testing GetFile");
    let file_path = temp_dir.path().join("input_file");
    std::fs::write(&file_path, "test").unwrap();

    let mut context = MockProcessContext::new();
    context.properties.insert(
        "Input Directory".to_string(),
        temp_dir.path().to_str().unwrap().to_string(),
    );

    let get_file = GetFileRs::schedule(&context, &MockLogger::new()).unwrap();

    let mut session = MockProcessSession::new();
    get_file
        .trigger(&mut context, &mut session, &MockLogger::new())
        .expect("Should succeed");
    assert_eq!(session.num_of_transferred_flow_files(), 1);
}

fn make_file(temp_dir: &TempDir, file_name: &str, size: usize, age: Duration) {
    let path = temp_dir.path().join(file_name);
    std::fs::write(&path, "a".repeat(size)).unwrap();
    let file_time = FileTime::from_system_time(SystemTime::now() - age);
    filetime::set_file_mtime(path, file_time).expect("Cannot set file time");
}

fn create_test_directory() -> TempDir {
    let temp_dir = tempfile::tempdir().expect("temp dir is required for testing GetFile");
    make_file(&temp_dir, "small_new", 10, Duration::from_secs(10));
    make_file(&temp_dir, "small_old", 11, Duration::from_secs(3600));

    make_file(&temp_dir, "large_new", 1000, Duration::from_secs(0));
    make_file(&temp_dir, "large_old", 2000, Duration::from_secs(3600));
    make_file(&temp_dir, ".small_hidden", 10, Duration::from_secs(0));
    temp_dir
}

#[test]
fn complex_dir_without_filters() {
    let test_directory = create_test_directory();

    let mut context = MockProcessContext::new();
    context.properties.insert(
        "Input Directory".to_string(),
        test_directory.path().to_str().unwrap().to_string(),
    );
    context
        .properties
        .insert("Batch Size".to_string(), "10".to_string());

    let mut session = MockProcessSession::new();
    let get_file = GetFileRs::schedule(&context, &MockLogger::new()).unwrap();
    get_file
        .trigger(&mut context, &mut session, &MockLogger::new())
        .expect("Should succeed");
    assert_eq!(session.num_of_transferred_flow_files(), 4);
}

fn test_complex_dir_with_filter(
    property_name: &str,
    property_vale: &str,
    expected_filename_part: &str,
) {
    let test_directory = create_test_directory();

    let mut context = MockProcessContext::new();
    context.properties.insert(
        DIRECTORY.name.to_string(),
        test_directory.path().to_str().unwrap().to_string(),
    );
    context
        .properties
        .insert(BATCH_SIZE.name.to_string(), "10".to_string());

    context
        .properties
        .insert(property_name.to_string(), property_vale.to_string());

    let mut session = MockProcessSession::new();
    let get_file = GetFileRs::schedule(&context, &MockLogger::new()).unwrap();
    get_file
        .trigger(&mut context, &mut session, &MockLogger::new())
        .expect("Should succeed");
    assert_eq!(session.num_of_transferred_flow_files(), 2);
    let transferred_flow_files = session.transferred_flow_files.borrow();
    assert!(transferred_flow_files.iter().all(|transfer| {
        transfer.relationship == SUCCESS.name
            && transfer
                .flow_file
                .attributes
                .get("filename")
                .and_then(|filename| Some(filename.contains(expected_filename_part)))
                .unwrap_or(false)
    }));
    let sum_file_len = transferred_flow_files
        .iter()
        .fold(0, |acc, transfer| acc + transfer.flow_file.content_len());

    let metrics = get_file.calculate_metrics();
    assert_eq!(metrics.len(), 2);
    assert_eq!(metrics[0].0, "accepted_files".to_string());
    assert_eq!(metrics[0].1, 2.0);
    assert_eq!(metrics[1].0, "input_bytes".to_string());
    assert_eq!(metrics[1].1, sum_file_len as f64);
}

#[test]
fn complex_dir_with_filters() {
    test_complex_dir_with_filter(MIN_AGE.name, "5 min", "old");
    test_complex_dir_with_filter(MAX_AGE.name, "5 min", "new");
    test_complex_dir_with_filter(MIN_SIZE.name, "50 B", "large");
    test_complex_dir_with_filter(MAX_SIZE.name, "50 B", "small");
}

#[test]
fn test_hidden_files_and_batch_size() {
    let temp_dir = tempfile::tempdir().expect("temp dir is required for testing GetFile");
    make_file(&temp_dir, ".one", 10, Duration::from_secs(0));
    make_file(&temp_dir, ".two", 10, Duration::from_secs(0));
    make_file(&temp_dir, ".three", 10, Duration::from_secs(0));

    let mut context = MockProcessContext::new();
    context.properties.insert(
        DIRECTORY.name.to_string(),
        temp_dir.path().to_str().unwrap().to_string(),
    );
    context
        .properties
        .insert(BATCH_SIZE.name.to_string(), "2".to_string());

    context
        .properties
        .insert(IGNORE_HIDDEN_FILES.name.to_string(), "false".to_string());

    let mut session = MockProcessSession::new();
    let get_file = GetFileRs::schedule(&context, &MockLogger::new()).unwrap();
    get_file
        .trigger(&mut context, &mut session, &MockLogger::new())
        .expect("Should succeed");
    assert_eq!(session.num_of_transferred_flow_files(), 2);
}
