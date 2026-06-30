mod controller_service_definition;
mod properties;

use crate::controller_services::public_key_service::properties::{KEYRING, KEYRING_FILE};
use minifi_native::macros::ComponentIdentifier;
use minifi_native::{EnableControllerService, GetProperty, Logger, MinifiError};
use pgp::composed::{Deserializable, SignedPublicKey};

#[derive(Debug, ComponentIdentifier, PartialEq)]
pub(crate) struct PGPPublicKeyService {
    public_keys: Vec<SignedPublicKey>,
}

impl EnableControllerService for PGPPublicKeyService {
    fn enable<P: GetProperty, L: Logger>(context: &P, _logger: &L) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        let mut public_keys = vec![];
        if let Some(keyring_file_path) = context.get_property(&KEYRING_FILE)? {
            if let Ok((keys, _headers)) = SignedPublicKey::from_armor_file_many(&keyring_file_path)
            {
                public_keys.extend(keys.filter_map(|key| key.ok()));
            } else if let Ok(keys) = SignedPublicKey::from_file_many(keyring_file_path) {
                public_keys.extend(keys.filter_map(|key| key.ok()));
            }
        }
        if let Some(keyring_ascii) = context.get_property(&KEYRING)? {
            if let Ok((keys, _headers)) = SignedPublicKey::from_armor_many(keyring_ascii.as_bytes())
            {
                public_keys.extend(keys.filter_map(|key| key.ok()));
            }
        }

        if public_keys.is_empty() {
            return Err(MinifiError::controller_service_err(
                "Could not load any valid keys",
            ));
        }
        Ok(Self { public_keys })
    }
}

impl PGPPublicKeyService {
    pub fn get(&self, target_id: &str) -> Option<&SignedPublicKey> {
        self.public_keys.iter().find(|public_key| {
            public_key.details.users.iter().any(|user| {
                user.id
                    .as_str()
                    .map(|user_id| user_id.contains(target_id))
                    .unwrap_or(false)
            })
        })
    }
}

#[cfg(test)]
mod tests;
