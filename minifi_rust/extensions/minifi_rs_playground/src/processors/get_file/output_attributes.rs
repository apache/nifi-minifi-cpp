use minifi_native::OutputAttribute;

pub(crate) const FILENAME_OUTPUT_ATTRIBUTE: OutputAttribute = OutputAttribute {
    name: "filename",
    relationships: &["success"],
    description: "The filename is set to the name of the file on disk",
};

pub(crate) const ABSOLUTE_PATH_OUTPUT_ATTRIBUTE: OutputAttribute = OutputAttribute {
    name: "absolute.path",
    relationships: &["success"],
    description: "The full/absolute path from where a file was picked up. The current 'path' attribute is still populated, but may be a relative path",
};
