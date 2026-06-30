use super::PGPPrivateKeyService;
use crate::utils;
use minifi_native::{
    ControllerServiceDefinition, Property, PropertyDefinition, ProvidedInterface,
    property_definitions,
};
use std::path::PathBuf;

pub(super) const KEY_FILE: Property<Option<PathBuf>> = Property::new(
    "Key File",
    "File path to PGP Secret Key encoded in binary or ASCII Armor",
)
.supports_expression_language();

pub(super) const KEY: Property<Option<String>> =
    Property::new("Key", "Secret Key encoded in ASCII Armor").sensitive();

pub(super) const KEY_PASSPHRASE: Property<Option<utils::Password>> = Property::new(
    "Key Passphrase",
    "Passphrase used for decrypting Private Keys",
)
.sensitive();

impl ControllerServiceDefinition for PGPPrivateKeyService {
    const DESCRIPTION: &'static str =
        "PGP Private Key Service provides Private Keys loaded from files or properties";
    const PROPERTIES: &'static [PropertyDefinition] =
        property_definitions![KEY_FILE, KEY, KEY_PASSPHRASE];
    const PROVIDED_APIS: &'static [ProvidedInterface<Self>] = &[];
}
