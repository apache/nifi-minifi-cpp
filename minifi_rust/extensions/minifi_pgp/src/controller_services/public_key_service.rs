mod controller_service_definition;
use controller_service_definition::*;

use crate::controller_services::key_lookup::key_matches;
use minifi_native::macros::ComponentIdentifier;
use minifi_native::{EnableControllerService, GetProperty, Logger, MinifiError, warn};
use pgp::composed::{Deserializable, SignedPublicKey};
use pgp::types::KeyDetails;

#[derive(Debug, ComponentIdentifier, PartialEq)]
pub(crate) struct PGPPublicKeyService {
    public_keys: Vec<SignedPublicKey>,
}

impl EnableControllerService for PGPPublicKeyService {
    fn enable<P: GetProperty, L: Logger>(context: &P, logger: &L) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        let mut public_keys = vec![];
        if let Some(keyring_file_path) = context.get_property(&KEYRING_FILE)? {
            if let Ok((keys, _headers)) = SignedPublicKey::from_armor_file_many(&keyring_file_path)
            {
                collect_keys(keys, &mut public_keys, logger);
            } else if let Ok(keys) = SignedPublicKey::from_file_many(keyring_file_path) {
                collect_keys(keys, &mut public_keys, logger);
            }
        }
        if let Some(keyring_ascii) = context.get_property(&KEYRING)?
            && let Ok((keys, _headers)) = SignedPublicKey::from_armor_many(keyring_ascii.as_bytes())
        {
            collect_keys(keys, &mut public_keys, logger);
        }

        if public_keys.is_empty() {
            return Err(MinifiError::custom("Could not load any valid keys"));
        }
        Ok(Self { public_keys })
    }
}

fn collect_keys<I, L>(keys: I, out: &mut Vec<SignedPublicKey>, logger: &L)
where
    I: Iterator<Item = pgp::errors::Result<SignedPublicKey>>,
    L: Logger,
{
    for key in keys {
        match key {
            Ok(k) => out.push(k),
            Err(e) => warn!(logger, "Skipping unparseable public key: {}", e),
        }
    }
}

