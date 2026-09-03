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

use minifi_native::{MinifiError, PropertyConstraints, PropertySchema, PropertyType};
use pgp::composed::{Deserializable, SignedPublicKey, SignedSecretKey};

pub(crate) struct SecretKeyFile {}

impl PropertySchema for SecretKeyFile {
    const CONSTRAINT: Option<PropertyConstraints> = None;
    const IS_REQUIRED: bool = false;
}

impl PropertyType for SecretKeyFile {
    type Output = Vec<SignedSecretKey>;

    fn parse(s: &str) -> Result<Self::Output, MinifiError> {
        let mut result: Vec<SignedSecretKey> = Vec::new();
        if let Ok((keys, _headers)) = SignedSecretKey::from_armor_file_many(s) {
            result.extend(keys.filter_map(Result::ok));
        } else if let Ok(keys) = SignedSecretKey::from_file_many(s) {
            result.extend(keys.filter_map(Result::ok));
        }
        if result.is_empty() {
            Err(MinifiError::validation(
                "Couldn't load any valid secret keys",
            ))
        } else {
            Ok(result)
        }
    }
}

pub(crate) struct PublicKeyFile {}
impl PropertySchema for PublicKeyFile {
    const CONSTRAINT: Option<PropertyConstraints> = None;
    const IS_REQUIRED: bool = false;
}

impl PropertyType for PublicKeyFile {
    type Output = Vec<SignedPublicKey>;

    fn parse(s: &str) -> Result<Self::Output, MinifiError> {
        let mut result: Vec<SignedPublicKey> = Vec::new();
        if let Ok((keys, _headers)) = SignedPublicKey::from_armor_file_many(s) {
            result.extend(keys.filter_map(Result::ok));
        } else if let Ok(keys) = SignedPublicKey::from_file_many(s) {
            result.extend(keys.filter_map(Result::ok));
        }
        if result.is_empty() {
            Err(MinifiError::validation(
                "Couldn't load any valid public keys",
            ))
        } else {
            Ok(result)
        }
    }
}

#[cfg(test)]
mod secret_key_file_tests {
    use super::*;
    use crate::test_utils::get_test_key_path;

    fn assert_invalid_secret_key_file(file_name: &str) {
        assert!(SecretKeyFile::parse(&get_test_key_path(file_name)).is_err())
    }
    fn assert_valid_secret_key_file(file_name: &str) {
        assert!(
            !SecretKeyFile::parse(&get_test_key_path(file_name))
                .unwrap()
                .is_empty()
        )
    }
    #[test]
    fn test_invalid_secret_keyfiles() {
        assert_invalid_secret_key_file("alice.asc");
        assert_invalid_secret_key_file("alice.gpg");
        assert_invalid_secret_key_file("garbage.gpg");
        assert_invalid_secret_key_file("truncated_private.asc");
        assert_invalid_secret_key_file("non_existent.asc");
    }

    #[test]
    fn test_valid_secret_keyfiles() {
        assert_valid_secret_key_file("alice_private.asc");
        assert_valid_secret_key_file("alice_private.gpg");
        assert_valid_secret_key_file("bob_private.asc");
        assert_valid_secret_key_file("bob_private.gpg");
        assert_valid_secret_key_file("secret_keyring.asc");
        assert_valid_secret_key_file("secret_keyring.gpg");
    }
}

#[cfg(test)]
mod public_key_file_tests {
    use crate::controller_services::key_file_property::PublicKeyFile;
    use crate::test_utils::get_test_key_path;
    use minifi_native::PropertyType;

    fn assert_invalid_public_key_file(file_name: &str) {
        assert!(PublicKeyFile::parse(&get_test_key_path(file_name)).is_err())
    }
    fn assert_valid_public_key_file(file_name: &str) {
        assert!(
            !PublicKeyFile::parse(&get_test_key_path(file_name))
                .unwrap()
                .is_empty()
        )
    }
    #[test]
    fn test_invalid_public_keyfiles() {
        assert_invalid_public_key_file("alice_private.asc");
        assert_invalid_public_key_file("alice_private.gpg");
        assert_invalid_public_key_file("garbage.gpg");
        assert_invalid_public_key_file("truncated.asc");
        assert_invalid_public_key_file("non_existent.asc");
    }

    #[test]
    fn test_valid_public_keyfiles() {
        assert_valid_public_key_file("alice.asc");
        assert_valid_public_key_file("alice.gpg");
        assert_valid_public_key_file("keyring.asc");
        assert_valid_public_key_file("keyring.gpg");
    }
}
