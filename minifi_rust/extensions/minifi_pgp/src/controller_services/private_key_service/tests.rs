use super::PGPPrivateKeyService;
use crate::test_utils::get_test_key_path;
use minifi_native::MinifiError::ControllerServiceError;
use minifi_native::{
    ComponentIdentifier, EnableControllerService, MockControllerServiceContext, MockLogger,
};

fn assert_private_key_service_enable_fails_with_no_valid_keys(
    context: &MockControllerServiceContext,
) {
    if let Err(ControllerServiceError(error)) =
        PGPPrivateKeyService::enable(context, &MockLogger::new())
    {
        assert_eq!(error, "Could not load any valid keys");
    } else {
        panic!("Didnt fail with no_valid_keys");
    }
}

#[test]
fn test_component_id() {
    assert_eq!(
        PGPPrivateKeyService::CLASS_NAME,
        "minifi_pgp::controller_services::private_key_service::PGPPrivateKeyService"
    );
    assert_eq!(PGPPrivateKeyService::GROUP_NAME, "minifi_pgp");
    assert_eq!(PGPPrivateKeyService::VERSION, "0.1.0");
}

#[test]
fn default_fails() {
    let context = MockControllerServiceContext::new();
    assert_private_key_service_enable_fails_with_no_valid_keys(&context);
}

#[test]
fn corrupted_binary_keyring_file() {
    let mut context = MockControllerServiceContext::new();
    context
        .properties
        .insert("Key File".to_string(), get_test_key_path("garbage.gpg"));

    assert_private_key_service_enable_fails_with_no_valid_keys(&context);
}

#[test]
fn armored_public_key_file() {
    let mut context = MockControllerServiceContext::new();
    context.properties.insert(
        "Key File".to_string(),
        get_test_key_path("private_mistake.asc"),
    );

    assert_private_key_service_enable_fails_with_no_valid_keys(&context);
}

#[test]
fn corrupted_armored_key_file() {
    let mut context = MockControllerServiceContext::new();
    context.properties.insert(
        "Key File".to_string(),
        get_test_key_path("truncated_private.asc"),
    );

    assert_private_key_service_enable_fails_with_no_valid_keys(&context);
}

#[test]
fn non_existent_keyfile() {
    let mut context = MockControllerServiceContext::new();
    context.properties.insert(
        "Key File".to_string(),
        get_test_key_path("non_existent.asc"),
    );

    assert_private_key_service_enable_fails_with_no_valid_keys(&context);
}

#[test]
fn single_armored_key_file() {
    let mut context = MockControllerServiceContext::new();
    context.properties.insert(
        "Key File".to_string(),
        get_test_key_path("alice_private.asc"),
    );

    let controller_service =
        PGPPrivateKeyService::enable(&context, &MockLogger::new()).expect("should enable");
    assert!(controller_service.get_secret_key("Alice").is_some());
    assert!(
        controller_service
            .get_secret_key("alice@example.com")
            .is_some()
    );

    assert!(controller_service.get_secret_key("Bob").is_none());
    assert!(controller_service.get_secret_key("Carol").is_none());
}

#[test]
fn single_binary_key_file() {
    let mut context = MockControllerServiceContext::new();
    context.properties.insert(
        "Key File".to_string(),
        get_test_key_path("alice_private.gpg"),
    );

    let controller_service =
        PGPPrivateKeyService::enable(&context, &MockLogger::new()).expect("should enable");
    assert!(controller_service.get_secret_key("A").is_some());
    assert!(controller_service.get_secret_key("Alice").is_some());
    assert!(
        controller_service
            .get_secret_key("Alice <alice@example.com>")
            .is_some()
    );

    assert!(controller_service.get_secret_key("<Alice>").is_none());

    assert!(controller_service.get_secret_key("Bob").is_none());
    assert!(controller_service.get_secret_key("Carol").is_none());
}

#[test]
fn armored_keyring_key_file() {
    let mut context = MockControllerServiceContext::new();
    context.properties.insert(
        "Key File".to_string(),
        get_test_key_path("secret_keyring.asc"),
    );

    let controller_service =
        PGPPrivateKeyService::enable(&context, &MockLogger::new()).expect("should enable");
    assert!(controller_service.get_secret_key("Alice").is_some());
    assert!(controller_service.get_secret_key("Bob").is_some());
    assert!(controller_service.get_secret_key("bob@home.io").is_some());
    assert!(controller_service.get_secret_key("bob@work.com").is_some());
    assert!(controller_service.get_secret_key("Carol").is_none());
}

#[test]
fn binary_keyring_key_file() {
    let mut context = MockControllerServiceContext::new();
    context.properties.insert(
        "Key File".to_string(),
        get_test_key_path("secret_keyring.gpg"),
    );

    let controller_service =
        PGPPrivateKeyService::enable(&context, &MockLogger::new()).expect("should enable");
    assert!(controller_service.get_secret_key("Alice").is_some());
    assert!(controller_service.get_secret_key("Bob").is_some());
    assert!(controller_service.get_secret_key("bob@home.io").is_some());
    assert!(controller_service.get_secret_key("bob@work.com").is_some());
    assert!(controller_service.get_secret_key("Carol").is_none());
}

#[test]
fn armored_keyring() {
    let mut context = MockControllerServiceContext::new();

    let file_content = std::fs::read_to_string(get_test_key_path("secret_keyring.asc"))
        .expect("required for test");

    context.properties.insert("Key".to_string(), file_content);

    let controller_service =
        PGPPrivateKeyService::enable(&context, &MockLogger::new()).expect("should enable");
    assert!(controller_service.get_secret_key("Alice").is_some());
    assert!(controller_service.get_secret_key("Bob").is_some());
    assert!(controller_service.get_secret_key("bob@home.io").is_some());
    assert!(controller_service.get_secret_key("bob@work.com").is_some());
    assert!(controller_service.get_secret_key("Carol").is_none());
}

#[test]
fn armored_single_key() {
    let mut context = MockControllerServiceContext::new();

    let file_content =
        std::fs::read_to_string(get_test_key_path("alice_private.asc")).expect("required for test");

    context.properties.insert("Key".to_string(), file_content);

    let controller_service =
        PGPPrivateKeyService::enable(&context, &MockLogger::new()).expect("should enable");
    assert!(controller_service.get_secret_key("Alice").is_some());
    assert!(controller_service.get_secret_key("Bob").is_none());
    assert!(controller_service.get_secret_key("Carol").is_none());
}

#[test]
fn corrupted_armored_key() {
    let mut context = MockControllerServiceContext::new();

    let file_content = std::fs::read_to_string(get_test_key_path("truncated_private.asc"))
        .expect("required for test");

    context.properties.insert("Key".to_string(), file_content);

    assert_private_key_service_enable_fails_with_no_valid_keys(&context);
}

#[test]
fn public_ascii_key() {
    let mut context = MockControllerServiceContext::new();

    let file_content =
        std::fs::read_to_string(get_test_key_path("alice.asc")).expect("required for test");

    context.properties.insert("Key".to_string(), file_content);

    assert_private_key_service_enable_fails_with_no_valid_keys(&context);
}
