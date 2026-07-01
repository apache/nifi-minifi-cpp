mod controller_service_definition;
mod properties;

#[cfg(test)]
use crate::controller_services::key_lookup::key_matches;
use minifi_native::macros::ComponentIdentifier;
use minifi_native::{EnableControllerService, GetProperty, Logger, MinifiError, warn};
use pgp::composed::{Deserializable, SignedSecretKey, TheRing};
#[cfg(test)]
use pgp::types::KeyDetails;
use pgp::types::Password;

#[derive(Debug, ComponentIdentifier)]
pub(crate) struct PGPPrivateKeyService {
    private_keys: Vec<SignedSecretKey>,
    passphrase: Password,
}

impl EnableControllerService for PGPPrivateKeyService {
    fn enable<P: GetProperty, L: Logger>(context: &P, logger: &L) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        let mut private_keys = vec![];
        if let Some(keyring_file_path) = context.get_property(&properties::KEY_FILE)? {
            if let Ok((keys, _headers)) = SignedSecretKey::from_armor_file_many(&keyring_file_path)
            {
                collect_keys(keys, &mut private_keys, logger);
            } else if let Ok(keys) = SignedSecretKey::from_file_many(keyring_file_path) {
                collect_keys(keys, &mut private_keys, logger);
            }
        }
        if let Some(keyring_ascii) = context.get_property(&properties::KEY)?
            && let Ok((keys, _headers)) = SignedSecretKey::from_armor_many(keyring_ascii.as_bytes())
        {
            collect_keys(keys, &mut private_keys, logger);
        }

        let passphrase =
            if let Some(passphrase_str) = context.get_property(&properties::KEY_PASSPHRASE)? {
                Password::from(passphrase_str)
            } else {
                Password::empty()
            };

        if private_keys.is_empty() {
            return Err(MinifiError::controller_service_err(
                "Could not load any valid keys",
            ));
        }
        Ok(Self {
            private_keys,
            passphrase,
        })
    }
}

impl PGPPrivateKeyService {
    pub fn get_the_ring(&'_ self) -> TheRing<'_> {
        TheRing {
            secret_keys: self.private_keys.iter().collect(),
            key_passwords: vec![&self.passphrase],
            message_password: vec![],
            session_keys: vec![],
            decrypt_options: Default::default(),
        }
    }

    #[cfg(test)]
    pub fn get_secret_key(&self, target_id: &str) -> Option<&SignedSecretKey> {
        self.private_keys.iter().find(|private_key| {
            key_matches(
                &private_key.primary_key.legacy_key_id(),
                &private_key.details,
                target_id,
            )
        })
    }
}

fn collect_keys<I, L>(keys: I, out: &mut Vec<SignedSecretKey>, logger: &L)
where
    I: Iterator<Item = pgp::errors::Result<SignedSecretKey>>,
    L: Logger,
{
    for key in keys {
        match key {
            Ok(k) => out.push(k),
            Err(e) => warn!(logger, "Skipping unparseable private key: {}", e),
        }
    }
}

#[cfg(test)]
mod tests;
