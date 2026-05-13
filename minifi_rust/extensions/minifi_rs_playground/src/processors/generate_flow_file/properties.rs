use minifi_native::{Property, StandardPropertyValidator};

pub(crate) const FILE_SIZE: Property = Property {
    name: "File Size",
    description: "The size of the file that will be used",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: true,
    default_value: Some("1 kB"),
    validator: StandardPropertyValidator::DataSizeValidator,
    allowed_values: &[],
    allowed_type: "",
};

pub(crate) const BATCH_SIZE: Property = Property {
    name: "Batch Size",
    description: "The number of FlowFiles to be transferred in each invocation",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some("1"),
    validator: StandardPropertyValidator::U64Validator,
    allowed_values: &[],
    allowed_type: "",
};

pub(crate) const DATA_FORMAT: Property = Property {
    name: "Data Format",
    description: "Specifies whether the data should be Text or Binary",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some("Binary"),
    validator: StandardPropertyValidator::AlwaysValidValidator,
    allowed_values: &["Text", "Binary"],
    allowed_type: "",
};

pub(crate) const UNIQUE_FLOW_FILES: Property = Property {
    name: "Unique FlowFiles",
    description: "If true, each FlowFile that is generated will be unique. If false, a random value will be generated and all FlowFiles will get the same content but this offers much higher throughput (but see the description of Custom Text for special non-random use cases)",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some("true"),
    validator: StandardPropertyValidator::BoolValidator,
    allowed_values: &[],
    allowed_type: "",
};

pub(crate) const CUSTOM_TEXT: Property = Property {
    name: "Custom Text",
    description: "If Data Format is text and if Unique FlowFiles is false, then this custom text will be used as content of the generated FlowFiles and the File Size will be ignored. Finally, if Expression Language is used, evaluation will be performed only once per batch of generated FlowFiles",
    is_required: false,
    is_sensitive: false,
    supports_expr_lang: true,
    default_value: None,
    validator: StandardPropertyValidator::AlwaysValidValidator,
    allowed_values: &[],
    allowed_type: "",
};
