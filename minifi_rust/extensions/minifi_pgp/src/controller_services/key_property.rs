use minifi_native::{MinifiError, PropertyConstraints, PropertySchema, PropertyType};
use pgp::composed::{Deserializable, SignedPublicKey, SignedSecretKey};

pub(crate) struct SecretKey {}

impl PropertySchema for SecretKey {
    const CONSTRAINT: Option<PropertyConstraints> = None;
    const IS_REQUIRED: bool = false;
}

impl PropertyType for SecretKey {
    type Output = Vec<SignedSecretKey>;

    fn parse(s: &str) -> Result<Self::Output, MinifiError> {
        let mut secret_keys: Vec<SignedSecretKey> = Vec::new();
        if let Ok((keys, _headers)) = SignedSecretKey::from_armor_many(s.as_bytes()) {
            secret_keys.extend(keys.filter_map(Result::ok));
        }
        if secret_keys.is_empty() {
            return Err(MinifiError::validation(
                "Couldnt load any valid secrey keys",
            ));
        }
        Ok(secret_keys)
    }
}

pub(crate) struct PublicKey {}
impl PropertySchema for PublicKey {
    const CONSTRAINT: Option<PropertyConstraints> = None;
    const IS_REQUIRED: bool = false;
}

impl PropertyType for PublicKey {
    type Output = Vec<SignedPublicKey>;

    fn parse(s: &str) -> Result<Self::Output, MinifiError> {
        let mut public_keys: Vec<SignedPublicKey> = Vec::new();
        if let Ok((keys, _headers)) = SignedPublicKey::from_armor_many(s.as_bytes()) {
            public_keys.extend(keys.filter_map(Result::ok));
        }
        if public_keys.is_empty() {
            return Err(MinifiError::validation(
                "Couldnt load any valid public keys",
            ));
        }
        Ok(public_keys)
    }
}
