use super::*;
use crate::processors::put_file::relationships::{FAILURE, SUCCESS};
use minifi_native::{MockLogger, MockProcessContext};

#[test]
fn schedule_succeeds_with_default_values() {
    assert!(PutFileRs::schedule(&MockProcessContext::new(), &MockLogger::new()).is_ok());
}

#[test]
fn simple_put_file_test() {
    let mut context = MockProcessContext::new();
    let temp_dir = tempfile::tempdir().expect("temp dir is required for testing PutFile");
    let put_file_dir = temp_dir.path().join("subdir");

    context.properties.insert(
        "Directory".to_string(),
        put_file_dir.to_str().unwrap().to_string(),
    );
    let put_file = PutFileRs::schedule(&context, &MockLogger::new()).expect("Should succeed");

    let mut input_stream = std::io::Cursor::new("test".as_bytes());
    context
        .attributes
        .insert("filename".to_string(), "test.txt".to_string());
    let result = put_file
        .transform(&context, &mut input_stream, &MockLogger::new())
        .expect("Should succeed");

    assert_eq!(result.target_relationship(), SUCCESS.name);

    let expected_path = temp_dir.path().join("subdir/test.txt");
    assert!(expected_path.exists());
    assert_eq!(std::fs::read_to_string(expected_path).unwrap(), "test");
}

#[test]
fn put_file_without_create_dirs() {
    let mut context = MockProcessContext::new();
    let temp_dir = tempfile::tempdir().expect("temp dir is required for testing PutFile");

    let put_file_dir = temp_dir.path().join("subdir");

    context.properties.insert(
        "Directory".to_string(),
        put_file_dir.to_str().unwrap().to_string(),
    );

    context.properties.insert(
        "Create Missing Directories".to_string(),
        "false".to_string(),
    );

    let put_file = PutFileRs::schedule(&context, &MockLogger::new()).expect("Should succeed");

    let mut input_stream = std::io::Cursor::new("test".as_bytes());
    context
        .attributes
        .insert("filename".to_string(), "test.txt".to_string());
    let result = put_file
        .transform(&context, &mut input_stream, &MockLogger::new())
        .expect("Should succeed");

    assert_eq!(result.target_relationship(), FAILURE.name);

    let expected_path = temp_dir.path().join("subdir/test.txt");
    assert!(!expected_path.exists());
}

#[test]
fn directory_is_full_counts_only_files() {
    let mut context = MockProcessContext::new();
    let temp_dir = tempfile::tempdir().expect("temp dir is required for testing PutFile");

    context.properties.insert(
        "Directory".to_string(),
        temp_dir.path().to_str().unwrap().to_string(),
    );
    context
        .properties
        .insert("Maximum File Count".to_string(), "2".to_string());

    let put_file = PutFileRs::schedule(&context, &MockLogger::new()).expect("Should succeed");

    let destination = temp_dir.path().join("test.txt");

    // No files yet → not full
    assert!(!put_file.directory_is_full(&destination));

    // Create a subdirectory; it must not be counted as a file
    std::fs::create_dir(temp_dir.path().join("subdir")).unwrap();
    assert!(!put_file.directory_is_full(&destination));

    // Add two files → full
    std::fs::write(temp_dir.path().join("a.txt"), b"a").unwrap();
    std::fs::write(temp_dir.path().join("b.txt"), b"b").unwrap();
    assert!(put_file.directory_is_full(&destination));

    // Remove one file → not full again
    std::fs::remove_file(temp_dir.path().join("a.txt")).unwrap();
    assert!(!put_file.directory_is_full(&destination));
}

#[cfg(unix)]
#[test]
fn put_file_test_permissions() {
    use std::os::unix::fs::PermissionsExt;
    let mut context = MockProcessContext::new();
    let temp_dir = tempfile::tempdir().expect("temp dir is required for testing PutFile");
    let put_file_dir = temp_dir.path().join("subdir");

    context.properties.insert(
        "Directory".to_string(),
        put_file_dir.to_str().unwrap().to_string(),
    );

    context
        .properties
        .insert("Directory Permissions".to_string(), "0777".to_string());

    context
        .properties
        .insert("Permissions".to_string(), "0777".to_string());
    let put_file = PutFileRs::schedule(&context, &MockLogger::new()).expect("Should succeed");

    let mut input_stream = std::io::Cursor::new("test".as_bytes());
    context
        .attributes
        .insert("filename".to_string(), "test.txt".to_string());
    let result = put_file
        .transform(&context, &mut input_stream, &MockLogger::new())
        .expect("Should succeed");

    assert_eq!(result.target_relationship(), SUCCESS.name);

    let expected_path = temp_dir.path().join("subdir/test.txt");
    assert!(expected_path.exists());
    assert_eq!(std::fs::read_to_string(&expected_path).unwrap(), "test");
    let parent_permissions = std::fs::metadata(put_file_dir).unwrap().permissions();
    let permissions = expected_path.metadata().unwrap().permissions();
    assert_eq!(permissions.mode(), 0o100777);
    assert_eq!(parent_permissions.mode(), 0o40777);
}
