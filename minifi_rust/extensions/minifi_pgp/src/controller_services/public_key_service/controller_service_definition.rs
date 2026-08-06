use super::PGPPublicKeyService;
use super::properties::*;
use minifi_native::{
    ControllerServiceDefinition, PropertyDefinition, ProvidedInterface, property_definitions,
};

impl ControllerServiceDefinition for PGPPublicKeyService {
    const DESCRIPTION: &'static str =
        "PGP Public Key Service providing Public Keys loaded from files";
    const PROPERTIES: &'static [PropertyDefinition] = property_definitions![KEYRING_FILE, KEYRING];
    const PROVIDED_APIS: &'static [ProvidedInterface<Self>] = &[];
}
