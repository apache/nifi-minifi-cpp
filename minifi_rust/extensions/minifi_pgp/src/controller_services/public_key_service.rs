mod controller_service_definition;
mod properties;

use crate::controller_services::key_lookup::key_matches;
use crate::controller_services::public_key_service::properties::{KEYRING, KEYRING_FILE};
use minifi_native::macros::ComponentIdentifier;
use minifi_native::{EnableControllerService, GetProperty, Logger, MinifiError, warn};
use pgp::composed::{Deserializable, SignedPublicKey};
use pgp::types::KeyDetails;

#[derive(Debug, ComponentIdentifier, PartialEq)]
pub(crate) struct PGPPublicKeyService {
    public_keys: Vec<SignedPublicKey>,
}

impl EnableControllerService for PGPPublicKeyService {
    fn enable<P: GetProperty, L: Logger>(context: &P, logger: &L) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        let mut public_keys = vec![];
        if let Some(keyring_file_path) = context.get_property(&KEYRING_FILE)? {
            if let Ok((keys, _headers)) = SignedPublicKey::from_armor_file_many(&keyring_file_path)
            {
                collect_keys(keys, &mut public_keys, logger);
            } else if let Ok(keys) = SignedPublicKey::from_file_many(keyring_file_path) {
                collect_keys(keys, &mut public_keys, logger);
            }
        }
        if let Some(keyring_ascii) = context.get_property(&KEYRING)?
            && let Ok((keys, _headers)) = SignedPublicKey::from_armor_many(keyring_ascii.as_bytes())
        {
            collect_keys(keys, &mut public_keys, logger);
        }

        if public_keys.is_empty() {
            return Err(MinifiError::controller_service_err(
                "Could not load any valid keys",
            ));
        }
        Ok(Self { public_keys })
    }
}

fn collect_keys<I, L>(keys: I, out: &mut Vec<SignedPublicKey>, logger: &L)
where
    I: Iterator<Item = pgp::errors::Result<SignedPublicKey>>,
    L: Logger,
{
    for key in keys {
        match key {
            Ok(k) => out.push(k),
            Err(e) => warn!(logger, "Skipping unparseable public key: {}", e),
        }
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
mod tests;
