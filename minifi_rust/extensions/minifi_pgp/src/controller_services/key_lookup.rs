use pgp::composed::SignedKeyDetails;
use pgp::types::KeyId;

/// Returns true when `target_id` matches either:
/// - the key's Key ID formatted as 16-character hex (case-insensitive), or
/// - any of its User IDs as a case-insensitive substring match.
pub(crate) fn key_matches(key_id: &KeyId, details: &SignedKeyDetails, target_id: &str) -> bool {
    let target = target_id.trim();
    if target.is_empty() {
        return false;
    }

    if key_id.to_string().eq_ignore_ascii_case(target) {
        return true;
    }

    let target_lower = target.to_ascii_lowercase();
    details.users.iter().any(|user| {
        user.id
            .as_str()
            .map(|user_id| user_id.to_ascii_lowercase().contains(&target_lower))
            .unwrap_or(false)
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn key_id_from_hex(hex: &str) -> KeyId {
        let mut bytes = [0u8; 8];
        for (i, chunk) in hex.as_bytes().chunks(2).take(8).enumerate() {
            bytes[i] = u8::from_str_radix(std::str::from_utf8(chunk).unwrap(), 16).unwrap();
        }
        KeyId::from(bytes)
    }

    #[test]
    fn empty_target_never_matches() {
        let details = SignedKeyDetails::new(vec![], vec![], vec![], vec![]);
        let key_id = key_id_from_hex("1122334455667788");
        assert!(!key_matches(&key_id, &details, ""));
        assert!(!key_matches(&key_id, &details, "   "));
    }

    #[test]
    fn matches_key_id_case_insensitive() {
        let details = SignedKeyDetails::new(vec![], vec![], vec![], vec![]);
        let key_id = key_id_from_hex("11ABcdEF33445566");

        assert!(key_matches(&key_id, &details, "11abcdef33445566"));
        assert!(key_matches(&key_id, &details, "11ABCDEF33445566"));
        assert!(!key_matches(&key_id, &details, "11abcdef3344556")); // 15 chars
        assert!(!key_matches(&key_id, &details, "abcdef33445566"));
    }
}
