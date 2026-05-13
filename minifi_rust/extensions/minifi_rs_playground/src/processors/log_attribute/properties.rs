use minifi_native::{LogLevel, Property, StandardPropertyValidator};
use strum::VariantNames;

pub(crate) const LOG_LEVEL: Property = Property {
    name: "Log Level",
    description: "The Log Level to use when logging the Attributes",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some("Info"), // todo! it would be nicer to come from enum value but wasnt able to use into_const_str from another crate
    validator: StandardPropertyValidator::AlwaysValidValidator,
    allowed_values: &LogLevel::VARIANTS,
    allowed_type: "",
};

pub(crate) const ATTRIBUTES_TO_LOG: Property = Property {
    name: "Attributes to Log",
    description: "A comma-separated list of Attributes to Log. If not specified, all attributes will be logged.",
    is_required: false,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: None,
    validator: StandardPropertyValidator::AlwaysValidValidator,
    allowed_values: &[],
    allowed_type: "",
};

pub(crate) const ATTRIBUTES_TO_IGNORE: Property = Property {
    name: "Attributes to Ignore",
    description: "A comma-separated list of Attributes to ignore. If not specified, no attributes will be ignored.",
    is_required: false,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: None,
    validator: StandardPropertyValidator::AlwaysValidValidator,
    allowed_values: &[],
    allowed_type: "",
};

pub(crate) const LOG_PAYLOAD: Property = Property {
    name: "Log Payload",
    description: "If true, the FlowFile's payload will be logged, in addition to its attributes. Otherwise, just the Attributes will be logged.",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some("false"),
    validator: StandardPropertyValidator::BoolValidator,
    allowed_values: &[],
    allowed_type: "",
};

pub(crate) const LOG_PREFIX: Property = Property {
    name: "Log Prefix",
    description: "Log prefix appended to the log lines. It helps to distinguish the output of multiple LogAttribute processors.",
    is_required: false,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: None,
    validator: StandardPropertyValidator::AlwaysValidValidator,
    allowed_values: &[],
    allowed_type: "",
};

pub(crate) const FLOW_FILES_TO_LOG: Property = Property {
    name: "FlowFiles To Log",
    description: "Number of flow files to log. If set to zero all flow files will be logged. Please note that this may block other threads from running if not used judiciously.",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some("1"),
    validator: StandardPropertyValidator::AlwaysValidValidator,
    allowed_values: &[],
    allowed_type: "",
};

pub(crate) const HEX_ENCODE_PAYLOAD: Property = Property {
    name: "Hexencode Payload",
    description: "If true, the FlowFile's payload will be logged in a hexencoded format",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some("false"),
    validator: StandardPropertyValidator::BoolValidator,
    allowed_values: &[],
    allowed_type: "",
};
