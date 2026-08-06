use crate::controller_services::public_key_service::PGPPublicKeyService;
use crate::processors::encrypt_content::FileEncoding;
use minifi_native::Property;

pub(crate) const FILE_ENCODING: Property<FileEncoding> =
    Property::new("File Encoding", "File Encoding for encryption")
        .with_default(FileEncoding::Binary.into_str());
pub(crate) const PASSWORD: Property<Option<String>> = Property::new(
    "Symmetric Password",
    "Password used for encrypting data with Password-Based Encryption",
)
.sensitive();

pub(crate) const PUBLIC_KEY_SEARCH: Property<Option<String>> = Property::new(
    "Public Key Search",
    "PGP Public Key Search will be used to match against the User ID or Key ID when formatted as uppercase hexadecimal string of 16 characters",
);

pub(crate) const PUBLIC_KEY_SERVICE: Property<Option<PGPPublicKeyService>> = Property::new(
    "Public Key Service",
    "PGP Public Key Service for encrypting data with Public Key Encryption",
);
