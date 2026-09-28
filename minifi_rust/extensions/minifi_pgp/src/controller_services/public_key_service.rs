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

use crate::controller_services::key_lookup::find_unique_key;
use crate::controller_services::key_parsing::load_service_keys;
use minifi_native::macros::ComponentIdentifier;
use minifi_native::{EnableControllerService, GetProperty, Logger, MinifiError};
use pgp::composed::SignedPublicKey;
use pgp::types::KeyDetails;
use service_def::*;

#[derive(Debug, ComponentIdentifier, PartialEq)]
pub(crate) struct PGPPublicKeyService {
    public_keys: Vec<SignedPublicKey>,
}

impl EnableControllerService for PGPPublicKeyService {
    fn enable<P: GetProperty, L: Logger>(context: &P, _logger: &L) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        let public_keys = load_service_keys(context, &KEYRING_FILE, &KEYRING)?;
        Ok(Self { public_keys })
    }
}

impl PGPPublicKeyService {
    pub fn get(&self, target_id: &str) -> Result<&SignedPublicKey, MinifiError> {
        find_unique_key(&self.public_keys, target_id, |public_key| {
            (public_key.primary_key.legacy_key_id(), &public_key.details)
        })
    }
}

mod service_def {
    use crate::controller_services::key_file_property::PublicKeyFile;
    use crate::controller_services::key_property::PublicKey;
    use crate::controller_services::public_key_service::PGPPublicKeyService;
    use minifi_native::{
        ControllerServiceDefinition, Property, PropertyDefinition, ProvidedInterface,
        property_definitions,
    };

    pub(crate) const KEYRING_FILE: Property<Option<PublicKeyFile>> = Property::new(
        "Keyring File",
        "File path to PGP Keyring or Public Key encoded in binary or ASCII Armor",
    )
    .supports_expression_language();

    pub(crate) const KEYRING: Property<Option<PublicKey>> = Property::new(
        "Keyring",
        "PGP Keyring or Public Key encoded in ASCII Armor",
    );

