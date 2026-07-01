use minifi_native::{Property, StandardPropertyValidator};

pub(crate) const KEYRING_FILE: Property = Property {
    name: "Keyring File",
    description: "File path to PGP Keyring or Secret Key encoded in binary or ASCII Armor",
    is_required: false,
    is_sensitive: false,
    supports_expr_lang: true,
    default_value: None,
    validator: StandardPropertyValidator::AlwaysValidValidator,
    allowed_values: &[],
    allowed_type: None,
};

pub(crate) const KEYRING: Property = Property {
    name: "Keyring",
    description: "PGP Keyring or Secret Key encoded in ASCII Armor",
    is_required: false,
    is_sensitive: true,
    supports_expr_lang: false,
    default_value: None,
    validator: StandardPropertyValidator::AlwaysValidValidator,
    allowed_values: &[],
    allowed_type: None,
};
