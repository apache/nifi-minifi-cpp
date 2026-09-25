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
use pgp::composed::SignedKeyDetails;
use pgp::types::KeyId;

fn matches_key_id(key_id: &KeyId, target: &str) -> bool {
    key_id.to_string().eq_ignore_ascii_case(target)
}

fn matches_user_id(details: &SignedKeyDetails, target_lower: &str) -> bool {
    details.users.iter().any(|user| {
        user.id
            .as_str()
            .map(|user_id| user_id.to_ascii_lowercase().contains(target_lower))
            .unwrap_or(false)
    })
}

pub(crate) fn find_unique_key<'a, K, F>(
    keys: &'a [K],
    target_id: &str,
    key_parts: F,
) -> Result<&'a K, MinifiError>
where
    F: Fn(&'a K) -> (KeyId, &'a SignedKeyDetails),
{
    let target = target_id.trim();
    if target.is_empty() {
        return Err(MinifiError::custom("No key search string was given"));
    }

    if let Some(key) = keys
        .iter()
        .find(|key| matches_key_id(&key_parts(key).0, target))
    {
        return Ok(key);
    }

    let target_lower = target.to_ascii_lowercase();
    let mut matches = keys
        .iter()
        .filter(|key| matches_user_id(key_parts(key).1, &target_lower));

    let Some(first_match) = matches.next() else {
        return Err(MinifiError::custom(format!(
            "No key matching '{target}' was found"
        )));
    };

    let ambiguous: Vec<String> = std::iter::once(first_match)
        .chain(matches)
        .map(|key| key_parts(key).0.to_string())
        .collect();
    if ambiguous.len() > 1 {
        return Err(MinifiError::custom(format!(
            "'{target}' is ambiguous, it matches {} keys: {}",
            ambiguous.len(),
            ambiguous.join(", ")
        )));
    }

    Ok(first_match)
}

#[cfg(test)]
mod tests {
    use super::*;
    use pgp::composed::SignedKeyDetails;

    /// A stand-in for a key: just the parts `find_unique_key` looks at.
    #[derive(Debug)]
    struct TestKey {
        key_id: KeyId,
        details: SignedKeyDetails,
    }

    fn key_id_from_hex(hex: &str) -> KeyId {
        let mut bytes = [0u8; 8];
        for (i, chunk) in hex.as_bytes().chunks(2).take(8).enumerate() {
            bytes[i] = u8::from_str_radix(std::str::from_utf8(chunk).unwrap(), 16).unwrap();
        }
        KeyId::from(bytes)
    }

    fn find<'a>(keys: &'a [TestKey], target: &str) -> Result<&'a TestKey, MinifiError> {
        find_unique_key(keys, target, |key| (key.key_id, &key.details))
    }

    fn no_details() -> SignedKeyDetails {
        SignedKeyDetails::new(vec![], vec![], vec![], vec![])
    }

    #[test]
    fn empty_target_never_matches() {
        let keys = [TestKey {
            key_id: key_id_from_hex("1122334455667788"),
            details: no_details(),
        }];
        assert!(find(&keys, "").is_err());
        assert!(find(&keys, "   ").is_err());
    }

    #[test]
    fn matches_key_id_case_insensitive() {
        let keys = [TestKey {
            key_id: key_id_from_hex("11ABcdEF33445566"),
            details: no_details(),
        }];

        assert!(find(&keys, "11abcdef33445566").is_ok());
        assert!(find(&keys, "11ABCDEF33445566").is_ok());
        assert!(find(&keys, "11abcdef3344556").is_err()); // 15 chars
        assert!(find(&keys, "abcdef33445566").is_err());
    }

    #[test]
    fn missing_key_reports_the_search_string() {
        let keys = [TestKey {
            key_id: key_id_from_hex("1122334455667788"),
            details: no_details(),
        }];
        let err = find(&keys, "99aabbccddeeff00").unwrap_err().to_string();
        assert!(err.contains("99aabbccddeeff00"), "{err}");
    }
}
