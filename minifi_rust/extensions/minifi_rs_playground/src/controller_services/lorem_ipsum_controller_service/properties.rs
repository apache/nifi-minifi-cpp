use minifi_native::{Property, StandardPropertyValidator};

pub(crate) const LENGTH: Property = Property {
    name: "Length",
    description: "How many words to generate",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some("25"),
    validator: StandardPropertyValidator::U64Validator,
    allowed_values: &[],
    allowed_type: "",
};
