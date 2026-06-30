use std::path::PathBuf;

pub fn get_test_key_path(filename: &str) -> String {
    let mut path = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    path.push("test_keys");
    path.push(filename);
    path.display().to_string()
}

pub fn get_test_message(filename: &str) -> Vec<u8> {
    let mut path = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    path.push("test_messages");
    path.push(filename);
    std::fs::read(path).expect("test message should be readable")
}
