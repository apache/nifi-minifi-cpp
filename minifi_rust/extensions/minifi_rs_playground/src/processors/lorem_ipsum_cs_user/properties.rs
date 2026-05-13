use crate::controller_services::lorem_ipsum_controller_service::LoremIpsumControllerService;
use minifi_native::ComponentIdentifier;
use minifi_native::{Property, StandardPropertyValidator};
use strum::VariantNames;

pub(crate) const CONTROLLER_SERVICE: Property = Property {
    name: "Lorem Ipsum Controller Service",
    description: "Name of the lorem ipsum controller service",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: None,
    validator: StandardPropertyValidator::AlwaysValidValidator,
    allowed_values: &[],
    allowed_type: LoremIpsumControllerService::CLASS_NAME,
};

pub(crate) const WRITE_METHOD: Property = Property {
    name: "Write Method",
    description: "Which API to test",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some(super::WriteMethod::Buffer.into_str()),
    validator: StandardPropertyValidator::AlwaysValidValidator,
    allowed_values: super::WriteMethod::VARIANTS,
    allowed_type: "",
};