    impl ControllerServiceDefinition for PGPPublicKeyService {
        const DESCRIPTION: &'static str =
            "PGP Public Key Service providing Public Keys loaded from files";
        const PROPERTIES: &'static [PropertyDefinition] =
            property_definitions![KEYRING_FILE, KEYRING];
        const PROVIDED_APIS: &'static [ProvidedInterface<Self>] = &[];
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
            PGPPublicKeyService::CLASS_NAME,
            "minifi_pgp::controller_services::public_key_service::PGPPublicKeyService"
        );
        assert_eq!(PGPPublicKeyService::GROUP_NAME, "minifi_pgp");
        assert_eq!(PGPPublicKeyService::VERSION, "1.0.0");
    }

    #[test]
    fn default_fails() {
        let context = MockControllerServiceContext::new();
        assert!(PGPPublicKeyService::enable(&context, &MockLogger::new()).is_err());
    }

    #[test]
    fn armored_private_key_file() {
        let mut context = MockControllerServiceContext::new();
        context.properties.insert(
            "Keyring File".to_string(),
            get_test_key_path("alice_private.asc"),
        );

        assert!(PGPPublicKeyService::enable(&context, &MockLogger::new()).is_err());
    }

    #[test]
    fn single_armored_key_file() {
        let mut context = MockControllerServiceContext::new();
        context
            .properties
            .insert("Keyring File".to_string(), get_test_key_path("alice.asc"));

        let controller_service = PGPPublicKeyService::enable(&context, &MockLogger::new())
            .expect("enable should succeed");

        assert!(controller_service.get("Alice").is_ok());
        assert!(controller_service.get("alice@example.com").is_ok());

        assert!(controller_service.get("Bob").is_err());
        assert!(controller_service.get("Carol").is_err());
    }

    #[test]
    fn single_binary_key_file() {
        let mut context = MockControllerServiceContext::new();
        context
            .properties
            .insert("Keyring File".to_string(), get_test_key_path("alice.gpg"));

        let service = PGPPublicKeyService::enable(&context, &MockLogger::new())
            .expect("enable should succeed");
        assert!(service.get("A").is_ok());
        assert!(service.get("Alice").is_ok());
        assert!(service.get("Alice <alice@example.com>").is_ok());

        assert!(service.get("<Alice>").is_err());

        assert!(service.get("Bob").is_err());
        assert!(service.get("Carol").is_err());
    }

    #[test]
    fn armored_keyring_key_file() {
        let mut context = MockControllerServiceContext::new();
        context
            .properties
            .insert("Keyring File".to_string(), get_test_key_path("keyring.asc"));

        let service = PGPPublicKeyService::enable(&context, &MockLogger::new())
            .expect("enable should succeed");
        assert!(service.get("Alice").is_ok());
        assert!(service.get("Bob").is_ok());
        assert!(service.get("bob@home.io").is_ok());
        assert!(service.get("bob@work.com").is_ok());
        assert!(service.get("Carol").is_err());
    }

    #[test]
    fn binary_keyring_key_file() {
        let mut context = MockControllerServiceContext::new();
        context
            .properties
            .insert("Keyring File".to_string(), get_test_key_path("keyring.gpg"));

        let service = PGPPublicKeyService::enable(&context, &MockLogger::new())
            .expect("enable should succeed");
        assert!(service.get("Alice").is_ok());
        assert!(service.get("Bob").is_ok());
        assert!(service.get("bob@home.io").is_ok());
        assert!(service.get("bob@work.com").is_ok());
        assert!(service.get("Carol").is_err());
    }

    #[test]
    fn armored_keyring() {
        let mut context = MockControllerServiceContext::new();

        let file_content =
            std::fs::read_to_string(get_test_key_path("keyring.asc")).expect("required for test");

        context
            .properties
            .insert("Keyring".to_string(), file_content);

        let service = PGPPublicKeyService::enable(&context, &MockLogger::new())
            .expect("enable should succeed");
        assert!(service.get("Alice").is_ok());
        assert!(service.get("Bob").is_ok());
        assert!(service.get("bob@home.io").is_ok());
        assert!(service.get("bob@work.com").is_ok());
        assert!(service.get("Carol").is_err());
    }

    #[test]
    fn armored_single_key() {
        let mut context = MockControllerServiceContext::new();

        let file_content =
            std::fs::read_to_string(get_test_key_path("alice.asc")).expect("required for test");

        context
            .properties
            .insert("Keyring".to_string(), file_content);

        let service = PGPPublicKeyService::enable(&context, &MockLogger::new())
            .expect("enable should succeed");
        assert!(service.get("Alice").is_ok());
        assert!(service.get("Bob").is_err());
        assert!(service.get("Carol").is_err());
    }

    #[test]
    fn private_ascii_key() {
        let mut context = MockControllerServiceContext::new();

        let file_content = std::fs::read_to_string(get_test_key_path("alice_private.asc"))
            .expect("required for test");

        context
            .properties
            .insert("Keyring".to_string(), file_content);

        assert!(PGPPublicKeyService::enable(&context, &MockLogger::new()).is_err());
    }

    #[test]
    fn looks_up_by_key_id_hex() {
        let mut context = MockControllerServiceContext::new();
        context
            .properties
            .insert("Keyring File".to_string(), get_test_key_path("alice.asc"));

        let service = PGPPublicKeyService::enable(&context, &MockLogger::new())
            .expect("enable should succeed");

        let alice = service.get("Alice").expect("Alice should exist");
        let key_id_hex = alice.primary_key.legacy_key_id().to_string();
        assert_eq!(key_id_hex.len(), 16);
        assert!(service.get(&key_id_hex).is_ok());
        assert!(service.get(&key_id_hex.to_ascii_uppercase()).is_ok());
        assert!(service.get(&key_id_hex[..8]).is_err());
        assert!(service.get("0123456789abcdef").is_err());
    }

    fn ambiguous_keyring_service() -> PGPPublicKeyService {
        let mut context = MockControllerServiceContext::new();
        context.properties.insert(
            "Keyring File".to_string(),
            get_test_key_path("ambiguous_keyring.gpg"),
        );

        PGPPublicKeyService::enable(&context, &MockLogger::new()).expect("enable should succeed")
    }

    /// A User ID search that matches more than one key must fail loudly instead of picking one
    /// of them, otherwise a look-alike key silently becomes the recipient.
    #[test]
    fn a_user_id_matching_several_keys_is_reported_as_ambiguous() {
        let service = ambiguous_keyring_service();

        // "bob@home.io" is a substring of the look-alike "bob@home.io.attacker.test" too.
        let err = service.get("bob@home.io").unwrap_err().to_string();
        assert!(err.contains("ambiguous"), "{err}");
        assert!(err.contains("bob@home.io"), "{err}");

        // Unaffected searches still resolve.
        assert!(service.get("Alice").is_ok());
        assert!(service.get("bob@work.com").is_ok());
        assert!(service.get("bob@home.io.attacker.test").is_ok());
    }

    /// An exact Key ID is never ambiguous, so it stays usable as the way to disambiguate.
    #[test]
    fn a_key_id_search_wins_over_an_ambiguous_user_id() {
        let service = ambiguous_keyring_service();

        let real_bob = service
            .get("bob@work.com")
            .expect("the real Bob should be found by his unique User ID");
        let real_bob_key_id = real_bob.primary_key.legacy_key_id().to_string();

        let found = service
            .get(&real_bob_key_id)
            .expect("a Key ID search should never be ambiguous");
        assert_eq!(
            found.primary_key.legacy_key_id(),
            real_bob.primary_key.legacy_key_id()
        );
    }
}
