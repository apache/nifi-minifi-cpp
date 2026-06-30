use super::PGPPrivateKeyService;
use super::properties::*;
use minifi_native::{ControllerServiceDefinition, Property, ProvidedInterface};

impl ControllerServiceDefinition for PGPPrivateKeyService {
    const DESCRIPTION: &'static str =
        "PGP Private Key Service provides Private Keys loaded from files or properties";
    const PROPERTIES: &'static [Property] = &[KEY_FILE, KEY, KEY_PASSPHRASE];
    const PROVIDED_APIS: &'static [ProvidedInterface<Self>] = &[];
}
