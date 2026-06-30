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

#[cfg(test)]
use crate::controller_services::key_lookup::find_unique_key;
use crate::controller_services::key_parsing::load_service_keys;
use minifi_native::macros::ComponentIdentifier;
use minifi_native::{EnableControllerService, GetProperty, Logger, MinifiError};
use pgp::composed::{SignedSecretKey, TheRing};
#[cfg(test)]
use pgp::types::KeyDetails;
use service_def::*;

#[derive(Debug, ComponentIdentifier)]
pub(crate) struct PGPPrivateKeyService {
    private_keys: Vec<SignedSecretKey>,
    passphrases: Vec<pgp::types::Password>,
}

impl EnableControllerService for PGPPrivateKeyService {
    fn enable<P: GetProperty, L: Logger>(context: &P, _logger: &L) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        let private_keys = load_service_keys(context, &KEY_FILE, &KEY)?;
        let passphrases = context.get_property(&KEY_PASSWORD)?.unwrap_or_default();

        Ok(Self {
            private_keys,
            passphrases,
        })
    }
}

impl PGPPrivateKeyService {
    pub fn get_the_ring(&'_ self) -> TheRing<'_> {
        TheRing {
            secret_keys: self.private_keys.iter().collect(),
            key_passwords: self.passphrases.iter().collect(),
            message_password: vec![],
            session_keys: vec![],
            decrypt_options: Default::default(),
        }
    }

    #[cfg(test)]
    pub fn get_secret_key(&self, target_id: &str) -> Result<&SignedSecretKey, MinifiError> {
        find_unique_key(&self.private_keys, target_id, |private_key| {
            (
                private_key.primary_key.legacy_key_id(),
                &private_key.details,
            )
        })
    }
}

mod service_def {
    use crate::controller_services::key_file_property::SecretKeyFile;
    use crate::controller_services::key_property::SecretKey;
    use crate::controller_services::private_key_service::PGPPrivateKeyService;
    use crate::utils;
    use minifi_native::{
        ControllerServiceDefinition, Property, PropertyDefinition, ProvidedInterface,
        property_definitions,
    };

    pub(super) const KEY_FILE: Property<Option<SecretKeyFile>> = Property::new(
        "Keyring File",
        "File path to PGP Secret Key encoded in binary or ASCII Armor",
    )
    .supports_expression_language();

    pub(super) const KEY: Property<Option<SecretKey>> =
        Property::new("Keyring", "Secret Key encoded in ASCII Armor").sensitive();

    pub(super) const KEY_PASSWORD: Property<Option<utils::Passwords>> = Property::new(
        "Key Password",
        "Password used for decrypting Private Keys. Multiple passwords may be supplied one per line, each of them is tried in turn",
    )
    .sensitive();

    impl ControllerServiceDefinition for PGPPrivateKeyService {
        const DESCRIPTION: &'static str =
            "PGP Private Key Service provides Private Keys loaded from files or properties";
        const PROPERTIES: &'static [PropertyDefinition] =
            property_definitions![KEY_FILE, KEY, KEY_PASSWORD];
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
            "Keyring File".to_string(),
            get_test_key_path("alice_private.asc"),
        );

        let service =
            PGPPrivateKeyService::enable(&context, &MockLogger::new()).expect("should enable");
        assert!(service.get_secret_key("Alice").is_ok());
        assert!(service.get_secret_key("alice@example.com").is_ok());

        assert!(service.get_secret_key("Bob").is_err());
        assert!(service.get_secret_key("Carol").is_err());
    }

    #[test]
    fn single_binary_key_file() {
        let mut context = MockControllerServiceContext::new();
        context.properties.insert(
            "Keyring File".to_string(),
            get_test_key_path("alice_private.gpg"),
        );

        let service =
            PGPPrivateKeyService::enable(&context, &MockLogger::new()).expect("should enable");
        assert!(service.get_secret_key("A").is_ok());
        assert!(service.get_secret_key("Alice").is_ok());
        assert!(service.get_secret_key("Alice <alice@example.com>").is_ok());

        assert!(service.get_secret_key("<Alice>").is_err());

        assert!(service.get_secret_key("Bob").is_err());
        assert!(service.get_secret_key("Carol").is_err());
    }

    #[test]
    fn armored_keyring_key_file() {
        let mut context = MockControllerServiceContext::new();
        context.properties.insert(
            "Keyring File".to_string(),
            get_test_key_path("secret_keyring.asc"),
        );

        let service =
            PGPPrivateKeyService::enable(&context, &MockLogger::new()).expect("should enable");
        assert!(service.get_secret_key("Alice").is_ok());
        assert!(service.get_secret_key("Bob").is_ok());
        assert!(service.get_secret_key("bob@home.io").is_ok());
        assert!(service.get_secret_key("bob@work.com").is_ok());
        assert!(service.get_secret_key("Carol").is_err());
    }

    #[test]
    fn binary_keyring_key_file() {
        let mut context = MockControllerServiceContext::new();
        context.properties.insert(
            "Keyring File".to_string(),
            get_test_key_path("secret_keyring.gpg"),
        );

        let service =
            PGPPrivateKeyService::enable(&context, &MockLogger::new()).expect("should enable");
        assert!(service.get_secret_key("Alice").is_ok());
        assert!(service.get_secret_key("Bob").is_ok());
        assert!(service.get_secret_key("bob@home.io").is_ok());
        assert!(service.get_secret_key("bob@work.com").is_ok());
        assert!(service.get_secret_key("Carol").is_err());
    }

    #[test]
    fn armored_keyring() {
        let mut context = MockControllerServiceContext::new();

        let file_content = std::fs::read_to_string(get_test_key_path("secret_keyring.asc"))
            .expect("required for test");

        context
            .properties
            .insert("Keyring".to_string(), file_content);

        let service =
            PGPPrivateKeyService::enable(&context, &MockLogger::new()).expect("should enable");
        assert!(service.get_secret_key("Alice").is_ok());
        assert!(service.get_secret_key("Bob").is_ok());
        assert!(service.get_secret_key("bob@home.io").is_ok());
        assert!(service.get_secret_key("bob@work.com").is_ok());
        assert!(service.get_secret_key("Carol").is_err());
    }

    #[test]
    fn armored_single_key() {
        let mut context = MockControllerServiceContext::new();

        let file_content = std::fs::read_to_string(get_test_key_path("alice_private.asc"))
            .expect("required for test");

        context
            .properties
            .insert("Keyring".to_string(), file_content);

        let service =
            PGPPrivateKeyService::enable(&context, &MockLogger::new()).expect("should enable");
        assert!(service.get_secret_key("Alice").is_ok());
        assert!(service.get_secret_key("Bob").is_err());
        assert!(service.get_secret_key("Carol").is_err());
    }

    #[test]
    fn public_ascii_key() {
        let mut context = MockControllerServiceContext::new();

        let file_content =
            std::fs::read_to_string(get_test_key_path("alice.asc")).expect("required for test");

        context
            .properties
            .insert("Keyring".to_string(), file_content);

        assert!(PGPPrivateKeyService::enable(&context, &MockLogger::new()).is_err());
    }
}
