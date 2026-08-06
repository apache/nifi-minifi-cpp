use crate::controller_services::private_key_service::PGPPrivateKeyService;
use crate::processors::decrypt_content::DecryptionStrategy;
use crate::utils;
use minifi_native::Property;

pub(super) const DECRYPTION_STRATEGY: Property<DecryptionStrategy> = Property::new(
    "Decryption Strategy",
    "Strategy for writing files to success after decryption",
)
.with_default(DecryptionStrategy::Decrypted.into_str());

pub(super) const SYMMETRIC_PASSWORD: Property<Option<utils::Password>> = Property::new(
    "Symmetric Password",
    "Password used for decrypting data encrypted with Password-Based Encryption",
)
.sensitive();

pub(super) const PRIVATE_KEY_SERVICE: Property<Option<PGPPrivateKeyService>> = Property::new(
    "Private Key Service",
    "PGP Private Key Service for decrypting data encrypted with Public Key Encryption",
);
