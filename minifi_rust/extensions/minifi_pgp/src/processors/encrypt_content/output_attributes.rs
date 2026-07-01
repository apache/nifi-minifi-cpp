use minifi_native::OutputAttribute;

pub(crate) const FILE_ENCODING: OutputAttribute = OutputAttribute {
    name: "pgp.file.encoding",
    relationships: &["success"],
    description: "File Encoding",
};