impl PGPPublicKeyService {
    pub fn get(&self, target_id: &str) -> Option<&SignedPublicKey> {
        self.public_keys.iter().find(|public_key| {
            key_matches(
                &public_key.primary_key.legacy_key_id(),
                &public_key.details,
                target_id,
            )
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::test_utils::get_test_key_path;
    use minifi_native::MinifiError::CustomError;
    use minifi_native::{ComponentIdentifier, MockControllerServiceContext, MockLogger};

    fn assert_public_key_service_enable_fails_with_no_valid_keys(
        context: &MockControllerServiceContext,
    ) {
        if let Err(CustomError(error)) = PGPPublicKeyService::enable(context, &MockLogger::new()) {
            assert_eq!(error, "Could not load any valid keys");
        } else {
            panic!("Didnt fail with no_valid_keys");
        }
    }

    #[test]
    fn test_component_id() {
        assert_eq!(
            PGPPublicKeyService::CLASS_NAME,
            "minifi_pgp::controller_services::public_key_service::PGPPublicKeyService"
        );
        assert_eq!(PGPPublicKeyService::GROUP_NAME, "minifi_pgp");
        assert_eq!(PGPPublicKeyService::VERSION, "0.1.0");
    }

    #[test]
    fn default_fails() {
        let context = MockControllerServiceContext::new();

        assert_public_key_service_enable_fails_with_no_valid_keys(&context);
    }

    #[test]
    fn corrupted_binary_keyring_file() {
        let mut context = MockControllerServiceContext::new();
        context
            .properties
            .insert("Keyring File".to_string(), get_test_key_path("garbage.gpg"));

        assert_public_key_service_enable_fails_with_no_valid_keys(&context);
    }

    #[test]
    fn armored_private_key_file() {
        let mut context = MockControllerServiceContext::new();
        context.properties.insert(
            "Keyring File".to_string(),
            get_test_key_path("alice_private.asc"),
        );

        assert_public_key_service_enable_fails_with_no_valid_keys(&context);
    }

    #[test]
    fn corrupted_armored_key_file() {
        let mut context = MockControllerServiceContext::new();
        context.properties.insert(
            "Keyring File".to_string(),
            get_test_key_path("truncated.asc"),
        );

        assert_public_key_service_enable_fails_with_no_valid_keys(&context);
    }

    #[test]
    fn non_existent_keyfile() {
        let mut context = MockControllerServiceContext::new();
        context.properties.insert(
            "Keyring File".to_string(),
            get_test_key_path("non_existent.asc"),
        );

        assert_public_key_service_enable_fails_with_no_valid_keys(&context);
    }

    #[test]
    fn single_armored_key_file() {
        let mut context = MockControllerServiceContext::new();
        context
            .properties
            .insert("Keyring File".to_string(), get_test_key_path("alice.asc"));

        let controller_service = PGPPublicKeyService::enable(&context, &MockLogger::new())
            .expect("enable should succeed");

        assert!(controller_service.get("Alice").is_some());
        assert!(controller_service.get("alice@example.com").is_some());

        assert!(controller_service.get("Bob").is_none());
        assert!(controller_service.get("Carol").is_none());
    }

    #[test]
    fn single_binary_key_file() {
        let mut context = MockControllerServiceContext::new();
        context
            .properties
            .insert("Keyring File".to_string(), get_test_key_path("alice.gpg"));

        let controller_service = PGPPublicKeyService::enable(&context, &MockLogger::new())
            .expect("enable should succeed");
        assert!(controller_service.get("A").is_some());
        assert!(controller_service.get("Alice").is_some());
        assert!(
            controller_service
                .get("Alice <alice@example.com>")
                .is_some()
        );

        assert!(controller_service.get("<Alice>").is_none());

        assert!(controller_service.get("Bob").is_none());
        assert!(controller_service.get("Carol").is_none());
    }

    #[test]
    fn armored_keyring_key_file() {
        let mut context = MockControllerServiceContext::new();
        context
            .properties
            .insert("Keyring File".to_string(), get_test_key_path("keyring.asc"));

        let controller_service = PGPPublicKeyService::enable(&context, &MockLogger::new())
            .expect("enable should succeed");
        assert!(controller_service.get("Alice").is_some());
        assert!(controller_service.get("Bob").is_some());
        assert!(controller_service.get("bob@home.io").is_some());
        assert!(controller_service.get("bob@work.com").is_some());
        assert!(controller_service.get("Carol").is_none());
    }

    #[test]
    fn binary_keyring_key_file() {
        let mut context = MockControllerServiceContext::new();
        context
            .properties
            .insert("Keyring File".to_string(), get_test_key_path("keyring.gpg"));

        let controller_service = PGPPublicKeyService::enable(&context, &MockLogger::new())
            .expect("enable should succeed");
        assert!(controller_service.get("Alice").is_some());
        assert!(controller_service.get("Bob").is_some());
        assert!(controller_service.get("bob@home.io").is_some());
        assert!(controller_service.get("bob@work.com").is_some());
        assert!(controller_service.get("Carol").is_none());
    }

    #[test]
    fn armored_keyring() {
        let mut context = MockControllerServiceContext::new();

        let file_content =
            std::fs::read_to_string(get_test_key_path("keyring.asc")).expect("required for test");

        context
            .properties
            .insert("Keyring".to_string(), file_content);

        let controller_service = PGPPublicKeyService::enable(&context, &MockLogger::new())
            .expect("enable should succeed");
        assert!(controller_service.get("Alice").is_some());
        assert!(controller_service.get("Bob").is_some());
        assert!(controller_service.get("bob@home.io").is_some());
        assert!(controller_service.get("bob@work.com").is_some());
        assert!(controller_service.get("Carol").is_none());
    }

    #[test]
    fn armored_single_key() {
        let mut context = MockControllerServiceContext::new();

        let file_content =
            std::fs::read_to_string(get_test_key_path("alice.asc")).expect("required for test");

        context
            .properties
            .insert("Keyring".to_string(), file_content);

        let controller_service = PGPPublicKeyService::enable(&context, &MockLogger::new())
            .expect("enable should succeed");
        assert!(controller_service.get("Alice").is_some());
        assert!(controller_service.get("Bob").is_none());
        assert!(controller_service.get("Carol").is_none());
    }

    #[test]
    fn corrupted_armored_key() {
        let mut context = MockControllerServiceContext::new();

        let file_content =
            std::fs::read_to_string(get_test_key_path("truncated.asc")).expect("required for test");

        context
            .properties
            .insert("Keyring".to_string(), file_content);

        assert_public_key_service_enable_fails_with_no_valid_keys(&context);
    }

    #[test]
    fn private_ascii_key() {
        let mut context = MockControllerServiceContext::new();

        let file_content = std::fs::read_to_string(get_test_key_path("alice_private.asc"))
            .expect("required for test");

        context
            .properties
            .insert("Keyring".to_string(), file_content);

        assert_public_key_service_enable_fails_with_no_valid_keys(&context);
    }

    #[test]
    fn looks_up_by_key_id_hex() {
        let mut context = MockControllerServiceContext::new();
        context
            .properties
            .insert("Keyring File".to_string(), get_test_key_path("alice.asc"));

        let controller_service = PGPPublicKeyService::enable(&context, &MockLogger::new())
            .expect("enable should succeed");

        // Get Alice's Key ID from the loaded key so the test doesn't hard-code hex bytes.
        let alice = controller_service.get("Alice").expect("Alice should exist");
        let key_id_hex = alice.primary_key.legacy_key_id().to_string();
        assert_eq!(key_id_hex.len(), 16);

        // Full 16-char hex, both cases, should match.
        assert!(controller_service.get(&key_id_hex).is_some());
        assert!(
            controller_service
                .get(&key_id_hex.to_ascii_uppercase())
                .is_some()
        );

        // A partial or unrelated hex string should not.
        assert!(controller_service.get(&key_id_hex[..8]).is_none());
        assert!(controller_service.get("0123456789abcdef").is_none());
    }
}
