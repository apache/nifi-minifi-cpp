use super::PGPPrivateKeyService;
use super::properties::*;
use minifi_native::{
    ControllerServiceDefinition, PropertyDefinition, ProvidedInterface, property_definitions,
};

impl ControllerServiceDefinition for PGPPrivateKeyService {
    const DESCRIPTION: &'static str =
        "PGP Private Key Service provides Private Keys loaded from files or properties";
    const PROPERTIES: &'static [PropertyDefinition] =
        property_definitions![KEY_FILE, KEY, KEY_PASSPHRASE];
    const PROVIDED_APIS: &'static [ProvidedInterface<Self>] = &[];
}
