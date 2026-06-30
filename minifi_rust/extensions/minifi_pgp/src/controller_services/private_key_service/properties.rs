use minifi_native::{Property, StandardPropertyValidator};

pub(crate) const KEY_FILE: Property = Property {
    name: "Key File",
    description: "File path to PGP Secret Key encoded in binary or ASCII Armor",
    is_required: false,
    is_sensitive: false,
    supports_expr_lang: true,
    default_value: None,
    validator: StandardPropertyValidator::AlwaysValidValidator,
    allowed_values: &[],
    allowed_type: None,
};

pub(crate) const KEY: Property = Property {
    name: "Key",
    description: "Secret Key encoded in ASCII Armor",
    is_required: false,
    is_sensitive: true,
    supports_expr_lang: false,
    default_value: None,
    validator: StandardPropertyValidator::AlwaysValidValidator,
    allowed_values: &[],
    allowed_type: None,
};

pub(crate) const KEY_PASSPHRASE: Property = Property {
    name: "Key Passphrase",
    description: "Passphrase used for decrypting Private Keys",
    is_required: false,
    is_sensitive: true,
    supports_expr_lang: false,
    default_value: None,
    validator: StandardPropertyValidator::AlwaysValidValidator,
    allowed_values: &[],
    allowed_type: None,
};
