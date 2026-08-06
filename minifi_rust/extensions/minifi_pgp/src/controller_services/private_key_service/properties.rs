use crate::utils;
use minifi_native::Property;
use std::path::PathBuf;

pub(crate) const KEY_FILE: Property<Option<PathBuf>> = Property::new(
    "Key File",
    "File path to PGP Secret Key encoded in binary or ASCII Armor",
)
.supports_expression_language();

pub(crate) const KEY: Property<Option<String>> =
    Property::new("Key", "Secret Key encoded in ASCII Armor").sensitive();

pub(crate) const KEY_PASSPHRASE: Property<Option<utils::Password>> = Property::new(
    "Key Passphrase",
    "Passphrase used for decrypting Private Keys",
)
.sensitive();
