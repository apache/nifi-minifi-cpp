use minifi_native::Property;
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
