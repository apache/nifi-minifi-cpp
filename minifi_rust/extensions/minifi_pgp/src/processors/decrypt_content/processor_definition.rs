use super::{DecryptContentPGP, DecryptionStrategy};
use crate::controller_services::private_key_service::PGPPrivateKeyService;
use crate::utils;
use minifi_native::{
    OutputAttribute, ProcessorDefinition, ProcessorInputRequirement, Property, PropertyDefinition,
    Relationship, property_definitions,
};

pub(super) const LITERAL_DATA_FILENAME: OutputAttribute = OutputAttribute {
    name: "pgp.literal.data.filename",
    relationships: &["success"],
    description: "Filename from decrypted Literal Data (Note that OpenPGP signatures do not include the formatting octet, the file name, and the date field of the Literal Data packet in a signature hash; therefore, those fields are not protected against tampering in a signed document. Therefore a lot of implementations omit these inherently malleable metadata)",
};

pub(super) const LITERAL_DATA_MODIFIED: OutputAttribute = OutputAttribute {
    name: "pgp.literal.data.modified",
    relationships: &["success"],
    description: "Modified Date from decrypted Literal Data (Note that OpenPGP signatures do not include the formatting octet, the file name, and the date field of the Literal Data packet in a signature hash; therefore, those fields are not protected against tampering in a signed document. Therefore a lot of implementations omit these inherently malleable metadata)",
};

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

pub(super) const SUCCESS: Relationship = Relationship {
    name: "success",
    description: "Decryption Succeeded",
};

pub(super) const FAILURE: Relationship = Relationship {
    name: "failure",
    description: "Decryption Failed",
};

impl ProcessorDefinition for DecryptContentPGP {
    const DESCRIPTION: &'static str = "Decrypt contents of OpenPGP messages. Using the Packaged Decryption Strategy preserves OpenPGP encoding to support subsequent signature verification.";
    const INPUT_REQUIREMENT: ProcessorInputRequirement = ProcessorInputRequirement::Required;
    const SUPPORTS_DYNAMIC_PROPERTIES: bool = false;
    const SUPPORTS_DYNAMIC_RELATIONSHIPS: bool = false;
    const OUTPUT_ATTRIBUTES: &'static [OutputAttribute] =
        &[LITERAL_DATA_FILENAME, LITERAL_DATA_MODIFIED];
    const RELATIONSHIPS: &'static [Relationship] = &[SUCCESS, FAILURE];

    fn properties() -> &'static [PropertyDefinition] {
        const PROPERTIES: &[PropertyDefinition] =
            property_definitions![DECRYPTION_STRATEGY, SYMMETRIC_PASSWORD, PRIVATE_KEY_SERVICE,];
        PROPERTIES
    }
}
