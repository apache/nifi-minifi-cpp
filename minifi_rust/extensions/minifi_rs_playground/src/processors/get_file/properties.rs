use minifi_native::{Property, StandardPropertyValidator};

pub(crate) const DIRECTORY: Property = Property {
    name: "Input Directory",
    description: "The input directory from which to pull files",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: true,
    default_value: None,
    validator: StandardPropertyValidator::NonBlankValidator,
    allowed_values: &[],
    allowed_type: "",
};

pub(crate) const RECURSE: Property = Property {
    name: "Recurse Subdirectories",
    description: "Indicates whether or not to pull files from subdirectories",
    is_required: false,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some("true"),
    validator: StandardPropertyValidator::BoolValidator,
    allowed_values: &[],
    allowed_type: "",
};

pub(crate) const KEEP_SOURCE_FILE: Property = Property {
    name: "Keep Source File",
    description: "If true, the file is not deleted after it has been copied to the Content Repository",
    is_required: false,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some("false"),
    validator: StandardPropertyValidator::BoolValidator,
    allowed_values: &[],
    allowed_type: "",
};

pub(crate) const MIN_AGE: Property = Property {
    name: "Minimum File Age",
    description: "The minimum age that a file must be in order to be pulled; any file younger than this amount of time (according to last modification date) will be ignored",
    is_required: false,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: None,
    validator: StandardPropertyValidator::TimePeriodValidator,
    allowed_values: &[],
    allowed_type: "",
};

pub(crate) const MAX_AGE: Property = Property {
    name: "Maximum File Age",
    description: "The maximum age that a file must be in order to be pulled;  any file older than this amount of time (according to last modification date) will be ignored",
    is_required: false,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: None,
    validator: StandardPropertyValidator::TimePeriodValidator,
    allowed_values: &[],
    allowed_type: "",
};

pub(crate) const MIN_SIZE: Property = Property {
    name: "Minimum File Size",
    description: "The minimum size that a file can be in order to be pulled",
    is_required: false,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: None,
    validator: StandardPropertyValidator::DataSizeValidator,
    allowed_values: &[],
    allowed_type: "",
};

pub(crate) const MAX_SIZE: Property = Property {
    name: "Maximum File Size",
    description: "The maximum size that a file can be in order to be pulled",
    is_required: false,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: None,
    validator: StandardPropertyValidator::DataSizeValidator,
    allowed_values: &[],
    allowed_type: "",
};

pub(crate) const IGNORE_HIDDEN_FILES: Property = Property {
    name: "Ignore Hidden Files",
    description: "Indicates whether or not hidden files should be ignored",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some("true"),
    validator: StandardPropertyValidator::BoolValidator,
    allowed_values: &[],
    allowed_type: "",
};

pub(crate) const POLLING_INTERVAL: Property = Property {
    name: "Polling Interval",
    description: "Indicates how long to wait before performing a directory listing",
    is_required: false,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: None,
    validator: StandardPropertyValidator::TimePeriodValidator,
    allowed_values: &[],
    allowed_type: "",
};

pub(crate) const BATCH_SIZE: Property = Property {
    name: "Batch Size",
    description: "The maximum number of files to pull in each iteration",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some("10"),
    validator: StandardPropertyValidator::U64Validator,
    allowed_values: &[],
    allowed_type: "",
};
