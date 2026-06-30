use crate::controller_services::private_key_service::PGPPrivateKeyService;
use crate::processors::decrypt_content::{DecryptContentPGP, output_attributes};
use crate::test_utils;
use crate::test_utils::get_test_message;
use minifi_native::{
    ComponentIdentifier, EnableControllerService, FlowFileStreamTransform, IoState,
    MockControllerServiceContext, MockLogger, MockProcessContext, Schedule,
};

#[test]
fn test_ids() {
    assert_eq!(
        DecryptContentPGP::CLASS_NAME,
        "minifi_pgp::processors::decrypt_content::DecryptContentPGP"
    );
    assert_eq!(DecryptContentPGP::GROUP_NAME, "minifi_pgp");
    assert_eq!(DecryptContentPGP::VERSION, "0.1.0");
}

#[test]
fn fails_to_schedule_by_default() {
    let decrypt_content =
        DecryptContentPGP::schedule(&MockProcessContext::new(), &MockLogger::new());
    assert!(decrypt_content.is_err());
}

#[test]
fn schedules_with_password() {
    let mut context = MockProcessContext::new();
    context.properties.insert(
        super::properties::SYMMETRIC_PASSWORD.name.to_string(),
        "my_secret_password".to_string(),
    );
    let decrypt_content = DecryptContentPGP::schedule(&context, &MockLogger::new());
    assert!(decrypt_content.is_ok());
}

#[test]
fn schedules_with_controller() {
    let mut context = MockProcessContext::new();
    context.properties.insert(
        super::properties::PRIVATE_KEY_SERVICE.name.to_string(),
        "my_private_key_service".to_string(),
    );
    let decrypt_content = DecryptContentPGP::schedule(&context, &MockLogger::new());
    assert!(decrypt_content.is_ok());
}

#[derive(Copy, Clone)]
struct PrivateKeyData {
    key_filename: &'static str,
    passphrase: Option<&'static str>,
}

impl PrivateKeyData {
    fn into_controller(self) -> PGPPrivateKeyService {
        let mut context = MockControllerServiceContext::new();
        context
            .properties
            .insert("Key File", test_utils::get_test_key_path(self.key_filename));

        if let Some(passphrase) = self.passphrase {
            context.properties.insert("Key Passphrase", passphrase);
        }

        PGPPrivateKeyService::enable(&context, &MockLogger::new()).expect("should enable")
    }
}

fn test_decryption(
    message_file_name: &str,
    private_key_data: Option<PrivateKeyData>,
    symmetric_password: Option<&'static str>,
    expected_result: Result<&[u8], ()>,
) {
    let mut processor_context = MockProcessContext::new();
    if let Some(private_key) = private_key_data {
        processor_context.controller_services.insert(
            "my_private_key_service".to_string(),
            Box::new(private_key.into_controller()),
        );
        processor_context.properties.insert(
            super::properties::PRIVATE_KEY_SERVICE.name.to_string(),
            "my_private_key_service".to_string(),
        );
    }
    if let Some(symmetric_password) = symmetric_password {
        processor_context.properties.insert(
            super::properties::SYMMETRIC_PASSWORD.name.to_string(),
            symmetric_password.to_string(),
        );
    }

    let decrypt_content = DecryptContentPGP::schedule(&processor_context, &MockLogger::new())
        .expect("Should schedule without any properties");
    let mut output: Vec<u8> = Vec::new();
    let mut flow_file_stream = std::io::Cursor::new(get_test_message(message_file_name));
    let res = decrypt_content
        .transform(
            &processor_context,
            &mut flow_file_stream,
            &mut output,
            &MockLogger::new(),
        )
        .expect("Should be able to transform");

    match expected_result {
        Ok(_result_bytes) => {
            assert_eq!(
                res.target_relationship_name(),
                super::relationships::SUCCESS.name
            );
            assert_eq!(res.write_status(), IoState::Ok);
            let data_modified = res
                .get_attribute(output_attributes::LITERAL_DATA_MODIFIED.name)
                .unwrap()
                .parse::<u64>()
                .expect("Should be u64");
            assert!(data_modified > 1770000000000);
            assert!(data_modified < 1780000000000);
            assert!(
                res.get_attribute(output_attributes::LITERAL_DATA_FILENAME.name)
                    .is_some()
            );
        }
        Err(_) => {
            assert_eq!(
                res.target_relationship_name(),
                super::relationships::FAILURE.name
            );
            assert_eq!(res.write_status(), IoState::Cancel);
        }
    }
}

#[test]
fn decrypts_with_password() {
    test_decryption(
        "password_encrypted_foo.gpg",
        None,
        Some("my_secret_password"),
        Ok("foo\n".as_bytes()),
    );
    test_decryption(
        "password_encrypted_foo.asc",
        None,
        Some("my_secret_password"),
        Ok("foo\n".as_bytes()),
    );
    test_decryption(
        "foo_for_alice.gpg",
        None,
        Some("my_secret_password"),
        Err(()),
    );
    test_decryption(
        "foo_for_alice.asc",
        None,
        Some("my_secret_password"),
        Err(()),
    );
}

#[test]
fn decrypts_for_alice() {
    let alice_private_key_data = PrivateKeyData {
        key_filename: "alice_private.asc",
        passphrase: Some("whiterabbit"),
    };

    test_decryption(
        "foo_for_alice.asc",
        Some(alice_private_key_data),
        None,
        Ok("foo\n".as_bytes()),
    );

    test_decryption(
        "foo_for_alice.gpg",
        Some(alice_private_key_data),
        None,
        Ok("foo\n".as_bytes()),
    );

    test_decryption(
        "password_encrypted_foo.gpg",
        Some(alice_private_key_data),
        None,
        Err(()),
    );

    test_decryption(
        "password_encrypted_foo.asc",
        Some(alice_private_key_data),
        None,
        Err(()),
    );
}

#[test]
fn decryption_of_not_encrypted_data() {
    let alice_private_key = PrivateKeyData {
        key_filename: "alice_private.asc",
        passphrase: Some("whiterabbit"),
    };

    let mut processor_context = MockProcessContext::new();
    processor_context.controller_services.insert(
        "my_private_key_service".to_string(),
        Box::new(alice_private_key.into_controller()),
    );
    processor_context.properties.insert(
        super::properties::PRIVATE_KEY_SERVICE.name.to_string(),
        "my_private_key_service".to_string(),
    );

    let logger = MockLogger::new();

    let decrypt_content = DecryptContentPGP::schedule(&processor_context, &logger)
        .expect("Should schedule without any properties");
    let mut result: Vec<u8> = vec![];
    let mut flow_file_stream = std::io::Cursor::new("something not encrypted".as_bytes());
    let res = decrypt_content
        .transform(
            &mut processor_context,
            &mut flow_file_stream,
            &mut result,
            &logger,
        )
        .expect("Should be able to transform");

    assert_eq!(
        res.target_relationship_name(),
        super::relationships::FAILURE.name
    );
    assert_eq!(res.write_status(), IoState::Cancel);
}
