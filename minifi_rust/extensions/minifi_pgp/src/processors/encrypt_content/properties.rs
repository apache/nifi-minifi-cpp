use crate::controller_services::public_key_service::PGPPublicKeyService;
use crate::processors::encrypt_content::FileEncoding;
use minifi_native::ComponentIdentifier;
use minifi_native::{Property, StandardPropertyValidator};
use strum::VariantNames;

pub(crate) const FILE_ENCODING: Property = Property {
    name: "File Encoding",
    description: "File Encoding for encryption",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some(FileEncoding::Binary.into_str()),
    validator: StandardPropertyValidator::AlwaysValidValidator,
    allowed_values: FileEncoding::VARIANTS,
    allowed_type: None,
};

pub(crate) const PASSWORD: Property = Property {
    name: "Symmetric Password",
    description: "Password used for encrypting data with Password-Based Encryption",
    is_required: false,
    is_sensitive: true,
    supports_expr_lang: false,
    default_value: None,
    validator: StandardPropertyValidator::AlwaysValidValidator,
    allowed_values: &[],
    allowed_type: None,
};

pub(crate) const PUBLIC_KEY_SEARCH: Property = Property {
    name: "Public Key Search",
    description: "PGP Public Key Search will be used to match against the User ID or Key ID when formatted as uppercase hexadecimal string of 16 characters",
    is_required: false,
    is_sensitive: false,
    supports_expr_lang: true,
    default_value: None,
    validator: StandardPropertyValidator::AlwaysValidValidator,
    allowed_values: &[],
    allowed_type: None,
};

pub(crate) const PUBLIC_KEY_SERVICE: Property = Property {
    name: "Public Key Service",
    description: "PGP Public Key Service for encrypting data with Public Key Encryption",
    is_required: false,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: None,
    validator: StandardPropertyValidator::AlwaysValidValidator,
    allowed_values: &[],
    allowed_type: Some(PGPPublicKeyService::CLASS_NAME),
};
