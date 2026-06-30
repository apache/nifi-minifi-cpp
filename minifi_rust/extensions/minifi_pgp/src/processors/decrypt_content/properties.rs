use crate::controller_services::private_key_service::PGPPrivateKeyService;
use crate::processors::decrypt_content::DecryptionStrategy;
use minifi_native::ComponentIdentifier;
use minifi_native::Property;
use minifi_native::PropertyConstraints::{AllowedType, AllowedValues, NoConstraints};
use strum::VariantNames;

pub(crate) const DECRYPTION_STRATEGY: Property = Property {
    name: "Decryption Strategy",
    description: "Strategy for writing files to success after decryption",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some(DecryptionStrategy::Decrypted.into_str()),
    constraints: AllowedValues(DecryptionStrategy::VARIANTS),
};

pub(crate) const SYMMETRIC_PASSWORD: Property = Property {
    name: "Symmetric Password",
    description: "Password used for decrypting data encrypted with Password-Based Encryption",
    is_required: false,
    is_sensitive: true,
    supports_expr_lang: false,
    default_value: None,
    constraints: NoConstraints,
};

pub(crate) const PRIVATE_KEY_SERVICE: Property = Property {
    name: "Private Key Service",
    description: "PGP Private Key Service for decrypting data encrypted with Public Key Encryption",
    is_required: false,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: None,
    constraints: AllowedType(PGPPrivateKeyService::CLASS_NAME),
};
