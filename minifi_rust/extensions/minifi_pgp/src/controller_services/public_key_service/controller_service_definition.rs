use super::PGPPublicKeyService;
use minifi_native::{
    ControllerServiceDefinition, Property, PropertyDefinition, ProvidedInterface,
    property_definitions,
};
use std::path::PathBuf;

pub(crate) const KEYRING_FILE: Property<Option<PathBuf>> = Property::new(
    "Keyring File",
    "File path to PGP Keyring or Secret Key encoded in binary or ASCII Armor",
)
.supports_expression_language();

pub(crate) const KEYRING: Property<Option<String>> = Property::new(
    "Keyring",
    "PGP Keyring or Secret Key encoded in ASCII Armor",
)
.sensitive();

impl ControllerServiceDefinition for PGPPublicKeyService {
    const DESCRIPTION: &'static str =
        "PGP Public Key Service providing Public Keys loaded from files";
    const PROPERTIES: &'static [PropertyDefinition] = property_definitions![KEYRING_FILE, KEYRING];
    const PROVIDED_APIS: &'static [ProvidedInterface<Self>] = &[];
}
