use super::PGPPublicKeyService;
use super::properties::*;
use minifi_native::{property_definitions, ControllerServiceDefinition, PropertyDefinition, ProvidedInterface};

impl ControllerServiceDefinition for PGPPublicKeyService {
    const DESCRIPTION: &'static str =
        "PGP Public Key Service providing Public Keys loaded from files";
    const PROPERTIES: &'static [PropertyDefinition] = property_definitions![KEYRING_FILE, KEYRING];
    const PROVIDED_APIS: &'static [ProvidedInterface<Self>] = &[];
}
