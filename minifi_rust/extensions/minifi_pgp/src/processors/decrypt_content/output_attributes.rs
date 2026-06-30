use minifi_native::OutputAttribute;

pub(crate) const LITERAL_DATA_FILENAME: OutputAttribute = OutputAttribute {
    name: "pgp.literal.data.filename",
    relationships: &["success"],
    description: "Filename from decrypted Literal Data (Note that OpenPGP signatures do not include the formatting octet, the file name, and the date field of the Literal Data packet in a signature hash; therefore, those fields are not protected against tampering in a signed document. Therefore a lot of implementation omit these inherently malleable metadata)",
};

pub(crate) const LITERAL_DATA_MODIFIED: OutputAttribute = OutputAttribute {
    name: "pgp.literal.data.modified",
    relationships: &["success"],
    description: "Modified Date from decrypted Literal Data (Note that OpenPGP signatures do not include the formatting octet, the file name, and the date field of the Literal Data packet in a signature hash; therefore, those fields are not protected against tampering in a signed document. Therefore a lot of implementation omit these inherently malleable metadata)",
};
