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

mod controller_service_definition;
use controller_service_definition::*;

use crate::controller_services::key_lookup::key_matches;
use minifi_native::macros::ComponentIdentifier;
use minifi_native::{EnableControllerService, GetProperty, Logger, MinifiError};
use pgp::composed::SignedPublicKey;
use pgp::types::KeyDetails;

#[derive(Debug, ComponentIdentifier, PartialEq)]
pub(crate) struct PGPPublicKeyService {
    public_keys: Vec<SignedPublicKey>,
}

impl EnableControllerService for PGPPublicKeyService {
    fn enable<P: GetProperty, L: Logger>(context: &P, _logger: &L) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        let mut public_keys = context.get_property(&KEYRING_FILE)?.unwrap_or_default();
        public_keys.extend(context.get_property(&KEYRING)?.unwrap_or_default());

        if public_keys.is_empty() {
            return Err(MinifiError::validation("Could not load any valid keys"));
        }
        Ok(Self { public_keys })
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

        let service = PGPPublicKeyService::enable(&context, &MockLogger::new())
            .expect("enable should succeed");
        assert!(service.get("A").is_some());
        assert!(service.get("Alice").is_some());
        assert!(service.get("Alice <alice@example.com>").is_some());

        assert!(service.get("<Alice>").is_none());

        assert!(service.get("Bob").is_none());
        assert!(service.get("Carol").is_none());
    }

    #[test]
    fn armored_keyring_key_file() {
        let mut context = MockControllerServiceContext::new();
        context
            .properties
            .insert("Keyring File".to_string(), get_test_key_path("keyring.asc"));

        let service = PGPPublicKeyService::enable(&context, &MockLogger::new())
            .expect("enable should succeed");
        assert!(service.get("Alice").is_some());
        assert!(service.get("Bob").is_some());
        assert!(service.get("bob@home.io").is_some());
        assert!(service.get("bob@work.com").is_some());
        assert!(service.get("Carol").is_none());
    }

    #[test]
    fn binary_keyring_key_file() {
        let mut context = MockControllerServiceContext::new();
        context
            .properties
            .insert("Keyring File".to_string(), get_test_key_path("keyring.gpg"));

        let service = PGPPublicKeyService::enable(&context, &MockLogger::new())
            .expect("enable should succeed");
        assert!(service.get("Alice").is_some());
        assert!(service.get("Bob").is_some());
        assert!(service.get("bob@home.io").is_some());
        assert!(service.get("bob@work.com").is_some());
        assert!(service.get("Carol").is_none());
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
        assert!(service.get("Alice").is_some());
        assert!(service.get("Bob").is_some());
        assert!(service.get("bob@home.io").is_some());
        assert!(service.get("bob@work.com").is_some());
        assert!(service.get("Carol").is_none());
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
        assert!(service.get("Alice").is_some());
        assert!(service.get("Bob").is_none());
        assert!(service.get("Carol").is_none());
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
        assert!(service.get(&key_id_hex).is_some());
        assert!(service.get(&key_id_hex.to_ascii_uppercase()).is_some());
        assert!(service.get(&key_id_hex[..8]).is_none());
        assert!(service.get("0123456789abcdef").is_none());
    }
}
