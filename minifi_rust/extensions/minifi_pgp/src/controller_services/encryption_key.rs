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

use minifi_native::MinifiError;
use pgp::composed::{SignedPublicKey, SignedPublicSubKey};
use pgp::packet::{PublicKey, SignatureType};
use pgp::types::KeyDetails;

/// The component key of a certificate that a message should actually be encrypted to.
///
/// `SignedPublicKey`'s own `EncryptionKey` implementation always uses the primary key and
/// ignores subkeys, which fails for the layout `gpg --gen-key` produces nowadays: a sign-only
/// primary key (Ed25519) plus a dedicated encryption subkey (Cv25519).
#[derive(Debug)]
pub(crate) enum EncryptionTarget<'a> {
    Primary(&'a PublicKey),
    Subkey(&'a SignedPublicSubKey),
}

/// Picks the key of `certificate` to encrypt to.
///
/// Encryption subkeys are preferred, newest first, the way GnuPG picks them; the primary key is
/// only used when the certificate has no usable encryption subkey.
pub(crate) fn select_encryption_target(
    certificate: &SignedPublicKey,
) -> Result<EncryptionTarget<'_>, MinifiError> {
    let newest_encryption_subkey = certificate
        .public_subkeys
        .iter()
        .filter(|subkey| is_encryption_subkey(subkey))
        .max_by_key(|subkey| subkey.created_at());

    if let Some(subkey) = newest_encryption_subkey {
        return Ok(EncryptionTarget::Subkey(subkey));
    }

    if certificate.primary_key.algorithm().can_encrypt() {
        return Ok(EncryptionTarget::Primary(&certificate.primary_key));
    }

    Err(MinifiError::custom(format!(
        "Key {} cannot be used for encryption, it has no encryption subkey and its primary key is {:?} which cannot encrypt",
        certificate.primary_key.fingerprint(),
        certificate.primary_key.algorithm()
    )))
}

fn is_encryption_subkey(subkey: &SignedPublicSubKey) -> bool {
    if !subkey.key.algorithm().can_encrypt() {
        return false;
    }

    let is_revoked = subkey
        .signatures
        .iter()
        .any(|signature| signature.typ() == Some(SignatureType::SubkeyRevocation));
    if is_revoked {
        return false;
    }

    subkey
        .signatures
        .iter()
        .filter(|signature| signature.typ() == Some(SignatureType::SubkeyBinding))
        .any(|signature| {
            let key_flags = signature.key_flags();
            key_flags.encrypt_comms() || key_flags.encrypt_storage()
        })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::test_utils::get_test_key_path;
    use pgp::composed::Deserializable;

    fn load_certificate(file_name: &str) -> SignedPublicKey {
        let (certificate, _headers) =
            SignedPublicKey::from_armor_file(get_test_key_path(file_name)).unwrap();
        certificate
    }

    #[test]
    fn rsa_primary_key_is_used_when_there_is_no_encryption_subkey() {
        // alice.asc is an RSA key whose primary key carries the encrypt capability itself.
        let certificate = load_certificate("alice.asc");
        assert!(certificate.primary_key.algorithm().can_encrypt());
        assert!(matches!(
            select_encryption_target(&certificate).unwrap(),
            EncryptionTarget::Primary(_)
        ));
    }

    #[test]
    fn encryption_subkey_is_preferred_over_a_sign_only_primary_key() {
        // dave.asc has an Ed25519 sign-only primary key and a Cv25519 encryption subkey,
        // the layout `gpg --gen-key` produces by default.
        let certificate = load_certificate("dave.asc");
        assert!(!certificate.primary_key.algorithm().can_encrypt());

        let target = select_encryption_target(&certificate).unwrap();
        let EncryptionTarget::Subkey(subkey) = target else {
            panic!("expected the encryption subkey to be selected");
        };
        assert!(subkey.key.algorithm().can_encrypt());
    }

    #[test]
    fn sign_only_key_without_encryption_subkey_is_rejected() {
        // erin.asc is an Ed25519 sign-only primary key with no subkeys at all.
        let certificate = load_certificate("erin.asc");
        let err = select_encryption_target(&certificate)
            .unwrap_err()
            .to_string();
        assert!(err.contains("cannot be used for encryption"), "{err}");
    }
}
