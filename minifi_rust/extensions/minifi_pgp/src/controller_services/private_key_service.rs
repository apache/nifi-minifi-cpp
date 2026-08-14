mod controller_service_definition;
use controller_service_definition::*;

#[cfg(test)]
use crate::controller_services::key_lookup::key_matches;
use minifi_native::macros::ComponentIdentifier;
use minifi_native::{EnableControllerService, GetProperty, Logger, MinifiError};
use pgp::composed::{SignedSecretKey, TheRing};
#[cfg(test)]
use pgp::types::KeyDetails;

#[derive(Debug, ComponentIdentifier)]
pub(crate) struct PGPPrivateKeyService {
    private_keys: Vec<SignedSecretKey>,
    passphrase: pgp::types::Password,
}

impl EnableControllerService for PGPPrivateKeyService {
    fn enable<P: GetProperty, L: Logger>(context: &P, _logger: &L) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        let mut private_keys = context.get_property(&KEY_FILE)?.unwrap_or_default();
        private_keys.extend(context.get_property(&KEY)?.unwrap_or_default());

        let passphrase = context.get_property(&KEY_PASSPHRASE)?.unwrap_or_default();

        if private_keys.is_empty() {
            return Err(MinifiError::validation("Could not load any valid keys"));
        }
        Ok(Self {
            private_keys,
            passphrase,
        })
    }
}

impl PGPPrivateKeyService {
    pub fn get_the_ring(&'_ self) -> TheRing<'_> {
        TheRing {
            secret_keys: self.private_keys.iter().collect(),
            key_passwords: vec![&self.passphrase],
            message_password: vec![],
            session_keys: vec![],
            decrypt_options: Default::default(),
        }
    }

    #[cfg(test)]
    pub fn get_secret_key(&self, target_id: &str) -> Option<&SignedSecretKey> {
        self.private_keys.iter().find(|private_key| {
            key_matches(
                &private_key.primary_key.legacy_key_id(),
                &private_key.details,
                target_id,
            )
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::test_utils::get_test_key_path;
    use minifi_native::{ComponentIdentifier, MockControllerServiceContext, MockLogger};

    #[test]
    fn test_component_id() {
        assert_eq!(
            PGPPrivateKeyService::CLASS_NAME,
            "minifi_pgp::controller_services::private_key_service::PGPPrivateKeyService"
        );
        assert_eq!(PGPPrivateKeyService::GROUP_NAME, "minifi_pgp");
        assert_eq!(PGPPrivateKeyService::VERSION, "1.0.0");
    }

    #[test]
    fn default_fails() {
        let context = MockControllerServiceContext::new();
        assert!(PGPPrivateKeyService::enable(&context, &MockLogger::new()).is_err());
    }

    #[test]
    fn single_armored_key_file() {
        let mut context = MockControllerServiceContext::new();
        context.properties.insert(
            "Key File".to_string(),
            get_test_key_path("alice_private.asc"),
        );

        let service =
            PGPPrivateKeyService::enable(&context, &MockLogger::new()).expect("should enable");
        assert!(service.get_secret_key("Alice").is_some());
        assert!(service.get_secret_key("alice@example.com").is_some());

        assert!(service.get_secret_key("Bob").is_none());
        assert!(service.get_secret_key("Carol").is_none());
    }

    #[test]
    fn single_binary_key_file() {
        let mut context = MockControllerServiceContext::new();
        context.properties.insert(
            "Key File".to_string(),
            get_test_key_path("alice_private.gpg"),
        );

        let service =
            PGPPrivateKeyService::enable(&context, &MockLogger::new()).expect("should enable");
        assert!(service.get_secret_key("A").is_some());
        assert!(service.get_secret_key("Alice").is_some());
        assert!(
            service
                .get_secret_key("Alice <alice@example.com>")
                .is_some()
        );

        assert!(service.get_secret_key("<Alice>").is_none());

        assert!(service.get_secret_key("Bob").is_none());
        assert!(service.get_secret_key("Carol").is_none());
    }

    #[test]
    fn armored_keyring_key_file() {
        let mut context = MockControllerServiceContext::new();
        context.properties.insert(
            "Key File".to_string(),
            get_test_key_path("secret_keyring.asc"),
        );

        let service =
            PGPPrivateKeyService::enable(&context, &MockLogger::new()).expect("should enable");
        assert!(service.get_secret_key("Alice").is_some());
        assert!(service.get_secret_key("Bob").is_some());
        assert!(service.get_secret_key("bob@home.io").is_some());
        assert!(service.get_secret_key("bob@work.com").is_some());
        assert!(service.get_secret_key("Carol").is_none());
    }

    #[test]
    fn binary_keyring_key_file() {
        let mut context = MockControllerServiceContext::new();
        context.properties.insert(
            "Key File".to_string(),
            get_test_key_path("secret_keyring.gpg"),
        );

        let service =
            PGPPrivateKeyService::enable(&context, &MockLogger::new()).expect("should enable");
        assert!(service.get_secret_key("Alice").is_some());
        assert!(service.get_secret_key("Bob").is_some());
        assert!(service.get_secret_key("bob@home.io").is_some());
        assert!(service.get_secret_key("bob@work.com").is_some());
        assert!(service.get_secret_key("Carol").is_none());
    }

    #[test]
    fn armored_keyring() {
        let mut context = MockControllerServiceContext::new();

        let file_content = std::fs::read_to_string(get_test_key_path("secret_keyring.asc"))
            .expect("required for test");

        context.properties.insert("Key".to_string(), file_content);

        let service =
            PGPPrivateKeyService::enable(&context, &MockLogger::new()).expect("should enable");
        assert!(service.get_secret_key("Alice").is_some());
        assert!(service.get_secret_key("Bob").is_some());
        assert!(service.get_secret_key("bob@home.io").is_some());
        assert!(service.get_secret_key("bob@work.com").is_some());
        assert!(service.get_secret_key("Carol").is_none());
    }

    #[test]
    fn armored_single_key() {
        let mut context = MockControllerServiceContext::new();

        let file_content = std::fs::read_to_string(get_test_key_path("alice_private.asc"))
            .expect("required for test");

        context.properties.insert("Key".to_string(), file_content);

        let service =
            PGPPrivateKeyService::enable(&context, &MockLogger::new()).expect("should enable");
        assert!(service.get_secret_key("Alice").is_some());
        assert!(service.get_secret_key("Bob").is_none());
        assert!(service.get_secret_key("Carol").is_none());
    }

    #[test]
    fn public_ascii_key() {
        let mut context = MockControllerServiceContext::new();

        let file_content =
            std::fs::read_to_string(get_test_key_path("alice.asc")).expect("required for test");

        context.properties.insert("Key".to_string(), file_content);

        assert!(PGPPrivateKeyService::enable(&context, &MockLogger::new()).is_err());
    }
}
