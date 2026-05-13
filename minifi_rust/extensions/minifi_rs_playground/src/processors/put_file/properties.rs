use minifi_native::{Property, StandardPropertyValidator};
use strum::VariantNames;

use super::ConflictResolutionStrategy;
pub(crate) const DIRECTORY: Property = Property {
    name: "Directory",
    description: "The output directory to which to put files",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: true,
    default_value: Some("."),
    validator: StandardPropertyValidator::NonBlankValidator,
    allowed_values: &[],
    allowed_type: "",
};

pub(crate) const CONFLICT_RESOLUTION: Property = Property {
    name: "Conflict Resolution Strategy",
    description: "Indicates what should happen when a file with the same name already exists in the output directory",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some(ConflictResolutionStrategy::Fail.into_str()),
    validator: StandardPropertyValidator::AlwaysValidValidator,
    allowed_values: &ConflictResolutionStrategy::VARIANTS,
    allowed_type: "",
};

pub(crate) const CREATE_DIRS: Property = Property {
    name: "Create Missing Directories",
    description: "If true, then missing destination directories will be created. If false, flowfiles are penalized and sent to failure.",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some("true"),
    validator: StandardPropertyValidator::BoolValidator,
    allowed_values: &[],
    allowed_type: "",
};

pub(crate) const MAX_FILE_COUNT: Property = Property {
    name: "Maximum File Count",
    description: "Specifies the maximum number of files that can exist in the output directory",
    is_required: false,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: None, // Diverged from the original implementation, u64 with no default describes the behavior better
    validator: StandardPropertyValidator::U64Validator,
    allowed_values: &[],
    allowed_type: "",
};
